#ifndef OPENBOR_LIBRETRO_UTF8_IO_H
#define OPENBOR_LIBRETRO_UTF8_IO_H
/* Include CRT declarations before redirecting engine calls. */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <direct.h>
FILE *bor_utf8_fopen(const char *path, const char *mode);
int bor_utf8_open(const char *path, int flags, ...);
int bor_utf8_mkdir(const char *path);
#ifndef BOR_UTF8_IMPLEMENTATION
#define fopen bor_utf8_fopen
#define open bor_utf8_open
#define mkdir bor_utf8_mkdir
#define _mkdir bor_utf8_mkdir
#endif
#endif
