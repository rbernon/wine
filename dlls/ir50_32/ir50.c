/*
 * Intel Indeo 5 Video Decoder
 * Copyright 2023 Shaun Ren for CodeWeavers
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
 *
 */

#include <stdarg.h>
#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "commdlg.h"
#include "vfw.h"
#include "ir50_private.h"
#include "initguid.h"

#include "wine/debug.h"
#include "wine/winedmo.h"

WINE_DEFAULT_DEBUG_CHANNEL(ir50_32);

static HINSTANCE IR50_32_hModule;

#define IV50_MAGIC mmioFOURCC('I','V','5','0')
#define compare_fourcc(fcc1, fcc2) (((fcc1)^(fcc2))&~0x20202020)

DEFINE_MEDIATYPE_GUID(MFVideoFormat_IV50, MAKEFOURCC('I','V','5','0'));

static inline UINT64
make_uint64( UINT32 high, UINT32 low )
{
    return ((UINT64)high << 32) | low;
}

struct decoder
{
    struct winedmo_transform winedmo_transform;
};

static LRESULT
IV50_Open( const ICINFO *icinfo )
{
    struct decoder *decoder = 0;

    TRACE("DRV_OPEN %p\n", icinfo);

    if ( icinfo && compare_fourcc( icinfo->fccType, ICTYPE_VIDEO ) )
        return 0;

    decoder = calloc(1, sizeof(*decoder));
    return (LRESULT)decoder;
}

static LRESULT
IV50_DecompressQuery( LPBITMAPINFO in, LPBITMAPINFO out )
{
    TRACE("ICM_DECOMPRESS_QUERY %p %p\n", in, out);

    TRACE("in->planes  = %d\n", in->bmiHeader.biPlanes);
    TRACE("in->bpp     = %d\n", in->bmiHeader.biBitCount);
    TRACE("in->height  = %ld\n", in->bmiHeader.biHeight);
    TRACE("in->width   = %ld\n", in->bmiHeader.biWidth);
    TRACE("in->compr   = %#lx\n", in->bmiHeader.biCompression);

    if ( in->bmiHeader.biCompression != IV50_MAGIC )
    {
        TRACE("can't do %#lx compression\n", in->bmiHeader.biCompression);
        return ICERR_BADFORMAT;
    }

    /* output must be same dimensions as input */
    if ( out )
    {
        TRACE("out->planes = %d\n", out->bmiHeader.biPlanes);
        TRACE("out->bpp    = %d\n", out->bmiHeader.biBitCount);
        TRACE("out->height = %ld\n", out->bmiHeader.biHeight);
        TRACE("out->width  = %ld\n", out->bmiHeader.biWidth);
        TRACE("out->compr  = %#lx\n", out->bmiHeader.biCompression);

        if ( out->bmiHeader.biCompression != BI_RGB )
        {
            TRACE("incompatible compression requested\n");
            return ICERR_BADFORMAT;
        }

        if ( out->bmiHeader.biBitCount != 32 && out->bmiHeader.biBitCount != 24 && out->bmiHeader.biBitCount != 16 )
        {
            TRACE("incompatible depth requested\n");
            return ICERR_BADFORMAT;
        }

        if ( in->bmiHeader.biPlanes != out->bmiHeader.biPlanes ||
             in->bmiHeader.biHeight != abs(out->bmiHeader.biHeight) ||
             in->bmiHeader.biWidth  != out->bmiHeader.biWidth )
        {
            TRACE("incompatible output dimensions requested\n");
            return ICERR_BADFORMAT;
        }
    }

    return ICERR_OK;
}

static LRESULT
IV50_DecompressGetFormat( LPBITMAPINFO in, LPBITMAPINFO out )
{
    DWORD size;

    TRACE("ICM_DECOMPRESS_GETFORMAT %p %p\n", in, out);

    if ( !in )
        return ICERR_BADPARAM;

    if ( in->bmiHeader.biCompression != IV50_MAGIC )
        return ICERR_BADFORMAT;

    size = in->bmiHeader.biSize;
    if ( out )
    {
        memcpy( out, in, size );
        out->bmiHeader.biHeight = abs(in->bmiHeader.biHeight);
        out->bmiHeader.biCompression = BI_RGB;
        out->bmiHeader.biBitCount = 32;
        out->bmiHeader.biSizeImage = out->bmiHeader.biWidth * out->bmiHeader.biHeight * 4;
        return ICERR_OK;
    }

    return size;
}

static LRESULT IV50_DecompressBegin( struct decoder *decoder, LPBITMAPINFO in, LPBITMAPINFO out )
{
    union winedmo_format input_format = {0}, output_format = {0};
    const GUID *output_subtype;
    LRESULT r = ICERR_INTERNAL;

    TRACE("ICM_DECOMPRESS_BEGIN %p %p %p\n", decoder, in, out);

    if ( !decoder )
        return ICERR_BADPARAM;

    if ( out->bmiHeader.biBitCount == 32 )
        output_subtype = &MFVideoFormat_RGB32;
    else if ( out->bmiHeader.biBitCount == 24 )
        output_subtype = &MFVideoFormat_RGB24;
    else if ( out->bmiHeader.biBitCount == 16 )
        output_subtype = &MFVideoFormat_RGB555;
    else
        return ICERR_BADFORMAT;

    input_format.video.dwSize = sizeof(input_format.video);
    input_format.video.guidFormat = MFVideoFormat_IV50;
    input_format.video.videoInfo.dwWidth = in->bmiHeader.biWidth;
    input_format.video.videoInfo.dwHeight = in->bmiHeader.biHeight;

    output_format.video.dwSize = sizeof(output_format.video);
    output_format.video.guidFormat = *output_subtype;
    output_format.video.videoInfo.dwWidth = out->bmiHeader.biWidth;
    output_format.video.videoInfo.dwHeight = out->bmiHeader.biHeight;
    if (out->bmiHeader.biHeight >= 0) output_format.video.videoInfo.VideoFlags |= MFVideoFlag_BottomUpLinearRep;

    winedmo_transform_destroy(&decoder->winedmo_transform);
    if (winedmo_transform_create( MFMediaType_Video, &input_format, &output_format, &decoder->winedmo_transform ))
        goto done;

    r = ICERR_OK;

done:
    return r;
}

struct buffer
{
    IMediaBuffer IMediaBuffer_iface;
    UINT len;
    UINT max_len;
    BYTE *data;
};

static struct buffer *impl_from_IMediaBuffer(IMediaBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct buffer, IMediaBuffer_iface);
}

static HRESULT WINAPI buffer_QueryInterface(IMediaBuffer *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IMediaBuffer))
    {
        IMediaBuffer_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI buffer_AddRef(IMediaBuffer *iface)
{
    TRACE("iface %p.\n", iface);
    return 2;
}

static ULONG WINAPI buffer_Release(IMediaBuffer *iface)
{
    TRACE("iface %p.\n", iface);
    return 1;
}

static HRESULT WINAPI buffer_SetLength(IMediaBuffer *iface, DWORD len)
{
    struct buffer *buffer = impl_from_IMediaBuffer(iface);
    TRACE("iface %p, len %lu.\n", iface, len);
    buffer->len = len;
    return S_OK;
}

static HRESULT WINAPI buffer_GetMaxLength(IMediaBuffer *iface, DWORD *len)
{
    struct buffer *buffer = impl_from_IMediaBuffer(iface);
    TRACE("iface %p, len %p.\n", iface, len);
    *len = buffer->max_len;
    return S_OK;
}

static HRESULT WINAPI buffer_GetBufferAndLength(IMediaBuffer *iface, BYTE **data, DWORD *len)
{
    struct buffer *buffer = impl_from_IMediaBuffer(iface);
    TRACE("iface %p, data %p, len %p.\n", iface, data, len);
    *len = buffer->len;
    *data = buffer->data;
    return S_OK;
}

static const IMediaBufferVtbl buffer_vtbl =
{
    buffer_QueryInterface,
    buffer_AddRef,
    buffer_Release,
    buffer_SetLength,
    buffer_GetMaxLength,
    buffer_GetBufferAndLength,
};

static LRESULT IV50_Decompress( struct decoder *decoder, ICDECOMPRESS *icd, DWORD size )
{
    struct buffer input_buffer = {.IMediaBuffer_iface.lpVtbl = &buffer_vtbl}, output_buffer = {.IMediaBuffer_iface.lpVtbl = &buffer_vtbl};
    DMO_OUTPUT_DATA_BUFFER input = {0}, output = {0};
    LRESULT r = ICERR_INTERNAL;
    NTSTATUS status;

    TRACE("ICM_DECOMPRESS %p %p %lu\n", decoder, icd, size);

    input_buffer.max_len = icd->lpbiInput->biSizeImage;
    input_buffer.len = icd->lpbiInput->biSizeImage;
    input_buffer.data = icd->lpInput;
    input.pBuffer = &input_buffer.IMediaBuffer_iface;

    if ((status = winedmo_transform_process_input( decoder->winedmo_transform, &input )))
        WARN( "Failed to process input, status %#lx\n", status );
    else
    {
        output_buffer.len = 0;
        output_buffer.max_len = icd->lpbiOutput->biSizeImage;
        output_buffer.data = icd->lpOutput;
        output.pBuffer = &output_buffer.IMediaBuffer_iface;
        status = winedmo_transform_process_output( decoder->winedmo_transform, &output );
        if (!status)
            r = ICERR_OK;
        else if (status == STATUS_PENDING)
        {
            TRACE("no output received.\n");
            r = ICERR_OK;
        }
    }

    return r;
}

static LRESULT IV50_GetInfo( ICINFO *icinfo, DWORD dwSize )
{
    TRACE("ICM_GETINFO %p %lu\n", icinfo, dwSize);

    if ( !icinfo ) return sizeof(ICINFO);
    if ( dwSize < sizeof(ICINFO) ) return 0;

    icinfo->dwSize       = sizeof(ICINFO);
    icinfo->fccType      = ICTYPE_VIDEO;
    icinfo->fccHandler   = IV50_MAGIC;
    icinfo->dwFlags      = 0;
    icinfo->dwVersion    = ICVERSION;
    icinfo->dwVersionICM = ICVERSION;

    LoadStringW( IR50_32_hModule, IDS_NAME, icinfo->szName, ARRAY_SIZE(icinfo->szName) );
    LoadStringW( IR50_32_hModule, IDS_DESCRIPTION, icinfo->szDescription, ARRAY_SIZE(icinfo->szDescription) );
    /* msvfw32 will fill icinfo->szDriver for us */

    return sizeof(ICINFO);
}

/***********************************************************************
 *              DriverProc (IR50_32.@)
 */
LRESULT WINAPI IV50_DriverProc( DWORD_PTR dwDriverId, HDRVR hdrvr, UINT msg,
                                LPARAM lParam1, LPARAM lParam2 )
{
    struct decoder *decoder = (struct decoder *) dwDriverId;
    LRESULT r = ICERR_UNSUPPORTED;

    TRACE("%Id %p %04x %08Ix %08Ix\n", dwDriverId, hdrvr, msg, lParam1, lParam2);

    switch( msg )
    {
    case DRV_LOAD:
        TRACE("DRV_LOAD\n");
        r = 1;
        break;

    case DRV_OPEN:
        r = IV50_Open((ICINFO *)lParam2);
        break;

    case DRV_CLOSE:
        TRACE("DRV_CLOSE\n");
        if (decoder) winedmo_transform_destroy( &decoder->winedmo_transform );
        free( decoder );
        r = 1;
        break;

    case DRV_ENABLE:
    case DRV_DISABLE:
    case DRV_FREE:
        break;

    case ICM_GETINFO:
        r = IV50_GetInfo( (ICINFO *) lParam1, (DWORD) lParam2 );
        break;

    case ICM_DECOMPRESS_QUERY:
        r = IV50_DecompressQuery( (LPBITMAPINFO) lParam1, (LPBITMAPINFO) lParam2 );
        break;

    case ICM_DECOMPRESS_GET_FORMAT:
        r = IV50_DecompressGetFormat( (LPBITMAPINFO) lParam1, (LPBITMAPINFO) lParam2 );
        break;

    case ICM_DECOMPRESS_GET_PALETTE:
        FIXME("ICM_DECOMPRESS_GET_PALETTE\n");
        break;

    case ICM_DECOMPRESS:
        r = IV50_Decompress( decoder, (ICDECOMPRESS *) lParam1, (DWORD) lParam2 );
        break;

    case ICM_DECOMPRESS_BEGIN:
        r = IV50_DecompressBegin( decoder, (LPBITMAPINFO) lParam1, (LPBITMAPINFO) lParam2 );
        break;

    case ICM_DECOMPRESS_END:
        r = ICERR_UNSUPPORTED;
        break;

    case ICM_DECOMPRESSEX_QUERY:
        FIXME("ICM_DECOMPRESSEX_QUERY\n");
        break;

    case ICM_DECOMPRESSEX:
        FIXME("ICM_DECOMPRESSEX\n");
        break;

    case ICM_COMPRESS_QUERY:
        r = ICERR_BADFORMAT;
        /* fall through */
    case ICM_COMPRESS_GET_FORMAT:
    case ICM_COMPRESS_END:
    case ICM_COMPRESS:
        FIXME("compression not implemented\n");
        break;

    case ICM_CONFIGURE:
        break;

    default:
        FIXME("Unknown message: %04x %Id %Id\n", msg, lParam1, lParam2);
    }

    return r;
}

/***********************************************************************
 *              DllMain
 */
BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
    TRACE("(%p,%lu,%p)\n", hModule, dwReason, lpReserved);

    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        IR50_32_hModule = hModule;
        break;
    }
    return TRUE;
}
