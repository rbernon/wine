/*
 * Copyright 2012 Austin English
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

#include "gst_private.h"
#include "dmoreg.h"
#include "initguid.h"
#include "wmsdk.h"

WINE_DEFAULT_DEBUG_CHANNEL(wmvcore);

struct wm_stream
{
    struct wm_reader *reader;

    WMT_STREAM_SELECTION selection;
    AM_MEDIA_TYPE mt;
    WORD index;

    enum
    {
        STREAM_STARTING,
        STREAM_RUNNING,
        STREAM_ENDED,
    } state;
    bool read_compressed;

    IMediaObject *decoder;
    struct list compressed_samples;
    IWMReaderAllocatorEx *output_allocator;
    IWMReaderAllocatorEx *stream_allocator;
};

struct wm_reader
{
    IUnknown IUnknown_inner;
    IWMSyncReader2 IWMSyncReader2_iface;
    IWMHeaderInfo3 IWMHeaderInfo3_iface;
    IWMLanguageList IWMLanguageList_iface;
    IWMPacketSize2 IWMPacketSize2_iface;
    IWMProfile3 IWMProfile3_iface;
    IWMReaderPlaylistBurn IWMReaderPlaylistBurn_iface;
    IWMReaderTimecode IWMReaderTimecode_iface;
    IUnknown *outer;
    LONG refcount;

    CRITICAL_SECTION cs;
    CRITICAL_SECTION shutdown_cs;
    QWORD start_time;
    QWORD file_size;
    INT64 duration;

    IStream *stream;
    HANDLE file;

    struct winedmo_stream winedmo_stream;
    struct winedmo_demuxer winedmo_demuxer;

    struct wm_stream *streams;
    UINT32 stream_count;
};

static HRESULT copy_wm_media_type(WM_MEDIA_TYPE *dst, DWORD *size, const AM_MEDIA_TYPE *src)
{
    const DWORD capacity = *size;

    *size = sizeof(*dst) + src->cbFormat;
    if (!dst)
        return S_OK;
    if (capacity < *size)
        return ASF_E_BUFFERTOOSMALL;

    strmbase_dump_media_type(src);
    memcpy(dst, src, sizeof(*dst));
    memcpy(dst + 1, src->pbFormat, src->cbFormat);
    dst->pbFormat = (BYTE *)(dst + 1);

    return S_OK;
}

static struct wm_stream *get_stream_by_output_number(struct wm_reader *reader, DWORD output)
{
    if (output < reader->stream_count)
        return &reader->streams[output];
    WARN("Invalid output number %lu.\n", output);
    return NULL;
}

static struct wm_stream *wm_reader_get_stream_by_stream_number(struct wm_reader *reader, WORD stream_number)
{
    if (stream_number && stream_number <= reader->stream_count)
        return &reader->streams[stream_number - 1];
    WARN("Invalid stream number %u.\n", stream_number);
    return NULL;
}

struct output_props
{
    IWMOutputMediaProps IWMOutputMediaProps_iface;
    LONG refcount;

    AM_MEDIA_TYPE mt;
};

static inline struct output_props *impl_from_IWMOutputMediaProps(IWMOutputMediaProps *iface)
{
    return CONTAINING_RECORD(iface, struct output_props, IWMOutputMediaProps_iface);
}

static HRESULT WINAPI output_props_QueryInterface(IWMOutputMediaProps *iface, REFIID iid, void **out)
{
    struct output_props *props = impl_from_IWMOutputMediaProps(iface);

    TRACE("props %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IWMOutputMediaProps))
        *out = &props->IWMOutputMediaProps_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI output_props_AddRef(IWMOutputMediaProps *iface)
{
    struct output_props *props = impl_from_IWMOutputMediaProps(iface);
    ULONG refcount = InterlockedIncrement(&props->refcount);

    TRACE("%p increasing refcount to %lu.\n", props, refcount);

    return refcount;
}

static ULONG WINAPI output_props_Release(IWMOutputMediaProps *iface)
{
    struct output_props *props = impl_from_IWMOutputMediaProps(iface);
    ULONG refcount = InterlockedDecrement(&props->refcount);

    TRACE("%p decreasing refcount to %lu.\n", props, refcount);

    if (!refcount)
    {
        FreeMediaType(&props->mt);
        free(props);
    }

    return refcount;
}

static HRESULT WINAPI output_props_GetType(IWMOutputMediaProps *iface, GUID *major_type)
{
    const struct output_props *props = impl_from_IWMOutputMediaProps(iface);

    TRACE("iface %p, major_type %p.\n", iface, major_type);

    *major_type = props->mt.majortype;
    return S_OK;
}

static HRESULT WINAPI output_props_GetMediaType(IWMOutputMediaProps *iface, WM_MEDIA_TYPE *mt, DWORD *size)
{
    const struct output_props *props = impl_from_IWMOutputMediaProps(iface);

    TRACE("iface %p, mt %p, size %p.\n", iface, mt, size);

    return copy_wm_media_type(mt, size, &props->mt);
}

static HRESULT WINAPI output_props_SetMediaType(IWMOutputMediaProps *iface, WM_MEDIA_TYPE *mt)
{
    struct output_props *props = impl_from_IWMOutputMediaProps(iface);

    TRACE("iface %p, mt %p.\n", iface, mt);

    if (!mt)
        return E_POINTER;

    if (!IsEqualGUID(&props->mt.majortype, &mt->majortype))
        return E_FAIL;

    FreeMediaType(&props->mt);
    return CopyMediaType(&props->mt, (AM_MEDIA_TYPE *)mt);
}

static HRESULT WINAPI output_props_GetStreamGroupName(IWMOutputMediaProps *iface, WCHAR *name, WORD *len)
{
    FIXME("iface %p, name %p, len %p, stub!\n", iface, name, len);
    return E_NOTIMPL;
}

static HRESULT WINAPI output_props_GetConnectionName(IWMOutputMediaProps *iface, WCHAR *name, WORD *len)
{
    FIXME("iface %p, name %p, len %p, stub!\n", iface, name, len);
    return E_NOTIMPL;
}

static const struct IWMOutputMediaPropsVtbl output_props_vtbl =
{
    output_props_QueryInterface,
    output_props_AddRef,
    output_props_Release,
    output_props_GetType,
    output_props_GetMediaType,
    output_props_SetMediaType,
    output_props_GetStreamGroupName,
    output_props_GetConnectionName,
};

static HRESULT output_props_create(const AM_MEDIA_TYPE *mt, IWMOutputMediaProps **out)
{
    struct output_props *object;
    HRESULT hr;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;
    if (FAILED(hr = CopyMediaType(&object->mt, mt)))
    {
        free(object);
        return hr;
    }

    object->IWMOutputMediaProps_iface.lpVtbl = &output_props_vtbl;
    object->refcount = 1;
    TRACE("Created output properties %p.\n", object);

    *out = &object->IWMOutputMediaProps_iface;
    return S_OK;
}

struct media_buffer
{
    IMediaBuffer IMediaBuffer_iface;
    LONG refcount;
    INSSBuffer *sample;
};

static struct media_buffer *impl_from_IMediaBuffer(IMediaBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct media_buffer, IMediaBuffer_iface);
}

static HRESULT WINAPI media_buffer_QueryInterface(IMediaBuffer *iface, REFIID iid, void **out)
{
    struct media_buffer *buffer = impl_from_IMediaBuffer(iface);

    TRACE("buffer %p, iid %s, out %p.\n", buffer, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IMediaBuffer))
    {
        IMediaBuffer_AddRef(&buffer->IMediaBuffer_iface);
        *out = &buffer->IMediaBuffer_iface;
        return S_OK;
    }

    *out = NULL;
    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI media_buffer_AddRef(IMediaBuffer *iface)
{
    struct media_buffer *buffer = impl_from_IMediaBuffer(iface);
    ULONG refcount = InterlockedIncrement(&buffer->refcount);
    TRACE("buffer %p increasing refcount to %lu.\n", buffer, refcount);
    return refcount;
}

static ULONG WINAPI media_buffer_Release(IMediaBuffer *iface)
{
    struct media_buffer *buffer = impl_from_IMediaBuffer(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);

    TRACE("buffer %p decreasing refcount to %lu.\n", buffer, refcount);

    if (!refcount)
    {
        INSSBuffer_Release(buffer->sample);
        free(buffer);
    }

    return refcount;
}

static HRESULT WINAPI media_buffer_SetLength(IMediaBuffer *iface, DWORD length)
{
    struct media_buffer *buffer = impl_from_IMediaBuffer(iface);
    return INSSBuffer_SetLength(buffer->sample, length);
}

static HRESULT WINAPI media_buffer_GetMaxLength(IMediaBuffer *iface, DWORD *max_length)
{
    struct media_buffer *buffer = impl_from_IMediaBuffer(iface);
    return INSSBuffer_GetMaxLength(buffer->sample, max_length);
}

static HRESULT WINAPI media_buffer_GetBufferAndLength(IMediaBuffer *iface, BYTE **data, DWORD *length)
{
    struct media_buffer *buffer = impl_from_IMediaBuffer(iface);
    return INSSBuffer_GetBufferAndLength(buffer->sample, data, length);
}

static const IMediaBufferVtbl media_buffer_vtbl =
{
    media_buffer_QueryInterface,
    media_buffer_AddRef,
    media_buffer_Release,
    media_buffer_SetLength,
    media_buffer_GetMaxLength,
    media_buffer_GetBufferAndLength,
};

static HRESULT media_buffer_create( INSSBuffer *sample, IMediaBuffer **out )
{
    struct media_buffer *buffer;

    if (!(buffer = calloc(1, sizeof(*buffer))))
        return E_OUTOFMEMORY;
    buffer->IMediaBuffer_iface.lpVtbl = &media_buffer_vtbl;
    buffer->refcount = 1;

    INSSBuffer_AddRef((buffer->sample = sample));
    TRACE("created %p\n", buffer);

    *out = &buffer->IMediaBuffer_iface;
    return S_OK;
}

static HRESULT media_type_from_mf_video_format( const MFVIDEOFORMAT *format, WM_MEDIA_TYPE *mt, DWORD *size )
{
    UINT capacity = *size;
    VIDEOINFOHEADER *vih;

    *size = sizeof(*mt) + sizeof(*vih) + format->dwSize - sizeof(*format);
    if (!mt || capacity < *size)
        return mt ? ASF_E_BUFFERTOOSMALL : S_OK;

    memset(mt, 0, *size);
    mt->majortype = MEDIATYPE_Video;
    mt->subtype = format->guidFormat;
    mt->bTemporalCompression = TRUE;
    mt->formattype = FORMAT_VideoInfo;
    mt->pbFormat = (BYTE *)(mt + 1);
    mt->cbFormat = *size - sizeof(*mt);

    vih = (VIDEOINFOHEADER *)mt + 1;
    vih->bmiHeader.biSize = sizeof(vih->bmiHeader);
    vih->bmiHeader.biWidth = format->videoInfo.dwWidth;
    vih->bmiHeader.biHeight = format->videoInfo.dwHeight;
    vih->bmiHeader.biPlanes = 1;
    vih->bmiHeader.biBitCount = 24;
    vih->bmiHeader.biCompression = format->guidFormat.Data1;
    SetRect(&vih->rcSource, 0, 0, vih->bmiHeader.biWidth, vih->bmiHeader.biHeight);
    SetRect(&vih->rcTarget, 0, 0, vih->bmiHeader.biWidth, vih->bmiHeader.biHeight);
    memcpy(vih + 1, format + 1, format->dwSize - sizeof(*format));

    return S_OK;
}

static HRESULT media_type_from_wave_format_ex( const WAVEFORMATEX *format, WM_MEDIA_TYPE *mt, DWORD *size )
{
    UINT capacity = *size;

    *size = sizeof(*mt) + sizeof(*format) + format->cbSize;
    if (!mt || capacity < *size)
        return mt ? ASF_E_BUFFERTOOSMALL : S_OK;

    memset(mt, 0, *size);
    mt->majortype = MEDIATYPE_Audio;
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        WAVEFORMATEXTENSIBLE *wfxext = (WAVEFORMATEXTENSIBLE *)format;
        mt->subtype = wfxext->SubFormat;
    }
    else
    {
        mt->subtype = MEDIASUBTYPE_PCM;
        mt->subtype.Data1 = format->wFormatTag;
    }
    mt->bFixedSizeSamples = TRUE;
    mt->lSampleSize = format->nBlockAlign;
    mt->formattype = FORMAT_WaveFormatEx;
    mt->pbFormat = (BYTE *)(mt + 1);
    mt->cbFormat = *size - sizeof(*mt);
    memcpy(mt->pbFormat, format, sizeof(*format) + format->cbSize);

    return S_OK;
}

struct sample
{
    INSSBuffer INSSBuffer_iface;
    LONG refcount;

    DWORD flags;
    REFERENCE_TIME time;
    REFERENCE_TIME duration;
    struct list entry;

    UINT capacity;
    UINT size;
    BYTE data[];
};

static struct sample *impl_from_INSSBuffer(INSSBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct sample, INSSBuffer_iface);
}

static HRESULT WINAPI sample_QueryInterface(INSSBuffer *iface, REFIID iid, void **out)
{
    struct sample *sample = impl_from_INSSBuffer(iface);

    TRACE("sample %p, iid %s, out %p.\n", sample, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_INSSBuffer))
    {
        INSSBuffer_AddRef(&sample->INSSBuffer_iface);
        *out = &sample->INSSBuffer_iface;
        return S_OK;
    }

    *out = NULL;
    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI sample_AddRef(INSSBuffer *iface)
{
    struct sample *sample = impl_from_INSSBuffer(iface);
    ULONG refcount = InterlockedIncrement(&sample->refcount);

    TRACE("%p increasing refcount to %lu.\n", sample, refcount);

    return refcount;
}

static ULONG WINAPI sample_Release(INSSBuffer *iface)
{
    struct sample *sample = impl_from_INSSBuffer(iface);
    ULONG refcount = InterlockedDecrement(&sample->refcount);

    TRACE("%p decreasing refcount to %lu.\n", sample, refcount);

    if (!refcount)
        free(sample);

    return refcount;
}

static HRESULT WINAPI sample_GetLength(INSSBuffer *iface, DWORD *size)
{
    struct sample *sample = impl_from_INSSBuffer(iface);
    FIXME("iface %p, size %p, stub!\n", iface, size);
    *size = sample->size;
    return S_OK;
}

static HRESULT WINAPI sample_SetLength(INSSBuffer *iface, DWORD size)
{
    struct sample *sample = impl_from_INSSBuffer(iface);

    TRACE("iface %p, size %lu.\n", sample, size);

    if (size > sample->capacity)
        return E_INVALIDARG;

    sample->size = size;
    return S_OK;
}

static HRESULT WINAPI sample_GetMaxLength(INSSBuffer *iface, DWORD *size)
{
    struct sample *sample = impl_from_INSSBuffer(iface);

    TRACE("buffer %p, size %p.\n", sample, size);

    *size = sample->capacity;
    return S_OK;
}

static HRESULT WINAPI sample_GetBuffer(INSSBuffer *iface, BYTE **data)
{
    struct sample *sample = impl_from_INSSBuffer(iface);

    TRACE("buffer %p, data %p.\n", sample, data);

    *data = sample->data;
    return S_OK;
}

static HRESULT WINAPI sample_GetBufferAndLength(INSSBuffer *iface, BYTE **data, DWORD *size)
{
    struct sample *sample = impl_from_INSSBuffer(iface);

    TRACE("buffer %p, data %p, size %p.\n", sample, data, size);

    *size = sample->size;
    *data = sample->data;
    return S_OK;
}

static const INSSBufferVtbl sample_vtbl =
{
    sample_QueryInterface,
    sample_AddRef,
    sample_Release,
    sample_GetLength,
    sample_SetLength,
    sample_GetMaxLength,
    sample_GetBuffer,
    sample_GetBufferAndLength,
};

static HRESULT sample_create(UINT size, INSSBuffer **out)
{
    struct sample *sample;

    if (!(sample = calloc(1, offsetof(struct sample, data[size]))))
        return E_OUTOFMEMORY;
    sample->INSSBuffer_iface.lpVtbl = &sample_vtbl;
    sample->refcount = 1;

    sample->capacity = size;
    TRACE("created %p\n", sample);

    *out = &sample->INSSBuffer_iface;
    return S_OK;
}

struct stream_config
{
    IWMStreamConfig IWMStreamConfig_iface;
    IWMMediaProps IWMMediaProps_iface;
    LONG refcount;

    UINT index;
    AM_MEDIA_TYPE mt;
};

static struct stream_config *impl_from_IWMStreamConfig(IWMStreamConfig *iface)
{
    return CONTAINING_RECORD(iface, struct stream_config, IWMStreamConfig_iface);
}

static HRESULT WINAPI stream_config_QueryInterface(IWMStreamConfig *iface, REFIID iid, void **out)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);

    TRACE("config %p, iid %s, out %p.\n", config, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IWMStreamConfig))
        *out = &config->IWMStreamConfig_iface;
    else if (IsEqualGUID(iid, &IID_IWMMediaProps))
        *out = &config->IWMMediaProps_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI stream_config_AddRef(IWMStreamConfig *iface)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    ULONG refcount = InterlockedIncrement(&config->refcount);

    TRACE("%p increasing refcount to %lu.\n", config, refcount);

    return refcount;
}

static ULONG WINAPI stream_config_Release(IWMStreamConfig *iface)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    ULONG refcount = InterlockedDecrement(&config->refcount);

    TRACE("%p decreasing refcount to %lu.\n", config, refcount);

    if (!refcount)
    {
        FreeMediaType(&config->mt);
        free(config);
    }

    return refcount;
}

static HRESULT WINAPI stream_config_GetStreamType(IWMStreamConfig *iface, GUID *type)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    TRACE("config %p, type %p.\n", config, type);
    *type = config->mt.majortype;
    return S_OK;
}

static HRESULT WINAPI stream_config_GetStreamNumber(IWMStreamConfig *iface, WORD *number)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    TRACE("config %p, number %p.\n", config, number);
    *number = config->index + 1;
    return S_OK;
}

static HRESULT WINAPI stream_config_SetStreamNumber(IWMStreamConfig *iface, WORD number)
{
    FIXME("iface %p, number %u, stub!\n", iface, number);
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_config_GetStreamName(IWMStreamConfig *iface, WCHAR *name, WORD *len)
{
    FIXME("iface %p, name %p, len %p, stub!\n", iface, name, len);
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_config_SetStreamName(IWMStreamConfig *iface, const WCHAR *name)
{
    FIXME("iface %p, name %s, stub!\n", iface, debugstr_w(name));
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_config_GetConnectionName(IWMStreamConfig *iface, WCHAR *name, WORD *len)
{
    FIXME("iface %p, name %p, len %p, stub!\n", iface, name, len);
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_config_SetConnectionName(IWMStreamConfig *iface, const WCHAR *name)
{
    FIXME("iface %p, name %s, stub!\n", iface, debugstr_w(name));
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_config_GetBitrate(IWMStreamConfig *iface, DWORD *bitrate)
{
    FIXME("iface %p, bitrate %p, stub!\n", iface, bitrate);
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_config_SetBitrate(IWMStreamConfig *iface, DWORD bitrate)
{
    FIXME("iface %p, bitrate %lu, stub!\n", iface, bitrate);
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_config_GetBufferWindow(IWMStreamConfig *iface, DWORD *window)
{
    FIXME("iface %p, window %p, stub!\n", iface, window);
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_config_SetBufferWindow(IWMStreamConfig *iface, DWORD window)
{
    FIXME("iface %p, window %lu, stub!\n", iface, window);
    return E_NOTIMPL;
}

static const IWMStreamConfigVtbl stream_config_vtbl =
{
    stream_config_QueryInterface,
    stream_config_AddRef,
    stream_config_Release,
    stream_config_GetStreamType,
    stream_config_GetStreamNumber,
    stream_config_SetStreamNumber,
    stream_config_GetStreamName,
    stream_config_SetStreamName,
    stream_config_GetConnectionName,
    stream_config_SetConnectionName,
    stream_config_GetBitrate,
    stream_config_SetBitrate,
    stream_config_GetBufferWindow,
    stream_config_SetBufferWindow,
};

static struct stream_config *impl_from_IWMMediaProps(IWMMediaProps *iface)
{
    return CONTAINING_RECORD(iface, struct stream_config, IWMMediaProps_iface);
}

static HRESULT WINAPI stream_props_QueryInterface(IWMMediaProps *iface, REFIID iid, void **out)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    return IWMStreamConfig_QueryInterface(&config->IWMStreamConfig_iface, iid, out);
}

static ULONG WINAPI stream_props_AddRef(IWMMediaProps *iface)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    return IWMStreamConfig_AddRef(&config->IWMStreamConfig_iface);
}

static ULONG WINAPI stream_props_Release(IWMMediaProps *iface)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    return IWMStreamConfig_Release(&config->IWMStreamConfig_iface);
}

static HRESULT WINAPI stream_props_GetType(IWMMediaProps *iface, GUID *type)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    TRACE("iface %p, type %p\n", iface, type);
    *type = config->mt.majortype;
    return S_OK;
}

static HRESULT WINAPI stream_props_GetMediaType(IWMMediaProps *iface, WM_MEDIA_TYPE *mt, DWORD *size)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    TRACE("iface %p, mt %p, size %p.\n", iface, mt, size);
    return copy_wm_media_type(mt, size, &config->mt);
}

static HRESULT WINAPI stream_props_SetMediaType(IWMMediaProps *iface, WM_MEDIA_TYPE *mt)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    FIXME("iface %p, mt %p, stub!\n", iface, mt);
    FreeMediaType(&config->mt);
    return CopyMediaType(&config->mt, (AM_MEDIA_TYPE *)mt);
}

static const IWMMediaPropsVtbl stream_props_vtbl =
{
    stream_props_QueryInterface,
    stream_props_AddRef,
    stream_props_Release,
    stream_props_GetType,
    stream_props_GetMediaType,
    stream_props_SetMediaType,
};

static struct wm_reader *impl_from_IWMProfile3(IWMProfile3 *iface)
{
    return CONTAINING_RECORD(iface, struct wm_reader, IWMProfile3_iface);
}

static HRESULT WINAPI profile_QueryInterface(IWMProfile3 *iface, REFIID iid, void **out)
{
    struct wm_reader *reader = impl_from_IWMProfile3(iface);
    return IUnknown_QueryInterface(reader->outer, iid, out);
}

static ULONG WINAPI profile_AddRef(IWMProfile3 *iface)
{
    struct wm_reader *reader = impl_from_IWMProfile3(iface);
    return IUnknown_AddRef(reader->outer);
}

static ULONG WINAPI profile_Release(IWMProfile3 *iface)
{
    struct wm_reader *reader = impl_from_IWMProfile3(iface);
    return IUnknown_Release(reader->outer);
}

static HRESULT WINAPI profile_GetVersion(IWMProfile3 *iface, WMT_VERSION *version)
{
    FIXME("iface %p, version %p, stub!\n", iface, version);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetName(IWMProfile3 *iface, WCHAR *name, DWORD *length)
{
    FIXME("iface %p, name %p, length %p, stub!\n", iface, name, length);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_SetName(IWMProfile3 *iface, const WCHAR *name)
{
    FIXME("iface %p, name %s, stub!\n", iface, debugstr_w(name));
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetDescription(IWMProfile3 *iface, WCHAR *description, DWORD *length)
{
    FIXME("iface %p, description %p, length %p, stub!\n", iface, description, length);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_SetDescription(IWMProfile3 *iface, const WCHAR *description)
{
    FIXME("iface %p, description %s, stub!\n", iface, debugstr_w(description));
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetStreamCount(IWMProfile3 *iface, DWORD *count)
{
    struct wm_reader *reader = impl_from_IWMProfile3(iface);

    TRACE("reader %p, count %p.\n", reader, count);

    if (!count)
        return E_INVALIDARG;

    EnterCriticalSection(&reader->cs);
    *count = reader->stream_count;
    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static HRESULT WINAPI profile_GetStream(IWMProfile3 *iface, DWORD index, IWMStreamConfig **config)
{
    struct wm_reader *reader = impl_from_IWMProfile3(iface);
    struct stream_config *object;
    struct wm_stream *stream;
    HRESULT hr;

    TRACE("reader %p, index %lu, config %p.\n", reader, index, config);

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;
    object->IWMStreamConfig_iface.lpVtbl = &stream_config_vtbl;
    object->IWMMediaProps_iface.lpVtbl = &stream_props_vtbl;
    object->refcount = 1;
    object->index = index;

    EnterCriticalSection(&reader->cs);
    if ((stream = wm_reader_get_stream_by_stream_number(reader, index + 1)))
        hr = CopyMediaType(&object->mt, &stream->mt);
    else
        hr = E_INVALIDARG;
    LeaveCriticalSection(&reader->cs);

    if (FAILED(hr))
    {
        free(object);
        return hr;
    }

    TRACE("Created stream config %p.\n", object);
    *config = &object->IWMStreamConfig_iface;
    return S_OK;
}

static HRESULT WINAPI profile_GetStreamByNumber(IWMProfile3 *iface, WORD stream_number, IWMStreamConfig **config)
{
    HRESULT hr;

    TRACE("iface %p, stream_number %u, config %p.\n", iface, stream_number, config);

    if (!stream_number)
        return NS_E_NO_STREAM;

    hr = profile_GetStream(iface, stream_number - 1, config);
    if (hr == E_INVALIDARG)
        hr = NS_E_NO_STREAM;

    return hr;
}

static HRESULT WINAPI profile_RemoveStream(IWMProfile3 *iface, IWMStreamConfig *config)
{
    FIXME("iface %p, config %p, stub!\n", iface, config);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_RemoveStreamByNumber(IWMProfile3 *iface, WORD stream_number)
{
    FIXME("iface %p, stream_number %u, stub!\n", iface, stream_number);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_AddStream(IWMProfile3 *iface, IWMStreamConfig *config)
{
    FIXME("iface %p, config %p, stub!\n", iface, config);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_ReconfigStream(IWMProfile3 *iface, IWMStreamConfig *config)
{
    FIXME("iface %p, config %p, stub!\n", iface, config);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_CreateNewStream(IWMProfile3 *iface, REFGUID type, IWMStreamConfig **config)
{
    FIXME("iface %p, type %s, config %p, stub!\n", iface, debugstr_guid(type), config);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetMutualExclusionCount(IWMProfile3 *iface, DWORD *count)
{
    FIXME("iface %p, count %p, stub!\n", iface, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetMutualExclusion(IWMProfile3 *iface, DWORD index, IWMMutualExclusion **excl)
{
    FIXME("iface %p, index %lu, excl %p, stub!\n", iface, index, excl);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_RemoveMutualExclusion(IWMProfile3 *iface, IWMMutualExclusion *excl)
{
    FIXME("iface %p, excl %p, stub!\n", iface, excl);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_AddMutualExclusion(IWMProfile3 *iface, IWMMutualExclusion *excl)
{
    FIXME("iface %p, excl %p, stub!\n", iface, excl);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_CreateNewMutualExclusion(IWMProfile3 *iface, IWMMutualExclusion **excl)
{
    FIXME("iface %p, excl %p, stub!\n", iface, excl);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetProfileID(IWMProfile3 *iface, GUID *id)
{
    FIXME("iface %p, id %p, stub!\n", iface, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetStorageFormat(IWMProfile3 *iface, WMT_STORAGE_FORMAT *format)
{
    FIXME("iface %p, format %p, stub!\n", iface, format);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_SetStorageFormat(IWMProfile3 *iface, WMT_STORAGE_FORMAT format)
{
    FIXME("iface %p, format %#x, stub!\n", iface, format);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetBandwidthSharingCount(IWMProfile3 *iface, DWORD *count)
{
    FIXME("iface %p, count %p, stub!\n", iface, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetBandwidthSharing(IWMProfile3 *iface, DWORD index, IWMBandwidthSharing **sharing)
{
    FIXME("iface %p, index %lu, sharing %p, stub!\n", iface, index, sharing);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_RemoveBandwidthSharing( IWMProfile3 *iface, IWMBandwidthSharing *sharing)
{
    FIXME("iface %p, sharing %p, stub!\n", iface, sharing);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_AddBandwidthSharing(IWMProfile3 *iface, IWMBandwidthSharing *sharing)
{
    FIXME("iface %p, sharing %p, stub!\n", iface, sharing);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_CreateNewBandwidthSharing( IWMProfile3 *iface, IWMBandwidthSharing **sharing)
{
    FIXME("iface %p, sharing %p, stub!\n", iface, sharing);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetStreamPrioritization(IWMProfile3 *iface, IWMStreamPrioritization **stream)
{
    FIXME("iface %p, stream %p, stub!\n", iface, stream);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_SetStreamPrioritization(IWMProfile3 *iface, IWMStreamPrioritization *stream)
{
    FIXME("iface %p, stream %p, stub!\n", iface, stream);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_RemoveStreamPrioritization(IWMProfile3 *iface)
{
    FIXME("iface %p, stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_CreateNewStreamPrioritization(IWMProfile3 *iface, IWMStreamPrioritization **stream)
{
    FIXME("iface %p, stream %p, stub!\n", iface, stream);
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetExpectedPacketCount(IWMProfile3 *iface, QWORD duration, QWORD *count)
{
    FIXME("iface %p, duration %s, count %p, stub!\n", iface, debugstr_time(duration), count);
    return E_NOTIMPL;
}

static const IWMProfile3Vtbl profile_vtbl =
{
    profile_QueryInterface,
    profile_AddRef,
    profile_Release,
    profile_GetVersion,
    profile_GetName,
    profile_SetName,
    profile_GetDescription,
    profile_SetDescription,
    profile_GetStreamCount,
    profile_GetStream,
    profile_GetStreamByNumber,
    profile_RemoveStream,
    profile_RemoveStreamByNumber,
    profile_AddStream,
    profile_ReconfigStream,
    profile_CreateNewStream,
    profile_GetMutualExclusionCount,
    profile_GetMutualExclusion,
    profile_RemoveMutualExclusion,
    profile_AddMutualExclusion,
    profile_CreateNewMutualExclusion,
    profile_GetProfileID,
    profile_GetStorageFormat,
    profile_SetStorageFormat,
    profile_GetBandwidthSharingCount,
    profile_GetBandwidthSharing,
    profile_RemoveBandwidthSharing,
    profile_AddBandwidthSharing,
    profile_CreateNewBandwidthSharing,
    profile_GetStreamPrioritization,
    profile_SetStreamPrioritization,
    profile_RemoveStreamPrioritization,
    profile_CreateNewStreamPrioritization,
    profile_GetExpectedPacketCount,
};

static struct wm_reader *impl_from_IWMHeaderInfo3(IWMHeaderInfo3 *iface)
{
    return CONTAINING_RECORD(iface, struct wm_reader, IWMHeaderInfo3_iface);
}

static HRESULT WINAPI header_info_QueryInterface(IWMHeaderInfo3 *iface, REFIID iid, void **out)
{
    struct wm_reader *reader = impl_from_IWMHeaderInfo3(iface);
    return IUnknown_QueryInterface(reader->outer, iid, out);
}

static ULONG WINAPI header_info_AddRef(IWMHeaderInfo3 *iface)
{
    struct wm_reader *reader = impl_from_IWMHeaderInfo3(iface);
    return IUnknown_AddRef(reader->outer);
}

static ULONG WINAPI header_info_Release(IWMHeaderInfo3 *iface)
{
    struct wm_reader *reader = impl_from_IWMHeaderInfo3(iface);
    return IUnknown_Release(reader->outer);
}

static HRESULT WINAPI header_info_GetAttributeCount(IWMHeaderInfo3 *iface, WORD stream_number, WORD *count)
{
    FIXME("iface %p, stream_number %u, count %p, stub!\n", iface, stream_number, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetAttributeByIndex(IWMHeaderInfo3 *iface, WORD index, WORD *stream_number,
        WCHAR *name, WORD *name_len, WMT_ATTR_DATATYPE *type, BYTE *value, WORD *size)
{
    FIXME("iface %p, index %u, stream_number %p, name %p, name_len %p, type %p, value %p, size %p, stub!\n",
            iface, index, stream_number, name, name_len, type, value, size);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetAttributeByName(IWMHeaderInfo3 *iface, WORD *stream_number,
        const WCHAR *name, WMT_ATTR_DATATYPE *type, BYTE *value, WORD *size)
{
    struct wm_reader *reader = impl_from_IWMHeaderInfo3(iface);
    const WORD capacity = *size;

    TRACE("reader %p, stream_number %p, name %s, type %p, value %p, size %u.\n",
            reader, stream_number, debugstr_w(name), type, value, *size);

    if (!stream_number)
        return E_INVALIDARG;

    if (!wcscmp(name, L"Duration"))
    {
        QWORD duration;

        if (*stream_number)
        {
            WARN("Requesting duration for stream %u, returning ASF_E_NOTFOUND.\n", *stream_number);
            return ASF_E_NOTFOUND;
        }

        *size = sizeof(QWORD);
        if (!value)
        {
            *type = WMT_TYPE_QWORD;
            return S_OK;
        }
        if (capacity < *size)
            return ASF_E_BUFFERTOOSMALL;

        *type = WMT_TYPE_QWORD;
        EnterCriticalSection(&reader->cs);
        duration = reader->duration;
        LeaveCriticalSection(&reader->cs);
        TRACE("Returning duration %s.\n", debugstr_time(duration));
        memcpy(value, &duration, sizeof(QWORD));
        return S_OK;
    }
    else if (!wcscmp(name, L"Seekable"))
    {
        if (*stream_number)
        {
            WARN("Requesting duration for stream %u, returning ASF_E_NOTFOUND.\n", *stream_number);
            return ASF_E_NOTFOUND;
        }

        *size = sizeof(BOOL);
        if (!value)
        {
            *type = WMT_TYPE_BOOL;
            return S_OK;
        }
        if (capacity < *size)
            return ASF_E_BUFFERTOOSMALL;

        *type = WMT_TYPE_BOOL;
        *(BOOL *)value = TRUE;
        return S_OK;
    }
    else
    {
        FIXME("Unknown attribute %s.\n", debugstr_w(name));
        return ASF_E_NOTFOUND;
    }
}

static HRESULT WINAPI header_info_SetAttribute(IWMHeaderInfo3 *iface, WORD stream_number,
        const WCHAR *name, WMT_ATTR_DATATYPE type, const BYTE *value, WORD size)
{
    FIXME("iface %p, stream_number %u, name %s, type %#x, value %p, size %u, stub!\n",
            iface, stream_number, debugstr_w(name), type, value, size);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetMarkerCount(IWMHeaderInfo3 *iface, WORD *count)
{
    FIXME("iface %p, count %p, stub!\n", iface, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetMarker(IWMHeaderInfo3 *iface,
        WORD index, WCHAR *name, WORD *len, QWORD *time)
{
    FIXME("iface %p, index %u, name %p, len %p, time %p, stub!\n", iface, index, name, len, time);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_AddMarker(IWMHeaderInfo3 *iface, const WCHAR *name, QWORD time)
{
    FIXME("iface %p, name %s, time %s, stub!\n", iface, debugstr_w(name), debugstr_time(time));
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_RemoveMarker(IWMHeaderInfo3 *iface, WORD index)
{
    FIXME("iface %p, index %u, stub!\n", iface, index);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetScriptCount(IWMHeaderInfo3 *iface, WORD *count)
{
    FIXME("iface %p, count %p, stub!\n", iface, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetScript(IWMHeaderInfo3 *iface, WORD index, WCHAR *type,
        WORD *type_len, WCHAR *command, WORD *command_len, QWORD *time)
{
    FIXME("iface %p, index %u, type %p, type_len %p, command %p, command_len %p, time %p, stub!\n",
            iface, index, type, type_len, command, command_len, time);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_AddScript(IWMHeaderInfo3 *iface,
        const WCHAR *type, const WCHAR *command, QWORD time)
{
    FIXME("iface %p, type %s, command %s, time %s, stub!\n",
            iface, debugstr_w(type), debugstr_w(command), debugstr_time(time));
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_RemoveScript(IWMHeaderInfo3 *iface, WORD index)
{
    FIXME("iface %p, index %u, stub!\n", iface, index);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetCodecInfoCount(IWMHeaderInfo3 *iface, DWORD *count)
{
    FIXME("iface %p, count %p, stub!\n", iface, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetCodecInfo(IWMHeaderInfo3 *iface, DWORD index, WORD *name_len,
        WCHAR *name, WORD *desc_len, WCHAR *desc, WMT_CODEC_INFO_TYPE *type, WORD *size, BYTE *info)
{
    FIXME("iface %p, index %lu, name_len %p, name %p, desc_len %p, desc %p, type %p, size %p, info %p, stub!\n",
            iface, index, name_len, name, desc_len, desc, type, size, info);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetAttributeCountEx(IWMHeaderInfo3 *iface, WORD stream_number, WORD *count)
{
    FIXME("iface %p, stream_number %u, count %p, stub!\n", iface, stream_number, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetAttributeIndices(IWMHeaderInfo3 *iface, WORD stream_number,
        const WCHAR *name, WORD *lang_index, WORD *indices, WORD *count)
{
    FIXME("iface %p, stream_number %u, name %s, lang_index %p, indices %p, count %p, stub!\n",
            iface, stream_number, debugstr_w(name), lang_index, indices, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetAttributeByIndexEx(IWMHeaderInfo3 *iface,
        WORD stream_number, WORD index, WCHAR *name, WORD *name_len,
        WMT_ATTR_DATATYPE *type, WORD *lang_index, BYTE *value, DWORD *size)
{
    FIXME("iface %p, stream_number %u, index %u, name %p, name_len %p,"
            " type %p, lang_index %p, value %p, size %p, stub!\n",
            iface, stream_number, index, name, name_len, type, lang_index, value, size);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_ModifyAttribute(IWMHeaderInfo3 *iface, WORD stream_number,
        WORD index, WMT_ATTR_DATATYPE type, WORD lang_index, const BYTE *value, DWORD size)
{
    FIXME("iface %p, stream_number %u, index %u, type %#x, lang_index %u, value %p, size %lu, stub!\n",
            iface, stream_number, index, type, lang_index, value, size);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_AddAttribute(IWMHeaderInfo3 *iface,
        WORD stream_number, const WCHAR *name, WORD *index,
        WMT_ATTR_DATATYPE type, WORD lang_index, const BYTE *value, DWORD size)
{
    FIXME("iface %p, stream_number %u, name %s, index %p, type %#x, lang_index %u, value %p, size %lu, stub!\n",
            iface, stream_number, debugstr_w(name), index, type, lang_index, value, size);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_DeleteAttribute(IWMHeaderInfo3 *iface, WORD stream_number, WORD index)
{
    FIXME("iface %p, stream_number %u, index %u, stub!\n", iface, stream_number, index);
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_AddCodecInfo(IWMHeaderInfo3 *iface, const WCHAR *name,
        const WCHAR *desc, WMT_CODEC_INFO_TYPE type, WORD size, BYTE *info)
{
    FIXME("iface %p, name %s, desc %s, type %#x, size %u, info %p, stub!\n",
            info, debugstr_w(name), debugstr_w(desc), type, size, info);
    return E_NOTIMPL;
}

static const IWMHeaderInfo3Vtbl header_info_vtbl =
{
    header_info_QueryInterface,
    header_info_AddRef,
    header_info_Release,
    header_info_GetAttributeCount,
    header_info_GetAttributeByIndex,
    header_info_GetAttributeByName,
    header_info_SetAttribute,
    header_info_GetMarkerCount,
    header_info_GetMarker,
    header_info_AddMarker,
    header_info_RemoveMarker,
    header_info_GetScriptCount,
    header_info_GetScript,
    header_info_AddScript,
    header_info_RemoveScript,
    header_info_GetCodecInfoCount,
    header_info_GetCodecInfo,
    header_info_GetAttributeCountEx,
    header_info_GetAttributeIndices,
    header_info_GetAttributeByIndexEx,
    header_info_ModifyAttribute,
    header_info_AddAttribute,
    header_info_DeleteAttribute,
    header_info_AddCodecInfo,
};

static struct wm_reader *impl_from_IWMLanguageList(IWMLanguageList *iface)
{
    return CONTAINING_RECORD(iface, struct wm_reader, IWMLanguageList_iface);
}

static HRESULT WINAPI language_list_QueryInterface(IWMLanguageList *iface, REFIID iid, void **out)
{
    struct wm_reader *reader = impl_from_IWMLanguageList(iface);
    return IUnknown_QueryInterface(reader->outer, iid, out);
}

static ULONG WINAPI language_list_AddRef(IWMLanguageList *iface)
{
    struct wm_reader *reader = impl_from_IWMLanguageList(iface);
    return IUnknown_AddRef(reader->outer);
}

static ULONG WINAPI language_list_Release(IWMLanguageList *iface)
{
    struct wm_reader *reader = impl_from_IWMLanguageList(iface);
    return IUnknown_Release(reader->outer);
}

static HRESULT WINAPI language_list_GetLanguageCount(IWMLanguageList *iface, WORD *count)
{
    FIXME("iface %p, count %p, stub!\n", iface, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI language_list_GetLanguageDetails(IWMLanguageList *iface,
        WORD index, WCHAR *lang, WORD *len)
{
    FIXME("iface %p, index %u, lang %p, len %p, stub!\n", iface, index, lang, len);
    return E_NOTIMPL;
}

static HRESULT WINAPI language_list_AddLanguageByRFC1766String(IWMLanguageList *iface,
        const WCHAR *lang, WORD *index)
{
    FIXME("iface %p, lang %s, index %p, stub!\n", iface, debugstr_w(lang), index);
    return E_NOTIMPL;
}

static const IWMLanguageListVtbl language_list_vtbl =
{
    language_list_QueryInterface,
    language_list_AddRef,
    language_list_Release,
    language_list_GetLanguageCount,
    language_list_GetLanguageDetails,
    language_list_AddLanguageByRFC1766String,
};

static struct wm_reader *impl_from_IWMPacketSize2(IWMPacketSize2 *iface)
{
    return CONTAINING_RECORD(iface, struct wm_reader, IWMPacketSize2_iface);
}

static HRESULT WINAPI packet_size_QueryInterface(IWMPacketSize2 *iface, REFIID iid, void **out)
{
    struct wm_reader *reader = impl_from_IWMPacketSize2(iface);
    return IUnknown_QueryInterface(reader->outer, iid, out);
}

static ULONG WINAPI packet_size_AddRef(IWMPacketSize2 *iface)
{
    struct wm_reader *reader = impl_from_IWMPacketSize2(iface);
    return IUnknown_AddRef(reader->outer);
}

static ULONG WINAPI packet_size_Release(IWMPacketSize2 *iface)
{
    struct wm_reader *reader = impl_from_IWMPacketSize2(iface);
    return IUnknown_Release(reader->outer);
}

static HRESULT WINAPI packet_size_GetMaxPacketSize(IWMPacketSize2 *iface, DWORD *size)
{
    FIXME("iface %p, size %p, stub!\n", iface, size);
    return E_NOTIMPL;
}

static HRESULT WINAPI packet_size_SetMaxPacketSize(IWMPacketSize2 *iface, DWORD size)
{
    FIXME("iface %p, size %lu, stub!\n", iface, size);
    return E_NOTIMPL;
}

static HRESULT WINAPI packet_size_GetMinPacketSize(IWMPacketSize2 *iface, DWORD *size)
{
    FIXME("iface %p, size %p, stub!\n", iface, size);
    return E_NOTIMPL;
}

static HRESULT WINAPI packet_size_SetMinPacketSize(IWMPacketSize2 *iface, DWORD size)
{
    FIXME("iface %p, size %lu, stub!\n", iface, size);
    return E_NOTIMPL;
}

static const IWMPacketSize2Vtbl packet_size_vtbl =
{
    packet_size_QueryInterface,
    packet_size_AddRef,
    packet_size_Release,
    packet_size_GetMaxPacketSize,
    packet_size_SetMaxPacketSize,
    packet_size_GetMinPacketSize,
    packet_size_SetMinPacketSize,
};

static struct wm_reader *impl_from_IWMReaderPlaylistBurn(IWMReaderPlaylistBurn *iface)
{
    return CONTAINING_RECORD(iface, struct wm_reader, IWMReaderPlaylistBurn_iface);
}

static HRESULT WINAPI playlist_QueryInterface(IWMReaderPlaylistBurn *iface, REFIID iid, void **out)
{
    struct wm_reader *reader = impl_from_IWMReaderPlaylistBurn(iface);
    return IUnknown_QueryInterface(reader->outer, iid, out);
}

static ULONG WINAPI playlist_AddRef(IWMReaderPlaylistBurn *iface)
{
    struct wm_reader *reader = impl_from_IWMReaderPlaylistBurn(iface);
    return IUnknown_AddRef(reader->outer);
}

static ULONG WINAPI playlist_Release(IWMReaderPlaylistBurn *iface)
{
    struct wm_reader *reader = impl_from_IWMReaderPlaylistBurn(iface);
    return IUnknown_Release(reader->outer);
}

static HRESULT WINAPI playlist_InitPlaylistBurn(IWMReaderPlaylistBurn *iface, DWORD count,
        const WCHAR **filenames, IWMStatusCallback *callback, void *context)
{
    FIXME("iface %p, count %lu, filenames %p, callback %p, context %p, stub!\n",
            iface, count, filenames, callback, context);
    return E_NOTIMPL;
}

static HRESULT WINAPI playlist_GetInitResults(IWMReaderPlaylistBurn *iface, DWORD count, HRESULT *hrs)
{
    FIXME("iface %p, count %lu, hrs %p, stub!\n", iface, count, hrs);
    return E_NOTIMPL;
}

static HRESULT WINAPI playlist_Cancel(IWMReaderPlaylistBurn *iface)
{
    FIXME("iface %p, stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI playlist_EndPlaylistBurn(IWMReaderPlaylistBurn *iface, HRESULT hr)
{
    FIXME("iface %p, hr %#lx, stub!\n", iface, hr);
    return E_NOTIMPL;
}

static const IWMReaderPlaylistBurnVtbl playlist_vtbl =
{
    playlist_QueryInterface,
    playlist_AddRef,
    playlist_Release,
    playlist_InitPlaylistBurn,
    playlist_GetInitResults,
    playlist_Cancel,
    playlist_EndPlaylistBurn,
};

static struct wm_reader *impl_from_IWMReaderTimecode(IWMReaderTimecode *iface)
{
    return CONTAINING_RECORD(iface, struct wm_reader, IWMReaderTimecode_iface);
}

static HRESULT WINAPI timecode_QueryInterface(IWMReaderTimecode *iface, REFIID iid, void **out)
{
    struct wm_reader *reader = impl_from_IWMReaderTimecode(iface);
    return IUnknown_QueryInterface(reader->outer, iid, out);
}

static ULONG WINAPI timecode_AddRef(IWMReaderTimecode *iface)
{
    struct wm_reader *reader = impl_from_IWMReaderTimecode(iface);
    return IUnknown_AddRef(reader->outer);
}

static ULONG WINAPI timecode_Release(IWMReaderTimecode *iface)
{
    struct wm_reader *reader = impl_from_IWMReaderTimecode(iface);
    return IUnknown_Release(reader->outer);
}

static HRESULT WINAPI timecode_GetTimecodeRangeCount(IWMReaderTimecode *iface,
        WORD stream_number, WORD *count)
{
    FIXME("iface %p, stream_number %u, count %p, stub!\n", iface, stream_number, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI timecode_GetTimecodeRangeBounds(IWMReaderTimecode *iface,
        WORD stream_number, WORD index, DWORD *start, DWORD *end)
{
    FIXME("iface %p, stream_number %u, index %u, start %p, end %p, stub!\n",
            iface, stream_number, index, start, end);
    return E_NOTIMPL;
}

static const IWMReaderTimecodeVtbl timecode_vtbl =
{
    timecode_QueryInterface,
    timecode_AddRef,
    timecode_Release,
    timecode_GetTimecodeRangeCount,
    timecode_GetTimecodeRangeBounds,
};

static HRESULT stream_init_decoder(struct wm_stream *stream)
{
    DMO_PARTIAL_MEDIATYPE input_type = {0};
    IEnumDMO *enum_dmo;
    HRESULT hr;
    GUID clsid;

    strmbase_dump_media_type(&stream->mt);

    input_type.type = stream->mt.majortype;
    input_type.subtype = stream->mt.subtype;
    if (IsEqualGUID(&stream->mt.majortype, &MEDIATYPE_Video))
        hr = DMOEnum(&DMOCATEGORY_VIDEO_DECODER, 0, 1, &input_type, 0, NULL, &enum_dmo);
    else if (IsEqualGUID(&stream->mt.majortype, &MEDIATYPE_Audio))
        hr = DMOEnum(&DMOCATEGORY_AUDIO_DECODER, 0, 1, &input_type, 0, NULL, &enum_dmo);
    else
    {
        WARN("Ignoring unknown major type %s\n", debugstr_guid(&stream->mt.majortype));
        return S_OK;
    }

    if (SUCCEEDED(hr))
    {
        do
        {
            if ((hr = IEnumDMO_Next(enum_dmo, 1, &clsid, NULL, NULL)) != S_OK)
                break;
            hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IMediaObject, (void **)&stream->decoder);
        } while (FAILED(hr));

        IEnumDMO_Release(enum_dmo);
    }

    if (FAILED(hr) || !stream->decoder)
    {
        WARN("Failed to find or create a decoder, hr %#lx\n", hr);
        stream->read_compressed = true;
        return S_OK;
    }

    if (FAILED(hr = IMediaObject_SetInputType(stream->decoder, 0, &stream->mt, 0)))
    {
        ERR("Failed to set the decoder input type, hr %#lx\n", hr);
        return hr;
    }

    if (IsEqualGUID(&stream->mt.majortype, &MEDIATYPE_Video))
    {
        /* Call of Juarez: Bound in Blood breaks if I420 is enumerated.
         * Some native decoders output I420, but the msmpeg4v3 decoder
         * never does.
         *
         * Shadowgrounds provides wmv3 video and assumes that the initial
         * video type will be BGR. */
        const VIDEOINFOHEADER *stream_vih = (VIDEOINFOHEADER *)stream->mt.pbFormat;
        VIDEOINFOHEADER vih =
        {
            .bmiHeader =
            {
                .biSize = sizeof(vih.bmiHeader),
                .biWidth = stream_vih->bmiHeader.biWidth,
                .biHeight = abs(stream_vih->bmiHeader.biHeight),
                .biPlanes = 1,
                .biBitCount = 24,
                .biCompression = BI_RGB,
            },
            .rcSource = stream_vih->rcSource,
            .rcTarget = stream_vih->rcTarget,
        };
        AM_MEDIA_TYPE mt =
        {
            .majortype = MEDIATYPE_Video,
            .subtype = MEDIASUBTYPE_RGB24,
            .bFixedSizeSamples = TRUE,
            .formattype = FORMAT_VideoInfo,
            .cbFormat = sizeof(vih),
            .pbFormat = (BYTE *)&vih,
        };

        vih.bmiHeader.biSizeImage = vih.bmiHeader.biHeight * vih.bmiHeader.biWidth * 4;

        strmbase_dump_media_type((AM_MEDIA_TYPE *)&mt);
        if (FAILED(hr = IMediaObject_SetOutputType(stream->decoder, 0, &mt, 0)))
            ERR("Failed to set decoder output type %#lx\n", hr );
    }
    else if (IsEqualGUID(&stream->mt.majortype, &MEDIATYPE_Audio))
    {
        /* R.U.S.E enumerates available audio types, picks the first one it
         * likes, and then sets the wrong stream to that type. libav might
         * give us WG_AUDIO_FORMAT_F32LE by default, which will result in
         * the game incorrectly interpreting float data as integer.
         * Therefore just match native and always set our default format to
         * S16LE. */
        const WAVEFORMATEX *stream_wfx = (WAVEFORMATEX *)stream->mt.pbFormat;
        WAVEFORMATEX wfx =
        {
            .wFormatTag = WAVE_FORMAT_PCM,
            .wBitsPerSample = 16,
            .nChannels = stream_wfx->nChannels ? stream_wfx->nChannels : 2,
            .nSamplesPerSec = stream_wfx->nSamplesPerSec ? stream_wfx->nSamplesPerSec : 44100,
        };
        AM_MEDIA_TYPE mt =
        {
            .majortype = MEDIATYPE_Audio,
            .subtype = MEDIASUBTYPE_PCM,
            .bFixedSizeSamples = TRUE,
            .formattype = FORMAT_WaveFormatEx,
            .cbFormat = sizeof(wfx),
            .pbFormat = (BYTE *)&wfx,
        };

        wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8,
        wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec,

        strmbase_dump_media_type(&mt);
        if (FAILED(hr = IMediaObject_SetOutputType(stream->decoder, 0, &mt, 0)))
            ERR("Failed to set decoder output type %#lx\n", hr );
    }

    return hr;
}

static HRESULT media_type_from_winedmo_format( GUID major, union winedmo_format *format, WM_MEDIA_TYPE *mt, DWORD *size )
{
    if (IsEqualGUID( &major, &MEDIATYPE_Video ))
        return media_type_from_mf_video_format( &format->video, mt, size );
    if (IsEqualGUID( &major, &MEDIATYPE_Audio ))
        return media_type_from_wave_format_ex( &format->audio, mt, size );

    FIXME( "Unsupported major type %s\n", debugstr_guid( &major ) );
    return E_NOTIMPL;
}

static HRESULT get_stream_media_type(struct winedmo_demuxer demuxer, UINT index, WM_MEDIA_TYPE *mt, DWORD *size)
{
    union winedmo_format *format;
    NTSTATUS status;
    HRESULT hr;
    GUID major;

    if ((status = winedmo_demuxer_stream_type(demuxer, index, &major, &format)))
        hr = HRESULT_FROM_NT(status);
    else
    {
        hr = media_type_from_winedmo_format(major, format, mt, size);
        free(format);
    }

    return hr;
}

static HRESULT init_stream(struct wm_reader *reader)
{
    HRESULT hr;
    WORD i;

    if (!(reader->streams = calloc(reader->stream_count, sizeof(*reader->streams))))
        hr = E_OUTOFMEMORY;

    for (i = 0; i < reader->stream_count; ++i)
    {
        struct wm_stream *stream = &reader->streams[i];
        WM_MEDIA_TYPE *mt;
        DWORD size = 0;

        stream->reader = reader;
        stream->index = i;
        stream->selection = WMT_ON;
        list_init(&stream->compressed_samples);

        if (SUCCEEDED(hr = get_stream_media_type(reader->winedmo_demuxer, i, NULL, &size)))
        {
            if (!(mt = malloc(size)))
                hr = E_OUTOFMEMORY;
            else
                hr = get_stream_media_type(reader->winedmo_demuxer, i, mt, &size);
        }

        if (SUCCEEDED(hr))
        {
            CopyMediaType(&stream->mt, (AM_MEDIA_TYPE *)mt);
            strmbase_dump_media_type(&stream->mt);
            hr = stream_init_decoder(stream);
        }
    }

    return S_OK;
}

static HRESULT wm_stream_allocate_sample(struct wm_stream *stream, DWORD size, INSSBuffer **out)
{
    if (!stream->read_compressed && stream->output_allocator)
        return IWMReaderAllocatorEx_AllocateForOutputEx(stream->output_allocator, stream->index,
                size, out, 0, 0, 0, NULL);
    if (stream->read_compressed && stream->stream_allocator)
        return IWMReaderAllocatorEx_AllocateForStreamEx(stream->stream_allocator, stream->index + 1,
                size, out, 0, 0, 0, NULL);

    return E_FAIL;
}

static HRESULT stream_pop_compressed_sample(struct wm_stream *stream, struct sample **out)
{
    struct sample *sample;
    struct list *ptr;

    if (!(ptr = list_head(&stream->compressed_samples)))
        return E_PENDING;
    sample = LIST_ENTRY(ptr, struct sample, entry);
    list_remove(&sample->entry);

    *out = sample;
    return S_OK;
}

static void stream_queue_compressed_sample(struct wm_stream *stream, struct sample *sample)
{
    list_add_tail(&stream->compressed_samples, &sample->entry);
    INSSBuffer_AddRef(&sample->INSSBuffer_iface);
}

static HRESULT stream_read_compressed_sample(struct wm_stream *stream, INSSBuffer **out, QWORD *pts, QWORD *duration, DWORD *flags)
{
    struct sample *sample;
    DWORD length;
    HRESULT hr;
    BYTE *data;

    if (FAILED(stream_pop_compressed_sample(stream, &sample)))
        return S_FALSE;

    INSSBuffer_GetLength(&sample->INSSBuffer_iface, &length);
    if (FAILED(hr = wm_stream_allocate_sample(stream, length, out)))
    {
        INSSBuffer_AddRef(&sample->INSSBuffer_iface);
        *out = &sample->INSSBuffer_iface;
        hr = S_OK;
    }
    else if (SUCCEEDED(hr = INSSBuffer_GetBuffer(*out, &data)))
    {
        memcpy(data, sample->data, sample->size);
        INSSBuffer_SetLength(*out, sample->size);
    }

    *pts = 0;
    *flags = 0;
    *duration = 0;
    if (sample->flags & DMO_OUTPUT_DATA_BUFFERF_TIME) *pts = sample->time;
    if (sample->flags & DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH) *duration = sample->duration;
    if (sample->flags & DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT) *flags |= WM_SF_CLEANPOINT;
    if (!*duration) *duration = 10000;

    if (stream->state == STREAM_STARTING) *flags |= WM_SF_DISCONTINUITY;
    stream->state = STREAM_RUNNING;

    INSSBuffer_Release(&sample->INSSBuffer_iface);
    return hr;
}

static HRESULT stream_get_next_queued_sample(struct wm_stream *stream, INSSBuffer **out,
        QWORD *pts, QWORD *duration, DWORD *flags)
{
    DMO_OUTPUT_DATA_BUFFER output = {0};
    DWORD status, size, align;
    HRESULT hr;

    if (stream->read_compressed)
        return stream_read_compressed_sample(stream, out, pts, duration, flags);

    IMediaObject_GetOutputSizeInfo(stream->decoder, 0, &size, &align);
    if (FAILED(hr = wm_stream_allocate_sample(stream, size, out))
            && FAILED(hr = sample_create(size, out)))
        return hr;
    if (SUCCEEDED(hr = media_buffer_create(*out, &output.pBuffer)))
    {
        hr = IMediaObject_ProcessOutput(stream->decoder, 0, 1, &output, &status);
        IMediaBuffer_Release(output.pBuffer);
    }
    if (FAILED(hr) || hr == S_FALSE)
    {
        INSSBuffer_Release(*out);
        *out = NULL;
        return hr;
    }

    *pts = 0;
    *flags = 0;
    *duration = 0;
    if (output.dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIME) *pts = output.rtTimestamp;
    if (output.dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH) *duration = output.rtTimelength;
    if (output.dwStatus & DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT) *flags |= WM_SF_CLEANPOINT;
    if (!*duration) *duration = 10000;

    if (stream->state == STREAM_STARTING) *flags |= WM_SF_DISCONTINUITY;
    stream->state = STREAM_RUNNING;

    return S_OK;
}

static HRESULT reader_get_next_queued_sample(struct wm_reader *reader, struct wm_stream **stream,
        INSSBuffer **buffer, QWORD *pts, QWORD *duration, DWORD *flags)
{
    HRESULT hr;
    UINT i;

    if (*stream)
        return stream_get_next_queued_sample(*stream, buffer, pts, duration, flags);

    for (i = 0, hr = S_FALSE; i < reader->stream_count && hr == S_FALSE; i++)
    {
        if ((hr = stream_get_next_queued_sample(&reader->streams[i], buffer, pts, duration, flags)) == S_OK)
            *stream = &reader->streams[i];
    }

    return hr;
}

static struct wm_reader *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct wm_reader, IUnknown_inner);
}

static HRESULT WINAPI unknown_inner_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    struct wm_reader *reader = impl_from_IUnknown(iface);

    TRACE("reader %p, iid %s, out %p.\n", reader, debugstr_guid(iid), out);

    if (IsEqualIID(iid, &IID_IUnknown)
            || IsEqualIID(iid, &IID_IWMSyncReader)
            || IsEqualIID(iid, &IID_IWMSyncReader2))
        *out = &reader->IWMSyncReader2_iface;
    else if (IsEqualIID(iid, &IID_IWMHeaderInfo)
            || IsEqualIID(iid, &IID_IWMHeaderInfo2)
            || IsEqualIID(iid, &IID_IWMHeaderInfo3))
        *out = &reader->IWMHeaderInfo3_iface;
    else if (IsEqualIID(iid, &IID_IWMLanguageList))
        *out = &reader->IWMLanguageList_iface;
    else if (IsEqualIID(iid, &IID_IWMPacketSize)
            || IsEqualIID(iid, &IID_IWMPacketSize2))
        *out = &reader->IWMPacketSize2_iface;
    else if (IsEqualIID(iid, &IID_IWMProfile)
            || IsEqualIID(iid, &IID_IWMProfile2)
            || IsEqualIID(iid, &IID_IWMProfile3))
        *out = &reader->IWMProfile3_iface;
    else if (IsEqualIID(iid, &IID_IWMReaderPlaylistBurn))
        *out = &reader->IWMReaderPlaylistBurn_iface;
    else if (IsEqualIID(iid, &IID_IWMReaderTimecode))
        *out = &reader->IWMReaderTimecode_iface;
    else
    {
        FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        *out = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI unknown_inner_AddRef(IUnknown *iface)
{
    struct wm_reader *reader = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&reader->refcount);
    TRACE("%p increasing refcount to %lu.\n", reader, refcount);
    return refcount;
}

static ULONG WINAPI unknown_inner_Release(IUnknown *iface)
{
    struct wm_reader *reader = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&reader->refcount);

    TRACE("%p decreasing refcount to %lu.\n", reader, refcount);

    if (!refcount)
    {
        while (reader->stream_count--)
        {
            struct wm_stream *stream = reader->streams + reader->stream_count;
            struct sample *sample;
            while (SUCCEEDED(stream_pop_compressed_sample(stream, &sample)))
                INSSBuffer_Release(&sample->INSSBuffer_iface);
            if (stream->decoder)
                IMediaObject_Release(stream->decoder);
            FreeMediaType(&stream->mt);
        }
        free(reader->streams);

        IWMSyncReader2_Close(&reader->IWMSyncReader2_iface);

        reader->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&reader->cs);
        reader->shutdown_cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&reader->shutdown_cs);

        free(reader);
    }

    return refcount;
}

static const IUnknownVtbl unknown_inner_vtbl =
{
    unknown_inner_QueryInterface,
    unknown_inner_AddRef,
    unknown_inner_Release,
};

static struct wm_reader *impl_from_IWMSyncReader2(IWMSyncReader2 *iface)
{
    return CONTAINING_RECORD(iface, struct wm_reader, IWMSyncReader2_iface);
}

static HRESULT WINAPI reader_QueryInterface(IWMSyncReader2 *iface, REFIID iid, void **out)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    return IUnknown_QueryInterface(reader->outer, iid, out);
}

static ULONG WINAPI reader_AddRef(IWMSyncReader2 *iface)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    return IUnknown_AddRef(reader->outer);
}

static ULONG WINAPI reader_Release(IWMSyncReader2 *iface)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    return IUnknown_Release(reader->outer);
}

static HRESULT WINAPI reader_Close(IWMSyncReader2 *iface)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);

    TRACE("reader %p.\n", reader);

    EnterCriticalSection(&reader->cs);

    if (!reader->stream && !reader->file)
    {
        LeaveCriticalSection(&reader->cs);
        return NS_E_INVALID_REQUEST;
    }

    winedmo_demuxer_destroy(&reader->winedmo_demuxer);

    if (reader->stream)
    {
        IStream_Release(reader->stream);
        reader->stream = NULL;
    }

    if (reader->file)
    {
        CloseHandle(reader->file);
        reader->file = NULL;
    }

    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static HRESULT WINAPI reader_GetMaxOutputSampleSize(IWMSyncReader2 *iface, DWORD output, DWORD *max)
{
    struct wm_reader *This = impl_from_IWMSyncReader2(iface);
    FIXME("(%p)->(%lu %p): stub!\n", This, output, max);
    return E_NOTIMPL;
}

static HRESULT WINAPI reader_GetMaxStreamSampleSize(IWMSyncReader2 *iface, WORD stream_number, DWORD *size)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    DWORD max_lookahead, alignment;
    struct wm_stream *stream;
    HRESULT hr = S_OK;

    TRACE("reader %p, stream_number %u, size %p.\n", reader, stream_number, size);

    EnterCriticalSection(&reader->cs);

    if (!(stream = wm_reader_get_stream_by_stream_number(reader, stream_number)))
    {
        LeaveCriticalSection(&reader->cs);
        return E_INVALIDARG;
    }

    if (!stream->decoder)
        *size = 0x10000;
    else if (stream->read_compressed)
        hr = IMediaObject_GetInputSizeInfo(stream->decoder, 0, size, &max_lookahead, &alignment);
    else
        hr = IMediaObject_GetOutputSizeInfo(stream->decoder, 0, size, &alignment);

    LeaveCriticalSection(&reader->cs);
    return hr;
}

static HRESULT demuxer_read_sample(struct winedmo_demuxer demuxer, UINT *index, struct sample **out)
{
    DMO_OUTPUT_DATA_BUFFER output = {0};
    UINT sample_size = 0x1000;
    INSSBuffer *iface;
    NTSTATUS status;
    HRESULT hr;

    do
    {
        if (FAILED(hr = sample_create(sample_size, &iface)))
            return hr;
        if (FAILED(hr = media_buffer_create(iface, &output.pBuffer)))
        {
            INSSBuffer_Release(iface);
            return hr;
        }

        if ((status = winedmo_demuxer_read(demuxer, index, &output, &sample_size)))
        {
            if (status == STATUS_BUFFER_TOO_SMALL) hr = S_FALSE;
            else if (status == STATUS_END_OF_FILE) hr = NS_E_NO_MORE_SAMPLES;
            else hr = HRESULT_FROM_NT(status);
            INSSBuffer_Release(iface);
        }

        IMediaBuffer_Release(output.pBuffer);
    } while (hr == S_FALSE);

    if (SUCCEEDED(hr))
    {
        struct sample *sample = impl_from_INSSBuffer(iface);
        sample->flags = output.dwStatus;
        sample->time = output.rtTimestamp;
        sample->duration = output.rtTimelength;
        *out = sample;
    }

    return hr;
}

static HRESULT WINAPI reader_GetNextSample(IWMSyncReader2 *iface,
        WORD stream_number, INSSBuffer **buffer, QWORD *pts, QWORD *duration,
        DWORD *flags, DWORD *output_number, WORD *ret_stream_number)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream, *other;
    HRESULT hr = S_FALSE;

    TRACE("reader %p, stream_number %u, buffer %p, pts %p, duration %p,"
            " flags %p, output_number %p, ret_stream_number %p.\n",
            reader, stream_number, buffer, pts, duration, flags, output_number, ret_stream_number);

    if (!stream_number && !output_number && !ret_stream_number)
        return E_INVALIDARG;

    EnterCriticalSection(&reader->cs);

    if (!stream_number)
        stream = NULL;
    else if (!(stream = wm_reader_get_stream_by_stream_number(reader, stream_number)))
        hr = E_INVALIDARG;
    else if (stream->selection == WMT_OFF)
        hr = NS_E_INVALID_REQUEST;
    else if (stream->state == STREAM_ENDED)
        hr = NS_E_NO_MORE_SAMPLES;
    else
        hr = reader_get_next_queued_sample(reader, &stream, buffer, pts, duration, flags);

    while (hr == S_FALSE)
    {
        UINT32 stream_index = stream ? stream->index : 0;
        struct sample *sample;

        if (!stream || FAILED(hr = stream_pop_compressed_sample(stream, &sample)))
            hr = demuxer_read_sample(reader->winedmo_demuxer, &stream_index, &sample);
        if (FAILED(hr))
        {
            hr = NS_E_NO_MORE_SAMPLES;
            ERR("failed %#lx\n", hr);
            break;
        }

        ERR("got compressed sample pts %I64d duration %I64d for stream %u\n", sample->time, sample->duration, stream_index);
        if ((other = wm_reader_get_stream_by_stream_number(reader, stream_index + 1)))
        {
            IMediaBuffer *buffer;

            if (other->selection == WMT_OFF)
            {
                INSSBuffer_Release(&sample->INSSBuffer_iface);
                hr = S_FALSE;
                continue;
            }

            if (other->read_compressed)
                stream_queue_compressed_sample(other, sample);
            else if (SUCCEEDED(hr = media_buffer_create(&sample->INSSBuffer_iface, &buffer)))
            {
                if (FAILED(hr = IMediaObject_ProcessInput(other->decoder, 0, buffer, 0, sample->time, sample->duration)))
                {
                    ERR("IMediaObject_ProcessInput %p hr %#lx\n", other->decoder, hr);
                    stream_queue_compressed_sample(other, sample);
                }
                IMediaBuffer_Release(buffer);
            }
        }

        hr = reader_get_next_queued_sample(reader, &stream, buffer, pts, duration, flags);
        INSSBuffer_Release(&sample->INSSBuffer_iface);
    }

    if (hr == S_OK)
    {
        ERR("got sample for stream %u pts %I64d, duration %I64d\n", stream->index, pts ? *pts : 0, duration ? *duration : 0);
        stream_number = stream->index + 1;
    }

    if (stream && hr == NS_E_NO_MORE_SAMPLES)
        stream->state = STREAM_ENDED;

    if (output_number && hr == S_OK)
        *output_number = stream_number - 1;
    if (ret_stream_number && (hr == S_OK || stream_number))
        *ret_stream_number = stream_number;

    LeaveCriticalSection(&reader->cs);
    return hr;
}

static HRESULT WINAPI reader_GetOutputCount(IWMSyncReader2 *iface, DWORD *count)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);

    TRACE("reader %p, count %p.\n", reader, count);

    EnterCriticalSection(&reader->cs);
    *count = reader->stream_count;
    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static HRESULT WINAPI reader_GetOutputFormat(IWMSyncReader2 *iface, DWORD output,
        DWORD index, IWMOutputMediaProps **props)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;
    HRESULT hr;

    TRACE("reader %p, output %lu, index %lu, props %p.\n", reader, output, index, props);

    EnterCriticalSection(&reader->cs);
    if (!(stream = get_stream_by_output_number(reader, output)))
        hr = E_INVALIDARG;
    else if (stream->decoder && !stream->read_compressed)
    {
        AM_MEDIA_TYPE mt;

        if (FAILED(hr = IMediaObject_GetOutputType(stream->decoder, 0, index, &mt)))
            hr = NS_E_INVALID_OUTPUT_FORMAT;
        else
        {
            hr = output_props_create(&mt, props);
            FreeMediaType(&mt);
        }
    }
    else if (!index)
    {
        hr = output_props_create(&stream->mt, props);
    }
    else
    {
        hr = NS_E_INVALID_OUTPUT_FORMAT;
    }
    LeaveCriticalSection(&reader->cs);

    if (FAILED(hr))
        ERR("Failed to get media type, hr %#lx\n", hr);
    return hr;
}

static HRESULT WINAPI reader_GetOutputFormatCount(IWMSyncReader2 *iface, DWORD output, DWORD *count)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;
    HRESULT hr = S_OK;

    TRACE("reader %p, output %lu, count %p.\n", reader, output, count);

    EnterCriticalSection(&reader->cs);
    if (!(stream = get_stream_by_output_number(reader, output)))
        hr = E_INVALIDARG;
    else if (stream->decoder && !stream->read_compressed)
    {
        DWORD index = 0;

        while (SUCCEEDED(IMediaObject_GetOutputType(stream->decoder, 0, index, NULL)))
            index++;
        *count = index;
    }
    else
    {
        *count = 1;
    }
    LeaveCriticalSection(&reader->cs);

    return hr;
}

static HRESULT WINAPI reader_GetOutputNumberForStream(IWMSyncReader2 *iface,
        WORD stream_number, DWORD *output)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);

    TRACE("reader %p, stream_number %u, output %p.\n", reader, stream_number, output);

    *output = stream_number - 1;
    return S_OK;
}

static HRESULT WINAPI reader_GetOutputProps(IWMSyncReader2 *iface, DWORD output,
        IWMOutputMediaProps **props)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;
    DMO_MEDIA_TYPE mt;
    HRESULT hr;

    TRACE("reader %p, output %lu, props %p.\n", reader, output, props);

    EnterCriticalSection(&reader->cs);
    if (!(stream = get_stream_by_output_number(reader, output)))
        hr = E_INVALIDARG;
    else if (!stream->decoder)
        hr = output_props_create(&stream->mt, props);
    else if (SUCCEEDED(hr = IMediaObject_GetOutputCurrentType(stream->decoder, 0, &mt)))
    {
        hr = output_props_create(&mt, props);
        FreeMediaType(&mt);
    }
    LeaveCriticalSection(&reader->cs);

    if (FAILED(hr))
        ERR("Failed to get media type, hr %#lx\n", hr);
    return hr;
}

static HRESULT WINAPI reader_GetOutputSetting(IWMSyncReader2 *iface, DWORD output_num, const WCHAR *name,
        WMT_ATTR_DATATYPE *type, BYTE *value, WORD *length)
{
    struct wm_reader *This = impl_from_IWMSyncReader2(iface);
    FIXME("(%p)->(%lu %s %p %p %p): stub!\n", This, output_num, debugstr_w(name), type, value, length);
    return E_NOTIMPL;
}

static HRESULT WINAPI reader_GetReadStreamSamples(IWMSyncReader2 *iface, WORD stream_number, BOOL *compressed)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;

    TRACE("reader %p, stream_number %u, compressed %p.\n", reader, stream_number, compressed);

    EnterCriticalSection(&reader->cs);

    if (!(stream = wm_reader_get_stream_by_stream_number(reader, stream_number)))
    {
        LeaveCriticalSection(&reader->cs);
        return E_INVALIDARG;
    }

    *compressed = stream->read_compressed;

    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static HRESULT WINAPI reader_GetStreamNumberForOutput(IWMSyncReader2 *iface,
        DWORD output, WORD *stream_number)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);

    TRACE("reader %p, output %lu, stream_number %p.\n", reader, output, stream_number);

    *stream_number = output + 1;
    return S_OK;
}

static HRESULT WINAPI reader_GetStreamSelected(IWMSyncReader2 *iface,
        WORD stream_number, WMT_STREAM_SELECTION *selection)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;

    TRACE("reader %p, stream_number %u, selection %p.\n", reader, stream_number, selection);

    EnterCriticalSection(&reader->cs);

    if (!(stream = wm_reader_get_stream_by_stream_number(reader, stream_number)))
    {
        LeaveCriticalSection(&reader->cs);
        return E_INVALIDARG;
    }

    *selection = stream->selection;

    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static NTSTATUS CDECL reader_seek_file_cb( struct winedmo_stream *stream, UINT64 *pos )
{
    struct wm_reader *reader = CONTAINING_RECORD(stream, struct wm_reader, winedmo_stream);
    LARGE_INTEGER position = {.QuadPart = *pos};
    if (!SetFilePointerEx(reader->file, position, &position, FILE_BEGIN)) return STATUS_UNSUCCESSFUL;
    *pos = position.QuadPart;
    return STATUS_SUCCESS;
}

static NTSTATUS CDECL reader_read_file_cb( struct winedmo_stream *stream, BYTE *buffer, ULONG *size )
{
    struct wm_reader *reader = CONTAINING_RECORD(stream, struct wm_reader, winedmo_stream);
    if (!ReadFile(reader->file, buffer, *size, size, NULL)) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

static HRESULT WINAPI reader_Open(IWMSyncReader2 *iface, const WCHAR *filename)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    BY_HANDLE_FILE_INFORMATION fi = {0};
    WCHAR mime_type[256];
    HRESULT hr = S_OK;
    NTSTATUS status;
    HANDLE file;

    TRACE("reader %p, filename %s.\n", reader, debugstr_w(filename));

    file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, 0);
    if (file == INVALID_HANDLE_VALUE)
        return HRESULT_FROM_WIN32(GetLastError());
    GetFileInformationByHandle(file, &fi);

    EnterCriticalSection(&reader->cs);

    if (reader->stream || reader->file)
    {
        LeaveCriticalSection(&reader->cs);
        WARN("Stream is already open; returning E_UNEXPECTED.\n");
        CloseHandle(file);
        return E_UNEXPECTED;
    }

    reader->file = file;
    reader->file_size = (UINT64)fi.nFileSizeHigh << 32 | fi.nFileSizeLow;
    reader->winedmo_stream.p_seek = reader_seek_file_cb;
    reader->winedmo_stream.p_read = reader_read_file_cb;

    if ((status = winedmo_demuxer_create(filename, &reader->winedmo_stream, reader->file_size, &reader->duration,
            &reader->stream_count, mime_type, &reader->winedmo_demuxer)))
        hr = HRESULT_FROM_WIN32(status);

    if (FAILED(hr) || FAILED(hr = init_stream(reader)))
    {
        CloseHandle(reader->file);
        reader->file = NULL;
    }

    LeaveCriticalSection(&reader->cs);
    return hr;
}

static NTSTATUS CDECL reader_seek_stream_cb( struct winedmo_stream *stream, UINT64 *pos )
{
    struct wm_reader *reader = CONTAINING_RECORD(stream, struct wm_reader, winedmo_stream);
    LARGE_INTEGER move = {.QuadPart = *pos};
    if (FAILED(IStream_Seek(reader->stream, move, STREAM_SEEK_SET, NULL))) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

static NTSTATUS CDECL reader_read_stream_cb( struct winedmo_stream *stream, BYTE *buffer, ULONG *size )
{
    struct wm_reader *reader = CONTAINING_RECORD(stream, struct wm_reader, winedmo_stream);
    if (FAILED(IStream_Read(reader->stream, buffer, *size, size))) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

static HRESULT WINAPI reader_OpenStream(IWMSyncReader2 *iface, IStream *stream)
{
    static const ULONG64 canary_size = 0xdeadbeeffeedcafe;
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    WCHAR mime_type[256];
    NTSTATUS status;
    STATSTG stat;
    HRESULT hr;

    TRACE("reader %p, stream %p.\n", reader, stream);

    stat.cbSize.QuadPart = canary_size;
    if (FAILED(hr = IStream_Stat(stream, &stat, STATFLAG_NONAME)))
    {
        ERR("Failed to stat stream, hr %#lx.\n", hr);
        return hr;
    }

    if (stat.cbSize.QuadPart == canary_size)
    {
        /* Call of Juarez: Gunslinger implements IStream_Stat as an empty function returning S_OK, leaving
         * the output stat unchanged. Windows doesn't call IStream_Seek(_SEEK_END) and probably validates
         * the size against WMV file headers so the bigger cbSize doesn't change anything.
         * Such streams work as soon as the uninitialized cbSize is big enough which is usually the case
         * (if that is not the case Windows will favour shorter cbSize). */
        static const LARGE_INTEGER zero = { 0 };
        ULARGE_INTEGER pos = { .QuadPart = canary_size };

        if (SUCCEEDED(hr = IStream_Seek(stream, zero, STREAM_SEEK_END, &pos)))
            IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
        stat.cbSize.QuadPart = pos.QuadPart == canary_size ? -1 : pos.QuadPart;
        ERR("IStream_Stat did not fill the stream size, size from _Seek %I64u.\n", stat.cbSize.QuadPart);
    }

    EnterCriticalSection(&reader->cs);

    if (reader->stream || reader->file)
    {
        LeaveCriticalSection(&reader->cs);
        WARN("Stream is already open; returning E_UNEXPECTED.\n");
        return E_UNEXPECTED;
    }

    reader->stream = stream;
    IStream_AddRef(reader->stream);
    reader->file_size = stat.cbSize.QuadPart;
    reader->winedmo_stream.p_seek = reader_seek_stream_cb;
    reader->winedmo_stream.p_read = reader_read_stream_cb;

    if ((status = winedmo_demuxer_create(NULL, &reader->winedmo_stream, reader->file_size, &reader->duration,
            &reader->stream_count, mime_type, &reader->winedmo_demuxer)))
        hr = HRESULT_FROM_WIN32(status);

    if (FAILED(hr) || FAILED(hr = init_stream(reader)))
    {
        IStream_Release(reader->stream);
        reader->stream = NULL;
    }

    LeaveCriticalSection(&reader->cs);
    return hr;
}

static HRESULT stream_set_decoder_output_type(struct wm_stream *stream, const WM_MEDIA_TYPE *mt)
{
    HRESULT hr;

    if ((IsEqualGUID( &stream->mt.formattype, &FORMAT_VideoInfo ) || IsEqualGUID( &stream->mt.formattype, &FORMAT_VideoInfo2 ))
            && (IsEqualGUID( &mt->formattype, &FORMAT_VideoInfo ) || IsEqualGUID( &mt->formattype, &FORMAT_VideoInfo2 )))
    {
        VIDEOINFOHEADER *stream_vih = (VIDEOINFOHEADER *)stream->mt.pbFormat, *vih = (VIDEOINFOHEADER *)mt->pbFormat;
        if (stream_vih->bmiHeader.biWidth != vih->bmiHeader.biWidth)
            return NS_E_INVALID_OUTPUT_FORMAT;
        if (abs(stream_vih->bmiHeader.biHeight) != abs(vih->bmiHeader.biHeight))
            return NS_E_INVALID_OUTPUT_FORMAT;
    }
    else if (IsEqualGUID( &stream->mt.formattype, &FORMAT_WaveFormatEx ) && IsEqualGUID( &mt->formattype, &FORMAT_WaveFormatEx ))
    {
        WAVEFORMATEX *stream_wfx = (WAVEFORMATEX *)stream->mt.pbFormat, *wfx = (WAVEFORMATEX *)mt->pbFormat;
        if (stream_wfx->nChannels != wfx->nChannels)
            return NS_E_AUDIO_CODEC_NOT_INSTALLED;
    }
    else
    {
        FIXME( "Unsupported format types %s/%s\n", debugstr_guid( &stream->mt.formattype ), debugstr_guid( &mt->formattype ) );
        return NS_E_INCOMPATIBLE_FORMAT;
    }

    if (FAILED(hr = IMediaObject_SetOutputType(stream->decoder, 0, (AM_MEDIA_TYPE *)mt, 0)))
    {
        WARN("Failed to set DMO output media type, hr %#lx\n", hr);
        return NS_E_INVALID_OUTPUT_FORMAT;
    }
    return S_OK;
}

static HRESULT WINAPI reader_SetOutputProps(IWMSyncReader2 *iface, DWORD output, IWMOutputMediaProps *props)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;
    HRESULT hr = S_OK;
    DWORD size;
    void *mt;

    TRACE("reader %p, output %lu, props %p.\n", reader, output, props);

    if (FAILED(hr = IWMOutputMediaProps_GetMediaType(props, NULL, &size)))
        return hr;
    if (!(mt = malloc(size)))
        return E_OUTOFMEMORY;
    if (FAILED(hr = IWMOutputMediaProps_GetMediaType(props, mt, &size)))
        goto done;
    strmbase_dump_media_type(mt);

    EnterCriticalSection(&reader->cs);
    if (!(stream = get_stream_by_output_number(reader, output)) || !stream->decoder)
        hr = E_INVALIDARG;
    else if (!stream->decoder)
        hr = E_INVALIDARG;
    else if (FAILED(hr = IMediaObject_SetOutputType(stream->decoder, 0, mt, 0)))
    {
        ERR("Failed to set the decoder output type, hr %#lx\n", hr);
        hr = NS_E_INVALID_OUTPUT_FORMAT;
    }
    LeaveCriticalSection(&reader->cs);

done:
    free(mt);
    return hr;
}

static HRESULT WINAPI reader_SetOutputSetting(IWMSyncReader2 *iface, DWORD output,
        const WCHAR *name, WMT_ATTR_DATATYPE type, const BYTE *value, WORD size)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);

    TRACE("reader %p, output %lu, name %s, type %#x, value %p, size %u.\n",
            reader, output, debugstr_w(name), type, value, size);

    if (!wcscmp(name, L"VideoSampleDurations"))
    {
        FIXME("Ignoring VideoSampleDurations setting.\n");
        return S_OK;
    }
    if (!wcscmp(name, L"EnableDiscreteOutput"))
    {
        FIXME("Ignoring EnableDiscreteOutput setting.\n");
        return S_OK;
    }
    if (!wcscmp(name, L"SpeakerConfig"))
    {
        FIXME("Ignoring SpeakerConfig setting.\n");
        return S_OK;
    }
    else
    {
        FIXME("Unknown setting %s; returning E_NOTIMPL.\n", debugstr_w(name));
        return E_NOTIMPL;
    }
}

static HRESULT WINAPI reader_SetRange(IWMSyncReader2 *iface, QWORD start, LONGLONG duration)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    WORD i;

    TRACE("reader %p, start %I64u, duration %I64d.\n", reader, start, duration);

    EnterCriticalSection(&reader->cs);

    reader->start_time = start;
    winedmo_demuxer_seek(reader->winedmo_demuxer, start);

    for (i = 0; i < reader->stream_count; ++i)
    {
        struct sample *sample;

        while (SUCCEEDED(stream_pop_compressed_sample(&reader->streams[i], &sample)))
            INSSBuffer_Release(&sample->INSSBuffer_iface);

        IMediaObject_Flush(reader->streams[i].decoder);
        reader->streams[i].state = STREAM_STARTING;
    }

    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static HRESULT WINAPI reader_SetRangeByFrame(IWMSyncReader2 *iface, WORD stream_num, QWORD frame_num,
        LONGLONG frames)
{
    struct wm_reader *This = impl_from_IWMSyncReader2(iface);
    FIXME("(%p)->(%d %s %s): stub!\n", This, stream_num, wine_dbgstr_longlong(frame_num), wine_dbgstr_longlong(frames));
    return E_NOTIMPL;
}

static HRESULT WINAPI reader_SetReadStreamSamples(IWMSyncReader2 *iface, WORD stream_number, BOOL compressed)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;

    TRACE("reader %p, stream_number %u, compressed %d.\n", reader, stream_number, compressed);

    EnterCriticalSection(&reader->cs);

    if (!(stream = wm_reader_get_stream_by_stream_number(reader, stream_number)))
    {
        LeaveCriticalSection(&reader->cs);
        return E_INVALIDARG;
    }

    if (compressed || stream->decoder)
        stream->read_compressed = compressed;

    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static HRESULT WINAPI reader_SetStreamsSelected(IWMSyncReader2 *iface,
        WORD count, WORD *stream_numbers, WMT_STREAM_SELECTION *selections)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;
    WORD i;

    TRACE("reader %p, count %u, stream_numbers %p, selections %p.\n",
            reader, count, stream_numbers, selections);

    if (!count)
        return E_INVALIDARG;

    EnterCriticalSection(&reader->cs);

    for (i = 0; i < count; ++i)
    {
        if (!(stream = wm_reader_get_stream_by_stream_number(reader, stream_numbers[i])))
        {
            LeaveCriticalSection(&reader->cs);
            WARN("Invalid stream number %u; returning NS_E_INVALID_REQUEST.\n", stream_numbers[i]);
            return NS_E_INVALID_REQUEST;
        }
    }

    for (i = 0; i < count; ++i)
    {
        stream = wm_reader_get_stream_by_stream_number(reader, stream_numbers[i]);
        stream->selection = selections[i];
    }

    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static HRESULT WINAPI reader_SetRangeByTimecode(IWMSyncReader2 *iface, WORD stream_num,
        WMT_TIMECODE_EXTENSION_DATA *start, WMT_TIMECODE_EXTENSION_DATA *end)
{
    struct wm_reader *This = impl_from_IWMSyncReader2(iface);
    FIXME("(%p)->(%u %p %p): stub!\n", This, stream_num, start, end);
    return E_NOTIMPL;
}

static HRESULT WINAPI reader_SetRangeByFrameEx(IWMSyncReader2 *iface, WORD stream_num, QWORD frame_num,
        LONGLONG frames_to_read, QWORD *starttime)
{
    struct wm_reader *This = impl_from_IWMSyncReader2(iface);
    FIXME("(%p)->(%u %s %s %p): stub!\n", This, stream_num, wine_dbgstr_longlong(frame_num),
          wine_dbgstr_longlong(frames_to_read), starttime);
    return E_NOTIMPL;
}

static HRESULT WINAPI reader_SetAllocateForOutput(IWMSyncReader2 *iface, DWORD output, IWMReaderAllocatorEx *allocator)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;

    TRACE("reader %p, output %lu, allocator %p.\n", reader, output, allocator);

    EnterCriticalSection(&reader->cs);

    if (!(stream = get_stream_by_output_number(reader, output)))
    {
        LeaveCriticalSection(&reader->cs);
        return E_INVALIDARG;
    }

    if (stream->output_allocator)
        IWMReaderAllocatorEx_Release(stream->output_allocator);
    if ((stream->output_allocator = allocator))
        IWMReaderAllocatorEx_AddRef(stream->output_allocator);

    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static HRESULT WINAPI reader_GetAllocateForOutput(IWMSyncReader2 *iface, DWORD output, IWMReaderAllocatorEx **allocator)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;

    TRACE("reader %p, output %lu, allocator %p.\n", reader, output, allocator);

    if (!allocator)
        return E_INVALIDARG;

    EnterCriticalSection(&reader->cs);

    if (!(stream = get_stream_by_output_number(reader, output)))
    {
        LeaveCriticalSection(&reader->cs);
        return E_INVALIDARG;
    }

    stream = reader->streams + output;
    if ((*allocator = stream->output_allocator))
        IWMReaderAllocatorEx_AddRef(*allocator);

    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static HRESULT WINAPI reader_SetAllocateForStream(IWMSyncReader2 *iface, DWORD stream_number, IWMReaderAllocatorEx *allocator)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;

    TRACE("reader %p, stream_number %lu, allocator %p.\n", reader, stream_number, allocator);

    EnterCriticalSection(&reader->cs);

    if (!(stream = wm_reader_get_stream_by_stream_number(reader, stream_number)))
    {
        LeaveCriticalSection(&reader->cs);
        return E_INVALIDARG;
    }

    if (stream->stream_allocator)
        IWMReaderAllocatorEx_Release(stream->stream_allocator);
    if ((stream->stream_allocator = allocator))
        IWMReaderAllocatorEx_AddRef(stream->stream_allocator);

    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static HRESULT WINAPI reader_GetAllocateForStream(IWMSyncReader2 *iface, DWORD stream_number, IWMReaderAllocatorEx **allocator)
{
    struct wm_reader *reader = impl_from_IWMSyncReader2(iface);
    struct wm_stream *stream;

    TRACE("reader %p, stream_number %lu, allocator %p.\n", reader, stream_number, allocator);

    if (!allocator)
        return E_INVALIDARG;

    EnterCriticalSection(&reader->cs);

    if (!(stream = wm_reader_get_stream_by_stream_number(reader, stream_number)))
    {
        LeaveCriticalSection(&reader->cs);
        return E_INVALIDARG;
    }

    if ((*allocator = stream->stream_allocator))
        IWMReaderAllocatorEx_AddRef(*allocator);

    LeaveCriticalSection(&reader->cs);
    return S_OK;
}

static const IWMSyncReader2Vtbl reader_vtbl =
{
    reader_QueryInterface,
    reader_AddRef,
    reader_Release,
    reader_Open,
    reader_Close,
    reader_SetRange,
    reader_SetRangeByFrame,
    reader_GetNextSample,
    reader_SetStreamsSelected,
    reader_GetStreamSelected,
    reader_SetReadStreamSamples,
    reader_GetReadStreamSamples,
    reader_GetOutputSetting,
    reader_SetOutputSetting,
    reader_GetOutputCount,
    reader_GetOutputProps,
    reader_SetOutputProps,
    reader_GetOutputFormatCount,
    reader_GetOutputFormat,
    reader_GetOutputNumberForStream,
    reader_GetStreamNumberForOutput,
    reader_GetMaxOutputSampleSize,
    reader_GetMaxStreamSampleSize,
    reader_OpenStream,
    reader_SetRangeByTimecode,
    reader_SetRangeByFrameEx,
    reader_SetAllocateForOutput,
    reader_GetAllocateForOutput,
    reader_SetAllocateForStream,
    reader_GetAllocateForStream
};

extern HRESULT WINAPI winegstreamer_create_wm_sync_reader(IUnknown *outer, void **out);

HRESULT sync_reader_create(IUnknown *outer, void **out)
{
    struct wm_reader *object;
    NTSTATUS status;

    if ((status = winedmo_demuxer_check("video/x-ms-asf")))
    {
        WARN("Unsupported demuxer, status %#lx.\n", status);
        return winegstreamer_create_wm_sync_reader(outer, out);
    }

    TRACE("out %p.\n", out);

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IUnknown_inner.lpVtbl = &unknown_inner_vtbl;
    object->IWMSyncReader2_iface.lpVtbl = &reader_vtbl;
    object->IWMHeaderInfo3_iface.lpVtbl = &header_info_vtbl;
    object->IWMLanguageList_iface.lpVtbl = &language_list_vtbl;
    object->IWMPacketSize2_iface.lpVtbl = &packet_size_vtbl;
    object->IWMProfile3_iface.lpVtbl = &profile_vtbl;
    object->IWMReaderPlaylistBurn_iface.lpVtbl = &playlist_vtbl;
    object->IWMReaderTimecode_iface.lpVtbl = &timecode_vtbl;
    object->outer = outer ? outer : &object->IUnknown_inner;
    object->refcount = 1;

    InitializeCriticalSectionEx(&object->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO);
    object->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": reader.cs");
    InitializeCriticalSectionEx(&object->shutdown_cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO);
    object->shutdown_cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": reader.shutdown_cs");

    TRACE("Created reader %p.\n", object);
    *out = outer ? (void *)&object->IUnknown_inner : (void *)&object->IWMSyncReader2_iface;
    return S_OK;
}

HRESULT WINAPI WMCreateSyncReader(IUnknown *reserved, DWORD rights, IWMSyncReader **reader)
{
    TRACE("reserved %p, rights %#lx, reader %p.\n", reserved, rights, reader);

    return sync_reader_create(NULL, (void **)reader);
}

HRESULT WINAPI WMCreateSyncReaderPriv(IWMSyncReader **reader)
{
    TRACE("reader %p.\n", reader);

    return sync_reader_create(NULL, (void **)reader);
}
