/* Test-only SDK parent: lend an owned stdout file to the replay executable,
 * then restore/close it. No shell redirection extension is assumed. */
#include <zos_vfs.h>
#include <zos_sys.h>
void replay_ready(void) { __asm nop __endasm; }
int main(void) {
    int8_t fd=open("H:/REPLAY.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    uint8_t error,status=255,restored,closed;
    if(fd<0)return 1;
    if(swap(fd,DEV_STDOUT)) {close(fd);return 2;}
    replay_ready();
    error=exec(EXEC_PRESERVE_PROGRAM,"H:/REPLAY.COM",0,&status);
    restored=swap(fd,DEV_STDOUT);closed=close(fd);
    return error||status||restored||closed?3:0;
}
