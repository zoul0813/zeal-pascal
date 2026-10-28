#ifndef ZP_H
#define ZP_H
#include <stdint.h>
#include <string.h>
#ifdef __SDCC
#define NATIVE __sdcccall(1)
#else
#define NATIVE
#endif
#ifdef __SDCC
#define SOURCE_BLOCK_CALL __banked
#else
#define SOURCE_BLOCK_CALL
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
#define RT_RANGE 0x40dbu
#define RT_CHARCOPY 0x40deu
#define RT_STRINGCHAR 0x40e1u
#define RT_TEXTCMP 0x40e4u
#define RT_PUSHSTRING 0x40e7u
#define RT_ARGSTRING 0x40eau
#define RT_HOLDSTRING 0x40edu
#define RT_CONCAT 0x40f0u
#define RT_INDEX 0x40f3u
#define RT_SUBSTRING 0x40f6u
#define RT_POS 0x40f9u
#define RT_DELETE 0x40fcu
#define RT_INSERT 0x40ffu
#define RT_MOVE 0x4102u
#define RT_SETZERO 0x4105u
#define RT_SETLOAD 0x4108u
#define RT_SETSTORE 0x410bu
#define RT_SETADD 0x410eu
#define RT_SETBIN 0x4111u
#define RT_SETCMP 0x4114u
#define RT_SETMEM 0x4117u
#define RT_GUARD 0x411au
#define RT_ALLOC 0x411du
#define RT_FREE 0x4120u
#define RT_RELEASE 0x4123u
#define RT_AVAIL 0x4126u
#define RT_STACKSET 0x4129u
#define RT_HEAPPTR 0x412cu
#define RT_RECURPTR 0x412eu
#define RT_FILL 0x4130u
#define RT_REAL 0x4133u
#define RT_REAL_BUFFER 0x4136u
#define RT_FORMAT 0x413cu
#define RT_FIELD 0x413fu
#define RT_STR 0x4142u
#define RT_VAL 0x4145u
#define RT_INPUT_BEGIN 0x4148u
#define RT_INPUT_VALUE 0x414bu
#define RT_INPUT_END 0x414eu
#define RT_BUFLEN 0x4151u
#define RT_CBREAK 0x4152u
#define RT_IOCALL 0x4153u
#define RT_IORESULT 0x4156u
#define RT_FILECLEANUP 0x4159u
#define RT_HEAPFLOOR 0x415cu
#define RT_IOFAIL 0x415eu
#define RT_ERROR 0x4161u
#define RT_IOSTATUS 0x4164u
#define RT_PARAMTAIL 0xff00u
#define DEFAULT_SOURCE "T:/src/pascal/UNTITLED.pas"
enum { PAGE_SOURCE=1, PAGE_IMAGE, PAGE_MAP, PAGE_RUN, PAGE_SNAPSHOT, PAGE_COMPILER, PAGE_WORKER, PAGE_SOURCES };

typedef struct {
    uint8_t page, dirty;
    uint16_t length, cursor, previous;
    uint32_t revision;
    char path[128];
} Source;
/* An owner of source pages and their identities. Descriptors live in an owned
 * far page; the near handle authenticates its current immutable contents. */
typedef struct { uint8_t page,count; uint32_t lifetime,version; } Sources;
typedef struct {
    uint8_t code;
    uint16_t start, end;
} Diagnostic;
/* Cancellation is a job outcome, never a Pascal syntax diagnostic. */
#define COMPILE_INTERRUPTED 255u
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
    Sources sources; /* canonical source snapshots retained with this build */
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
extern uint16_t ram_io_detail;
extern uint8_t ram_cleanup_detail;
extern uint8_t ram_pages[3];
extern uint16_t ram_entry;
extern volatile uint32_t page_versions[64];
extern const uint8_t runtime_start[], runtime_end[];
extern const uint8_t trampoline_start[], trampoline_end[];
extern const uint8_t file_start[],file_end[],file_gate_start[],file_gate_end[];

uint8_t page_new(uint8_t role);
uint8_t page_drop(uint8_t page);
uint8_t page_role(uint8_t page);
uint32_t page_lifetime(uint8_t page);
uint8_t pages_used(void);
void page_initialized(uint8_t page, uint16_t extent);
uint8_t source_new(Source *s, const char *path);
void source_close(Source *s);
uint8_t source_replace(Source *s, uint16_t first, uint16_t last,
                       const uint8_t *text, uint16_t length);
uint8_t source_insert_from(Source *s,uint16_t at,const Source *from,uint16_t first,uint16_t last,uint16_t spaces) SOURCE_BLOCK_CALL;
uint8_t source_move(Source *s,uint16_t first,uint16_t last,uint16_t at,uint16_t spaces) SOURCE_BLOCK_CALL;
uint8_t source_import(Source *s, const uint8_t *bytes, uint16_t length, Diagnostic *d);
uint8_t import_begin(SourceImport *in,const char *path,Diagnostic *d);
uint8_t import_byte(SourceImport *in,uint8_t value,Diagnostic *d);
uint8_t import_finish(SourceImport *in,Source *s,Diagnostic *d);
void source_location(const Source *s, uint16_t position, uint16_t *line, uint16_t *column);
uint16_t source_rebase(uint16_t position,uint16_t first,uint16_t last,uint16_t inserted,uint8_t right);
uint8_t source_snapshot(const Source *s, Source *snapshot);
uint16_t source_editor_offset(const Source *s, uint16_t canonical);
void sha_init(Sha256 *s);
void sha_byte(Sha256 *s, uint8_t b);
void sha_end(Sha256 *s, uint8_t out[32]);
void source_hash(const Source *s, uint8_t out[32]);
uint8_t compile(const Source *s, Build *b, Diagnostic *d);
uint8_t compile_project(const Source *root,const Sources *buffers,Build *b,Diagnostic *d,uint8_t interruptible);
uint8_t compile_interrupt_reason(void);
/* On compile failure these resident values survive until the next compile.
 * A missing/unreadable source has no authenticated hash (NULL). */
const char *compile_error_path(void);
const uint8_t *compile_error_hash(void);
/* Path arguments must live in retained data/stack, like the source catalog. */
uint8_t source_path(const char *name,char *out) SOURCE_BLOCK_CALL;
/* Resolve against the native current directory without a source extension. */
uint8_t native_path(const char *name,char *out) SOURCE_BLOCK_CALL;
void build_close(Build *b);
uint8_t build_check(const Source *s, const Build *b);
uint8_t build_ready(const Source *s,const Build *b);
uint8_t build_locate(const Source *s, const Build *b, uint16_t pc, uint16_t *position);
uint8_t build_location(const Source *root,const Build *b,uint16_t pc,uint8_t *file,uint16_t *position);
uint8_t run_build(const Source *s, const Build *b);
uint8_t save_build(const Source *s,const Build *b);
uint8_t save_source(Source *s,uint8_t replace_backup);
uint8_t save_source_as(Source *s,const char *name,uint8_t replace_backup);
uint8_t load_source(Source *s,const char *path,Diagnostic *d);
uint8_t read_block(Source *block,const char *path,Diagnostic *d);
uint8_t save_block(const Source *s,uint16_t first,uint16_t last,const char *path,uint8_t replace);
uint8_t run_saved(const Source *s,const Build *b,uint8_t *status);
const char *output_path(void);
const char *output_recovery(void);
uint8_t image_get(const Build *b, uint16_t offset);

uint8_t console_open(void);
uint8_t platform_supported(void);
void console_close(void);
uint8_t key_read(void);
/* Long-operation polling retains ordinary decoded keys for later readers.
 * Return1 for Ctrl-U,2 when the32-key typeahead buffer is full. */
uint8_t key_interrupt(void);
void key_typeahead_hold(uint8_t hold);
uint8_t key_decode(uint8_t raw);
void screen_clear(void);
void screen_at(uint8_t x, uint8_t y);
void screen_mark(uint8_t highlighted);
void output(const char *text);
void output_number(uint16_t n);
void output_hex(uint16_t n,uint8_t digits);
extern uint8_t screen_width, screen_height, platform_error;
#endif
