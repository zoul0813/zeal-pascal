#ifndef SYSFILE_OBJECTS_H
#define SYSFILE_OBJECTS_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned char *data;
    size_t size, capacity;
} Bytes;
typedef struct {
    unsigned site, symbol, target, addend, kind;
} Relocation;
typedef struct {
    char *name;
    unsigned size, absolute, address, base;
    unsigned char *data, *initialized;
    Relocation *relocations;
    size_t count;
} Area;
typedef struct {
    char *name;
    unsigned defined, value;
    int area;
} Symbol;
typedef struct {
    Area *areas;
    size_t area_count;
    Symbol *symbols;
    size_t symbol_count;
} Object;
typedef struct {
    char *name;
    Object *object;
} Member;
typedef struct {
    Member *members;
    size_t count;
} Archive;

void sf_die(const char *message);
void *sf_resize(void *p, size_t count, size_t width);
char *sf_string(const char *text, size_t size);
Bytes sf_read(const char *path);
void sf_append(Bytes *bytes, const void *data, size_t size);
Object *sf_object(const unsigned char *data, size_t size);
Archive sf_archive(const unsigned char *data, size_t size);
void sf_object_free(Object *object);
void sf_archive_free(Archive *archive);
#endif
