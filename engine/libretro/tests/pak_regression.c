#include "openbor.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "../content_path.h"
static unsigned test_readlsb32(const unsigned char *p) { return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24); }
#define readlsb32 test_readlsb32
#include "pak_validator.inc"
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(0)
static int valid(unsigned char *data,size_t size) {
    FILE *f=tmpfile(); if(!f) return -1;
    fwrite(data,1,size,f); rewind(f);
    int result=validate_pak(f); fclose(f); return result;
}
int main(void) {
    unsigned char data[30]={'P','A','C','K',0,0,0,0,1,2,3,4,14,0,0,0,8,0,0,0,4,0,0,0,'a',0,12,0,0,0};
    CHECK(valid(data,sizeof(data))==1);
    data[0]=0x1d; data[1]=0x11; data[2]=0x14; data[3]=0x7f;
    CHECK(valid(data,sizeof(data))==1);
    data[4]=1; CHECK(valid(data,sizeof(data))==0); data[4]=0;
    data[20]=5; CHECK(valid(data,sizeof(data))==0); data[20]=4;
    data[25]='b'; CHECK(valid(data,sizeof(data))==0); data[25]=0;
    CHECK(valid(data,sizeof(data)-1)==0);
    data[0]=0; CHECK(valid(data,sizeof(data))==0);
    CHECK(libretro_is_content_alias("Paks/SORX.pak"));
    CHECK(libretro_is_content_alias("PAKS\\1.0.0.PAK"));
    CHECK(!libretro_is_content_alias("Paks/../other.pak"));
    CHECK(!libretro_is_content_alias("Paks/file.txt"));
    CHECK(!libretro_is_content_alias("data/test.pak"));
    CHECK(!libretro_is_content_alias("Paks"));
    CHECK(!libretro_is_content_alias(""));
    puts("PASS: standard/alternate archives, corrupt archives and content aliases."); return 0;
}
