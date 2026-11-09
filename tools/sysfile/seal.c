/* Seal checksums/runtime identity on an assembler-authored ZPU byte stream. */
#define _POSIX_C_SOURCE 200809L
#include "../host/sha256.h"
#include <stddef.h>
#include <stdint.h>
typedef struct { unsigned char *data; size_t size,capacity; } Bytes;
static void sf_die(const char *message);
static void *sf_resize(void *p,size_t n,size_t width);
static void sf_append(Bytes *b,const void *p,size_t n);
static char *sf_string(const char *p,size_t n);
static Bytes sf_read(const char *path);
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILE_LIMIT (1024u*1024u)
static void sf_die(const char *message) {
    fprintf(stderr, "sysfile: %s\n", message);
    exit(EXIT_FAILURE);
}
static void * sf_resize(void *p, size_t count, size_t width) {
    void *out;
    if (!width || count > FILE_LIMIT / width)
        sf_die("input/allocation limit exceeded");
    out = realloc(p, count ? count * width : 1);
    if (!out)
        sf_die("out of host memory");
    return out;
}
static char * sf_string(const char *text, size_t size) {
    char *out = sf_resize(NULL, size + 1, 1);
    if (size && memchr(text, 0, size))
        sf_die("embedded NUL in text");
    if (size)
        memcpy(out, text, size);
    out[size] = 0;
    return out;
}
static void sf_append(Bytes *b, const void *data, size_t size) {
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
static Bytes sf_read(const char *path) {
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
static void put_long(unsigned char *p, uint32_t value) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        p[i] = (unsigned char)(value >> (8 * i));
}
static uint32_t get_long(const unsigned char *p) {
    return (uint32_t)get_word(p) | ((uint32_t)get_word(p + 2) << 16);
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


static int valid_name(const unsigned char *p,unsigned size) {
    unsigned i;
    if(!size||!(p[0]=='_'||(p[0]>='a'&&p[0]<='z')))return 0;
    for(i=1;i<size;++i)
        if(!(p[i]=='_'||(p[i]>='a'&&p[i]<='z')||(p[i]>='0'&&p[i]<='9')))return 0;
    return 1;
}

/* Validate the assembler-authored container without interpreting objects,
 * assigning addresses, changing instructions, or applying relocations. */
static void validate_layout(const Bytes *data,unsigned blob,unsigned fragments,
                            unsigned references,unsigned code,unsigned workspace) {
    const unsigned char *p=data->data;
    size_t records=blob+16u,refs=records+8u*fragments,payload=refs+8u*references;
    unsigned i,j,start=0,first=0,at=48u+p[18]+p[19],exports=get_word(p+20);
    if(!p[18]||p[18]>11||!p[19]||p[19]>127||at>blob||
       !valid_name(p+48,p[18])||memchr(p+48+p[18],0,p[19])||get_word(p+22)||get_word(p+24)||
       get_word(p+36)||get_word(p+38)||get_word(p+40)!=at||
       get_word(p+42)!=at||get_word(p+44)!=at||get_word(p+46)||
       get_long(p+blob+12))sf_die("invalid SYSFILE directory");
    for(i=0;i<fragments;++i) {
        const unsigned char *f=p+records+8u*i;
        unsigned size=get_word(f+2),count=get_word(f+6),previous=0;
        if(!size||get_word(f)!=start||get_word(f+4)!=first||
           size>code+workspace-start||count>references-first)
            sf_die("invalid fragment partition");
        if(start>=code) {
            if(i+1!=fragments||start!=code||size!=workspace||count)
                sf_die("invalid workspace fragment");
            for(j=0;j<size;++j)if(p[payload+start+j])sf_die("initialized workspace");
        } else if(size>code-start)sf_die("fragment crosses code boundary");
        for(j=0;j<count;++j) {
            const unsigned char *r=p+refs+8u*(first+j);
            unsigned site=get_word(r),encoded=get_word(r+4),kind=encoded>>14;
            unsigned target=encoded&0xfffu,offset=get_word(r+6),width=kind?1u:2u,value,actual;
            if(get_word(r+2)!=1||(encoded&0x3000u)||kind>2||target>=fragments||
               offset>=get_word(p+records+8u*target+2)||
               site<previous||site>size||width>size-site)
                sf_die("invalid fragment relocation");
            value=(unsigned)payload+get_word(p+records+8u*target)+offset;
            actual=kind?p[payload+start+site]:get_word(p+payload+start+site);
            if(actual!=(kind==2?value>>8:kind==1?value&255u:value))
                sf_die("relocation disagrees with assembled operand");
            previous=site+width;
        }
        start+=size;first+=count;
    }
    if(start!=code+workspace||first!=references)sf_die("incomplete fragment partition");
    for(i=0;i<exports;++i) {
        unsigned length,target,offset;
        if(at>blob||blob-at<14)sf_die("truncated export");
        length=p[at];target=get_word(p+at+8);offset=get_word(p+at+10);
        if(!length||length>blob-at-14||p[at+1]||get_word(p+at+2)||
           get_word(p+at+4)!=65535u||get_word(p+at+6)||get_word(p+at+12)||
           target>=fragments||get_word(p+records+8u*target)>=code||
           offset>=get_word(p+records+8u*target+2))sf_die("invalid executable export");
        if(!valid_name(p+at+14,length))sf_die("invalid export name");
        { unsigned previous=48u+p[18]+p[19];
          for(j=0;j<i;++j) {
              if(p[previous]==length&&!memcmp(p+previous+14,p+at+14,length))
                  sf_die("duplicate export name");
              previous+=14u+p[previous];
          }
        }
        at+=14u+length;
    }
    if(at>blob||blob-at!=fragments)sf_die("invalid capability directory");
    for(i=0;i<fragments;++i)if(p[at+i])sf_die("unexpected SYSFILE capability");
}

/* Assembly emitted the entire ZPU. This step only validates its extents,
 * seals its checksum, and later binds the compiler/runtime fingerprint. */
static void seal(const char *directory,const char *input) {
    Bytes data=sf_read(input),report={0};
    char hash[65];
    unsigned blob,fragments,references,code,workspace;
    size_t expected;
    if(data.size<64||data.size>65534||memcmp(data.data,"ZPU1\1\0",6)||
       get_word(data.data+6)!=data.size||get_long(data.data+8)||get_long(data.data+28))
        sf_die("invalid unsealed ZPU");
    blob=get_word(data.data+26);
    if(blob>data.size-16||memcmp(data.data+blob,"ZPF1\1\0",6))
        sf_die("invalid fragment header");
    fragments=get_word(data.data+blob+6);references=get_word(data.data+blob+8);
    code=get_word(data.data+32);workspace=get_word(data.data+34);
    expected=(size_t)blob+16u+8u*fragments+8u*references+code+workspace;
    if(!fragments||fragments>4096||code+workspace>=32768||
       expected!=data.size||get_word(data.data+blob+10)!=code+workspace)
        sf_die("invalid fragment extent");
    validate_layout(&data,blob,fragments,references,code,workspace);
    put_long(data.data+28,fingerprint(data.data,data.size));digest(&data,hash);
    format(&report,"{\n  \"bytes\": %zu,\n  \"code_bytes\": %u,\n  \"data_bytes\": %u,\n"
        "  \"fragments\": %u,\n  \"relocations\": %u,\n  \"sha256\": \"%s\"\n}\n",
        data.size,code,workspace,fragments,references,hash);
    save(directory,"sysfile.payload",&data);save(directory,"direct-file.payload.json",&report);
    fwrite(report.data,1,report.size,stdout);free(data.data);free(report.data);
}
int main(int argc,char **argv) {
    if(argc==4&&!strcmp(argv[1],"seal"))seal(argv[2],argv[3]);
    else if(argc==3&&!strcmp(argv[1],"stamp"))stamp(argv[2]);
    else sf_die("usage: sysfile-seal seal DIR RAW | stamp DIR");
    return 0;
}
