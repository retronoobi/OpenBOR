#include "state_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); exit(1); } } while(0)
void *checkAlloc(void *p,size_t n,const char *fn,const char *file,int line) { CHECK(p || !n); return p; }
typedef struct { int a,b; } test_entity;
static test_entity before={1,2},after={3,4};
static bool identify(void *ctx,const void *p,bor_state_ref *r)
{
    if(p!=&before.b) return false;
    *r=(bor_state_ref){1,7,offsetof(test_entity,b)}; return true;
}
static bool resolve(void *ctx,bor_state_ref r,void **p)
{
    if(r.kind!=1 || r.id!=7 || r.offset!=offsetof(test_entity,b)) return false;
    *p=&after.b; return true;
}
int main(void)
{
    uint8_t bytes[512],copy[512],hash[32]={0},other[32]={1};
    ScriptVariant source[7],dest[7];
    for(unsigned i=0;i<7;++i) { ScriptVariant_Init(&source[i]); ScriptVariant_Init(&dest[i]); }
    source[0].vt=VT_INTEGER; source[0].lVal=INT32_MIN+17;
    source[1].vt=VT_DECIMAL; source[1].dblVal=-0.0;
    source[2].vt=VT_STR; source[2].strVal=StrCache_CreateNewFrom("Jaspion / ação / save\n");
    source[3].vt=VT_PTR; source[3].ptrVal=&before.b;
    source[4]=source[3]; /* Alias must be preserved after object relocation. */
    source[5].vt=VT_PTR; source[5].ptrVal=NULL;
    bor_state_objects objects={NULL,identify,resolve};
    memset(bytes,0xAF,sizeof(bytes));
    bor_state_stream w=bor_state_writer(bytes+BOR_STATE_HEADER,sizeof(bytes)-BOR_STATE_HEADER);
    for(unsigned i=0;i<7;++i) CHECK(bor_state_variant_write(&w,&source[i],&objects));
    CHECK(bor_state_seal(bytes,sizeof(bytes),w.position,hash));
    for(size_t i=BOR_STATE_HEADER+w.position;i<sizeof(bytes);++i) CHECK(bytes[i]==0);
    bor_state_stream r;
    CHECK(bor_state_open(bytes,sizeof(bytes),hash,&r));
    for(unsigned i=0;i<7;++i) CHECK(bor_state_variant_read(&r,&dest[i],&objects));
    CHECK(r.position==w.position && dest[0].lVal==source[0].lVal);
    CHECK(!memcmp(&dest[1].dblVal,&source[1].dblVal,8));
    CHECK(dest[2].strVal!=source[2].strVal);
    CHECK(!strcmp(StrCache_Get(dest[2].strVal),StrCache_Get(source[2].strVal)));
    CHECK(dest[3].ptrVal==&after.b && dest[4].ptrVal==dest[3].ptrVal && !dest[5].ptrVal && dest[6].vt==VT_EMPTY);
    CHECK(!bor_state_open(bytes,sizeof(bytes),other,&r));
    for(size_t n=0;n<BOR_STATE_HEADER+w.position;++n) CHECK(!bor_state_open(bytes,n,hash,&r));
    /* Every meaningful byte is checked or has a fixed required value. */
    for(size_t i=0;i<BOR_STATE_HEADER+w.position;++i) {
        memcpy(copy,bytes,sizeof(copy)); copy[i]^=1;
        CHECK(!bor_state_open(copy,sizeof(copy),hash,&r));
    }
    ScriptVariant keep; ScriptVariant_Init(&keep); keep.vt=VT_INTEGER; keep.lVal=123;
    w=bor_state_writer(copy,sizeof(copy)); CHECK(bor_state_variant_write(&w,&source[3],&objects));
    r=bor_state_reader(copy,w.position); CHECK(!bor_state_variant_read(&r,&keep,NULL));
    CHECK(keep.vt==VT_INTEGER && keep.lVal==123);
    w=bor_state_writer(copy,3); CHECK(!bor_state_variant_write(&w,&source[0],&objects));
    CHECK(!w.ok && w.position==0);
    w=bor_state_writer(copy,sizeof(copy)); source[3].ptrVal=&before.a;
    CHECK(!bor_state_variant_write(&w,&source[3],&objects));
    /* Truncated string: no string allocation or destination mutation. */
    w=bor_state_writer(copy,sizeof(copy)); CHECK(bor_state_variant_write(&w,&source[2],NULL));
    r=bor_state_reader(copy,w.position-1); CHECK(!bor_state_variant_read(&r,&keep,NULL));
    CHECK(keep.lVal==123);
    w=bor_state_writer(copy,sizeof(copy));
    bor_state_put32(&w,VT_INTEGER); bor_state_put64(&w,UINT64_C(0x100000000));
    r=bor_state_reader(copy,w.position); CHECK(!bor_state_variant_read(&r,&keep,NULL));
    CHECK(keep.vt==VT_INTEGER && keep.lVal==123);
    for(unsigned i=0;i<7;++i) { ScriptVariant_Clear(&source[i]); ScriptVariant_Clear(&dest[i]); }
    StrCache_Clear();
    puts("State codec: identities, aliases, script values, CRC, bounds and rejection passed.");
    return 0;
}
