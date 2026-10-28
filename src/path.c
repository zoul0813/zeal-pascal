#include "path.h"
#include <string.h>

static uint8_t resolve(const char *name,const char *directory,char *out,uint8_t folder) {
    char full[128];
    uint16_t i,a,n,w=3,j;
    n=(uint16_t)strlen(name);
    if(!n||n>127||(!folder&&name[n-1]=='/')) return 0;
    if(n>1&&name[1]==':') {
        if(n<3||name[2]!='/'||(!folder&&n<4)) return 0;
        strcpy(full,name);
    } else {
        i=(uint16_t)strlen(directory);
        if(i<3||i>127) return 0;
        if(name[0]=='/') { memcpy(full,directory,2); full[2]=0; }
        else {
            strcpy(full,directory);
            if(full[i-1]!='/') { if(i==127) return 0; full[i++]='/'; full[i]=0; }
        }
        if(strlen(full)+n>127) return 0;
        strcat(full,name);
    }
    if(full[0]>='a'&&full[0]<='z') full[0]-=32;
    if(full[0]<'A'||full[0]>'Z'||full[1]!=':'||full[2]!='/') return 0;
    memcpy(out,full,3); i=3;
    while(full[i]) {
        while(full[i]=='/') ++i;
        a=i; while(full[i]&&full[i]!='/') ++i; n=i-a;
        if(!n) break;
        if(n==1&&full[a]=='.') { if(!folder&&!full[i]) return 0; continue; }
        if(n==2&&full[a]=='.'&&full[a+1]=='.') {
            if(!folder&&!full[i]) return 0;
            if(w>3) { --w; while(w>3&&out[w-1]!='/') --w; }
            continue;
        }
        if(n>15) return 0;
        if(w>3&&out[w-1]!='/') out[w++]='/';
        if(w+n>127) return 0;
        for(j=0;j<n;++j) {
            if((uint8_t)full[a+j]<32||full[a+j]==':') return 0;
            out[w++]=full[a+j];
        }
    }
    if(!folder&&w<=3) return 0;
    while(w>3&&out[w-1]=='/')--w;
    out[w]=0; return 1;
}
uint8_t path_resolve(const char *name,const char *directory,char *out) {
    return resolve(name,directory,out,0);
}
uint8_t path_directory(const char *name,const char *directory,char *out) {
    return resolve(name,directory,out,1);
}
