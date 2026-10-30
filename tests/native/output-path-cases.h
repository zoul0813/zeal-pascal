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

/* Preparation happens before the compiler, which may read unrelated includes.
 * Publication must reuse exactly the reserved directory and original targets. */
static void prepared_output_cases(void) {
    unsigned kind,pages,before_events;uint8_t ok;int f;
    char reserved[128],foreign[128];Source loaded,changed;Diagnostic error;Sources buffers;uint8_t hash[32];
    for(kind=3;kind<5;++kind) {
        fail_at=cancel_at=shorts=bad_io=fail_read=fail_write=0;setup(kind,3);pages=owned();
        check(prepare_build(&source,0,kind==4,1));strcpy(reserved,output_recovery());
        check(*reserved&&find(reserved)>=0&&!opened()&&owned()==pages);
        before_events=events;
        check(!prepare_build(&source,0,kind==4,1)&&events==before_events&&!strcmp(reserved,output_recovery()));
        /* Read an existing include and a missing path without retargeting the
         * pending transaction. Missing parents must not be created by a read. */
        seed("T:/parts.inc",(const uint8_t*)"const answer=42;\n",16);
        check(read_block(&loaded,"T:/parts.inc",&error));source_close(&loaded);
        check(!read_block(&loaded,"T:/missing/no.inc",&error)&&find("T:/missing")<0);
        check(!strcmp(reserved,output_recovery())&&owned()==pages&&!opened());
        changed=source;strcpy(changed.path,"T:/different.pas");
        before_events=events;
        check(!save_build(&changed,&build,1)&&events==before_events&&!strcmp(reserved,output_recovery()));
        check(!save_source(&source,1,1)&&!strcmp(reserved,output_recovery()));
        check(!save_block(&source,0,17,"T:/other.pas",1,1)&&!strcmp(reserved,output_recovery()));
        check(save_build(&source,&build,1)&&find(reserved)<0&&!*output_recovery());
        check(!memcmp(&source,&before,sizeof(source))&&owned()==pages&&!opened());
        f=find(target[0]);check(f>=0&&files[f].length==build.length);
        f=find(target[1]);check(f>=0&&files[f].length==build.map_length);
        check(equals("T:/parts.inc",(const uint8_t*)"const answer=42;\n",16));
    }
    fail_at=cancel_at=0;setup(3,3);pages=owned();
    check(sources_new(&buffers));changed=source;strcpy(changed.path,target[0]);source_hash(&changed,hash);
    check(sources_borrow(&buffers,&changed,hash)==0);before_events=events;
    check(!prepare_build(&source,&buffers,0,1)&&platform_error==ERR_INVALID_PATH&&events==before_events&&!*output_recovery());
    check(sources_close(&buffers)&&owned()==pages);
    check(equals(target[0],old[0],old_length[0])&&equals(target[1],old[1],old_length[1]));
    fail_at=cancel_at=0;setup(3,3);pages=owned();
    check(prepare_build(&source,0,0,1));strcpy(reserved,output_recovery());
    platform_error=ERR_NO_MORE_MEMORY;
    check(cancel_prepared_build()&&platform_error==ERR_NO_MORE_MEMORY&&find(reserved)<0&&!*output_recovery());
    check(equals(target[0],old[0],old_length[0])&&equals(target[1],old[1],old_length[1]));
    check(!memcmp(&source,&before,sizeof(source))&&owned()==pages);
    /* An undeletable reservation becomes recovery, never a lock on saving
     * dirty source. Foreign members must never be recursively erased. */
    check(prepare_build(&source,0,0,1));strcpy(reserved,output_recovery());
    strcpy(foreign,reserved);strcat(foreign,"/FOREIGN");seed(foreign,(const uint8_t*)"keep",4);
    check(!cancel_prepared_build()&&!strcmp(reserved,output_recovery())&&equals(foreign,(const uint8_t*)"keep",4));
    before_events=events;check(cancel_prepared_build()&&events==before_events);
    check(save_source(&source,1,1)&&!*output_recovery());
    check(find(reserved)>=0&&equals(foreign,(const uint8_t*)"keep",4)&&equals(source.path,fresh,sizeof(fresh)));
    check(!rm(foreign)&&!rm(reserved));
    check(prepare_build(&source,0,0,1));strcpy(reserved,output_recovery());fail_at=events+1;
    check(!cancel_prepared_build()&&!strcmp(reserved,output_recovery()));
    fail_at=0;check(save_block(&source,0,17,"T:/other.pas",1,1));
    check(prepare_build(&source,0,0,1)&&strcmp(reserved,output_recovery()));
    check(cancel_prepared_build()&&find(reserved)>=0&&!opened()&&owned()==pages);
    check(!rm(reserved));
    check(prepare_build(&source,0,0,1));strcpy(reserved,output_recovery());cancel_at=polls+1;
    ok=save_build(&source,&build,1);check(!ok&&output_interrupt_reason()==1);
    check(!strcmp(reserved,output_recovery())&&find(reserved)>=0&&owned()==pages);
    strcpy(foreign,reserved);strcat(foreign,"/NEW.COM");
    check(!rm(foreign)&&!rm(reserved)); /* Publication retains recovery members. */
    fail_at=cancel_at=0;
}

static void single_page_block_read(void) {
    uint8_t held[15],i;Source block;Diagnostic error;unsigned pages;
    fail_at=cancel_at=shorts=bad_io=fail_read=fail_write=0;setup(0,0);
    seed("T:/part.inc",(const uint8_t*)"const answer=42;\n",16);
    for(i=0;i<14;++i) {held[i]=page_new(PAGE_COMPILER);check(held[i]!=255);}
    pages=owned();check(pages==15&&read_block(&block,"T:/part.inc",&error));
    check(block.length==16&&owned()==16&&!opened());source_close(&block);check(owned()==pages);
    held[14]=page_new(PAGE_COMPILER);check(held[14]!=255&&owned()==16);
    check(!read_block(&block,"T:/part.inc",&error)&&error.code==98&&owned()==16&&!opened());
    check(!read_block(&block,"T:/missing.inc",&error)&&owned()==16&&!opened());
    for(i=0;i<15;++i)check(page_drop(held[i]));check(owned()==1&&!memcmp(&source,&before,sizeof(source)));
}
