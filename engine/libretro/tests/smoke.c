/* Minimal libretro 1.7.5 host: direct path, video/audio, input and unload. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#define RETRO_IMPORT_SYMBOLS
#include "libretro.h"
static unsigned frames, videos, audible, polls, shutdown_requested;
static int in_game;
static FILE *pcm;
static char save_path[1024];
static HMODULE core_module;
static LONG WINAPI exception_log(EXCEPTION_POINTERS *e) {
    CONTEXT context=*e->ContextRecord;
    fprintf(stderr,"Exception %lx frame=%u\n",e->ExceptionRecord->ExceptionCode,frames);
    for(unsigned i=0;i<20 && context.Rip;++i) {
        HMODULE module=NULL; char name[MAX_PATH]={0};
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCSTR)context.Rip,&module);
        if(module) GetModuleFileNameA(module,name,sizeof(name));
        fprintf(stderr,"  %s +0x%llx\n",name,(unsigned long long)(context.Rip-(DWORD64)module));
        DWORD64 base=0; PRUNTIME_FUNCTION fn=RtlLookupFunctionEntry(context.Rip,&base,NULL);
        if(fn) {
            PVOID handler=NULL; DWORD64 frame=0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER,base,context.Rip,fn,&context,&handler,&frame,NULL);
        } else {
            SIZE_T read=0; DWORD64 address=0;
            if(!ReadProcessMemory(GetCurrentProcess(),(void*)context.Rsp,&address,sizeof(address),&read) || read!=sizeof(address)) break;
            context.Rip=address; context.Rsp+=8;
        }
    }
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
static void log_cb(enum retro_log_level level, const char *fmt, ...) {
    char text[1024];
    va_list args; va_start(args, fmt); vsnprintf(text,sizeof(text),fmt,args); va_end(args);
    if(strstr(text,"[state] playing=1")) in_game=1;
    if(strstr(text,"[state] playing=0")) in_game=0;
    fputs(text,stderr);
}
static bool env(unsigned cmd, void *data) {
    switch(cmd) {
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char **)data=save_path; return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: ((struct retro_log_callback *)data)->log=log_cb; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_XRGB8888;
    case RETRO_ENVIRONMENT_SET_GEOMETRY: {
        struct retro_game_geometry *g=data;
        fprintf(stderr,"[geometry] frame=%u %ux%u aspect=%.4f\n",frames,g->base_width,g->base_height,g->aspect_ratio);
        return true;
    }
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: return true;
    case RETRO_ENVIRONMENT_SHUTDOWN: shutdown_requested=1; return true;
    default: return false;
    }
}
static void video(const void *data, unsigned w, unsigned h, size_t pitch) {
    if(!data) return;
    ++videos;
    if(frames % 600 == 599) {
        char path[80]; snprintf(path,sizeof(path),"frame-%u.ppm",frames+1);
        FILE *f=fopen(path,"wb"); if(!f) return;
        fprintf(f,"P6\n%u %u\n255\n",w,h);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            uint32_t c=((const uint32_t *)((const char *)data+y*pitch))[x];
            unsigned char rgb[]={c>>16,c>>8,c}; fwrite(rgb,1,3,f);
        }
        fclose(f);
    }
}
static size_t audio(const int16_t *data,size_t count) {
    if(pcm) fwrite(data,sizeof(int16_t)*2,count,pcm);
    for(size_t i=0;i<count*2;++i) if(data[i]) { ++audible; break; }
    return count;
}
static void poll(void) { ++polls; }
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id) {
    if(getenv("OPENBOR_TEST_NO_INPUT")) return 0;
    if(port) return 0;
    if(getenv("OPENBOR_TEST_LOAD_SAVE") && !in_game && frames>=1300 && frames<1305)
        return id==RETRO_DEVICE_ID_JOYPAD_DOWN;
    if(getenv("OPENBOR_TEST_BUTTON_CYCLE") && frames>1500) {
        static const unsigned buttons[]={RETRO_DEVICE_ID_JOYPAD_Y,RETRO_DEVICE_ID_JOYPAD_B,
            RETRO_DEVICE_ID_JOYPAD_A,RETRO_DEVICE_ID_JOYPAD_X,RETRO_DEVICE_ID_JOYPAD_L,
            RETRO_DEVICE_ID_JOYPAD_R,RETRO_DEVICE_ID_JOYPAD_START,RETRO_DEVICE_ID_JOYPAD_RIGHT};
        return id==buttons[(frames/60)%8] && frames%60<5;
    }
    if(in_game && getenv("OPENBOR_TEST_SWAP")) {
        unsigned button=strcmp(getenv("OPENBOR_TEST_SWAP"),"left")==0 ? RETRO_DEVICE_ID_JOYPAD_L : RETRO_DEVICE_ID_JOYPAD_R;
        return id==button && frames%600>=100 && frames%600<105;
    }
    if(getenv("OPENBOR_TEST_SELECT") && frames>=2400 && frames<3300)
        return id==RETRO_DEVICE_ID_JOYPAD_RIGHT && frames%60<5;
    if(getenv("OPENBOR_TEST_BLOCK")) {
        /* Select Dazzler (three entries right), then hold and release defense. */
        if(frames > 1500 && frames < 1700)
            return id==RETRO_DEVICE_ID_JOYPAD_RIGHT &&
                ((frames>=1540 && frames<1545) || (frames>=1580 && frames<1585) || (frames>=1620 && frames<1625));
        if(frames>=2400 && frames<2700)
            return id==RETRO_DEVICE_ID_JOYPAD_X;
        if(frames>=2700 && frames<3300) return 0;
    }
    if(id==RETRO_DEVICE_ID_JOYPAD_START) return !in_game && frames > 600 && frames % 300 < 10;
    if(frames>1500 && id==RETRO_DEVICE_ID_JOYPAD_Y) return frames%20<10;
    if(in_game && getenv("OPENBOR_COMBAT_SWEEP")) {
        unsigned phase=frames%1200;
        if(id==RETRO_DEVICE_ID_JOYPAD_RIGHT) return phase<650;
        if(id==RETRO_DEVICE_ID_JOYPAD_LEFT) return phase>=650 && phase<1000;
        if(id==RETRO_DEVICE_ID_JOYPAD_UP) return phase>=1000 && phase<1100;
        if(id==RETRO_DEVICE_ID_JOYPAD_DOWN) return phase>=1100;
    } else if(frames>1500 && id==RETRO_DEVICE_ID_JOYPAD_RIGHT) return frames%300<150;
    return 0;
}
int wmain(int argc,wchar_t **wide_argv) {
    char **argv=calloc(argc,sizeof(char *));
    for(int i=0;i<argc;++i) {
        int size=WideCharToMultiByte(CP_UTF8,0,wide_argv[i],-1,NULL,0,NULL,NULL);
        argv[i]=malloc(size); WideCharToMultiByte(CP_UTF8,0,wide_argv[i],-1,argv[i],size,NULL,NULL);
    }
    if(argc<3) return 2;
    _putenv("OPENBOR_LIBRETRO_TRACE=1"); /* Input automation needs gameplay state. */
    if(getenv("OPENBOR_CAPTURE_PCM")) pcm=fopen("audio.pcm","wb");
    wchar_t cwd[1024]; GetCurrentDirectoryW(1024,cwd);
    WideCharToMultiByte(CP_UTF8,0,cwd,-1,save_path,sizeof(save_path),NULL,NULL);
    HMODULE dll=LoadLibraryA(argv[1]); if(!dll) { fprintf(stderr,"LoadLibrary error %lu\n",GetLastError()); return 3; }
    core_module=dll; AddVectoredExceptionHandler(1,exception_log);
#define BIND(name) __typeof__(&name) name##_fn=(__typeof__(&name))GetProcAddress(dll,#name); if(!name##_fn) return 4
    BIND(retro_set_environment); BIND(retro_set_video_refresh); BIND(retro_set_audio_sample_batch);
    BIND(retro_set_input_poll); BIND(retro_set_input_state); BIND(retro_init); BIND(retro_deinit);
    BIND(retro_load_game); BIND(retro_unload_game); BIND(retro_run); BIND(retro_get_system_info); BIND(retro_reset);
    retro_set_environment_fn(env); retro_set_video_refresh_fn(video); retro_set_audio_sample_batch_fn(audio);
    retro_set_input_poll_fn(poll); retro_set_input_state_fn(input); retro_init_fn();
    struct retro_system_info info; retro_get_system_info_fn(&info);
    fprintf(stderr,"%s %s fullpath=%d\n",info.library_name,info.library_version,info.need_fullpath);
    struct retro_game_info invalid={"missing.pak",NULL,0,NULL};
    if(retro_load_game_fn(NULL) || retro_load_game_fn(&invalid)) return 8;
    FILE *bad=fopen("invalid.pak","wb");
    if(!bad) return 9;
    fwrite("PACK\0\0\0\0",1,8,bad); fclose(bad);
    invalid.path="invalid.pak";
    if(retro_load_game_fn(&invalid)) return 10;
    struct retro_game_info game={argv[2],NULL,0,NULL};
    if(!retro_load_game_fn(&game)) return 5;
    retro_unload_game_fn(); /* unload before first retro_run */
    if(!retro_load_game_fn(&game)) return 5;
    unsigned limit=argc>3?atoi(argv[3]):2400;
    for(frames=0;frames<limit && !shutdown_requested;++frames) retro_run_fn();
    retro_unload_game_fn();
    if(argc > 4) {
        fprintf(stderr,"Reloading content in same core instance.\n");
        if(!retro_load_game_fn(&game)) return 7;
        retro_run_fn(); retro_reset_fn();
        for(frames=0;frames<limit && !shutdown_requested;++frames) retro_run_fn();
        retro_unload_game_fn();
    }
    retro_deinit_fn(); FreeLibrary(dll);
    fprintf(stderr,"frames=%u video=%u audible=%u polls=%u shutdown=%u\n",frames,videos,audible,polls,shutdown_requested);
    return videos && audible && !shutdown_requested ? 0 : 6;
}
