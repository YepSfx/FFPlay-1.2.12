#include "config.h"

#include "libavutil/avstring.h"
#include "libavutil/colorspace.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

//#undef  CONFIG_AVFILTER
#if CONFIG_AVFILTER
# include "libavfilter/avcodec.h"
# include "libavfilter/avfilter.h"
# include "libavfilter/avfiltergraph.h"
# include "libavfilter/buffersink.h"
# include "libavfilter/buffersrc.h"
#endif

#include <SDL.h>
#include <SDL_thread.h>

#include <assert.h>

#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "cmdutils.h"

#include "FFPlayLib.h"

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 5

#define PIXELBYTE32     4
#define PIXELBYTE24     3

/* SDL audio buffer size, in samples. Should be small to have precise
   A/V sync as SDL does not have hardware buffer fullness info. */
#define SDL_AUDIO_BUFFER_SIZE 1024

/* no AV sync correction is done if below the AV sync threshold */
#define AV_SYNC_THRESHOLD 0.01
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* external clock speed adjustment constants for realtime sources based on buffer fullness */
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

/* polls for possible required screen refresh at least this often, should be less than 1/fps */
#define REFRESH_RATE 0.01
#define MSG_REFRESH  10000      
/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
/* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000

#if 1
typedef struct MyAVPacketList {
    AVPacket pkt;
    struct MyAVPacketList *next;
    int serial;
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList *first_pkt, *last_pkt;
    int       nb_packets;
    int       size;
    int       abort_request;
    int       serial;
    SDL_mutex *mutex;
    SDL_cond  *cond;
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 4
#define SUBPICTURE_QUEUE_SIZE 4

typedef struct VideoPicture 
{
    double      pts;             // presentation timestamp for this picture
    int64_t     pos;            // byte position in file
    SDL_Overlay *bmp;
    int width,  height; /* source height & width */
    AVRational  sample_aspect_ratio;
    int         allocated;
    int         reallocate;
    int         serial;

#if CONFIG_AVFILTER
    AVFilterBufferRef *picref;
#endif
} VideoPicture;

typedef struct SubPicture {
    double pts; /* presentation time stamp for this picture */
    AVSubtitle sub;
} SubPicture;

typedef struct AudioParams {
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
} AudioParams;

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

typedef struct VideoState {
    SDL_Thread    *read_tid;
    SDL_Thread    *video_tid;
    AVInputFormat *iformat;
    int           no_background;
    int           abort_request;
    int           force_refresh;
    int           paused;
    int           last_paused;
    int           queue_attachments_req;
    int           seek_req;
    int           seek_flags;
    int64_t       seek_pos;
    int64_t       seek_rel;
    int           read_pause_return;
    AVFormatContext *ic;
    int           realtime;

    int         audio_stream;

    int         av_sync_type;
    double      external_clock;                   ///< external clock base
    double      external_clock_drift;             ///< external clock base - time (av_gettime) at which we updated external_clock
    int64_t     external_clock_time;             ///< last reference time
    double      external_clock_speed;             ///< speed of the external clock

    double      audio_clock;
    int         audio_clock_serial;
    double      audio_diff_cum; /* used for AV difference average computation */
    double      audio_diff_avg_coef;
    double      audio_diff_threshold;
    int         audio_diff_avg_count;
    AVStream    *audio_st;
    PacketQueue audioq;
    int         audio_hw_buf_size;
    uint8_t     silence_buf[SDL_AUDIO_BUFFER_SIZE];
    uint8_t     *audio_buf;
    uint8_t     *audio_buf1;
    unsigned int  audio_buf_size; /* in bytes */
    unsigned int  audio_buf1_size;
    int           audio_buf_index; /* in bytes */
    int         audio_write_buf_size;
    AVPacket    audio_pkt_temp;
    AVPacket    audio_pkt;
    int         audio_pkt_temp_serial;
    struct      AudioParams audio_src;
    struct      AudioParams audio_tgt;
    struct      SwrContext *swr_ctx;
    double      audio_current_pts;
    double      audio_current_pts_drift;
    int         frame_drops_early;
    int         frame_drops_late;
    AVFrame     *frame;

    enum ShowMode {
        SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
    } show_mode;
    int16_t     sample_array[SAMPLE_ARRAY_SIZE];
    int         sample_array_index;
    int         last_i_start;
    RDFTContext *rdft;
    int         rdft_bits;
    FFTSample   *rdft_data;
    int         xpos;
    double      last_vis_time;

    SDL_Thread  *subtitle_tid;
    int         subtitle_stream;
    int         subtitle_stream_changed;
    AVStream    *subtitle_st;
    PacketQueue subtitleq;
    SubPicture  subpq[SUBPICTURE_QUEUE_SIZE];
    int         subpq_size, subpq_rindex, subpq_windex;
    SDL_mutex   *subpq_mutex;
    SDL_cond    *subpq_cond;

    double        frame_timer;
    double        frame_last_pts;
    double        frame_last_duration;
    double        frame_last_dropped_pts;
    double        frame_last_returned_time;
    double        frame_last_filter_delay;
    int64_t       frame_last_dropped_pos;
    int           video_stream;
    AVStream      *video_st;
    PacketQueue   videoq;
    double        video_current_pts;       // current displayed pts
    double        video_current_pts_drift; // video_current_pts - time (av_gettime) at which we updated video_current_pts - used to have running video pts
    int64_t       video_current_pos;      // current displayed file pos
    double        max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
    int           video_clock_serial;
    VideoPicture  pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int           pictq_size, pictq_rindex, pictq_windex;
    SDL_mutex     *pictq_mutex;
    SDL_cond      *pictq_cond;
#if !CONFIG_AVFILTER
    struct SwsContext *img_convert_ctx;
#endif
    SDL_Rect      last_display_rect;

    char          filename[1024];
    int           width, height, xleft, ytop;
    int           step;

#if CONFIG_AVFILTER
    AVFilterContext *in_video_filter;   // the first filter in the video chain
    AVFilterContext *out_video_filter;  // the last filter in the video chain
    int             use_dr1;
    FrameBuffer     *buffer_pool;
#endif

    int             last_video_stream, last_audio_stream, last_subtitle_stream;

    SDL_cond        *continue_read_thread;
} VideoState;
#endif

static int64_t sws_flags = SWS_BICUBIC;

const char program_name[] = "FFPLAY";
const int  program_birth_year = 2017;

/* options specified by the user */
static AVInputFormat *file_iformat;
static const char *input_filename;
static const char *window_title;
static int fs_screen_width;
static int fs_screen_height;
static int default_width  = 640;
static int default_height = 480;
static int screen_width  = 0;
static int screen_height = 0;
static int audio_disable;
static int video_disable;
static int subtitle_disable;
static int wanted_stream[AVMEDIA_TYPE_NB] = {
    [AVMEDIA_TYPE_AUDIO]    = -1,
    [AVMEDIA_TYPE_VIDEO]    = -1,
    [AVMEDIA_TYPE_SUBTITLE] = -1,
};
static int seek_by_bytes = -1;
static int display_disable;
static int show_status = 1;
static int av_sync_type = AV_SYNC_AUDIO_MASTER;
static int64_t start_time = AV_NOPTS_VALUE;
static int64_t duration = AV_NOPTS_VALUE;
static int workaround_bugs = 1;
static int fast = 0;
static int genpts = 0;
static int lowres = 0;
static int idct = FF_IDCT_AUTO;
static enum AVDiscard skip_frame       = AVDISCARD_DEFAULT;
static enum AVDiscard skip_idct        = AVDISCARD_DEFAULT;
static enum AVDiscard skip_loop_filter = AVDISCARD_DEFAULT;
static int error_concealment = 3;
static int decoder_reorder_pts = -1;
static int autoexit = 1;
static int exit_on_keydown;
static int exit_on_mousedown;
static int loop = 1;
static int framedrop = -1;
static int infinite_buffer = -1;
static enum ShowMode show_mode = SHOW_MODE_NONE;
static const char *audio_codec_name;
static const char *subtitle_codec_name;
static const char *video_codec_name;
static double rdftspeed = 0.02;
static int64_t cursor_last_shown;
static int cursor_hidden = 0;
#if CONFIG_AVFILTER
static char *vfilters = NULL;
#endif

/* current context */
static int is_full_screen;
static int64_t audio_callback_time;
static int isHalt = 0;
static int isFileOpen = 0;
static AVPacket flush_pkt;

#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)

static SDL_Surface *screen;

static FFP_EVENTS  *FFP_events = NULL;
static FFP_AUD_PARAMS ffp_aud_params;
static FFP_VID_PARAMS ffp_vid_params;
static FFP_VID_PARAMS ffp_vidstr_params;

static void __ClearVideoState();

static void __sendInfoMessage(int infoCode, char *Message)
{
    if (FFP_events->event_info)
        FFP_events->event_info(FFP_events->sender, infoCode, Message);
}

static void __exit(int exitCode)
{
    __ClearVideoState();
    if (FFP_events->event_exit)
        FFP_events->event_exit(FFP_events->sender, exitCode);
    else
        exit(exitCode);
}

static long int       crv_tab[256]    = {0,};   
static long int       cbu_tab[256]    = {0,};   
static long int       cgu_tab[256]    = {0,};   
static long int       cgv_tab[256]    = {0,};   
static long int       tab_76309[256]  = {0,}; 
static unsigned char  clp[1024]       = {0,};   

static int init_yuv420p = 0;

static void init_yuv420p_table() 
{   
    long int crv,cbu,cgu,cgv;   
    int i,ind;      
   
    memset(crv_tab, 0 ,sizeof(crv_tab));
    memset(cbu_tab, 0 ,sizeof(cbu_tab));
    memset(cgu_tab, 0 ,sizeof(cgu_tab));
    memset(cgv_tab, 0 ,sizeof(cgv_tab));
    memset(tab_76309, 0 ,sizeof(tab_76309));
    memset(clp, 0 ,sizeof(clp));

    init_yuv420p = 0;
                
    crv = 104597; cbu = 132201;  /* fra matrise i global.h */   
    cgu = 25675;  cgv = 53279;   
   
    for (i = 0; i < 256; i++)    
    {   
        crv_tab[i] = (i-128) * crv;   
        cbu_tab[i] = (i-128) * cbu;   
        cgu_tab[i] = (i-128) * cgu;   
        cgv_tab[i] = (i-128) * cgv;   
        tab_76309[i] = 76309*(i-16);   
    }   
   
    for (i = 0; i < 384; i++)   
        clp[i] = 0;   
    ind = 384;   
    for (i = 0;i < 256; i++)   
        clp[ind++] = i;   
    ind = 640;   
    for (i = 0;i < 384; i++)   
        clp[ind++] = 255;   
        
    init_yuv420p = 1;
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt);

static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
    MyAVPacketList *pkt1;

    if (q->abort_request)
       return -1;

    pkt1 = av_malloc(sizeof(MyAVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    if (pkt == &flush_pkt)
        q->serial++;
    pkt1->serial = q->serial;

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    /* XXX: should duplicate packet data in DV case */
    SDL_CondSignal(q->cond);
    return 0;
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    int ret;

    /* duplicate the packet */
    if (pkt != &flush_pkt && av_dup_packet(pkt) < 0)
        return -1;

    SDL_LockMutex(q->mutex);
    ret = packet_queue_put_private(q, pkt);
    SDL_UnlockMutex(q->mutex);

    if (pkt != &flush_pkt && ret < 0)
        av_free_packet(pkt);

    return ret;
}

/* packet queue handling */
static void packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
    q->abort_request = 1;
}

static void packet_queue_flush(PacketQueue *q)
{
    MyAVPacketList *pkt, *pkt1;

    SDL_LockMutex(q->mutex);
    for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    SDL_UnlockMutex(q->mutex);
}

static void packet_queue_destroy(PacketQueue *q)
{
    packet_queue_flush(q);
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

static void packet_queue_abort(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request = 1;

    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
}

static void packet_queue_start(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);
    q->abort_request = 0;
    packet_queue_put_private(q, &flush_pkt);
    SDL_UnlockMutex(q->mutex);
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
{
    MyAVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            *pkt = pkt1->pkt;
            if (serial)
                *serial = pkt1->serial;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

static inline void fill_rectangle(SDL_Surface *screen,
                                  int x, int y, int w, int h, int color, int update)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_FillRect(screen, &rect, color);
    if (update && w > 0 && h > 0)
        SDL_UpdateRect(screen, x, y, w, h);
}

/* draw only the border of a rectangle */
static void fill_border(int xleft, int ytop, int width, int height, int x, int y, int w, int h, int color, int update)
{
    int w1, w2, h1, h2;

    /* fill the background */
    w1 = x;
    if (w1 < 0)
        w1 = 0;
    w2 = width - (x + w);
    if (w2 < 0)
        w2 = 0;
    h1 = y;
    if (h1 < 0)
        h1 = 0;
    h2 = height - (y + h);
    if (h2 < 0)
        h2 = 0;
    fill_rectangle(screen,
                   xleft, ytop,
                   w1, height,
                   color, update);
    fill_rectangle(screen,
                   xleft + width - w2, ytop,
                   w2, height,
                   color, update);
    fill_rectangle(screen,
                   xleft + w1, ytop,
                   width - w1 - w2, h1,
                   color, update);
    fill_rectangle(screen,
                   xleft + w1, ytop + height - h2,
                   width - w1 - w2, h2,
                   color, update);
}

#define ALPHA_BLEND(a, oldp, newp, s)\
((((oldp << s) * (255 - (a))) + (newp * (a))) / (255 << s))

#define RGBA_IN(r, g, b, a, s)\
{\
    unsigned int v = ((const uint32_t *)(s))[0];\
    a = (v >> 24) & 0xff;\
    r = (v >> 16) & 0xff;\
    g = (v >> 8) & 0xff;\
    b = v & 0xff;\
}

#define YUVA_IN(y, u, v, a, s, pal)\
{\
    unsigned int val = ((const uint32_t *)(pal))[*(const uint8_t*)(s)];\
    a = (val >> 24) & 0xff;\
    y = (val >> 16) & 0xff;\
    u = (val >> 8) & 0xff;\
    v = val & 0xff;\
}

#define YUVA_OUT(d, y, u, v, a)\
{\
    ((uint32_t *)(d))[0] = (a << 24) | (y << 16) | (u << 8) | v;\
}


#define BPP 1

static void blend_subrect(AVPicture *dst, const AVSubtitleRect *rect, int imgw, int imgh)
{
    int wrap, wrap3, width2, skip2;
    int y, u, v, a, u1, v1, a1, w, h;
    uint8_t *lum, *cb, *cr;
    const uint8_t *p;
    const uint32_t *pal;
    int dstx, dsty, dstw, dsth;

    dstw = av_clip(rect->w, 0, imgw);
    dsth = av_clip(rect->h, 0, imgh);
    dstx = av_clip(rect->x, 0, imgw - dstw);
    dsty = av_clip(rect->y, 0, imgh - dsth);
    lum = dst->data[0] + dsty * dst->linesize[0];
    cb  = dst->data[1] + (dsty >> 1) * dst->linesize[1];
    cr  = dst->data[2] + (dsty >> 1) * dst->linesize[2];

    width2 = ((dstw + 1) >> 1) + (dstx & ~dstw & 1);
    skip2 = dstx >> 1;
    wrap = dst->linesize[0];
    wrap3 = rect->pict.linesize[0];
    p = rect->pict.data[0];
    pal = (const uint32_t *)rect->pict.data[1];  /* Now in YCrCb! */

    if (dsty & 1) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            cb++;
            cr++;
            lum++;
            p += BPP;
        }
        for (w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += 2 * BPP;
            lum += 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            p++;
            lum++;
        }
        p += wrap3 - dstw * BPP;
        lum += wrap - dstw - dstx;
        cb += dst->linesize[1] - width2 - skip2;
        cr += dst->linesize[2] - width2 - skip2;
    }
    for (h = dsth - (dsty & 1); h >= 2; h -= 2) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            p += wrap3;
            lum += wrap;
            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += -wrap3 + BPP;
            lum += -wrap + 1;
        }
        for (w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            p += wrap3;
            lum += wrap;

            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);

            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 2);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 2);

            cb++;
            cr++;
            p += -wrap3 + 2 * BPP;
            lum += -wrap + 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            p += wrap3;
            lum += wrap;
            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += -wrap3 + BPP;
            lum += -wrap + 1;
        }
        p += wrap3 + (wrap3 - dstw * BPP);
        lum += wrap + (wrap - dstw - dstx);
        cb += dst->linesize[1] - width2 - skip2;
        cr += dst->linesize[2] - width2 - skip2;
    }
    /* handle odd height */
    if (h) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            cb++;
            cr++;
            lum++;
            p += BPP;
        }
        for (w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v, 1);
            cb++;
            cr++;
            p += 2 * BPP;
            lum += 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
        }
    }
}

static void free_subpicture(SubPicture *sp)
{
    avsubtitle_free(&sp->sub);
}

static void calculate_display_rect(SDL_Rect *rect, int scr_xleft, int scr_ytop, int scr_width, int scr_height, VideoPicture *vp)
{
    float aspect_ratio;
    int width, height, x, y;

    if (vp->sample_aspect_ratio.num == 0)
        aspect_ratio = 0;
    else
        aspect_ratio = av_q2d(vp->sample_aspect_ratio);

    if (aspect_ratio <= 0.0)
        aspect_ratio = 1.0;
    aspect_ratio *= (float)vp->width / (float)vp->height;

    /* XXX: we suppose the screen has a 1.0 pixel ratio */
    height = scr_height;
    width = ((int)rint(height * aspect_ratio)) & ~1;
    if (width > scr_width) {
        width = scr_width;
        height = ((int)rint(width / aspect_ratio)) & ~1;
    }
    x = (scr_width - width) / 2;
    y = (scr_height - height) / 2;
    rect->x = scr_xleft + x;
    rect->y = scr_ytop  + y;
    rect->w = FFMAX(width,  1);
    rect->h = FFMAX(height, 1);
}

static void video_image_display(VideoState *is)
{
    VideoPicture    *vp;
    SubPicture      *sp;
    AVPicture       pict;
    SDL_Rect        rect;
    FFP_YUV420P_DATA    yuvData;
    int i;

    if (isHalt)
    {
       __sendInfoMessage(FFP_INFO_NONE, "PLAY Video halts!");
       return;
    }    
    memset( &yuvData, 0 , sizeof(yuvData) );

    vp = &is->pictq[is->pictq_rindex];
    if (vp->bmp) {
        if (is->subtitle_st) 
        {
            if (is->subpq_size > 0) 
            {
                sp = &is->subpq[is->subpq_rindex];

                if (vp->pts >= sp->pts + ((float) sp->sub.start_display_time / 1000)) 
                {
                    SDL_LockYUVOverlay (vp->bmp);

                    pict.data[0] = vp->bmp->pixels[0];
                    pict.data[1] = vp->bmp->pixels[2];
                    pict.data[2] = vp->bmp->pixels[1];

                    pict.linesize[0] = vp->bmp->pitches[0];
                    pict.linesize[1] = vp->bmp->pitches[2];
                    pict.linesize[2] = vp->bmp->pitches[1];

                    for (i = 0; i < sp->sub.num_rects; i++)
                        blend_subrect(&pict, sp->sub.rects[i],
                                      vp->bmp->w, vp->bmp->h);

                    SDL_UnlockYUVOverlay (vp->bmp);
                }
            }
        }

        calculate_display_rect(&rect, is->xleft, is->ytop, is->width, is->height, vp);

        switch(FFP_events->ui_type)
        {
          case FFP_CLI:
                SDL_DisplayYUVOverlay(vp->bmp, &rect);    //VIDEO Data Callback location
            break;
          case FFP_GUI:
            if (FFP_events->event_video)
            {
                SDL_LockYUVOverlay (vp->bmp);
                yuvData.w = vp->bmp->w;
                yuvData.h = vp->bmp->h;

                yuvData.pixels = vp->bmp->pixels;

                FFP_events->event_video( FFP_events->sender, &yuvData );
                         
                SDL_UnlockYUVOverlay (vp->bmp);                
            }
            else
            {
                SDL_DisplayYUVOverlay(vp->bmp, &rect);    //VIDEO Data Callback location
            }
            break;
          default:
                SDL_DisplayYUVOverlay(vp->bmp, &rect);    //VIDEO Data Callback location
            break;
        }

        if (FFP_events->playstatus != FFP_PLAY)
        {
            FFP_events->playstatus = FFP_PLAY;
            if (FFP_events->event_play_status)
               FFP_events->event_play_status( FFP_events->sender, FFP_PLAY);                
        }    
                 
        if (rect.x != is->last_display_rect.x || 
                rect.y != is->last_display_rect.y || 
                     rect.w != is->last_display_rect.w || 
                         rect.h != is->last_display_rect.h || is->force_refresh) 
        {
            int bgcolor = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
            fill_border(is->xleft, is->ytop, is->width, is->height, rect.x, rect.y, rect.w, rect.h, bgcolor, 1);
            is->last_display_rect = rect;
        }

    }
}

static inline int compute_mod(int a, int b)
{
    return a < 0 ? a%b + b : a%b;
}

static void video_audio_display(VideoState *s)
{
    int i, i_start, x, y1, y, ys, delay, n, nb_display_channels;
    int ch, channels, h, h2, bgcolor, fgcolor;
    int64_t time_diff;
    int rdft_bits, nb_freq;

    for (rdft_bits = 1; (1 << rdft_bits) < 2 * s->height; rdft_bits++)
        ;
    nb_freq = 1 << (rdft_bits - 1);

    /* compute display index : center on currently output samples */
    channels = s->audio_tgt.channels;
    nb_display_channels = channels;
    if (!s->paused) {
        int data_used= s->show_mode == SHOW_MODE_WAVES ? s->width : (2*nb_freq);
        n = 2 * channels;
        delay = s->audio_write_buf_size;
        delay /= n;

        /* to be more precise, we take into account the time spent since
           the last buffer computation */
        if (audio_callback_time) {
            time_diff = av_gettime() - audio_callback_time;
            delay -= (time_diff * s->audio_tgt.freq) / 1000000;
        }

        delay += 2 * data_used;
        if (delay < data_used)
            delay = data_used;

        i_start= x = compute_mod(s->sample_array_index - delay * channels, SAMPLE_ARRAY_SIZE);
        if (s->show_mode == SHOW_MODE_WAVES) {
            h = INT_MIN;
            for (i = 0; i < 1000; i += channels) {
                int idx = (SAMPLE_ARRAY_SIZE + x - i) % SAMPLE_ARRAY_SIZE;
                int a = s->sample_array[idx];
                int b = s->sample_array[(idx + 4 * channels) % SAMPLE_ARRAY_SIZE];
                int c = s->sample_array[(idx + 5 * channels) % SAMPLE_ARRAY_SIZE];
                int d = s->sample_array[(idx + 9 * channels) % SAMPLE_ARRAY_SIZE];
                int score = a - d;
                if (h < score && (b ^ c) < 0) {
                    h = score;
                    i_start = idx;
                }
            }
        }

        s->last_i_start = i_start;
    } else {
        i_start = s->last_i_start;
    }

    bgcolor = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
    if (s->show_mode == SHOW_MODE_WAVES) {
        fill_rectangle(screen,
                       s->xleft, s->ytop, s->width, s->height,
                       bgcolor, 0);

        fgcolor = SDL_MapRGB(screen->format, 0xff, 0xff, 0xff);

        /* total height for one channel */
        h = s->height / nb_display_channels;
        /* graph height / 2 */
        h2 = (h * 9) / 20;
        for (ch = 0; ch < nb_display_channels; ch++) {
            i = i_start + ch;
            y1 = s->ytop + ch * h + (h / 2); /* position of center line */
            for (x = 0; x < s->width; x++) {
                y = (s->sample_array[i] * h2) >> 15;
                if (y < 0) {
                    y = -y;
                    ys = y1 - y;
                } else {
                    ys = y1;
                }
                fill_rectangle(screen,
                               s->xleft + x, ys, 1, y,
                               fgcolor, 0);
                i += channels;
                if (i >= SAMPLE_ARRAY_SIZE)
                    i -= SAMPLE_ARRAY_SIZE;
            }
        }

        fgcolor = SDL_MapRGB(screen->format, 0x00, 0x00, 0xff);

        for (ch = 1; ch < nb_display_channels; ch++) {
            y = s->ytop + ch * h;
            fill_rectangle(screen,
                           s->xleft, y, s->width, 1,
                           fgcolor, 0);
        }
        SDL_UpdateRect(screen, s->xleft, s->ytop, s->width, s->height);
    } else {
        nb_display_channels= FFMIN(nb_display_channels, 2);
        if (rdft_bits != s->rdft_bits) {
            av_rdft_end(s->rdft);
            av_free(s->rdft_data);
            s->rdft = av_rdft_init(rdft_bits, DFT_R2C);
            s->rdft_bits = rdft_bits;
            s->rdft_data = av_malloc(4 * nb_freq * sizeof(*s->rdft_data));
        }
        {
            FFTSample *data[2];
            for (ch = 0; ch < nb_display_channels; ch++) {
                data[ch] = s->rdft_data + 2 * nb_freq * ch;
                i = i_start + ch;
                for (x = 0; x < 2 * nb_freq; x++) {
                    double w = (x-nb_freq) * (1.0 / nb_freq);
                    data[ch][x] = s->sample_array[i] * (1.0 - w * w);
                    i += channels;
                    if (i >= SAMPLE_ARRAY_SIZE)
                        i -= SAMPLE_ARRAY_SIZE;
                }
                av_rdft_calc(s->rdft, data[ch]);
            }
            // least efficient way to do this, we should of course directly access it but its more than fast enough
            for (y = 0; y < s->height; y++) {
                double w = 1 / sqrt(nb_freq);
                int a = sqrt(w * sqrt(data[0][2 * y + 0] * data[0][2 * y + 0] + data[0][2 * y + 1] * data[0][2 * y + 1]));
                int b = (nb_display_channels == 2 ) ? sqrt(w * sqrt(data[1][2 * y + 0] * data[1][2 * y + 0]
                       + data[1][2 * y + 1] * data[1][2 * y + 1])) : a;
                a = FFMIN(a, 255);
                b = FFMIN(b, 255);
                fgcolor = SDL_MapRGB(screen->format, a, b, (a + b) / 2);

                fill_rectangle(screen,
                            s->xpos, s->height-y, 1, 1,
                            fgcolor, 0);
            }
        }
        SDL_UpdateRect(screen, s->xpos, s->ytop, 1, s->height);
        if (!s->paused)
            s->xpos++;
        if (s->xpos >= s->width)
            s->xpos= s->xleft;
    }
}

static void stream_close(VideoState *is)
{
    VideoPicture *vp;
    int i;
    AVFormatContext *ic = is->ic;    
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    is->abort_request = 1;

    SDL_WaitThread(is->read_tid, NULL);
    packet_queue_destroy(&is->videoq);
    packet_queue_destroy(&is->audioq);
    packet_queue_destroy(&is->subtitleq);

    /* free all pictures */
    for (i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++) {
        vp = &is->pictq[i];
#if CONFIG_AVFILTER
        avfilter_unref_bufferp(&vp->picref);
#endif
        if (vp->bmp) {
            SDL_FreeYUVOverlay(vp->bmp);
            vp->bmp = NULL;
        }
    }
    SDL_DestroyMutex(is->pictq_mutex);
    SDL_DestroyCond(is->pictq_cond);
    SDL_DestroyMutex(is->subpq_mutex);
    SDL_DestroyCond(is->subpq_cond);
    SDL_DestroyCond(is->continue_read_thread);

#if !CONFIG_AVFILTER
    sws_freeContext(is->img_convert_ctx);
#endif
    av_free(is);
    __ClearVideoState();
}

static void do_exit(VideoState *is)
{
    if (is) {
        stream_close(is);
    }

    av_lockmgr_register(NULL);
    uninit_opts();
#if CONFIG_AVFILTER
    avfilter_uninit();
    av_freep(&vfilters);
#endif
    avformat_network_deinit();
    if (show_status)
        printf("\n");

    SDL_ShowCursor(1);
    cursor_hidden = 0;        
    cursor_last_shown = av_gettime();
	
    SDL_Quit();

    screen = NULL;

    av_log(NULL, AV_LOG_QUIET, "%s", "");

    __exit(0);
}

static void do_stop(VideoState *is)
{
   SDL_Event  event;
   event.type = SDL_QUIT;

   SDL_PushEvent(&event);
}

static int video_open(VideoState *is, int force_set_video_mode, VideoPicture *vp)
{
    int flags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL;
    int w,h;
    SDL_Rect rect;

    char msg[64];

    if (is_full_screen) flags |= SDL_FULLSCREEN;
    else                flags |= SDL_RESIZABLE;

    if (vp && vp->width) {
        calculate_display_rect(&rect, 0, 0, INT_MAX, vp->height, vp);
        default_width  = rect.w;
        default_height = rect.h;
    }

    if (is_full_screen && fs_screen_width) {
        w = fs_screen_width;
        h = fs_screen_height;
    } else if (!is_full_screen && screen_width) {
        w = screen_width;
        h = screen_height;
    } else {
        w = default_width;
        h = default_height;
    }
    if (screen && is->width == screen->w && screen->w == w
       && is->height== screen->h && screen->h == h && !force_set_video_mode)
        return 0;

    screen = SDL_SetVideoMode(w, h, 0, flags);

    if (!screen) 
    {
        __sendInfoMessage( FFP_INFO_ERROR, "Device: could not set video mode - exiting");
        do_exit(is);
    }
    
    if (!window_title)
        window_title = input_filename;

    SDL_WM_SetCaption(window_title, window_title);

    is->width  = screen->w;
    is->height = screen->h;
        
    return 0;
}

/* display the current picture, if any */
static void video_display(VideoState *is)
{
    if (!screen)
        video_open(is, 0, NULL);
    if (is->audio_st && is->show_mode != SHOW_MODE_VIDEO)
        video_audio_display(is);
    else if (is->video_st)
        video_image_display(is);
}

/* get the current audio clock value */
static double get_audio_clock(VideoState *is)
{
    if (is->audio_clock_serial != is->audioq.serial)
        return NAN;
    if (is->paused) {
        return is->audio_current_pts;
    } else {
        return is->audio_current_pts_drift + av_gettime() / 1000000.0;
    }
}

/* get the current video clock value */
static double get_video_clock(VideoState *is)
{
    if (is->video_clock_serial != is->videoq.serial)
        return NAN;
    if (is->paused) {
        return is->video_current_pts;
    } else {
        return is->video_current_pts_drift + av_gettime() / 1000000.0;
    }
}

/* get the current external clock value */
static double get_external_clock(VideoState *is)
{
    if (is->paused) {
        return is->external_clock;
    } else {
        double time = av_gettime() / 1000000.0;
        return is->external_clock_drift + time - (time - is->external_clock_time / 1000000.0) * (1.0 - is->external_clock_speed);
    }
}

static int get_master_sync_type(VideoState *is) {
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    } else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

/* get the current master clock value */
static double get_master_clock(VideoState *is)
{
    double val;

    switch (get_master_sync_type(is)) {
        case AV_SYNC_VIDEO_MASTER:
            val = get_video_clock(is);
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = get_audio_clock(is);
            break;
        default:
            val = get_external_clock(is);
            break;
    }
    return val;
}

static void update_external_clock_pts(VideoState *is, double pts)
{
   is->external_clock_time = av_gettime();
   is->external_clock = pts;
   is->external_clock_drift = pts - is->external_clock_time / 1000000.0;
}

static void check_external_clock_sync(VideoState *is, double pts) {
    double ext_clock = get_external_clock(is);
    if (isnan(ext_clock) || fabs(ext_clock - pts) > AV_NOSYNC_THRESHOLD) {
        update_external_clock_pts(is, pts);
    }
}

static void update_external_clock_speed(VideoState *is, double speed) {
    update_external_clock_pts(is, get_external_clock(is));
    is->external_clock_speed = speed;
}

static void check_external_clock_speed(VideoState *is) {
   if (is->video_stream >= 0 && is->videoq.nb_packets <= MIN_FRAMES / 2 ||
       is->audio_stream >= 0 && is->audioq.nb_packets <= MIN_FRAMES / 2) {
       update_external_clock_speed(is, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, is->external_clock_speed - EXTERNAL_CLOCK_SPEED_STEP));
   } else if ((is->video_stream < 0 || is->videoq.nb_packets > MIN_FRAMES * 2) &&
              (is->audio_stream < 0 || is->audioq.nb_packets > MIN_FRAMES * 2)) {
       update_external_clock_speed(is, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, is->external_clock_speed + EXTERNAL_CLOCK_SPEED_STEP));
   } else {
       double speed = is->external_clock_speed;
       if (speed != 1.0)
           update_external_clock_speed(is, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
   }
}

/* seek in the stream */
static void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes)
{
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
        SDL_CondSignal(is->continue_read_thread);
    }
}

/* pause or resume the video */
static void stream_toggle_pause(VideoState *is)
{
    if (is->paused) {
        is->frame_timer += av_gettime() / 1000000.0 + is->video_current_pts_drift - is->video_current_pts;
        if (is->read_pause_return != AVERROR(ENOSYS)) {
            is->video_current_pts = is->video_current_pts_drift + av_gettime() / 1000000.0;
        }
        is->video_current_pts_drift = is->video_current_pts - av_gettime() / 1000000.0;
    }
    update_external_clock_pts(is, get_external_clock(is));
    is->paused = !is->paused;
}

static void toggle_pause(VideoState *is)
{
    stream_toggle_pause(is);
    is->step = 0;
    
    switch(is->paused)
    {
      case 0:
        if (FFP_events->event_play_status)
        {
            FFP_events->playstatus = FFP_RESUMED;
            FFP_events->event_play_status( FFP_events->sender, FFP_RESUMED);
        }
        break;
      case 1:
        if (FFP_events->event_play_status)
        {
            FFP_events->playstatus = FFP_PAUSED;
            FFP_events->event_play_status( FFP_events->sender, FFP_PAUSED);
        }
        break;
    }
}

static void step_to_next_frame(VideoState *is)
{
    /* if the stream is paused unpause it, then step */
    if (is->paused)
        stream_toggle_pause(is);
    is->step = 1;
}

static double compute_target_delay(double delay, VideoState *is)
{
    double sync_threshold, diff;

    /* update delay to follow master synchronisation source */
    if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        diff = get_video_clock(is) - get_master_clock(is);

        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD, delay);
        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            if (diff <= -sync_threshold)
                delay = 0;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }

    av_dlog(NULL, "video: delay=%0.3f A-V=%f\n",
            delay, -diff);

    return delay;
}

static void pictq_next_picture(VideoState *is) {
    /* update queue size and signal for next picture */
    if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
        is->pictq_rindex = 0;

    SDL_LockMutex(is->pictq_mutex);
    is->pictq_size--;
    SDL_CondSignal(is->pictq_cond);
    SDL_UnlockMutex(is->pictq_mutex);
}

static int pictq_prev_picture(VideoState *is) {
    VideoPicture *prevvp;
    int ret = 0;
    /* update queue size and signal for the previous picture */
    prevvp = &is->pictq[(is->pictq_rindex + VIDEO_PICTURE_QUEUE_SIZE - 1) % VIDEO_PICTURE_QUEUE_SIZE];
    if (prevvp->allocated && prevvp->serial == is->videoq.serial) {
        SDL_LockMutex(is->pictq_mutex);
        if (is->pictq_size < VIDEO_PICTURE_QUEUE_SIZE - 1) {
            if (--is->pictq_rindex == -1)
                is->pictq_rindex = VIDEO_PICTURE_QUEUE_SIZE - 1;
            is->pictq_size++;
            ret = 1;
        }
        SDL_CondSignal(is->pictq_cond);
        SDL_UnlockMutex(is->pictq_mutex);
    }
    return ret;
}

static void update_video_pts(VideoState *is, double pts, int64_t pos, int serial) 
{
    double time = av_gettime() / 1000000.0;
    /* update current video pts */
    is->video_current_pts = pts;
    is->video_current_pts_drift = is->video_current_pts - time;
    is->video_current_pos = pos;
    is->frame_last_pts = pts;
    is->video_clock_serial = serial;
    if (is->videoq.serial == serial)
        check_external_clock_sync(is, is->video_current_pts);
}

/* called to display each frame */
static void video_refresh(void *opaque, double *remaining_time)
{
    VideoState *is = opaque;
    VideoPicture *vp;
    double time;

    SubPicture *sp, *sp2;

    if (!is->paused && get_master_sync_type(is) == AV_SYNC_EXTERNAL_CLOCK && is->realtime)
        check_external_clock_speed(is);

    if (!display_disable && is->show_mode != SHOW_MODE_VIDEO && is->audio_st) 
    {
        time = av_gettime() / 1000000.0;
        if (is->force_refresh || is->last_vis_time + rdftspeed < time) 
        {
            video_display(is);
            is->last_vis_time = time;
        }
        *remaining_time = FFMIN(*remaining_time, is->last_vis_time + rdftspeed - time);
    }

    if (is->video_st) 
    {
        int redisplay = 0;
        if (is->force_refresh)
            redisplay = pictq_prev_picture(is);
retry:
        if (is->pictq_size == 0) 
        {
            SDL_LockMutex(is->pictq_mutex);
            if (is->frame_last_dropped_pts != AV_NOPTS_VALUE && is->frame_last_dropped_pts > is->frame_last_pts) {
                update_video_pts(is, is->frame_last_dropped_pts, is->frame_last_dropped_pos, 0);
                is->frame_last_dropped_pts = AV_NOPTS_VALUE;
            }
            SDL_UnlockMutex(is->pictq_mutex);
            // nothing to do, no picture to display in the queue
        } else {
            double last_duration, duration, delay;
            /* dequeue the picture */
            vp = &is->pictq[is->pictq_rindex];

            if (vp->serial != is->videoq.serial) 
            {
                pictq_next_picture(is);
                redisplay = 0;
                goto retry;
            }

            if (is->paused)
                goto display;

            /* compute nominal last_duration */
            last_duration = vp->pts - is->frame_last_pts;
            if (last_duration > 0 && last_duration < is->max_frame_duration) 
            {
                /* if duration of the last frame was sane, update last_duration in video state */
                is->frame_last_duration = last_duration;
            }
            delay = compute_target_delay(is->frame_last_duration, is);

            time= av_gettime()/1000000.0;
            if (time < is->frame_timer + delay) 
            {
                *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
                return;
            }

            if (delay > 0)
                is->frame_timer += delay * FFMAX(1, floor((time-is->frame_timer) / delay));

            SDL_LockMutex(is->pictq_mutex);
            update_video_pts(is, vp->pts, vp->pos, vp->serial);
            SDL_UnlockMutex(is->pictq_mutex);

            if (is->pictq_size > 1) 
            {
                VideoPicture *nextvp = &is->pictq[(is->pictq_rindex + 1) % VIDEO_PICTURE_QUEUE_SIZE];
                duration = nextvp->pts - vp->pts;
                if(!is->step && (redisplay || framedrop>0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) && time > is->frame_timer + duration){
                    if (!redisplay)
                        is->frame_drops_late++;
                    pictq_next_picture(is);
                    redisplay = 0;
                    goto retry;
                }
            }

            if (is->subtitle_st) {
                if (is->subtitle_stream_changed) {
                    SDL_LockMutex(is->subpq_mutex);

                    while (is->subpq_size) {
                        free_subpicture(&is->subpq[is->subpq_rindex]);

                        /* update queue size and signal for next picture */
                        if (++is->subpq_rindex == SUBPICTURE_QUEUE_SIZE)
                            is->subpq_rindex = 0;

                        is->subpq_size--;
                    }
                    is->subtitle_stream_changed = 0;

                    SDL_CondSignal(is->subpq_cond);
                    SDL_UnlockMutex(is->subpq_mutex);
                } else {
                    if (is->subpq_size > 0) {
                        sp = &is->subpq[is->subpq_rindex];

                        if (is->subpq_size > 1)
                            sp2 = &is->subpq[(is->subpq_rindex + 1) % SUBPICTURE_QUEUE_SIZE];
                        else
                            sp2 = NULL;

                        if ((is->video_current_pts > (sp->pts + ((float) sp->sub.end_display_time / 1000)))
                                || (sp2 && is->video_current_pts > (sp2->pts + ((float) sp2->sub.start_display_time / 1000))))
                        {
                            free_subpicture(sp);

                            /* update queue size and signal for next picture */
                            if (++is->subpq_rindex == SUBPICTURE_QUEUE_SIZE)
                                is->subpq_rindex = 0;

                            SDL_LockMutex(is->subpq_mutex);
                            is->subpq_size--;
                            SDL_CondSignal(is->subpq_cond);
                            SDL_UnlockMutex(is->subpq_mutex);
                        }
                    }
                }
            }

display:
            /* display picture */
            if (!display_disable && is->show_mode == SHOW_MODE_VIDEO)
                video_display(is);

            pictq_next_picture(is);

            if (is->step && !is->paused)
                stream_toggle_pause(is);
        }
    }
    is->force_refresh = 0;
    if (show_status) {
        static int64_t last_time;
        int64_t cur_time;
        int aqsize, vqsize, sqsize;
        double av_diff;

        cur_time = av_gettime();
        if (!last_time || (cur_time - last_time) >= 30000) 
        {
            aqsize = 0;
            vqsize = 0;
            sqsize = 0;
            if (is->audio_st)
                aqsize = is->audioq.size;
            if (is->video_st)
                vqsize = is->videoq.size;
            if (is->subtitle_st)
                sqsize = is->subtitleq.size;
            av_diff = 0;
            if (is->audio_st && is->video_st)
                av_diff = get_audio_clock(is) - get_video_clock(is);
            printf("%7.2f A-V:%7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%"PRId64"/%"PRId64"   \r",
                   get_master_clock(is),
                   av_diff,
                   is->frame_drops_early + is->frame_drops_late,
                   aqsize / 1024,
                   vqsize / 1024,
                   sqsize,
                   is->video_st ? is->video_st->codec->pts_correction_num_faulty_dts : 0,
                   is->video_st ? is->video_st->codec->pts_correction_num_faulty_pts : 0);
            fflush(stdout);
            last_time = cur_time;
        }
    }
}

/* allocate a picture (needs to do that in main thread to avoid
   potential locking problems */
static void alloc_picture(VideoState *is)
{
    VideoPicture *vp;
    
    memset(&ffp_vid_params, 0, sizeof(ffp_vid_params));
    
    vp = &is->pictq[is->pictq_windex];

    if (vp->bmp)
        SDL_FreeYUVOverlay(vp->bmp);

#if CONFIG_AVFILTER
    avfilter_unref_bufferp(&vp->picref);
#endif

    video_open(is, 0, vp);

    vp->bmp = SDL_CreateYUVOverlay(vp->width, vp->height,
                                   SDL_YV12_OVERLAY,
                                   screen);
    if (!vp->bmp || vp->bmp->pitches[0] < vp->width) 
    {
        /* SDL allocates a buffer smaller than requested if the video
         * overlay hardware is unable to support the requested size. */
        char errorMsg[256];
        sprintf(errorMsg, "Error: the video system does not support an image\n"
                        "size of %dx%d pixels. Try using -lowres or -vf \"scale=w:h\"\n"
                        "to reduce the image size.", vp->width, vp->height);
        __sendInfoMessage(FFP_INFO_ERROR, errorMsg);
        do_exit(is);
    }
    
    duration = is->ic->duration;
    
    SDL_LockMutex(is->pictq_mutex);
    vp->allocated = 1;
    SDL_CondSignal(is->pictq_cond);
    SDL_UnlockMutex(is->pictq_mutex);
}

static void duplicate_right_border_pixels(SDL_Overlay *bmp) {
    int i, width, height;
    Uint8 *p, *maxp;
    for (i = 0; i < 3; i++) {
        width  = bmp->w;
        height = bmp->h;
        if (i > 0) {
            width  >>= 1;
            height >>= 1;
        }
        if (bmp->pitches[i] > width) {
            maxp = bmp->pixels[i] + bmp->pitches[i] * height - 1;
            for (p = bmp->pixels[i] + width - 1; p < maxp; p += bmp->pitches[i])
                *(p+1) = *p;
        }
    }
}

static int queue_picture(VideoState *is, AVFrame *src_frame, double pts, int64_t pos, int serial)
{
    VideoPicture *vp;

#if defined(DEBUG_SYNC) && 0
    printf("frame_type=%c pts=%0.3f\n",
           av_get_picture_type_char(src_frame->pict_type), pts);
#endif

    /* wait until we have space to put a new picture */
    SDL_LockMutex(is->pictq_mutex);

    /* keep the last already displayed picture in the queue */
    while (is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE - 2 &&
           !is->videoq.abort_request) {
        SDL_CondWait(is->pictq_cond, is->pictq_mutex);
    }
    SDL_UnlockMutex(is->pictq_mutex);

    if (is->videoq.abort_request)
        return -1;

    vp = &is->pictq[is->pictq_windex];

#if CONFIG_AVFILTER
    vp->sample_aspect_ratio = ((AVFilterBufferRef *)src_frame->opaque)->video->sample_aspect_ratio;
#else
    vp->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, src_frame);
#endif

    /* alloc or resize hardware picture buffer */
    if (!vp->bmp || vp->reallocate || !vp->allocated ||
        vp->width  != src_frame->width ||
        vp->height != src_frame->height) {
        SDL_Event event;

        vp->allocated  = 0;
        vp->reallocate = 0;
        vp->width = src_frame->width;
        vp->height = src_frame->height;

#ifdef CJ_VIDEO_START_RESIZE        
        event.type = SDL_VIDEORESIZE;
        event.resize.w = src_frame->width;
        event.resize.h = src_frame->height;
        SDL_PushEvent(&event);
#endif
        /* the allocation must be done in the main thread to avoid
           locking problems. */
        event.type = FF_ALLOC_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);

        /* wait until the picture is allocated */
        SDL_LockMutex(is->pictq_mutex);
        while (!vp->allocated && !is->videoq.abort_request) {
            SDL_CondWait(is->pictq_cond, is->pictq_mutex);
        }
        /* if the queue is aborted, we have to pop the pending ALLOC event or wait for the allocation to complete */
        if (is->videoq.abort_request && SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENTMASK(FF_ALLOC_EVENT)) != 1) {
            while (!vp->allocated) {
                SDL_CondWait(is->pictq_cond, is->pictq_mutex);
            }
        }
        SDL_UnlockMutex(is->pictq_mutex);

        if (is->videoq.abort_request)
            return -1;
    }

    /* if the frame is not skipped, then display it */
    if (vp->bmp) {
        AVPicture pict = { { 0 } };
#if CONFIG_AVFILTER
        avfilter_unref_bufferp(&vp->picref);
        vp->picref = src_frame->opaque;
#endif

        /* get a pointer on the bitmap */
        SDL_LockYUVOverlay (vp->bmp);

        pict.data[0] = vp->bmp->pixels[0];
        pict.data[1] = vp->bmp->pixels[2];
        pict.data[2] = vp->bmp->pixels[1];

        pict.linesize[0] = vp->bmp->pitches[0];
        pict.linesize[1] = vp->bmp->pitches[2];
        pict.linesize[2] = vp->bmp->pitches[1];

#if CONFIG_AVFILTER
        // FIXME use direct rendering
        av_picture_copy(&pict, (AVPicture *)src_frame,
                        src_frame->format, vp->width, vp->height);
#else
        av_opt_get_int(sws_opts, "sws_flags", 0, &sws_flags);
        is->img_convert_ctx = sws_getCachedContext(is->img_convert_ctx,
            vp->width, vp->height, src_frame->format, vp->width, vp->height,
            AV_PIX_FMT_YUV420P, sws_flags, NULL, NULL, NULL);
        if (is->img_convert_ctx == NULL) {
            __sendInfoMessage(FFP_INFO_ERROR, "Device: could not set video mode - exiting");
            __exit(1);
        }
        sws_scale(is->img_convert_ctx, src_frame->data, src_frame->linesize,
                  0, vp->height, pict.data, pict.linesize);
#endif
        /* workaround SDL PITCH_WORKAROUND */
        duplicate_right_border_pixels(vp->bmp);
        /* update the bitmap content */
        SDL_UnlockYUVOverlay(vp->bmp);

        vp->pts = pts;
        vp->pos = pos;
        vp->serial = serial;

        /* now we can update the picture count */
        if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
            is->pictq_windex = 0;
        SDL_LockMutex(is->pictq_mutex);
        is->pictq_size++;
        SDL_UnlockMutex(is->pictq_mutex);
    }
    return 0;
}

static int get_video_frame(VideoState *is, AVFrame *frame, int64_t *pts, AVPacket *pkt, int *serial)
{
    int got_picture;

    if (packet_queue_get(&is->videoq, pkt, 1, serial) < 0)
        return -1;

    if (pkt->data == flush_pkt.data) {
        avcodec_flush_buffers(is->video_st->codec);

        SDL_LockMutex(is->pictq_mutex);
        // Make sure there are no long delay timers (ideally we should just flush the queue but that's harder)
        while (is->pictq_size && !is->videoq.abort_request) {
            SDL_CondWait(is->pictq_cond, is->pictq_mutex);
        }
        is->video_current_pos = -1;
        is->frame_last_pts = AV_NOPTS_VALUE;
        is->frame_last_duration = 0;
        is->frame_timer = (double)av_gettime() / 1000000.0;
        is->frame_last_dropped_pts = AV_NOPTS_VALUE;
        SDL_UnlockMutex(is->pictq_mutex);

        return 0;
    }

    if(avcodec_decode_video2(is->video_st->codec, frame, &got_picture, pkt) < 0)
        return 0;

    if (got_picture) {
        int ret = 1;

        if (decoder_reorder_pts == -1) {
            *pts = av_frame_get_best_effort_timestamp(frame);
        } else if (decoder_reorder_pts) {
            *pts = frame->pkt_pts;
        } else {
            *pts = frame->pkt_dts;
        }

        if (*pts == AV_NOPTS_VALUE) {
            *pts = 0;
        }

        if (framedrop>0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) {
            SDL_LockMutex(is->pictq_mutex);
            if (is->frame_last_pts != AV_NOPTS_VALUE && *pts) {
                double clockdiff = get_video_clock(is) - get_master_clock(is);
                double dpts = av_q2d(is->video_st->time_base) * *pts;
                double ptsdiff = dpts - is->frame_last_pts;
                if (!isnan(clockdiff) && fabs(clockdiff) < AV_NOSYNC_THRESHOLD &&
                     ptsdiff > 0 && ptsdiff < AV_NOSYNC_THRESHOLD &&
                     clockdiff + ptsdiff - is->frame_last_filter_delay < 0) {
                    is->frame_last_dropped_pos = pkt->pos;
                    is->frame_last_dropped_pts = dpts;
                    is->frame_drops_early++;
                    ret = 0;
                }
            }
            SDL_UnlockMutex(is->pictq_mutex);
        }

        return ret;
    }
    return 0;
}

#if CONFIG_AVFILTER
static int configure_filtergraph(AVFilterGraph *graph, const char *filtergraph,
                                 AVFilterContext *source_ctx, AVFilterContext *sink_ctx)
{
    int ret;
    AVFilterInOut *outputs = NULL, *inputs = NULL;

    if (filtergraph) {
        outputs = avfilter_inout_alloc();
        inputs  = avfilter_inout_alloc();
        if (!outputs || !inputs) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        outputs->name       = av_strdup("in");
        outputs->filter_ctx = source_ctx;
        outputs->pad_idx    = 0;
        outputs->next       = NULL;

        inputs->name        = av_strdup("out");
        inputs->filter_ctx  = sink_ctx;
        inputs->pad_idx     = 0;
        inputs->next        = NULL;

        if ((ret = avfilter_graph_parse(graph, filtergraph, &inputs, &outputs, NULL)) < 0)
            goto fail;
    } else {
        if ((ret = avfilter_link(source_ctx, 0, sink_ctx, 0)) < 0)
            goto fail;
    }

    ret = avfilter_graph_config(graph, NULL);
fail:
    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);
    return ret;
}

static int configure_video_filters(AVFilterGraph *graph, VideoState *is, const char *vfilters, AVFrame *frame)
{
    static const enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    char sws_flags_str[128];
    char buffersrc_args[256];
    int ret;
    AVBufferSinkParams *buffersink_params = av_buffersink_params_alloc();
    AVFilterContext *filt_src = NULL, *filt_out = NULL, *filt_crop;
    AVCodecContext *codec = is->video_st->codec;
    
    if (!buffersink_params)
    {
       return AVERROR(ENOMEM);
    }
    av_opt_get_int(sws_opts, "sws_flags", 0, &sws_flags);
    snprintf(sws_flags_str, sizeof(sws_flags_str), "flags=%"PRId64, sws_flags);
    graph->scale_sws_opts = av_strdup(sws_flags_str);

    snprintf(buffersrc_args, sizeof(buffersrc_args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             frame->width, frame->height, frame->format,
             is->video_st->time_base.num, is->video_st->time_base.den,
             codec->sample_aspect_ratio.num, FFMAX(codec->sample_aspect_ratio.den, 1));
    printf("\n\n>> %s \n", buffersrc_args);
#if 0
    static int i;
    char Buffer1[256], Buffer2[256];
    sprintf(Buffer2,"ffplay_buffer_%d",i);
    i++; 
    if ((ret = avfilter_graph_create_filter(&filt_src,
                                            avfilter_get_by_name("buffer"),
                                            Buffer2, buffersrc_args, NULL,
                                            graph)) < 0)
#else
    if ((ret = avfilter_graph_create_filter(&filt_src,
                                            avfilter_get_by_name("buffer"),
                                            "ffplay_buffer", buffersrc_args, NULL,
                                            graph)) < 0)
#endif
    {
        char errorStr[256];
        av_strerror( ret, errorStr, sizeof(errorStr) );
        __sendInfoMessage(FFP_INFO_ERROR,errorStr);
        goto fail;
    }
    buffersink_params->pixel_fmts = pix_fmts;
    ret = avfilter_graph_create_filter(&filt_out,
                                       avfilter_get_by_name("ffbuffersink"),
                                       "ffplay_buffersink", NULL, buffersink_params, graph);
    if (ret < 0)
    {
        goto fail;
    }
    /* SDL YUV code is not handling odd width/height for some driver
     * combinations, therefore we crop the picture to an even width/height. */
    if ((ret = avfilter_graph_create_filter(&filt_crop,
                                            avfilter_get_by_name("crop"),
                                            "ffplay_crop", "floor(in_w/2)*2:floor(in_h/2)*2", NULL, graph)) < 0)
    {
        goto fail;
    }
    if ((ret = avfilter_link(filt_crop, 0, filt_out, 0)) < 0)
    {
        goto fail;
    }
    if ((ret = configure_filtergraph(graph, vfilters, filt_src, filt_crop)) < 0)
    {
        goto fail;
    }
    is->in_video_filter  = filt_src;
    is->out_video_filter = filt_out;

fail:
    av_freep(&buffersink_params);
    return ret;
}

#endif  /* CONFIG_AVFILTER */

static int video_thread(void *arg)
{
    AVPacket pkt = { 0 };
    VideoState *is = arg;
    AVFrame *frame = avcodec_alloc_frame();
    int64_t pts_int = AV_NOPTS_VALUE, pos = -1;
    double pts;
    int ret;
    int serial = 0;

#if CONFIG_AVFILTER
    AVCodecContext  *codec = is->video_st->codec;
    AVFilterGraph   *graph = avfilter_graph_alloc();
    AVFilterContext *filt_out = NULL, *filt_in = NULL;
    int last_w = 0;
    int last_h = 0;
    enum AVPixelFormat last_format = -2;
    int last_serial = -1;

    if (codec->codec->capabilities & CODEC_CAP_DR1) {
        is->use_dr1 = 1;
        codec->get_buffer     = codec_get_buffer;
        codec->release_buffer = codec_release_buffer;
        codec->opaque         = &is->buffer_pool;
    }
#endif

    for (;;) {
#if CONFIG_AVFILTER
        AVFilterBufferRef *picref;
        AVRational tb;
#endif
        while (is->paused && !is->videoq.abort_request)
            SDL_Delay(10);

        avcodec_get_frame_defaults(frame);
        av_free_packet(&pkt);

        ret = get_video_frame(is, frame, &pts_int, &pkt, &serial);
        if (ret < 0)
            goto the_end;

        if (!ret)
            continue;

#if CONFIG_AVFILTER
        if (   last_w != frame->width
            || last_h != frame->height
            || last_format != frame->format
            || last_serial != serial) {
            av_log(NULL, AV_LOG_DEBUG,
                   "Video frame changed from size:%dx%d format:%s serial:%d to size:%dx%d format:%s serial:%d\n",
                   last_w, last_h,
                   (const char *)av_x_if_null(av_get_pix_fmt_name(last_format), "none"), last_serial,
                   frame->width, frame->height,
                   (const char *)av_x_if_null(av_get_pix_fmt_name(frame->format), "none"), serial);
            avfilter_graph_free(&graph); 

            graph = avfilter_graph_alloc();
            if ((ret = configure_video_filters(graph, is, vfilters, frame)) < 0) {
                SDL_Event event;
                event.type = FF_QUIT_EVENT;
                event.user.data1 = is;
                SDL_PushEvent(&event);
                av_free_packet(&pkt);
                goto the_end;
            }
            filt_in  = is->in_video_filter;
            filt_out = is->out_video_filter;
            last_w = frame->width;
            last_h = frame->height;
            last_format = frame->format;
            last_serial = serial;
        }

        frame->pts = pts_int;
        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);
        if (is->use_dr1 && frame->opaque) {
            FrameBuffer      *buf = frame->opaque;
            AVFilterBufferRef *fb = avfilter_get_video_buffer_ref_from_arrays(
                                        frame->data, frame->linesize,
                                        AV_PERM_READ | AV_PERM_PRESERVE,
                                        frame->width, frame->height,
                                        frame->format);

            avfilter_copy_frame_props(fb, frame);
            fb->buf->priv           = buf;
            fb->buf->free           = filter_release_buffer;

            buf->refcount++;
            av_buffersrc_add_ref(filt_in, fb, AV_BUFFERSRC_FLAG_NO_COPY);

        } else
            av_buffersrc_write_frame(filt_in, frame);

        av_free_packet(&pkt);

        while (ret >= 0) {
            is->frame_last_returned_time = av_gettime() / 1000000.0;

            ret = av_buffersink_get_buffer_ref(filt_out, &picref, 0);
            if (ret < 0) {
                ret = 0;
                break;
            }

            is->frame_last_filter_delay = av_gettime() / 1000000.0 - is->frame_last_returned_time;
            if (fabs(is->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
                is->frame_last_filter_delay = 0;

            avfilter_copy_buf_props(frame, picref);

            pts_int = picref->pts;
            tb      = filt_out->inputs[0]->time_base;
            pos     = picref->pos;
            frame->opaque = picref;

            if (av_cmp_q(tb, is->video_st->time_base)) {
                av_unused int64_t pts1 = pts_int;
                pts_int = av_rescale_q(pts_int, tb, is->video_st->time_base);
                av_dlog(NULL, "video_thread(): "
                        "tb:%d/%d pts:%"PRId64" -> tb:%d/%d pts:%"PRId64"\n",
                        tb.num, tb.den, pts1,
                        is->video_st->time_base.num, is->video_st->time_base.den, pts_int);
            }
            pts = pts_int * av_q2d(is->video_st->time_base);
            ret = queue_picture(is, frame, pts, pos, serial);
        }
#else
        pts = pts_int * av_q2d(is->video_st->time_base);
        ret = queue_picture(is, frame, pts, pkt.pos, serial);
#endif

        if (ret < 0)
            goto the_end;
    }
 the_end:
    avcodec_flush_buffers(is->video_st->codec);
#if CONFIG_AVFILTER
    avfilter_graph_free(&graph);
#endif
    av_free_packet(&pkt);
    avcodec_free_frame(&frame);
    return 0;
}

static int subtitle_thread(void *arg)
{
    VideoState *is = arg;
    SubPicture *sp;
    AVPacket pkt1, *pkt = &pkt1;
    int got_subtitle;
    double pts;
    int i, j;
    int r, g, b, y, u, v, a;

    for (;;) {
        while (is->paused && !is->subtitleq.abort_request) {
            SDL_Delay(10);
        }
        if (packet_queue_get(&is->subtitleq, pkt, 1, NULL) < 0)
            break;

        if (pkt->data == flush_pkt.data) {
            avcodec_flush_buffers(is->subtitle_st->codec);
            continue;
        }
        SDL_LockMutex(is->subpq_mutex);
        while (is->subpq_size >= SUBPICTURE_QUEUE_SIZE &&
               !is->subtitleq.abort_request) {
            SDL_CondWait(is->subpq_cond, is->subpq_mutex);
        }
        SDL_UnlockMutex(is->subpq_mutex);

        if (is->subtitleq.abort_request)
            return 0;

        sp = &is->subpq[is->subpq_windex];

       /* NOTE: ipts is the PTS of the _first_ picture beginning in
           this packet, if any */
        pts = 0;
        if (pkt->pts != AV_NOPTS_VALUE)
            pts = av_q2d(is->subtitle_st->time_base) * pkt->pts;

        avcodec_decode_subtitle2(is->subtitle_st->codec, &sp->sub,
                                 &got_subtitle, pkt);
        if (got_subtitle && sp->sub.format == 0) {
            if (sp->sub.pts != AV_NOPTS_VALUE)
                pts = sp->sub.pts / (double)AV_TIME_BASE;
            sp->pts = pts;

            for (i = 0; i < sp->sub.num_rects; i++)
            {
                for (j = 0; j < sp->sub.rects[i]->nb_colors; j++)
                {
                    RGBA_IN(r, g, b, a, (uint32_t*)sp->sub.rects[i]->pict.data[1] + j);
                    y = RGB_TO_Y_CCIR(r, g, b);
                    u = RGB_TO_U_CCIR(r, g, b, 0);
                    v = RGB_TO_V_CCIR(r, g, b, 0);
                    YUVA_OUT((uint32_t*)sp->sub.rects[i]->pict.data[1] + j, y, u, v, a);
                }
            }

            /* now we can update the picture count */
            if (++is->subpq_windex == SUBPICTURE_QUEUE_SIZE)
                is->subpq_windex = 0;
            SDL_LockMutex(is->subpq_mutex);
            is->subpq_size++;
            SDL_UnlockMutex(is->subpq_mutex);
        }
        av_free_packet(pkt);
    }
    return 0;
}

/* copy samples for viewing in editor window */
static void update_sample_display(VideoState *is, short *samples, int samples_size)
{
    int size, len;

    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - is->sample_array_index;
        if (len > size)
            len = size;
        memcpy(is->sample_array + is->sample_array_index, samples, len * sizeof(short));
        samples += len;
        is->sample_array_index += len;
        if (is->sample_array_index >= SAMPLE_ARRAY_SIZE)
            is->sample_array_index = 0;
        size -= len;
    }
}

/* return the wanted number of samples to get better sync if sync_type is video
 * or external master clock */
static int synchronize_audio(VideoState *is, int nb_samples)
{
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = get_audio_clock(is) - get_master_clock(is);

        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                is->audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = FFMIN(FFMAX(wanted_nb_samples, min_nb_samples), max_nb_samples);
                }
                av_dlog(NULL, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
                        diff, avg_diff, wanted_nb_samples - nb_samples,
                        is->audio_clock, is->audio_diff_threshold);
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum       = 0;
        }
    }

    return wanted_nb_samples;
}

/**
 * Decode one audio frame and return its uncompressed size.
 *
 * The processed audio frame is decoded, converted if required, and
 * stored in is->audio_buf, with size in bytes given by the return
 * value.
 */
static int audio_decode_frame(VideoState *is)
{
    AVPacket *pkt_temp = &is->audio_pkt_temp;
    AVPacket *pkt = &is->audio_pkt;
    AVCodecContext *dec = is->audio_st->codec;
    int len1, len2, data_size, resampled_data_size;
    int64_t dec_channel_layout;
    int got_frame;
    av_unused double audio_clock0;
    int new_packet = 0;
    int flush_complete = 0;
    int wanted_nb_samples;

    for (;;) {
        /* NOTE: the audio packet can contain several frames */
        while (pkt_temp->size > 0 || (!pkt_temp->data && new_packet)) {
            if (!is->frame) {
                if (!(is->frame = avcodec_alloc_frame()))
                    return AVERROR(ENOMEM);
            } else
                avcodec_get_frame_defaults(is->frame);

            if (is->audioq.serial != is->audio_pkt_temp_serial)
                break;

            if (is->paused)
                return -1;

            if (flush_complete)
                break;
            new_packet = 0;
            len1 = avcodec_decode_audio4(dec, is->frame, &got_frame, pkt_temp);
            if (len1 < 0) {
                /* if error, we skip the frame */
                pkt_temp->size = 0;
                break;
            }

            pkt_temp->data += len1;
            pkt_temp->size -= len1;

            if (!got_frame) {
                /* stop sending empty packets if the decoder is finished */
                if (!pkt_temp->data && dec->codec->capabilities & CODEC_CAP_DELAY)
                    flush_complete = 1;
                continue;
            }
            data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(is->frame),
                                                   is->frame->nb_samples,
                                                   is->frame->format, 1);

            dec_channel_layout =
                (is->frame->channel_layout && av_frame_get_channels(is->frame) == av_get_channel_layout_nb_channels(is->frame->channel_layout)) ?
                is->frame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(is->frame));
            wanted_nb_samples = synchronize_audio(is, is->frame->nb_samples);

            if (is->frame->format        != is->audio_src.fmt            ||
                dec_channel_layout       != is->audio_src.channel_layout ||
                is->frame->sample_rate   != is->audio_src.freq           ||
                (wanted_nb_samples       != is->frame->nb_samples && !is->swr_ctx)) {
                swr_free(&is->swr_ctx);
                is->swr_ctx = swr_alloc_set_opts(NULL,
                                                 is->audio_tgt.channel_layout, is->audio_tgt.fmt, is->audio_tgt.freq,
                                                 dec_channel_layout,           is->frame->format, is->frame->sample_rate,
                                                 0, NULL);
                if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) 
                {
                    char errorMsg[256];
                    sprintf(errorMsg, "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!",
                            is->frame->sample_rate, av_get_sample_fmt_name(is->frame->format), av_frame_get_channels(is->frame),
                            is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt), is->audio_tgt.channels );
                    __sendInfoMessage(FFP_INFO_WARNING, errorMsg);
                    break;
                }
                is->audio_src.channel_layout = dec_channel_layout;
                is->audio_src.channels       = av_frame_get_channels(is->frame);
                is->audio_src.freq = is->frame->sample_rate;
                is->audio_src.fmt = is->frame->format;
            }

            if (is->swr_ctx) {
                const uint8_t **in = (const uint8_t **)is->frame->extended_data;
                uint8_t **out = &is->audio_buf1;
                int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / is->frame->sample_rate + 256;
                int out_size  = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, out_count, is->audio_tgt.fmt, 0);
                if (wanted_nb_samples != is->frame->nb_samples) {
                    if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - is->frame->nb_samples) * is->audio_tgt.freq / is->frame->sample_rate,
                                                wanted_nb_samples * is->audio_tgt.freq / is->frame->sample_rate) < 0) {
                        __sendInfoMessage( FFP_INFO_WARNING, "swr_set_compensation() failed.");
                        break;
                    }
                }
                av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
                if (!is->audio_buf1)
                    return AVERROR(ENOMEM);
                len2 = swr_convert(is->swr_ctx, out, out_count, in, is->frame->nb_samples);
                if (len2 < 0) {
                    __sendInfoMessage( FFP_INFO_WARNING,"swr_convert() failed.");
                    break;
                }
                if (len2 == out_count) {
                    __sendInfoMessage(FFP_INFO_WARNING, "warning: audio buffer is probably too small.");
                    swr_init(is->swr_ctx);
                }
                is->audio_buf = is->audio_buf1;
                resampled_data_size = len2 * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
            } else {
                is->audio_buf = is->frame->data[0];
                resampled_data_size = data_size;
            }

            audio_clock0 = is->audio_clock;
            is->audio_clock += (double)data_size /
                (av_frame_get_channels(is->frame) * is->frame->sample_rate * av_get_bytes_per_sample(is->frame->format));
#ifdef DEBUG
            {
                static double last_clock;
                printf("audio: delay=%0.3f clock=%0.3f clock0=%0.3f\n",
                       is->audio_clock - last_clock,
                       is->audio_clock, audio_clock0);
                last_clock = is->audio_clock;
            }
#endif
            return resampled_data_size;
        }

        /* free the current packet */
        if (pkt->data)
            av_free_packet(pkt);
        memset(pkt_temp, 0, sizeof(*pkt_temp));

        if (is->audioq.abort_request) {
            return -1;
        }

        if (is->audioq.nb_packets == 0)
            SDL_CondSignal(is->continue_read_thread);

        /* read next packet */
        if ((new_packet = packet_queue_get(&is->audioq, pkt, 1, &is->audio_pkt_temp_serial)) < 0)
            return -1;

        if (pkt->data == flush_pkt.data) {
            avcodec_flush_buffers(dec);
            flush_complete = 0;
        }

        *pkt_temp = *pkt;

        /* if update the audio clock with the pts */
        if (pkt->pts != AV_NOPTS_VALUE) {
            is->audio_clock = av_q2d(is->audio_st->time_base)*pkt->pts;
            is->audio_clock_serial = is->audio_pkt_temp_serial;
        }
    }
}

/* prepare a new audio buffer */
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    VideoState *is = opaque;
    int audio_size, len1;
    int bytes_per_sec;
    int frame_size = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, 1, is->audio_tgt.fmt, 1);
    void *pWorkBuff = NULL;
    
    if (isHalt)
    {
        __sendInfoMessage(FFP_INFO_NONE, "PLAY Audio halts!");;
       return;
    }    
  
    audio_callback_time = av_gettime();

    pWorkBuff = malloc(len);
    while (len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) 
        {
           audio_size = audio_decode_frame(is);
           if (audio_size < 0) 
           {
                /* if error, just output silence */
               is->audio_buf      = is->silence_buf;
               is->audio_buf_size = sizeof(is->silence_buf) / frame_size * frame_size;
           } 
           else 
           {
               if (is->show_mode != SHOW_MODE_VIDEO)
                   update_sample_display(is, (int16_t *)is->audio_buf, audio_size);
               is->audio_buf_size = audio_size;
           }
           is->audio_buf_index = 0;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;
#if 1        
        if (pWorkBuff)
        {
            memcpy( pWorkBuff, (uint8_t *)is->audio_buf + is->audio_buf_index, len1); 
            if (FFP_events->event_audio)
            {
              FFP_events->event_audio( FFP_events->sender, pWorkBuff, len1);
            }
            memcpy(stream, pWorkBuff, len1);
        }
        else
        {
            memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);        
        }
#else
        memcpy( pWorkBuff, (uint8_t *)is->audio_buf + is->audio_buf_index, len1); 
        if (FFP_events->event_audio)
        {
           FFP_events->event_audio( FFP_events->sender, pWorkBuff, len1);
        }
        memcpy(stream, pWorkBuff, len1);
#endif        
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
    free(pWorkBuff);
    
    bytes_per_sec = is->audio_tgt.freq * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    /* Let's assume the audio driver that is used by SDL has two periods. */
    is->audio_current_pts = is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / bytes_per_sec;
    is->audio_current_pts_drift = is->audio_current_pts - audio_callback_time / 1000000.0;
    if (is->audioq.serial == is->audio_clock_serial)
        check_external_clock_sync(is, is->audio_current_pts);
}

static int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params)
{
    SDL_AudioSpec wanted_spec, spec;
    const char *env;
    const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    char errorMsg[256];
    memset(&ffp_aud_params, 0, sizeof(ffp_aud_params));
    
    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
    }
    if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_spec.channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        __sendInfoMessage(FFP_INFO_WARNING, "Invalid sample rate or channel count!");
        return -1;
    }
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = opaque;
    while (SDL_OpenAudio(&wanted_spec, &spec) < 0) 
    {
        sprintf(errorMsg, "SDL_OpenAudio (%d channels): %s", wanted_spec.channels, SDL_GetError());
        __sendInfoMessage(FFP_INFO_WARNING, errorMsg);
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) 
        {
            __sendInfoMessage(FFP_INFO_WARNING, "No more channel combinations to try, audio open failed.");
            return -1;
        }
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
    }
    if (spec.format != AUDIO_S16SYS) {
        sprintf(errorMsg,"Audio format %d is not supported!", spec.format);
        __sendInfoMessage(FFP_INFO_WARNING, errorMsg);
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            sprintf( errorMsg, "Audio channel count %d is not supported!", spec.channels );
            __sendInfoMessage(FFP_INFO_WARNING, errorMsg);
            return -1;
        }
    }

    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels =  spec.channels;

    ffp_aud_params.Freq     = spec.freq;
    ffp_aud_params.Channels = spec.channels;
    ffp_aud_params.Format   = spec.format;
    ffp_aud_params.SamplesInBuffer = spec.samples;
    ffp_aud_params.BufferSizeInBytes = spec.size;
    
    return spec.size;
}

/* open a given stream. Return 0 if OK */
static int stream_component_open(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;
    AVCodec *codec;
    const char *forced_codec_name = NULL;
    AVDictionary *opts;
    AVDictionaryEntry *t = NULL;
    char errorMsg[256];
    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;
    avctx = ic->streams[stream_index]->codec;

    codec = avcodec_find_decoder(avctx->codec_id);

    switch(avctx->codec_type){
        case AVMEDIA_TYPE_AUDIO   : is->last_audio_stream    = stream_index; forced_codec_name =    audio_codec_name; break;
        case AVMEDIA_TYPE_SUBTITLE: is->last_subtitle_stream = stream_index; forced_codec_name = subtitle_codec_name; break;
        case AVMEDIA_TYPE_VIDEO   : is->last_video_stream    = stream_index; forced_codec_name =    video_codec_name; break;
    }
    if (forced_codec_name)
        codec = avcodec_find_decoder_by_name(forced_codec_name);
    if (!codec) {
        if (forced_codec_name) sprintf(errorMsg, "No codec could be found with name '%s'.", forced_codec_name);
        else                   sprintf(errorMsg, "No codec could be found with id %d.", avctx->codec_id);
        __sendInfoMessage(FFP_INFO_WARNING, errorMsg);
        return -1;
    }

    avctx->codec_id = codec->id;
    avctx->workaround_bugs   = workaround_bugs;
    avctx->lowres            = lowres;
    if(avctx->lowres > codec->max_lowres){
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
                codec->max_lowres);
        avctx->lowres= codec->max_lowres;
    }
    avctx->idct_algo         = idct;
    avctx->skip_frame        = skip_frame;
    avctx->skip_idct         = skip_idct;
    avctx->skip_loop_filter  = skip_loop_filter;
    avctx->error_concealment = error_concealment;

    if(avctx->lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
    if (fast)   avctx->flags2 |= CODEC_FLAG2_FAST;
    if(codec->capabilities & CODEC_CAP_DR1)
        avctx->flags |= CODEC_FLAG_EMU_EDGE;

    opts = filter_codec_opts(codec_opts, avctx->codec_id, ic, ic->streams[stream_index], codec);
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
    if (avcodec_open2(avctx, codec, &opts) < 0)
        return -1;
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        return AVERROR_OPTION_NOT_FOUND;
    }

    /* prepare audio output */
    if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        int audio_hw_buf_size = audio_open(is, avctx->channel_layout, avctx->channels, avctx->sample_rate, &is->audio_src);
        if (audio_hw_buf_size < 0)
            return -1;
        is->audio_hw_buf_size = audio_hw_buf_size;
        is->audio_tgt = is->audio_src;
    }

    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_stream = stream_index;
        is->audio_st = ic->streams[stream_index];
        is->audio_buf_size  = 0;
        is->audio_buf_index = 0;

        /* init averaging filter */
        is->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        /* since we do not have a precise anough audio fifo fullness,
           we correct audio sync only if larger than this threshold */
        is->audio_diff_threshold = 2.0 * is->audio_hw_buf_size / av_samples_get_buffer_size(NULL, is->audio_tgt.channels, is->audio_tgt.freq, is->audio_tgt.fmt, 1);

        memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
        memset(&is->audio_pkt_temp, 0, sizeof(is->audio_pkt_temp));
        packet_queue_start(&is->audioq);
        SDL_PauseAudio(0);
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        packet_queue_start(&is->videoq);
        is->video_tid = SDL_CreateThread(video_thread, is);
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_stream = stream_index;
        is->subtitle_st = ic->streams[stream_index];
        packet_queue_start(&is->subtitleq);

        is->subtitle_tid = SDL_CreateThread(subtitle_thread, is);
        break;
    default:
        break;
    }
    return 0;
}

static void stream_component_close(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    avctx = ic->streams[stream_index]->codec;

    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        packet_queue_abort(&is->audioq);

        SDL_CloseAudio();

        packet_queue_flush(&is->audioq);
        av_free_packet(&is->audio_pkt);
        swr_free(&is->swr_ctx);
        av_freep(&is->audio_buf1);
        is->audio_buf1_size = 0;
        is->audio_buf = NULL;
        avcodec_free_frame(&is->frame);

        if (is->rdft) {
            av_rdft_end(is->rdft);
            av_freep(&is->rdft_data);
            is->rdft = NULL;
            is->rdft_bits = 0;
        }
        break;
    case AVMEDIA_TYPE_VIDEO:
        packet_queue_abort(&is->videoq);
        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        SDL_LockMutex(is->pictq_mutex);
        SDL_CondSignal(is->pictq_cond);
        SDL_UnlockMutex(is->pictq_mutex);

        SDL_WaitThread(is->video_tid, NULL);

        packet_queue_flush(&is->videoq);
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        packet_queue_abort(&is->subtitleq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        SDL_LockMutex(is->subpq_mutex);
        is->subtitle_stream_changed = 1;

        SDL_CondSignal(is->subpq_cond);
        SDL_UnlockMutex(is->subpq_mutex);

        SDL_WaitThread(is->subtitle_tid, NULL);

        packet_queue_flush(&is->subtitleq);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    avcodec_close(avctx);
#if CONFIG_AVFILTER
    free_buffer_pool(&is->buffer_pool);
#endif
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = NULL;
        is->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = NULL;
        is->video_stream = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_st = NULL;
        is->subtitle_stream = -1;
        break;
    default:
        break;
    }
}

static int decode_interrupt_cb(void *ctx)
{
    VideoState *is = ctx;
    return is->abort_request;
}

static int is_realtime(AVFormatContext *s)
{
    if(   !strcmp(s->iformat->name, "rtp")
       || !strcmp(s->iformat->name, "rtsp")
       || !strcmp(s->iformat->name, "sdp")
    )
        return 1;

    if(s->pb && (   !strncmp(s->filename, "rtp:", 4)
                 || !strncmp(s->filename, "udp:", 4)
                )
    )
        return 1;
    return 0;
}

/* this thread gets the stream from the disk or the network */
static int read_thread(void *arg)
{
    int isStream = 0;
    
    VideoState *is = arg;
    AVFormatContext *ic = NULL;
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    int eof = 0;
    int pkt_in_play_range = 0;
    AVDictionaryEntry *t;
    AVDictionary **opts;
    int orig_nb_streams;
    SDL_mutex *wait_mutex = SDL_CreateMutex();
    char errorMsg[256];
    memset(st_index, -1, sizeof(st_index));
    is->last_video_stream = is->video_stream = -1;
    is->last_audio_stream = is->audio_stream = -1;
    is->last_subtitle_stream = is->subtitle_stream = -1;

    ic = avformat_alloc_context();
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = is;
    err = avformat_open_input(&ic, is->filename, is->iformat, &format_opts);
    if (err < 0) {
        print_error(is->filename, err);
        ret = -1;
        goto fail;
    }
    if ((t = av_dict_get(format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }
    is->ic = ic;

    if (genpts)
        ic->flags |= AVFMT_FLAG_GENPTS;

    opts = setup_find_stream_info_opts(ic, codec_opts);
    orig_nb_streams = ic->nb_streams;

    err = avformat_find_stream_info(ic, opts);
    if (err < 0) {
        sprintf(errorMsg, "%s: could not find codec parameters.", is->filename);
        __sendInfoMessage(FFP_INFO_ERROR, errorMsg);                 
        ret = -1;
        goto fail;
    }
    for (i = 0; i < orig_nb_streams; i++)
        av_dict_free(&opts[i]);
    av_freep(&opts);

    if (ic->pb)
        ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use url_feof() to test for the end

    if (seek_by_bytes < 0)
        seek_by_bytes = !!(ic->iformat->flags & AVFMT_TS_DISCONT);

    is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    /* if seeking requested, we execute it */
    if (start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;

        timestamp = start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            sprintf(errorMsg,"%s: could not seek to position %0.3f",
                    is->filename, (double)timestamp / AV_TIME_BASE);
            __sendInfoMessage(FFP_INFO_WARNING, errorMsg);         
        }
    }

    is->realtime = is_realtime(ic);

    for (i = 0; i < ic->nb_streams; i++)
        ic->streams[i]->discard = AVDISCARD_ALL;
    if (!video_disable)
        st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                wanted_stream[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    if (!audio_disable)
        st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                                wanted_stream[AVMEDIA_TYPE_AUDIO],
                                st_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);
    if (!video_disable && !subtitle_disable)
        st_index[AVMEDIA_TYPE_SUBTITLE] =
            av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
                                wanted_stream[AVMEDIA_TYPE_SUBTITLE],
                                (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                 st_index[AVMEDIA_TYPE_AUDIO] :
                                 st_index[AVMEDIA_TYPE_VIDEO]),
                                NULL, 0);
    if (show_status) {
        av_dump_format(ic, 0, is->filename, 0);
    }

    is->show_mode = show_mode;

    /* open the streams */
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        stream_component_open(is, st_index[AVMEDIA_TYPE_AUDIO]);
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret = stream_component_open(is, st_index[AVMEDIA_TYPE_VIDEO]);
    }
    if (is->show_mode == SHOW_MODE_NONE)
        is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;

    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        stream_component_open(is, st_index[AVMEDIA_TYPE_SUBTITLE]);
    }

    if (is->video_stream < 0 && is->audio_stream < 0) {
        sprintf(errorMsg, "%s: could not open codecs", is->filename);
        __sendInfoMessage( FFP_INFO_ERROR, errorMsg);
        ret = -1;
        goto fail;
    }

    if (infinite_buffer < 0 && is->realtime)
        infinite_buffer = 1;

    for (;;) {
        if (is->abort_request)
            break;
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
        if (is->paused &&
                (!strcmp(ic->iformat->name, "rtsp") ||
                 (ic->pb && !strncmp(input_filename, "mmsh:", 5)))) {
            /* wait 10 ms to avoid trying to get another packet */
            /* XXX: horrible */
            SDL_Delay(10);
            continue;
        }
        if ( (!strcmp(ic->iformat->name, "rtsp") ||
             (ic->pb && !strncmp(input_filename, "mmsh:", 5)))) 
        {
            isStream = 1;
        }
        
#endif
        if (is->seek_req) {
            int64_t seek_target = is->seek_pos;
            int64_t seek_min    = is->seek_rel > 0 ? seek_target - is->seek_rel + 2: INT64_MIN;
            int64_t seek_max    = is->seek_rel < 0 ? seek_target - is->seek_rel - 2: INT64_MAX;
// FIXME the +-2 is due to rounding being not done in the correct direction in generation
//      of the seek_pos/seek_rel variables

            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0) {
                sprintf( errorMsg, "%s: error while seeking.", is->ic->filename);
                __sendInfoMessage( FFP_INFO_WARNING, errorMsg );
            } else {
                if (is->audio_stream >= 0) {
                    packet_queue_flush(&is->audioq);
                    packet_queue_put(&is->audioq, &flush_pkt);
                }
                if (is->subtitle_stream >= 0) {
                    packet_queue_flush(&is->subtitleq);
                    packet_queue_put(&is->subtitleq, &flush_pkt);
                }
                if (is->video_stream >= 0) {
                    packet_queue_flush(&is->videoq);
                    packet_queue_put(&is->videoq, &flush_pkt);
                }
                if (is->seek_flags & AVSEEK_FLAG_BYTE) {
                   update_external_clock_pts(is, NAN);
                } else {
                   update_external_clock_pts(is, seek_target / (double)AV_TIME_BASE);
                }
            }
            is->seek_req = 0;
            eof = 0;
            if (is->paused)
                step_to_next_frame(is);
        }
        if (is->queue_attachments_req) {
            avformat_queue_attached_pictures(ic);
            is->queue_attachments_req = 0;
        }

        /* if the queue are full, no need to read more */
        if (infinite_buffer<1 &&
              (is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
            || (   (is->audioq   .nb_packets > MIN_FRAMES || is->audio_stream < 0 || is->audioq.abort_request)
                && (is->videoq   .nb_packets > MIN_FRAMES || is->video_stream < 0 || is->videoq.abort_request)
                && (is->subtitleq.nb_packets > MIN_FRAMES || is->subtitle_stream < 0 || is->subtitleq.abort_request)))) {
            /* wait 10 ms */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        if (eof) {
            if (is->video_stream >= 0) {
                av_init_packet(pkt);
                pkt->data = NULL;
                pkt->size = 0;
                pkt->stream_index = is->video_stream;
                packet_queue_put(&is->videoq, pkt);
            }
            if (is->audio_stream >= 0 &&
                is->audio_st->codec->codec->capabilities & CODEC_CAP_DELAY) {
                av_init_packet(pkt);
                pkt->data = NULL;
                pkt->size = 0;
                pkt->stream_index = is->audio_stream;
                packet_queue_put(&is->audioq, pkt);
            }
            SDL_Delay(10);
            if (is->audioq.size + is->videoq.size + is->subtitleq.size == 0) {
                if (loop != 1 && (!loop || --loop)) {
                    stream_seek(is, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
                } else if (autoexit) {
                    ret = AVERROR_EOF;
                    goto fail;
                }
            }
            eof=0;
            continue;
        }
        
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            if (isStream)
            {
                sprintf( errorMsg, "rtsp,mmsh stream read error: %d", ret);
                __sendInfoMessage(FFP_INFO_STREAM_ERROR, errorMsg);
                goto fail;
            }
            if (ret == AVERROR_EOF || url_feof(ic->pb))
                eof = 1;
            if (ic->pb && ic->pb->error)
                break;
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
               
            continue;
        }
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        pkt_in_play_range = duration == AV_NOPTS_VALUE ||
                (pkt->pts - ic->streams[pkt->stream_index]->start_time) *
                av_q2d(ic->streams[pkt->stream_index]->time_base) -
                (double)(start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000
                <= ((double)duration / 1000000);
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
            packet_queue_put(&is->audioq, pkt);
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range) {
            packet_queue_put(&is->videoq, pkt);
        } else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
            packet_queue_put(&is->subtitleq, pkt);
        } else {
            av_free_packet(pkt);
        }
    }
    /* wait until the end */
    while (!is->abort_request) {
        SDL_Delay(100);
    }

    ret = 0;
 fail:
    /* close each stream */
    if (is->audio_stream >= 0)
        stream_component_close(is, is->audio_stream);
    if (is->video_stream >= 0)
        stream_component_close(is, is->video_stream);
    if (is->subtitle_stream >= 0)
        stream_component_close(is, is->subtitle_stream);
    if (is->ic) {
        avformat_close_input(&is->ic);
    }

    if (ret != 0) {
        SDL_Event event;
        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    SDL_DestroyMutex(wait_mutex);
    return 0;
}

static VideoState *stream_open(const char *filename, AVInputFormat *iformat)
{
    VideoState *is;

    is = av_mallocz(sizeof(VideoState));
    if (!is)
        return NULL;
    av_strlcpy(is->filename, filename, sizeof(is->filename));
    is->iformat = iformat;
    is->ytop    = 0;
    is->xleft   = 0;

    /* start video display */
    is->pictq_mutex = SDL_CreateMutex();
    is->pictq_cond  = SDL_CreateCond();

    is->subpq_mutex = SDL_CreateMutex();
    is->subpq_cond  = SDL_CreateCond();

    packet_queue_init(&is->videoq);
    packet_queue_init(&is->audioq);
    packet_queue_init(&is->subtitleq);

    is->continue_read_thread = SDL_CreateCond();

    update_external_clock_pts(is, NAN);
    update_external_clock_speed(is, 1.0);
    is->audio_current_pts_drift = -av_gettime() / 1000000.0;
    is->video_current_pts_drift = is->audio_current_pts_drift;
    is->audio_clock_serial = -1;
    is->video_clock_serial = -1;
    is->av_sync_type = av_sync_type;
    is->read_tid     = SDL_CreateThread(read_thread, is);
    if (!is->read_tid) {
        av_free(is);
        return NULL;
    }
    return is;
}

static void stream_cycle_channel(VideoState *is, int codec_type)
{
    AVFormatContext *ic = is->ic;
    int start_index, stream_index;
    int old_index;
    AVStream *st;

    if (codec_type == AVMEDIA_TYPE_VIDEO) {
        start_index = is->last_video_stream;
        old_index = is->video_stream;
    } else if (codec_type == AVMEDIA_TYPE_AUDIO) {
        start_index = is->last_audio_stream;
        old_index = is->audio_stream;
    } else {
        start_index = is->last_subtitle_stream;
        old_index = is->subtitle_stream;
    }
    stream_index = start_index;
    for (;;) {
        if (++stream_index >= is->ic->nb_streams)
        {
            if (codec_type == AVMEDIA_TYPE_SUBTITLE)
            {
                stream_index = -1;
                is->last_subtitle_stream = -1;
                goto the_end;
            }
            if (start_index == -1)
                return;
            stream_index = 0;
        }
        if (stream_index == start_index)
            return;
        st = ic->streams[stream_index];
        if (st->codec->codec_type == codec_type) {
            /* check that parameters are OK */
            switch (codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                if (st->codec->sample_rate != 0 &&
                    st->codec->channels != 0)
                    goto the_end;
                break;
            case AVMEDIA_TYPE_VIDEO:
            case AVMEDIA_TYPE_SUBTITLE:
                goto the_end;
            default:
                break;
            }
        }
    }
 the_end:
    stream_component_close(is, old_index);
    stream_component_open(is, stream_index);
    if (codec_type == AVMEDIA_TYPE_VIDEO)
        is->queue_attachments_req = 1;
}


static void toggle_full_screen(VideoState *is)
{
#if defined(__APPLE__) && SDL_VERSION_ATLEAST(1, 2, 14)
    /* OS X needs to reallocate the SDL overlays */
    int i;
    for (i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++)
        is->pictq[i].reallocate = 1;
#endif
    is_full_screen = !is_full_screen;
    video_open(is, 1, NULL);
}

static void toggle_audio_display(VideoState *is)
{
    int bgcolor = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
    int next = is->show_mode;
    do {
        next = (next + 1) % SHOW_MODE_NB;
    } while (next != is->show_mode && (next == SHOW_MODE_VIDEO && !is->video_st || next != SHOW_MODE_VIDEO && !is->audio_st));
    if (is->show_mode != next) {
        fill_rectangle(screen,
                    is->xleft, is->ytop, is->width, is->height,
                    bgcolor, 1);
        is->force_refresh = 1;
        is->show_mode = next;
    }
}

static void refresh_loop_wait_event(VideoState *is, SDL_Event *event) 
{
    double  remaining_time = 0.0;
    double  currentTimeinMilliSecond = 0;
    static int msgUpdate;
    SDL_PumpEvents();
    while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_ALLEVENTS)) 
    {
        if (!cursor_hidden && av_gettime() - cursor_last_shown > CURSOR_HIDE_DELAY) 
        {
            SDL_ShowCursor(0);
            cursor_hidden = 1;
            __sendInfoMessage(FFP_INFO_DEBUG, "Mouse pointer OFF");
        }
        if (remaining_time > 0.0)
        {
            av_usleep((int64_t)(remaining_time * 1000000.0));
        }
        remaining_time = REFRESH_RATE;
        
        if (is->show_mode != SHOW_MODE_NONE && (!is->paused || is->force_refresh))
        {
            video_refresh(is, &remaining_time);

            #if 0
            currentTimeinMilliSecond = (is->video_current_pts);
            if ((!(msgUpdate++ % MSG_REFRESH))&&(duration!=AV_NOPTS_VALUE))   //Occurs around every 500ms
            {
                FFP_events->current_in_s = currentTimeinMilliSecond;
                FFP_events->duration_in_us = duration;
            }
            #else
	    currentTimeinMilliSecond = get_master_clock(is);
	    if (is->video_stream >= 0)
            {
	       if ((duration!=AV_NOPTS_VALUE))
	       {
	           FFP_events->current_in_s = currentTimeinMilliSecond;
	           FFP_events->duration_in_us = duration;
	       }
            }
            else if (is->audio_stream >=0)
            {
               duration = is->ic->duration;  
	       if ((duration!=AV_NOPTS_VALUE))
	       {
	           FFP_events->current_in_s = currentTimeinMilliSecond;
	           FFP_events->duration_in_us = duration;
	       }
            } 
            #endif                    
 
            if (FFP_events->event_play_status)
            {
                if ( (is->audioq.size == 0) &&(is->videoq.size == 0) &&
                     (is->subtitleq.size == 0) && (duration > 0) )
                {
                    FFP_events->playstatus = FFP_EOF;
                    FFP_events->event_play_status( FFP_events->sender, FFP_EOF);
                }
            }
            if (isHalt)
            {
                __sendInfoMessage(FFP_INFO_DEBUG, "PLAY halts!");
                break;
            }
        }
        SDL_PumpEvents();

        SDL_Delay(10);
    }
}

/* handle an event sent by the GUI */
static int event_loop_alive = 0;

static void event_cli_loop(VideoState *cur_stream)
{
    SDL_Event event;
    double incr, pos, frac;
    int    isQuit = 0;
    
    char errorMsg[256];
    for (;;) {
        double x;
        event_loop_alive = 1;
        refresh_loop_wait_event(cur_stream, &event);
        switch (event.type) 
        {
            case SDL_KEYDOWN:
              if (exit_on_keydown) 
              {
                  do_exit(cur_stream);
                  break;
              }
              switch (event.key.keysym.sym) 
              {
                 case SDLK_ESCAPE:
                 case SDLK_q:
                      do_exit(cur_stream);
                      isQuit = 1;
                      break;
                 case SDLK_f:
                      toggle_full_screen(cur_stream);
                      cur_stream->force_refresh = 1;
                      break;
                 case SDLK_p:
                 case SDLK_SPACE:
                      toggle_pause(cur_stream);
                      break;
                 case SDLK_s: // S: Step to next frame
                      step_to_next_frame(cur_stream);
                      break;
                 case SDLK_a:
                      stream_cycle_channel(cur_stream, AVMEDIA_TYPE_AUDIO);
                      break;
                 case SDLK_v:
                      stream_cycle_channel(cur_stream, AVMEDIA_TYPE_VIDEO);
                      break;
                 case SDLK_t:
                      stream_cycle_channel(cur_stream, AVMEDIA_TYPE_SUBTITLE);
                      break;
                 case SDLK_w:
                      toggle_audio_display(cur_stream);
                      break;
                 case SDLK_PAGEUP:
                      incr = 600.0;
                      goto do_seek;
                 case SDLK_PAGEDOWN:
                      incr = -600.0;
                      goto do_seek;
                 case SDLK_LEFT:
                      incr = -10.0;
                      goto do_seek;
                 case SDLK_RIGHT:
                      incr = 10.0;
                      goto do_seek;
                 case SDLK_UP:
                      incr = 60.0;
                      goto do_seek;
                 case SDLK_DOWN:
                      incr = -60.0;
                      do_seek:
                        if (seek_by_bytes) 
                        {
                            if (cur_stream->video_stream >= 0 && cur_stream->video_current_pos >= 0) 
                            {
                                pos = cur_stream->video_current_pos;
                            } 
                            else if (cur_stream->audio_stream >= 0 && cur_stream->audio_pkt.pos >= 0) 
                            {
                                pos = cur_stream->audio_pkt.pos;
                            }
                            else
                                pos = avio_tell(cur_stream->ic->pb);
                            if (cur_stream->ic->bit_rate)
                                incr *= cur_stream->ic->bit_rate / 8.0;
                            else
                                incr *= 180000.0;
                            pos += incr;
                            stream_seek(cur_stream, pos, incr, 1);
                        } 
                        else 
                        {
                            pos = get_master_clock(cur_stream);
                            if (isnan(pos))
                                pos = (double)cur_stream->seek_pos / AV_TIME_BASE;
                            pos += incr;
                            if (cur_stream->ic->start_time != AV_NOPTS_VALUE && pos < cur_stream->ic->start_time / (double)AV_TIME_BASE)
                                pos = cur_stream->ic->start_time / (double)AV_TIME_BASE;
                            stream_seek(cur_stream, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE), 0);
                        }
                      break;
                 default:
                      break;
              }
              break;
         case SDL_VIDEOEXPOSE:
              cur_stream->force_refresh = 1;
              break;
         case SDL_MOUSEBUTTONDOWN:
              if (exit_on_mousedown) 
              {
                  // do_exit(cur_stream);
                  break;
              }
         case SDL_MOUSEMOTION:
              if (cursor_hidden) 
              {
                 SDL_ShowCursor(1);
                 cursor_hidden = 0;
                 __sendInfoMessage(FFP_INFO_DEBUG, "Mouse pointer ON");
              }
             cursor_last_shown = av_gettime();
             if (event.type == SDL_MOUSEBUTTONDOWN) 
             {
                x = event.button.x;
             } else {
                 if (event.motion.state != SDL_PRESSED)
                    break;
                 x = event.motion.x;
             }
             if (seek_by_bytes || cur_stream->ic->duration <= 0) 
             {
                 uint64_t size =  avio_size(cur_stream->ic->pb);
                 stream_seek(cur_stream, size*x/cur_stream->width, 0, 1);
             } 
             else 
             {
                 int64_t ts;
                 int ns, hh, mm, ss;
                 int tns, thh, tmm, tss;
                 tns  = cur_stream->ic->duration / 1000000LL;
                 thh  = tns / 3600;
                 tmm  = (tns % 3600) / 60;
                 tss  = (tns % 60);
                 frac = x / cur_stream->width;
                 ns   = frac * tns;
                 hh   = ns / 3600;
                 mm   = (ns % 3600) / 60;
                 ss   = (ns % 60);
                 sprintf(errorMsg, "Seek to %2.0f%% (%2d:%02d:%02d) of total duration (%2d:%02d:%02d) ", frac*100,
                         hh, mm, ss, thh, tmm, tss);
                 __sendInfoMessage( FFP_INFO_NONE, errorMsg);
                 ts = frac * cur_stream->ic->duration;
                 if (cur_stream->ic->start_time != AV_NOPTS_VALUE)
                        ts += cur_stream->ic->start_time;
                 stream_seek(cur_stream, ts, 0, 0);
            }
            break;
         case SDL_VIDEORESIZE:
            screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 0,
                                      SDL_HWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_HWACCEL);

            if (FFP_events->event_video_resize)
            {
                FFP_events->event_video_resize( FFP_events->sender, event.resize.w, event.resize.h );
            }

            screen_width  = cur_stream->width  = event.resize.w;
            screen_height = cur_stream->height = event.resize.h;
            
            cur_stream->force_refresh = 1;
            __sendInfoMessage(FFP_INFO_DEBUG, "SDL_VIDEORESIZE event");
            break;
        case SDL_QUIT:
        case FF_QUIT_EVENT:
            __sendInfoMessage(FFP_INFO_NONE, "Request Quit (QUIT event)...");
            do_exit(cur_stream);
            isQuit = 1;
            break;
        case FF_ALLOC_EVENT:
            alloc_picture(event.user.data1);
            break;
        default:
            break;
        }
        if (isQuit)
        {
            isQuit = 0;
            break;
        }
    }
    if (FFP_events->event_play_status)
    {
        FFP_events->playstatus = FFP_STOP;
        FFP_events->event_play_status( FFP_events->sender, FFP_STOP);
    }
    event_loop_alive = 0;    
}

static void event_gui_loop(VideoState *cur_stream)
{
    SDL_Event event;
    double incr, pos, frac;
    int    isQuit = 0, w, h;
    VideoPicture *vp;
    VideoState   *thisVideoState;
    
    for (;;) {
        double x;
        event_loop_alive = 1;
        refresh_loop_wait_event(cur_stream, &event);
        switch(event.type)
        {
          case SDL_QUIT:
          case FF_QUIT_EVENT:
               __sendInfoMessage(FFP_INFO_NONE, "Request Quit (QUIT event)...");
              do_exit(cur_stream);
              isQuit = 1;
              break;
          case SDL_VIDEOEXPOSE:
              cur_stream->force_refresh = 1;
              break;
          case SDL_VIDEORESIZE:
              screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 0,
                                      SDL_HWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_HWACCEL);

              if (FFP_events->event_video_resize)
              {
                  FFP_events->event_video_resize( FFP_events->sender, event.resize.w, event.resize.h );
              }

              screen_width  = cur_stream->width  = event.resize.w;
              screen_height = cur_stream->height = event.resize.h;
              cur_stream->force_refresh = 1;
              __sendInfoMessage(FFP_INFO_DEBUG, "SDL_VIDEORESIZE event");
              break;
          case FF_ALLOC_EVENT:
              alloc_picture(event.user.data1);
              break;
          case SDL_MOUSEMOTION:
              if (cursor_hidden) 
              {
                 SDL_ShowCursor(1);
                 cursor_hidden = 0;
                 __sendInfoMessage(FFP_INFO_DEBUG, "Mouse pointer ON");
              }
              cursor_last_shown = av_gettime();              
              break;
          default:
              break;
        }
        if (isHalt)
        {
           do_exit(cur_stream);
           isQuit = 0;
           __sendInfoMessage(FFP_INFO_NONE, "Event loop exit!");  
           break;         
        }
        if (isQuit)
        {
            isQuit = 0;
            break;
        }
    }
    if (FFP_events->event_play_status)
    {
        FFP_events->playstatus = FFP_STOP;        
        FFP_events->event_play_status( FFP_events->sender, FFP_STOP);
    }
    event_loop_alive = 0;    
}

int opt_frame_size(void *optctx, const char *opt, const char *arg)
{
    av_log(NULL, AV_LOG_WARNING, "Option -s is deprecated, use -video_size.\n");
    return opt_default(NULL, "video_size", arg);
}

int opt_width(void *optctx, const char *opt, const char *arg)
{
    screen_width = parse_number_or_die(opt, arg, OPT_INT64, 1, INT_MAX);
    return 0;
}

int opt_height(void *optctx, const char *opt, const char *arg)
{
    screen_height = parse_number_or_die(opt, arg, OPT_INT64, 1, INT_MAX);
    return 0;
}

int opt_format(void *optctx, const char *opt, const char *arg)
{
    char errorMsg[256];
    file_iformat = av_find_input_format(arg);
    if (!file_iformat) {
        sprintf(errorMsg, "Unknown input format: %s", arg);
        __sendInfoMessage(FFP_INFO_ERROR, errorMsg);        
        return AVERROR(EINVAL);
    }
    return 0;
}

int opt_frame_pix_fmt(void *optctx, const char *opt, const char *arg)
{
    av_log(NULL, AV_LOG_WARNING, "Option -pix_fmt is deprecated, use -pixel_format.");
    return opt_default(NULL, "pixel_format", arg);
}

int opt_sync(void *optctx, const char *opt, const char *arg)
{
    char errorMsg[256];
    if (!strcmp(arg, "audio"))
        av_sync_type = AV_SYNC_AUDIO_MASTER;
    else if (!strcmp(arg, "video"))
        av_sync_type = AV_SYNC_VIDEO_MASTER;
    else if (!strcmp(arg, "ext"))
        av_sync_type = AV_SYNC_EXTERNAL_CLOCK;
    else {
        sprintf(errorMsg, "Unknown value for %s: %s", opt, arg);
        __sendInfoMessage(FFP_INFO_ERROR, errorMsg);        
        __exit(1);
    }
    return 0;
}

int opt_seek(void *optctx, const char *opt, const char *arg)
{
    start_time = parse_time_or_die(opt, arg, 1);
    return 0;
}

int opt_duration(void *optctx, const char *opt, const char *arg)
{
    duration = parse_time_or_die(opt, arg, 1);
    return 0;
}

int opt_show_mode(void *optctx, const char *opt, const char *arg)
{
    show_mode = !strcmp(arg, "video") ? SHOW_MODE_VIDEO :
                !strcmp(arg, "waves") ? SHOW_MODE_WAVES :
                !strcmp(arg, "rdft" ) ? SHOW_MODE_RDFT  :
                parse_number_or_die(opt, arg, OPT_INT, 0, SHOW_MODE_NB-1);
    return 0;
}

static void opt_input_file(void *optctx, const char *filename)
{
    char errorMsg[256];
#if 0
    if (input_filename) 
    {
        sprintf(errorMsg,"Argument '%s' provided as input filename, but '%s' was already specified.",
                filename, input_filename);
        __sendInfoMessage(FFP_INFO_ERROR, errorMsg);        
        __exit(1);
    }
#endif    
    if (!strcmp(filename, "-"))
        filename = "pipe:";
    input_filename = filename;
}

int opt_codec(void *optctx, const char *opt, const char *arg)
{
   char errorMsg[256];
   const char *spec = strchr(opt, ':');
   if (!spec) {
       sprintf(errorMsg, "No media specifier was specified in '%s' in option '%s'.",
               arg, opt);
       __sendInfoMessage(FFP_INFO_ERROR, errorMsg);        
       return AVERROR(EINVAL);
   }
   spec++;
   switch (spec[0]) {
   case 'a' :    audio_codec_name = arg; break;
   case 's' : subtitle_codec_name = arg; break;
   case 'v' :    video_codec_name = arg; break;
   default:
       sprintf(errorMsg, "Invalid media specifier '%s' in option '%s'", spec, opt );
       __sendInfoMessage(FFP_INFO_ERROR, errorMsg);
       return AVERROR(EINVAL);
   }
   return 0;
}

static int dummy;

static VideoState  *is;

static void __ClearVideoState()
{
    is = NULL;
}

static const OptionDef options[] = {
#include "cmdutils_common_opts.h"
    { "x", HAS_ARG, { .func_arg = opt_width }, "force displayed width", "width" },
    { "y", HAS_ARG, { .func_arg = opt_height }, "force displayed height", "height" },
    { "s", HAS_ARG | OPT_VIDEO, { .func_arg = opt_frame_size }, "set frame size (WxH or abbreviation)", "size" },
    { "fs", OPT_BOOL, { &is_full_screen }, "force full screen" },
    { "an", OPT_BOOL, { &audio_disable }, "disable audio" },
    { "vn", OPT_BOOL, { &video_disable }, "disable video" },
    { "sn", OPT_BOOL, { &subtitle_disable }, "disable subtitling" },
    { "ast", OPT_INT | HAS_ARG | OPT_EXPERT, { &wanted_stream[AVMEDIA_TYPE_AUDIO] }, "select desired audio stream", "stream_number" },
    { "vst", OPT_INT | HAS_ARG | OPT_EXPERT, { &wanted_stream[AVMEDIA_TYPE_VIDEO] }, "select desired video stream", "stream_number" },
    { "sst", OPT_INT | HAS_ARG | OPT_EXPERT, { &wanted_stream[AVMEDIA_TYPE_SUBTITLE] }, "select desired subtitle stream", "stream_number" },
    { "ss", HAS_ARG, { .func_arg = opt_seek }, "seek to a given position in seconds", "pos" },
    { "t", HAS_ARG, { .func_arg = opt_duration }, "play  \"duration\" seconds of audio/video", "duration" },
    { "bytes", OPT_INT | HAS_ARG, { &seek_by_bytes }, "seek by bytes 0=off 1=on -1=auto", "val" },
    { "nodisp", OPT_BOOL, { &display_disable }, "disable graphical display" },
    { "f", HAS_ARG, { .func_arg = opt_format }, "force format", "fmt" },
    { "pix_fmt", HAS_ARG | OPT_EXPERT | OPT_VIDEO, { .func_arg = opt_frame_pix_fmt }, "set pixel format", "format" },
    { "stats", OPT_BOOL | OPT_EXPERT, { &show_status }, "show status", "" },
    { "bug", OPT_INT | HAS_ARG | OPT_EXPERT, { &workaround_bugs }, "workaround bugs", "" },
    { "fast", OPT_BOOL | OPT_EXPERT, { &fast }, "non spec compliant optimizations", "" },
    { "genpts", OPT_BOOL | OPT_EXPERT, { &genpts }, "generate pts", "" },
    { "drp", OPT_INT | HAS_ARG | OPT_EXPERT, { &decoder_reorder_pts }, "let decoder reorder pts 0=off 1=on -1=auto", ""},
    { "lowres", OPT_INT | HAS_ARG | OPT_EXPERT, { &lowres }, "", "" },
    { "skiploop", OPT_INT | HAS_ARG | OPT_EXPERT, { &skip_loop_filter }, "", "" },
    { "skipframe", OPT_INT | HAS_ARG | OPT_EXPERT, { &skip_frame }, "", "" },
    { "skipidct", OPT_INT | HAS_ARG | OPT_EXPERT, { &skip_idct }, "", "" },
    { "idct", OPT_INT | HAS_ARG | OPT_EXPERT, { &idct }, "set idct algo",  "algo" },
    { "ec", OPT_INT | HAS_ARG | OPT_EXPERT, { &error_concealment }, "set error concealment options",  "bit_mask" },
    { "sync", HAS_ARG | OPT_EXPERT, { .func_arg = opt_sync }, "set audio-video sync. type (type=audio/video/ext)", "type" },
//    { "autoexit", OPT_BOOL | OPT_EXPERT, { &autoexit }, "exit at the end", "" },
    { "exitonkeydown", OPT_BOOL | OPT_EXPERT, { &exit_on_keydown }, "exit on key down", "" },
    { "exitonmousedown", OPT_BOOL | OPT_EXPERT, { &exit_on_mousedown }, "exit on mouse down", "" },
    { "loop", OPT_INT | HAS_ARG | OPT_EXPERT, { &loop }, "set number of times the playback shall be looped", "loop count" },
    { "framedrop", OPT_BOOL | OPT_EXPERT, { &framedrop }, "drop frames when cpu is too slow", "" },
    { "infbuf", OPT_BOOL | OPT_EXPERT, { &infinite_buffer }, "don't limit the input buffer size (useful with realtime streams)", "" },
    { "window_title", OPT_STRING | HAS_ARG, { &window_title }, "set window title", "window title" },
#if CONFIG_AVFILTER
    { "vf", OPT_STRING | HAS_ARG, { &vfilters }, "set video filters", "filter_graph" },
#endif
    { "rdftspeed", OPT_INT | HAS_ARG| OPT_AUDIO | OPT_EXPERT, { &rdftspeed }, "rdft speed", "msecs" },
    { "showmode", HAS_ARG, { .func_arg = opt_show_mode}, "select show mode (0 = video, 1 = waves, 2 = RDFT)", "mode" },
    { "default", HAS_ARG | OPT_AUDIO | OPT_VIDEO | OPT_EXPERT, { .func_arg = opt_default }, "generic catch all option", "" },
    { "i", OPT_BOOL, { &dummy}, "read specified file", "input_file"},
    { "codec", HAS_ARG, { .func_arg = opt_codec}, "force decoder", "decoder_name" },
    { "acodec", HAS_ARG | OPT_STRING | OPT_EXPERT, {    &audio_codec_name }, "force audio decoder",    "decoder_name" },
    { "scodec", HAS_ARG | OPT_STRING | OPT_EXPERT, { &subtitle_codec_name }, "force subtitle decoder", "decoder_name" },
    { "vcodec", HAS_ARG | OPT_STRING | OPT_EXPERT, {    &video_codec_name }, "force video decoder",    "decoder_name" },
    { NULL, },
};

static void show_usage(void)
{
    av_log(NULL, AV_LOG_INFO, "Simple media player\n");
    av_log(NULL, AV_LOG_INFO, "usage: %s [options] input_file\n", program_name);
    av_log(NULL, AV_LOG_INFO, "\n");
}

void show_help_default(const char *opt, const char *arg)
{
    av_log_set_callback(log_callback_help);
    show_usage();
    show_help_options(options, "Main options:", 0, OPT_EXPERT, 0);
    show_help_options(options, "Advanced options:", OPT_EXPERT, 0, 0);
    printf("\n");
    show_help_children(avcodec_get_class(), AV_OPT_FLAG_DECODING_PARAM);
    show_help_children(avformat_get_class(), AV_OPT_FLAG_DECODING_PARAM);
#if !CONFIG_AVFILTER
    show_help_children(sws_get_class(), AV_OPT_FLAG_ENCODING_PARAM);
#else
    show_help_children(avfilter_get_class(), AV_OPT_FLAG_FILTERING_PARAM);
#endif
    printf("\nWhile playing:\n"
           "q, ESC              quit\n"
           "f                   toggle full screen\n"
           "p, SPC              pause\n"
           "a                   cycle audio channel\n"
           "v                   cycle video channel\n"
           "t                   cycle subtitle channel\n"
           "w                   show audio waves\n"
           "s                   activate frame-step mode\n"
           "left/right          seek backward/forward 10 seconds\n"
           "down/up             seek backward/forward 1 minute\n"
           "page down/page up   seek backward/forward 10 minutes\n"
           "mouse click         seek to percentage in file corresponding to fraction of width\n"
           );
}

static int lockmgr(void **mtx, enum AVLockOp op)
{
   switch(op) {
      case AV_LOCK_CREATE:
          *mtx = SDL_CreateMutex();
          if(!*mtx)
              return 1;
          return 0;
      case AV_LOCK_OBTAIN:
          return !!SDL_LockMutex(*mtx);
      case AV_LOCK_RELEASE:
          return !!SDL_UnlockMutex(*mtx);
      case AV_LOCK_DESTROY:
          SDL_DestroyMutex(*mtx);
          return 0;
   }
   return 1;
}

FFP_BOOL EXPORTDLL multimedia_stream_open()
{
    FFP_BOOL rtn = FFP_FALSE;
    
    is = stream_open(input_filename, file_iformat);
    
    if (is)
    {
       isFileOpen = 1;
       rtn = FFP_TRUE;
    }
    return rtn;   
}

void EXPORTDLL multimedia_stream_stop()
{
    isFileOpen = 0;
    do_stop(is);
}

void EXPORTDLL multimedia_stream_start()
{
#if 1
  switch(FFP_events->ui_type)
  {
    case FFP_CLI:
      event_cli_loop(is);
      break;
    case FFP_GUI:
      event_gui_loop(is);      
      break;
    default:
      __sendInfoMessage( FFP_INFO_WARNING, "STI_EVENT_TYPE is not defined. Set to default (STI_EVENT_CLI)");
      event_gui_loop(is);
      break;      
  }
#else
  event_cli_loop(is);
#endif  
}

void EXPORTDLL multimedia_exit()
{
    do_exit(NULL);
}

void EXPORTDLL multimedia_reset_pointer()
{
#if 0
    SDL_ShowCursor(1);
    cursor_hidden = 0;        
    cursor_last_shown = av_gettime();
    __sendInfoMessage(FFP_INFO_NONE, "Mouse pointer ON");
#else
    SDL_Event event;
    event.type = SDL_MOUSEMOTION;	
    SDL_PushEvent(&event);
#endif
}

void EXPORTDLL multimedia_resize_screen(int w, int h)
{
    SDL_Event  event;
    event.type = SDL_VIDEORESIZE;
    event.resize.w = w;
    event.resize.h = h;
    SDL_PushEvent(&event);
}

int EXPORTDLL multimedia_init_device(FFP_EVENTS *sti_events)
{
    int flags, rtn = 0;
    char dummy_videodriver[] = "SDL_VIDEODRIVER=dummy";
    char dummy_screen[64];
    
    init_yuv420p_table();
    
    screen   = NULL;
    autoexit = 1;
    SDL_ShowCursor(1);
    cursor_hidden = 0;        
    cursor_last_shown = av_gettime();

    if (display_disable) {
        video_disable = 1;
    }
    
    FFP_events = sti_events;
    sprintf(dummy_screen,"SDL_WINDOWID=%d", FFP_events->screenID);
    switch(FFP_events->ui_type)
    {
      case FFP_CLI:     
           flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
           break;
      case FFP_GUI:
           flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
           SDL_putenv(dummy_screen); 
           break;
    }
    
    if (audio_disable)
        flags &= ~SDL_INIT_AUDIO;
    if (display_disable)
        SDL_putenv(dummy_videodriver); /* For the event queue, we always need a video driver. */
#if !defined(__MINGW32__) && !defined(__APPLE__)
    flags |= SDL_INIT_EVENTTHREAD; /* Not supported on Windows or Mac OS X */
#endif
    if (SDL_Init (flags)) 
    {
        char errorMsg[256];
        sprintf(errorMsg, "Could not initialize devices - %s", SDL_GetError() );
        __sendInfoMessage( FFP_INFO_ERROR, errorMsg );
        __sendInfoMessage(FFP_INFO_ERROR, "(Did you set the DISPLAY variable?)");
        rtn = 1;
        return rtn;
    }

    fs_screen_width   = 0;
    fs_screen_height  = 0;
    screen_width      = 0;
    screen_height     = 0;

    start_time = AV_NOPTS_VALUE;
    duration = AV_NOPTS_VALUE;
    
    isHalt = 0;
    
    if (!display_disable) 
    {
#if HAVE_SDL_VIDEO_SIZE
        const SDL_VideoInfo *vi = SDL_GetVideoInfo();
        fs_screen_width = vi->current_w;
        fs_screen_height = vi->current_h;
#endif
    }    
    SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    av_log_set_flags(AV_LOG_SKIP_REPEATED);

    /* register all codecs, demux and protocols */
    avcodec_register_all();
#if CONFIG_AVDEVICE
    avdevice_register_all();
#endif
#if CONFIG_AVFILTER
    avfilter_register_all();
#endif
    av_register_all();
    avformat_network_init();

    if (av_lockmgr_register(lockmgr)) 
    {
        __sendInfoMessage( FFP_INFO_ERROR, "Could not initialize lock manager!");
        multimedia_exit();
        rtn = 2;
        return rtn;
    }

    if ((FFP_events->screenID <= 0) && 
                                    (FFP_events->ui_type==FFP_GUI))
    {
        __sendInfoMessage( FFP_INFO_ERROR, "Screen ID is empty!");
        multimedia_exit();
        rtn = 3;
        return rtn;
    }

    av_init_packet(&flush_pkt);
    flush_pkt.data = (char *)(intptr_t)"FLUSH";
    
    memset(&ffp_vidstr_params, 0, sizeof(ffp_vid_params));

    return rtn;
}

const char* EXPORTDLL multimedia_get_filename()
{
  return input_filename;  
}

void EXPORTDLL multimedia_set_filename( const char *filename )
{
    init_opts();
    opt_input_file( (void*)options, filename );
}

void EXPORTDLL multimedia_parse_options(int argc, char **argv)
{
    int i;

    parse_loglevel(argc, argv, options);
    init_opts();
    show_banner(argc, argv, options);

    parse_options(NULL, argc, argv, options, opt_input_file);
}

static void refresh_loop_test_wait_event(SDL_Event *event) 
{
    double remaining_time = 0.0;

    SDL_PumpEvents();
    while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_ALLEVENTS)) 
    {
        if (remaining_time > 0.0)
            av_usleep((int64_t)(remaining_time * 1000000.0));
        
        remaining_time = REFRESH_RATE;

        SDL_PumpEvents();
    }

}
static void event_test_loop()
{
    SDL_Event event;
    
    int isQuit = 0;
    
    for (;;) {
        refresh_loop_test_wait_event(&event);
        switch(event.type)
        {
          case SDL_QUIT:
          case FF_QUIT_EVENT:
              isQuit = 1;
              do_exit( NULL );
              break;
          default:
              break;
        }
 
        if (isQuit)
            break; 
    }
}

FFP_AUD_PARAMS*  EXPORTDLL multimedia_get_audioformat()
{
    if (isFileOpen)
       return (FFP_AUD_PARAMS*)(&ffp_aud_params);
    else
       return NULL;   
}

FFP_VID_PARAMS* EXPORTDLL multimedia_get_videoformat()
{
    if ( (screen) && (isFileOpen) )
    {
      ffp_vid_params.width  = screen->w;
      ffp_vid_params.height = screen->h;
      ffp_vid_params.Bpp    = screen->format->BytesPerPixel;

      return (FFP_VID_PARAMS*)(&ffp_vid_params);
    }
    else
    {
      return NULL;
    }
}

int EXPORTDLL multimedia_test_xwin_draw_start(int XWinID, int width, int height)
{
      
   int flags, rtn = 0;
   char dummy_screen[64];

   sprintf(dummy_screen,"SDL_WINDOWID=%d", XWinID);
   SDL_putenv(dummy_screen); 

   flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
   if (SDL_Init (flags)) 
       {
        char errorMsg[256];
        sprintf(errorMsg, "Could not initialize devices - %s", SDL_GetError() );
        __sendInfoMessage( FFP_INFO_ERROR, errorMsg );
        __sendInfoMessage(FFP_INFO_ERROR, "(Did you set the DISPLAY variable?)");
        rtn = 1;
        return rtn;
        }

   screen = SDL_SetVideoMode(width, height, 0, (SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL) );               
                
   SDL_LockSurface(screen);
   SDL_FillRect(screen,0, 0x00000000);
   SDL_UnlockSurface(screen);
   SDL_UpdateRect(screen, 0, 0, 0, 0);

   return 0;
}

void EXPORTDLL multimedia_test_xwin_draw_stop()
{
   SDL_Quit();
}

void EXPORTDLL multimedia_clear_screen(int screenID, int width, int height)
{
   multimedia_test_xwin_draw_start(screenID, width, height);
}

int multimedia_test_init(FFP_EVENTS *sti_events)
{
    return multimedia_init_device(sti_events);
}

void multimedia_test_start()
{
    event_test_loop();
}

void multimedia_test_stop()
{
    do_stop(is);
}

void EXPORTDLL multimedia_yuv420p_to_rgb24(FFP_YUV420P_DATA *yuvData,unsigned char* rgbbuffer)   
{
    int y1, y2, u, v;    
    unsigned char *py1, *py2;   
    int i, j, c1, c2, c3, c4;   
    unsigned char *d1, *d2;   
    unsigned char *src_u, *src_v;

    int width  = yuvData->w;
    int height = yuvData->h;   
    
    src_u = yuvData->pixels[1]; //src_u = yuvbuffer + width * height;   // u
    src_v = yuvData->pixels[2]; //src_v = src_u + width * height / 4;  // v
    py1   = yuvData->pixels[0]; //py1 = yuvbuffer;   // y

    py2 = py1 + width;   
    d1 = rgbbuffer;   
    d2 = d1 + PIXELBYTE24 * width;   

    if (init_yuv420p == 0)
    {
        init_yuv420p_table();
    }

    for (j = 0; j < height; j += 2)    
    {    
        for (i = 0; i < width; i += 2)    
        {
            u = *src_u++;   
            v = *src_v++;   
   
            c1 = crv_tab[v];   
            c2 = cgu_tab[u];   
            c3 = cgv_tab[v];   
            c4 = cbu_tab[u];   
   
            //up-left   
            y1 = tab_76309[*py1++];    
            *d1++ = clp[384+((y1 + c1)>>16)];     
            *d1++ = clp[384+((y1 - c2 - c3)>>16)];   
            *d1++ = clp[384+((y1 + c4)>>16)];   
   
            //down-left   
            y2 = tab_76309[*py2++];   
            *d2++ = clp[384+((y2 + c1)>>16)];     
            *d2++ = clp[384+((y2 - c2 - c3)>>16)];   
            *d2++ = clp[384+((y2 + c4)>>16)];   
   
            //up-right   
            y1 = tab_76309[*py1++];   
            *d1++ = clp[384+((y1 + c1)>>16)];     
            *d1++ = clp[384+((y1 - c2 - c3)>>16)];   
            *d1++ = clp[384+((y1 + c4)>>16)];   
   
            //down-right   
            y2 = tab_76309[*py2++];   
            *d2++ = clp[384+((y2 + c1)>>16)];     
            *d2++ = clp[384+((y2 - c2 - c3)>>16)];   
            *d2++ = clp[384+((y2 + c4)>>16)];   
        }
        d1  += PIXELBYTE24*width;
        d2  += PIXELBYTE24*width;
        py1 += width;
        py2 += width;
    }
}

void EXPORTDLL multimedia_yuv420p_to_rgb32(FFP_YUV420P_DATA *yuvData,unsigned char* rgbbuffer)   
{
    int y1, y2, u, v;    
    unsigned char *py1, *py2;   
    int i, j, c1, c2, c3, c4;   
    unsigned char *d1, *d2;   
    unsigned char *src_u, *src_v;

    int width  = yuvData->w;
    int height = yuvData->h;   
    
    src_u = yuvData->pixels[1]; //src_u = yuvbuffer + width * height;   // u
    src_v = yuvData->pixels[2]; //src_v = src_u + width * height / 4;  // v
    py1   = yuvData->pixels[0]; //py1 = yuvbuffer;   // y

    py2 = py1 + width;   
    d1  = rgbbuffer;   
    d2  = d1 + PIXELBYTE32 * width;   

    if (init_yuv420p == 0)
    {
        init_yuv420p_table();
    }

    for (j = 0; j < height; j += 2)    
    {    
        for (i = 0; i < width; i += 2)    
        {
            u = *src_u++;   
            v = *src_v++;   
   
            c1 = crv_tab[v];   
            c2 = cgu_tab[u];   
            c3 = cgv_tab[v];   
            c4 = cbu_tab[u];   
   
            //up-left   
            y1 = tab_76309[*py1++];    
            *d1++ = clp[384+((y1 + c1)>>16)];     
            *d1++ = clp[384+((y1 - c2 - c3)>>16)];   
            *d1++ = clp[384+((y1 + c4)>>16)];   
            *d1++ = 0;      
            //down-left   
            y2 = tab_76309[*py2++];   
            *d2++ = clp[384+((y2 + c1)>>16)];     
            *d2++ = clp[384+((y2 - c2 - c3)>>16)];   
            *d2++ = clp[384+((y2 + c4)>>16)];   
            *d2++ = 0;
            //up-right   
            y1 = tab_76309[*py1++];   
            *d1++ = clp[384+((y1 + c1)>>16)];     
            *d1++ = clp[384+((y1 - c2 - c3)>>16)];   
            *d1++ = clp[384+((y1 + c4)>>16)];   
            *d1++ = 0;         
            //down-right   
            y2 = tab_76309[*py2++];   
            *d2++ = clp[384+((y2 + c1)>>16)];     
            *d2++ = clp[384+((y2 - c2 - c3)>>16)];   
            *d2++ = clp[384+((y2 + c4)>>16)];   
            *d2++ = 0;            
        }
        d1  += PIXELBYTE32*width;
        d2  += PIXELBYTE32*width;
        py1 += width;
        py2 += width;
    }
}

int64_t EXPORTDLL multimedia_get_duration_in_mSec()
{
    return (duration / 1000);
}

void EXPORTDLL multimedia_pause_resume()
{
    if (is)
      toggle_pause(is);
}

int EXPORTDLL multimedia_event_loop_alive()
{
    return event_loop_alive;
}

void multimedia_halt_palying()
{
    isHalt = 1;
}

void multimedia_toggle_fullscreen()
{
    toggle_full_screen(is);
    is->force_refresh = 1;
}
