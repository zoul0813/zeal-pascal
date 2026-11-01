#ifndef ZP_TRANSFER_H
#define ZP_TRANSFER_H
#include "chain.h"
#include "file_runtime.h"
#include <stddef.h>
enum { ZT_PREPARE,ZT_COMMIT,ZT_DISCARD,ZT_PREPARE_EXECUTE };
/* Caller-owned stack job. Path is already authenticated by TRANSFER_PATH;
 * prepare snapshots it and the expected profile before lending any mappings.
 * After prepare, the caller restores native input ownership before commit. */
typedef struct {
    uint16_t action;
    ChainCaller caller;
    uint16_t path,error,detail;
} TransferJob;
typedef char transfer_job_shape[(sizeof(TransferJob)==26&&offsetof(TransferJob,error)==22&&
    offsetof(TransferJob,detail)==24)?1:-1];
#define ZT_PAGE 0xfe75u
#define ZT_FILE 0xfe77u
#define ZT_MODE 0xfe79u
void transfer_dispatch(TransferJob *job) CHAIN_CALL;
uint8_t transfer_cleanup(FileCleanup *job) CHAIN_CALL;
void transfer_file_call(uint8_t mode,void *job) CHAIN_CALL;
/* Assembly abandons the old stack before install and never returns. */
void transfer_commit(void);
void transfer_install(void);
void transfer_original_pages(uint8_t originals[3]) CHAIN_CALL;
#endif
