/* Decode real, synthetic VP8/Vorbis files with the production player.
 * Only engine I/O and the frontend clock/output are replaced. */
#include "openbor.h"
#include "webm.h"
#include <stdarg.h>
#include <math.h>
#include <setjmp.h>
#undef printf
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); exit(1); } } while(0)
char packfile[MAX_FILENAME_LEN];
s_savedata savedata;
unsigned long long bothnewkeys;
static a_playrecstatus recorder;
a_playrecstatus *playrecstatus=&recorder;
static FILE *media;
static uint64_t clock_us;
static unsigned ticks, pictures, restored, music_updates, close_music, skip_at, abort_at;
static unsigned output_rate=44100;
static uint32_t color;
static double energy, difference, sum;
static size_t samples;
static jmp_buf abort_point;
static int external;
void writeToLogFile(const char *fmt,...) { va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap); }
void exitIfFalse(int value,const char *what,const char *func,const char *file,int line)
{ if(!value) { fprintf(stderr,"%s %s %s:%d\n",what,func,file,line); exit(2); } }
void *checkAlloc(void *p,size_t size,const char *func,const char *file,int line)
{ CHECK(p || !size); return p; }
int openpackfile(const char *path,const char *unused) { CHECK(!media); media=fopen(path,"rb"); return media ? 0 : -1; }
int closepackfile(int handle) { CHECK(media); fclose(media); media=NULL; return 0; }
int readpackfile(int handle,void *dst,int size) { return (int)fread(dst,1,size,media); }
int seekpackfile(int handle,int offset,int whence) { return fseek(media,offset,whence) ? -1 : (int)ftell(media); }
u64 timer_uticks(void) { return clock_us; }
void inputrefresh() { bothnewkeys=skip_at && ticks>=skip_at ? FLAG_ATTACK : 0; }
void sound_close_music(void) { ++close_music; }
void sound_update_music(void) { ++music_updates; }
void libretro_webm_present(const uint32_t *p,unsigned w,unsigned h,float aspect)
{ CHECK(external || (w==16 && h==16 && fabs(aspect-1)<0.001)); ++pictures; color=p[0]; }
void libretro_webm_restore_video(void) { ++restored; }
void libretro_webm_yield(void)
{
    int16_t out[4096]={0};
    unsigned count=output_rate/60;
    if(libretro_webm_audio(out,count,output_rate)) for(unsigned i=0;i<count;++i) {
        double l=out[i*2],r=out[i*2+1];
        energy+=l*l; difference+=fabs(l-r); sum+=fabs(l+r); ++samples;
        CHECK(external || (l>-32768 && l<32767 && r>-32768 && r<32767));
    }
    clock_us+=16667; ++ticks;
    CHECK(ticks<(external ? 36000 : 180));
    if(abort_at && ticks==abort_at) { libretro_webm_close(); longjmp(abort_point,1); }
}
static void reset(void)
{
    CHECK(!media); libretro_webm_close();
    clock_us=0; ticks=pictures=restored=music_updates=close_music=skip_at=abort_at=0;
    energy=difference=sum=0; samples=0; savedata.musicvol=100;
}
int main(int argc,char **argv)
{
    if(argc>1) {
        external=1; reset();
        CHECK(playwebm(argv[1],1)==1);
        printf("frames=%u seconds=%.3f RMS=%.1f\n",pictures,ticks/60.0,samples ? sqrt(energy/samples) : 0);
        return 0;
    }
    reset(); CHECK(playwebm("stereo.webm",0)==1);
    CHECK(pictures==15 && ticks>=59 && ticks<=64 && restored==1 && close_music==1);
    CHECK((color>>16)>245 && ((color>>8)&255)<5 && (color&255)<5);
    CHECK(sqrt(energy/samples)>5000 && sqrt(energy/samples)<6500);
    CHECK(sum/samples<20 && difference/samples>9000);
    double full_energy=energy;
    reset(); savedata.musicvol=50; CHECK(playwebm("stereo.webm",0)==1);
    CHECK(energy/full_energy>0.249 && energy/full_energy<0.251);
    reset(); skip_at=4; CHECK(playwebm("stereo.webm",0)==-1 && ticks==4 && restored==1);
    reset(); skip_at=4; CHECK(playwebm("stereo.webm",1)==1 && pictures==15);
    reset(); CHECK(playwebm("silent.webm",0)==1);
    CHECK(pictures==12 && ticks>=29 && ticks<=32 && music_updates==ticks && !close_music);
    CHECK((color&255)>245 && (color>>16)<5);
    reset(); output_rate=48000; CHECK(playwebm("mono.webm",0)==1);
    CHECK(pictures==7 && ticks>=29 && ticks<=35 && energy>0 && difference==0);
    reset(); CHECK(playwebm("missing.webm",0)==0 && restored==1);
    reset(); CHECK(playwebm("README.md",0)==0 && restored==1);
    reset(); abort_at=5;
    if(!setjmp(abort_point)) { playwebm("stereo.webm",0); CHECK(0); }
    CHECK(!media); /* Unload releases decoder, packet, queued video and audio. */
    reset(); CHECK(playwebm("stereo.webm",0)==1);
    CHECK(pictures==15 && restored==1);
    puts("WebM: video, audio/resampling, volume, EOF, skip/noskip, errors and interrupted unload passed.");
    return 0;
}
