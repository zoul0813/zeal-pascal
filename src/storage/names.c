#include "storage/names.h"
#include "common/path.h"
#include "source/catalog.h"

/* Filename options must be available even when compiler banks are parked. */
#ifdef __SDCC
#pragma codeseg BANK2
#pragma constseg BANK2
#endif

static char named_root[128],named_leaf[16];
const char *program_name(const char *root) SOURCE_BLOCK_CALL {
    return named_leaf[0]&&!strcmp(root,named_root)?named_leaf:0;
}
uint8_t set_program_name(const char *root,const char *name) SOURCE_BLOCK_CALL {
    char path[128],directory[128];
    char *leaf;
    const char *extension=strrchr(name,'.');
    size_t length=strlen(name);
    if(length) {
        if(length>15||strchr(name,'/')||strchr(name,':'))return 0;
        strcpy(directory,root);leaf=strrchr(directory,'/');
        if(!leaf)return 0;
        leaf[1]=0;
        if(!path_resolve(name,directory,path))return 0;
        if(extension) {
            if(!same_file_name(extension,".bin")&&!same_file_name(extension,".com"))return 0;
        } else if(length+4>15||strlen(path)+4>127)return 0;
    }
    strcpy(named_root,root);strcpy(named_leaf,name);
    if(length&&!extension)strcat(named_leaf,".bin");
    return 1;
}

static char lowercase(char c) {return c>='A'&&c<='Z'?c+'a'-'A':c;}

uint8_t build_path(const Source *s,char *out,uint8_t chain) SOURCE_BLOCK_CALL {
    char *leaf;
    const char *name;
    if(!path_program(s->path,out,chain))return 0;
    if(chain)return 1;
    leaf=strrchr(out,'/')+1;
    name=program_name(s->path);
    if(name)strcpy(leaf,name);
    else for(;*leaf;++leaf)*leaf=lowercase(*leaf);
    return 1;
}

/* HostFS can alias case variants. Protect every build input before replacing
 * an output, including a Pascal source with a nonstandard extension. */
uint8_t same_file_name(const char *a,const char *b) SOURCE_BLOCK_CALL {
    while(*a&&lowercase(*a)==lowercase(*b)){++a;++b;}
    return lowercase(*a)==lowercase(*b);
}
uint8_t output_conflicts(const Source *s,const Build *build,const char *path) SOURCE_BLOCK_CALL {
    Source input;uint8_t id;
    if(same_file_name(s->path,path))return 1;
    if(sources_valid(&build->sources))for(id=0;id<build->sources.count;++id) {
        if(!sources_get(&build->sources,id,&input)||same_file_name(input.path,path))return 1;
    }
    return 0;
}
