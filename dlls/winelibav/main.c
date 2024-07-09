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

static struct io_context *get_io_context( UINT64 handle )
{
    return (struct io_context *)(UINT_PTR)handle;
}

static IStream *get_stream( UINT64 handle )
{
    return (IStream *)(UINT_PTR)handle;
}

static NTSTATUS WINAPI read_callback( void *args, ULONG size )
{
    struct read_params *params = args;
    struct io_context *io_ctx = get_io_context( params->io_ctx );
    IStream *stream = get_stream( io_ctx->stream );
    HRESULT hr;
    ULONG ret;

    if (FAILED(hr = IStream_Read( stream, io_ctx->buffer, params->size, &ret ))) ret = -1;
    return NtCallbackReturn( &ret, sizeof(ret), STATUS_SUCCESS );
}

static NTSTATUS WINAPI write_callback( void *args, ULONG size )
{
    struct write_params *params = args;
    struct io_context *io_ctx = get_io_context( params->io_ctx );
    IStream *stream = get_stream( io_ctx->stream );
    HRESULT hr;
    ULONG ret;

    if (FAILED(hr = IStream_Write( stream, io_ctx->buffer, params->size, &ret ))) ret = -1;
    return NtCallbackReturn( &ret, sizeof(ret), STATUS_SUCCESS );
}

static NTSTATUS WINAPI seek_callback( void *args, ULONG size )
{
    struct seek_params *params = args;
    struct io_context *io_ctx = get_io_context( params->io_ctx );
    IStream *stream = get_stream( io_ctx->stream );
    LARGE_INTEGER off = {.QuadPart = params->offset};
    ULARGE_INTEGER pos;
    HRESULT hr;

    if (FAILED(hr = IStream_Seek( stream, off, 0, &pos ))) pos.QuadPart = -1;
    return NtCallbackReturn( &pos.QuadPart, sizeof(pos.QuadPart), STATUS_SUCCESS );
}

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, void *reserved )
{
    TRACE( "instance %p, reason %lu, reserved %p\n", instance, reason, reserved );

    if (reason == DLL_PROCESS_ATTACH)
    {
        struct process_attach_params params =
        {
            .read_callback = (UINT_PTR)read_callback,
            .write_callback = (UINT_PTR)write_callback,
            .seek_callback = (UINT_PTR)seek_callback,
        };
        DisableThreadLibraryCalls( instance );
        __wine_init_unix_call();
        UNIX_CALL( process_attach, &params );
    }

    return TRUE;
}

HRESULT CDECL wineav_demuxer_create( const WCHAR *url, IStream *stream, UINT64 *file_size, UINT64 *duration,
                                     UINT *stream_count, WCHAR *mime_type, demuxer_t *handle )
{
    static LARGE_INTEGER zero;
    struct demuxer_create_params params = {0};
    struct io_context *context;
    ULARGE_INTEGER pos;
    char *tmp = NULL;
    UINT len, status;
    STATSTG stat;
    HRESULT hr;

    TRACE( "url %s, stream %p\n", debugstr_w(url), stream );

    if (FAILED(hr = IStream_Stat( stream, &stat, STATFLAG_NONAME ))) return hr;
    if (FAILED(hr = IStream_Seek( stream, zero, STREAM_SEEK_CUR, &pos ))) return hr;

    if (!(context = malloc( 0x10000 ))) return 0;
    context->length = stat.cbSize.QuadPart;
    context->position = pos.QuadPart;
    context->stream = (UINT_PTR)stream;
    IStream_AddRef( stream );
    context->buffer_size = 0x10000 - sizeof(*context);
    params.io_ctx = context;

    if (url && (len = WideCharToMultiByte( CP_ACP, 0, url, -1, NULL, 0, NULL, NULL )) && (tmp = malloc( len )))
    {
        WideCharToMultiByte( CP_ACP, 0, url, -1, tmp, len, NULL, NULL );
        params.url = tmp;
    }

    status = UNIX_CALL( demuxer_create, &params );
    free( tmp );

    if (status) return HRESULT_FROM_NT(status);

    TRACE( "opened %#I64x, file_size %#I64x, duration %I64d, stream_count %u, mime_type %s\n", params.demuxer,
           stat.cbSize.QuadPart, params.duration, params.stream_count, debugstr_a(params.mime_type) );

    *file_size = stat.cbSize.QuadPart;
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
        struct io_context *ctx = params.io_ctx;
        IStream *stream = get_stream( ctx->stream );
        IStream_Release( stream );
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
    DWORD size;
    BYTE *data;

    TRACE( "demuxer %#I64x, stream %p, sample %p\n", demuxer, stream, sample );

    if (FAILED(hr = MFCreateMemoryBuffer( 0x1000, &buffer ))) return hr;
    if (FAILED(hr = IMFMediaBuffer_Lock( buffer, &data, &size, NULL ))) goto done;
    params.sample.data = (UINT_PTR)data;
    params.sample.size = size;

    if ((status = UNIX_CALL( demuxer_read, &params )) && status == STATUS_BUFFER_TOO_SMALL)
    {
        IMFMediaBuffer_Release( buffer );
        if (FAILED(hr = MFCreateMemoryBuffer( params.sample.size, &buffer ))) return hr;
        if (FAILED(hr = IMFMediaBuffer_Lock( buffer, &data, &size, NULL ))) goto done;
        params.sample.data = (UINT_PTR)data;
        params.sample.size = size;

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
        if (FAILED(hr = IMFMediaBuffer_SetCurrentLength( buffer, params.sample.size ))) goto done;
        if (FAILED(hr = create_sample( buffer, params.sample.pts, params.sample.duration, sample ))) goto done;
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
        return media_type_from_video_format( media_type->video, out );
    if (IsEqualGUID( &media_type->major, &MFMediaType_Audio ))
        return MFCreateAudioMediaType( media_type->audio, (IMFAudioMediaType **)out );

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
        if (!(params.media_type.format = CoTaskMemAlloc( params.media_type.format_size )))
            return ERROR_OUTOFMEMORY;
        status = UNIX_CALL( demuxer_stream_type, &params );
    }

    if (status)
    {
        CoTaskMemFree( params.media_type.format );
        WARN( "Failed to get output media type, status %#lx\n", status );
        return HRESULT_FROM_NT(status);
    }

    hr = media_type_to_mf( &params.media_type, media_type );
    CoTaskMemFree( params.media_type.format );
    return hr;
}

static HRESULT video_format_from_media_type(IMFMediaType *media_type, MFVIDEOFORMAT **format, UINT32 *format_size)
{
    GUID subtype;
    HRESULT hr;

    if (FAILED(hr = IMFMediaType_GetGUID(media_type, &MF_MT_SUBTYPE, &subtype)))
        return hr;
    if (FAILED(hr = MFCreateMFVideoFormatFromMFMediaType(media_type, format, format_size)))
        return hr;

    /* fixup MPEG video formats here, so we can consistently use MFVIDEOFORMAT internally */
    if (IsEqualGUID(&subtype, &MEDIASUBTYPE_MPEG1Payload)
            || IsEqualGUID(&subtype, &MEDIASUBTYPE_MPEG1Packet)
            || IsEqualGUID(&subtype, &MEDIASUBTYPE_MPEG2_VIDEO))
    {
        struct mpeg_video_format *mpeg;
        UINT32 mpeg_size, len;

        if (FAILED(IMFMediaType_GetBlobSize(media_type, &MF_MT_MPEG_SEQUENCE_HEADER, &len)))
            len = 0;
        mpeg_size = offsetof(struct mpeg_video_format, sequence_header[len]);

        if ((mpeg = CoTaskMemAlloc(mpeg_size)))
        {
            memset(mpeg, 0, mpeg_size);
            mpeg->hdr = **format;

            IMFMediaType_GetBlob(media_type, &MF_MT_MPEG_SEQUENCE_HEADER, mpeg->sequence_header, len, NULL);
            IMFMediaType_GetUINT32(media_type, &MF_MT_MPEG_START_TIME_CODE, (UINT32 *)&mpeg->start_time_code);
            IMFMediaType_GetUINT32(media_type, &MF_MT_MPEG2_PROFILE, &mpeg->profile);
            IMFMediaType_GetUINT32(media_type, &MF_MT_MPEG2_LEVEL, &mpeg->level);
            IMFMediaType_GetUINT32(media_type, &MF_MT_MPEG2_FLAGS, &mpeg->flags);

            CoTaskMemFree(*format);
            *format = &mpeg->hdr;
            *format_size = mpeg_size;
        }
    }

    return hr;
}

static HRESULT media_type_from_mf(IMFMediaType *media_type, struct media_type *out)
{
    HRESULT hr;

    if (FAILED(hr = IMFMediaType_GetMajorType(media_type, &out->major)))
        return hr;

    if (IsEqualGUID(&out->major, &MFMediaType_Video))
        return video_format_from_media_type(media_type, &out->video,
                &out->format_size);
    if (IsEqualGUID(&out->major, &MFMediaType_Audio))
        return MFCreateWaveFormatExFromMFMediaType(media_type, &out->audio,
                &out->format_size, 0);

    FIXME("Unsupported major type %s\n", debugstr_guid(&out->major));
    return E_NOTIMPL;
}

HRESULT CDECL wineav_audio_converter_create( IMFMediaType *input_type, IMFMediaType *output_type, audio_converter_t *out )
{
    struct audio_converter_create_params params = {0};
    NTSTATUS status;
    HRESULT hr;

    if (FAILED(hr = media_type_from_mf(input_type, &params.input))) return hr;
    if (SUCCEEDED(hr = media_type_from_mf(output_type, &params.output))) goto done;

    if ((status = UNIX_CALL( audio_converter_create, &params ))) hr = HRESULT_FROM_NT(status);
    else *out = params.converter;

    CoTaskMemFree(params.output.format);
done:
    CoTaskMemFree(params.input.format);
    return hr;
}

HRESULT CDECL wineav_audio_converter_destroy( audio_converter_t converter )
{
    struct audio_converter_destroy_params params = {.converter = converter};
    NTSTATUS status;

    if ((status = UNIX_CALL( audio_converter_destroy, &params ))) return HRESULT_FROM_NT(status);
    return S_OK;
}

HRESULT CDECL wineav_audio_converter_process( audio_converter_t converter, IMFSample *input, IMFSample *output )
{
    struct audio_converter_process_params params = {.converter = converter};
    NTSTATUS status;

    if ((status = UNIX_CALL( audio_converter_process, &params ))) return HRESULT_FROM_NT(status);
    return S_OK;
}

HRESULT CDECL wineav_video_converter_create( IMFMediaType *input_type, IMFMediaType *output_type, video_converter_t *out )
{
    struct video_converter_create_params params = {0};
    NTSTATUS status;
    HRESULT hr;

    if (FAILED(hr = media_type_from_mf(input_type, &params.input))) return hr;
    if (SUCCEEDED(hr = media_type_from_mf(output_type, &params.output))) goto done;

    if ((status = UNIX_CALL( video_converter_create, &params ))) hr = HRESULT_FROM_NT(status);
    else *out = params.converter;

    CoTaskMemFree(params.output.format);
done:
    CoTaskMemFree(params.input.format);
    return hr;
}

HRESULT CDECL wineav_video_converter_destroy( video_converter_t converter )
{
    struct video_converter_destroy_params params = {.converter = converter};
    NTSTATUS status;

    if ((status = UNIX_CALL( video_converter_destroy, &params ))) return HRESULT_FROM_NT(status);
    return S_OK;
}

HRESULT CDECL wineav_video_converter_process( video_converter_t converter, IMFSample *input, IMFSample *output )
{
    struct video_converter_process_params params = {.converter = converter};
    NTSTATUS status;

    if ((status = UNIX_CALL( video_converter_process, &params ))) return HRESULT_FROM_NT(status);
    return S_OK;
}
