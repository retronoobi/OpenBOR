#ifndef LIBRETRO_WEBM_H
#define LIBRETRO_WEBM_H
#include <stddef.h>
#include <stdint.h>
int playwebm(const char *path, int noskip);
void libretro_webm_close(void);
int libretro_webm_audio(int16_t *out, size_t frames, unsigned rate);
void libretro_webm_yield(void);
void libretro_webm_present(const uint32_t *pixels, unsigned w, unsigned h, float aspect);
void libretro_webm_restore_video(void);
#endif
