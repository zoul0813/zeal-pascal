#include "harness.h"
#include "compiler_members.h"
#include "compiler_groups.h"
#include "output_transport.h"
#include "file_runtime.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
volatile uint8_t test_group_hash[32];
volatile uint8_t test_phase_hash[32];
volatile uint8_t test_late_hash[32];
volatile uint8_t test_owner_hash[32];
Source s;Build b;Diagnostic d;
void test_complete(void);
void test_phase_restore_marker(void) NATIVE;
#include "late-cases.h"

static void group_code(uint16_t bytes,uint8_t value,uint16_t maps,uint16_t relocs) {
    uint16_t i,p;
    for(i=0;i<bytes;++i) {
        p=cc.emitted++;
        if(p<0xc000u)far_set(b.pages[2],p-0x8000u,(uint8_t)(value+i));
        else far_set(cc.stage_page,p-0xc000u,(uint8_t)(value+i));
    }
    cc.entries+=maps;cc.relocations+=relocs;
}
static void group_cases(void) {
    CompilerGroups groups;CompilerGroup record;CompilerMember member;
    char path[]="H:/GROUPS.SPL";
    uint8_t id,ordinal,owned=pages_used();uint16_t i;
    int8_t fd=output_open_path(path,O_RDWR|O_CREAT|O_TRUNC);
    check(fd>=0);if(fd<0)return;
    cc_groups_init(&groups);check(cc_groups_begin(&groups,fd));
    if(groups.store.failed) {check(cc_groups_discard(&groups));return;}
    cc.routine_count=255;cc.emitted=0xbff0;cc.entries=2;cc.relocations=3;
    check(cc_group_open(&groups,0)==0);check(cc_group_member_begin(&groups,0,1));
    group_code(8,0x10,1,1);
    check(cc_group_open(&groups,1)==1);check(cc_group_member_begin(&groups,1,2));
    group_code(20,0x30,2,2);check(cc_group_member_end(&groups,1,2,0xbffc));
    check(cc.emitted==0xbff8&&cc.entries==3&&cc.relocations==4&&groups.active==1);
    check(cc_group_member_begin(&groups,1,3));group_code(40,0x40,1,1);
    check(cc_group_member_end(&groups,1,3,0xc000));check(cc_group_close(&groups,1));
    check(cc.emitted==0xc020&&cc.entries==3&&cc.relocations==4&&groups.active==1);
    group_code(8,0x60,1,1);check(cc_group_member_end(&groups,0,1,0xc020));
    check(cc.emitted==0xbff0&&cc.entries==2&&cc.relocations==3&&!groups.active);
    check(cc_group_member_begin(&groups,0,255));group_code(24,0x70,1,1);
    check(cc_group_member_end(&groups,0,255,0xbff0));check(cc_group_close(&groups,0));
    check(cc.emitted==0xc028&&cc.entries==2&&cc.relocations==3&&!groups.active);
    check(cc_group_get(&groups,0,&record)&&record.origin==0xbff0&&record.bytes==56&&record.members==2&&
          record.parent_group==255&&record.parent_member==255&&record.owner==0&&record.parent==0&&record.closed);
    check(cc_group_get(&groups,1,&record)&&record.origin==0xbff8&&record.bytes==40&&record.members==2&&
          record.parent_group==0&&record.parent_member==0&&record.owner==1&&record.parent==1&&record.closed);
    check(cc_group_member_get(&groups,1,&id,&ordinal,&member)&&id==0&&ordinal==0&&member.offset==168&&
          member.origin==0xbff0&&member.bytes==56&&member.entry==0xc020&&member.maps==2&&member.relocs==2);
    memcpy((void*)test_group_hash,member.hash,32);
    check(cc_group_member_get(&groups,255,&id,&ordinal,&member)&&id==0&&ordinal==1&&member.offset==288&&member.bytes==24);
    check(groups.store.length==356);
    for(i=0;i<56;++i) {
        uint16_t p=0xbff0u+i;
        if(p<0xc000u) {if(far_get(b.pages[2],p-0x8000u))break;}
        else if(far_get(cc.stage_page,p-0xc000u))break;
    }
    check(i==56);
    for(i=0;i<PAGE_SIZE;++i) {
        if(far_get(b.pages[2],i)!=(i>=0x3ff0u?0:(uint8_t)(i*3+9))||
           far_get(cc.stage_page,i)!=(i<40?0:(uint8_t)(i*5+1))||
           far_get(b.map_page,i)!=(uint8_t)(i*7+3)||far_get(cc.workspace,i)!=(uint8_t)(i*11+5))break;
    }
    check(i==PAGE_SIZE);
    check(cc_groups_discard(&groups));check(groups.page==255&&groups.store.fd==-1&&pages_used()==owned);
}
static void phase_cases(void) {
    uint8_t before[32],after[32],machine_before[7],machine_after[7],owned=pages_used(),status=255,bad=0,ok;
    Source original;Build prior;
    char path[]="H:/PHASE.PAS",text[]="begin end.";
    int8_t fd;
    test_machine(machine_before);fd=open("H:/PHASE.BAD",O_RDONLY);
    if(fd>=0) {bad=1;check(!close(fd));}
    check(source_new(&s,path));if(s.page==255)return;
    check(source_replace(&s,0,0,(const uint8_t*)text,10));original=s;
    code_hash(before);
    check(code_required==14&&code_phase_fd>=0&&code_phase_park());
    check(code_pages[13]==255&&code_phase_pending[0]==255&&pages_used()==owned); /* source owns the freed slot's count */
    test_phase_restore_marker();
    if(bad) {
        prior=b;
        check(!compile(&s,&b,&d)&&d.code==COMPILE_PLATFORM_FAILURE&&boot_error==ERR_ENTRY_CORRUPTED);
        check(!memcmp(&s,&original,sizeof(s))&&!memcmp(&b,&prior,sizeof(b))&&code_pages[13]==255&&code_phase_pending[0]==255&&pages_used()==owned);
        test_phase_restore_marker();
    }
    check(code_phase_restore()&&code_pages[13]!=255&&code_phase_pending[0]==255&&pages_used()==owned+1);
    code_hash(after);check(!memcmp(before,after,32));memcpy((void*)test_phase_hash,after,32);
    ok=compile(&s,&b,&d);check(ok&&b.valid);
    if(ok) {
        check(run_build(&s,&b)&&ram_result[26]==1);
        check(save_build(&s,&b,0));check(run_saved(&s,&b,&status)&&status==0);
        build_close(&b);
    }
    check(!memcmp(&s,&original,sizeof(s)));source_close(&s);check(pages_used()==owned);
    test_machine(machine_after);check(!memcmp(machine_before,machine_after,7));
}
int app_main(void) {
    CompilerMemberStore store;
    CompilerMember member,first;
    uint16_t i;
    uint8_t before[7],after[7],owned,exit_mode=0,released;
    char path[]="H:/MEMBERS.SPL",late_path[]="H:/LATE.SPL",helper_path[]="H:/HELPERS.SPL",owner_path[]="H:/OWNERS.SPL";
    int8_t fd;
    if(!console_open())return 1;
    fd=open("H:/RETURN.TEST",O_RDONLY);if(fd>=0) {exit_mode=1;check(!close(fd));}
    owned=pages_used();test_machine(before);
    memset(&cc,0,sizeof(cc));memset(&member,0,sizeof(member));
    b.pages[2]=page_new(PAGE_IMAGE);b.map_page=page_new(PAGE_MAP);
    cc.workspace=page_new(PAGE_COMPILER);cc.stage_page=page_new(PAGE_COMPILER);
    check(b.pages[2]!=255&&b.map_page!=255&&cc.workspace!=255&&cc.stage_page!=255);
    if(b.pages[2]==255||b.map_page==255||cc.workspace==255||cc.stage_page==255)goto done;
    cc.image=&b;cc.emitted=0xc201;cc.file_bytes=71;cc.entries=5;cc.relocations=7;cc.routine_count=2;
    for(i=0;i<PAGE_SIZE;++i) {
        far_set(b.pages[2],i,(uint8_t)(i*3+9));far_set(cc.stage_page,i,(uint8_t)(i*5+1));
        far_set(b.map_page,i,(uint8_t)(i*7+3));far_set(cc.workspace,i,(uint8_t)(i*11+5));
    }
    member.origin=0xbff1;member.entry=0xc000;member.map_first=2;member.reloc_first=3;
    fd=output_open_path(path,O_RDWR|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd<0)goto done;
    check(cc_members_begin(&store,fd));test_stage=1;
    check(cc_members_flush(&store,&member,0,0));
    check(!store.failed&&store.length==616&&member.offset==0&&member.bytes==528&&member.maps==3&&member.relocs==4);
    memcpy((void*)test_hash,member.hash,32);first=member;
    check(cc_members_flush(&store,&member,0,0));
    check(!store.failed&&store.length==1232&&member.offset==616&&!memcmp(first.hash,member.hash,32));
    check(cc.emitted==0xc201&&cc.entries==5&&cc.relocations==7&&cc.routine_count==2);
    check(output_finish(fd,1));test_stage=2;
    /* Preserve the complete parent prefixes and global routine records. */
    for(i=0;i<PAGE_SIZE;++i) {
        if(far_get(b.pages[2],i)!=(uint8_t)(i*3+9)||far_get(cc.stage_page,i)!=(uint8_t)(i*5+1)||
           far_get(b.map_page,i)!=(uint8_t)(i*7+3)||far_get(cc.workspace,i)!=(uint8_t)(i*11+5))break;
    }
    check(i==PAGE_SIZE);
    fd=output_open_path(path,O_RDWR);check(fd>=0);
    if(fd>=0) {
        check(!cc_members_begin(&store,fd)&&store.failed&&platform_error);
        check(output_finish(fd,1));
    }
    test_stage=3;group_cases();late_cases(late_path,&d,(uint8_t*)test_late_hash);
    root_helper_cases(helper_path,&d);
    remaining_helper_cases(owner_path,&d,(uint8_t*)test_owner_hash);
    member_address_guard(&d);
done:
    if(b.pages[2]!=255) {released=page_drop(b.pages[2]);check(released);if(released)b.pages[2]=255;}
    if(b.map_page!=255) {released=page_drop(b.map_page);check(released);if(released)b.map_page=255;}
    if(cc.workspace!=255) {released=page_drop(cc.workspace);check(released);if(released)cc.workspace=255;}
    if(cc.stage_page!=255) {released=page_drop(cc.stage_page);check(released);if(released)cc.stage_page=255;}
    check(pages_used()==owned);test_machine(after);check(!memcmp(before,after,7));
    if(b.pages[2]==255&&b.map_page==255&&cc.workspace==255&&cc.stage_page==255) {
        memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=255;b.sources.page=255;
        test_stage=4;phase_cases();
    }
    test_stage=6;test_done=255;if(exit_mode)return test_failed?1:0;
    test_complete();return 0;
}
