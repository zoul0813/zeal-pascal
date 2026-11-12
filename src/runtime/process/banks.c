#include "runtime/process/banks.h"
#include "runtime/process/chain.h"
#include <string.h>
#include <zos_vfs.h>
#include <zos_sys.h>
#include <zos_errors.h>

#define BANK_BYTES 16384u
static uint16_t word(const uint8_t *p) {return p[0]|((uint16_t)p[1]<<8);}
static uint32_t dword(const uint8_t *p) {return word(p)|((uint32_t)word(p+2)<<16);}
static uint8_t fail(OverlayBanks *b,uint8_t error) {if(!b->error)b->error=error;return 0;}
static const uint8_t *record(const OverlayBanks *b,uint16_t member) {return b->directory+16u+16u*member;}
static uint8_t exact(OverlayBanks *b,int8_t fd,uint16_t size) {
    uint16_t done=0,n;uint8_t error;
    while(done<size) {
        n=size-done;error=read(fd,b->buffer+done,&n);
        if(error)return fail(b,error);
        if(!n||n>size-done)return fail(b,ERR_ENTRY_CORRUPTED);
        done+=n;
    }
    return 1;
}
static uint8_t position(OverlayBanks *b,int8_t fd,uint32_t offset) {
    int32_t at=(int32_t)offset;uint8_t error=seek(fd,&at,SEEK_SET);
    if(error)return fail(b,error);
    return (uint32_t)at==offset?1:fail(b,ERR_ENTRY_CORRUPTED);
}
void overlay_banks_init(OverlayBanks *b) {
    memset(b,0,sizeof(*b));memset(b->files,255,sizeof(b->files));
    memset(b->owned,255,sizeof(b->owned));memset(b->execution,255,sizeof(b->execution));
    memset(b->selected,255,sizeof(b->selected));memset(b->parent,255,sizeof(b->parent));
    memset(b->cached_member,255,sizeof(b->cached_member));memset(b->cached_page,255,sizeof(b->cached_page));
}
uint8_t overlay_banks_bind(OverlayBanks *b,const char *name,const uint8_t *directory,uint16_t bytes) {
    uint16_t i,count,parent,size;uint8_t g,n=0,component=0,seen[OVERLAY_GROUPS];const uint8_t *r;
    if(b->ready||b->directory||b->owned_count||b->error||!directory||bytes<16||!name)
        return fail(b,ERR_INVALID_PARAMETER);
    while(name[n]) {
        if(n==11||(uint8_t)name[n]<32||(uint8_t)name[n]>=127||name[n]==':'||name[n]=='\\')
            return fail(b,ERR_INVALID_PATH);
        if(name[n]=='/') {
            if(!component||(component==1&&name[n-1]=='.')||
               (component==2&&name[n-1]=='.'&&name[n-2]=='.'))return fail(b,ERR_INVALID_PATH);
            component=0;
        } else ++component;
        ++n;
    }
    if(!component||(component==1&&name[n-1]=='.')||
       (component==2&&name[n-1]=='.'&&name[n-2]=='.'))return fail(b,ERR_INVALID_PATH);
    count=word(directory+6);g=(uint8_t)word(directory+8);
    if(memcmp(directory,"ZBD1",4)||word(directory+4)!=1||!count||!g||word(directory+8)>OVERLAY_GROUPS||
       g>count||word(directory+10)!=16||dword(directory+12)||16ul+16ul*count!=bytes)
        return fail(b,ERR_ENTRY_CORRUPTED);
    b->directory=directory;b->directory_bytes=bytes;b->count=count;b->groups=g;
    memcpy(b->basename,name,n+1);memset(seen,0,sizeof(seen));
    for(g=0;g<b->groups;++g)b->extent[g]=bytes;
    for(i=0;i<count;++i) {
        r=record(b,i);parent=word(r+2);size=word(r+4);
        if(word(r)>=b->groups||!size||size>BANK_BYTES||word(r+6)>=size||word(r+14)||
           (parent!=OVERLAY_NO_MEMBER&&parent>=i))return fail(b,ERR_ENTRY_CORRUPTED);
        g=(uint8_t)word(r);
        if((seen[g]&&b->parent[g]!=parent)||(parent!=OVERLAY_NO_MEMBER&&word(record(b,parent))==g)||
           dword(r+8)!=b->extent[g])return fail(b,ERR_ENTRY_CORRUPTED);
        seen[g]=1;b->parent[g]=parent;b->extent[g]+=size;
    }
    for(g=0;g<b->groups;++g)if(!seen[g])return fail(b,ERR_ENTRY_CORRUPTED);
    b->ready=1;return 1;
}
static uint8_t directory_path(OverlayBanks *b,uint8_t drive) {
    uint16_t n;uint8_t error=curdir(b->path);
    if(error)return fail(b,error);
    for(n=0;n<sizeof(b->path)&&b->path[n];++n);
    if(n<3||n==sizeof(b->path)||b->path[1]!=':'||b->path[2]!='/')return fail(b,ERR_INVALID_PATH);
    if(b->path[0]>='a'&&b->path[0]<='z')b->path[0]-=32;
    if(b->path[0]<'A'||b->path[0]>'Z')return fail(b,ERR_INVALID_PATH);
    if(drive)b->path[0]='A'+drive-1;
    return 1;
}
uint8_t overlay_banks_set_drive(OverlayBanks *b,uint16_t drive) {
    char root[4];int8_t fd;uint8_t error;
    if(!b->ready||b->error||drive>26)return fail(b,ERR_INVALID_PARAMETER);
    if(!directory_path(b,(uint8_t)drive))return 0;
    memcpy(root,b->path,3);root[3]=0;fd=opendir(root);
    if(fd<0)return fail(b,(uint8_t)-fd);
    error=close(fd);if(error)return fail(b,error);
    b->drive=(uint8_t)drive;return 1;
}
static uint8_t group_open(OverlayBanks *b,uint8_t group) {
    uint16_t n,at,part;int8_t fd;uint8_t error;zos_stat_t info;
    if(b->files[group]>=0)return 1;
    if(!directory_path(b,b->drive))return 0;
    n=(uint16_t)strlen(b->path);if(b->path[n-1]!='/') {
        if(n==127)return fail(b,ERR_INVALID_PATH);b->path[n++]='/';b->path[n]=0;
    }
    if(n+strlen(b->basename)+4u>127u)return fail(b,ERR_INVALID_PATH);
    strcpy(b->path+n,b->basename);n=(uint16_t)strlen(b->path);
    b->path[n++]='.';b->path[n++]='0';b->path[n++]='0'+group/10;b->path[n++]='0'+group%10;b->path[n]=0;
    fd=open(b->path,O_RDONLY);if(fd<0)return fail(b,(uint8_t)-fd);
    b->files[group]=fd;b->drives[group]=(uint8_t)b->path[0];
    error=dstat(fd,&info);if(error)return fail(b,error);
    if(!D_ISFILE(info.s_flags)||info.s_size!=b->extent[group])return fail(b,ERR_ENTRY_CORRUPTED);
    if(!exact(b,fd,16))return 0;
    if(memcmp(b->buffer,"ZBG1",4)||memcmp(b->buffer+4,b->directory+4,8)||
       word(b->buffer+12)!=group||word(b->buffer+14))return fail(b,ERR_ENTRY_CORRUPTED);
    for(at=16;at<b->directory_bytes;at+=part) {
        part=b->directory_bytes-at;if(part>sizeof(b->buffer))part=sizeof(b->buffer);
        if(!exact(b,fd,part))return 0;
        if(memcmp(b->buffer,b->directory+at,part))return fail(b,ERR_ENTRY_CORRUPTED);
    }
    /* A seek+read verifies the final extent without reading unused payloads. */
    if(!position(b,fd,b->extent[group]))return 0;
    n=1;error=read(fd,b->buffer,&n);if(error)return fail(b,error);
    return !n?1:fail(b,ERR_ENTRY_CORRUPTED);
}
static uint8_t page(OverlayBanks *b,uint8_t *out) {
    uint8_t p=255,error,i,j;uint16_t at;
    error=b->owned_count==OVERLAY_RAM_PAGES?ERR_NO_MORE_MEMORY:palloc(&p);
    if(!error) {
        if(p<32||p>=64)return fail(b,ERR_FAILURE);
        for(i=0;i<b->owned_count;++i)if(b->owned[i]==p)return fail(b,ERR_FAILURE);
        b->owned[b->owned_count++]=p;
    } else {
        if(error!=ERR_NO_MORE_MEMORY)return fail(b,error);
        p=255; /* Failed allocation leaves its output byte unspecified. */
        /* Only immutable templates are evictable. Mutable group state and
         * every live execution mapping remain owned and byte-for-byte intact. */
        for(i=0;i<OVERLAY_RAM_PAGES;++i) {
            j=(b->victim+i)%OVERLAY_RAM_PAGES;
            if(b->cached_page[j]!=255) {
                p=b->cached_page[j];b->cached_page[j]=255;b->cached_member[j]=OVERLAY_NO_MEMBER;
                b->victim=(j+1u)%OVERLAY_RAM_PAGES;break;
            }
        }
        if(p==255)return fail(b,ERR_NO_MORE_MEMORY);
    }
    *out=p;memset(chain_buffer,0,sizeof(chain_buffer));chain_store=1;
    for(at=0;at<BANK_BYTES;at+=sizeof(chain_buffer))chain_page_io(p,at,sizeof(chain_buffer));
    return 1;
}
static uint8_t template_page(OverlayBanks *b,uint16_t member) {
    const uint8_t *r=record(b,member);uint8_t i,p,group=(uint8_t)word(r);
    uint16_t at,n,j,a=0,z=0,size=word(r+4);
    for(i=0;i<OVERLAY_RAM_PAGES;++i)if(b->cached_member[i]==member)return b->cached_page[i];
    if(!group_open(b,group)||!page(b,&p)||!position(b,b->files[group],dword(r+8)))return 255;
    for(at=0;at<size;at+=n) {
        n=size-at;if(n>sizeof(b->buffer))n=sizeof(b->buffer);
        if(!exact(b,b->files[group],n))return 255;
        for(j=0;j<n;++j){a=(a+b->buffer[j])%255u;z=(z+a)%255u;}
        memcpy(chain_buffer,b->buffer,n);chain_store=1;chain_page_io(p,at,n);
    }
    if(((uint16_t)(z<<8)|a)!=word(r+12)){fail(b,ERR_ENTRY_CORRUPTED);return 255;}
    for(i=0;i<OVERLAY_RAM_PAGES;++i)if(b->cached_page[i]==255) {
        b->cached_member[i]=member;b->cached_page[i]=p;return p;
    }
    fail(b,ERR_NO_MORE_MEMORY);return 255;
}
static uint8_t descendant(OverlayBanks *b,uint8_t group,uint16_t member) {
    uint16_t parent=b->parent[group];
    while(parent!=OVERLAY_NO_MEMBER) {
        if(parent==member)return 1;parent=b->parent[word(record(b,parent))];
    }
    return 0;
}
uint16_t overlay_banks_entry(const OverlayBanks *b,uint16_t member) {
    return b->ready&&member<b->count?word(record(b,member)+6):0;
}
uint8_t overlay_banks_enter(OverlayBanks *b,uint16_t member) {
    const uint8_t *r;uint16_t at,old,parent;uint8_t group,i,p;
    if(!b->ready||b->error||member>=b->count){fail(b,ERR_INVALID_PARAMETER);return 255;}
    r=record(b,member);group=(uint8_t)word(r);parent=word(r+2);old=b->selected[group];
    if(parent!=OVERLAY_NO_MEMBER&&b->selected[word(record(b,parent))]!=parent) {
        fail(b,ERR_INVALID_PARAMETER);return 255;
    }
    if(b->active[group]==65535u){fail(b,ERR_NO_MORE_MEMORY);return 255;}
    if(old!=member) {
        if(b->active[group]){fail(b,ERR_INVALID_PARAMETER);return 255;}
        for(i=0;i<b->groups;++i)if(old!=OVERLAY_NO_MEMBER&&descendant(b,i,old)&&b->active[i]) {
            fail(b,ERR_INVALID_PARAMETER);return 255;
        }
        if(!group_open(b,group))return 255;
        /* Allocate the work page first: recycling a template to obtain it
         * must not invalidate the template chosen for this same switch. */
        if(b->execution[group]==255&&!page(b,b->execution+group))return 255;
        p=template_page(b,member);if(p==255)return 255;
        for(at=0;at<BANK_BYTES;at+=sizeof(chain_buffer)) {
            chain_store=0;chain_page_io(p,at,sizeof(chain_buffer));
            chain_store=1;chain_page_io(b->execution[group],at,sizeof(chain_buffer));
        }
        for(i=0;i<b->groups;++i)if(old!=OVERLAY_NO_MEMBER&&descendant(b,i,old))b->selected[i]=OVERLAY_NO_MEMBER;
        b->selected[group]=member;
    }
    ++b->active[group];return b->execution[group];
}
uint8_t overlay_banks_leave(OverlayBanks *b,uint16_t member) {
    uint16_t group;
    if(!b->ready||member>=b->count)return fail(b,ERR_INVALID_PARAMETER);
    group=word(record(b,member));
    if(b->selected[group]!=member||!b->active[group])return fail(b,ERR_INVALID_PARAMETER);
    --b->active[group];return 1;
}
uint8_t overlay_banks_close(OverlayBanks *b) {
    uint8_t i,error,ok=1;
    b->ready=0;
    for(i=0;i<b->groups;++i)if(b->files[i]>=0) {
        error=close(b->files[i]);b->files[i]=-1;if(error){fail(b,error);ok=0;}
    }
    for(i=0;i<b->owned_count;++i)if(b->owned[i]!=255) {
        error=pfree(b->owned[i]);if(error){fail(b,error);ok=0;}else b->owned[i]=255;
    }
    if(ok)overlay_banks_init(b);
    return ok;
}
