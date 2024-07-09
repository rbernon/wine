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

#include <stddef.h>
#include <stdarg.h>

#define COBJMACROS
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "ntuser.h"

#include "initguid.h"
#include "uuids.h"
#include "mfapi.h"
#include "mferror.h"

#include "wine/debug.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(libav);

struct context
{
    IMFByteStream *stream;
    struct io_context io;
};

static NTSTATUS WINAPI call_io_read_callback( void *args, ULONG size )
{
    struct call_io_read_params *params = args;
    struct context *ctx = CONTAINING_RECORD( (UINT_PTR)params->context, struct context, io );
    ULONG ret = params->size;
    HRESULT hr;

    TRACE( "stream %p, size %#x, buffer %p\n", ctx->stream, params->size, ctx->io.buffer );

    if (FAILED(hr = IMFByteStream_Read( ctx->stream, ctx->io.buffer, params->size, &ret ))) ret = -1;
    return NtCallbackReturn( &ret, sizeof(ret), STATUS_SUCCESS );
}

static NTSTATUS WINAPI call_io_write_callback( void *args, ULONG size )
{
    struct call_io_write_params *params = args;
    struct context *ctx = CONTAINING_RECORD( (UINT_PTR)params->context, struct context, io );
    HRESULT hr;
    ULONG ret;

    TRACE( "stream %p, size %#x, buffer %p\n", ctx->stream, params->size, ctx->io.buffer );

    if (FAILED(hr = IMFByteStream_Write( ctx->stream, ctx->io.buffer, params->size, &ret ))) ret = -1;
    return NtCallbackReturn( &ret, sizeof(ret), STATUS_SUCCESS );
}

static NTSTATUS WINAPI call_io_seek_callback( void *args, ULONG size )
{
    struct call_io_seek_params *params = args;
    struct context *ctx = CONTAINING_RECORD( (UINT_PTR)params->context, struct context, io );
    QWORD pos, ret = params->offset;
    HRESULT hr;

    TRACE( "stream %p, offset %I64d\n", ctx->stream, params->offset );

    if (FAILED(hr = IMFByteStream_GetCurrentPosition( ctx->stream, &pos ))) goto done;
    if (ret != pos && FAILED( hr = IMFByteStream_SetCurrentPosition( ctx->stream, ret ))) ret = -1;

done:
    return NtCallbackReturn( &ret, sizeof(ret), STATUS_SUCCESS );
}

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, void *reserved )
{
    TRACE( "instance %p, reason %lu, reserved %p\n", instance, reason, reserved );

    if (reason == DLL_PROCESS_ATTACH)
    {
        KERNEL_CALLBACK_PROC *kernel_callback_table;

        DisableThreadLibraryCalls( instance );

        kernel_callback_table = NtCurrentTeb()->Peb->KernelCallbackTable;
        kernel_callback_table[NtUserCallIOReadCallback] = call_io_read_callback;
        kernel_callback_table[NtUserCallIOWriteCallback] = call_io_write_callback;
        kernel_callback_table[NtUserCallIOSeekCallback] = call_io_seek_callback;

        __wine_init_unix_call();
        UNIX_CALL( process_attach, NULL );
    }

    return TRUE;
}

HRESULT CDECL wineav_demuxer_create( const WCHAR *url, IMFByteStream *stream, INT64 *duration,
                                     UINT *stream_count, WCHAR *mime_type, demuxer_t *handle )
{
    struct demuxer_create_params params = {0};
    struct context *context;
    QWORD position, length;
    char *tmp = NULL;
    UINT len, status;
    HRESULT hr;

    TRACE( "url %s, stream %p\n", debugstr_w(url), stream );

    if (FAILED(hr = IMFByteStream_GetLength( stream, &length ))) return hr;
    if (FAILED(hr = IMFByteStream_GetCurrentPosition( stream, &position ))) return hr;

    if (!(context = malloc( 0x10000 ))) return 0;
    IMFByteStream_AddRef( (context->stream = stream) );
    context->io.total_size = length;
    context->io.position = position;
    context->io.buffer_size = 0x10000 - sizeof(*context);
    params.io_ctx = &context->io;

    if (url && (len = WideCharToMultiByte( CP_ACP, 0, url, -1, NULL, 0, NULL, NULL )) && (tmp = malloc( len )))
    {
        WideCharToMultiByte( CP_ACP, 0, url, -1, tmp, len, NULL, NULL );
        params.url = tmp;
    }

    status = UNIX_CALL( demuxer_create, &params );
    free( tmp );

    if (status) return HRESULT_FROM_NT(status);

    TRACE( "opened %#I64x, duration %I64d, stream_count %u, mime_type %s\n", params.demuxer,
           params.duration, params.stream_count, debugstr_a(params.mime_type) );

    *duration = params.duration;
    *stream_count = params.stream_count;
    MultiByteToWideChar( CP_ACP, 0, params.mime_type, -1, mime_type, 256 );
    *handle = params.demuxer;

    return S_OK;
}

void CDECL wineav_demuxer_destroy( demuxer_t demuxer )
{
    struct demuxer_destroy_params params = {.demuxer = demuxer};

    TRACE( "demuxer %#I64x\n", demuxer );

    if (!UNIX_CALL( demuxer_destroy, &params ))
    {
        struct context *ctx = CONTAINING_RECORD( params.io_ctx, struct context, io );
        IMFByteStream_Release( ctx->stream );
        free( ctx );
    }
}

static HRESULT create_sample( IMFMediaBuffer *buffer, INT64 pts, INT64 duration, IMFSample **out )
{
    IMFSample *sample;
    HRESULT hr;

    TRACE( "buffer %p, pts %I64d, duration %I64d, out %p\n", buffer, pts, duration, out );

    if (FAILED(hr = MFCreateSample( &sample ))) return hr;
    if (FAILED(hr = IMFSample_SetSampleTime( sample, pts ))) goto done;
    if (FAILED(hr = IMFSample_SetSampleDuration( sample, duration ))) goto done;
    if (buffer && FAILED( hr = IMFSample_AddBuffer( sample, buffer ))) goto done;
    IMFSample_AddRef( (*out = sample) );

done:
    IMFSample_Release( sample );
    return hr;
}

HRESULT CDECL wineav_demuxer_read( demuxer_t demuxer, UINT *stream, IMFSample **sample )
{
    struct demuxer_read_params params = {.demuxer = demuxer};
    IMFMediaBuffer *buffer = NULL;
    HRESULT hr = S_OK;
    NTSTATUS status;

    TRACE( "demuxer %#I64x, stream %p, sample %p\n", demuxer, stream, sample );

    if (FAILED(hr = MFCreateMemoryBuffer( 0x1000, &buffer ))) return hr;
    if (FAILED(hr = IMFMediaBuffer_Lock( buffer, &params.data, &params.size, NULL ))) goto done;

    if ((status = UNIX_CALL( demuxer_read, &params )) && status == STATUS_BUFFER_TOO_SMALL)
    {
        IMFMediaBuffer_Release( buffer );
        if (FAILED(hr = MFCreateMemoryBuffer( params.size, &buffer ))) return hr;
        if (FAILED(hr = IMFMediaBuffer_Lock( buffer, &params.data, &params.size, NULL ))) goto done;
        status = UNIX_CALL( demuxer_read, &params );
    }

    if (FAILED(hr = IMFMediaBuffer_Unlock( buffer ))) goto done;

    if (status)
    {
        if (status == STATUS_END_OF_FILE) hr = MF_E_END_OF_STREAM;
        else hr = HRESULT_FROM_NT(status);
    }
    else
    {
        if (FAILED(hr = IMFMediaBuffer_SetCurrentLength( buffer, params.size ))) goto done;
        if (FAILED(hr = create_sample( buffer, params.pts, params.duration, sample ))) goto done;
        *stream = params.stream;
    }

done:
    if (FAILED(hr)) WARN( "Failed to read sample type, hr %#lx\n", hr );
    IMFMediaBuffer_Release( buffer );
    return hr;
}

HRESULT CDECL wineav_demuxer_seek( demuxer_t demuxer, INT64 timestamp )
{
    struct demuxer_seek_params params = {.demuxer = demuxer, .timestamp = timestamp};
    NTSTATUS status;

    TRACE( "demuxer %#I64x, timestamp %I64d\n", demuxer, timestamp );

    if ((status = UNIX_CALL( demuxer_seek, &params )))
    {
        WARN( "Failed to set position, status %#lx\n", status );
        return HRESULT_FROM_NT(status);
    }

    return S_OK;
}

HRESULT CDECL wineav_demuxer_stream_lang( demuxer_t demuxer, UINT stream, WCHAR *buffer, UINT len )
{
    struct demuxer_stream_lang_params params = {.demuxer = demuxer, .stream = stream};
    NTSTATUS status;

    TRACE( "demuxer %#I64x, stream %u\n", demuxer, stream );

    if ((status = UNIX_CALL( demuxer_stream_lang, &params )))
    {
        WARN( "Failed to get stream lang, status %#lx\n", status );
        return HRESULT_FROM_NT(status);
    }

    len = MultiByteToWideChar( CP_UTF8, 0, params.buffer, -1, buffer, len );
    buffer[len - 1] = 0;
    return S_OK;
}

HRESULT CDECL wineav_demuxer_stream_name( demuxer_t demuxer, UINT stream, WCHAR *buffer, UINT len )
{
    struct demuxer_stream_name_params params = {.demuxer = demuxer, .stream = stream};
    NTSTATUS status;

    TRACE( "demuxer %#I64x, stream %u\n", demuxer, stream );

    if ((status = UNIX_CALL( demuxer_stream_name, &params )))
    {
        WARN( "Failed to get stream name, status %#lx\n", status );
        return HRESULT_FROM_NT(status);
    }

    len = MultiByteToWideChar( CP_UTF8, 0, params.buffer, -1, buffer, len );
    buffer[len - 1] = 0;
    return S_OK;
}


static HRESULT media_type_from_video_format( const MFVIDEOFORMAT *format, IMFMediaType **media_type )
{
    HRESULT hr;

    if (FAILED(hr = MFCreateVideoMediaType( format, (IMFVideoMediaType **)media_type )) ||
        format->dwSize <= sizeof(*format))
        return hr;

    /* fixup MPEG video formats here, so we can consistently use MFVIDEOFORMAT internally */
    if (IsEqualGUID( &format->guidFormat, &MEDIASUBTYPE_MPEG1Payload ) ||
        IsEqualGUID( &format->guidFormat, &MEDIASUBTYPE_MPEG1Packet ) ||
        IsEqualGUID( &format->guidFormat, &MEDIASUBTYPE_MPEG2_VIDEO ))
    {
        struct mpeg_video_format *mpeg = (struct mpeg_video_format *)format;
        IMFMediaType_SetBlob( *media_type, &MF_MT_MPEG_SEQUENCE_HEADER, mpeg->sequence_header, mpeg->sequence_header_count );
        IMFMediaType_SetUINT32( *media_type, &MF_MT_MPEG_START_TIME_CODE, mpeg->start_time_code );
        IMFMediaType_SetUINT32( *media_type, &MF_MT_MPEG2_PROFILE, mpeg->profile );
        IMFMediaType_SetUINT32( *media_type, &MF_MT_MPEG2_LEVEL, mpeg->level );
        IMFMediaType_SetUINT32( *media_type, &MF_MT_MPEG2_FLAGS, mpeg->flags );
        IMFMediaType_DeleteItem( *media_type, &MF_MT_USER_DATA );
    }

    if (FAILED(IMFMediaType_GetItem(*media_type, &MF_MT_VIDEO_ROTATION, NULL)))
        IMFMediaType_SetUINT32(*media_type, &MF_MT_VIDEO_ROTATION, MFVideoRotationFormat_0);

    return hr;
}

static HRESULT media_type_to_mf( const struct media_type *media_type, IMFMediaType **out )
{
    if (IsEqualGUID( &media_type->major, &MFMediaType_Video ))
        return media_type_from_video_format( media_type->u.video, out );
    if (IsEqualGUID( &media_type->major, &MFMediaType_Audio ))
        return MFCreateAudioMediaType( media_type->u.audio, (IMFAudioMediaType **)out );

    FIXME( "Unsupported major type %s\n", debugstr_guid( &media_type->major ) );
    return E_NOTIMPL;
}

HRESULT CDECL wineav_demuxer_stream_type( demuxer_t demuxer, UINT stream, IMFMediaType **media_type )
{
    struct demuxer_stream_type_params params = {.demuxer = demuxer, .stream = stream};
    NTSTATUS status;
    HRESULT hr;

    TRACE( "demuxer %#I64x, stream %u, media_type %p\n", demuxer, stream, media_type );

    if ((status = UNIX_CALL( demuxer_stream_type, &params )) && status == STATUS_BUFFER_TOO_SMALL)
    {
        if (!(params.media_type.u.format = CoTaskMemAlloc( params.media_type.format_size )))
            return ERROR_OUTOFMEMORY;
        status = UNIX_CALL( demuxer_stream_type, &params );
    }

    if (status)
    {
        CoTaskMemFree( params.media_type.u.format );
        WARN( "Failed to get output media type, status %#lx\n", status );
        return HRESULT_FROM_NT(status);
    }

    hr = media_type_to_mf( &params.media_type, media_type );
    CoTaskMemFree( params.media_type.u.format );
    return hr;
}
