/* OpenBOR native state format, see ../../LICENSE. */
#include "state_codec.h"
#include <limits.h>
#include <string.h>
#include <zlib.h>

bor_state_stream bor_state_writer(void *data,size_t size)
{ return (bor_state_stream){data,NULL,size,0,data!=NULL}; }
bor_state_stream bor_state_reader(const void *data,size_t size)
{ return (bor_state_stream){NULL,data,size,0,data!=NULL}; }
static bool room(bor_state_stream *s,size_t n)
{
    if(!s->ok || s->position>s->size || n>s->size-s->position) { s->ok=false; return false; }
    return true;
}
static void put(bor_state_stream *s,const void *data,size_t n)
{
    if(!s->output || !room(s,n)) { s->ok=false; return; }
    memcpy(s->output+s->position,data,n); s->position+=n;
}
void bor_state_put32(bor_state_stream *s,uint32_t n)
{ uint8_t b[4]; for(unsigned i=0;i<4;++i) b[i]=(uint8_t)(n>>(8*i)); put(s,b,4); }
void bor_state_put64(bor_state_stream *s,uint64_t n)
{ uint8_t b[8]; for(unsigned i=0;i<8;++i) b[i]=(uint8_t)(n>>(8*i)); put(s,b,8); }
static uint64_t get(bor_state_stream *s,unsigned n)
{
    uint64_t value=0;
    if(!s->input || !room(s,n)) { s->ok=false; return 0; }
    for(unsigned i=0;i<n;++i) value|=(uint64_t)s->input[s->position++]<<(8*i);
    return value;
}
uint32_t bor_state_get32(bor_state_stream *s) { return (uint32_t)get(s,4); }
uint64_t bor_state_get64(bor_state_stream *s) { return get(s,8); }

/* Header: magic[8], schema, engine, payload bytes[8], PAK SHA256[32],
 * CRC32(header bytes 0..55 + payload), reserved[4]. Padding is canonical zero. */
bool bor_state_seal(void *data,size_t capacity,size_t payload,const uint8_t hash[32])
{
    if(!data || !hash || capacity<BOR_STATE_HEADER || payload>capacity-BOR_STATE_HEADER || payload>UINT_MAX) return false;
    bor_state_stream s=bor_state_writer(data,BOR_STATE_HEADER);
    put(&s,"BORSTATE",8); bor_state_put32(&s,BOR_STATE_SCHEMA); bor_state_put32(&s,7533);
    bor_state_put64(&s,payload); put(&s,hash,32);
    uint32_t crc=(uint32_t)crc32(0,data,56);
    crc=(uint32_t)crc32(crc,(uint8_t*)data+BOR_STATE_HEADER,(uInt)payload);
    bor_state_put32(&s,crc); bor_state_put32(&s,0);
    memset((uint8_t*)data+BOR_STATE_HEADER+payload,0,capacity-BOR_STATE_HEADER-payload);
    return s.ok;
}
bool bor_state_open(const void *data,size_t size,const uint8_t hash[32],bor_state_stream *payload)
{
    if(!data || !hash || !payload || size<BOR_STATE_HEADER) return false;
    const uint8_t *bytes=data;
    if(memcmp(bytes,"BORSTATE",8)) return false;
    bor_state_stream s=bor_state_reader(data,BOR_STATE_HEADER); s.position=8;
    if(bor_state_get32(&s)!=BOR_STATE_SCHEMA || bor_state_get32(&s)!=7533) return false;
    uint64_t length=bor_state_get64(&s);
    if(length>size-BOR_STATE_HEADER || length>UINT_MAX || memcmp(bytes+24,hash,32)) return false;
    s.position=56;
    uint32_t expected=bor_state_get32(&s);
    if(bor_state_get32(&s)!=0) return false;
    uint32_t crc=(uint32_t)crc32(0,data,56);
    crc=(uint32_t)crc32(crc,bytes+BOR_STATE_HEADER,(uInt)length);
    if(crc!=expected) return false;
    *payload=bor_state_reader(bytes+BOR_STATE_HEADER,(size_t)length);
    return true;
}
bool bor_state_variant_write(bor_state_stream *s,const ScriptVariant *v,const bor_state_objects *objects)
{
    if(!s || !v || !s->ok) return false;
    bor_state_put32(s,v->vt);
    switch(v->vt) {
    case VT_EMPTY: break;
    case VT_INTEGER: bor_state_put64(s,(uint64_t)v->lVal); break;
    case VT_DECIMAL: {
        uint64_t bits; _Static_assert(sizeof(v->dblVal)==sizeof(bits),"state needs binary64");
        memcpy(&bits,&v->dblVal,sizeof(bits)); bor_state_put64(s,bits); break;
    }
    case VT_STR: {
        const char *text=StrCache_Get(v->strVal);
        if(!text) { s->ok=false; break; }
        size_t size=strlen(text);
        if(size>BOR_STATE_MAX_STRING) { s->ok=false; break; }
        bor_state_put32(s,(uint32_t)size); put(s,text,size); break;
    }
    case VT_PTR: {
        bor_state_ref ref={0};
        if(v->ptrVal && (!objects || !objects->identify ||
           !objects->identify(objects->context,v->ptrVal,&ref) || !ref.kind || !ref.id)) { s->ok=false; break; }
        bor_state_put32(s,ref.kind); bor_state_put32(s,ref.id); bor_state_put32(s,ref.offset); break;
    }
    default: s->ok=false; break;
    }
    return s->ok;
}
bool bor_state_variant_read(bor_state_stream *s,ScriptVariant *destination,const bor_state_objects *objects)
{
    if(!s || !destination || !s->ok) return false;
    ScriptVariant value; ScriptVariant_Init(&value);
    value.vt=bor_state_get32(s);
    const char *string=NULL; uint32_t string_length=0;
    switch(value.vt) {
    case VT_EMPTY: break;
    case VT_INTEGER: {
        uint64_t bits=bor_state_get64(s);
        int64_t number; memcpy(&number,&bits,8);
        _Static_assert(sizeof(value.lVal)==4 || sizeof(value.lVal)==8,"unsupported script integer");
        if(sizeof(value.lVal)==4 && (number<INT32_MIN || number>INT32_MAX)) s->ok=false;
        else value.lVal=(LONG)number;
        break;
    }
    case VT_DECIMAL: {
        uint64_t bits=bor_state_get64(s); memcpy(&value.dblVal,&bits,8); break;
    }
    case VT_STR:
        string_length=bor_state_get32(s);
        if(string_length>BOR_STATE_MAX_STRING || !room(s,string_length)) { s->ok=false; break; }
        string=(const char*)s->input+s->position;
        if(memchr(string,0,string_length)) { s->ok=false; break; }
        s->position+=string_length; break;
    case VT_PTR: {
        bor_state_ref ref;
        ref.kind=bor_state_get32(s); ref.id=bor_state_get32(s); ref.offset=bor_state_get32(s);
        value.ptrVal=NULL;
        if(ref.kind || ref.id || ref.offset) {
            if(!s->ok || !ref.kind || !ref.id || !objects || !objects->resolve ||
               !objects->resolve(objects->context,ref,&value.ptrVal) || !value.ptrVal) s->ok=false;
        }
        break;
    }
    default: s->ok=false; break;
    }
    if(!s->ok) return false; /* Invalid input must not change a live variable. */
    if(string) {
        value.strVal=StrCache_Pop((int)string_length);
        char *text=StrCache_Get(value.strVal);
        memcpy(text,string,string_length); text[string_length]=0;
    }
    ScriptVariant_Clear(destination);
    *destination=value;
    return true;
}
