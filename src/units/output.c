#include "units/output.h"
#include "units/internal.h"
#include "output_transport.h"
#include "path.h"
#include <zos_vfs.h>
#include <zos_errors.h>

static char scratch[128];
static uint8_t owned;

uint8_t unit_output_discard(void) ROUTINES {
    uint8_t error;
    if(!owned)return 1;
    error=rm(scratch);
    if(error){platform_error=error;return 0;}
    owned=0;return 1;
}
int8_t unit_output_begin(const Source *root) ROUTINES {
    uint16_t length;uint8_t error;zos_stat_t info;int8_t fd;
    if(!unit_output_discard())return -1;
    if(!path_program(root->path,scratch,0)) {platform_error=ERR_INVALID_PATH;return -1;}
    length=(uint16_t)strlen(scratch);strcpy(scratch+length-3,"$PU");
    if(!strcmp(scratch,root->path)){platform_error=ERR_INVALID_PATH;return -1;}
    error=stat(scratch,&info);
    if(error!=ERR_NO_SUCH_ENTRY){platform_error=error?error:ERR_ALREADY_EXIST;return -1;}
    fd=open(scratch,O_WRONLY|O_CREAT);
    if(fd<0){platform_error=(uint8_t)-fd;return -1;}
    owned=1;return fd;
}
uint8_t unit_output_publish(const Source *root,char *published) ROUTINES {
    uint8_t header[ZPU_HEADER_BYTES+11],length,i,error;uint16_t directory;
    int8_t fd;zos_stat_t info;
    if(!owned){platform_error=ERR_INVALID_PARAMETER;return 0;}
    fd=output_open_path(scratch,O_RDONLY);if(fd<0)return 0;
    error=output_read_buffer(fd,header,sizeof(header));
    if(!output_finish(fd,error))return 0;
    length=header[18];
    if(memcmp(header,"ZPU1",4)||header[4]!=ZPU_ABI||header[5]||!length||length>11) {
        platform_error=ERR_ENTRY_CORRUPTED;return 0;
    }
    strcpy(published,root->path);directory=(uint16_t)strlen(published);
    while(directory&&published[directory-1]!='/')--directory;
    if(directory+length+4>127){platform_error=ERR_INVALID_PATH;return 0;}
    for(i=0;i<length;++i) {
        uint8_t c=header[ZPU_HEADER_BYTES+i];
        if(!((c>='a'&&c<='z')||c=='_'||(i&&c>='0'&&c<='9'))) {
            platform_error=ERR_ENTRY_CORRUPTED;return 0;
        }
        published[directory+i]=(char)(c>='a'&&c<='z'?c-'a'+'A':c);
    }
    strcpy(published+directory+length,".ZPU");
    if(!strcmp(published,root->path)||!strcmp(published,scratch)) {
        platform_error=ERR_INVALID_PATH;return 0;
    }
    error=stat(published,&info);
    if(!error) {
        if(!D_ISFILE(info.s_flags)){platform_error=ERR_NOT_A_FILE;return 0;}
        if(!output_poll())return 0;
        error=rm(published);
    } else if(error==ERR_NO_SUCH_ENTRY)error=0;
    if(error){platform_error=error;return 0;}
    /* Match executable publication: source and completed scratch survive
     * compilation failure; output replacement is a checked sequential copy. */
    return output_copy_file(scratch,published);
}
