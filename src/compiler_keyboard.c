#include "compiler.h"
#include "file_runtime.h"

/* Each activation owns a 255-character circular typeahead queue (count, head,
 * 256 byte slots). Interruption polling must continue beyond ordinary keys,
 * retaining them for later reads. At capacity leave further events with the
 * physical driver. The prefix owns raw release pairs and modifier decoding. */
void cc_keyboard_engine(void) FRONTEND {
    uint16_t skip,state,empty,i,body;
    if(cc.keyboard_entry||cc.suppress||cc.error->code) return;
    skip=cc_forward(0xc3);
    state=cc.keyboard_state=cc.emitted;
    for(i=0;i<258;++i)cc_emit(0);
    cc.keyboard_status=cc.emitted; cc_emit(0);
    cc.keyboard_entry=cc.emitted;
    cc_address(0x3a,state); cc_emit(0xb7); empty=cc_forward(0xca);
    cc_emit(0x3d);cc_address(0x32,state);
    cc_address(0x3a,state+1);cc_emit(0x6f);cc_emit(0x26);cc_emit(0);
    cc_emit(0x3c);cc_address(0x32,state+1);
    cc_address(0x11,state+2);cc_emit(0x19);cc_emit(0x7e);cc_emit(0xc9);
    cc_patch(empty); cc_op16(0xc3,cc_runtime_private(RP_INPUT_RAW_KEY));

    cc.keyboard_probe=cc.emitted;
    cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); cc_op16(0xc2,RT_IOFAIL);
    cc_address(0x3a,state);cc_emit(0xfe);cc_emit(255);empty=cc_forward(0xc2);
    cc_emit(0xaf);cc_emit(0xc9);cc_patch(empty);
    cc.keyboard_console_probe=cc.emitted;
    cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);cc_op16(0xc2,RT_IOFAIL);
    /* A nested console check borrows input without taking its caller's echo
     * handle or raw mode. Catch physical failures locally before restoring
     * those owners and propagating the primary I/O result. */
    cc_op16(0x3a,cc_runtime_private(RP_ECHO_FD));cc_emit(0xf5);
    cc_op16(0x3a,cc_runtime_private(RP_INPUT_MODE_VALUE));cc_emit(0xf5);
    cc_emit(0x3e); cc_emit(255); cc_op16(0x32,cc_runtime_private(RP_ECHO_FD));
    cc_op16(0xcd,RT_IOCALL);body=cc.emitted;cc_relocate(body);
    cc_emit(0);cc_emit(0);cc_emit(0);
    cc_emit(0xf5);cc_op16(0x21,3);cc_emit(0x39);cc_emit(0x5e);cc_emit(0x16);cc_emit(0);
    cc_op16(0xcd,cc_runtime_private(RP_INPUT_MODE));cc_emit(0x47);
    cc_emit(0xf1);cc_emit(0xd1);cc_emit(0xd1);cc_emit(0x4f);
    cc_emit(0x7a);cc_op16(0x32,cc_runtime_private(RP_ECHO_FD));
    cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);cc_op16(0xc2,RT_IOFAIL);
    cc_emit(0x78);cc_emit(0xb7);cc_op16(0xc2,cc_runtime_private(RP_IO_ERROR));
    cc_emit(0x79);cc_emit(0xc9);
    cc_word(body,cc.emitted);
    cc_op16(0x11,4); cc_op16(0xcd,cc_runtime_private(RP_INPUT_MODE));
    cc_emit(0xb7); cc_op16(0xc2,cc_runtime_private(RP_INPUT_FAIL));
    cc_op16(0xc3,cc_runtime_private(RP_INPUT_RAW_KEY));

    /* Probe guarantees space. Zero means no decoded key, never queue data. */
    cc.keyboard_enqueue=cc.emitted;
    cc_emit(0xb7);cc_emit(0xc8);cc_emit(0x47);
    cc_address(0x3a,state);cc_emit(0x3c);cc_address(0x32,state);cc_emit(0x3d);
    cc_address(0x21,state+1);cc_emit(0x86);cc_emit(0x6f);cc_emit(0x26);cc_emit(0);
    cc_address(0x11,state+2);cc_emit(0x19);cc_emit(0x70);cc_emit(0xc9);

    cc.keyboard_poll=cc.emitted;
    cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);cc_op16(0xc2,RT_IOFAIL);
    cc_address(0x3a,state);cc_emit(0xb7);empty=cc_forward(0xc2);
    cc_address(0xcd,cc.keyboard_probe);cc_address(0xcd,cc.keyboard_enqueue);
    cc_patch(empty);cc_address(0x3a,state);cc_emit(0xb7);empty=cc_forward(0xca);
    cc_emit(0x3e);cc_emit(1);cc_patch(empty);
    cc_address(0x32,cc.keyboard_status);cc_emit(0xc9);
    cc_patch(skip);
}
void cc_keypressed(void) FRONTEND {
    uint16_t done;
    cc_keyboard_engine();
    cc_reserve(512); cc_op16(0xcd,RT_IOCALL);
    cc_relocate(cc.emitted);
    cc_emit((uint8_t)cc.keyboard_poll); cc_emit((uint8_t)(cc.keyboard_poll>>8)); cc_emit(cc.io_check);
    /* IOCALL owns HL while restoring its landing. Read the owned result only
     * after that restoration; unchecked/pending failures return False. */
    cc_op16(0x21,0); cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); done=cc_forward(0xc2);
    cc_address(0x3a,cc.keyboard_status); cc_emit(0x6f); cc_patch(done);
}

static void key_index(uint8_t opcode,uint8_t offset) { cc_emit(0xfd); cc_emit(opcode); cc_emit(offset); }
/* IY is an authenticated FileJob. The file owner lends its current byte for
 * this one synchronous operation; the caller returns it before propagating
 * errors. Output and echo may invoke Pascal callbacks after prefix restoration.
 * The existing prefix owns all physical
 * input, modifier/release decoding, typeahead, mode restoration and failures. */
void cc_logical_stream(void) FRONTEND {
    uint16_t body,peek,take,space,ready,read,line,predicate,skip,loop,done;
    uint16_t character,string,token,empty,eof,yes,no,answer,lines,cached,raw,quiet,cr,output,newline,nonempty;
    body=cc_forward(0xc3);
    peek=cc.emitted;
    cc_emit(0xc5); cc_emit(0xd5); cc_emit(0xe5);
    key_index(0x7e,37); cc_emit(0xb7); ready=cc_forward(0xc2);
    key_index(0x7e,34); cc_emit(0xfe); cc_emit(ZF_STANDARD_CON); raw=cc_forward(0xc2);
    cc_op16(0xcd,cc_runtime_private(RP_INPUT_SOURCE)); cached=cc_forward(0xc3);
    cc_patch(raw);
    cc_emit(0x3e); cc_emit(255); cc_op16(0x32,cc_runtime_private(RP_ECHO_FD));
    cc_op16(0x11,0); cc_op16(0xcd,cc_runtime_private(RP_INPUT_MODE));
    cc_emit(0xb7); cc_op16(0xc2,cc_runtime_private(RP_INPUT_FAIL));
    key_index(0x7e,34); cc_emit(0xfe); cc_emit(ZF_STANDARD_TRM); quiet=cc_forward(0xc2);
    cc_op16(0xcd,cc_runtime_private(RP_INPUT_OPEN));
    cc_patch(quiet);raw=cc.emitted;cc_op16(0xcd,cc_runtime_private(RP_INPUT_KEY));
    cc_emit(0x47);key_index(0x7e,34);cc_emit(0xfe);cc_emit(ZF_STANDARD_TRM);cc_emit(0x78);
    quiet=cc_forward(0xc2);cc_emit(0xfe);cc_emit(3);empty=cc_forward(0xc2);
    cc_op16(0x3a,RT_CBREAK);cc_emit(0xb7);cc_address(0xca,raw);
    cc_op16(0xc3,cc_runtime_private(RP_INPUT_BREAK));cc_patch(quiet);cc_patch(empty);
    key_index(0x77,36); key_index(0x36,37); cc_emit(1);
    key_index(0x7e,34); cc_emit(0xfe); cc_emit(ZF_STANDARD_TRM); quiet=cc_forward(0xc2);
    key_index(0x7e,36); cc_emit(0xfe); cc_emit(13); cr=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(32); empty=cc_forward(0xda);
    cc_emit(0xfe); cc_emit(127); done=cc_forward(0xd2);
    cc_op16(0xcd,cc_runtime_private(RP_INPUT_ECHO)); output=cc_forward(0xc3);
    cc_patch(cr); cc_op16(0xcd,cc_runtime_private(RP_INPUT_ECHO));
    cc_emit(0x3e); cc_emit(10); cc_op16(0xcd,cc_runtime_private(RP_INPUT_ECHO));
    cc_patch(quiet); cc_patch(empty); cc_patch(done); cc_patch(output);
    cc_op16(0xcd,cc_runtime_private(RP_INPUT_CLEANUP));
    cc_emit(0xb7); cc_op16(0xc2,cc_runtime_private(RP_IO_ERROR));
    skip=cc_forward(0xc3);
    cc_patch(cached); key_index(0x77,36); key_index(0x36,37); cc_emit(1);
    cc_patch(skip);
    cc_patch(ready); cc_emit(0xe1); cc_emit(0xd1); cc_emit(0xc1); key_index(0x7e,36); cc_emit(0xc9);
    take=cc.emitted; cc_address(0xcd,peek); key_index(0x36,37); cc_emit(0); cc_emit(0xc9);
    space=cc.emitted;
    cc_emit(0xfe); cc_emit(32); cc_emit(0xc8); cc_emit(0xfe); cc_emit(9); cc_emit(0xc8);
    cc_emit(0xfe); cc_emit(13); cc_emit(0xc8); cc_emit(0xfe); cc_emit(10); cc_emit(0xc9);

    cc_patch(body); key_index(0x7e,0);
    cc_emit(0xfe); cc_emit(ZF_TEXT_WRITE); output=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(ZF_TEXT_WRITELN); read=cc_forward(0xc2);
    cc_patch(output);
    cc_emit(0xfe); cc_emit(ZF_TEXT_WRITELN); nonempty=cc_forward(0xca);
    key_index(0x6e,6); key_index(0x66,7); cc_emit(0x7e); cc_emit(0xb7); cc_emit(0xc8);
    cc_patch(nonempty);
    key_index(0x7e,0); cc_emit(0xfe); cc_emit(ZF_TEXT_WRITELN); newline=cc_forward(0xca);
    key_index(0x6e,6); key_index(0x66,7); cc_emit(0x46); cc_emit(0x23);
    loop=cc.emitted; cc_emit(0x78); cc_emit(0xb7); done=cc_forward(0xca);
    cc_emit(0xc5); cc_emit(0xe5); cc_emit(0x7e); cc_hook_output();
    cc_emit(0xe1); cc_emit(0xc1); cc_emit(0x23); cc_emit(0x05); cc_address(0xc3,loop);
    cc_patch(newline); cc_emit(0x3e); cc_emit(13); cc_hook_output();
    cc_emit(0x3e); cc_emit(10); cc_hook_output();
    cc_patch(done); cc_emit(0xc9);
    cc_patch(read); key_index(0x7e,0);
    cc_emit(0xfe); cc_emit(ZF_TEXT_BEGIN); skip=cc_forward(0xc2);
    key_index(0x7e,8); cc_emit(0xe6); cc_emit(2); cc_emit(0xc8); cc_address(0xc3,peek);
    cc_patch(skip); cc_emit(0xfe); cc_emit(ZF_TEXT_READ); read=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(ZF_TEXT_READLN); predicate=cc_forward(0xc2);
    line=cc.emitted; cc_address(0xcd,peek); cc_emit(0xfe); cc_emit(26); cc_emit(0xc8);
    cc_address(0xcd,take); cc_emit(0xfe); cc_emit(13); cr=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(10); skip=cc_forward(0xc2);
    key_index(0x7e,34); cc_emit(0xfe); cc_emit(ZF_STANDARD_CON); cc_emit(0xc8);
    cc_patch(skip); cc_address(0xc3,line);
    cc_patch(cr); key_index(0x7e,34); cc_emit(0xfe); cc_emit(ZF_STANDARD_CON); cc_emit(0xc0);
    /* Discard only an LF already in the shared physical buffer. A different
     * binding may own it, or may have refilled the source; never fetch a key. */
    cc_op16(0x3a,cc_runtime_private(RP_LINE_REMAINING)); cc_emit(0xb7); cc_emit(0xc8);
    cc_op16(0x2a,cc_runtime_private(RP_LINE_NEXT)); cc_emit(0x7e); cc_emit(0xfe); cc_emit(10); cc_emit(0xc0);
    cc_op16(0xc3,cc_runtime_private(RP_INPUT_SOURCE));

    cc_patch(read); key_index(0x6e,6); key_index(0x66,7); key_index(0x46,8); key_index(0x4e,9);
    cc_emit(0x78); cc_emit(0xfe); cc_emit(ZF_TEXT_CHAR); character=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(ZF_TEXT_STRING); string=cc_forward(0xca);
    cc_op16(0x11,ZF_NUMBER_TOKEN); cc_emit(0x19); cc_emit(0x0e); cc_emit(30);
    loop=cc.emitted; cc_address(0xcd,peek); cc_address(0xcd,space); token=cc_forward(0xc2);
    cc_address(0xcd,take); cc_address(0xc3,loop);
    cc_patch(string); cc_patch(token); cc_emit(0xe5); cc_emit(0x23); cc_emit(0x1e); cc_emit(0);
    loop=cc.emitted; cc_emit(0x79); cc_emit(0xb7); done=cc_forward(0xca);
    cc_address(0xcd,peek); cc_emit(0xfe); cc_emit(26); eof=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(13); empty=cc_forward(0xca);
    cc_emit(0x57); cc_emit(0x78); cc_emit(0xfe); cc_emit(ZF_TEXT_STRING); string=cc_forward(0xca);
    cc_emit(0x7a); cc_address(0xcd,space); token=cc_forward(0xca);
    cc_patch(string); cc_address(0xcd,take); cc_emit(0x77); cc_emit(0x23); cc_emit(0x1c); cc_emit(0x0d);
    cc_address(0xc3,loop);
    cc_patch(done); cc_patch(eof); cc_patch(empty); cc_patch(token);
    cc_emit(0xe1); cc_emit(0x73); /* short token/String length */
    cc_emit(0x78); cc_emit(0xfe); cc_emit(ZF_TEXT_STRING); done=cc_forward(0xca);
    cc_emit(0x7b); cc_emit(0xb7); cc_emit(0xc8);
    cc_patch(done); key_index(0x36,10); cc_emit(1); cc_emit(0xc9);
    cc_patch(character); cc_address(0xcd,take); cc_emit(0x77);
    key_index(0x36,10); cc_emit(1); cc_emit(0xc9);

    cc_patch(predicate); loop=cc.emitted; cc_address(0xcd,peek); cc_emit(0x47);
    key_index(0x7e,0); cc_emit(0xfe); cc_emit(ZF_SEEKEOF); lines=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(ZF_SEEKEOLN); answer=cc_forward(0xc2);
    cc_emit(0x78); cc_emit(0xfe); cc_emit(32); skip=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(9); no=cc_forward(0xc2);
    cc_patch(skip); cc_address(0xcd,take); cc_address(0xc3,loop);
    cc_patch(lines); cc_emit(0x78); cc_address(0xcd,space); skip=cc_forward(0xc2);
    cc_address(0xcd,take); cc_address(0xc3,loop);
    cc_patch(answer); cc_patch(skip); cc_patch(no); cc_emit(0x78); cc_emit(0xfe); cc_emit(26); yes=cc_forward(0xca);
    key_index(0x7e,0); cc_emit(0xfe); cc_emit(ZF_EOF); empty=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(ZF_SEEKEOF); eof=cc_forward(0xca);
    cc_emit(0x78); cc_emit(0xfe); cc_emit(13); token=cc_forward(0xc2);
    cc_patch(yes); cc_op16(0x21,1); done=cc_forward(0xc3);
    cc_patch(empty); cc_patch(eof); cc_patch(token); cc_op16(0x21,0);
    cc_patch(done); key_index(0x75,20); key_index(0x74,21); cc_emit(0xc9);
}
