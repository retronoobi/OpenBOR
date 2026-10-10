#include "../../openbor.c"
#include "../../source/scriptlib/ScriptVariant.c"
/* Engine definitions above supply the globals directly; avoid the legacy
 * extern declarations whose signedness differs from those definitions. */
#define SCRIPT_COMMON_H
#include "../../source/openborscript/constants.c"
#undef printf
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(0)
void *checkAlloc(void *p,size_t size,const char *fn,const char *file,int line) { if(!p && size) abort(); return p; }
void exitIfFalse(int ok,const char *what,const char *fn,const char *file,int line) { if(!ok) abort(); }
void writeToLogFile(const char *fmt,...) {}
int main(void)
{
    {
        ScriptVariant arg={0}, result={0}, *args[]={&arg}, *out=&result;
        arg.vt=VT_STR; arg.strVal=StrCache_CreateNewFrom("GLOBAL_CONFIG_PROPERTY_CHEATS");
        CHECK(mapstrings_transconst(args,1) && arg.vt==VT_INTEGER && arg.lVal==_GLOBAL_CONFIG_CHEATS);
        CHECK(openbor_transconst(args,&out,1)==S_OK && result.lVal==_GLOBAL_CONFIG_CHEATS);
        ScriptVariant_Clear(&arg); ScriptVariant_Clear(&result);
    }
    const char *names[]={"PLAYER_MIN_Z","PLAYER_MAX_Z","FRONTPANEL_Z","player_min_z"};
    for(unsigned i=0;i<4;++i) {
        ScriptVariant arg,result; ScriptVariant_Init(&arg); ScriptVariant_Init(&result);
        arg.vt=VT_STR; arg.strVal=StrCache_CreateNewFrom(names[i]);
        ScriptVariant *args[]={&arg},*out=&result;
        PLAYER_MIN_Z=160; PLAYER_MAX_Z=232;
        CHECK(mapstrings_transconst(args,1) && arg.vt==VT_STR);
        CHECK(openbor_transconst(args,&out,1)==S_OK);
        CHECK(result.lVal==(i==1 ? 232 : i==2 ? 282 : 160));
        PLAYER_MIN_Z=40; PLAYER_MAX_Z=90;
        CHECK(openbor_transconst(args,&out,1)==S_OK && arg.vt==VT_STR);
        CHECK(result.lVal==(i==1 ? 90 : i==2 ? 140 : 40));
        ScriptVariant_Clear(&arg); ScriptVariant_Clear(&result);
    }
    ScriptVariant arg,result; ScriptVariant_Init(&arg); ScriptVariant_Init(&result);
    ScriptVariant *args[]={&arg},*out=&result;
    arg.vt=VT_STR; arg.strVal=StrCache_CreateNewFrom("ANI_IDLE");
    CHECK(mapstrings_transconst(args,1) && arg.vt==VT_INTEGER && arg.lVal==ANI_IDLE);
    CHECK(openbor_transconst(NULL,&out,0)==E_FAIL && out==NULL);
    ScriptVariant_Clear(&arg); StrCache_Clear();
    puts("Legacy constants: compilation, runtime level bounds and static constants passed.");
    return 0;
}
