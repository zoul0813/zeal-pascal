#include "harness.h"
#include "compiler/groups/members.h"
#include "compiler/groups/groups.h"
#include "storage/transport.h"
#include "runtime/files/operations.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done,release_original;
volatile uint16_t test_failures[64],test_index;
Source s;Build b;Diagnostic d;
void test_complete(void);
void test_phase_restore_marker(void) NATIVE;
void test_release_retry(void) NATIVE;

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
    check(cc_group_member_get(&groups,1,&id,&ordinal,&member)&&id==0&&ordinal==0&&member.offset==144&&
          member.origin==0xbff0&&member.bytes==56&&member.entry==0xc020&&member.maps==2&&member.relocs==2);
    check(cc_group_member_get(&groups,255,&id,&ordinal,&member)&&id==0&&ordinal==1&&member.offset==248&&member.bytes==24);
    check(groups.store.length==308);
    for(i=0;i<56;++i) {
        uint16_t p=0xbff0u+i;
        if(p<0xc000u) {if(far_get(b.pages[2],p-0x8000u))break;}
        else if(far_get(cc.stage_page,p-0xc000u))break;
    }
    check(i==56);
    for(i=0;i<PAGE_SIZE;++i) {
        if(far_get(b.pages[2],i)!=(i>=0x3ff0u?0:(uint8_t)(i*3+9))||
           far_get(cc.stage_page,i)!=(i<40?0:(uint8_t)(i*5+1))||
           far_get(b.map_page,i)!=(uint8_t)(i*7+3))break;
    }
    check(i==PAGE_SIZE);
    for(i=0;i<28;++i)if(cc_reloc_get(i)!=(uint8_t)(i*11+5))break;
    check(i==28);
    check(cc_groups_discard(&groups));check(groups.page==255&&groups.store.fd==-1&&pages_used()==owned);
}
static void phase_cases(void) {
    uint8_t machine_before[7],machine_after[7],owned=pages_used(),status=255,bad=0,ok,bank;
    uint8_t phases=0,held[32],count=0,page;
    Source original;Build prior;
    for(bank=0;bank<code_required;++bank)if(CODE_DEFERRED(bank))++phases;
    char path[]="H:/PHASE.PAS",text[]="begin end.",saved[]="H:/PHASE.COM";
    int8_t fd;
    test_machine(machine_before);fd=open("H:/PHASE.BAD",O_RDONLY);
    if(fd>=0) {bad=1;check(!close(fd));}
    check(source_new(&s,path));if(s.page==255)return;
    check(source_replace(&s,0,0,(const uint8_t*)text,10));original=s;
    check(code_capture(0));
    check(code_required>CODE_COMPILE&&code_required<=CODE_BANK_LIMIT);
    check(code_phase_fd>=0&&code_phase_park());
    for(bank=0;bank<code_required;++bank)if(CODE_DEFERRED(bank))
        check(code_pages[bank]==255&&code_phase_pending[bank]==255);
    check(pages_used()==owned+1-phases); /* One source page, all phases parked. */
    test_phase_restore_marker();
    if(bad) {
        prior=b;
        check(!compile(&s,&b,&d)&&d.code==COMPILE_PLATFORM_FAILURE&&boot_error==ERR_ENTRY_CORRUPTED);
        check(!memcmp(&s,&original,sizeof(s))&&!memcmp(&b,&prior,sizeof(b))&&pages_used()==owned+1-phases);
        for(bank=0;bank<code_required;++bank)if(CODE_DEFERRED(bank))
            check(code_pages[bank]==255&&code_phase_pending[bank]==255);
        test_phase_restore_marker();
    }
    /* Leave exactly one native allocator page: the first cold load succeeds
     * and the next fails before touching the caller's source/build. */
    prior=b;
    while(count<32&&palloc(&page)==0)held[count++]=page;
    check(count>0);if(count)check(pfree(held[--count])==0);
    check(!compile(&s,&b,&d)&&d.code==COMPILE_PLATFORM_FAILURE&&boot_error);
    check(!memcmp(&s,&original,sizeof(s))&&!memcmp(&b,&prior,sizeof(b))&&
          code_pages[CODE_EXPRESSIONS]!=255&&pages_used()==owned+2-phases);
    while(count)check(pfree(held[--count])==0);
    check(compile(&s,&b,&d)&&b.valid);build_close(&b);
    check(code_phase_restore()&&pages_used()==owned+1);
    for(bank=0;bank<code_required;++bank)if(CODE_DEFERRED(bank))
        check(code_pages[bank]!=255&&code_phase_pending[bank]==255);
    check(code_capture(1));
    ok=compile(&s,&b,&d);check(ok&&b.valid);
    if(ok) {
        check(run_build(&s,&b)&&ram_result[26]==1);
        check(save_build(&s,&b,0));check(run_saved(&s,&b,&status)&&status==0);
        build_close(&b);
    }
    check(code_phase_park());status=255;
    check(!execute_native(saved,&status)&&status==0&&pages_used()==owned+1-phases);
    for(bank=0;bank<code_required;++bank)if(CODE_DEFERRED(bank))
        check(code_pages[bank]==255&&code_phase_pending[bank]==255);
    check(code_phase_restore());
    check(!memcmp(&s,&original,sizeof(s)));source_close(&s);check(pages_used()==owned);
    test_machine(machine_after);check(!memcmp(machine_before,machine_after,7));
}
/* The first release fails below the phase loader. Poison that freed page
 * before retry: executing its stale bytes would halt instead of passing. */
static void release_failure(void) {
    uint8_t original=code_pages[CODE_SHARED],loader=code_pages[CODE_ROUTINES],held[32],count=0,page,poisoned=0;
    uint16_t at;
    release_original=original;code_pages[CODE_SHARED]=0;code_release();
    check(code_owned==CODE_SHARED+1&&boot_error&&code_phase_fd>=0);
    code_pages[CODE_SHARED]=original;
    while(count<32&&palloc(&page)==0) {
        held[count++]=page;
        if(page==loader) {
            poisoned=1;
            for(at=0;at<PAGE_SIZE;at+=256)check(far_fill(page,at,0x76,256));
        }
    }
    check(poisoned);
    while(count)check(pfree(held[--count])==0);
    boot_error=0;test_release_retry(); /* Resident entry never returns. */
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
    fd=open("H:/RELEASE.TEST",O_RDONLY);if(fd>=0){exit_mode=2;check(!close(fd));}
    fd=open("H:/PARK.TEST",O_RDONLY);if(fd>=0){exit_mode=3;check(!close(fd));}
    fd=open("H:/PENDING.TEST",O_RDONLY);if(fd>=0){exit_mode=4;check(!close(fd));}
    owned=pages_used();test_machine(before);
    memset(&cc,0,sizeof(cc));memset(&member,0,sizeof(member));
    b.pages[2]=page_new(PAGE_IMAGE);b.map_page=page_new(PAGE_MAP);
    cc.primary_stage=255;cc.stage_page=page_new(PAGE_COMPILER);
    cc.error=&d;check(cc_tables_begin());
    check(b.pages[2]!=255&&b.map_page!=255&&cc.stage_page!=255);
    if(b.pages[2]==255||b.map_page==255||cc.stage_page==255)goto done;
    cc.image=&b;cc.emitted=0xc201;cc.entries=5;cc.relocations=7;cc.routine_count=2;
    for(i=0;i<PAGE_SIZE;++i) {
        far_set(b.pages[2],i,(uint8_t)(i*3+9));far_set(cc.stage_page,i,(uint8_t)(i*5+1));
        far_set(b.map_page,i,(uint8_t)(i*7+3));
    }
    for(i=0;i<28;++i)cc_reloc_put(i,(uint8_t)(i*11+5));
    member.origin=0xbff1;member.entry=0xc000;member.map_first=2;member.reloc_first=3;
    fd=output_open_path(path,O_RDWR|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd<0)goto done;
    check(cc_members_begin(&store,fd));test_stage=1;
    check(cc_members_flush(&store,&member,0,0));
    check(!store.failed&&store.length==592&&member.offset==0&&member.bytes==528&&member.maps==3&&member.relocs==4);
    first=member;
    check(cc_members_flush(&store,&member,0,0));
    check(!store.failed&&store.length==1184&&member.offset==592&&first.bytes==member.bytes);
    check(cc.emitted==0xc201&&cc.entries==5&&cc.relocations==7&&cc.routine_count==2);
    check(output_finish(fd,1));test_stage=2;
    /* Preserve the complete parent prefixes and global routine records. */
    for(i=0;i<PAGE_SIZE;++i) {
        if(far_get(b.pages[2],i)!=(uint8_t)(i*3+9)||far_get(cc.stage_page,i)!=(uint8_t)(i*5+1)||
           far_get(b.map_page,i)!=(uint8_t)(i*7+3))break;
    }
    check(i==PAGE_SIZE);
    for(i=0;i<28;++i)if(cc_reloc_get(i)!=(uint8_t)(i*11+5))break;
    check(i==28);
    fd=output_open_path(path,O_RDWR);check(fd>=0);
    if(fd>=0) {
        check(!cc_members_begin(&store,fd)&&store.failed&&platform_error);
        check(output_finish(fd,1));
    }
    test_stage=3;group_cases();late_cases(late_path,&d);
    root_helper_cases(helper_path,&d);
    remaining_helper_cases(owner_path,&d);
    member_address_guard(&d);
done:
    if(b.pages[2]!=255) {released=page_drop(b.pages[2]);check(released);if(released)b.pages[2]=255;}
    if(b.map_page!=255) {released=page_drop(b.map_page);check(released);if(released)b.map_page=255;}
    check(cc_tables_end());
    if(cc.stage_page!=255) {released=page_drop(cc.stage_page);check(released);if(released)cc.stage_page=255;}
    check(pages_used()==owned);test_machine(after);check(!memcmp(before,after,7));
    if(b.pages[2]==255&&b.map_page==255&&cc.stage_page==255) {
        memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=255;b.sources.page=255;
        test_stage=4;phase_cases();
    }
    test_stage=6;test_done=255;
    if(exit_mode==2)release_failure();
    if(exit_mode>=3) {
        check(code_phase_park());
        if(exit_mode==4) {
            uint8_t pending=255;check(palloc(&pending)==0);
            if(pending!=255)code_phase_pending[CODE_EXPRESSIONS]=pending;
        }
    }
    if(exit_mode)return test_failed?1:0;
    test_complete();return 0;
}
