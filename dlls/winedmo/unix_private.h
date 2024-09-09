/*
 * Copyright 2024 Rémi Bernon for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include <stdint.h>

#ifdef HAVE_FFMPEG
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/cpu.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#ifdef HAVE_LIBAVCODEC_BSF_H
# include <libavcodec/bsf.h>
#endif
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#else
typedef struct AVCodecParameters AVCodecParameters;
typedef struct AVRational AVRational;
#endif /* HAVE_FFMPEG */

#include "unixlib.h"
#include "wine/debug.h"

/* unixlib.c */
extern int64_t unix_seek_callback( void *opaque, int64_t offset, int whence );
extern int unix_read_callback( void *opaque, uint8_t *buffer, int size );
#if LIBAVFORMAT_VERSION_MAJOR < 61
extern int unix_write_callback( void *opaque, uint8_t *buffer, int size );
#else
extern int unix_write_callback( void *opaque, const uint8_t *buffer, int size );
#endif

extern AVPacket *packet_from_sample( const struct sample *sample );

struct transform;
struct transform_ops
{
    NTSTATUS ( *destroy )(struct transform *iface);
    NTSTATUS ( *get_output_type )(struct transform *iface, struct media_type *media_type);
    NTSTATUS ( *set_output_type )(struct transform *iface, const struct media_type *media_type);
    NTSTATUS ( *process_input )(struct transform *iface, const struct sample *sample);
    NTSTATUS ( *process_output )(struct transform *iface, struct sample *sample);
    NTSTATUS ( *drain )(struct transform *iface, BOOL discard);
};

struct transform
{
    const struct transform_ops *ops;
};

struct encoder
{
    AVCodecContext *codec;
};

/* unix_decoder.c */
struct decoder;

extern NTSTATUS decoder_create( const struct media_type *input_type, struct decoder **out );
extern NTSTATUS decoder_destroy( struct decoder *decoder );
extern NTSTATUS decoder_process_input( struct decoder *decoder, const struct sample *sample );
extern NTSTATUS decoder_process_output( struct decoder *decoder, AVFrame *input_frame );
extern NTSTATUS decoder_drain( struct decoder *decoder, BOOL discard );

/* unix_demuxer.c */
extern NTSTATUS demuxer_check( void * );
extern NTSTATUS demuxer_create( void * );
extern NTSTATUS demuxer_destroy( void * );
extern NTSTATUS demuxer_read( void * );
extern NTSTATUS demuxer_seek( void * );
extern NTSTATUS demuxer_stream_lang( void * );
extern NTSTATUS demuxer_stream_name( void * );
extern NTSTATUS demuxer_stream_type( void * );

/* unix_encoder.c */
extern NTSTATUS encoder_init( struct encoder *encoder, const struct media_type *output_type );
extern NTSTATUS encoder_cleanup( struct encoder *encoder );

/* unix_muxer.c */
extern NTSTATUS muxer_check( void * );
extern NTSTATUS muxer_create( void * );
extern NTSTATUS muxer_destroy( void * );
extern NTSTATUS muxer_add_stream( void * );
extern NTSTATUS muxer_start( void * );
extern NTSTATUS muxer_write( void * );

/* unix_audio.c */
extern NTSTATUS audio_transform_check( struct transform_check_params *params );
extern NTSTATUS audio_transform_create( struct transform_create_params *params );

/* unix_video.c */
extern NTSTATUS video_transform_check( struct transform_check_params *params );
extern NTSTATUS video_transform_create( struct transform_create_params *params );

/* unix_media_type.c */
extern NTSTATUS codec_params_from_media_type( AVCodecParameters *params, AVRational *sar,
                                              AVRational *fps, const struct media_type *media_type );
extern NTSTATUS media_type_from_codec_params( const AVCodecParameters *params, const AVRational *sar,
                                              const AVRational *fps, UINT32 align, struct media_type *media_type );
extern enum AVPixelFormat pixel_format_from_subtype( const GUID *subtype );
extern enum AVPixelFormat pixel_format_from_video_format( const MFVIDEOFORMAT *format );
extern enum AVSampleFormat sample_format_from_subtype( const GUID *subtype, UINT depth );
extern enum AVSampleFormat sample_format_from_wave_format( const WAVEFORMATEX *format );
extern enum AVCodecID codec_id_from_audio_subtype( const GUID *subtype );
extern enum AVCodecID codec_id_from_video_subtype( const GUID *subtype );
