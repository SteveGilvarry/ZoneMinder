#ifndef ZM_VIDEOSTORE_H
#define ZM_VIDEOSTORE_H

#include "zm_ffmpeg.h"
extern "C"  {
#include "libavutil/audio_fifo.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/timestamp.h"

#include "libswresample/swresample.h"
}

#if HAVE_LIBAVCODEC

#include "zm_monitor.h"

#define VSYNC_AUTO       -1
#define VSYNC_PASSTHROUGH 0
#define VSYNC_CFR         1
#define VSYNC_VFR         2
#define VSYNC_VSCFR       0xfe
#define VSYNC_DROP        0xff

class VideoStore {
private:

  AVOutputFormat *output_format;
  AVFormatContext *oc;
  AVStream *video_output_stream;
  AVStream *audio_output_stream;
  AVCodecContext *video_output_context;

  AVStream *video_input_stream;
  AVStream *audio_input_stream;

  // we are transcoding
  AVFrame *input_frame;
  AVFrame *output_frame;

  AVCodecContext *video_input_context;
  AVCodecContext *audio_input_context;

  // The following are used when encoding the audio stream to AAC
  AVCodec *audio_output_codec;
  AVCodecContext *audio_output_context;
  int data_present;
  AVAudioFifo *fifo;
  int output_frame_size;
  SwrContext *resample_context = NULL;
  uint8_t *converted_input_samples = NULL;
    
  const char *filename;
  const char *format;
    
  bool keyframeMessage;
  int keyframeSkipNumber;
    
  int64_t video_start_pts;
  int64_t video_start_dts;
  int64_t audio_start_pts;
  int64_t audio_start_dts;
  int64_t prevDts;
  int64_t filter_in_rescale_delta_last;
  
enum HWAccelID {
  HWACCEL_NONE = 0,
  HWACCEL_AUTO,
  HWACCEL_VDPAU,
  HWACCEL_DXVA2,
  HWACCEL_VDA,
  HWACCEL_VIDEOTOOLBOX,
  HWACCEL_QSV,
  HWACCEL_VAAPI,
  HWACCEL_CUVID,
};

enum OSTFinished {
    ENCODER_FINISHED = 1,
    MUXER_FINISHED = 2,
};

enum forced_keyframes_const {
    FKF_N,
    FKF_N_FORCED,
    FKF_PREV_FORCED_N,
    FKF_PREV_FORCED_T,
    FKF_T,
    FKF_NB
};

int copy_ts;
int video_sync_method;
int audio_sync_method;
int debug_ts;
int do_benchmark_all;

typedef struct InputFilter {
    AVFilterContext    *filter;
    struct InputStream *ist;
    struct FilterGraph *graph;
    uint8_t            *name;
} InputFilter;

typedef struct OutputFilter {
    AVFilterContext     *filter;
    struct OutputStream *ost;
    struct FilterGraph  *graph;
    uint8_t             *name;

    /* temporary storage until stream maps are processed */
    AVFilterInOut       *out_tmp;
    enum AVMediaType     type;
} OutputFilter;

typedef struct FilterGraph {
    int            index;
    const char    *graph_desc;

    AVFilterGraph *graph;
    int reconfiguration;

    InputFilter   **inputs;
    int          nb_inputs;
    OutputFilter **outputs;
    int         nb_outputs;
} FilterGraph;

typedef struct InputStream {
    int file_index;
    AVStream *st;
    int discard;             /* true if stream data should be discarded */
    int user_set_discard;
    int decoding_needed;     /* non zero if the packets must be decoded in 'raw_fifo', see DECODING_FOR_* */
#define DECODING_FOR_OST    1
#define DECODING_FOR_FILTER 2

    AVCodecContext *dec_ctx;
    AVCodec *dec;
    AVFrame *decoded_frame;
    AVFrame *filter_frame; /* a ref of decoded_frame, to be sent to filters */

    int64_t       start;     /* time when read started */
    /* predicted dts of the next packet read for this stream or (when there are
     * several frames in a packet) of the next frame in current packet (in AV_TIME_BASE units) */
    int64_t       next_dts;
    int64_t       dts;       ///< dts of the last packet read for this stream (in AV_TIME_BASE units)

    int64_t       next_pts;  ///< synthetic pts for the next decode frame (in AV_TIME_BASE units)
    int64_t       pts;       ///< current pts of the decoded frame  (in AV_TIME_BASE units)
    int           wrap_correction_done;

    int64_t filter_in_rescale_delta_last;

    int64_t min_pts; /* pts with the smallest value in a current stream */
    int64_t max_pts; /* pts with the higher value in a current stream */
    int64_t nb_samples; /* number of samples in the last decoded audio frame before looping */

    double ts_scale;
    int saw_first_ts;
    int showed_multi_packet_warning;
    AVDictionary *decoder_opts;
    AVRational framerate;               /* framerate forced with -r */
    int top_field_first;
    int guess_layout_max;

    int autorotate;
    int resample_height;
    int resample_width;
    int resample_pix_fmt;

    int      resample_sample_fmt;
    int      resample_sample_rate;
    int      resample_channels;
    uint64_t resample_channel_layout;

    int fix_sub_duration;
    struct { /* previous decoded subtitle and related variables */
        int got_output;
        int ret;
        AVSubtitle subtitle;
    } prev_sub;

    struct sub2video {
        int64_t last_pts;
        int64_t end_pts;
        AVFrame *frame;
        int w, h;
    } sub2video;

    int dr1;

    /* decoded data from this stream goes into all those filters
     * currently video and audio only */
    InputFilter **filters;
    int        nb_filters;

    int reinit_filters;

    /* hwaccel options */
    enum HWAccelID hwaccel_id;
    char  *hwaccel_device;
    enum AVPixelFormat hwaccel_output_format;

    /* hwaccel context */
    enum HWAccelID active_hwaccel_id;
    void  *hwaccel_ctx;
    void (*hwaccel_uninit)(AVCodecContext *s);
    int  (*hwaccel_get_buffer)(AVCodecContext *s, AVFrame *frame, int flags);
    int  (*hwaccel_retrieve_data)(AVCodecContext *s, AVFrame *frame);
    enum AVPixelFormat hwaccel_pix_fmt;
    enum AVPixelFormat hwaccel_retrieved_pix_fmt;
    AVBufferRef *hw_frames_ctx;

    /* stats */
    // combined size of all the packets read
    uint64_t data_size;
    /* number of packets successfully read for this stream */
    uint64_t nb_packets;
    // number of frames/samples retrieved from the decoder
    uint64_t frames_decoded;
    uint64_t samples_decoded;
} InputStream;

typedef struct InputFile {
    AVFormatContext *ctx;
    int eof_reached;      /* true if eof reached */
    int eagain;           /* true if last read attempt returned EAGAIN */
    int ist_index;        /* index of first stream in input_streams */
    int loop;             /* set number of times input stream should be looped */
    int64_t duration;     /* actual duration of the longest stream in a file
                             at the moment when looping happens */
    AVRational time_base; /* time base of the duration */
    int64_t input_ts_offset;

    int64_t ts_offset;
    int64_t last_ts;
    int64_t start_time;   /* user-specified start time in AV_TIME_BASE or AV_NOPTS_VALUE */
    int seek_timestamp;
    int64_t recording_time;
    int nb_streams;       /* number of stream that ffmpeg is aware of; may be different
                             from ctx.nb_streams if new streams appear during av_read_frame() */
    int nb_streams_warn;  /* number of streams that the user was warned of */
    int rate_emu;
    int accurate_seek;

#if HAVE_PTHREADS
    AVThreadMessageQueue *in_thread_queue;
    pthread_t thread;           /* thread reading from this file */
    int non_blocking;           /* reading packets from the thread should not block */
    int joined;                 /* the thread has been joined */
    int thread_queue_size;      /* maximum number of queued packets */
#endif
} InputFile;

typedef struct OutputStream {
    int file_index;          /* file index */
    int index;               /* stream index in the output file */
    int source_index;        /* InputStream index */
    AVStream *st;            /* stream in the output file */
    int encoding_needed;     /* true if encoding needed for this stream */
    int frame_number;
    /* input pts and corresponding output pts
       for A/V sync */
    InputStream *sync_ist; /* input stream to sync against */
    int64_t sync_opts;       /* output frame counter, could be changed to some true timestamp */ // FIXME look at frame_number
    /* pts of the first frame encoded for this stream, used for limiting
     * recording time */
    int64_t first_pts;
    /* dts of the last packet sent to the muxer */
    int64_t last_mux_dts;
    AVBitStreamFilterContext *bitstream_filters;

    int                    nb_bitstream_filters;
    uint8_t                  *bsf_extradata_updated;
    AVBSFContext            **bsf_ctx;

    AVCodecContext *enc_ctx;
    AVCodecParameters *ref_par; /* associated input codec parameters with encoders options applied */
    AVCodec *enc;
    int64_t max_frames;
    AVFrame *filtered_frame;
    AVFrame *last_frame;
    int last_dropped;
    int last_nb0_frames[3];

    void  *hwaccel_ctx;

    /* video only */
    AVRational frame_rate;
    int is_cfr;
    int force_fps;
    int top_field_first;
    int rotate_overridden;

    AVRational frame_aspect_ratio;

    /* forced key frames */
    int64_t *forced_kf_pts;
    int forced_kf_count;
    int forced_kf_index;
    char *forced_keyframes;
    AVExpr *forced_keyframes_pexpr;
    double forced_keyframes_expr_const_values[FKF_NB];

    /* audio only */
    int *audio_channels_map;             /* list of the channels id to pick from the source stream */
    int audio_channels_mapped;           /* number of channels in audio_channels_map */

    char *logfile_prefix;
    FILE *logfile;

    OutputFilter *filter;
    char *avfilter;
    char *filters;         ///< filtergraph associated to the -filter option
    char *filters_script;  ///< filtergraph script associated to the -filter_script option

    AVDictionary *encoder_opts;
    AVDictionary *sws_dict;
    AVDictionary *swr_opts;
    AVDictionary *resample_opts;
    char *apad;
    OSTFinished finished;        /* no more packets should be written for this stream */
    int unavailable;                     /* true if the steram is unavailable (possibly temporarily) */
    int stream_copy;
    const char *attachment_filename;
    int copy_initial_nonkeyframes;
    int copy_prior_start;
    char *disposition;

    int keep_pix_fmt;

    AVCodecParserContext *parser;

    /* stats */
    // combined size of all the packets written
    uint64_t data_size;
    // number of packets send to the muxer
    uint64_t packets_written;
    // number of frames/samples sent to the encoder
    uint64_t frames_encoded;
    uint64_t samples_encoded;

    /* packet quality factor */
    int quality;

    /* packet picture type */
    int pict_type;

    /* frame encode sum of squared error values */
    int64_t error[4];
} OutputStream;

typedef struct OutputFile {
    AVFormatContext *ctx;
    AVDictionary *opts;
    int ost_index;       /* index of the first stream in output_streams */
    int64_t recording_time;  ///< desired length of the resulting file in microseconds == AV_TIME_BASE units
    int64_t start_time;      ///< start time in microseconds == AV_TIME_BASE units
    uint64_t limit_filesize; /* filesize limit expressed in bytes */

    int shortest;
} OutputFile;

InputStream **input_streams;
int        nb_input_streams;
InputFile   **input_files;
int        nb_input_files;

OutputStream **output_streams;
int         nb_output_streams;
OutputFile   **output_files;
int         nb_output_files;

public:
  VideoStore(const char *filename_in, const char *format_in, AVStream *video_input_stream, AVStream *audio_input_stream, int64_t nStartTime, Monitor::Orientation p_orientation );
	~VideoStore();

  int writeVideoFramePacket( AVPacket *pkt );
  int writeAudioFramePacket( AVPacket *pkt );
  void dumpPacket( AVPacket *pkt ); 
  void do_streamcopy( InputStream *ist, OutputStream *ost, const AVPacket *pkt );
  void write_packet( AVFormatContext *s, AVPacket *pkt, OutputStream *ost );
  void close_output_stream(OutputStream *ost);
  void output_packet(AVFormatContext *s, AVPacket *pkt, OutputStream *ost);
  void write_frame(AVFormatContext *s, AVPacket *pkt, OutputStream *ost);
  void close_all_output_streams(OutputStream *ost, OSTFinished this_stream, OSTFinished others);
  int process_input_packet(InputStream *ist, const AVPacket *pkt, int no_eof);
  int decode_audio(InputStream *ist, AVPacket *pkt, int *got_output);
  int decode_video(InputStream *ist, AVPacket *pkt, int *got_output);
  int check_output_constraints(InputStream *ist, OutputStream *ost);
  int send_filter_eof(InputStream *ist);
  void update_benchmark(const char *fmt, ...);
  void check_decode_result(InputStream *ist, int *got_output, int ret);
  int guess_input_channel_layout(InputStream *ist);
  int64_t getutime(void);
};

#endif //havelibav
#endif //zm_videostore_h

