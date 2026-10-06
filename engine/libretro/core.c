/* OpenBOR 4.0 libretro backend. See ../../LICENSE for engine license. */
#define WIN32_LEAN_AND_MEAN
#define LONG WIN_LONG
#define DWORD WIN_DWORD
#define ULONG WIN_ULONG
#define HRESULT WIN_HRESULT
#include <windows.h>
#undef LONG
#undef DWORD
#undef ULONG
#undef HRESULT
#include <direct.h>
#include "libretro.h"
#include "openbor.h"
#undef printf

static retro_environment_t environment;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t sample_cb;
static retro_audio_sample_batch_t audio_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
static retro_log_printf_t logger;
static void *frontend_fiber, *engine_fiber;
static bool converted, stopping, finished, loaded, started;
static uint64_t clock_us, interval_us;
static unsigned timer_offset, width = 320, height = 240;
static float display_aspect = 4.0f / 3.0f;
static unsigned audio_rate = 44100, audio_bits = 16, audio_remainder;
static bool audio_active;
static bool trace_enabled;
static unsigned run_count;
extern int libretro_trace_state(int emit);
static uint32_t *framebuffer, palette[256];
static size_t framebuffer_pixels;
static int gamma_value, brightness_value;
static unsigned devices[4] = {1,1,1,1};
char packfile[MAX_FILENAME_LEN], paksDir[MAX_FILENAME_LEN];
char savesDir[MAX_FILENAME_LEN], logsDir[MAX_FILENAME_LEN], screenShotsDir[MAX_FILENAME_LEN];
int opengl;
u8 pDeltaBuffer[480 * 2592];
char *GfxBlitterNames[25] = {"Frontend", "Frontend", "Frontend", "Frontend", "Frontend", "Frontend", "Frontend", "Frontend", "Frontend", "Frontend", "Frontend", "Frontend"};
s_joysticks joysticks[4];

static void yield_frame(void)
{
    SwitchToFiber(frontend_fiber);
    if(stopping) {
        libretro_webm_close();
        borShutdown(0, "Content unloaded by libretro.\n");
    }
}

void borExit(int status)
{
    libretro_webm_close();
    if(logger) logger(status ? RETRO_LOG_ERROR : RETRO_LOG_INFO, "OpenBOR stopped (%d).\n", status);
    finished = true;
    for(;;) SwitchToFiber(frontend_fiber);
}

static VOID WINAPI engine_entry(void *unused)
{
    char *argv[] = {"openbor_libretro", packfile, NULL};
    (void)unused;
    started = true;
    setSystemRam();
    packfile_mode(0);
    openborMain(2, argv);
    borExit(0);
}

void retro_set_environment(retro_environment_t cb) { environment = cb; }
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { sample_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_init(void)
{
    trace_enabled = getenv("OPENBOR_LIBRETRO_TRACE") != NULL;
    struct retro_log_callback log;
    if(environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) logger = log.log;
}
void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
    info->library_name = "OpenBOR";
    info->library_version = "7533-libretro-r6";
    info->valid_extensions = "pak";
    info->need_fullpath = true;
    info->block_extract = false;
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
    memset(info, 0, sizeof(*info));
    info->geometry.base_width = width;
    info->geometry.base_height = height;
    info->geometry.max_width = 1920;
    info->geometry.max_height = 1080;
    info->geometry.aspect_ratio = display_aspect;
    info->timing.fps = 60.0;
    info->timing.sample_rate = audio_rate;
}
static bool setup_paths(const char *path)
{
    char absolute[MAX_FILENAME_LEN], base[MAX_FILENAME_LEN];
    const char *save = NULL;
    char *slash;
    wchar_t input[MAX_FILENAME_LEN], full[MAX_FILENAME_LEN];
    if(!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, input, MAX_FILENAME_LEN)) return false;
    DWORD length = GetFullPathNameW(input, MAX_FILENAME_LEN, full, NULL);
    if(!length || length >= MAX_FILENAME_LEN || !WideCharToMultiByte(CP_UTF8, 0, full, -1, absolute, sizeof(absolute), NULL, NULL)) return false;
    strcpy(packfile, absolute);
    strcpy(base, absolute);
    slash = strrchr(base, '\\');
    if(!slash) return false;
    *slash = 0;
    strcpy(paksDir, base);
    environment(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save);
    if(!save || !*save) save = base;
    /* Engine save functions append the content basename to these paths. */
    if(strlen(save) + strlen("/OpenBOR/ScreenShots/") + strlen(strrchr(absolute, '\\') + 1) + 8 >= MAX_FILENAME_LEN) return false;
    snprintf(savesDir, sizeof(savesDir), "%s/OpenBOR", save);
    if(_mkdir(savesDir) && errno != EEXIST) return false;
    snprintf(logsDir, sizeof(logsDir), "%s/Logs", savesDir);
    snprintf(screenShotsDir, sizeof(screenShotsDir), "%s/ScreenShots", savesDir);
    if(_mkdir(logsDir) && errno != EEXIST) return false;
    if(_mkdir(screenShotsDir) && errno != EEXIST) return false;
    return true;
}
static bool validate_pak(FILE *file)
{
    unsigned char bytes[MAX_FILENAME_LEN + 12];
    if(fread(bytes,1,8,file) != 8 || memcmp(bytes,"PACK\0\0\0\0",8)) return false;
    if(fseek(file,0,SEEK_END)) return false;
    long size = ftell(file);
    if(size < 25 || fseek(file,-4,SEEK_END) || fread(bytes,1,4,file) != 4) return false;
    unsigned table = readlsb32(bytes), position = table, entries = 0;
    if(table < 8 || table >= (unsigned)size-4 || fseek(file,table,SEEK_SET)) return false;
    while(position < (unsigned)size-4) {
        if(fread(bytes,1,12,file) != 12) return false;
        unsigned record = readlsb32(bytes), offset = readlsb32(bytes+4), length = readlsb32(bytes+8);
        if(record < 13 || record > sizeof(bytes) || record > (unsigned)size-4-position ||
           offset < 8 || offset > table || length > table-offset) return false;
        if(fread(bytes,1,record-12,file) != record-12 || !memchr(bytes,0,record-12)) return false;
        position += record; ++entries;
    }
    return entries > 0 && position == (unsigned)size-4;
}
bool retro_load_game(const struct retro_game_info *game)
{
    FILE *file;
    enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
    if(loaded || !game || !game->path || strlen(game->path) < 5 ||
       stricmp(game->path + strlen(game->path) - 4, ".pak") || !setup_paths(game->path)) return false;
    file = fopen(packfile, "rb");
    if(!file) return false;
    bool valid = validate_pak(file);
    fclose(file);
    if(!valid || !environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format)) return false;
    converted = !IsThreadAFiber();
    frontend_fiber = converted ? ConvertThreadToFiber(NULL) : GetCurrentFiber();
    if(!frontend_fiber) return false;
    engine_fiber = CreateFiber(8 * 1024 * 1024, engine_entry, NULL);
    if(!engine_fiber) { if(converted) ConvertFiberToThread(); return false; }
    loaded = true; stopping = finished = started = false;
    clock_us = interval_us = 0; audio_remainder = 0;
    return true;
}
void retro_unload_game(void)
{
    if(!loaded) return;
    if(started && !finished) { stopping = true; SwitchToFiber(engine_fiber); }
    DeleteFiber(engine_fiber); engine_fiber = NULL;
    if(converted) ConvertFiberToThread();
    loaded = false;
    free(framebuffer); framebuffer = NULL; framebuffer_pixels = 0;
    extern FILE *openborLog, *scriptLog;
    if(openborLog) { fclose(openborLog); openborLog = NULL; }
    if(scriptLog) { fclose(scriptLog); scriptLog = NULL; }
}
void retro_deinit(void) { retro_unload_game(); }
void retro_run(void)
{
    int16_t samples[4096];
    if(!loaded) return;
    if(finished) { environment(RETRO_ENVIRONMENT_SHUTDOWN, NULL); return; }
    if(poll_cb) poll_cb();
    clock_us += 16667;
    SwitchToFiber(engine_fiber);
    if(trace_enabled && !finished) {
        int playing=libretro_trace_state(++run_count % 600 == 0);
        static int previous=-1;
        if(playing != previous && logger) logger(RETRO_LOG_INFO,"[state] playing=%d\n",playing);
        previous=playing;
    }
    if(framebuffer && video_cb) video_cb(framebuffer, width, height, width * sizeof(uint32_t));
    audio_remainder += audio_rate;
    unsigned frames = audio_remainder / 60;
    audio_remainder %= 60;
    if(frames > 2048) frames = 2048;
    memset(samples, 0, frames * 4);
    if(!finished && !libretro_webm_audio(samples, frames, audio_rate) && audio_active) {
        update_sample((unsigned char *)samples, frames * 2 * (audio_bits / 8));
        if(audio_bits == 8) for(int i = (int)frames * 2 - 1; i >= 0; --i) samples[i] = (((unsigned char *)samples)[i] - 128) * 256;
    }
    if(audio_cb) audio_cb(samples, frames);
    else if(sample_cb) for(unsigned i = 0; i < frames; ++i) sample_cb(samples[i*2], samples[i*2+1]);
}
void retro_reset(void) { /* Engine has no safe complete in-process reset yet. */ }
void retro_set_controller_port_device(unsigned port, unsigned device) { if(port < 4) devices[port] = device; }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void *data, size_t size) { return false; }
void *retro_get_memory_data(unsigned id) { return NULL; }
size_t retro_get_memory_size(unsigned id) { return 0; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t count) { return false; }

int video_set_mode(s_videomodes mode)
{
    if(mode.hRes <= 0 || mode.vRes <= 0) return 1;
    if(mode.hRes > 1920 || mode.vRes > 1080) return 0;
    width = mode.hRes; height = mode.vRes;
    size_t pixels = (size_t)width * height;
    uint32_t *buffer = realloc(framebuffer, pixels * 4);
    if(!buffer) return 0;
    framebuffer = buffer; framebuffer_pixels = pixels;
    display_aspect = (float)width / height;
    memset(framebuffer, 0, pixels * 4);
    struct retro_system_av_info av;
    retro_get_system_av_info(&av);
    environment(RETRO_ENVIRONMENT_SET_GEOMETRY, &av.geometry);
    return 1;
}
void libretro_webm_yield(void) { yield_frame(); }
void libretro_webm_present(const uint32_t *pixels, unsigned w, unsigned h, float aspect)
{
    if(!framebuffer || width!=w || height!=h) {
        s_videomodes mode={0}; mode.hRes=w; mode.vRes=h;
        if(!video_set_mode(mode)) borShutdown(1,"Cannot allocate WebM framebuffer.\n");
    }
    if(display_aspect!=aspect) {
        display_aspect=aspect;
        struct retro_game_geometry geometry={w,h,1920,1080,aspect};
        environment(RETRO_ENVIRONMENT_SET_GEOMETRY,&geometry);
    }
    memcpy(framebuffer,pixels,(size_t)w*h*sizeof(uint32_t));
}
void libretro_webm_restore_video(void)
{
    extern s_videomodes videomodes;
    video_set_mode(videomodes);
}
static unsigned correct(unsigned c)
{
    int value = c + brightness_value;
    if(gamma_value) value += (255 - value) * gamma_value / 512;
    return value < 0 ? 0 : value > 255 ? 255 : value;
}
int video_copy_screen(s_screen *screen)
{
    if(!screen) return 0;
    if(screen->width != width || screen->height != height) {
        s_videomodes mode = {0}; mode.hRes = screen->width; mode.vRes = screen->height;
        if(!video_set_mode(mode)) return 0;
    }
    for(size_t i = 0; i < framebuffer_pixels; ++i) {
        unsigned r, g, b, color;
        if(screen->pixelformat == PIXEL_32) {
            color = ((uint32_t *)screen->data)[i]; r = color & 255; g = (color >> 8) & 255; b = (color >> 16) & 255;
        } else if(screen->pixelformat == 3) {
            r = screen->data[i*3]; g = screen->data[i*3+1]; b = screen->data[i*3+2];
        } else if(screen->pixelformat == PIXEL_16) {
            color = ((uint16_t *)screen->data)[i]; r = (color & 31) * 255 / 31; g = ((color >> 5) & 63) * 255 / 63; b = ((color >> 11) & 31) * 255 / 31;
        } else { color = palette[screen->data[i]]; r = color >> 16; g = (color >> 8) & 255; b = color & 255; }
        framebuffer[i] = (correct(r) << 16) | (correct(g) << 8) | correct(b);
    }
    yield_frame();
    return 1;
}
void vga_setpalette(unsigned char *pal) { for(unsigned i = 0; i < 256; ++i) palette[i] = (pal[i*3] << 16) | (pal[i*3+1] << 8) | pal[i*3+2]; }
void vga_vwait(void) {}
void video_clearscreen(void) { if(framebuffer) memset(framebuffer, 0, framebuffer_pixels * 4); }
void video_fullscreen_flip(void) {}
void video_stretch(int value) {}
void video_set_window_title(const char *title) {}
void video_set_color_correction(int gamma, int brightness) { gamma_value = gamma; brightness_value = brightness; }
void borTimerInit(void) { interval_us = clock_us; }
void borTimerExit(void) {}
unsigned timer_gettick(void) { return clock_us / 1000; }
u64 timer_uticks(void) { return clock_us; }
unsigned timer_getinterval(unsigned freq)
{
    /* Audio-only fades also need to return control to the frontend. */
    if(!freq) return 0;
    if((clock_us - interval_us) * freq < 1000000) yield_frame();
    unsigned ticks = (clock_us - interval_us) * freq / 1000000;
    if(ticks) interval_us += (uint64_t)ticks * 1000000 / freq;
    return ticks;
}
unsigned get_last_interval(void) { return interval_us / 1000; }
void set_last_interval(unsigned v) { interval_us = (uint64_t)v * 1000; }
void set_ticks(unsigned v) { timer_offset = v; }
int SB_playstart(int bits, int rate)
{
    if((bits != 8 && bits != 16) || rate <= 0 || rate > 96000) return 0;
    audio_bits = bits; audio_rate = rate; audio_active = true;
    struct retro_system_av_info av; retro_get_system_av_info(&av);
    environment(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
    return 1;
}
void SB_playstop(void) { audio_active = false; }
void SB_setvolume(char dev, char volume) {}
void SB_updatevolume(int volume) {}
void control_init(int enabled) {}
void control_exit(void) {}
int control_usejoy(int enabled) { return 1; }
int control_getjoyenabled(void) { return 1; }
void control_setkey(s_playercontrols *p, unsigned flag, int key) { if(p) { unsigned i=0; while(flag >>= 1) ++i; p->settings[i] = key; } }
int control_scankey(void) { return 0; }
int keyboard_getlastkey(void) { return 0; }
char *control_getkeyname(unsigned code) { return "RetroPad"; }
char *get_joystick_name(const char *name) { return "RetroPad"; }
void control_rumble(int port, int ratio, int msec) {}
void control_update(s_playercontrols **controls, int count)
{
    static const unsigned buttons[] = {RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT};
    static const u64 flags[] = {FLAG_MOVEUP, FLAG_MOVEDOWN, FLAG_MOVELEFT, FLAG_MOVERIGHT, FLAG_ATTACK, FLAG_JUMP, FLAG_SPECIAL, FLAG_ATTACK2, FLAG_ATTACK3, FLAG_ATTACK4, FLAG_START, FLAG_ESC};
    for(int p = 0; p < count && p < 4; ++p) if(controls[p]) {
        u64 keys = 0;
        if(input_cb && devices[p] != RETRO_DEVICE_NONE) for(unsigned b = 0; b < 12; ++b) if(input_cb(p, RETRO_DEVICE_JOYPAD, 0, buttons[b])) keys |= flags[b];
        controls[p]->newkeyflags = keys & ~controls[p]->keyflags;
        controls[p]->keyflags = keys; controls[p]->kb_break = !!(keys & FLAG_ESC);
    }
}
