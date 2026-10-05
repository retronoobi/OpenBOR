/* Exercise the real mixer, including its 8-bit unsigned PCM conversion. */
#include "../../source/gamelib/soundmix.c"
#undef printf

int main(void)
{
    unsigned char source[32];
    int16_t output[64];
    s_soundcache cache = {0};
    s32 accumulation[64];
    const int volumes[] = {0, 32, 64, 96};
    const int values[] = {0, 64, 128, 192, 255};
    soundcache = &cache;
    cache.sample.sampleptr = source;
    cache.sample.soundlen = sizeof(source);
    cache.sample.bits = 8;
    mixbuf = accumulation;
    max_channels = 1;
    playbits = 16;
    for(unsigned v=0;v<4;++v) for(unsigned s=0;s<5;++s)
    {
        memset(source,values[s],sizeof(source));
        memset(vchannel,0,sizeof(vchannel));
        vchannel[0].active = CHANNEL_LOOPING;
        vchannel[0].channels = SOUND_MONO;
        vchannel[0].fp_period = INT_TO_FIX(1);
        vchannel[0].volume[0] = vchannel[0].volume[1] = volumes[v];
        update_sample((unsigned char *)output,sizeof(output));
        int expected = (int)((values[s]-128)*256*volumes[v]/MAXVOLUME*1.5) >> MIXSHIFT;
        for(unsigned i=0;i<64;++i) if(output[i] != expected) {
            fprintf(stderr,"PCM=%d volume=%d got=%d expected=%d\n",values[s],volumes[v],output[i],expected);
            return 1;
        }
    }
    puts("PASS: 8-bit silence, mute, polarity and gain (20 cases).\n");
    return 0;
}
