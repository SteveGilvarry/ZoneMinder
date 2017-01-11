//
// ZoneMinder Video Storage Implementation
// Written by Chris Wiggins
// http://chriswiggins.co.nz
// Modification by Steve Gilvarry
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

#define __STDC_FORMAT_MACROS 1

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "zm.h"
#include "zm_videostore.h"

extern "C" {
#include "libavutil/time.h"
#include "libavutil/timestamp.h"
}

VideoStore::VideoStore(const char *filename_in, const char *format_in,
    AVStream *p_video_input_stream,
    AVStream *p_audio_input_stream,
    int64_t nStartTime,
    Monitor::Orientation orientation
    ) {
  video_input_stream = p_video_input_stream;
  audio_input_stream = p_audio_input_stream;
  InputStream **input_streams = NULL;
  int nb_input_streams = 0;
  InputFile **input_files = NULL;
  int nb_input_files = 0;

  OutputStream **output_streams = NULL;
  int nb_output_streams = 0;
  OutputFile **output_files = NULL;
  int nb_output_files = 0;

  int copy_ts = 0;
  int video_sync_method = VSYNC_AUTO;
  int audio_sync_method = 0;
  int debug_ts = 0;
  int do_benchmark_all = 0;
  int current_time;

  FilterGraph **filtergraphs;
  int nb_filtergraphs;

  video_input_context = video_input_stream->codec;

  //store inputs in variables local to class
  filename = filename_in;
  format = format_in;

  keyframeMessage = false;
  keyframeSkipNumber = 0;

  Info("Opening video storage stream %s format: %s\n", filename, format);

  int ret;
  static char error_buffer[255];
  //Init everything we need, shouldn't have to do this, ffmpeg_camera or something else will call it.
  //av_register_all();

  ret = avformat_alloc_output_context2(&oc, NULL, NULL, filename);
  if ( ret < 0 ) {
    Warning("Could not create video storage stream %s as no output context"
        " could be assigned based on filename: %s",
        filename,
        av_make_error_string(ret).c_str()
        );
  } else {
    Debug( 2, "Success allocating output context" );
  }

  //Couldn't deduce format from filename, trying from format name
  if (!oc) {
    avformat_alloc_output_context2(&oc, NULL, format, filename);
    if (!oc) {
      Fatal("Could not create video storage stream %s as no output context"
          " could not be assigned based on filename or format %s",
          filename, format);
    }
  } else {
    Debug( 2, "Success allocating output context" );
  }

  AVDictionary *pmetadata = NULL;
  int dsr = av_dict_set(&pmetadata, "title", "Zoneminder Security Recording", 0);
  if (dsr < 0) Warning("%s:%d: title set failed", __FILE__, __LINE__ );

  oc->metadata = pmetadata;

  output_format = oc->oformat;

  video_output_stream = avformat_new_stream(oc, video_input_context->codec);
  if (!video_output_stream) {
    Fatal("Unable to create video out stream\n");
  } else {
    Debug(2, "Success creating video out stream" );
  }

  video_output_context = video_output_stream->codec;

#if LIBAVCODEC_VERSION_CHECK(57, 0, 0, 0, 0)
  Debug(2, "setting parameters");
  ret = avcodec_parameters_to_context( video_output_context, video_input_stream->codecpar );
  if ( ret < 0 ) {
    Error( "Could not initialize stream parameters" );
    return;
  } else {
    Debug(2, "Success getting parameters");
  }
#else
  ret = avcodec_copy_context(video_output_context, video_input_context );
  if (ret < 0) { 
    Fatal("Unable to copy input video context to output video context %s\n", 
        av_make_error_string(ret).c_str());
  } else {
    Debug(3, "Success copying context" );
  }
#endif

  Debug(3, "Time bases: VIDEO input stream (%d/%d) input codec: (%d/%d) output stream: (%d/%d) output codec (%d/%d)", 
        video_input_stream->time_base.num,
        video_input_stream->time_base.den,
        video_input_context->time_base.num,
        video_input_context->time_base.den,
        video_output_stream->time_base.num,
        video_output_stream->time_base.den,
        video_output_context->time_base.num,
        video_output_context->time_base.den
        );

#if 0
  if ( video_input_context->sample_aspect_ratio.den && ( video_output_stream->sample_aspect_ratio.den != video_input_context->sample_aspect_ratio.den ) ) {
	  Warning("Fixing sample_aspect_ratio.den from (%d) to (%d)", video_output_stream->sample_aspect_ratio.den, video_input_context->sample_aspect_ratio.den );
	  video_output_stream->sample_aspect_ratio.den = video_input_context->sample_aspect_ratio.den;
  } else {
    Debug(3, "aspect ratio denominator is (%d)", video_output_stream->sample_aspect_ratio.den  );
  }
  if ( video_input_context->sample_aspect_ratio.num && ( video_output_stream->sample_aspect_ratio.num != video_input_context->sample_aspect_ratio.num ) ) {
	  Warning("Fixing sample_aspect_ratio.num from video_output_stream(%d) to video_input_stream(%d)", video_output_stream->sample_aspect_ratio.num, video_input_context->sample_aspect_ratio.num );
	  video_output_stream->sample_aspect_ratio.num = video_input_context->sample_aspect_ratio.num;
  } else {
    Debug(3, "aspect ratio numerator is (%d)", video_output_stream->sample_aspect_ratio.num  );
  }
  if ( video_output_context->codec_id != video_input_context->codec_id ) {
	  Warning("Fixing video_output_context->codec_id");
	  video_output_context->codec_id = video_input_context->codec_id;
  }
  if ( ! video_output_context->time_base.num ) {
	  Warning("video_output_context->time_base.num is not set%d/%d. Fixing by setting it to 1", video_output_context->time_base.num, video_output_context->time_base.den);	
	  Warning("video_output_context->time_base.num is not set%d/%d. Fixing by setting it to 1", video_output_stream->time_base.num, video_output_stream->time_base.den);	
	  video_output_context->time_base.num = video_output_stream->time_base.num;
	  video_output_context->time_base.den = video_output_stream->time_base.den;
  }

  if ( video_output_stream->sample_aspect_ratio.den != video_output_context->sample_aspect_ratio.den ) {
    Warning( "Fixing sample_aspect_ratio.den" );
         video_output_stream->sample_aspect_ratio.den = video_output_context->sample_aspect_ratio.den;
  }
  if ( video_output_stream->sample_aspect_ratio.num != video_input_context->sample_aspect_ratio.num ) {
    Warning( "Fixing sample_aspect_ratio.num" );
         video_output_stream->sample_aspect_ratio.num = video_input_context->sample_aspect_ratio.num;
  }
  if ( video_output_context->codec_id != video_input_context->codec_id ) {
         Warning("Fixing video_output_context->codec_id");
         video_output_context->codec_id = video_input_context->codec_id;
  }
  if ( ! video_output_context->time_base.num ) {
         Warning("video_output_context->time_base.num is not set%d/%d. Fixing by setting it to 1", video_output_context->time_base.num, video_output_context->time_base.den); 
         Warning("video_output_context->time_base.num is not set%d/%d. Fixing by setting it to 1", video_output_stream->time_base.num, video_output_stream->time_base.den);       
         video_output_context->time_base.num = video_output_stream->time_base.num;
         video_output_context->time_base.den = video_output_stream->time_base.den;
  }
#endif

       // WHY?
  //video_output_context->codec_tag = 0;
  if (!video_output_context->codec_tag) {
    Debug(2, "No codec_tag");
    if (! oc->oformat->codec_tag
        || av_codec_get_id (oc->oformat->codec_tag, video_input_context->codec_tag) == video_output_context->codec_id
        || av_codec_get_tag(oc->oformat->codec_tag, video_input_context->codec_id) <= 0) {
      Warning("Setting codec tag");
      video_output_context->codec_tag = video_input_context->codec_tag;
    }
  }

  if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
    video_output_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  if ( orientation ) {
    if ( orientation == Monitor::ROTATE_0 ) {

    } else if ( orientation == Monitor::ROTATE_90 ) {
      dsr = av_dict_set( &video_output_stream->metadata, "rotate", "90", 0);
      if (dsr < 0) Warning("%s:%d: title set failed", __FILE__, __LINE__ );
    } else if ( orientation == Monitor::ROTATE_180 ) {
      dsr = av_dict_set( &video_output_stream->metadata, "rotate", "180", 0);
      if (dsr < 0) Warning("%s:%d: title set failed", __FILE__, __LINE__ );
    } else if ( orientation == Monitor::ROTATE_270 ) {
      dsr = av_dict_set( &video_output_stream->metadata, "rotate", "270", 0);
      if (dsr < 0) Warning("%s:%d: title set failed", __FILE__, __LINE__ );
    } else {
      Warning( "Unsupported Orientation(%d)", orientation );
    }
  }

  audio_output_codec = NULL;
  audio_input_context = NULL;

  if (audio_input_stream) {
    audio_input_context = audio_input_stream->codec;

    if ( audio_input_context->codec_id != AV_CODEC_ID_AAC ) {
      avcodec_string(error_buffer, sizeof(error_buffer), audio_input_context, 0 );
      Debug(3, "Got something other than AAC (%s)", error_buffer );
      audio_output_stream = NULL;

      audio_output_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
      if ( audio_output_codec ) {
Debug(2, "Have audio output codec");
        audio_output_stream = avformat_new_stream( oc, audio_output_codec );

        audio_output_context = audio_output_stream->codec;

        if ( audio_output_context ) {

Debug(2, "Have audio_output_context");
          AVDictionary *opts = NULL;
          av_dict_set(&opts, "strict", "experimental", 0);

          /* put sample parameters */
          audio_output_context->bit_rate = audio_input_context->bit_rate;
          audio_output_context->sample_rate = audio_input_context->sample_rate;
          audio_output_context->channels = audio_input_context->channels;
          audio_output_context->channel_layout = audio_input_context->channel_layout;
          audio_output_context->sample_fmt = audio_input_context->sample_fmt;
          //audio_output_context->refcounted_frames = 1;

        if (audio_output_codec->supported_samplerates) {
            int found = 0;
            for ( unsigned int i = 0; audio_output_codec->supported_samplerates[i]; i++) {
              if ( audio_output_context->sample_rate == audio_output_codec->supported_samplerates[i] ) {
                found = 1;
                break;
              }
            }
            if ( found ) {
              Debug(3, "Sample rate is good");
            } else {
              audio_output_context->sample_rate = audio_output_codec->supported_samplerates[0];
              Debug(1, "Sampel rate is no good, setting to (%d)", audio_output_codec->supported_samplerates[0] );
            }
        }

        /* check that the encoder supports s16 pcm input */
        if (!check_sample_fmt( audio_output_codec, audio_output_context->sample_fmt)) {
          Error( "Encoder does not support sample format %s, setting to FLTP",
              av_get_sample_fmt_name( audio_output_context->sample_fmt));
          audio_output_context->sample_fmt = AV_SAMPLE_FMT_FLTP;
        }

        Debug(3, "Audio Time bases input stream (%d/%d) input codec: (%d/%d) output_stream (%d/%d) output codec (%d/%d)", 
            audio_input_stream->time_base.num,
            audio_input_stream->time_base.den,
            audio_input_context->time_base.num,
            audio_input_context->time_base.den,
            audio_output_stream->time_base.num,
            audio_output_stream->time_base.den,
            audio_output_context->time_base.num,
            audio_output_context->time_base.den
            );
        /** Set the sample rate for the container. */
        //audio_output_stream->time_base.den = audio_input_context->sample_rate;
        //audio_output_stream->time_base.num = 1;

        ret = avcodec_open2(audio_output_context, audio_output_codec, &opts );
        if ( ret < 0 ) {
          av_strerror(ret, error_buffer, sizeof(error_buffer));
          Fatal( "could not open codec (%d) (%s)\n", ret, error_buffer );
        } else {

          Debug(1, "Audio output bit_rate (%d) sample_rate(%d) channels(%d) fmt(%d) layout(%d) frame_size(%d), refcounted_frames(%d)", 
              audio_output_context->bit_rate,
              audio_output_context->sample_rate,
              audio_output_context->channels,
              audio_output_context->sample_fmt,
              audio_output_context->channel_layout,
              audio_output_context->frame_size,
              audio_output_context->refcounted_frames
              );
#if 1
          /** Create the FIFO buffer based on the specified output sample format. */
          if (!(fifo = av_audio_fifo_alloc(audio_output_context->sample_fmt,
                  audio_output_context->channels, 1))) {
            Error("Could not allocate FIFO\n");
            return;
          }
#endif
          output_frame_size = audio_output_context->frame_size;
          /** Create a new frame to store the audio samples. */
          if (!(input_frame = zm_av_frame_alloc())) {
            Error("Could not allocate input frame");
            return;
          }

          /** Create a new frame to store the audio samples. */
          if (!(output_frame = zm_av_frame_alloc())) {
            Error("Could not allocate output frame");
            av_frame_free(&input_frame);
            return;
          }
          /**
         * Create a resampler context for the conversion.
         * Set the conversion parameters.
         * Default channel layouts based on the number of channels
         * are assumed for simplicity (they are sometimes not detected
         * properly by the demuxer and/or decoder).
         */
        resample_context = swr_alloc_set_opts(NULL,
                                              av_get_default_channel_layout(audio_output_context->channels),
                                              audio_output_context->sample_fmt,
                                              audio_output_context->sample_rate,
                                              av_get_default_channel_layout( audio_input_context->channels),
                                              audio_input_context->sample_fmt,
                                              audio_input_context->sample_rate,
                                              0, NULL);
        if (!resample_context) {
            Error( "Could not allocate resample context\n");
            return;
        }
        /**
        * Perform a sanity check so that the number of converted samples is
        * not greater than the number of samples to be converted.
        * If the sample rates differ, this case has to be handled differently
        */
        av_assert0(audio_output_context->sample_rate == audio_input_context->sample_rate);
        /** Open the resampler with the specified parameters. */
        if ((ret = swr_init(resample_context)) < 0) {
            Error( "Could not open resample context\n");
            swr_free(&resample_context);
            return;
        }
        /**
         * Allocate as many pointers as there are audio channels.
         * Each pointer will later point to the audio samples of the corresponding
         * channels (although it may be NULL for interleaved formats).
         */
        if (!( converted_input_samples = (uint8_t *)calloc( audio_output_context->channels, sizeof(*converted_input_samples))) ) {
          Error( "Could not allocate converted input sample pointers\n");
          return;
        }
        /**
         * Allocate memory for the samples of all channels in one consecutive
         * block for convenience.
         */
        if ((ret = av_samples_alloc( &converted_input_samples, NULL,
                audio_output_context->channels,
                audio_output_context->frame_size,
                audio_output_context->sample_fmt, 0)) < 0) {
          Error( "Could not allocate converted input samples (error '%s')\n",
              av_make_error_string(ret).c_str() );

          av_freep(converted_input_samples);
          free(converted_input_samples);
          return;
        }
        Debug(2, "Success opening AAC codec");
        } 
        av_dict_free(&opts);
        } else {
          Error( "could not allocate codec context for AAC\n");
        }
      } else {
        Error( "could not find codec for AAC\n");
      }

    } else {
      Debug(3, "Got AAC" );

      audio_output_stream = avformat_new_stream(oc, audio_input_context->codec);
      if ( ! audio_output_stream ) {
        Error("Unable to create audio out stream\n");
        audio_output_stream = NULL;
      }
      audio_output_context = audio_output_stream->codec;

      ret = avcodec_copy_context(audio_output_context, audio_input_context);
      if (ret < 0) {
        Fatal("Unable to copy audio context %s\n", av_make_error_string(ret).c_str());
      }   
      audio_output_context->codec_tag = 0;
      if ( audio_output_context->channels > 1 ) {
        Warning("Audio isn't mono, changing it.");
        audio_output_context->channels = 1;
      } else {
        Debug(3, "Audio is mono");
      }
    } // end if is AAC
      if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
        audio_output_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
      }
 Debug(3, "Audio Time bases input stream time base(%d/%d) input codec tb: (%d/%d) video_output_stream->time-base(%d/%d) output codec tb (%d/%d)", 
        audio_input_stream->time_base.num,
        audio_input_stream->time_base.den,
        audio_input_context->time_base.num,
        audio_input_context->time_base.den,
        audio_output_stream->time_base.num,
        audio_output_stream->time_base.den,
        audio_output_context->time_base.num,
        audio_output_context->time_base.den
        );
  } else {
    Debug(3, "No Audio output stream");
    audio_output_stream = NULL;
  }    

  /* open the output file, if needed */
  if (!(output_format->flags & AVFMT_NOFILE)) {
    ret = avio_open2(&oc->pb, filename, AVIO_FLAG_WRITE,NULL,NULL);
    if (ret < 0) {
      Fatal("Could not open output file '%s': %s\n", filename,
          av_make_error_string(ret).c_str());
    }
  }

  //av_dict_set(&opts, "movflags", "frag_custom+dash+delay_moov", 0);
  //if ((ret = avformat_write_header(ctx, &opts)) < 0) {
  //}
  //os->ctx_inited = 1;
  //avio_flush(ctx->pb);
  //av_dict_free(&opts);

  /* Write the stream header, if any. */
  ret = avformat_write_header(oc, NULL);
  if (ret < 0) {
    zm_dump_stream_format( oc, 0, 0, 1 );
    if ( audio_output_stream ) 
      zm_dump_stream_format( oc, 1, 0, 1 );
    Error("Error occurred when writing output file header to %s: %s\n",
        filename,
        av_make_error_string(ret).c_str());
  }

  prevDts = 0;
  video_start_pts = 0;
  video_start_dts = 0;
  audio_start_pts = 0;
  audio_start_dts = 0;

  filter_in_rescale_delta_last = AV_NOPTS_VALUE;

  // now - when streaming started
  //startTime=av_gettime()-nStartTime;//oc->start_time;
  //Info("VideoStore startTime=%d\n",startTime);
} // VideoStore::VideoStore


VideoStore::~VideoStore(){
  /* Write the trailer before close */
  if ( int rc = av_write_trailer(oc) ) {
    Error("Error writing trailer %s",  av_err2str( rc ) );
  } else {
    Debug(3, "Sucess Writing trailer");
  }

  // I wonder if we should be closing the file first.
  // I also wonder if we really need to be doing all the context allocation/de-allocation constantly, or whether we can just re-use it.  Just do a file open/close/writeheader/etc.
  // What if we were only doing audio recording?
  if ( video_output_stream ) {
    avcodec_close(video_output_context);
  }
  if (audio_output_stream) {
    avcodec_close(audio_output_context);
  }

  // WHen will be not using a file ?
  if (!(output_format->flags & AVFMT_NOFILE)) {
    /* Close the output file. */
    if ( int rc = avio_close(oc->pb) ) {
      Error("Error closing avio %s",  av_err2str( rc ) );
    }
  } else {
    Debug(3, "Not closing avio because we are not writing to a file.");
  }

  /* free the stream */
  avformat_free_context(oc);
}


void VideoStore::dumpPacket( AVPacket *pkt ){
  char b[10240];

  snprintf(b, sizeof(b), " pts: %" PRId64 ", dts: %" PRId64 ", data: %p, size: %d, sindex: %d, dflags: %04x, s-pos: %" PRId64 ", c-duration: %" PRId64 "\n"
      , pkt->pts
      , pkt->dts
      , pkt->data
      , pkt->size
      , pkt->stream_index
      , pkt->flags
      , pkt->pos
      , pkt->duration
      );
  Debug(1, "%s:%d:DEBUG: %s", __FILE__, __LINE__, b);
}

int VideoStore::writeVideoFramePacket( AVPacket *ipkt ) {

  AVPacket opkt;
  AVPicture pict;

  Debug(4, "writeVideoFrame init_packet");
  av_init_packet(&opkt);

if ( 1 ) {
  //Scale the PTS of the outgoing packet to be the correct time base
  if (ipkt->pts != AV_NOPTS_VALUE) {
    if ( (!video_start_pts) || (video_start_pts > ipkt->pts) ) {
      Debug(1, "Resetting video_start_pts from (%d) to (%d)",  video_start_pts, ipkt->pts );
      //never gets set, so the first packet can set it.
      video_start_pts = ipkt->pts;
    }
    opkt.pts = av_rescale_q(ipkt->pts - video_start_pts, video_input_stream->time_base, video_output_stream->time_base);
 //- ost_tb_start_time;
    Debug(3, "opkt.pts = %d from ipkt->pts(%d) - startPts(%d)", opkt.pts, ipkt->pts, video_start_pts );
  } else {
    Debug(3, "opkt.pts = undef");
    opkt.pts = AV_NOPTS_VALUE;
  }

  //Scale the DTS of the outgoing packet to be the correct time base
  if(ipkt->dts == AV_NOPTS_VALUE) {
    // why are we using cur_dts instead of packet.dts?
    if ( (!video_start_dts) || (video_start_dts > video_input_stream->cur_dts) ) {
      Debug(1, "Resetting video_start_dts from (%d) to (%d) p.dts was (%d)",  video_start_dts, video_input_stream->cur_dts, ipkt->dts );
      video_start_dts = video_input_stream->cur_dts;
    }
    opkt.dts = av_rescale_q(video_input_stream->cur_dts - video_start_dts, AV_TIME_BASE_Q, video_output_stream->time_base);
    Debug(3, "opkt.dts = %d from video_input_stream->cur_dts(%d) - startDts(%d)", 
        opkt.dts, video_input_stream->cur_dts, video_start_dts
        );
  } else {
    if ( (!video_start_dts) || (video_start_dts > ipkt->dts) ) {
      Debug(1, "Resetting video_start_dts from (%d) to (%d)",  video_start_dts, ipkt->dts );
      video_start_dts = ipkt->dts;
    }
    opkt.dts = av_rescale_q(ipkt->dts - video_start_dts, video_input_stream->time_base, video_output_stream->time_base);
    Debug(3, "opkt.dts = %d from ipkt->dts(%d) - startDts(%d)", opkt.dts, ipkt->dts, video_start_dts );
  }
  if ( opkt.dts > opkt.pts ) {
    Debug( 1, "opkt.dts(%d) must be <= opkt.pts(%d). Decompression must happen before presentation.", opkt.dts, opkt.pts );
    opkt.dts = opkt.pts;
  }

  opkt.duration = av_rescale_q(ipkt->duration, video_input_stream->time_base, video_output_stream->time_base);
} else {
  // Using this results in super fast video output, might be because it should be using the codec time base instead of stream tb
  av_packet_rescale_ts( &opkt, video_input_stream->time_base, video_output_stream->time_base );
}

if ( opkt.dts != AV_NOPTS_VALUE ) {
    int64_t max = video_output_stream->cur_dts + !(oc->oformat->flags & AVFMT_TS_NONSTRICT);
    if (video_output_stream->cur_dts && video_output_stream->cur_dts != AV_NOPTS_VALUE && max > opkt.dts) {
    Warning("st:%d PTS: %"PRId64" DTS: %"PRId64" < %"PRId64" invalid, clipping\n", opkt.stream_index, opkt.pts, opkt.dts, max);
    if( opkt.pts >= opkt.dts)
      opkt.pts = FFMAX(opkt.pts, max);
    opkt.dts = max;
  }
}
  opkt.flags = ipkt->flags;
  opkt.pos=-1;

  opkt.data = ipkt->data;
  opkt.size = ipkt->size;

  // Some camera have audio on stream 0 and video on stream 1.  So when we remove the audio, video stream has to go on 0
  if ( ipkt->stream_index > 0 and ! audio_output_stream ) {
    Debug(1,"Setting stream index to 0 instead of %d", ipkt->stream_index );
    opkt.stream_index = 0;
  } else {
    opkt.stream_index = ipkt->stream_index;
  }

  /*opkt.flags |= AV_PKT_FLAG_KEY;*/

#if 0
  if (video_output_context->codec_type == AVMEDIA_TYPE_VIDEO && (output_format->flags & AVFMT_RAWPICTURE)) {
Debug(3, "video and RAWPICTURE");
    /* store AVPicture in AVPacket, as expected by the output format */
    avpicture_fill(&pict, opkt.data, video_output_context->pix_fmt, video_output_context->width, video_output_context->height, 0);
  av_image_fill_arrays( 
    opkt.data = (uint8_t *)&pict;
    opkt.size = sizeof(AVPicture);
    opkt.flags |= AV_PKT_FLAG_KEY;
   } else {
Debug(4, "Not video and RAWPICTURE");
  }
#endif

  AVPacket safepkt;
  memcpy(&safepkt, &opkt, sizeof(AVPacket));

  if ((opkt.data == NULL)||(opkt.size < 1)) {
    Warning("%s:%d: Mangled AVPacket: discarding frame", __FILE__, __LINE__ ); 
    dumpPacket( ipkt);
    dumpPacket(&opkt);

  } else if ((prevDts > 0) && (prevDts > opkt.dts)) {
    Warning("%s:%d: DTS out of order: %lld \u226E %lld; discarding frame", __FILE__, __LINE__, prevDts, opkt.dts); 
    prevDts = opkt.dts; 
    dumpPacket(&opkt);

  } else {
    int ret;

    prevDts = opkt.dts; // Unsure if av_interleaved_write_frame() clobbers opkt.dts when out of order, so storing in advance
    ret = av_interleaved_write_frame(oc, &opkt);
    if(ret<0){
      // There's nothing we can really do if the frame is rejected, just drop it and get on with the next
      Warning("%s:%d: Writing frame [av_interleaved_write_frame()] failed: %s(%d)  ", __FILE__, __LINE__,  av_make_error_string(ret).c_str(), (ret));
      dumpPacket(&safepkt);
    }
  }

  zm_av_unref_packet(&opkt); 

  return 0;

}

int VideoStore::writeAudioFramePacket( AVPacket *ipkt ) {
  Debug(4, "writeAudioFrame");

  if(!audio_output_stream) {
    Error("Called writeAudioFramePacket when no audio_output_stream");
    return 0;//FIXME -ve return codes do not free packet in ffmpeg_camera at the moment
  }
  /*if(!keyframeMessage)
    return -1;*/
  //zm_dump_stream_format( oc, ipkt->stream_index, 0, 1 );

  int ret;

  AVPacket opkt;

  av_init_packet(&opkt);
  Debug(5, "after init packet" );

#if 1
 //Scale the PTS of the outgoing packet to be the correct time base
  if (ipkt->pts != AV_NOPTS_VALUE) {
    if ( (!audio_start_pts) || ( audio_start_pts > ipkt->pts ) ) {
      Debug(1, "Resetting audeo_start_pts from (%d) to (%d)",  audio_start_pts, ipkt->pts );
      //never gets set, so the first packet can set it.
      audio_start_pts = ipkt->pts;
    }
    opkt.pts = av_rescale_q(ipkt->pts - audio_start_pts, audio_input_stream->time_base, audio_output_stream->time_base);
    Debug(2, "opkt.pts = %d from ipkt->pts(%d) - startPts(%d)", opkt.pts, ipkt->pts, audio_start_pts );
  } else {
    Debug(2, "opkt.pts = undef");
  }

  //Scale the DTS of the outgoing packet to be the correct time base
  if(ipkt->dts == AV_NOPTS_VALUE) {
    if ( (!audio_start_dts) || (audio_start_dts > audio_input_stream->cur_dts ) ) {
      Debug(1, "Resetting audeo_start_pts from (%d) to (%d)",  audio_start_dts, audio_input_stream->cur_dts );
      audio_start_dts = audio_input_stream->cur_dts;
    }
    opkt.dts = av_rescale_q(audio_input_stream->cur_dts - audio_start_dts, AV_TIME_BASE_Q, audio_output_stream->time_base);
    Debug(2, "opkt.dts = %d from video_input_stream->cur_dts(%d) - startDts(%d)",
        opkt.dts, audio_input_stream->cur_dts, audio_start_dts
        );
  } else {
    if ( (!audio_start_dts) || ( audio_start_dts > ipkt->dts ) ) {
      Debug(1, "Resetting audeo_start_dts from (%d) to (%d)",  audio_start_dts, ipkt->dts );
      audio_start_dts = ipkt->dts;
    }
    opkt.dts = av_rescale_q(ipkt->dts - audio_start_dts, audio_input_stream->time_base, audio_output_stream->time_base);
    Debug(2, "opkt.dts = %d from ipkt->dts(%d) - startDts(%d)", opkt.dts, ipkt->dts, audio_start_dts );
  }
  if ( opkt.dts > opkt.pts ) {
    Debug(1,"opkt.dts(%d) must be <= opkt.pts(%d). Decompression must happen before presentation.", opkt.dts, opkt.pts );
    opkt.dts = opkt.pts;
  }
    //opkt.pts = AV_NOPTS_VALUE;
    //opkt.dts = AV_NOPTS_VALUE;

  opkt.duration = av_rescale_q(ipkt->duration, audio_input_stream->time_base, audio_output_stream->time_base);
#else
#endif

  // pkt.pos:  byte position in stream, -1 if unknown 
  opkt.pos = -1;
  opkt.flags = ipkt->flags;
  opkt.stream_index = ipkt->stream_index;
Debug(2, "Stream index is %d", opkt.stream_index );

  if ( audio_output_codec ) {

  // Need to re-encode
#if 0
  ret = avcodec_send_packet( audio_input_context, ipkt );
  if ( ret < 0 ) {
    Error("avcodec_send_packet fail %s", av_make_error_string(ret).c_str());
    return 0;
  }

  ret = avcodec_receive_frame( audio_input_context, input_frame );
  if ( ret < 0 ) {
    Error("avcodec_receive_frame fail %s", av_make_error_string(ret).c_str());
    return 0;
  }
Debug(2, "Frame: samples(%d), format(%d), sample_rate(%d), channel layout(%d) refd(%d)", 
input_frame->nb_samples,
input_frame->format,
input_frame->sample_rate,
input_frame->channel_layout,
audio_output_context->refcounted_frames
);

  ret = avcodec_send_frame( audio_output_context, input_frame );
  if ( ret < 0 ) {
    av_frame_unref( input_frame );
    Error("avcodec_send_frame fail(%d),  %s codec is open(%d) is_encoder(%d)", ret, av_make_error_string(ret).c_str(),
avcodec_is_open( audio_output_context ),
av_codec_is_encoder( audio_output_context->codec)
);
    return 0;
  }
  ret = avcodec_receive_packet( audio_output_context, &opkt );
  if ( ret < 0 ) {
    av_frame_unref( input_frame );
    Error("avcodec_receive_packet fail %s", av_make_error_string(ret).c_str());
    return 0;
  }
  av_frame_unref( input_frame );
#else

  // convert the packet to the codec timebase from the stream timebase
  av_packet_rescale_ts( ipkt, audio_input_stream->time_base, audio_input_context->time_base );

    /**
     * Decode the audio frame stored in the packet.
     * The input audio stream decoder is used to do this.
     * If we are at the end of the file, pass an empty packet to the decoder
     * to flush it.
     */
    if ((ret = avcodec_decode_audio4(audio_input_context, input_frame,
                                       &data_present, ipkt)) < 0) {
        Error( "Could not decode frame (error '%s')\n",
                av_make_error_string(ret).c_str());
        dumpPacket( ipkt );
        av_frame_free(&input_frame);
        zm_av_unref_packet(&opkt);
        return 0;
    }
    if ( ! data_present ) {
      Debug(2, "Not ready to transcode a frame yet.");
      zm_av_unref_packet(&opkt);
      return 0;
    }

    int frame_size = input_frame->nb_samples;
    Debug(4, "Frame size: %d", frame_size );


    Debug(4, "About to convert");

    /** Convert the samples using the resampler. */
    if ((ret = swr_convert(resample_context,
            &converted_input_samples, frame_size,
            (const uint8_t **)input_frame->extended_data    , frame_size)) < 0) {
      Error( "Could not convert input samples (error '%s')\n",
          av_make_error_string(ret).c_str()
          );
      return 0;
    }

    Debug(4, "About to realloc");
    if ((ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
      Error( "Could not reallocate FIFO to %d\n", av_audio_fifo_size(fifo) + frame_size );
      return 0;
    }
    /** Store the new samples in the FIFO buffer. */
    Debug(4, "About to write");
    if (av_audio_fifo_write(fifo, (void **)&converted_input_samples, frame_size) < frame_size) {
      Error( "Could not write data to FIFO\n");
      return 0;
    }

    /** Create a new frame to store the audio samples. */
    if (!(output_frame = zm_av_frame_alloc())) {
      Error("Could not allocate output frame");
      return 0;
    }
    /**
     * Set the frame's parameters, especially its size and format.
     * av_frame_get_buffer needs this to allocate memory for the
     * audio samples of the frame.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity.
     */
    output_frame->nb_samples     = audio_output_context->frame_size;
    output_frame->channel_layout = audio_output_context->channel_layout;
    output_frame->channels       = audio_output_context->channels;
    output_frame->format         = audio_output_context->sample_fmt;
    output_frame->sample_rate    = audio_output_context->sample_rate;
    /**
     * Allocate the samples of the created frame. This call will make
     * sure that the audio frame can hold as many samples as specified.
     */
    Debug(4, "getting buffer");
    if (( ret = av_frame_get_buffer( output_frame, 0)) < 0) {
      Error( "Couldnt allocate output frame buffer samples (error '%s')",
          av_make_error_string(ret).c_str() );
      Error("Frame: samples(%d) layout (%d) format(%d) rate(%d)", output_frame->nb_samples,
          output_frame->channel_layout, output_frame->format , output_frame->sample_rate 
          );
      zm_av_unref_packet(&opkt);
      return 0;
    }

    /** Set a timestamp based on the sample rate for the container. */
    if (output_frame) {
      output_frame->pts = av_frame_get_best_effort_timestamp(output_frame);
    }
    Debug(4, "About to read");
    if (av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size) < frame_size) {
      Error( "Could not read data from FIFO\n");
      return 0;
    }
    /**
     * Encode the audio frame and store it in the temporary packet.
     * The output audio stream encoder is used to do this.
     */
    if (( ret = avcodec_encode_audio2( audio_output_context, &opkt,
            output_frame, &data_present )) < 0) {
      Error( "Could not encode frame (error '%s')",
          av_make_error_string(ret).c_str());
      zm_av_unref_packet(&opkt);
      return 0;
    }
    if ( ! data_present ) {
      Debug(2, "Not ready to output a frame yet.");
      zm_av_unref_packet(&opkt);
      return 0;
    }

    // Convert tb from code back to stream
    av_packet_rescale_ts(&opkt, audio_output_context->time_base, audio_output_stream->time_base);

#endif
  } else {
    opkt.data = ipkt->data;
    opkt.size = ipkt->size;
  }

  AVPacket safepkt;
  memcpy(&safepkt, &opkt, sizeof(AVPacket));
  ret = av_interleaved_write_frame(oc, &opkt);
  if(ret!=0){
    Error("Error writing audio frame packet: %s\n", av_make_error_string(ret).c_str());
    dumpPacket(&safepkt);
  } else {
    Debug(2,"Success writing audio frame" ); 
  }
  zm_av_unref_packet(&opkt);
  return 0;
}

void VideoStore::do_streamcopy(InputStream *ist, OutputStream *ost, const AVPacket *pkt) {
                        OutputFile *of = output_files[ost->file_index];
                        InputFile *f = input_files [ist->file_index];
                        int64_t start_time = (of->start_time == AV_NOPTS_VALUE) ? 0 : of->start_time;
                        int64_t ost_tb_start_time = av_rescale_q( start_time, AV_TIME_BASE_Q, ost->st->time_base );
                        AVPicture pict;
                        AVPacket opkt;

                        av_init_packet( &opkt );

                        if ((!ost->frame_number && !(pkt->flags & AV_PKT_FLAG_KEY)) &&
                            !ost->copy_initial_nonkeyframes)
                        return;

                        if (!ost->frame_number && !ost->copy_prior_start) {
                        int64_t comp_start = start_time;
                        if (copy_ts && f->start_time != AV_NOPTS_VALUE)
                        comp_start = FFMAX( start_time, f->start_time + f->ts_offset );
                        if (pkt->pts == AV_NOPTS_VALUE ?
                            ist->pts < comp_start :
                            pkt->pts < av_rescale_q( comp_start, AV_TIME_BASE_Q, ist->st->time_base ))
                        return;
        }

                        if (of->recording_time != INT64_MAX &&
                            ist->pts >= of->recording_time + start_time) {
                        close_output_stream( ost );
                        return;
  }

                        if (f->recording_time != INT64_MAX) {
                        start_time = f->ctx->start_time;
                        if (f->start_time != AV_NOPTS_VALUE && copy_ts)
                        start_time += f->start_time;
                        if (ist->pts >= f->recording_time + start_time) {
                        close_output_stream( ost );
                        return;
      }
  }

                        /* force the input stream PTS */
                        if (ost->enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
                        ost->sync_opts++;

                        if (pkt->pts != AV_NOPTS_VALUE)
                        opkt.pts = av_rescale_q( pkt->pts, ist->st->time_base, ost->st->time_base ) - ost_tb_start_time;
    else
                        opkt.pts = AV_NOPTS_VALUE;

                        if (pkt->dts == AV_NOPTS_VALUE)
                        opkt.dts = av_rescale_q( ist->dts, AV_TIME_BASE_Q, ost->st->time_base );
    else
                        opkt.dts = av_rescale_q( pkt->dts, ist->st->time_base, ost->st->time_base );
                        opkt.dts -= ost_tb_start_time;

                        if (ost->st->codec->codec_type == AVMEDIA_TYPE_AUDIO && pkt->dts != AV_NOPTS_VALUE) {
                        int duration = av_get_audio_frame_duration( ist->dec_ctx, pkt->size );
                        if (!duration)
                        duration = ist->dec_ctx->frame_size;
                        opkt.dts = opkt.pts = av_rescale_delta( ist->st->time_base, pkt->dts,
                                                                (AVRational) {
                          1, ist->dec_ctx->sample_rate
                        }, duration, &ist->filter_in_rescale_delta_last,
                                                                ost->st->time_base ) - ost_tb_start_time;
    }

                        opkt.duration = av_rescale_q( pkt->duration, ist->st->time_base, ost->st->time_base );
                        opkt.flags = pkt->flags;
                        // FIXME remove the following 2 lines they shall be replaced by the bitstream filters
                        if (ost->st->codec->codec_id != AV_CODEC_ID_H264
                            && ost->st->codec->codec_id != AV_CODEC_ID_MPEG1VIDEO
                            && ost->st->codec->codec_id != AV_CODEC_ID_MPEG2VIDEO
                            && ost->st->codec->codec_id != AV_CODEC_ID_VC1
                            ) {
                        int ret = av_parser_change( ost->parser, ost->st->codec,
                                                    &opkt.data, &opkt.size,
                                                    pkt->data, pkt->size,
                                                    pkt->flags & AV_PKT_FLAG_KEY );
                        if (ret < 0) {
                        av_log( NULL, AV_LOG_FATAL, "av_parser_change failed: %s\n",
                                av_err2str( ret ) );
                        Fatal( "av_parser_change failed" );
    }
                        if (ret) {
                        opkt.buf = av_buffer_create( opkt.data, opkt.size, av_buffer_default_free, NULL, 0 );
                        if (!opkt.buf)
                        Fatal( "av_buffer_create failed" );
      }
  } else {
                        opkt.data = pkt->data;
                        opkt.size = pkt->size;
  }
                        av_copy_packet_side_data( &opkt, pkt );

#if FF_API_LAVF_FMT_RAWPICTURE
      if (ost->st->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
          ost->st->codec->codec_id == AV_CODEC_ID_RAWVIDEO &&
          (of->ctx->oformat->flags & AVFMT_RAWPICTURE)) {
                        /* store AVPicture in AVPacket, as expected by the output format */
                        int ret = avpicture_fill( &pict, opkt.data, ost->st->codec->pix_fmt, ost->st->codec->width, ost->st->codec->height );
                        if (ret < 0) {

                        av_log( NULL, AV_LOG_FATAL, "avpicture_fill failed: %s\n",
                                av_err2str( ret ) );
                        Fatal( "avpicture_fill failed" );
    }
                        opkt.data = (uint8_t *) & pict;
                        opkt.size = sizeof (AVPicture);
                        opkt.flags |= AV_PKT_FLAG_KEY;
  }
#endif

                        write_frame( of->ctx, &opkt, ost );
}

void VideoStore::close_output_stream(OutputStream *ost) {
                        OutputFile *of = output_files[ost->file_index];

                        ost->finished == ENCODER_FINISHED;
                        if (of->shortest) {

                        int64_t end = av_rescale_q( ost->sync_opts - ost->first_pts, ost->enc_ctx->time_base, AV_TIME_BASE_Q );
                        of->recording_time = FFMIN( of->recording_time, end );
  }
}
void VideoStore::output_packet(AVFormatContext *s, AVPacket *pkt, OutputStream *ost) {
                        int ret = 0;

                        /* apply the output bitstream filters, if any */
                        if (ost->nb_bitstream_filters) {
                        int idx;

                        ret = av_bsf_send_packet( ost->bsf_ctx[0], pkt );
                        if (ret < 0)
                        goto finish;

                        idx = 1;
                        while (idx) {
                        /* get a packet from the previous filter up the chain */
                        ret = av_bsf_receive_packet( ost->bsf_ctx[idx - 1], pkt );
                        /* HACK! - aac_adtstoasc updates extradata after filtering the first frame when
             * the api states this shouldn't happen after init(). Propagate it here to the
             * muxer and to the next filters in the chain to workaround this.
             * TODO/FIXME - Make aac_adtstoasc use new packet side data instead of changing
             * par_out->extradata and adapt muxers accordingly to get rid of this. */
                        if (!(ost->bsf_extradata_updated[idx - 1] & 1)) {
                        ret = avcodec_parameters_copy( ost->st->codecpar, ost->bsf_ctx[idx - 1]->par_out );
                        if (ret < 0)
                        goto finish;
                        ost->bsf_extradata_updated[idx - 1] |= 1;
          }
                        if (ret == AVERROR( EAGAIN )) {
                        ret = 0;
                        idx--;
                        continue;
        } else if (ret < 0)
                        goto finish;

                        /* send it to the next filter down the chain or to the muxer */
                        if (idx < ost->nb_bitstream_filters) {
                        /* HACK/FIXME! - See above */
                        if (!(ost->bsf_extradata_updated[idx] & 2)) {
                        ret = avcodec_parameters_copy( ost->bsf_ctx[idx]->par_out, ost->bsf_ctx[idx - 1]->par_out );
                        if (ret < 0)
                        goto finish;
                        ost->bsf_extradata_updated[idx] |= 2;
              }
                        ret = av_bsf_send_packet( ost->bsf_ctx[idx], pkt );
                        if (ret < 0)
                        goto finish;
                        idx++;
            } else
                        write_packet( s, pkt, ost );
          }
  } else
                        write_packet( s, pkt, ost );

                        finish :
                        if (ret < 0 && ret != AVERROR_EOF) {
                        av_log( NULL, AV_LOG_ERROR, "Error applying bitstream filters to an output "
                                "packet for stream #%d:%d.\n", ost->file_index, ost->index );
    }
}
void VideoStore::write_packet(AVFormatContext *s, AVPacket *pkt, OutputStream *ost) {
                        AVStream *st = ost->st;
                        int ret;

                        if ((st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_sync_method == VSYNC_DROP) ||
                            (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_sync_method < 0))
                        pkt->pts = pkt->dts = AV_NOPTS_VALUE;

                        /*
       * Audio encoders may split the packets --  #frames in != #packets out.
       * But there is no reordering, so we can limit the number of output packets
       * by simply dropping them here.
       * Counting encoded video frames needs to be done separately because of
       * reordering, see do_video_out()
       */
                        if (!(st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && ost->encoding_needed)) {
                        if (ost->frame_number >= ost->max_frames) {
                        av_packet_unref( pkt );
                        return;
      }
                        ost->frame_number++;
    }
                        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                        int i;
                        uint8_t *sd = av_packet_get_side_data( pkt, AV_PKT_DATA_QUALITY_STATS,
                                                               NULL );
                        ost->quality = sd ? AV_RL32( sd ) : -1;
                        ost->pict_type = sd ? sd[4] : AV_PICTURE_TYPE_NONE;

                        for (i = 0; i < FF_ARRAY_ELEMS( ost->error ); i++) {
                        if (sd && i < sd[5])
                        ost->error[i] = AV_RL64( sd + 8 + 8 * i );
      else
                        ost->error[i] = -1;
      }

                        if (ost->frame_rate.num && ost->is_cfr) {
                        if (pkt->duration > 0)
                        av_log( NULL, AV_LOG_WARNING, "Overriding packet duration by frame rate, this should not happen\n" );
                        pkt->duration = av_rescale_q( 1, av_inv_q( ost->frame_rate ),
                                                      ost->st->time_base );
      }
  }

                        if (!(s->oformat->flags & AVFMT_NOTIMESTAMPS)) {
                        if (pkt->dts != AV_NOPTS_VALUE &&
                            pkt->pts != AV_NOPTS_VALUE &&
                            pkt->dts > pkt->pts) {
                        av_log( s, AV_LOG_WARNING, "Invalid DTS: %"PRId64" PTS: %"PRId64" in output stream %d:%d, replacing by guess\n",
                                pkt->dts, pkt->pts,
                                ost->file_index, ost->st->index );
                        pkt->pts =
                        pkt->dts = pkt->pts + pkt->dts + ost->last_mux_dts + 1
                        - FFMIN3( pkt->pts, pkt->dts, ost->last_mux_dts + 1 )
                        - FFMAX3( pkt->pts, pkt->dts, ost->last_mux_dts + 1 );
    }
                        if ((st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO || st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) &&
                            pkt->dts != AV_NOPTS_VALUE &&
                            !(st->codecpar->codec_id == AV_CODEC_ID_VP9 && ost->stream_copy) &&
                            ost->last_mux_dts != AV_NOPTS_VALUE) {
                        int64_t max = ost->last_mux_dts + !(s->oformat->flags & AVFMT_TS_NONSTRICT);
                        if (pkt->dts < max) {
                        int loglevel = max - pkt->dts > 2 || st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ? AV_LOG_WARNING : AV_LOG_DEBUG;
                        av_log( s, loglevel, "Non-monotonous DTS in output stream "
                                "%d:%d; previous: %"PRId64", current: %"PRId64"; ",
                                ost->file_index, ost->st->index, ost->last_mux_dts, pkt->dts );

                        av_log( s, loglevel, "changing to %"PRId64". This may result "
                                "in incorrect timestamps in the output file.\n",
                                max );
                        if (pkt->pts >= pkt->dts)
                        pkt->pts = FFMAX( pkt->pts, max );
                        pkt->dts = max;
        }
    }
  }
                        ost->last_mux_dts = pkt->dts;

                        ost->data_size += pkt->size;
                        ost->packets_written++;

                        pkt->stream_index = ost->index;

                        if (debug_ts) {
                        av_log( NULL, AV_LOG_INFO, "muxer <- type:%s pkt_pts:%s pkt_pts_time:%s pkt_dts:%s pkt_dts_time:%s size:%d\n",
                                av_get_media_type_string( ost->enc_ctx->codec_type ),
                                av_ts2str( pkt->pts ),
                                av_ts2timestr( pkt->pts, &ost->st->time_base ),
                                av_ts2str( pkt->dts ),
                                av_ts2timestr( pkt->dts, &ost->st->time_base ),
                                pkt->size
                                );
  }

                        ret = av_interleaved_write_frame( s, pkt );
                        if (ret < 0) {
                        Warning( "%s:%d: Writing frame [av_interleaved_write_frame()] failed: %s(%d)  ", __FILE__, __LINE__, av_make_error_string( ret ).c_str( ), (ret) );
                        close_all_output_streams( ost, MUXER_FINISHED | ENCODER_FINISHED, ENCODER_FINISHED );
  }
                        av_packet_unref( pkt );
}

void VideoStore::close_all_output_streams(OutputStream *ost, OSTFinished this_stream, OSTFinished others) {
                        int i;
                        for (i = 0; i < nb_output_streams; i++) {
                        OutputStream *ost2 = output_streams[i];
                        ost2->finished == ost == ost2 ? this_stream : others;
  }
}

void VideoStore::write_frame(AVFormatContext *s, AVPacket *pkt, OutputStream *ost) {
                        AVBitStreamFilterContext *bsfc = ost->bitstream_filters;
                        AVCodecContext *avctx = ost->encoding_needed ? ost->enc_ctx : ost->st->codec;
                        int ret;

                        if (!ost->st->codec->extradata_size && ost->enc_ctx->extradata_size) {
                        ost->st->codec->extradata = av_mallocz( ost->enc_ctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE );
                        if (ost->st->codec->extradata) {
                        memcpy( ost->st->codec->extradata, ost->enc_ctx->extradata, ost->enc_ctx->extradata_size );
                        ost->st->codec->extradata_size = ost->enc_ctx->extradata_size;
    }
  }

                        if ((avctx->codec_type == AVMEDIA_TYPE_VIDEO && video_sync_method == VSYNC_DROP) ||
                            (avctx->codec_type == AVMEDIA_TYPE_AUDIO && audio_sync_method < 0))
                        pkt->pts = pkt->dts = AV_NOPTS_VALUE;

                        /*
       * Audio encoders may split the packets --  #frames in != #packets out.
       * But there is no reordering, so we can limit the number of output packets
       * by simply dropping them here.
       * Counting encoded video frames needs to be done separately because of
       * reordering, see do_video_out()
       */
                        if (!(avctx->codec_type == AVMEDIA_TYPE_VIDEO && avctx->codec)) {
                        if (ost->frame_number >= ost->max_frames) {
                        av_packet_unref( pkt );
                        return;
      }
                        ost->frame_number++;
    }
                        if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                        int i;
                        uint8_t *sd = av_packet_get_side_data( pkt, AV_PKT_DATA_QUALITY_STATS,
                                                               NULL );
                        ost->quality = sd ? AV_RL32( sd ) : -1;
                        ost->pict_type = sd ? sd[4] : AV_PICTURE_TYPE_NONE;

                        for (i = 0; i < FF_ARRAY_ELEMS( ost->error ); i++) {
                        if (sd && i < sd[5])
                        ost->error[i] = AV_RL64( sd + 8 + 8 * i );
      else
                        ost->error[i] = -1;
      }

                        if (ost->frame_rate.num && ost->is_cfr) {
                        if (pkt->duration > 0)
                        av_log( NULL, AV_LOG_WARNING, "Overriding packet duration by frame rate, this should not happen\n" );
                        pkt->duration = av_rescale_q( 1, av_inv_q( ost->frame_rate ),
                                                      ost->st->time_base );
      }
  }

                        if (bsfc)
                        av_packet_split_side_data( pkt );

                        if ((ret = av_apply_bitstream_filters( avctx, pkt, bsfc )) < 0) {
                        Warning( "%s:%d: Applying bitstream filters [av_apply_bitstream_filters()] failed: %s(%d)  ", __FILE__, __LINE__, av_make_error_string( ret ).c_str( ), (ret) );
      }
                        if (pkt->size == 0 && pkt->side_data_elems == 0)
                        return;
                        if (!ost->st->codecpar->extradata && avctx->extradata) {
                        ost->st->codecpar->extradata = av_malloc( avctx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE );
                        if (!ost->st->codecpar->extradata) {
                        av_log( NULL, AV_LOG_ERROR, "Could not allocate extradata buffer to copy parser data.\n" );
      }
                        ost->st->codecpar->extradata_size = avctx->extradata_size;
                        memcpy( ost->st->codecpar->extradata, avctx->extradata, avctx->extradata_size );
    }

                        if (!(s->oformat->flags & AVFMT_NOTIMESTAMPS)) {
                        if (pkt->dts != AV_NOPTS_VALUE &&
                            pkt->pts != AV_NOPTS_VALUE &&
                            pkt->dts > pkt->pts) {
                        av_log( s, AV_LOG_WARNING, "Invalid DTS: %"PRId64" PTS: %"PRId64" in output stream %d:%d, replacing by guess\n",
                                pkt->dts, pkt->pts,
                                ost->file_index, ost->st->index );
                        pkt->pts =
                        pkt->dts = pkt->pts + pkt->dts + ost->last_mux_dts + 1
                        - FFMIN3( pkt->pts, pkt->dts, ost->last_mux_dts + 1 )
                        - FFMAX3( pkt->pts, pkt->dts, ost->last_mux_dts + 1 );
    }
                        if (
                            (avctx->codec_type == AVMEDIA_TYPE_AUDIO || avctx->codec_type == AVMEDIA_TYPE_VIDEO) &&
                            pkt->dts != AV_NOPTS_VALUE &&
                            !(avctx->codec_id == AV_CODEC_ID_VP9 && ost->stream_copy) &&
                            ost->last_mux_dts != AV_NOPTS_VALUE) {
                        int64_t max = ost->last_mux_dts + !(s->oformat->flags & AVFMT_TS_NONSTRICT);
                        if (pkt->dts < max) {
                        int loglevel = max - pkt->dts > 2 || avctx->codec_type == AVMEDIA_TYPE_VIDEO ? AV_LOG_WARNING : AV_LOG_DEBUG;
                        av_log( s, loglevel, "Non-monotonous DTS in output stream "
                                "%d:%d; previous: %"PRId64", current: %"PRId64"; ",
                                ost->file_index, ost->st->index, ost->last_mux_dts, pkt->dts );

                        av_log( s, loglevel, "changing to %"PRId64". This may result "
                                "in incorrect timestamps in the output file.\n",
                                max );
                        if (pkt->pts >= pkt->dts)
                        pkt->pts = FFMAX( pkt->pts, max );
                        pkt->dts = max;
        }
    }
  }
                        ost->last_mux_dts = pkt->dts;

                        ost->data_size += pkt->size;
                        ost->packets_written++;

                        pkt->stream_index = ost->index;

                        if (debug_ts) {
                        av_log( NULL, AV_LOG_INFO, "muxer <- type:%s "
                                "pkt_pts:%s pkt_pts_time:%s pkt_dts:%s pkt_dts_time:%s size:%d\n",
                                av_get_media_type_string( ost->enc_ctx->codec_type ),
                                av_ts2str( pkt->pts ), av_ts2timestr( pkt->pts, &ost->st->time_base ),
                                av_ts2str( pkt->dts ), av_ts2timestr( pkt->dts, &ost->st->time_base ),
                                pkt->size
                                );
  }

                        ret = av_interleaved_write_frame( s, pkt );
                        if (ret < 0) {
                        Warning( "%s:%d: Writing frame [av_interleaved_write_frame()] failed: %s(%d)  ", __FILE__, __LINE__, av_make_error_string( ret ).c_str( ), (ret) );
                        close_all_output_streams( ost, MUXER_FINISHED | ENCODER_FINISHED, ENCODER_FINISHED );
  }
                        av_packet_unref( pkt );
}
int VideoStore::process_input_packet(InputStream *ist, const AVPacket *pkt, int no_eof) {
                        int ret = 0, i;
                        int got_output = 0;

                        AVPacket avpkt;
                        if (!ist->saw_first_ts) {
                        ist->dts = ist->st->avg_frame_rate.num ? - ist->dec_ctx->has_b_frames * AV_TIME_BASE / av_q2d( ist->st->avg_frame_rate ) : 0;
                        ist->pts = 0;
                        if (pkt && pkt->pts != AV_NOPTS_VALUE && !ist->decoding_needed) {
                        ist->dts += av_rescale_q( pkt->pts, ist->st->time_base, AV_TIME_BASE_Q );
                        ist->pts = ist->dts; //unused but better to set it to a value thats not totally wrong
    }
                        ist->saw_first_ts = 1;
  }

                        if (ist->next_dts == AV_NOPTS_VALUE)
                        ist->next_dts = ist->dts;
                        if (ist->next_pts == AV_NOPTS_VALUE)
                        ist->next_pts = ist->pts;

                        if (!pkt) {
                        /* EOF handling */
                        av_init_packet( &avpkt );
                        avpkt.data = NULL;
                        avpkt.size = 0;
                        goto handle_eof;
      } else {
                        avpkt = *pkt;
      }

                        if (pkt->dts != AV_NOPTS_VALUE) {
                        ist->next_dts = ist->dts = av_rescale_q( pkt->dts, ist->st->time_base, AV_TIME_BASE_Q );
                        if (ist->dec_ctx->codec_type != AVMEDIA_TYPE_VIDEO || !ist->decoding_needed)
                        ist->next_pts = ist->pts = ist->dts;
    }

                        // while we have more to decode or while the decoder did output something on EOF
                        while (ist->decoding_needed && (avpkt.size > 0 || (!pkt && got_output))) {
                        int duration;
                        handle_eof :

                        ist->pts = ist->next_pts;
                        ist->dts = ist->next_dts;

                        if (avpkt.size && avpkt.size != pkt->size &&
                            !(ist->dec->capabilities & AV_CODEC_CAP_SUBFRAMES)) {
                        av_log( NULL, ist->showed_multi_packet_warning ? AV_LOG_VERBOSE : AV_LOG_WARNING,
                                "Multiple frames in a packet from stream %d\n", pkt->stream_index );
                        ist->showed_multi_packet_warning = 1;
    }

                        switch (ist->dec_ctx->codec_type) {
                        case AVMEDIA_TYPE_AUDIO:
                        ret = decode_audio( ist, &avpkt, &got_output );
                        break;
                        case AVMEDIA_TYPE_VIDEO:
                        ret = decode_video( ist, &avpkt, &got_output );
                        if (avpkt.duration) {
                        duration = av_rescale_q( avpkt.duration, ist->st->time_base, AV_TIME_BASE_Q );
        } else if (ist->dec_ctx->framerate.num != 0 && ist->dec_ctx->framerate.den != 0) {
                        int ticks = av_stream_get_parser( ist->st ) ? av_stream_get_parser( ist->st )->repeat_pict + 1 : ist->dec_ctx->ticks_per_frame;
                        duration = ((int64_t) AV_TIME_BASE *
                                    ist->dec_ctx->framerate.den * ticks) /
                        ist->dec_ctx->framerate.num / ist->dec_ctx->ticks_per_frame;
        } else
                        duration = 0;

                        if (ist->dts != AV_NOPTS_VALUE && duration) {
                        ist->next_dts += duration;
        } else
                        ist->next_dts = AV_NOPTS_VALUE;

                        if (got_output)
                        ist->next_pts += duration; //FIXME the duration is not correct in some cases
                        break;
                        case AVMEDIA_TYPE_SUBTITLE:
                        //Do cameras ever include subtitles??
                        //                        ret = transcode_subtitles( ist, &avpkt, &got_output );
                        break;
                        default:
                        return -1;
        }

                        if (ret < 0) {
                        av_log( NULL, AV_LOG_ERROR, "Error while decoding stream #%d:%d: %s\n",
                                ist->file_index, ist->st->index, av_err2str( ret ) );
      }

                        avpkt.dts =
                        avpkt.pts = AV_NOPTS_VALUE;

                        // touch data and size only if not EOF
                        if (pkt) {
                        if (ist->dec_ctx->codec_type != AVMEDIA_TYPE_AUDIO)
                        ret = avpkt.size;
                        avpkt.data += ret;
                        avpkt.size -= ret;
      }
                        if (!got_output) {
                        continue;
    }
                        if (got_output && !pkt)
                        break;
    }

                        /* after flushing, send an EOF on all the filter inputs attached to the stream */
                        /* except when looping we need to flush but not to send an EOF */
                        if (!pkt && ist->decoding_needed && !got_output && !no_eof) {
                        int ret = send_filter_eof( ist );
                        if (ret < 0) {
                        av_log( NULL, AV_LOG_FATAL, "Error marking filters as finished\n" );
    }
  }

                        /* handle stream copy */
                        if (!ist->decoding_needed) {
                        ist->dts = ist->next_dts;
                        switch (ist->dec_ctx->codec_type) {
                        case AVMEDIA_TYPE_AUDIO:
                        ist->next_dts += ((int64_t) AV_TIME_BASE * ist->dec_ctx->frame_size) /
                        ist->dec_ctx->sample_rate;
                        break;
                        case AVMEDIA_TYPE_VIDEO:
                        if (ist->framerate.num) {
                        // TODO: Remove work-around for c99-to-c89 issue 7
                        AVRational time_base_q = AV_TIME_BASE_Q;
                        int64_t next_dts = av_rescale_q( ist->next_dts, time_base_q, av_inv_q( ist->framerate ) );
                        ist->next_dts = av_rescale_q( next_dts + 1, av_inv_q( ist->framerate ), time_base_q );
        } else if (pkt->duration) {
                        ist->next_dts += av_rescale_q( pkt->duration, ist->st->time_base, AV_TIME_BASE_Q );
        } else if (ist->dec_ctx->framerate.num != 0) {
                        int ticks = av_stream_get_parser( ist->st ) ? av_stream_get_parser( ist->st )->repeat_pict + 1 : ist->dec_ctx->ticks_per_frame;
                        ist->next_dts += ((int64_t) AV_TIME_BASE *
                                          ist->dec_ctx->framerate.den * ticks) /
                        ist->dec_ctx->framerate.num / ist->dec_ctx->ticks_per_frame;
        }
                        break;
    }
                        ist->pts = ist->dts;
                        ist->next_pts = ist->next_dts;
  }
                        for (i = 0; pkt && i < nb_output_streams; i++) {
                        OutputStream *ost = output_streams[i];

                        if (!check_output_constraints( ist, ost ) || ost->encoding_needed)
                        continue;

                        do_streamcopy( ist, ost, pkt );
    }

                        return got_output;
}

int VideoStore::decode_audio(InputStream *ist, AVPacket *pkt, int *got_output) {
                        AVFrame *decoded_frame, *f;
                        AVCodecContext *avctx = ist->dec_ctx;
                        int i, ret, err = 0, resample_changed;
                        AVRational decoded_frame_tb;

                        if (!ist->decoded_frame && !(ist->decoded_frame = av_frame_alloc( )))
                        return AVERROR( ENOMEM );
                        if (!ist->filter_frame && !(ist->filter_frame = av_frame_alloc( )))
                        return AVERROR( ENOMEM );
                        decoded_frame = ist->decoded_frame;

                        update_benchmark( NULL );
                        ret = avcodec_decode_audio4( avctx, decoded_frame, got_output, pkt );
                        update_benchmark( "decode_audio %d.%d", ist->file_index, ist->st->index );

                        if (ret >= 0 && avctx->sample_rate <= 0) {
                        av_log( avctx, AV_LOG_ERROR, "Sample rate %d invalid\n", avctx->sample_rate );
                        ret = AVERROR_INVALIDDATA;
      }

                        check_decode_result( ist, got_output, ret );

                        if (!*got_output || ret < 0)
                        return ret;

                        ist->samples_decoded += decoded_frame->nb_samples;
                        ist->frames_decoded++;

#if 1
      /* increment next_dts to use for the case where the input stream does not
         have timestamps or there are multiple frames in the packet */
                        ist->next_pts += ((int64_t) AV_TIME_BASE * decoded_frame->nb_samples) /
                        avctx->sample_rate;
                        ist->next_dts += ((int64_t) AV_TIME_BASE * decoded_frame->nb_samples) /
                        avctx->sample_rate;
#endif

                        resample_changed = ist->resample_sample_fmt != decoded_frame->format ||
                        ist->resample_channels != avctx->channels ||
                        ist->resample_channel_layout != decoded_frame->channel_layout ||
                        ist->resample_sample_rate != decoded_frame->sample_rate;
                        if (resample_changed) {
                        char layout1[64], layout2[64];

                        if (!guess_input_channel_layout( ist )) {
                        av_log( NULL, AV_LOG_FATAL, "Unable to find default channel "
                                "layout for Input Stream #%d.%d\n", ist->file_index,
                                ist->st->index );
      }
                        decoded_frame->channel_layout = avctx->channel_layout;

                        av_get_channel_layout_string( layout1, sizeof (layout1), ist->resample_channels,
                                                      ist->resample_channel_layout );
                        av_get_channel_layout_string( layout2, sizeof (layout2), avctx->channels,
                                                      decoded_frame->channel_layout );

                        av_log( NULL, AV_LOG_INFO,
                                "Input stream #%d:%d frame changed from rate:%d fmt:%s ch:%d chl:%s to rate:%d fmt:%s ch:%d chl:%s\n",
                                ist->file_index, ist->st->index,
                                ist->resample_sample_rate, av_get_sample_fmt_name( ist->resample_sample_fmt ),
                                ist->resample_channels, layout1,
                                decoded_frame->sample_rate, av_get_sample_fmt_name( decoded_frame->format ),
                                avctx->channels, layout2 );

                        ist->resample_sample_fmt = decoded_frame->format;
                        ist->resample_sample_rate = decoded_frame->sample_rate;
                        ist->resample_channel_layout = decoded_frame->channel_layout;
                        ist->resample_channels = avctx->channels;

                        //                        for (i = 0; i < nb_filtergraphs; i++)
                        //                        if (ist_in_filtergraph( filtergraphs[i], ist )) {
                        //                        FilterGraph *fg = filtergraphs[i];
                        //                        if (configure_filtergraph( fg ) < 0) {
                        //                        av_log( NULL, AV_LOG_FATAL, "Error reinitializing filters!\n" );
                        //          }
                        //        }
    }

                        /* if the decoder provides a pts, use it instead of the last packet pts.
     the decoder could be delaying output by a packet or more. */
                        if (decoded_frame->pts != AV_NOPTS_VALUE) {
                        ist->dts = ist->next_dts = ist->pts = ist->next_pts = av_rescale_q( decoded_frame->pts, avctx->time_base, AV_TIME_BASE_Q );
                        decoded_frame_tb = avctx->time_base;
  } else if (decoded_frame->pkt_pts != AV_NOPTS_VALUE) {
                        decoded_frame->pts = decoded_frame->pkt_pts;
                        decoded_frame_tb = ist->st->time_base;
  } else if (pkt->pts != AV_NOPTS_VALUE) {
                        decoded_frame->pts = pkt->pts;
                        decoded_frame_tb = ist->st->time_base;
  } else {
                        decoded_frame->pts = ist->dts;
                        decoded_frame_tb = AV_TIME_BASE_Q;
  }
                        pkt->pts = AV_NOPTS_VALUE;
                        if (decoded_frame->pts != AV_NOPTS_VALUE)
                        decoded_frame->pts = av_rescale_delta( decoded_frame_tb, decoded_frame->pts,
                                                               (AVRational) {
                          1, avctx->sample_rate
                        }, decoded_frame->nb_samples, &ist->filter_in_rescale_delta_last,
                                                               (AVRational) {
                                                                 1, avctx->sample_rate
                                                               } );
                        ist->nb_samples = decoded_frame->nb_samples;
                        for (i = 0; i < ist->nb_filters; i++) {
                        if (i < ist->nb_filters - 1) {
                        f = ist->filter_frame;
                        err = av_frame_ref( f, decoded_frame );
                        if (err < 0)
                        break;
      } else
                        f = decoded_frame;
                        err = av_buffersrc_add_frame_flags( ist->filters[i]->filter, f,
                                                            AV_BUFFERSRC_FLAG_PUSH );
                        if (err == AVERROR_EOF)
                        err = 0; /* ignore */
                        if (err < 0)
                        break;
      }
                        decoded_frame->pts = AV_NOPTS_VALUE;

                        av_frame_unref( ist->filter_frame );
                        av_frame_unref( decoded_frame );
                        return err < 0 ? err : ret;
}
int VideoStore::decode_video(InputStream *ist, AVPacket *pkt, int *got_output) {
                        AVFrame *decoded_frame, *f;
                        int i, ret = 0, err = 0, resample_changed;
                        int64_t best_effort_timestamp;
                        AVRational *frame_sample_aspect;

                        if (!ist->decoded_frame && !(ist->decoded_frame = av_frame_alloc( )))
                        return AVERROR( ENOMEM );
                        if (!ist->filter_frame && !(ist->filter_frame = av_frame_alloc( )))
                        return AVERROR( ENOMEM );
                        decoded_frame = ist->decoded_frame;
                        pkt->dts = av_rescale_q( ist->dts, AV_TIME_BASE_Q, ist->st->time_base );

                        update_benchmark( NULL );
                        ret = avcodec_decode_video2( ist->dec_ctx,
                                                     decoded_frame, got_output, pkt );
                        update_benchmark( "decode_video %d.%d", ist->file_index, ist->st->index );

                        // The following line may be required in some cases where there is no parser
                        // or the parser does not has_b_frames correctly
                        if (ist->st->codec->has_b_frames < ist->dec_ctx->has_b_frames) {
                        if (ist->dec_ctx->codec_id == AV_CODEC_ID_H264) {
                        ist->st->codec->has_b_frames = ist->dec_ctx->has_b_frames;
        } else
                        av_log( ist->dec_ctx, AV_LOG_WARNING,
                                "has_b_frames is larger in decoder than demuxer %d > %d.\n"
                                "If you want to help, upload a sample "
                                "of this file to ftp://upload.ffmpeg.org/incoming/ "
                                "and contact the ffmpeg-devel mailing list. (ffmpeg-devel@ffmpeg.org)",
                                ist->dec_ctx->has_b_frames,
                                ist->st->codec->has_b_frames );
        }

                        check_decode_result( ist, got_output, ret );

                        if (*got_output && ret >= 0) {
                        if (ist->dec_ctx->width != decoded_frame->width ||
                            ist->dec_ctx->height != decoded_frame->height ||
                            ist->dec_ctx->pix_fmt != decoded_frame->format) {
                        av_log( NULL, AV_LOG_DEBUG, "Frame parameters mismatch context %d,%d,%d != %d,%d,%d\n",
                                decoded_frame->width,
                                decoded_frame->height,
                                decoded_frame->format,
                                ist->dec_ctx->width,
                                ist->dec_ctx->height,
                                ist->dec_ctx->pix_fmt );
    }
  }

                        if (!*got_output || ret < 0)
                        return ret;

                        if (ist->top_field_first >= 0)
                        decoded_frame->top_field_first = ist->top_field_first;

                        ist->frames_decoded++;

                        if (ist->hwaccel_retrieve_data && decoded_frame->format == ist->hwaccel_pix_fmt) {
                        err = ist->hwaccel_retrieve_data( ist->dec_ctx, decoded_frame );
                        if (err < 0)
                        goto fail;
        }
                        ist->hwaccel_retrieved_pix_fmt = decoded_frame->format;

                        best_effort_timestamp = av_frame_get_best_effort_timestamp( decoded_frame );
                        if (best_effort_timestamp != AV_NOPTS_VALUE) {
                        int64_t ts = av_rescale_q( decoded_frame->pts = best_effort_timestamp, ist->st->time_base, AV_TIME_BASE_Q );

                        if (ts != AV_NOPTS_VALUE)
                        ist->next_pts = ist->pts = ts;
    }

                        if (debug_ts) {
                        av_log( NULL, AV_LOG_INFO, "decoder -> ist_index:%d type:video "
                                "frame_pts:%s frame_pts_time:%s best_effort_ts:%"PRId64" best_effort_ts_time:%s keyframe:%d frame_type:%d time_base:%d/%d\n",
                                ist->st->index, av_ts2str( decoded_frame->pts ),
                                av_ts2timestr( decoded_frame->pts, &ist->st->time_base ),
                                best_effort_timestamp,
                                av_ts2timestr( best_effort_timestamp, &ist->st->time_base ),
                                decoded_frame->key_frame, decoded_frame->pict_type,
                                ist->st->time_base.num, ist->st->time_base.den );
  }

                        pkt->size = 0;

                        if (ist->st->sample_aspect_ratio.num)
                        decoded_frame->sample_aspect_ratio = ist->st->sample_aspect_ratio;

                        resample_changed = ist->resample_width != decoded_frame->width ||
                        ist->resample_height != decoded_frame->height ||
                        ist->resample_pix_fmt != decoded_frame->format;
                        if (resample_changed) {
                        av_log( NULL, AV_LOG_INFO,
                                "Input stream #%d:%d frame changed from size:%dx%d fmt:%s to size:%dx%d fmt:%s\n",
                                ist->file_index, ist->st->index,
                                ist->resample_width, ist->resample_height, av_get_pix_fmt_name( ist->resample_pix_fmt ),
                                decoded_frame->width, decoded_frame->height, av_get_pix_fmt_name( decoded_frame->format ) );

                        ist->resample_width = decoded_frame->width;
                        ist->resample_height = decoded_frame->height;
                        ist->resample_pix_fmt = decoded_frame->format;

                        // Come back to filtergraph implementation
                        //                        for (i = 0; i < nb_filtergraphs; i++) {
                        //                        if (ist_in_filtergraph( filtergraphs[i], ist ) && ist->reinit_filters &&
                        //                            configure_filtergraph( filtergraphs[i] ) < 0) {
                        //                        av_log( NULL, AV_LOG_FATAL, "Error reinitializing filters!\n" );
                        //        }
                        //      }
    }

                        frame_sample_aspect = av_opt_ptr( avcodec_get_frame_class( ), decoded_frame, "sample_aspect_ratio" );
                        for (i = 0; i < ist->nb_filters; i++) {
                        if (!frame_sample_aspect->num)
                        *frame_sample_aspect = ist->st->sample_aspect_ratio;

                        if (i < ist->nb_filters - 1) {
                        f = ist->filter_frame;
                        err = av_frame_ref( f, decoded_frame );
                        if (err < 0)
                        break;
        } else
                        f = decoded_frame;
                        ret = av_buffersrc_add_frame_flags( ist->filters[i]->filter, f, AV_BUFFERSRC_FLAG_PUSH );
                        if (ret == AVERROR_EOF) {
                        ret = 0; /* ignore */
      } else if (ret < 0) {
                        av_log( NULL, AV_LOG_FATAL,
                                "Failed to inject frame into filter network: %s\n", av_err2str( ret ) );
      }
  }

                        fail :
                        av_frame_unref( ist->filter_frame );
                        av_frame_unref( decoded_frame );
                        return err < 0 ? err : ret;
}

int VideoStore::check_output_constraints(InputStream *ist, OutputStream *ost) {
                        OutputFile *of = output_files[ost->file_index];
                        int ist_index = input_files[ist->file_index]->ist_index + ist->st->index;

                        if (ost->source_index != ist_index)
                        return 0;

                        if (ost->finished)
                        return 0;

                        if (of->start_time != AV_NOPTS_VALUE && ist->pts < of->start_time)
                        return 0;

                        return 1;
      }

int VideoStore::send_filter_eof(InputStream *ist) {
                        int i, ret;
                        for (i = 0; i < ist->nb_filters; i++) {
                        ret = av_buffersrc_add_frame( ist->filters[i]->filter, NULL );

                        if (ret < 0)
                        return ret;
    }
                        return 0;
}

void VideoStore::update_benchmark(const char *fmt, ...) {
                        if (do_benchmark_all) {
                        int64_t t = getutime( );
                        va_list va;
                        char buf[1024];

                        if (fmt) {

                        va_start( va, fmt );
                        vsnprintf( buf, sizeof (buf), fmt, va );
                        va_end( va );
                        av_log( NULL, AV_LOG_INFO, "bench: %8"PRIu64" %s \n", t - current_time, buf );
    }
                        current_time = t;
  }
}

int64_t VideoStore::getutime(void) {
#if HAVE_GETRUSAGE
  struct rusage rusage;

                        getrusage( RUSAGE_SELF, &rusage );
                        return (rusage.ru_utime.tv_sec * 1000000LL) + rusage.ru_utime.tv_usec;
#elif HAVE_GETPROCESSTIMES
  HANDLE proc;
                        FILETIME c, e, k, u;
                        proc = GetCurrentProcess( );
                        GetProcessTimes( proc, &c, &e, &k, &u );
                        return ((int64_t) u.dwHighDateTime << 32 | u.dwLowDateTime) / 10;
#else
  return av_gettime_relative( );
#endif
}

void VideoStore::check_decode_result(InputStream *ist, int *got_output, int ret) {
                        if (*got_output || ret < 0)
                        decode_error_stat[ret < 0]++;

                        if (ret < 0 && exit_on_error)
                        exit_program( 1 );

                        if (exit_on_error && *got_output && ist) {
                        if (av_frame_get_decode_error_flags( ist->decoded_frame ) || (ist->decoded_frame->flags & AV_FRAME_FLAG_CORRUPT)) {
                        av_log( NULL, AV_LOG_FATAL, "%s: corrupt decoded frame in stream %d\n", input_files[ist->file_index]->ctx->filename, ist->st->index );
        }
      }
}

int VideoStore::guess_input_channel_layout(InputStream *ist) {
                        AVCodecContext *dec = ist->dec_ctx;

                        if (!dec->channel_layout) {
                        char layout_name[256];

                        if (dec->channels > ist->guess_layout_max)
                        return 0;
                        dec->channel_layout = av_get_default_channel_layout( dec->channels );
                        if (!dec->channel_layout)
                        return 0;
                        av_get_channel_layout_string( layout_name, sizeof (layout_name),
                                                      dec->channels, dec->channel_layout );
                        av_log( NULL, AV_LOG_WARNING, "Guessed Channel Layout for Input Stream "
                                "#%d.%d : %s\n", ist->file_index, ist->st->index, layout_name );
      }
                        return 1;
}
