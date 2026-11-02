#include <zos_errors.h>

static void save_path_name(char name[128],char parent[128],uint8_t kind,uint16_t prefix) {
    uint16_t n=9,left,count;
    strcpy(name,"H:/P0000/");name[4]='0'+kind;name[5]='0'+prefix/100;
    name[6]='0'+prefix/10%10;name[7]='0'+prefix%10;
    memcpy(parent,name,8);parent[8]=0;
    while(n<prefix) {
        left=prefix-n;count=left>8?7:left-1;
        if(left-count-1==1)--count;
        while(count--)name[n++]='a';
        name[n++]='/';
    }
    strcpy(name+n,"x.pas");
}
static void native_save_paths(void) {
    Source item,prior;CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};
    char name[128],parent[128],backup[128];
    uint16_t prefix,n;uint8_t kind,ok,error,pages;int8_t fd;zos_stat_t info;
    for(kind=0;kind<5;++kind)for(prefix=111;prefix<=112;++prefix) {
        test_stage=62+kind;save_path_name(name,parent,kind,prefix);
        check(stat(parent,&info)==ERR_NO_SUCH_ENTRY);
        check(source_new(&item,kind==1||kind==2?"H:/PATHORIG.PAS":name));
        check(source_import(&item,(const uint8_t*)"begin end.",10,&d));item.cursor=4;item.previous=2;prior=item;
        if(kind>=3) {
            options.kind=kind==4?COMPILE_CHAIN:COMPILE_PROGRAM;
            check(compile_project(&item,&b,&d,0,&options));
            if(!b.valid) {source_close(&item);continue;}
        }
        pages=pages_used();
        if(kind==1)ok=save_source_as(&item,name,1,0);
        else if(kind==2)ok=save_block(&item,0,item.length,name,1,0);
        else if(kind>=3)ok=save_build(&item,&b,0);
        else ok=save_source(&item,1,0);
        error=platform_error;
        check(pages_used()==pages&&item.cursor==prior.cursor&&item.previous==prior.previous);
        check(ok&&!error);check(stat(parent,&info)==0&&D_ISDIR(info.s_flags));
        if(kind<2) {
            check(!item.dirty&&!strcmp(item.path,name));
            strcpy(backup,name);strcpy(backup+strlen(backup)-3,"BAK");
            check(seed(backup,"older backup"));
            check(source_replace(&item,0,0,(const uint8_t*)"{dirty}\n",8));
            check(save_source(&item,1,0));fd=open(backup,O_RDONLY);check(fd>=0);
            if(fd>=0) {n=32;check(!read(fd,console_scratch,&n)&&n==10&&!memcmp(console_scratch,"begin end.",10));check(!close(fd));}
        } else check(!memcmp(&item,&prior,sizeof(item)));
        if(kind>=3)build_close(&b);
        source_close(&item);check(pages_used()==code_required);
    }
}
