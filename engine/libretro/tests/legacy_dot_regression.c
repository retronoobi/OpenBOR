#include "../../openbor.c"
#include "../../source/scriptlib/ScriptVariant.c"
#include "../../source/openborscript/legacy_dot.h"
#undef printf
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(0)
void *checkAlloc(void *p,size_t size,const char *fn,const char *file,int line) { if(!p && size) abort(); return p; }
void exitIfFalse(int ok,const char *what,const char *fn,const char *file,int line) { if(!ok) abort(); }
void writeToLogFile(const char *fmt,...) {}
int main(void)
{
    entity ent={0}, owner={0};
    ScriptVariant values[9]={{0}}, result={0}, *args[9], *out=&result;
    const char *names[]={"owner","force","mode","rate","time","type"};
    int numbers[]={0,1,4,80,1600,ATK_NORMAL4};
    int i;
    for(i=0;i<9;++i) args[i]=values+i;
    values[2].vt=VT_INTEGER; values[2].lVal=1;
    for(i=0;i<6;++i) {
        ScriptVariant_Clear(values+3);
        values[3].vt=VT_STR; values[3].strVal=StrCache_CreateNewFrom(names[i]);
        values[4].vt=i==0 ? VT_PTR : VT_INTEGER;
        if(i==0) values[4].ptrVal=&owner; else values[4].lVal=numbers[i];
        CHECK(legacy_dot_access(&ent,args,&out,5,1)==S_OK);
        CHECK(legacy_dot_access(&ent,args,&out,4,0)==S_OK);
        CHECK(i==0 ? result.ptrVal==&owner : result.lVal==numbers[i]);
    }
    CHECK(ent.recursive_damage->index==1 && ent.recursive_damage->mode==DAMAGE_RECURSIVE_MODE_HP);
    /* Positional API uses time, mode, force, rate, type, owner. */
    ScriptVariant_Clear(values+3);
    values[2].lVal=9;
    for(i=3;i<8;++i) { values[i].vt=VT_INTEGER; values[i].lVal=i; }
    values[4].lVal=3; values[8].vt=VT_PTR; values[8].ptrVal=&owner;
    CHECK(legacy_dot_access(&ent,args,&out,9,1)==S_OK);
    CHECK(ent.recursive_damage->index==9 && ent.recursive_damage->next->index==1);
    CHECK(ent.recursive_damage->time==3 && ent.recursive_damage->force==5 && ent.recursive_damage->rate==6);
    CHECK(legacy_dot_mode(ent.recursive_damage->mode)==3 && ent.recursive_damage->owner==&owner);
    for(i=0;i<=5;++i) CHECK(legacy_dot_mode(recursive_damage_get_mode_setup_from_legacy_argument(i))==i);
    values[2].lVal=10;
    CHECK(legacy_dot_access(&ent,args,&out,9,1)==E_FAIL && !out);
    out=&result; values[2].lVal=-1;
    CHECK(legacy_dot_access(&ent,args,&out,4,0)==E_FAIL && !out);
    out=&result; values[2].lVal=5; values[3].lVal=0;
    CHECK(legacy_dot_access(&ent,args,&out,4,0)==S_OK && result.lVal==0);
    CHECK(ent.recursive_damage->index==9); /* Reading must not allocate. */
    free(ent.recursive_damage->next); free(ent.recursive_damage);
    ScriptVariant_Clear(&result); StrCache_Clear();
    puts("PASS: legacy DOT named/positional access, slots, modes and invalid indices.");
    return 0;
}
