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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#ifdef HAVE_FFMPEG

#include "unix_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

static UINT64 seek_callback;
static UINT64 read_callback;
static UINT64 write_callback;

int64_t unix_seek_callback( void *opaque, int64_t offset, int whence )
{
    struct seek_callback_params params = {.dispatch = {.callback = seek_callback}, .context = (UINT_PTR)opaque};
    struct stream_context *context = opaque;
    void *ret_ptr;
    ULONG ret_len;
    int status;

    TRACE( "opaque %p, offset %#"PRIx64", whence %#x\n", opaque, offset, whence );

    if (whence == AVSEEK_SIZE) return context->length;
    if (whence == SEEK_END) offset += context->length;
    if (whence == SEEK_CUR) offset += context->position;
    if ((UINT64)offset > context->length) offset = context->length;

    while (offset / context->capacity != context->position / context->capacity)
    {
        /* seek stream to multiples of buffer capacity */
        params.offset = (offset / context->capacity) * context->capacity;
        status = KeUserDispatchCallback( &params.dispatch, sizeof(params), &ret_ptr, &ret_len );
        if (status || ret_len != sizeof(UINT64)) return AVERROR( EINVAL );
        if (*(UINT64 *)ret_ptr != params.offset) offset = *(UINT64 *)ret_ptr; /* seeked somewhere else, retry */
        else context->position = offset;
        context->size = 0;
    }

    context->position = offset;
    return offset;
}

static int stream_context_read( struct stream_context *context )
{
    struct read_callback_params params = {.dispatch = {.callback = read_callback}, .context = (UINT_PTR)context};
    void *ret_ptr;
    ULONG ret_len;
    int status;

    params.size = context->capacity;
    status = KeUserDispatchCallback( &params.dispatch, sizeof(params), &ret_ptr, &ret_len );
    if (status || ret_len != sizeof(ULONG)) return AVERROR( EINVAL );
    context->size = *(ULONG *)ret_ptr;
    return 0;
}

int unix_read_callback( void *opaque, uint8_t *buffer, int size )
{
    struct stream_context *context = opaque;
    int ret, total = 0;

    if (!(size = min( (UINT64)size, context->length - context->position ))) return AVERROR_EOF;

    while (size)
    {
        int step, buffer_offset = context->position % context->capacity;

        if (!context->size && (ret = stream_context_read( context )) < 0) return ret;
        if (!(step = min( size, context->size - buffer_offset ))) break;
        memcpy( buffer, context->buffer + buffer_offset, step );
        buffer += step;
        total += step;
        size -= step;

        context->position += step;
        if (!(context->position % context->capacity)) context->size = 0;
    }

    if (!total) return AVERROR_EOF;
    return total;
}

#if LIBAVFORMAT_VERSION_MAJOR < 61
int unix_write_callback( void *opaque, uint8_t *buffer, int size )
#else
int unix_write_callback( void *opaque, const uint8_t *buffer, int size )
#endif
{
    struct stream_context *context = opaque;
    int ret, status, total = 0;

    TRACE( "opaque %p, buffer %p, size %#x\n", opaque, buffer, size );

    do
    {
        struct write_params params = {.dispatch = {.callback = write_callback}, .context = (UINT_PTR)context};
        void *ret_ptr;
        ULONG ret_len;

        params.size = min( size, context->buffer_size );
        memcpy( context->buffer, buffer, params.size );
        status = KeUserDispatchCallback( &params.dispatch, sizeof(params), &ret_ptr, &ret_len );
        if (status || ret_len != sizeof(ULONG)) return AVERROR( EINVAL );
        if (!(ret = *(ULONG *)ret_ptr)) break;
        buffer += ret;
        total += ret;
        size -= ret;
    } while (size && ret == context->buffer_size);

    if (!total) return AVERROR_EOF;
    context->position += total;
    return total;
}

static void vlog( void *ctx, int level, const char *fmt, va_list va_args )
{
    enum __wine_debug_class dbcl = __WINE_DBCL_TRACE;
    if (level <= AV_LOG_ERROR) dbcl = __WINE_DBCL_ERR;
    if (level <= AV_LOG_WARNING) dbcl = __WINE_DBCL_WARN;
    wine_dbg_vlog( dbcl, __wine_dbch___default, __FILE__, __LINE__, __func__, __WINE_DBG_RETADDR, fmt, va_args );
}

AVPacket *packet_from_sample( const struct sample *sample )
{
    AVPacket *packet = av_packet_alloc();
    av_new_packet( packet, sample->size + AV_INPUT_BUFFER_PADDING_SIZE );
    memcpy( packet->data, (void *)(UINT_PTR)sample->data, sample->size );
    packet->size = sample->size;

    packet->pts = sample->pts;
    packet->dts = sample->dts;
    packet->duration = sample->duration;
    if (sample->flags & SAMPLE_FLAG_SYNC_POINT) packet->flags |= AV_PKT_FLAG_KEY;

    return packet;
}

static const char *debugstr_version( UINT version )
{
    return wine_dbg_sprintf("%u.%u.%u", AV_VERSION_MAJOR(version), AV_VERSION_MINOR(version), AV_VERSION_MICRO(version));
}

static NTSTATUS process_attach( void *arg )
{
    struct process_attach_params *params = arg;
    const AVInputFormat *demuxer;
    const AVOutputFormat *muxer;
    const AVCodec *codec;
    void *opaque;

    TRACE( "FFmpeg support:\n" );
    TRACE( "  avutil version %s\n", debugstr_version(avutil_version()) );
    TRACE( "  avformat version %s\n", debugstr_version(avformat_version()) );
    TRACE( "  avcodec version %s\n", debugstr_version(avcodec_version()) );
    TRACE( "  swresample version %s\n", debugstr_version(swresample_version()) );
    TRACE( "  swscale version %s\n", debugstr_version(swscale_version()) );

    TRACE( "available demuxers:\n" );
    for (opaque = NULL; (demuxer = av_demuxer_iterate( &opaque ));)
    {
        TRACE( "  %s (%s)\n", demuxer->name, demuxer->long_name );
        if (demuxer->extensions) TRACE( "    extensions: %s\n", demuxer->extensions );
        if (demuxer->mime_type) TRACE( "    mime_types: %s\n", demuxer->mime_type );
    }

    TRACE( "available decoders:\n" );
    for (opaque = NULL; (codec = av_codec_iterate( &opaque ));)
    {
        if (!av_codec_is_decoder( codec )) continue;
        TRACE( "  %s (%s)\n", codec->name, codec->long_name );
    }

    TRACE( "available muxers:\n" );
    for (opaque = NULL; (muxer = av_muxer_iterate( &opaque ));)
    {
        TRACE( "  %s (%s)\n", muxer->name, muxer->long_name );
        if (muxer->extensions) TRACE( "    extensions: %s\n", muxer->extensions );
        if (muxer->mime_type) TRACE( "    mime_types: %s\n", muxer->mime_type );
    }

    TRACE( "available encoders:\n" );
    for (opaque = NULL; (codec = av_codec_iterate( &opaque ));)
    {
        if (!av_codec_is_encoder( codec )) continue;
        TRACE( "  %s (%s)\n", codec->name, codec->long_name );
    }

    av_log_set_callback( vlog );

    seek_callback = params->seek_callback;
    read_callback = params->read_callback;
    write_callback = params->write_callback;

    return STATUS_SUCCESS;
}


static struct transform *get_transform( struct winedmo_transform transform )
{
    return (struct transform *)(UINT_PTR)transform.handle;
}

static NTSTATUS transform_check( void *args )
{
    struct transform_check_params *params = args;
    if (IsEqualGUID( &params->major, &MFMediaType_Audio )) return audio_transform_check( args );
    if (IsEqualGUID( &params->major, &MFMediaType_Video )) return video_transform_check( args );
    FIXME( "Not implemented!\n" );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS transform_create( void *args )
{
    struct transform_create_params *params = args;
    if (IsEqualGUID( &params->input_type.major, &MFMediaType_Audio ) &&
        IsEqualGUID( &params->output_type.major, &MFMediaType_Audio ))
        return audio_transform_create( args );
    if (IsEqualGUID( &params->input_type.major, &MFMediaType_Video ) &&
        IsEqualGUID( &params->output_type.major, &MFMediaType_Video ))
        return video_transform_create( args );
    FIXME( "Not implemented!\n" );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS transform_destroy( void *args )
{
    struct transform_destroy_params *params = args;
    struct transform *transform = get_transform( params->transform );
    return transform->ops->destroy( transform );
}

static NTSTATUS transform_get_output_type( void *args )
{
    struct transform_get_output_type_params *params = args;
    struct transform *transform = get_transform( params->transform );
    if (!transform->ops->get_output_type) return STATUS_NOT_IMPLEMENTED;
    return transform->ops->get_output_type( transform, &params->media_type );
}

static NTSTATUS transform_set_output_type( void *args )
{
    struct transform_set_output_type_params *params = args;
    struct transform *transform = get_transform( params->transform );
    if (!transform->ops->set_output_type) return STATUS_NOT_IMPLEMENTED;
    return transform->ops->set_output_type( transform, &params->media_type );
}

static NTSTATUS transform_process_input( void *args )
{
    struct transform_process_input_params *params = args;
    struct transform *transform = get_transform( params->transform );
    return transform->ops->process_input( transform, &params->sample );
}

static NTSTATUS transform_process_output( void *args )
{
    struct transform_process_output_params *params = args;
    struct transform *transform = get_transform( params->transform );
    return transform->ops->process_output( transform, &params->sample );
}

static NTSTATUS transform_drain( void *args )
{
    struct transform_drain_params *params = args;
    struct transform *transform = get_transform( params->transform );
    if (!transform->ops->drain) return STATUS_NOT_IMPLEMENTED;
    return transform->ops->drain( transform, params->discard );
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
#define X( name ) [unix_##name] = name
    X( process_attach ),

    X( demuxer_check ),
    X( demuxer_create ),
    X( demuxer_destroy ),
    X( demuxer_read ),
    X( demuxer_seek ),
    X( demuxer_stream_lang ),
    X( demuxer_stream_name ),
    X( demuxer_stream_type ),

    X( muxer_check ),
    X( muxer_create ),
    X( muxer_destroy ),
    X( muxer_add_stream ),
    X( muxer_start ),
    X( muxer_write ),

    X( transform_check ),
    X( transform_create ),
    X( transform_destroy ),
    X( transform_get_output_type ),
    X( transform_set_output_type ),
    X( transform_process_input ),
    X( transform_process_output ),
    X( transform_drain ),
};

C_ASSERT(ARRAY_SIZE(__wine_unix_call_funcs) == unix_funcs_count);

#ifdef _WIN64

typedef ULONG PTR32;

static NTSTATUS wow64_demuxer_create( void *arg )
{
    struct
    {
        PTR32 url;
        PTR32 context;
        struct winedmo_demuxer demuxer;
        char mime_type[256];
        UINT32 stream_count;
        INT64 duration;
    } *params32 = arg;
    struct demuxer_create_params params;
    NTSTATUS status;

    params.url = UintToPtr( params32->url );
    params.context = UintToPtr( params32->context );
    if ((status = demuxer_create( &params ))) return status;
    params32->demuxer = params.demuxer;
    memcpy( params32->mime_type, params.mime_type, 256 );
    params32->stream_count = params.stream_count;
    params32->duration = params.duration;

    return status;
}

static NTSTATUS wow64_demuxer_destroy( void *arg )
{
    struct
    {
        struct winedmo_demuxer demuxer;
        PTR32 context;
    } *params32 = arg;
    struct demuxer_create_params params;
    NTSTATUS status;

    params.demuxer = params32->demuxer;
    if ((status = demuxer_destroy( &params ))) return status;
    params32->context = PtrToUint( params.context );

    return status;
}

static NTSTATUS wow64_muxer_create( void *arg )
{
    struct
    {
        PTR32 context;
        PTR32 url;
        char mime_type[256];
        struct winedmo_muxer muxer;
    } *params32 = arg;
    struct muxer_create_params params;
    NTSTATUS status;

    params.context = UintToPtr( params32->context );
    params.url = UintToPtr( params32->url );
    memcpy( params.mime_type, params32->mime_type, 256 );
    if ((status = muxer_create( &params ))) return status;
    params32->muxer = params.muxer;

    return status;
}

static NTSTATUS wow64_muxer_destroy( void *arg )
{
    struct
    {
        struct winedmo_muxer muxer;
        PTR32 context;
    } *params32 = arg;
    struct muxer_create_params params;
    NTSTATUS status;

    params.muxer = params32->muxer;
    if ((status = muxer_destroy( &params ))) return status;
    params32->context = PtrToUint( params.context );

    return status;
}

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
#define X64( name ) [unix_##name] = wow64_##name
    X( process_attach ),

    X( demuxer_check ),
    X64( demuxer_create ),
    X64( demuxer_destroy ),
    X( demuxer_read ),
    X( demuxer_seek ),
    X( demuxer_stream_lang ),
    X( demuxer_stream_name ),
    X( demuxer_stream_type ),

    X( muxer_check ),
    X64( muxer_create ),
    X64( muxer_destroy ),
    X( muxer_add_stream ),
    X( muxer_start ),
    X( muxer_write ),

    X( transform_check ),
    X( transform_create ),
    X( transform_destroy ),
    X( transform_get_output_type ),
    X( transform_set_output_type ),
    X( transform_process_input ),
    X( transform_process_output ),
    X( transform_drain ),
};

C_ASSERT(ARRAY_SIZE(__wine_unix_call_wow64_funcs) == unix_funcs_count);

#endif /* _WIN64 */

#endif /* HAVE_FFMPEG */
