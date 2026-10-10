/* Session isolation for the OpenBOR engine's process-lifetime global state.
 * The public DLL contains its engine as RCDATA, so installation is one DLL.
 * Only this core's randomly named temporary file is created/deleted.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"

static HMODULE engine;
static wchar_t engine_path[MAX_PATH];
static char *content_path;
static retro_environment_t env_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t batch_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
static unsigned devices[4] = {RETRO_DEVICE_JOYPAD,RETRO_DEVICE_JOYPAD,RETRO_DEVICE_JOYPAD,RETRO_DEVICE_JOYPAD};
#define FUNCTIONS(X) \
 X(retro_set_environment) X(retro_set_video_refresh) X(retro_set_audio_sample) \
 X(retro_set_audio_sample_batch) X(retro_set_input_poll) X(retro_set_input_state) \
 X(retro_init) X(retro_deinit) X(retro_load_game) X(retro_unload_game) \
 X(retro_run) X(retro_get_system_av_info) X(retro_set_controller_port_device)
#define DECLARE(name) static __typeof__(&name) engine_##name;
FUNCTIONS(DECLARE)

static void close_engine(void)
{
    if(engine) { FreeLibrary(engine); engine = NULL; }
    if(engine_path[0]) { DeleteFileW(engine_path); engine_path[0] = 0; }
}
static bool open_engine(void)
{
    HMODULE self;
    HRSRC resource;
    HGLOBAL blob;
    wchar_t temp[MAX_PATH];
    DWORD written, size;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         (LPCWSTR)&open_engine, &self)) return false;
    resource = FindResourceW(self, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(10));
    if(!resource) return false;
    size = SizeofResource(self, resource);
    blob = LoadResource(self, resource);
    DWORD length = GetTempPathW(MAX_PATH, temp);
    if(!blob || !size || !length || length >= MAX_PATH || !GetTempFileNameW(temp, L"obr", 0, engine_path)) return false;
    HANDLE file = CreateFileW(engine_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if(file == INVALID_HANDLE_VALUE) { close_engine(); return false; }
    bool ok = WriteFile(file, LockResource(blob), size, &written, NULL) && written == size;
    CloseHandle(file);
    if(!ok) { close_engine(); return false; }
    engine = LoadLibraryW(engine_path);
    if(!engine) { close_engine(); return false; }
#define BIND(name) engine_##name = (__typeof__(&name))GetProcAddress(engine, #name); if(!engine_##name) { close_engine(); return false; }
    FUNCTIONS(BIND)
    engine_retro_set_environment(env_cb);
    engine_retro_set_video_refresh(video_cb);
    engine_retro_set_audio_sample(audio_cb);
    engine_retro_set_audio_sample_batch(batch_cb);
    engine_retro_set_input_poll(poll_cb);
    engine_retro_set_input_state(input_cb);
    engine_retro_init();
    for(unsigned port=0;port<4;++port) engine_retro_set_controller_port_device(port,devices[port]);
    return true;
}
void retro_set_environment(retro_environment_t cb) {
    env_cb=cb;
    if(engine) engine_retro_set_environment(cb);
    static struct retro_input_descriptor descriptors[4*12+1];
    static const unsigned ids[]={4,5,6,7,1,0,9,8,10,11,3,2};
    static const char *names[]={"Up","Down","Left","Right","Attack","Jump","Special","Attack 2","Attack 3","Attack 4","Start / Pause","Menu / Back"};
    for(unsigned port=0;port<4;++port) for(unsigned button=0;button<12;++button) {
        struct retro_input_descriptor *d=&descriptors[port*12+button];
        d->port=port; d->device=RETRO_DEVICE_JOYPAD; d->index=0;
        d->id=ids[button]; d->description=names[button];
    }
    cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,descriptors);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb=cb; if(engine) engine_retro_set_video_refresh(cb); }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb=cb; if(engine) engine_retro_set_audio_sample(cb); }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { batch_cb=cb; if(engine) engine_retro_set_audio_sample_batch(cb); }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb=cb; if(engine) engine_retro_set_input_poll(cb); }
void retro_set_input_state(retro_input_state_t cb) { input_cb=cb; if(engine) engine_retro_set_input_state(cb); }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_init(void) {}
void retro_unload_game(void) {
    if(engine) { engine_retro_unload_game(); engine_retro_deinit(); }
    close_engine(); free(content_path); content_path=NULL;
}
void retro_deinit(void) { retro_unload_game(); }
void retro_get_system_info(struct retro_system_info *info) {
    memset(info,0,sizeof(*info)); info->library_name="OpenBOR";
    info->library_version="7533-libretro-r10"; info->valid_extensions="pak";
    info->need_fullpath=true;
}
void retro_get_system_av_info(struct retro_system_av_info *info) {
    if(engine) { engine_retro_get_system_av_info(info); return; }
    memset(info,0,sizeof(*info));
    info->geometry.base_width=320; info->geometry.base_height=240;
    info->geometry.max_width=1920; info->geometry.max_height=1080;
    info->geometry.aspect_ratio=4.0f/3.0f;
    info->timing.fps=60; info->timing.sample_rate=44100;
}
bool retro_load_game(const struct retro_game_info *game) {
    if(engine || !game || !game->path || !env_cb) return false;
    content_path=strdup(game->path);
    if(!content_path || !open_engine() || !engine_retro_load_game(game)) {
        retro_unload_game(); return false;
    }
    return true;
}
void retro_reset(void) {
    if(!content_path) return;
    char *path=strdup(content_path);
    if(!path) return;
    retro_unload_game();
    struct retro_game_info game={path,NULL,0,NULL};
    if(!retro_load_game(&game)) env_cb(RETRO_ENVIRONMENT_SHUTDOWN,NULL);
    free(path);
}
void retro_run(void) { if(engine) engine_retro_run(); }
void retro_set_controller_port_device(unsigned port,unsigned device) {
    if(port<4) devices[port]=device;
    if(engine) engine_retro_set_controller_port_device(port,device);
}
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data,size_t size) { return false; }
bool retro_unserialize(const void *data,size_t size) { return false; }
void *retro_get_memory_data(unsigned id) { return NULL; }
size_t retro_get_memory_size(unsigned id) { return 0; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index,bool enabled,const char *code) {}
bool retro_load_game_special(unsigned type,const struct retro_game_info *games,size_t count) { return false; }
