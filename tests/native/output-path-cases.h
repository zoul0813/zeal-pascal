/* Actual save APIs must reject an unusable transaction path before any VFS
 * operation. Exercise the longest recovery member as well as new outputs. */
static void long_output_name(char path[128],unsigned prefix) {
    unsigned n=3,remaining,count;
    strcpy(path,"T:/");
    while(n<prefix) {
        remaining=prefix-n;check(remaining>=2);
        count=remaining>8?7:remaining-1;
        if(remaining-count-1==1)--count;
        while(count--)path[n++]='a';
        path[n++]='/';
    }
    strcpy(path+n,"x.pas");
}
static void output_path_cases(void) {
    unsigned kind,prefix,pages;uint8_t ok;
    char name[128],primary[128],secondary[128];Source prior;
    for(kind=0;kind<5;++kind)for(prefix=110;prefix<=121;++prefix) {
        fail_at=cancel_at=rollback_fault=shorts=bad_io=fail_read=fail_write=0;
        setup(kind,0);long_output_name(name,prefix);
        strcpy(primary,name);strcpy(secondary,name);
        strcpy(secondary+strlen(secondary)-3,kind>=3?"ZMP":"BAK");
        if(kind>=3)strcpy(primary+strlen(primary)-3,kind==4?"CHN":"COM");
        if(kind!=1&&kind!=2)strcpy(source.path,name);
        prior=source;pages=owned();
        seed(primary,old[0],old_length[0]);
        if(kind!=2)seed(secondary,old[1],old_length[1]);
        events=polls=0;
        if(kind==1)ok=save_source_as(&source,name,1,1);
        else if(kind==2)ok=save_block(&source,17,389,name,1,1);
        else if(kind>=3)ok=save_build(&source,&build,1);
        else ok=save_source(&source,1,1);
        check(!opened()&&owned()==pages&&!memcmp(memory[source.page],fresh,sizeof(fresh)));
        if(prefix>111) {
            check(!ok&&platform_error==ERR_INVALID_PATH&&!events&&!polls);
            check(!memcmp(&source,&prior,sizeof(prior))&&!*output_recovery());
            check(equals(primary,old[0],old_length[0]));
            if(kind!=2)check(equals(secondary,old[1],old_length[1]));
        } else {
            check(ok&&!platform_error&&!*output_recovery());
            if(kind<2) {
                check(!strcmp(source.path,name)&&!source.dirty&&source.cursor==prior.cursor&&source.previous==prior.previous);
                check(equals(primary,fresh,sizeof(fresh))&&equals(secondary,old[0],old_length[0]));
            } else {
                check(!memcmp(&source,&prior,sizeof(prior)));
                if(kind==2)check(equals(primary,fresh+17,372));
                else {int f=find(primary);check(f>=0&&files[f].length==build.length);f=find(secondary);check(f>=0&&files[f].length==build.map_length);}
            }
        }
    }
}
