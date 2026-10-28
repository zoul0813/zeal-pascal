#include <stdint.h>
#include <string.h>
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zos_keyboard.h>
#include <zos_video.h>
#include <zvb_hardware.h>

/* Ordinary native SDK application: no Pascal header or IDE ABI. Its native
 * allocations are reclaimed by preserve-exec, and its files close normally. */
int main(void) {
    char directory[128];uint8_t page,raw[32];int8_t fd;uint16_t size;
    if(zvb_ctrl_video_mode!=ZVB_CTRL_VID_MODE_TEXT_640)return 17;
    if(curdir(directory)||palloc(&page))return 10;
    fd=open("H:/XCHILD.TXT",O_WRONLY|O_CREAT|O_TRUNC);if(fd<0)return 11;
    size=(uint16_t)strlen(directory);
    if(write(fd,directory,&size)||close(fd))return 12;
    /* Consume releases delivered during the handoff, as an ordinary input
     * application can. The suspended IDE decoder must then reset its state. */
    do {size=sizeof(raw);if(read(DEV_STDIN,raw,&size))return 16;}while(size);
    if(chdir("H:/project"))return 13;
    if(ioctl(DEV_STDIN,KB_CMD_SET_MODE,(void*)(KB_MODE_COOKED|KB_READ_NON_BLOCK)))return 14;
    if(ioctl(DEV_STDOUT,CMD_SET_COLORS,TEXT_COLOR(TEXT_COLOR_GREEN,TEXT_COLOR_BLUE)))return 15;
    zvb_ctrl_video_mode=ZVB_CTRL_VID_MODE_GFX_640_8BIT;
    while(zvb_ctrl_status&2) {}
    while(!(zvb_ctrl_status&2)) {}
    return 37;
}
