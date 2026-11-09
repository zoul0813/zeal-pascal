/* Pinned SDCC XL3/ASxxxx reader. Relocations remain symbolic until ZPU
 * fragment placement; unsupported input fails before publishing artifacts. */
#include "objects.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_LIMIT (8u * 1024u * 1024u)
void sf_die(const char *message) {
    fprintf(stderr, "sysfile: %s\n", message);
    exit(EXIT_FAILURE);
}
void *sf_resize(void *p, size_t count, size_t width) {
    void *out;
    if (!width || count > FILE_LIMIT / width)
        sf_die("input/allocation limit exceeded");
    out = realloc(p, count ? count * width : 1);
    if (!out)
        sf_die("out of host memory");
    return out;
}
char *sf_string(const char *text, size_t size) {
    char *out = sf_resize(NULL, size + 1, 1);
    if (size && memchr(text, 0, size))
        sf_die("embedded NUL in text");
    if (size)
        memcpy(out, text, size);
    out[size] = 0;
    return out;
}
void sf_append(Bytes *b, const void *data, size_t size) {
    size_t capacity;
    if (size > FILE_LIMIT - b->size)
        sf_die("input too large");
    if (b->size + size > b->capacity) {
        capacity = b->capacity ? b->capacity : 256;
        while (capacity < b->size + size)
            capacity *= 2;
        b->data = sf_resize(b->data, capacity, 1);
        b->capacity = capacity;
    }
    if (size)
        memcpy(b->data + b->size, data, size);
    b->size += size;
}
Bytes sf_read(const char *path) {
    Bytes b = {0};
    unsigned char buffer[4096];
    size_t size;
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        sf_die("cannot read input");
    }
    while ((size = fread(buffer, 1, sizeof(buffer), f)) != 0)
        sf_append(&b, buffer, size);
    if (ferror(f) || fclose(f))
        sf_die("input read failed");
    return b;
}
static unsigned hex(const char *text) {
    unsigned value = 0, digit;
    size_t n = strlen(text);
    if (!n || n > 8)
        sf_die("invalid hexadecimal number");
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else
            sf_die("invalid hexadecimal number");
        if (value > 0x0fffffffu)
            sf_die("hexadecimal overflow");
        value = value * 16 + digit;
    }
    return value;
}
static size_t tokens(char *line, char **words, size_t limit) {
    size_t n = 0;
    while (*line) {
        while (*line == ' ' || *line == '\t')
            ++line;
        if (!*line)
            break;
        if (n == limit)
            sf_die("too many record fields");
        words[n++] = line;
        while (*line && *line != ' ' && *line != '\t')
            ++line;
        if (*line)
            *line++ = 0;
    }
    return n;
}
static unsigned word(const unsigned char *p) {
    return p[0] | ((unsigned)p[1] << 8);
}
static void apply_text(Object *o, const Bytes *text, const unsigned char *r, size_t size) {
    Area *a;
    size_t i, at = 3, out = 0, previous = 3, edit = 4;
    unsigned area, start, width, mode, target, addend;
    unsigned char *decoded;
    Relocation relocation;
    if (size < 4 || r[0] || r[1] || (size - 4) % 4)
        sf_die("unsupported relocation record");
    area = word(r + 2);
    if (area >= o->area_count)
        sf_die("unknown text area");
    a = o->areas + area;
    start = word(text->data) | ((unsigned)text->data[2] << 16);
    if (start < a->address && a->absolute)
        sf_die("text before absolute area");
    if (a->absolute)
        start -= a->address;
    for (i = 4; i < size; i += 4) {
        mode = r[i];
        width = mode & 1 ? 3 : 2;
        target = word(r + i + 2);
        if (mode != 0 && mode != 2 && mode != 9 && mode != 11 && mode != 137 && mode != 139)
            sf_die("unsupported relocation mode");
        if (r[i + 1] < previous || r[i + 1] + width > text->size)
            sf_die("overlapping/out-of-range relocation");
        if (target >= (mode & 2 ? o->symbol_count : o->area_count))
            sf_die("unknown relocation target");
        previous = r[i + 1] + width;
    }
    decoded = sf_resize(NULL, text->size, 1);
    while (at < text->size) {
        if (edit < size && at == r[edit + 1]) {
            mode = r[edit];
            width = mode & 1 ? 3 : 2;
            addend = word(text->data + at);
            if (width == 3 && text->data[at + 2])
                sf_die("address addend exceeds Z80 range");
            relocation.site = start + (unsigned)out;
            relocation.symbol = !!(mode & 2);
            relocation.target = word(r + edit + 2);
            relocation.addend = addend;
            relocation.kind = mode & 128 ? 2 : mode & 1 ? 1 : 0;
            a->relocations = sf_resize(a->relocations, a->count + 1, sizeof(*a->relocations));
            a->relocations[a->count++] = relocation;
            decoded[out++] = 0;
            if (width == 2)
                decoded[out++] = 0;
            at += width;
            edit += 4;
        } else
            decoded[out++] = text->data[at++];
    }
    if (start > a->size || out > a->size - start)
        sf_die("text outside its area");
    for (i = 0; i < out; ++i) {
        if (a->initialized[start + i])
            sf_die("overlapping text");
        a->initialized[start + i] = 1;
        a->data[start + i] = decoded[i];
    }
    free(decoded);
}
Object *sf_object(const unsigned char *data, size_t size) {
    Object *o = calloc(1, sizeof(*o));
    Bytes pending = {0};
    char *copy, *line, *next, *w[1024];
    size_t n, i, line_number = 0;
    unsigned areas = 0, symbols = 0, header = 0, module = 0, value, flags, address;
    int current = -1;
    if (!o)
        sf_die("out of host memory");
    copy = sf_string((const char *)data, size);
    line = copy;
    while (*line) {
        next = strchr(line, '\n');
        if (next)
            *next++ = 0;
        n = strlen(line);
        if (n && line[n - 1] == '\r')
            line[--n] = 0;
        if (n > 4096)
            sf_die("object record too long");
        if (!line_number++) {
            if (strcmp(line, "XL3"))
                sf_die("expected XL3 object");
        } else {
            n = tokens(line, w, 1024);
            if (!n)
                sf_die("empty object record");
            if (!strcmp(w[0], "H")) {
                if (n != 6 || strcmp(w[2], "areas") || strcmp(w[4], "global") ||
                    strcmp(w[5], "symbols") || header++)
                    sf_die("invalid/duplicate object header");
                areas = hex(w[1]);
                symbols = hex(w[3]);
                if (areas > 4096 || symbols > 65536)
                    sf_die("object directory too large");
            } else if (!strcmp(w[0], "M")) {
                if (n != 2 || module++)
                    sf_die("invalid/duplicate module name");
            } else if (!strcmp(w[0], "O")) {
                if (n != 2 || strcmp(w[1], "-mz80"))
                    sf_die("unsupported object options");
            } else if (!strcmp(w[0], "A")) {
                Area a = {0};
                if (n != 8 || strcmp(w[2], "size") || strcmp(w[4], "flags") || strcmp(w[6], "addr"))
                    sf_die("invalid area");
                value = hex(w[3]);
                flags = hex(w[5]);
                address = hex(w[7]);
                if ((flags != 0 && flags != 8) || value > 65535 || address > 65536 ||
                    value > 65536 - address)
                    sf_die("unsupported area attributes/extent");
                for (i = 0; i < o->area_count; ++i)
                    if (!strcmp(o->areas[i].name, w[1]))
                        sf_die("duplicate area");
                a.name = sf_string(w[1], strlen(w[1]));
                a.size = value;
                a.absolute = flags == 8;
                a.address = address;
                a.data = calloc(value ? value : 1, 1);
                a.initialized = calloc(value ? value : 1, 1);
                if (!a.data || !a.initialized)
                    sf_die("out of host memory");
                if (o->area_count >= areas)
                    sf_die("too many areas");
                o->areas = sf_resize(o->areas, o->area_count + 1, sizeof(*o->areas));
                current = (int)o->area_count;
                o->areas[o->area_count++] = a;
            } else if (!strcmp(w[0], "S")) {
                Symbol s = {0};
                if (n != 3 || strlen(w[2]) != 9 ||
                    (strncmp(w[2], "Def", 3) && strncmp(w[2], "Ref", 3)))
                    sf_die("invalid symbol");
                s.defined = !strncmp(w[2], "Def", 3);
                s.value = hex(w[2] + 3);
                s.area = s.defined ? current : -1;
                if (s.value > 65535 || (!s.defined && s.value))
                    sf_die("invalid symbol value");
                s.name = sf_string(w[1], strlen(w[1]));
                if (o->symbol_count >= symbols)
                    sf_die("too many symbols");
                o->symbols = sf_resize(o->symbols, o->symbol_count + 1, sizeof(*o->symbols));
                o->symbols[o->symbol_count++] = s;
            } else if (!strcmp(w[0], "T") || !strcmp(w[0], "R")) {
                Bytes b = {0};
                unsigned char byte;
                for (i = 1; i < n; ++i) {
                    if (strlen(w[i]) != 2)
                        sf_die("invalid record byte");
                    byte = (unsigned char)hex(w[i]);
                    sf_append(&b, &byte, 1);
                }
                if (w[0][0] == 'T') {
                    if (pending.data || b.size < 3)
                        sf_die("invalid/unpaired text record");
                    pending = b;
                } else {
                    if (!pending.data)
                        sf_die("relocations without text");
                    apply_text(o, &pending, b.data, b.size);
                    free(pending.data);
                    memset(&pending, 0, sizeof(pending));
                    free(b.data);
                }
            } else
                sf_die("unsupported object record");
        }
        if (!next)
            break;
        line = next;
    }
    if (!line_number || pending.data || header != 1 || areas != o->area_count ||
        symbols != o->symbol_count)
        sf_die("incomplete object/header count mismatch");
    for (i = 0; i < o->symbol_count; ++i)
        if (o->symbols[i].defined && o->symbols[i].area >= 0) {
            Symbol *s = o->symbols + i;
            Area *a = o->areas + s->area;
            unsigned base = a->absolute ? a->address : 0;
            if (s->value < base || s->value - base > a->size)
                sf_die("symbol outside its area");
        }
    free(copy);
    return o;
}
static size_t decimal(const char *text, size_t size) {
    size_t value = 0, i = 0;
    unsigned digit;
    while (i < size && text[i] == ' ')
        ++i;
    if (i == size || !isdigit((unsigned char)text[i]))
        sf_die("invalid archive size/name");
    for (; i < size && text[i] != ' '; ++i) {
        if (!isdigit((unsigned char)text[i]))
            sf_die("invalid archive number");
        digit = (unsigned)(text[i] - '0');
        if (value > (FILE_LIMIT - digit) / 10)
            sf_die("archive number overflow");
        value = value * 10 + digit;
    }
    for (; i < size; ++i)
        if (text[i] != ' ')
            sf_die("invalid archive number suffix");
    return value;
}
Archive sf_archive(const unsigned char *data, size_t size) {
    Archive a = {0};
    size_t at = 8, n, length, offset, end;
    const unsigned char *names = NULL, *payload;
    size_t names_size = 0;
    char *name;
    Member m;
    if (size < 8 || memcmp(data, "!<arch>\n", 8))
        sf_die("expected ar library");
    while (at < size) {
        if (size - at < 60 || memcmp(data + at + 58, "`\n", 2))
            sf_die("invalid archive member header");
        n = decimal((const char *)data + at + 48, 10);
        length = 16;
        while (length && data[at + length - 1] == ' ')
            --length;
        name = sf_string((const char *)data + at, length);
        at += 60;
        if (n > size - at)
            sf_die("archive member outside file");
        payload = data + at;
        at += n;
        if (n & 1) {
            if (at == size || data[at] != '\n')
                sf_die("invalid archive padding");
            ++at;
        }
        if (!strcmp(name, "/") || !strcmp(name, "/SYM64/")) {
            free(name);
            continue;
        }
        if (!strcmp(name, "//")) {
            names = payload;
            names_size = n;
            free(name);
            continue;
        }
        if (!strncmp(name, "#1/", 3)) {
            length = decimal(name + 3, strlen(name + 3));
            if (!length || length > n)
                sf_die("invalid inline archive name");
            free(name);
            end = length;
            while (end && payload[end - 1] == 0)
                --end;
            name = sf_string((const char *)payload, end);
            payload += length;
            n -= length;
        } else if (name[0] == '/') {
            offset = decimal(name + 1, strlen(name + 1));
            end = offset;
            if (offset >= names_size || (offset && names[offset - 1] != '\n'))
                sf_die("invalid archive name-table offset");
            while (end + 1 < names_size && (names[end] != '/' || names[end + 1] != '\n'))
                ++end;
            if (end + 1 >= names_size)
                sf_die("unterminated archive name");
            free(name);
            name = sf_string((const char *)names + offset, end - offset);
        } else {
            length = strlen(name);
            if (length && name[length - 1] == '/')
                name[length - 1] = 0;
        }
        if (!*name)
            sf_die("empty archive member name");
        m.name = name;
        m.object = sf_object(payload, n);
        a.members = sf_resize(a.members, a.count + 1, sizeof(*a.members));
        a.members[a.count++] = m;
    }
    return a;
}
void sf_object_free(Object *o) {
    size_t i;
    for (i = 0; i < o->area_count; ++i) {
        free(o->areas[i].name);
        free(o->areas[i].data);
        free(o->areas[i].initialized);
        free(o->areas[i].relocations);
    }
    for (i = 0; i < o->symbol_count; ++i)
        free(o->symbols[i].name);
    free(o->areas);
    free(o->symbols);
    free(o);
}
void sf_archive_free(Archive *a) {
    size_t i;
    for (i = 0; i < a->count; ++i) {
        free(a->members[i].name);
        sf_object_free(a->members[i].object);
    }
    free(a->members);
}
