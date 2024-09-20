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

#define COBJMACROS
#include "unixlib.h"
#include "winnls.h"
#include "mfidl.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);


static struct stream_context *stream_context_create( struct winedmo_stream *stream, UINT64 stream_size )
{
    struct stream_context *context;

    if (!(context = malloc( sizeof(*context) + 0x10000 ))) return NULL;
    context->stream = (UINT_PTR)stream;
    context->length = stream_size;
    context->position = 0;
    context->capacity = 0x10000;

    return context;
}

static void stream_context_destroy( struct stream_context *context )
{
    free( context );
}


static struct stream_context *get_stream_context( UINT64 handle )
{
    return (struct stream_context *)(UINT_PTR)handle;
}

static struct winedmo_stream *get_stream( UINT64 handle )
{
    return (struct winedmo_stream *)(UINT_PTR)handle;
}

static NTSTATUS WINAPI seek_callback( void *args, ULONG size )
{
    struct seek_callback_params *params = args;
    struct stream_context *context = get_stream_context( params->context );
    struct winedmo_stream *stream = get_stream( context->stream );
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;
    UINT64 pos = params->offset;

    TRACE( "stream %p, offset %#I64x\n", stream, params->offset );

    if (!stream->p_seek || (status = stream->p_seek( stream, &pos )))
        WARN( "Failed to seek stream %p, status %#lx\n", stream, status );
    else
        TRACE( "Seeked stream %p to %#I64x\n", stream, pos );

    return NtCallbackReturn( &pos, sizeof(pos), status );
}

static NTSTATUS WINAPI read_callback( void *args, ULONG size )
{
    struct read_callback_params *params = args;
    struct stream_context *context = get_stream_context( params->context );
    struct winedmo_stream *stream = get_stream( context->stream );
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;
    ULONG ret = params->size;

    TRACE( "stream %p, size %#x\n", stream, params->size );

    if (!stream->p_read || (status = stream->p_read( stream, context->buffer, &ret )))
        WARN( "Failed to read from stream %p, status %#lx\n", stream, status );
    else
        TRACE( "Read %#lx bytes from stream %p\n", ret, stream );

    return NtCallbackReturn( &ret, sizeof(ret), status );
}

static NTSTATUS WINAPI write_callback( void *args, ULONG size )
{
    struct write_params *params = args;
    struct stream_context *context = get_stream_context( params->context );
    struct winedmo_stream *stream = get_stream( context->stream );
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;
    ULONG ret = params->size;

    if (!stream->p_write || (status = stream->p_write( stream, context->buffer, &ret )))
        WARN( "Failed to write to stream, status %#lx\n", status );
    return NtCallbackReturn( &ret, sizeof(ret), status );
}


BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, void *reserved )
{
    TRACE( "instance %p, reason %lu, reserved %p\n", instance, reason, reserved );

    if (reason == DLL_PROCESS_ATTACH)
    {
        struct process_attach_params params =
        {
            .seek_callback = (UINT_PTR)seek_callback,
            .read_callback = (UINT_PTR)read_callback,
            .write_callback = (UINT_PTR)write_callback,
        };
        NTSTATUS status;

        DisableThreadLibraryCalls( instance );

        status = __wine_init_unix_call();
        if (!status) status = UNIX_CALL( process_attach, &params );
        if (status) WARN( "Failed to init unixlib, status %#lx\n", status );
    }

    return TRUE;
}


static void buffer_lock( DMO_OUTPUT_DATA_BUFFER *buffer, BOOL read, struct sample *sample )
{
    BYTE *data, *scanline0;
    IMF2DBuffer2 *buffer2;
    LONG pitch = 0;
    HRESULT hr;
    DWORD size;

    if (read)
    {
        IMFSample *object;
        UINT64 dts = buffer->rtTimestamp;
        UINT32 value;

        if (SUCCEEDED(hr = IMediaBuffer_QueryInterface( buffer->pBuffer, &IID_IMFSample, (void **)&object )))
        {
            if (FAILED(IMFSample_GetUINT64( object, &MFSampleExtension_DecodeTimestamp, &dts ))) dts = INT64_MIN;
            if (FAILED(IMFSample_GetSampleTime( object, &buffer->rtTimestamp ))) buffer->rtTimestamp = INT64_MIN;
            if (FAILED(IMFSample_GetSampleDuration( object, &buffer->rtTimelength ))) buffer->rtTimelength = INT64_MIN;
            if (FAILED(IMFSample_GetUINT32( object, &MFSampleExtension_CleanPoint, &value ))) buffer->dwStatus = 0;
            else if (value) buffer->dwStatus |= DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT;
            IMFSample_Release( object );
        }

        sample->dts = dts;
        sample->pts = buffer->rtTimestamp;
        sample->duration = buffer->rtTimelength;
        if (buffer->dwStatus & DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT) sample->flags |= SAMPLE_FLAG_SYNC_POINT;
    }

    if (SUCCEEDED(hr = IMediaBuffer_QueryInterface( buffer->pBuffer, &IID_IMF2DBuffer2, (void **)&buffer2 )))
    {
        hr = IMF2DBuffer2_Lock2DSize( buffer2, read ? MF2DBuffer_LockFlags_Read : MF2DBuffer_LockFlags_Write,
                                      &scanline0, &pitch, &data, &size );
        if (FAILED(hr)) ERR( "Failed to lock 2D buffer %p, hr %#lx\n", buffer, hr );
        IMF2DBuffer2_Release( buffer2 );
    }
    if (FAILED(hr))
    {
        if (FAILED(hr = IMediaBuffer_GetBufferAndLength( buffer->pBuffer, &data, &size )))
            ERR( "Failed to get media buffer data %p, hr %#lx\n", buffer, hr );
        if (!read && FAILED(hr = IMediaBuffer_GetMaxLength( buffer->pBuffer, &size )))
            ERR( "Failed to get media buffer max length %p, hr %#lx\n", buffer, hr );
    }

    sample->stride = abs( pitch );
    sample->data = (UINT_PTR)data;
    sample->size = size;
}

static void buffer_unlock( DMO_OUTPUT_DATA_BUFFER *buffer, BOOL read, struct sample *sample, NTSTATUS status )
{
    IMF2DBuffer2 *buffer2;
    HRESULT hr;

    if (sample->stride && SUCCEEDED(hr = IMediaBuffer_QueryInterface( buffer->pBuffer, &IID_IMF2DBuffer2, (void **)&buffer2 )))
    {
        if (FAILED(hr = IMF2DBuffer2_Unlock2D( buffer2 ))) ERR( "Failed to unlock 2D buffer %p, hr %#lx\n", buffer, hr );
        IMF2DBuffer2_Release( buffer2 );
    }

    if (!read)
    {
        IMFSample *object;

        if (FAILED(hr = IMediaBuffer_SetLength( buffer->pBuffer, status ? 0 : sample->size )))
            ERR( "Failed to update buffer length, hr %#lx\n", hr );

        buffer->dwStatus = 0;
        if (SUCCEEDED(hr = IMediaBuffer_QueryInterface( buffer->pBuffer, &IID_IMFSample, (void **)&object )))
        {
            if (sample->dts != INT64_MIN) IMFSample_SetUINT64( object, &MFSampleExtension_DecodeTimestamp, sample->dts );
            if (sample->pts != INT64_MIN) IMFSample_SetSampleTime( object, sample->pts );
            if (sample->duration != INT64_MIN) IMFSample_SetSampleDuration( object, sample->duration );
            if (sample->flags & SAMPLE_FLAG_SYNC_POINT) IMFSample_SetUINT32( object, &MFSampleExtension_CleanPoint, 1 );
            if (sample->flags & SAMPLE_FLAG_DISCONTINUITY) IMFSample_SetUINT32( object, &MFSampleExtension_Discontinuity, 1 );
            IMFSample_Release( object );
        }

        if ((buffer->rtTimestamp = sample->pts) != INT64_MIN) buffer->dwStatus |= DMO_OUTPUT_DATA_BUFFERF_TIME;
        if ((buffer->rtTimelength = sample->duration) != INT64_MIN) buffer->dwStatus |= DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH;
        if (sample->flags & SAMPLE_FLAG_SYNC_POINT) buffer->dwStatus |= DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT;
        if (sample->flags & SAMPLE_FLAG_INCOMPLETE) buffer->dwStatus |= DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE;
    }
}


NTSTATUS CDECL winedmo_demuxer_check( const char *mime_type )
{
    struct demuxer_check_params params = {0};
    NTSTATUS status;

    TRACE( "mime_type %s\n", debugstr_a(mime_type) );
    lstrcpynA( params.mime_type, mime_type, sizeof(params.mime_type) );

    if ((status = UNIX_CALL( demuxer_check, &params ))) WARN( "returning %#lx\n", status );
    return status;
}

NTSTATUS CDECL winedmo_demuxer_create( const WCHAR *url, struct winedmo_stream *stream, UINT64 stream_size, INT64 *duration,
                                       UINT *stream_count, WCHAR *mime_type, struct winedmo_demuxer *demuxer )
{
    struct demuxer_create_params params = {0};
    char *tmp = NULL;
    NTSTATUS status;
    UINT len;

    TRACE( "url %s, stream %p, stream_size %#I64x, mime_type %p, demuxer %p\n", debugstr_w(url),
           stream, stream_size, mime_type, demuxer );

    if (!(params.context = stream_context_create( stream, stream_size ))) return STATUS_NO_MEMORY;

    if (url && (len = WideCharToMultiByte( CP_ACP, 0, url, -1, NULL, 0, NULL, NULL )) && (tmp = malloc( len )))
    {
        WideCharToMultiByte( CP_ACP, 0, url, -1, tmp, len, NULL, NULL );
        params.url = tmp;
    }
    status = UNIX_CALL( demuxer_create, &params );
    free( tmp );

    if (status)
    {
        WARN( "demuxer_create failed, status %#lx\n", status );
        stream_context_destroy( params.context );
        return status;
    }

    *duration = params.duration;
    *stream_count = params.stream_count;
    MultiByteToWideChar( CP_ACP, 0, params.mime_type, -1, mime_type, 256 );
    *demuxer = params.demuxer;
    TRACE( "created demuxer %#I64x, stream %p, duration %I64d, stream_count %u, mime_type %s\n",
           demuxer->handle, stream, params.duration, params.stream_count, debugstr_a(params.mime_type) );
    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_demuxer_destroy( struct winedmo_demuxer *demuxer )
{
    struct demuxer_destroy_params params = {.demuxer = *demuxer};
    NTSTATUS status;

    if (!demuxer->handle) return STATUS_SUCCESS;

    TRACE( "demuxer %#I64x\n", demuxer->handle );

    demuxer->handle = 0;
    status = UNIX_CALL( demuxer_destroy, &params );
    if (status) WARN( "demuxer_destroy failed, status %#lx\n", status );
    else stream_context_destroy( params.context );

    return status;
}

NTSTATUS CDECL winedmo_demuxer_read( struct winedmo_demuxer demuxer, UINT *stream, DMO_OUTPUT_DATA_BUFFER *buffer, UINT *buffer_size )
{
    struct demuxer_read_params params = {.demuxer = demuxer};
    NTSTATUS status;

    TRACE( "demuxer %#I64x, stream %p, buffer %p, buffer_size %p\n", demuxer.handle, stream, buffer, buffer_size );

    buffer_lock( buffer, FALSE, &params.sample );
    status = UNIX_CALL( demuxer_read, &params );
    buffer_unlock( buffer, FALSE, &params.sample, status );
    *buffer_size = params.sample.size;
    *stream = params.stream;

    if (status)
    {
        if (status == STATUS_END_OF_FILE) WARN( "Reached end of media file.\n" );
        else if (status != STATUS_BUFFER_TOO_SMALL) ERR( "Failed to read sample, status %#lx\n", status );
        return status;
    }

    TRACE( "Got buffer %p, buffer_size %#x on stream %u\n", buffer->pBuffer, *buffer_size, *stream );
    return status;
}

NTSTATUS CDECL winedmo_demuxer_seek( struct winedmo_demuxer demuxer, INT64 timestamp )
{
    struct demuxer_seek_params params = {.demuxer = demuxer, .timestamp = timestamp};
    NTSTATUS status;

    TRACE( "demuxer %#I64x, timestamp %I64d\n", demuxer.handle, timestamp );

    if ((status = UNIX_CALL( demuxer_seek, &params )))
    {
        WARN( "Failed to set position, status %#lx\n", status );
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_demuxer_stream_lang( struct winedmo_demuxer demuxer, UINT stream, WCHAR *buffer, UINT len )
{
    struct demuxer_stream_lang_params params = {.demuxer = demuxer, .stream = stream};
    NTSTATUS status;

    TRACE( "demuxer %#I64x, stream %u\n", demuxer.handle, stream );

    if ((status = UNIX_CALL( demuxer_stream_lang, &params )))
    {
        WARN( "Failed to get stream lang, status %#lx\n", status );
        return status;
    }

    len = MultiByteToWideChar( CP_UTF8, 0, params.buffer, -1, buffer, len );
    buffer[len - 1] = 0;
    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_demuxer_stream_name( struct winedmo_demuxer demuxer, UINT stream, WCHAR *buffer, UINT len )
{
    struct demuxer_stream_name_params params = {.demuxer = demuxer, .stream = stream};
    NTSTATUS status;

    TRACE( "demuxer %#I64x, stream %u\n", demuxer.handle, stream );

    if ((status = UNIX_CALL( demuxer_stream_name, &params )))
    {
        WARN( "Failed to get stream name, status %#lx\n", status );
        return status;
    }

    len = MultiByteToWideChar( CP_UTF8, 0, params.buffer, -1, buffer, len );
    buffer[len - 1] = 0;
    return STATUS_SUCCESS;
}


static HRESULT get_media_type( UINT code, void *params, struct media_type *media_type,
                               GUID *major, union winedmo_format **format )
{
    NTSTATUS status;

    media_type->format = NULL;
    if ((status = WINE_UNIX_CALL( code, params )) && status == STATUS_BUFFER_TOO_SMALL)
    {
        if (!(media_type->format = malloc( media_type->format_size ))) return STATUS_NO_MEMORY;
        status = WINE_UNIX_CALL( code, params );
    }

    if (!status)
    {
        *major = media_type->major;
        *format = media_type->format;
        return STATUS_SUCCESS;
    }

    WARN( "Failed to get media type, code %#x, status %#lx\n", code, status );
    free( media_type->format );
    return status;
}

NTSTATUS CDECL winedmo_demuxer_stream_type( struct winedmo_demuxer demuxer, UINT stream,
                                            GUID *major, union winedmo_format **format )
{
    struct demuxer_stream_type_params params = {.demuxer = demuxer, .stream = stream};
    TRACE( "demuxer %#I64x, stream %u, major %p, format %p\n", demuxer.handle, stream, major, format );
    return get_media_type( unix_demuxer_stream_type, &params, &params.media_type, major, format );
}


NTSTATUS CDECL winedmo_muxer_check( const char *mime_type )
{
    struct muxer_check_params params = {0};
    lstrcpynA( params.mime_type, mime_type, sizeof(params.mime_type) );
    return UNIX_CALL( muxer_check, &params );
}

NTSTATUS CDECL winedmo_muxer_create( const WCHAR *url, const WCHAR *mime_type, struct winedmo_stream *stream, struct winedmo_muxer *muxer )
{
    struct muxer_create_params params = {0};
    QWORD stream_size = 0;
    char *tmp = NULL;
    UINT len, status;

    TRACE( "stream %p\n", stream );

    if (!(params.context = stream_context_create( stream, stream_size ))) return STATUS_NO_MEMORY;

    if (url && (len = WideCharToMultiByte( CP_ACP, 0, url, -1, NULL, 0, NULL, NULL )) && (tmp = malloc( len )))
    {
        WideCharToMultiByte( CP_ACP, 0, url, -1, tmp, len, NULL, NULL );
        params.url = tmp;
    }
    WideCharToMultiByte( CP_ACP, 0, mime_type, -1, params.mime_type, ARRAY_SIZE( params.mime_type ), NULL, NULL );

    if ((status = UNIX_CALL( muxer_create, &params ))) return status;
    *muxer = params.muxer;
    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_muxer_destroy( struct winedmo_muxer *muxer )
{
    struct muxer_destroy_params params = {.muxer = *muxer};
    NTSTATUS status;

    TRACE( "muxer %#I64x\n", muxer->handle );

    if (!muxer->handle) return STATUS_SUCCESS;
    if ((status = UNIX_CALL( muxer_destroy, &params ))) return status;
    stream_context_destroy( params.context );
    muxer->handle = 0;

    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_muxer_add_stream( struct winedmo_muxer muxer, UINT32 stream_id, GUID major,
                                         union winedmo_format *format )
{
    struct muxer_add_stream_params params =
    {
        .muxer = muxer, .stream_id = stream_id,
        .media_type = {.major = major, .format = format},
    };
    NTSTATUS status;

    if (IsEqualGUID( &major, &MFMediaType_Video ))
        params.media_type.format_size = format->video.dwSize;
    else if (IsEqualGUID( &major, &MFMediaType_Audio ))
        params.media_type.format_size = sizeof(format->audio) + format->audio.cbSize;
    else
    {
        FIXME( "Major type %s not supported\n", debugstr_guid( &major ) );
        return STATUS_INVALID_PARAMETER;
    }

    if ((status = UNIX_CALL( muxer_add_stream, &params )))
    {
        WARN( "Failed to add stream, status %#lx\n", status );
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_muxer_start( struct winedmo_muxer muxer )
{
    struct muxer_start_params params = {.muxer = muxer};
    NTSTATUS status;

    if ((status = UNIX_CALL( muxer_start, &params ))) return status;
    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_muxer_write( struct winedmo_muxer muxer, UINT stream, DMO_OUTPUT_DATA_BUFFER *buffer )
{
    struct muxer_write_params params = {.muxer = muxer, .stream = stream};
    NTSTATUS status;

    buffer_lock( buffer, TRUE, &params.sample );
    status = UNIX_CALL( muxer_write, &params );
    buffer_unlock( buffer, TRUE, &params.sample, status );

    if (status) return status;
    return STATUS_SUCCESS;
}


NTSTATUS CDECL winedmo_transform_check( GUID major, GUID input, GUID output )
{
    struct transform_check_params params =
    {
        .major = major,
        .input = input,
        .output = output,
    };

    return UNIX_CALL( transform_check, &params );
}

NTSTATUS CDECL winedmo_transform_create( GUID major, union winedmo_format *input, union winedmo_format *output, struct winedmo_transform *transform )
{
    struct transform_create_params params =
    {
        .input_type = {.major = major, .format = input},
        .output_type = {.major = major, .format = output},
    };
    NTSTATUS status;

    TRACE( "\n" );

    if (IsEqualGUID( &major, &MFMediaType_Video ))
    {
        params.input_type.format_size = input->video.dwSize;
        params.output_type.format_size = output->video.dwSize;
    }
    else if (IsEqualGUID( &major, &MFMediaType_Audio ))
    {
        params.input_type.format_size = sizeof(input->audio) + input->audio.cbSize;
        params.output_type.format_size = sizeof(output->audio) + output->audio.cbSize;
    }
    else
    {
        FIXME( "Major type %s not supported\n", debugstr_guid( &major ) );
        return STATUS_INVALID_PARAMETER;
    }

    if ((status = UNIX_CALL( transform_create, &params )))
    {
        WARN( "Failed to create transform, status %#lx\n", status );
        return status;
    }

    *transform = params.transform;
    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_transform_destroy( struct winedmo_transform *transform )
{
    struct transform_destroy_params params = {.transform = *transform};
    NTSTATUS status;

    TRACE( "transform %#I64x\n", transform->handle );

    if (!transform->handle) return STATUS_SUCCESS;
    if ((status = UNIX_CALL( transform_destroy, &params ))) return status;
    transform->handle = 0;

    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_transform_get_output_type( struct winedmo_transform transform, GUID *major, union winedmo_format **format )
{
    struct transform_get_output_type_params params = {.transform = transform};
    TRACE( "transform %#I64x, format %p\n", transform.handle, format );
    return get_media_type( unix_transform_get_output_type, &params, &params.media_type, major, format );
}

NTSTATUS CDECL winedmo_transform_set_output_type( struct winedmo_transform transform, GUID major, union winedmo_format *format )
{
    struct transform_set_output_type_params params =
    {
        .transform = transform,
        .media_type = {.major = major, .format = format},
    };
    NTSTATUS status;

    if (IsEqualGUID( &major, &MFMediaType_Video ))
        params.media_type.format_size = format->video.dwSize;
    else if (IsEqualGUID( &major, &MFMediaType_Audio ))
        params.media_type.format_size = sizeof(format->audio) + format->audio.cbSize;
    else
    {
        FIXME( "Major type %s not supported\n", debugstr_guid( &major ) );
        return STATUS_INVALID_PARAMETER;
    }

    TRACE( "transform %#I64x\n", transform.handle );

    if ((status = UNIX_CALL( transform_set_output_type, &params )))
    {
        WARN( "Failed\n" );
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_transform_process_input( struct winedmo_transform transform, DMO_OUTPUT_DATA_BUFFER *buffer )
{
    struct transform_process_input_params params = {.transform = transform};
    NTSTATUS status;

    TRACE( "transform %#I64x\n", transform.handle );

    buffer_lock( buffer, TRUE, &params.sample );
    status = UNIX_CALL( transform_process_input, &params );
    buffer_unlock( buffer, TRUE, &params.sample, status );

    return status;
}

NTSTATUS CDECL winedmo_transform_process_output( struct winedmo_transform transform, DMO_OUTPUT_DATA_BUFFER *buffer )
{
    struct transform_process_output_params params = {.transform = transform};
    NTSTATUS status;

    TRACE( "transform %#I64x\n", transform.handle );

    buffer_lock( buffer, FALSE, &params.sample );
    status = UNIX_CALL( transform_process_output, &params );
    buffer_unlock( buffer, FALSE, &params.sample, status );

    return status;
}

NTSTATUS CDECL winedmo_transform_drain( struct winedmo_transform transform, BOOL discard )
{
    struct transform_drain_params params = {.transform = transform, .discard = discard};
    TRACE( "transform %#I64x\n", transform.handle );
    return UNIX_CALL( transform_drain, &params );
}
