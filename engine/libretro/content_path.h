#ifndef OPENBOR_LIBRETRO_CONTENT_PATH_H
#define OPENBOR_LIBRETRO_CONTENT_PATH_H
/* Single-content frontends select the archive, not its original install name.
 * Expose the active archive for legacy read-only Paks/<name>.pak requests. */
static int libretro_is_content_alias(const char *path)
{
    const char *name;
    size_t length;
    if(!path || strnicmp(path,"Paks",4) || (path[4]!='/' && path[4]!='\\')) return 0;
    name=path+5; length=strlen(name);
    return length>4 && !strchr(name,'/') && !strchr(name,'\\') &&
        !strchr(name,':') && stricmp(name+length-4,".pak")==0;
}
#endif
