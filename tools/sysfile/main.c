/* Host-side SYSFILE author. CMake owns toolchain invocations; this program
 * prepares assembly, resolves relocatable objects and writes the ZPU envelope. */
#define _POSIX_C_SOURCE 200809L
#include "../host/sha256.h"
#include "objects.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void format(Bytes *out, const char *fmt, ...) {
    va_list args, copy;
    int n;
    char *text;
    va_start(args, fmt);
    va_copy(copy, args);
    n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0)
        sf_die("formatting failed");
    text = sf_resize(NULL, (size_t)n + 1, 1);
    vsnprintf(text, (size_t)n + 1, fmt, args);
    va_end(args);
    sf_append(out, text, (size_t)n);
    free(text);
}
static char *path(const char *directory, const char *name) {
    size_t size = strlen(directory) + strlen(name) + 2;
    char *out = sf_resize(NULL, size, 1);
    snprintf(out, size, "%s/%s", directory, name);
    return out;
}
static void publish(const char *name, const void *data, size_t size) {
    char *temporary = sf_resize(NULL, strlen(name) + 12, 1);
    int fd;
    FILE *f;
    snprintf(temporary, strlen(name) + 12, "%s.XXXXXX", name);
    fd = mkstemp(temporary);
    if (fd < 0)
        sf_die("cannot create output temporary");
    f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        unlink(temporary);
        sf_die("cannot open output temporary");
    }
    if (fwrite(data, 1, size, f) != size || fflush(f) || fchmod(fd, 0644)) {
        fclose(f);
        unlink(temporary);
        sf_die("output write failed");
    }
    if (fclose(f) || rename(temporary, name)) {
        unlink(temporary);
        sf_die("output publication failed");
    }
    free(temporary);
}
static void save(const char *directory, const char *name, const Bytes *data) {
    char *p = path(directory, name);
    publish(p, data->data, data->size);
    free(p);
}
static unsigned get_word(const unsigned char *p) {
    return p[0] | ((unsigned)p[1] << 8);
}
static void put_word(unsigned char *p, unsigned value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}
static void put_long(unsigned char *p, uint32_t value) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        p[i] = (unsigned char)(value >> (8 * i));
}
static uint32_t get_long(const unsigned char *p) {
    return (uint32_t)get_word(p) | ((uint32_t)get_word(p + 2) << 16);
}
static void emit_word(Bytes *out, unsigned value) {
    unsigned char bytes[2];
    if (value > 65535)
        sf_die("ZPU word overflow");
    put_word(bytes, value);
    sf_append(out, bytes, 2);
}
static uint32_t fingerprint(const unsigned char *p, size_t size) {
    uint32_t hash = 2166136261u;
    size_t i;
    for (i = 0; i < size; ++i)
        if (i < 28 || i >= 32)
            hash = (hash ^ p[i]) * 16777619u;
    return hash;
}
static void digest(const Bytes *data, char hex[65]) {
    static const char digits[] = "0123456789abcdef";
    Sha256 state;
    unsigned char bytes[32];
    size_t i;
    sha_init(&state);
    for (i = 0; i < data->size; ++i)
        sha_byte(&state, data->data[i]);
    sha_end(&state, bytes);
    for (i = 0; i < 32; ++i) {
        hex[2 * i] = digits[bytes[i] >> 4];
        hex[2 * i + 1] = digits[bytes[i] & 15];
    }
    hex[64] = 0;
}
static char *trim(char *line) {
    while (*line == ' ' || *line == '\t')
        ++line;
    return line;
}
static int identifier(unsigned char c) {
    return isalnum(c) || c == '_';
}
static void prepare(const char *mode, const char *input, const char *output, const char *prefix) {
    Bytes raw = sf_read(input), result = {0};
    char *copy = sf_string((char *)raw.data, raw.size), *line, *next, *body, *found;
    char **names = NULL, *name;
    size_t count = 0, i, n;
    unsigned functions = 0;
    char current[80];
    if (prefix && strlen(prefix) > 32)
        sf_die("assembly prefix too long");
    if (prefix)
        for (i = 0; prefix[i]; ++i)
            if (!identifier((unsigned char)prefix[i]))
                sf_die("invalid assembly prefix");
    if (!strcmp(mode, "sdk")) {
        char *scan = sf_string(copy, strlen(copy));
        line = scan;
        while (*line) {
            next = strchr(line, '\n');
            if (next)
                *next++ = 0;
            body = trim(line);
            if (!strncmp(body, ".globl", 6) && isspace((unsigned char)body[6])) {
                body = trim(body + 6);
                n = 0;
                while (identifier((unsigned char)body[n]))
                    ++n;
                if (n) {
                    names = sf_resize(names, count + 1, sizeof(*names));
                    names[count++] = sf_string(body, n);
                }
            }
            if (!next)
                break;
            line = next;
        }
        free(scan);
    }
    snprintf(current, sizeof(current), "_%s_constants", prefix ? prefix : "");
    line = copy;
    while (*line) {
        next = strchr(line, '\n');
        if (next)
            *next++ = 0;
        n = strlen(line);
        if (n && line[n - 1] == '\r')
            line[--n] = 0;
        if (prefix) {
            found = strstr(line, "; Function ");
            if (found && (isalpha((unsigned char)found[11]) || found[11] == '_')) {
                snprintf(current, sizeof(current), "_%s_%u", prefix, ++functions);
                format(&result, "\t.area %s\n", current);
            }
            body = trim(line);
            if ((!strncmp(body, ".area _TEXT", 11) || !strncmp(body, ".area _CODE", 11)) &&
                !identifier((unsigned char)body[11]))
                format(&result, "\t.area %s\n", current);
            else
                format(&result, "%s\n", line);
        } else {
            n = 0;
            if (isalpha((unsigned char)line[0]) || line[0] == '_')
                while (identifier((unsigned char)line[n]))
                    ++n;
            if (n && line[n] == ':') {
                name = sf_string(line, n);
                for (i = 0; i < count; ++i)
                    if (!strcmp(name, names[i])) {
                        format(&result, "\t.area _SDK%s\n",
                               !strcmp(name, "_getchar") || !strcmp(name, "_putchar") ? "stdio"
                                                                                      : name);
                        break;
                    }
                free(name);
            }
            format(&result, "%s\n", line);
        }
        if (!next)
            break;
        line = next;
    }
    if (prefix && !functions)
        sf_die("no SDCC function boundaries found");
    publish(output, result.data, result.size);
    for (i = 0; i < count; ++i)
        free(names[i]);
    free(names);
    free(copy);
    free(raw.data);
    free(result.data);
}
static Symbol *defined(Object **objects, size_t count, const char *name, Object **owner) {
    size_t i, j;
    for (i = 0; i < count; ++i)
        for (j = 0; j < objects[i]->symbol_count; ++j) {
            Symbol *s = objects[i]->symbols + j;
            if (s->defined && !strcmp(s->name, name)) {
                if (owner)
                    *owner = objects[i];
                return s;
            }
        }
    return NULL;
}
static unsigned symbol_value(Object *o, const Symbol *s) {
    return s->value + (s->area >= 0 && o->areas[s->area].size ? o->areas[s->area].base : 0);
}
static int relocation_order(const void *a, const void *b) {
    const Relocation *x = a, *y = b;
    return x->site > y->site ? 1 : x->site < y->site ? -1 : 0;
}
static unsigned locate(Area **fragments, size_t count, unsigned code, unsigned total,
                       unsigned value, unsigned *offset) {
    size_t i;
    for (i = 0; i < count; ++i)
        if (value >= fragments[i]->base && value - fragments[i]->base < fragments[i]->size) {
            *offset = value - fragments[i]->base;
            return (unsigned)i;
        }
    if (value >= code && value < total) {
        *offset = value - code;
        return (unsigned)count;
    }
    sf_die("relocation target outside SYSFILE");
    return 0;
}
static const char *const exports[] = {
    "dispatch",    "table",     "reset",      "rewrite", "close",  "read",   "write",
    "position",    "seek",      "flush",      "erase",   "rename", "blocks", "text_read",
    "text_readln", "predicate", "text_write", "begin",   "path"};
static const char unit_name[] = "sysfile", unit_path[] = "T:/lib/SYSFILE.PAS";
static void pack(const char *directory, int count, char **inputs, const char *library) {
    Archive archive;
    Object **objects = NULL, **pending = NULL, *o, *owner;
    size_t object_count = 0, pending_count = 0, root_count, i, j, k, m, providers, chosen,
           member_count = 0;
    char **members = NULL;
    Area **fragments = NULL, *a;
    Symbol *s, *other;
    Relocation *r;
    Bytes raw = sf_read(library), payload = {0}, references = {0}, records = {0}, metadata = {0},
          artifact = {0}, report = {0};
    unsigned code, total, data_size, fragment_count = 0, f, offset = 0, target, value, start;
    char symbol[64], hash[65];
    unsigned char header[48] = {0}, entry[14] = {0}, blob[16] = {0}, zero = 0;
    archive = sf_archive(raw.data, raw.size);
    free(raw.data);
    for (i = 0; i < (size_t)count; ++i) {
        raw = sf_read(inputs[i]);
        o = sf_object(raw.data, raw.size);
        free(raw.data);
        objects = sf_resize(objects, object_count + 1, sizeof(*objects));
        objects[object_count++] = o;
    }
    root_count = object_count;
    pending = sf_resize(NULL, object_count, sizeof(*pending));
    memcpy(pending, objects, object_count * sizeof(*pending));
    pending_count = object_count;
    /* Match SDCC library extraction: only actual relocation references pull
     * a provider; definitions become visible immediately, pending scans LIFO. */
    while (pending_count) {
        o = pending[--pending_count];
        for (i = 0; i < o->area_count; ++i)
            for (j = 0; j < o->areas[i].count; ++j) {
                r = o->areas[i].relocations + j;
                if (!r->symbol)
                    continue;
                s = o->symbols + r->target;
                if (defined(objects, object_count, s->name, NULL) || !strncmp(s->name, "s__", 3) ||
                    !strncmp(s->name, "l__", 3))
                    continue;
                providers = 0;
                chosen = 0;
                for (k = 0; k < archive.count; ++k) {
                    Object *candidate = archive.members[k].object;
                    for (m = 0; m < candidate->symbol_count; ++m)
                        if (candidate->symbols[m].defined &&
                            !strcmp(candidate->symbols[m].name, s->name) &&
                            strcmp(s->name, ".__.ABS.")) {
                            ++providers;
                            chosen = k;
                        }
                }
                if (providers != 1)
                    sf_die(providers ? "ambiguous required library symbol"
                                     : "unresolved library reference");
                owner = archive.members[chosen].object;
                objects = sf_resize(objects, object_count + 1, sizeof(*objects));
                objects[object_count++] = owner;
                members = sf_resize(members, member_count + 1, sizeof(*members));
                members[member_count++] = archive.members[chosen].name;
                pending = sf_resize(pending, pending_count + 1, sizeof(*pending));
                pending[pending_count++] = owner;
            }
    }
    for (i = 0; i < object_count; ++i)
        for (j = 0; j < objects[i]->area_count; ++j) {
            a = objects[i]->areas + j;
            if (!a->size)
                continue;
            if (a->absolute || !strcmp(a->name, "_INITIALIZED") ||
                !strcmp(a->name, "_INITIALIZER") || !strcmp(a->name, "_GSINIT") ||
                !strcmp(a->name, "_GSFINAL"))
                sf_die("unsupported absolute/initialized native file state");
            if (!strcmp(a->name, "_DATA") || !strcmp(a->name, "_BSS"))
                continue;
            a->base = (unsigned)payload.size;
            sf_append(&payload, a->data, a->size);
            fragments = sf_resize(fragments, fragment_count + 1, sizeof(*fragments));
            fragments[fragment_count++] = a;
        }
    code = (unsigned)payload.size;
    for (i = 0; i < object_count; ++i)
        for (j = 0; j < objects[i]->area_count; ++j) {
            a = objects[i]->areas + j;
            if (!a->size || (strcmp(a->name, "_DATA") && strcmp(a->name, "_BSS")))
                continue;
            if (a->count)
                sf_die("relocated native file state");
            for (k = 0; k < a->size; ++k)
                if (a->initialized[k])
                    sf_die("initialized native file state");
            a->base = (unsigned)payload.size;
            sf_append(&payload, a->data, a->size);
        }
    total = (unsigned)payload.size;
    data_size = total - code;
    if (total >= 32768 || fragment_count + (data_size != 0) > 4096)
        sf_die("SYSFILE payload/fragment limit exceeded");
    for (i = 0; i < object_count; ++i)
        for (j = 0; j < objects[i]->symbol_count; ++j) {
            s = objects[i]->symbols + j;
            if (!s->defined)
                continue;
            other = defined(objects, object_count, s->name, &owner);
            if (symbol_value(objects[i], s) != symbol_value(owner, other))
                sf_die("duplicate symbol definition");
        }
    for (i = 0; i < object_count; ++i)
        for (j = 0; j < objects[i]->area_count; ++j) {
            a = objects[i]->areas + j;
            if (!a->size || !strcmp(a->name, "_DATA") || !strcmp(a->name, "_BSS"))
                continue;
            start = (unsigned)(references.size / 8);
            if (a->count > 1)
                qsort(a->relocations, a->count, sizeof(*a->relocations), relocation_order);
            for (k = 0; k < a->count; ++k) {
                r = a->relocations + k;
                if (r->symbol) {
                    s = defined(objects, object_count, objects[i]->symbols[r->target].name, &owner);
                    if (!s)
                        sf_die("unresolved symbol during placement");
                    target = symbol_value(owner, s);
                } else {
                    if (!objects[i]->areas[r->target].size)
                        sf_die("relocation targets an empty area");
                    target = objects[i]->areas[r->target].base;
                }
                value = target + r->addend;
                if (value >= 65536 && r->addend >= 32768)
                    value -= 65536;
                f = locate(fragments, fragment_count, code, total, value, &offset);
                emit_word(&references, r->site);
                emit_word(&references, 1);
                emit_word(&references, f | (r->kind << 14));
                emit_word(&references, offset);
            }
            emit_word(&records, a->base);
            emit_word(&records, a->size);
            emit_word(&records, start);
            emit_word(&records, (unsigned)(references.size / 8) - start);
        }
    if (data_size) {
        emit_word(&records, code);
        emit_word(&records, data_size);
        emit_word(&records, (unsigned)(references.size / 8));
        emit_word(&records, 0);
    }
    sf_append(&metadata, unit_name, strlen(unit_name));
    sf_append(&metadata, unit_path, strlen(unit_path));
    start = 48 + (unsigned)metadata.size;
    for (i = 0; i < sizeof(exports) / sizeof(exports[0]); ++i) {
        if (i == 0)
            strcpy(symbol, "_zf_entry");
        else if (i == 1)
            strcpy(symbol, "_zf_operations_address");
        else
            snprintf(symbol, sizeof(symbol), "_zf_op_%s", exports[i]);
        s = defined(objects, object_count, symbol, &owner);
        if (!s)
            sf_die("missing SYSFILE export");
        f = locate(fragments, fragment_count, code, total, symbol_value(owner, s), &offset);
        if (f >= fragment_count)
            sf_die("SYSFILE export is not executable");
        memset(entry, 0, sizeof(entry));
        entry[0] = (unsigned char)strlen(exports[i]);
        put_word(entry + 4, 65535);
        put_word(entry + 8, f);
        put_word(entry + 10, offset);
        sf_append(&metadata, entry, sizeof(entry));
        sf_append(&metadata, exports[i], strlen(exports[i]));
    }
    for (i = 0; i < fragment_count + (data_size != 0); ++i)
        sf_append(&metadata, &zero, 1);
    memcpy(blob, "ZPF1", 4);
    put_word(blob + 4, 1);
    put_word(blob + 6, fragment_count + (data_size != 0));
    put_word(blob + 8, (unsigned)(references.size / 8));
    put_word(blob + 10, total);
    memcpy(header, "ZPU1", 4);
    put_word(header + 4, 1);
    put_long(header + 12, (2166136261u ^ 32u) * 16777619u);
    put_word(header + 16, 1);
    header[18] = (unsigned char)strlen(unit_name);
    header[19] = (unsigned char)strlen(unit_path);
    put_word(header + 20, (unsigned)(sizeof(exports) / sizeof(exports[0])));
    put_word(header + 26, 48 + (unsigned)metadata.size);
    put_word(header + 32, code);
    put_word(header + 34, data_size);
    put_word(header + 40, start);
    put_word(header + 42, start);
    put_word(header + 44, start);
    sf_append(&artifact, header, sizeof(header));
    sf_append(&artifact, metadata.data, metadata.size);
    sf_append(&artifact, blob, sizeof(blob));
    sf_append(&artifact, records.data, records.size);
    sf_append(&artifact, references.data, references.size);
    sf_append(&artifact, payload.data, payload.size);
    if (artifact.size > 65534)
        sf_die("SYSFILE artifact too large");
    put_word(artifact.data + 6, (unsigned)artifact.size);
    put_long(artifact.data + 28, fingerprint(artifact.data, artifact.size));
    digest(&artifact, hash);
    format(&report,
           "{\n  \"bytes\": %zu,\n  \"code_bytes\": %u,\n  \"data_bytes\": %u,\n  \"fragments\": "
           "%u,\n  \"relocations\": %zu,\n  \"sdk_members\": [",
           artifact.size, code, data_size, fragment_count + (data_size != 0), references.size / 8);
    for (i = 0; i < member_count; ++i) {
        format(&report, "%s\n    \"", i ? "," : "");
        for (j = 0; members[i][j]; ++j) {
            unsigned char c = (unsigned char)members[i][j];
            if (c < 32 || c >= 127)
                sf_die("non-ASCII archive member name");
            if (c == '"' || c == '\\')
                format(&report, "\\");
            sf_append(&report, &c, 1);
        }
        format(&report, "\"");
    }
    format(&report, "%s],\n  \"sha256\": \"%s\"\n}\n", member_count ? "\n  " : "", hash);
    save(directory, "sysfile.payload", &artifact);
    save(directory, "direct-file.payload.json", &report);
    fwrite(report.data, 1, report.size, stdout);
    for (i = 0; i < root_count; ++i)
        sf_object_free(objects[i]);
    sf_archive_free(&archive);
    free(objects);
    free(pending);
    free(members);
    free(fragments);
    free(payload.data);
    free(references.data);
    free(records.data);
    free(metadata.data);
    free(artifact.data);
    free(report.data);
}
static char *space(char *p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
        ++p;
    return p;
}
static char *json_string(char *p) {
    unsigned i;
    if (*p++ != '"')
        sf_die("invalid payload report JSON string");
    while (*p && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c < 32 || c >= 127)
            sf_die("payload report must be ASCII JSON");
        if (c == '\\') {
            c = (unsigned char)*p++;
            if (!c)
                sf_die("truncated payload report escape");
            if (c == 'u') {
                for (i = 0; i < 4; ++i) {
                    if (!isxdigit((unsigned char)*p))
                        sf_die("invalid JSON Unicode escape");
                    ++p;
                }
            } else if (!strchr("\"\\/bfnrt", c))
                sf_die("invalid JSON escape");
        }
    }
    if (*p != '"')
        sf_die("unterminated payload report string");
    return p + 1;
}
static char *json_value(char *p, unsigned depth) {
    char close, *next;
    p = space(p);
    if (depth > 16)
        sf_die("payload report nesting limit");
    if (*p == '"')
        return json_string(p);
    if (*p == '{' || *p == '[') {
        close = *p++ == '{' ? '}' : ']';
        p = space(p);
        if (*p == close)
            return p + 1;
        for (;;) {
            if (close == '}') {
                p = space(json_string(p));
                if (*p++ != ':')
                    sf_die("invalid JSON object");
            }
            p = space(json_value(p, depth + 1));
            if (*p == close)
                return p + 1;
            if (*p++ != ',')
                sf_die("invalid JSON separator");
            p = space(p);
        }
    }
    if (*p == '-' || isdigit((unsigned char)*p)) {
        if (*p == '-')
            ++p;
        if (*p == '0')
            ++p;
        else {
            if (*p < '1' || *p > '9')
                sf_die("invalid JSON number");
            while (isdigit((unsigned char)*p))
                ++p;
        }
        if (*p == '.') {
            ++p;
            if (!isdigit((unsigned char)*p))
                sf_die("invalid JSON fraction");
            while (isdigit((unsigned char)*p))
                ++p;
        }
        if (*p == 'e' || *p == 'E') {
            ++p;
            if (*p == '+' || *p == '-')
                ++p;
            if (!isdigit((unsigned char)*p))
                sf_die("invalid JSON exponent");
            while (isdigit((unsigned char)*p))
                ++p;
        }
        return p;
    }
    for (next = "true"; next;
         next = !strcmp(next, "true") ? "false" : !strcmp(next, "false") ? "null" : NULL)
        if (!strncmp(p, next, strlen(next)))
            return p + strlen(next);
    sf_die("invalid payload report JSON value");
    return NULL;
}
static char *report_hash(char *text) {
    char *p = space(text), *key, *end, *hash = NULL;
    size_t n;
    if (*p++ != '{')
        sf_die("payload report must be an object");
    p = space(p);
    if (*p != '}')
        for (;;) {
            key = p;
            end = json_string(p);
            n = (size_t)(end - key);
            if (memchr(key, '\\', n))
                sf_die("escaped payload report key");
            p = space(end);
            if (*p++ != ':')
                sf_die("invalid payload report member");
            p = space(p);
            if (n == 8 && !memcmp(key, "\"sha256\"", 8)) {
                if (hash || *p != '"' || (size_t)(json_string(p) - p) != 66)
                    sf_die("invalid/duplicate payload sha256");
                hash = p + 1;
            }
            p = space(json_value(p, 0));
            if (*p == '}')
                break;
            if (*p++ != ',')
                sf_die("invalid payload report separator");
            p = space(p);
        }
    if (*p != '}' || *space(p + 1) || !hash)
        sf_die("invalid payload report/sha256");
    return hash;
}
static void stamp(const char *directory) {
    Bytes payload, report, profile, identity = {0};
    char *name, *json, *hash;
    char expected[65], actual[65];
    name = path(directory, "sysfile.payload");
    payload = sf_read(name);
    free(name);
    name = path(directory, "direct-file.payload.json");
    report = sf_read(name);
    free(name);
    name = path(directory, "pascal_runtime.profile");
    profile = sf_read(name);
    free(name);
    json = sf_string((char *)report.data, report.size);
    hash = report_hash(json);
    digest(&payload, expected);
    if (payload.size < 48 || memcmp(payload.data, "ZPU1\1\0", 6) ||
        get_word(payload.data + 6) != payload.size || get_long(payload.data + 8) ||
        get_long(payload.data + 28) != fingerprint(payload.data, payload.size) ||
        memcmp(hash, expected, 64))
        sf_die("invalid or stale SYSFILE payload");
    if (profile.size != 4)
        sf_die("runtime fingerprint must contain four bytes");
    memcpy(payload.data + 8, profile.data, 4);
    put_long(payload.data + 28, fingerprint(payload.data, payload.size));
    digest(&payload, actual);
    memcpy(report.data + (hash - json), actual, 64);
    format(&identity,
           "/* Compiler-private file backend identity. */\n#define SYSFILE_CHECKSUM %luUL\n",
           (unsigned long)get_long(payload.data + 28));
    save(directory, "SYSFILE.ZPU", &payload);
    save(directory, "file-unit.h", &identity);
    save(directory, "direct-file.json", &report);
    fwrite(report.data, 1, report.size, stdout);
    free(payload.data);
    free(report.data);
    free(profile.data);
    free(identity.data);
    free(json);
}
int main(int argc, char **argv) {
    if (argc == 5 && !strcmp(argv[1], "split"))
        prepare("split", argv[2], argv[3], argv[4]);
    else if (argc == 4 && !strcmp(argv[1], "sdk"))
        prepare("sdk", argv[2], argv[3], NULL);
    else if (argc >= 5 && !strcmp(argv[1], "payload"))
        pack(argv[2], argc - 4, argv + 3, argv[argc - 1]);
    else if (argc == 3 && !strcmp(argv[1], "stamp"))
        stamp(argv[2]);
    else
        sf_die("usage: sysfile-tool split INPUT OUTPUT PREFIX | sdk INPUT OUTPUT | payload DIR "
               "OBJECT... LIBRARY | stamp DIR");
    return 0;
}
