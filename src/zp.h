#ifndef ZP_H
#define ZP_H
#include <stdint.h>
#include <string.h>
#ifdef __SDCC
#define NATIVE __sdcccall(1)
#else
#define NATIVE
#endif

#define PAGE_SIZE 16384u
#define SOURCE_LIMIT 16383u
#define PREFIX_BYTES 16384u
#define GLOBAL_BASE 0x8000u
#define RT_NORMAL 0x40a0u
#define RT_RAM 0x40a3u
#define RT_DONE 0x40a6u
#define RT_HALT 0x40a9u
#define RT_CHAR 0x40acu
#define RT_INTEGER 0x40afu
#define RT_DIV 0x40b2u
#define RT_MOD 0x40b5u
#define RT_MUL 0x40b8u
#define RT_STACK 0x40bbu
#define RT_PC 0x40beu
#define RT_STRING 0x40c0u
#define RT_COPY 0x40c3u
#define RT_READLINE 0x40c6u
#define RT_READSTRING 0x40c9u
#define RT_COMPARE 0x40ccu
#define RT_STRCMP 0x40cfu
#define RT_BOOLEAN 0x40d2u
#define RT_SHL 0x40d5u
#define RT_SHR 0x40d8u
#define DEFAULT_SOURCE "T:/src/pascal/UNTITLED.pas"
enum { PAGE_SOURCE=1, PAGE_IMAGE, PAGE_MAP, PAGE_RUN, PAGE_SNAPSHOT };

typedef struct {
    uint8_t page, dirty;
    uint16_t length, cursor, previous;
    uint32_t revision;
    char path[128];
} Source;
typedef struct {
    uint8_t code;
    uint16_t start, end;
} Diagnostic;
typedef struct {
    Source staged;
    uint16_t out,line,spaces;
    uint8_t after_cr,eof;
} SourceImport;
typedef struct {
    uint8_t pages[3], map_page, valid;
    uint16_t length, code_origin, code_bytes, globals_bytes, map_length;
    uint32_t revision;
    uint8_t id[32], source_hash[32];
    uint32_t versions[5]; /* image pages, map, original source */
} Build;
typedef struct {
    uint32_t h[8], bytes;
    uint8_t block[64], used;
    uint32_t schedule[16], working[8];
} Sha256;

/* These mapping cores execute in retained page 1, with no window pointers. */
uint8_t far_get(uint8_t page, uint16_t offset) NATIVE;
void far_set(uint8_t page, uint16_t offset, uint8_t value) NATIVE;
void ram_enter(void) NATIVE;
extern uint8_t ram_result[128];
extern uint8_t ram_pages[3];
extern uint16_t ram_entry;
extern volatile uint32_t page_versions[64];
extern const uint8_t runtime_start[], runtime_end[];
extern const uint8_t trampoline_start[], trampoline_end[];

uint8_t page_new(uint8_t role);
uint8_t page_drop(uint8_t page);
uint8_t pages_used(void);
void page_initialized(uint8_t page, uint16_t extent);
uint8_t source_new(Source *s, const char *path);
void source_close(Source *s);
uint8_t source_replace(Source *s, uint16_t first, uint16_t last,
                       const uint8_t *text, uint16_t length);
uint8_t source_import(Source *s, const uint8_t *bytes, uint16_t length, Diagnostic *d);
uint8_t import_begin(SourceImport *in,const char *path,Diagnostic *d);
uint8_t import_byte(SourceImport *in,uint8_t value,Diagnostic *d);
uint8_t import_finish(SourceImport *in,Source *s,Diagnostic *d);
void source_location(const Source *s, uint16_t position, uint16_t *line, uint16_t *column);
uint8_t source_snapshot(const Source *s, Source *snapshot);
uint16_t source_editor_offset(const Source *s, uint16_t canonical);
void sha_init(Sha256 *s);
void sha_byte(Sha256 *s, uint8_t b);
void sha_end(Sha256 *s, uint8_t out[32]);
void source_hash(const Source *s, uint8_t out[32]);
uint8_t compile(const Source *s, Build *b, Diagnostic *d);
void build_close(Build *b);
uint8_t build_check(const Source *s, const Build *b);
uint8_t build_ready(const Source *s,const Build *b);
uint8_t build_locate(const Source *s, const Build *b, uint16_t pc, uint16_t *position);
uint8_t run_build(const Source *s, const Build *b);
uint8_t save_build(const Source *s,const Build *b);
uint8_t save_source(Source *s,uint8_t replace_backup);
uint8_t load_source(Source *s,const char *path,Diagnostic *d);
uint8_t run_saved(const Source *s,const Build *b,uint8_t *status);
const char *output_path(void);
const char *output_recovery(void);
uint8_t image_get(const Build *b, uint16_t offset);

uint8_t console_open(void);
void console_close(void);
uint8_t key_read(void);
uint8_t key_decode(uint8_t raw);
void screen_clear(void);
void screen_at(uint8_t x, uint8_t y);
void output(const char *text);
void output_number(uint16_t n);
void output_hex(uint16_t n,uint8_t digits);
extern uint8_t screen_width, screen_height, platform_error;
#endif
