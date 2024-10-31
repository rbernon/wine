/*
 * Copyright 2017 Alistair Leslie-Hughes
 * Copyright 2019 Vijay Kiran Kamuju
 * Copyright 2021 Zebediah Figura for CodeWeavers
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

#include <stdbool.h>
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include "wmsdk.h"
#include "wmsecure.h"
#include "amvideo.h"
#include "uuids.h"
#include "wmcodecdsp.h"

#include "wine/test.h"

static const char *debugstr_ok( const char *cond )
{
    int c, n = 0;
    /* skip possible casts */
    while ((c = *cond++))
    {
        if (c == '(') n++;
        if (!n) break;
        if (c == ')') n--;
    }
    if (!strchr( cond - 1, '(' )) return wine_dbg_sprintf( "got %s", cond - 1 );
    return wine_dbg_sprintf( "%.*s returned", (int)strcspn( cond - 1, "( " ), cond - 1 );
}

#define ok_eq( e, r, t, f, ... )                                                                   \
    do                                                                                             \
    {                                                                                              \
        t v = (r);                                                                                 \
        ok( v == (e), "%s " f "\n", debugstr_ok( #r ), v, ##__VA_ARGS__ );                         \
    } while (0)
#define ok_ne( e, r, t, f, ... )                                                                   \
    do                                                                                             \
    {                                                                                              \
        t v = (r);                                                                                 \
        ok( v != (e), "%s " f "\n", debugstr_ok( #r ), v, ##__VA_ARGS__ );                         \
    } while (0)
#define ok_wcs( e, r )                                                                             \
    do                                                                                             \
    {                                                                                              \
        const WCHAR *v = (r);                                                                      \
        ok( !wcscmp( v, (e) ), "%s %s\n", debugstr_ok(#r), debugstr_w(v) );                        \
    } while (0)
#define ok_str( e, r )                                                                             \
    do                                                                                             \
    {                                                                                              \
        const char *v = (r);                                                                       \
        ok( !strcmp( v, (e) ), "%s %s\n", debugstr_ok(#r), debugstr_a(v) );                        \
    } while (0)
#define ok_guid( e, r )                                                                            \
    do                                                                                             \
    {                                                                                              \
        const GUID *v = (r);                                                                       \
        ok( !IsEqualGUID( v, (e) ), "%s %s\n", debugstr_ok(#r), debugstr_guid(v) );                \
    } while (0)
#define ok_ret( e, r ) ok_eq( e, r, UINT_PTR, "%Iu, error %ld", GetLastError() )
#define ok_hr( e, r ) ok_eq( e, r, HRESULT, "%#lx" )

START_TEST(games)
{
    char buffer[1024];
    WM_MEDIA_TYPE *wmt = (WM_MEDIA_TYPE *)buffer;
    WMT_STREAM_SELECTION selections[2] = {WMT_ON, WMT_ON};
    IWMOutputMediaProps *props, *format;
    WORD streams[2] = {1, 2};
    IWMStreamConfig *config;
    WMT_ATTR_DATATYPE type;
    IWMHeaderInfo *header;
    IWMSyncReader *reader;
    IWMProfile *profile;
    DWORD count, output;
    WORD num, size;
    GUID major;

    ok_hr( S_OK, CoInitialize(0) );

    ok_hr( S_OK, WMCreateSyncReader( NULL, 0, &reader ) );
    if (winetest_platform_is_wine) ok_hr( S_OK, IWMSyncReader_Open( reader, L"Z:/home/rbernon/Code/build-wine/ruse001.asf" ) );
    else ok_hr( S_OK, IWMSyncReader_Open( reader, L"Z:/build-wine/ruse001.asf" ) );

    ok_hr( S_OK, IWMSyncReader_QueryInterface( reader, &IID_IWMProfile, (void **)&profile ) );
    ok_hr( S_OK, IWMProfile_GetStreamCount( profile, &count ) );
    ok_eq( 2, count, UINT, "%u" );

    ok_hr( S_OK, IWMProfile_GetStream( profile, 0, &config ) );
    ok_hr( S_OK, IWMStreamConfig_GetStreamNumber( config, &num ) );
    ok_eq( 1, num, UINT, "%u" );
    ok_hr( S_OK, IWMStreamConfig_GetStreamType( config, &major ) );
    ok_guid( &MEDIATYPE_Video, &major );
    IWMStreamConfig_Release( config );

    ok_hr( S_OK, IWMProfile_GetStream( profile, 1, &config ) );
    ok_hr( S_OK, IWMStreamConfig_GetStreamNumber( config, &num ) );
    ok_eq( 2, num, UINT, "%u" );
    ok_hr( S_OK, IWMStreamConfig_GetStreamType( config, &major ) );
    ok_guid( &MEDIATYPE_Audio, &major );
    IWMStreamConfig_Release( config );

    IWMProfile_Release( profile );

    ok_hr( S_OK, IWMSyncReader_SetStreamsSelected( reader, 2, streams, selections ) );
    ok_hr( S_OK, IWMSyncReader_GetOutputNumberForStream( reader, 2, &output ) );
    ok_eq( 1, output, UINT, "%u" );
    ok_hr( S_OK, IWMSyncReader_GetOutputNumberForStream( reader, 1, &output ) );
    ok_eq( 0, output, UINT, "%u" );
    ok_hr( S_OK, IWMSyncReader_GetOutputFormatCount( reader, 1, &output ) );
    ok_eq( 13, output, UINT, "%u" );

    ok_hr( S_OK, IWMSyncReader_GetOutputFormat( reader, 1, 0, &format ) );
ok( 0, "format %p\n", format );
    ok_hr( S_OK, IWMOutputMediaProps_GetMediaType( format, 0, &count ) );
    ok_eq( 176, count, UINT, "%u" );
memset(buffer, 0, sizeof(buffer));
    ok_hr( S_OK, IWMOutputMediaProps_GetMediaType( format, wmt, &count ) );
    ok_eq( 176, count, UINT, "%u" );
do
{
    const unsigned char *ptr = (void *)wmt, *end = ptr + count;
    ok(0, "dump %p-%p (%x)\n", (void *)ptr, (void *)end, (int)(end - ptr));
    for (int i = 0, j; ptr + i < end;)
    {
        char buffer[256], *buf = buffer;
        buf += sprintf(buf, "%08x ", i);
        for (j = 0; j < 8 && ptr + i + j < end; ++j)
            buf += sprintf(buf, " %02x", ptr[i + j]);
        for (; j < 8 && ptr + i + j >= end; ++j)
            buf += sprintf(buf, "   ");
        buf += sprintf(buf, " ");
        for (j = 8; j < 16 && ptr + i + j < end; ++j)
            buf += sprintf(buf, " %02x", ptr[i + j]);
        for (; j < 16 && ptr + i + j >= end; ++j)
            buf += sprintf(buf, "   ");
        buf += sprintf(buf, "  |");
        for (j = 0; j < 16 && ptr + i < end; ++j, ++i)
            buf += sprintf(buf, "%c", ptr[i] >= ' ' && ptr[i] <= '~' ? ptr[i] : '.');
        buf += sprintf(buf, "|");
        ok(0, "%s\n", buffer);
    }
}
while(0);
    IWMOutputMediaProps_Release( format );

    ok_hr( S_OK, IWMSyncReader_GetOutputFormat( reader, 1, 1, &format ) );
ok( 0, "format %p\n", format );
    ok_hr( S_OK, IWMOutputMediaProps_GetMediaType( format, 0, &count ) );
    ok_eq( 176, count, UINT, "%u" );
memset(buffer, 0, sizeof(buffer));
    ok_hr( S_OK, IWMOutputMediaProps_GetMediaType( format, wmt, &count ) );
    ok_eq( 176, count, UINT, "%u" );
do
{
    const unsigned char *ptr = (void *)wmt, *end = ptr + count;
    ok(0, "dump %p-%p (%x)\n", (void *)ptr, (void *)end, (int)(end - ptr));
    for (int i = 0, j; ptr + i < end;)
    {
        char buffer[256], *buf = buffer;
        buf += sprintf(buf, "%08x ", i);
        for (j = 0; j < 8 && ptr + i + j < end; ++j)
            buf += sprintf(buf, " %02x", ptr[i + j]);
        for (; j < 8 && ptr + i + j >= end; ++j)
            buf += sprintf(buf, "   ");
        buf += sprintf(buf, " ");
        for (j = 8; j < 16 && ptr + i + j < end; ++j)
            buf += sprintf(buf, " %02x", ptr[i + j]);
        for (; j < 16 && ptr + i + j >= end; ++j)
            buf += sprintf(buf, "   ");
        buf += sprintf(buf, "  |");
        for (j = 0; j < 16 && ptr + i < end; ++j, ++i)
            buf += sprintf(buf, "%c", ptr[i] >= ' ' && ptr[i] <= '~' ? ptr[i] : '.');
        buf += sprintf(buf, "|");
        ok(0, "%s\n", buffer);
    }
}
while(0);

    ok_hr( S_OK, IWMSyncReader_SetOutputProps( reader, 1, format ) );
    count = 0;
    ok_hr( S_OK, IWMSyncReader_SetOutputSetting( reader, 1, L"VideoSampleDurations", 3, (BYTE *)&count, sizeof(count) ) );

    ok_hr( S_OK, IWMSyncReader_QueryInterface( reader, &IID_IWMHeaderInfo, (void **)&header ) );

    num = 0;
    size = sizeof(buffer);
    ok_hr( ASF_E_NOTFOUND, IWMHeaderInfo_GetAttributeByName( header, &num, L"WM/VideoFrameRate", &type, (BYTE *)buffer, &size ) );
    num = 0;
    size = sizeof(buffer);
    ok_hr( S_OK, IWMHeaderInfo_GetAttributeByName( header, &num, L"Duration", &type, (BYTE *)buffer, &size ) );
    ok_eq( 0, num, UINT, "%u" );
    ok_eq( 4, type, UINT, "%u" );
    ok_eq( 8, size, UINT, "%u" );

    IWMHeaderInfo_Release( header );

    IWMOutputMediaProps_Release( format );


    ok_hr( S_OK, IWMSyncReader_GetOutputProps( reader, 0, &props ) );
ok( 0, "props %p\n", props );
    ok_hr( S_OK, IWMSyncReader_GetOutputFormatCount( reader, 0, &output ) );
    ok_eq( 37, output, UINT, "%u" );

    ok_hr( S_OK, IWMSyncReader_GetOutputFormat( reader, 0, 0, &format ) );
ok( 0, "format %p\n", format );
    ok_hr( S_OK, IWMOutputMediaProps_GetMediaType( format, 0, &count ) );
    ok_eq( 106, count, UINT, "%u" );
memset(buffer, 0, sizeof(buffer));
    ok_hr( S_OK, IWMOutputMediaProps_GetMediaType( format, wmt, &count ) );
    ok_eq( 106, count, UINT, "%u" );
do
{
    const unsigned char *ptr = (void *)wmt, *end = ptr + count;
    ok(0, "dump %p-%p (%x)\n", (void *)ptr, (void *)end, (int)(end - ptr));
    for (int i = 0, j; ptr + i < end;)
    {
        char buffer[256], *buf = buffer;
        buf += sprintf(buf, "%08x ", i);
        for (j = 0; j < 8 && ptr + i + j < end; ++j)
            buf += sprintf(buf, " %02x", ptr[i + j]);
        for (; j < 8 && ptr + i + j >= end; ++j)
            buf += sprintf(buf, "   ");
        buf += sprintf(buf, " ");
        for (j = 8; j < 16 && ptr + i + j < end; ++j)
            buf += sprintf(buf, " %02x", ptr[i + j]);
        for (; j < 16 && ptr + i + j >= end; ++j)
            buf += sprintf(buf, "   ");
        buf += sprintf(buf, "  |");
        for (j = 0; j < 16 && ptr + i < end; ++j, ++i)
            buf += sprintf(buf, "%c", ptr[i] >= ' ' && ptr[i] <= '~' ? ptr[i] : '.');
        buf += sprintf(buf, "|");
        ok(0, "%s\n", buffer);
    }
}
while(0);

    ok_hr( NS_E_INVALID_OUTPUT_FORMAT, IWMSyncReader_SetOutputProps( reader, 1, format ) );
    IWMOutputMediaProps_Release( format );

    IWMOutputMediaProps_Release( props );

/*

0104:   0:trace:wmvcore:000000a45875:sync_reader.c:2254:reader_GetOutputFormat iface 02A0DF4C, output 0, index 0, props 0012EFBC.
0104:   0:trace:wmvcore:000000a45875:sync_reader.c:2281:reader_GetOutputFormat iface 02A0DF4C, props 029FF710, ret 0.
0104:   0:trace:wmvcore:000000a45890:sync_reader.c:453:output_props_GetMediaType iface 029FF710, mt 00000000, size 0012EFD8.
0104:   0:trace:wmvcore:000000a45890:sync_reader.c:454:output_props_GetMediaType iface 029FF710, mt 00000000, size 90, ret 0.
0104:   0:trace:wmvcore:000000a458cc:sync_reader.c:453:output_props_GetMediaType iface 029FF710, mt 05A590F0, size 0012EFD8.
0104:   0:trace:wmvcore:000000a458cc:sync_reader.c:454:output_props_GetMediaType iface 029FF710, mt 05A590F0, size 90, ret 0.

0104:   0:trace:wmvcore:000000a45935:sync_reader.c:2595:reader_SetOutputProps iface 02A0DF4C, output 1, props 029FF710.
0104:   0:trace:wmvcore:000000a45935:sync_reader.c:2556:stream_set_decoder_output_type stream 02A0E00C, mt 02A0DDD8.
0104:   0:fixme:wmvcore:000000a45935:sync_reader.c:2575:stream_set_decoder_output_type Unsupported format types {05589f80-c356-11ce-bf01-00aa0055595a}/{05589f81-c356-11ce-bf01-00aa0055595a}
0104:   0:trace:wmvcore:000000a45935:sync_reader.c:2576:stream_set_decoder_output_type stream 02A0E00C, ret 0xc00d0041.
0104:   0:warn:wmvcore:000000a45935:sync_reader.c:2612:reader_SetOutputProps Unsupported media type, returning 0xc00d0041.
0104:   0:trace:wmvcore:000000a45935:sync_reader.c:2617:reader_SetOutputProps iface 02A0DF4C, props 029FF710, ret 0xc00d0041.
0104:   0:trace:wmvcore:000000a4597c:sync_reader.c:429:output_props_Release iface 029FF710.
0104:   0:trace:wmvcore:000000a4597c:sync_reader.c:437:output_props_Release iface 029FF710, ret 0.

0104:   0:trace:wmvcore:000000a45988:sync_reader.c:429:output_props_Release iface 029FF6F0.
0104:   0:trace:wmvcore:000000a45988:sync_reader.c:437:output_props_Release iface 029FF6F0, ret 0.

0104:   0:trace:wmvcore:000000a450a3:sync_reader.c:2155:reader_GetNextSample iface 02A0DF4C, stream_number 2, buffer 0012F248, pts 0012F25C, duration 0012F264, flags 0012F24C, output_number 00000000, ret_stream_number 00000000.
0104:   0:trace:wmvcore:000000a450a3:sync_reader.c:2232:reader_GetNextSample iface 02A0DF4C, buffer 00000000, pts 0, duration 0, flags 0, output_number 0, ret_stream_number 0, ret 0xc00d6d72.
0104:   0:trace:wmvcore:000000a43b3e:sync_reader.c:2035:reader_Release iface 02A0DF4C.
0104:   0:trace:wmvcore:000000a43b3e:sync_reader.c:2037:reader_Release iface 02A0DF4C, ret 0.
*/

    IWMSyncReader_Release( reader );

    CoUninitialize();
}
