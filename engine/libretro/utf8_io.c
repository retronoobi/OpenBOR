#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <errno.h>
#include "utf8_io.h"
static int wide(const char *path, wchar_t *out, int capacity)
{
    if(!path || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, out, capacity)) {
        errno = EINVAL; return 0;
    }
    return 1;
}
FILE *bor_utf8_fopen(const char *path, const char *mode)
{
    wchar_t name[32768], flags[32];
    if(!wide(path,name,32768) || !wide(mode,flags,32)) return NULL;
    return _wfopen(name,flags);
}
int bor_utf8_open(const char *path, int flags, ...)
{
    wchar_t name[32768];
    int mode = 0;
    if(flags & O_CREAT) { va_list args; va_start(args,flags); mode=va_arg(args,int); va_end(args); }
    if(!wide(path,name,32768)) return -1;
    return _wopen(name,flags,mode);
}
int bor_utf8_mkdir(const char *path)
{
    wchar_t name[32768];
    if(!wide(path,name,32768)) return -1;
    return _wmkdir(name);
}
