/* Shared host/native proof using the actual emitter and far group owner.
 * Caller supplies fresh staging and compiler tables and a resident pathname. */
static void late_cases(const char *path,Diagnostic *diagnostic) {
    CompilerGroups groups;
    CompilerMember member;
    CompilerSources context,*old_sources=cc.sources;
    Diagnostic *old_error=cc.error;
    uint16_t parameters,state,bridge,unpack_helper,routine,output,value=0x1234;
    uint16_t parent,head,tail,first,i,at;
    uint8_t id,ordinal;
    int8_t fd;
    memset(diagnostic,0,sizeof(*diagnostic));memset(&context,0,sizeof(context));
    cc.error=diagnostic;cc.sources=&context;cc.routine_count=255;cc.type_count=TYPE_LIMIT;
    cc.emitted=0xbff0;cc.entries=cc.relocations=0;
    cc_groups_init(&groups);
    fd=output_open_path(path,O_RDWR|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd<0)goto done;
    check(cc_groups_begin(&groups,fd));if(groups.store.failed)goto cleanup;
    check(groups.late.page==255&&!groups.late.symbols&&!groups.late.references);
    check(cc_groups_scope(&groups,1));check(cc_groups_domain(&groups,1,&id)&&id==0);
    parameters=cc_late_symbol(&groups.late,LATE_ROOT,LATE_PARAMETERS,0);
    state=cc_late_symbol(&groups.late,LATE_ROOT,LATE_RANDOM_STATE,0);
    bridge=cc_late_symbol(&groups.late,LATE_BRIDGE,255,0);
    unpack_helper=cc_late_symbol(&groups.late,LATE_ROOT,LATE_UNPACK,0);
    routine=cc_late_symbol(&groups.late,LATE_ROUTINE,255,255);
    output=cc_late_symbol(&groups.late,LATE_ROOT,LATE_OUTPUT,0);
    check(parameters==0&&state==1&&bridge==2&&unpack_helper==3&&routine==4&&output==5);
    if(groups.late.failed)goto cleanup;
    check(cc_late_symbol(&groups.late,LATE_ROOT,LATE_PARAMETERS,0)==parameters&&groups.late.symbols==6);
    check(!cc_late_value(&groups.late,parameters,&value)&&value==0x1234&&!groups.late.failed);
    cc_address(0x21,7);
    check(cc_late_reference(&groups.late,0xbff1,parameters,1,0));
    cc_emit(5);cc_relocate_value(0xbff3,65535u,1);
    check(cc_late_reference(&groups.late,0xbff3,state,65535u,1));
    cc_op16(0x11,2);cc_relocate_value(0xbff5,3,0);
    check(cc_late_reference(&groups.late,0xbff5,parameters,1,0));
    check(cc_late_reference(&groups.late,0xbff5,state,2,0));
    parent=cc_link(0xcd,0);check(parent==0xbff8&&groups.late.references==4);
    check(cc_group_open(&groups,0)==0&&cc_group_member_begin(&groups,0,1));
    check(cc_groups_scope(&groups,2));check(cc_groups_domain(&groups,2,&id)&&id==1);
    first=cc.emitted;head=cc_link(0xcd,parent);
    while(cc.emitted<0xc001)cc_emit(0);
    head=cc_link(0xcd,head);check(head==0xc002);
    check(cc_late_chain(&groups.late,first,head,output,&tail)&&tail==parent&&groups.late.references==6);
    check(cc_code_get(0xbff8)==0&&cc_code_get(0xbff9)==0&&
          cc_code_get(0xbffb)==0&&cc_code_get(0xbffc)==0&&cc_code_get(0xc002)==0&&cc_code_get(0xc003)==0);
    check(cc_group_member_end(&groups,0,1,first)&&groups.late.references==4&&cc.emitted==first);
    check(cc_group_member_get(&groups,1,&id,&ordinal,&member)&&id==0&&ordinal==0&&member.lates==2&&member.bytes==10);
    check(cc_group_member_begin(&groups,0,255));
    check(cc_groups_scope(&groups,254));check(cc_groups_domain(&groups,254,&id)&&id==255);
    cc_address(0x21,3);check(cc_late_reference(&groups.late,first+1,bridge,1,0));
    check(cc_group_member_end(&groups,0,255,first)&&groups.late.references==4);
    check(cc_group_member_get(&groups,255,&id,&ordinal,&member)&&ordinal==1&&member.lates==1&&member.bytes==3);
    check(cc_group_close(&groups,0)&&cc.emitted==0xc004&&groups.store.length==97);
    check(cc_groups_domain(&groups,2,&id)&&id==1&&cc_groups_domain(&groups,254,&ordinal)&&ordinal==255);
    for(i=first;i<cc.emitted;++i)if(cc_code_get(i))break;
    check(i==cc.emitted);
    check(cc_late_chain(&groups.late,CC_CODE_BASE,tail,output,&head)&&!head&&groups.late.references==5);
    at=cc.emitted;
    for(i=0;i<6;++i)cc_emit(0xc9);
    check(cc_late_bind(&groups.late,parameters,0,at));
    check(cc_late_bind(&groups.late,state,0,at+1));
    check(cc_late_bind(&groups.late,bridge,0,at+2));
    check(cc_late_bind(&groups.late,unpack_helper,0,at+3));
    check(cc_late_bind(&groups.late,routine,255,at+4));
    check(cc_late_bind(&groups.late,output,0,at+5));
    check(cc_late_bind(&groups.late,parameters,0,at)&&cc_late_value(&groups.late,bridge,&value)&&value==at+2);
    check(cc_late_finish(&groups.late)&&groups.late.sealed);
    check((cc_code_get(0xbff1)|((uint16_t)cc_code_get(0xbff2)<<8))==0xc00b);
    check(cc_code_get(0xbff3)==0);
    check((cc_code_get(0xbff5)|((uint16_t)cc_code_get(0xbff6)<<8))==0x4010);
    check((cc_code_get(parent)|((uint16_t)cc_code_get(parent+1)<<8))==0xc009);
    for(i=first;i<0xc004;++i)if(cc_code_get(i))break;
    check(i==0xc004);
    check(!cc_late_finish(&groups.late)&&groups.late.failed);
    check((cc_code_get(0xbff1)|((uint16_t)cc_code_get(0xbff2)<<8))==0xc00b&&!diagnostic->code);
cleanup:
    check(cc_groups_discard(&groups)&&groups.page==255&&groups.late.page==255&&groups.store.fd==-1);
done:
    cc.error=old_error;cc.sources=old_sources;
}

/* Exercise the production factories with an actual active member. Definitions
 * must appear only after the owning resident domain resumes. A later member
 * reuses those definitions without creating another journal or state copy. */
static void root_helper_cases(const char *path,Diagnostic *diagnostic) {
    CompilerGroups groups;CompilerMember member;
    CompilerSources context,*old_sources=cc.sources;Diagnostic *old_error=cc.error;
    uint16_t origin,parameters,random,state,symbol,value;
    uint8_t id,ordinal;int8_t fd;
    memset(diagnostic,0,sizeof(*diagnostic));memset(&context,0,sizeof(context));
    cc.error=diagnostic;cc.sources=&context;context.groups=&groups;
    cc.emitted=0xb000;cc.entries=cc.relocations=0;
    cc.parameter_entry=cc.random_entry=cc.random_state=0;
    cc_groups_init(&groups);fd=output_open_path(path,O_RDWR|O_CREAT|O_TRUNC);
    check(fd>=0);if(fd<0)goto done;
    check(cc_groups_begin(&groups,fd));if(groups.store.failed)goto cleanup;
    check(cc_group_open(&groups,0)==0&&cc_group_member_begin(&groups,0,1));
    origin=cc.emitted;cc_params(0);cc_randomize();
    cc_random(0);cc_release(0);cc_op16(0x21,10);cc_random(1);
    check(!diagnostic->code&&!cc.parameter_entry&&!cc.random_entry&&!cc.random_state&&groups.late.references==4);
    check(cc_root_pending(LATE_ROOT,LATE_PARAMETERS)&&cc_root_pending(LATE_ROOT,LATE_RANDOM)&&cc_root_pending(LATE_ROOT,LATE_RANDOM_STATE));
    check(cc_group_member_end(&groups,0,1,origin)&&!groups.late.references);
    check(cc_group_member_get(&groups,1,&id,&ordinal,&member)&&member.lates==4);
    check(cc_group_close(&groups,0));origin=cc.emitted;
    cc_params_finish();cc_random_finish();
    parameters=cc.parameter_entry;random=cc.random_entry;state=cc.random_state;
    check(!diagnostic->code&&parameters>=origin&&random>=origin&&state>=origin);
    check(!cc_root_pending(LATE_ROOT,LATE_PARAMETERS)&&!cc_root_pending(LATE_ROOT,LATE_RANDOM)&&!cc_root_pending(LATE_ROOT,LATE_RANDOM_STATE));
    symbol=cc_late_symbol(&groups.late,LATE_ROOT,LATE_PARAMETERS,0);
    check(cc_late_value(&groups.late,symbol,&value)&&value==parameters);
    symbol=cc_late_symbol(&groups.late,LATE_ROOT,LATE_RANDOM,0);
    check(cc_late_value(&groups.late,symbol,&value)&&value==random);
    symbol=cc_late_symbol(&groups.late,LATE_ROOT,LATE_RANDOM_STATE,0);
    check(cc_late_value(&groups.late,symbol,&value)&&value==state&&groups.late.symbols==3);
    check(cc_group_open(&groups,0)==1&&cc_group_member_begin(&groups,1,2));
    origin=cc.emitted;cc_params(0);cc_randomize();
    cc_random(0);cc_release(0);cc_op16(0x21,10);cc_random(1);
    check(cc.parameter_entry==parameters&&cc.random_entry==random&&cc.random_state==state&&!groups.late.references);
    check(cc_group_member_end(&groups,1,2,origin)&&cc_group_close(&groups,1));
    check(cc_group_member_get(&groups,2,&id,&ordinal,&member)&&id==1&&!member.lates);
    check(cc_late_finish(&groups.late)&&!diagnostic->code);
cleanup:
    check(cc_groups_discard(&groups)&&groups.late.page==255&&groups.page==255);
done:
    cc.error=old_error;cc.sources=old_sources;
}

static void member_address_guard(Diagnostic *diagnostic) {
    CompilerSources context,*old_sources=cc.sources;
    CompilerGroups groups;
    Diagnostic *old_error=cc.error;
    uint16_t origin=cc.emitted;
    memset(diagnostic,0,sizeof(*diagnostic));memset(&context,0,sizeof(context));
    memset(&groups,0,sizeof(groups));context.groups=&groups;groups.active=1;
    cc.sources=&context;cc.error=diagnostic;cc.routine_count=2;
    CCS_SET(0,role,S_PROCEDURE);CCS_SET(0,initial_type,2);
    cc_rput(2,R_PARENT,1);cc_rput(2,R_DESCRIPTOR,0);cc_rput(2,R_THUNK,0);cc_rput(1,R_LOCALS,0);
    cc.suppress=1;cc_routine_address(0);
    check(!diagnostic->code&&cc.emitted==origin&&!cc_rget(2,R_DESCRIPTOR));
    cc.suppress=0;cc_routine_address(0);
    check(diagnostic->code==99&&cc.emitted==origin&&!cc_rget(2,R_DESCRIPTOR)&&
          !cc_rget(2,R_THUNK)&&!cc_rget(1,R_LOCALS));
    cc.sources=old_sources;cc.error=old_error;
}

static void remaining_helper_cases(const char *path,Diagnostic *diagnostic) {
    CompilerGroups groups;CompilerMember member;
    CompilerSources context,*old_sources=cc.sources;Diagnostic *old_error=cc.error;
    uint16_t origin,parent_output,parent_unpack,keyboard,interrupt,file,unpack_helper,value,symbol;
    uint8_t id,ordinal;int8_t fd;
    memset(diagnostic,0,sizeof(*diagnostic));memset(&context,0,sizeof(context));
    cc.error=diagnostic;cc.sources=&context;context.groups=&groups;
    cc.emitted=CC_CODE_BASE;cc.entries=cc.relocations=0;cc.scope=cc.with_depth=0;
    cc.user_interrupt=cc.io_check=1;cc.temporaries=ZF_JOB_BYTES;cc.text_type=TYPE_NONE;
    cc.keyboard_entry=cc.keyboard_poll=cc.keyboard_status=cc.keyboard_state=0;
    cc.keyboard_probe=cc.keyboard_enqueue=cc.keyboard_console_probe=cc.interrupt_entry=0;
    cc.file_entry=cc.standard_table_operand=cc.file_stream_operand=cc.hook_mask_call=0;
    cc.hook_output_calls=cc.transfer_entry=cc.unpack_entry=0;
    cc.hook_character=cc.hook_echo=cc.hook_open=cc.error_entry=0;cc.console_used=0;
    memset(cc.hook_variables,0,sizeof(cc.hook_variables));memset(cc.hook_defaults,0,sizeof(cc.hook_defaults));
    cc.type_count=0;cc.symbol_count=1;
    for(id=0;id<9;++id)cc_new_type(0,0,0,0,0);
    cc_symbol_clear(0);
    CCT_SET(0,width,2);CCT_SET(3,width,2);CCT_SET(3,low,1);CCT_SET(3,high,2);
    CCT_SET(6,family,K_FILE);CCT_SET(6,width,176);CCT_SET(6,capacity,FILE_TYPED);
    CCT_SET(7,family,K_ARRAY);CCT_SET(7,width,352);CCT_SET(7,capacity,3);CCT_SET(7,element,6);
    CCT_SET(8,family,K_RECORD);CCT_SET(8,width,356);
    CCS_SET(0,role,S_FIELD);CCS_SET(0,owner,8);CCS_SET(0,type,7);CCS_SET(0,address,4);
    cc_groups_init(&groups);fd=output_open_path(path,O_RDWR|O_CREAT|O_TRUNC);
    check(fd>=0);if(fd<0)goto done;
    check(cc_groups_begin(&groups,fd));if(groups.store.failed)goto cleanup;
    cc_hook_output();parent_output=cc.hook_output_calls;
    cc_op16(0xcd,0);cc_unpack_reference(cc.emitted-2);parent_unpack=cc.unpack_entry;
    check(cc_group_open(&groups,0)==0&&cc_group_member_begin(&groups,0,1));
    origin=cc.emitted;cc_keypressed();cc_user_interrupt();
    /* Transfer's parser requests the file engine before emitting its call.
     * The parser itself is not enabled by this component fixture. */
    cc_root_request(LATE_ROOT,LATE_FILE);cc_transfer_statement(0);
    cc_hook_output();cc_op16(0xcd,0);cc_unpack_reference(cc.emitted-2);
    check(!diagnostic->code&&!cc.keyboard_entry&&!cc.interrupt_entry&&!cc.file_entry&&!CCT(8,high));
    check(groups.late.references==3);
    check(cc_group_member_end(&groups,0,1,origin)&&!groups.late.references);
    check(cc.hook_output_calls==parent_output&&!cc.transfer_entry&&cc.unpack_entry==parent_unpack);
    check(cc_group_member_get(&groups,1,&id,&ordinal,&member)&&member.lates==6);
    check(cc_group_close(&groups,0));origin=cc.emitted;
    cc_file_engine_finish();cc_keyboard_finish();cc_interrupt_finish();
    keyboard=cc.keyboard_entry;interrupt=cc.interrupt_entry;file=cc.file_entry;unpack_helper=(uint16_t)CCT(8,high);
    check(!diagnostic->code&&keyboard>=origin&&interrupt>=origin&&file>=origin&&unpack_helper==0);
    check(!CCT(6,high)&&!CCT(7,high)&&CCT(3,high)==2);
    check(!cc_root_pending(LATE_ROOT,LATE_KEYBOARD)&&!cc_root_pending(LATE_ROOT,LATE_INTERRUPT)&&
          !cc_root_pending(LATE_ROOT,LATE_FILE));
    check(cc_root_pending(LATE_ROOT,LATE_OUTPUT)&&cc_root_pending(LATE_ROOT,LATE_TRANSFER)&&cc_root_pending(LATE_ROOT,LATE_UNPACK));
    check(cc_group_open(&groups,0)==1&&cc_group_member_begin(&groups,1,2));
    origin=cc.emitted;cc_keypressed();cc_user_interrupt();
    check(cc.keyboard_entry==keyboard&&cc.interrupt_entry==interrupt&&cc.file_entry==file&&
          (uint16_t)CCT(8,high)==unpack_helper&&!groups.late.references);
    check(cc_group_member_end(&groups,1,2,origin)&&cc_group_close(&groups,1));
    check(cc_group_member_get(&groups,2,&id,&ordinal,&member)&&!member.lates);
    cc_transfer_finish();cc_file_stream_finish();cc_hooks_finish();cc_file_finish();cc_unpack_finish();
    check(!diagnostic->code&&!cc_root_pending(LATE_ROOT,LATE_OUTPUT)&&
          !cc_root_pending(LATE_ROOT,LATE_TRANSFER)&&!cc_root_pending(LATE_ROOT,LATE_UNPACK));
    symbol=cc_late_symbol(&groups.late,LATE_ROOT,LATE_OUTPUT,0);
    check(cc_late_value(&groups.late,symbol,&value)&&
          (cc_code_get(parent_output)|((uint16_t)cc_code_get(parent_output+1)<<8))==value);
    check((cc_code_get(parent_unpack)|((uint16_t)cc_code_get(parent_unpack+1)<<8))==cc.unpack_entry);
    check(cc_late_finish(&groups.late)&&!diagnostic->code);
cleanup:
    check(cc_groups_discard(&groups)&&groups.late.page==255&&groups.page==255);
done:
    cc.temporaries=0;cc.error=old_error;cc.sources=old_sources;
}
