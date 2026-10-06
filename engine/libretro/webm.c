/* WebM playback driven by retro_run, without decoder threads or wall-clock sleeps.
 * OpenBOR license: ../../LICENSE. VP8 and Vorbis are decoded by libvpx/libvorbis. */
#include "openbor.h"
#include "globals.h"
#include "webm.h"
#include "nestegg/nestegg.h"
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#include <vorbis/codec.h>
#include <limits.h>
#include <math.h>

#define VIDEO_QUEUE 32
#define AUDIO_QUEUE 384000
typedef struct {
    uint32_t *pixels;
    unsigned w, h;
    uint64_t us;
} movie_frame;
typedef struct {
    int handle, video, audio, eof, vpx_ready, vorbis_ready, vorbis_headers;
    nestegg *demux;
    nestegg_packet *pending;
    vpx_codec_ctx_t decoder;
    vorbis_info vi;
    vorbis_comment vc;
    vorbis_dsp_state vd;
    vorbis_block vb;
    movie_frame frames[VIDEO_QUEUE];
    unsigned first, count;
    uint64_t frame_delay, video_end, decoded_frames, start_us;
    float aspect, volume;
    float *pcm;
    uint64_t written, consumed, audio_start_us, output_frames;
    int audio_started;
    double position;
} movie;
static movie *active;
extern unsigned long long bothnewkeys;

static int movie_read(void *dst, size_t bytes, void *user)
{
    movie *m=user;
    if(bytes>INT_MAX) return -1;
    int n=readpackfile(m->handle,dst,(int)bytes);
    return n==(int)bytes ? 1 : n==0 ? 0 : -1;
}
static int movie_seek(int64_t offset,int whence,void *user)
{
    movie *m=user;
    if(offset<INT_MIN || offset>INT_MAX) return -1;
    return seekpackfile(m->handle,(int)offset,whence)<0 ? -1 : 0;
}
static int64_t movie_tell(void *user) { return seekpackfile(((movie*)user)->handle,0,SEEK_CUR); }

void libretro_webm_close(void)
{
    movie *m=active;
    if(!m) return;
    active=NULL;
    if(m->pending) nestegg_free_packet(m->pending);
    for(unsigned i=0;i<VIDEO_QUEUE;++i) free(m->frames[i].pixels);
    if(m->vpx_ready) vpx_codec_destroy(&m->decoder);
    if(m->vorbis_ready) { vorbis_block_clear(&m->vb); vorbis_dsp_clear(&m->vd); }
    if(m->vorbis_headers) { vorbis_comment_clear(&m->vc); vorbis_info_clear(&m->vi); }
    if(m->demux) nestegg_destroy(m->demux);
    if(m->handle>=0) closepackfile(m->handle);
    free(m->pcm);
    free(m);
}

static unsigned byte_clamp(int n) { return n<0 ? 0 : n>255 ? 255 : n; }
static int queue_image(movie *m,const vpx_image_t *img,uint64_t us)
{
    if(m->count==VIDEO_QUEUE || !img->d_w || !img->d_h || img->d_w>1920 || img->d_h>1080 ||
       img->fmt!=VPX_IMG_FMT_I420) return 0;
    movie_frame *f=&m->frames[(m->first+m->count)%VIDEO_QUEUE];
    f->pixels=malloc((size_t)img->d_w*img->d_h*sizeof(uint32_t));
    if(!f->pixels) return 0;
    f->w=img->d_w; f->h=img->d_h; f->us=us;
    /* VP8 uses limited-range BT.601 YUV420. Respect each decoder plane's stride. */
    for(unsigned y=0;y<f->h;++y) for(unsigned x=0;x<f->w;++x) {
        int Y=img->planes[0][y*img->stride[0]+x]-16;
        int U=img->planes[1][(y/2)*img->stride[1]+x/2]-128;
        int V=img->planes[2][(y/2)*img->stride[2]+x/2]-128;
        unsigned r=byte_clamp((298*Y+409*V+128)>>8);
        unsigned g=byte_clamp((298*Y-100*U-208*V+128)>>8);
        unsigned b=byte_clamp((298*Y+516*U+128)>>8);
        f->pixels[(size_t)y*f->w+x]=(r<<16)|(g<<8)|b;
    }
    ++m->count; ++m->decoded_frames;
    m->video_end=us+m->frame_delay;
    return 1;
}

static int decode_packet(movie *m,nestegg_packet *packet)
{
    unsigned track,chunks;
    uint64_t ns;
    if(nestegg_packet_track(packet,&track)<0 || nestegg_packet_count(packet,&chunks)<0 ||
       nestegg_packet_tstamp(packet,&ns)<0) return 0;
    for(unsigned c=0;c<chunks;++c) {
        unsigned char *data; size_t size;
        if(nestegg_packet_data(packet,c,&data,&size)<0 || size>INT_MAX) return 0;
        if((int)track==m->video) {
            if(vpx_codec_decode(&m->decoder,data,(unsigned)size,NULL,0)) return 0;
            vpx_codec_iter_t iter=NULL; vpx_image_t *img;
            uint64_t us=ns/1000+c*m->frame_delay;
            while((img=vpx_codec_get_frame(&m->decoder,&iter))) {
                if(!queue_image(m,img,us)) return 0;
                us+=m->frame_delay;
            }
        } else if((int)track==m->audio) {
            if(!m->audio_started) { m->audio_start_us=ns/1000; m->audio_started=1; }
            ogg_packet op={0}; op.packet=data; op.bytes=(long)size;
            if(vorbis_synthesis(&m->vb,&op) || vorbis_synthesis_blockin(&m->vd,&m->vb)) return 0;
            float **pcm; int count=vorbis_synthesis_pcmout(&m->vd,&pcm);
            if(count<0 || (uint64_t)count>AUDIO_QUEUE-(m->written-m->consumed)) return 0;
            for(int i=0;i<count;++i) {
                size_t slot=(size_t)(m->written++%AUDIO_QUEUE)*2;
                m->pcm[slot]=pcm[0][i];
                m->pcm[slot+1]=pcm[m->vi.channels==1 ? 0 : 1][i];
            }
            vorbis_synthesis_read(&m->vd,count);
        }
    }
    return 1;
}

/* Decode ahead in media time. Queue limits bound memory even for damaged files. */
static int pump(movie *m,uint64_t until)
{
    unsigned budget=0;
    while(!m->eof && m->count<VIDEO_QUEUE-2 && m->written-m->consumed<AUDIO_QUEUE-16384) {
        if(++budget>4096) return 0;
        if(!m->pending) {
            int r=nestegg_read_packet(m->demux,&m->pending);
            if(r<0) return 0;
            if(!r) { m->eof=1; break; }
        }
        uint64_t ns;
        if(nestegg_packet_tstamp(m->pending,&ns)<0) return 0;
        if(ns/1000>until) break;
        int ok=decode_packet(m,m->pending);
        nestegg_free_packet(m->pending); m->pending=NULL;
        if(!ok) return 0;
    }
    return 1;
}

int libretro_webm_audio(int16_t *out,size_t frames,unsigned rate)
{
    movie *m=active;
    if(!m || m->audio<0) return 0;
    for(size_t i=0;i<frames;++i,++m->output_frames) {
        double position=((double)m->output_frames/rate-(double)m->audio_start_us/1000000.0)*m->vi.rate;
        m->position=position;
        for(unsigned ch=0;ch<2;++ch) {
            double value=0;
            if(m->audio_started && position>=0 && position<(double)m->written) {
                uint64_t a=(uint64_t)position, b=a+1<m->written ? a+1 : a;
                if(a>=m->consumed) {
                    double fraction=position-a;
                    value=(m->pcm[(a%AUDIO_QUEUE)*2+ch]*(1-fraction)+m->pcm[(b%AUDIO_QUEUE)*2+ch]*fraction)*32768.0*m->volume;
                }
            }
            out[i*2+ch]=(int16_t)(value<-32768 ? -32768 : value>32767 ? 32767 : value);
        }
        if(position>=0) {
            uint64_t consumed=(uint64_t)position;
            if(consumed>m->written) consumed=m->written;
            if(consumed>m->consumed) m->consumed=consumed;
        }
    }
    return 1;
}

static int open_movie(movie *m,const char *path)
{
    nestegg_io io={movie_read,movie_seek,movie_tell,m};
    m->handle=openpackfile(path,packfile);
    if(m->handle<0 || nestegg_init(&m->demux,io,NULL,-1)<0) return 0;
    unsigned tracks;
    if(nestegg_track_count(m->demux,&tracks)<0) return 0;
    for(unsigned i=0;i<tracks;++i) {
        int type=nestegg_track_type(m->demux,i);
        if(type==NESTEGG_TRACK_VIDEO && m->video<0) m->video=i;
        if(type==NESTEGG_TRACK_AUDIO && m->audio<0) m->audio=i;
    }
    if(m->video<0) return 0;
    nestegg_video_params vp;
    if(nestegg_track_video_params(m->demux,m->video,&vp)<0 || vp.stereo_mode!=NESTEGG_VIDEO_MONO ||
       !vp.width || !vp.height || vp.width>1920 || vp.height>1080) return 0;
    int codec=nestegg_track_codec_id(m->demux,m->video);
    /* Match the original OpenBOR 4.0 WebM format: VP8 + optional Vorbis. */
    if(codec!=NESTEGG_CODEC_VP8) return 0;
    vpx_codec_dec_cfg_t cfg={0}; cfg.threads=1;
    if(vpx_codec_dec_init(&m->decoder,vpx_codec_vp8_dx(),&cfg,0)) return 0;
    m->vpx_ready=1;
    uint64_t duration=0;
    nestegg_track_default_duration(m->demux,m->video,&duration);
    m->frame_delay=duration ? duration/1000 : 33333;
    if(!m->frame_delay) m->frame_delay=1;
    m->aspect=(float)(vp.display_width ? vp.display_width : vp.width)/(vp.display_height ? vp.display_height : vp.height);
    if(m->audio>=0) {
        if(nestegg_track_codec_id(m->demux,m->audio)!=NESTEGG_CODEC_VORBIS) return 0;
        vorbis_info_init(&m->vi); vorbis_comment_init(&m->vc); m->vorbis_headers=1;
        unsigned chunks;
        if(nestegg_track_codec_data_count(m->demux,m->audio,&chunks)<0 || chunks!=3) return 0;
        for(unsigned i=0;i<3;++i) {
            unsigned char *data; size_t size;
            if(nestegg_track_codec_data(m->demux,m->audio,i,&data,&size)<0 || size>LONG_MAX) return 0;
            ogg_packet op={0}; op.packet=data; op.bytes=(long)size; op.b_o_s=i==0; op.packetno=i;
            if(vorbis_synthesis_headerin(&m->vi,&m->vc,&op)) return 0;
        }
        if(m->vi.channels<1 || m->vi.channels>2 || m->vi.rate<8000 || m->vi.rate>96000) return 0;
        if(vorbis_synthesis_init(&m->vd,&m->vi)) return 0;
        if(vorbis_block_init(&m->vd,&m->vb)) { vorbis_dsp_clear(&m->vd); return 0; }
        m->vorbis_ready=1;
        m->pcm=calloc(AUDIO_QUEUE*2,sizeof(float));
        if(!m->pcm) return 0;
    }
    printf("[WebM] %s: %ux%u VP8, audio=%ld Hz/%d channels\n",path,vp.width,vp.height,m->audio>=0 ? m->vi.rate : 0,m->audio>=0 ? m->vi.channels : 0);
    return 1;
}

int playwebm(const char *path,int noskip)
{
    if(active) return 0;
    movie *m=calloc(1,sizeof(*m));
    if(!m) return 0;
    active=m; m->handle=m->video=m->audio=-1;
    int result=0;
    if(!open_movie(m,path)) goto done;
    /* Cinematic audio is already mastered: 100% is unity, without mixer boost. */
    m->volume=(float)savedata.musicvol/100.0f;
    if(m->volume<0) m->volume=0;
    if(m->volume>1) m->volume=1;
    if(m->audio>=0) sound_close_music();
    m->start_us=timer_uticks();
    result=1;
    while(1) {
        inputrefresh(playrecstatus->status);
        if(!noskip && (bothnewkeys&(FLAG_ESC|FLAG_ANYBUTTON))) { result=-1; break; }
        uint64_t elapsed=timer_uticks()-m->start_us;
        if(!pump(m,elapsed+150000)) { result=0; break; }
        while(m->count && m->frames[m->first].us<=elapsed) {
            movie_frame *f=&m->frames[m->first];
            libretro_webm_present(f->pixels,f->w,f->h,m->aspect);
            free(f->pixels); f->pixels=NULL;
            m->first=(m->first+1)%VIDEO_QUEUE; --m->count;
        }
        if(m->eof && !m->count && elapsed>=m->video_end &&
           (m->audio<0 || !m->written || m->position>=(double)m->written)) break;
        if(m->audio<0) sound_update_music();
        libretro_webm_yield();
    }
done:
    printf("[WebM] result=%d frames=%llu: %s\n",result,(unsigned long long)m->decoded_frames,path);
    libretro_webm_close();
    libretro_webm_restore_video();
    return result;
}
