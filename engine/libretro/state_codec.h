#ifndef OPENBOR_STATE_CODEC_H
#define OPENBOR_STATE_CODEC_H
/* Native state building blocks. No memory addresses or C struct padding on disk.
 * These do not constitute a complete game state and must not be exposed through
 * retro_serialize until all engine sections and restore transactions are ready. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ScriptVariant.h"
#define BOR_STATE_HEADER 64u
#define BOR_STATE_SCHEMA 1u
#define BOR_STATE_MAX_STRING (1024u * 1024u)
typedef struct {
    uint8_t *output;
    const uint8_t *input;
    size_t size, position;
    bool ok;
} bor_state_stream;
typedef struct { uint32_t kind, id, offset; } bor_state_ref;
typedef struct {
    void *context;
    bool (*identify)(void *, const void *, bor_state_ref *);
    bool (*resolve)(void *, bor_state_ref, void **);
} bor_state_objects;
bor_state_stream bor_state_writer(void *, size_t);
bor_state_stream bor_state_reader(const void *, size_t);
void bor_state_put32(bor_state_stream *, uint32_t);
void bor_state_put64(bor_state_stream *, uint64_t);
uint32_t bor_state_get32(bor_state_stream *);
uint64_t bor_state_get64(bor_state_stream *);
bool bor_state_seal(void *, size_t, size_t, const uint8_t pak_sha256[32]);
bool bor_state_open(const void *, size_t, const uint8_t pak_sha256[32], bor_state_stream *);
bool bor_state_variant_write(bor_state_stream *, const ScriptVariant *, const bor_state_objects *);
bool bor_state_variant_read(bor_state_stream *, ScriptVariant *, const bor_state_objects *);
#endif
