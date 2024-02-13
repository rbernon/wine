/*
 * Copyright (C) 2023 Rémi Bernon for CodeWeavers
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

#include <stdarg.h>
#include <stddef.h>

#include <windef.h>
#include <winbase.h>

#define COBJMACROS
#include "objidl.h"

#include "dmusici.h"
#include "dmusicf.h"
#include "dmusics.h"
#include "dmksctrl.h"

#include "wine/test.h"

#include "test_tool.h"
#include "test_track.h"
#include "test_port.h"
#include "test_instrument.h"

static const char *debugstr_dmus_err( HRESULT hr )
{
    switch (hr)
    {
    case DMUS_S_PARTIALLOAD: return "DMUS_S_PARTIALLOAD";
    case DMUS_S_PARTIALDOWNLOAD: return "DMUS_S_PARTIALDOWNLOAD";
    case DMUS_S_REQUEUE: return "DMUS_S_REQUEUE";
    case DMUS_S_FREE: return "DMUS_S_FREE";
    case DMUS_S_END: return "DMUS_S_END";
    case DMUS_S_STRING_TRUNCATED: return "DMUS_S_STRING_TRUNCATED";
    case DMUS_S_LAST_TOOL: return "DMUS_S_LAST_TOOL";
    case DMUS_S_OVER_CHORD: return "DMUS_S_OVER_CHORD";
    case DMUS_S_UP_OCTAVE: return "DMUS_S_UP_OCTAVE";
    case DMUS_S_DOWN_OCTAVE: return "DMUS_S_DOWN_OCTAVE";
    case DMUS_S_NOBUFFERCONTROL: return "DMUS_S_NOBUFFERCONTROL";
    case DMUS_S_GARBAGE_COLLECTED: return "DMUS_S_GARBAGE_COLLECTED";
    case DMUS_E_DRIVER_FAILED: return "DMUS_E_DRIVER_FAILED";
    case DMUS_E_PORTS_OPEN: return "DMUS_E_PORTS_OPEN";
    case DMUS_E_DEVICE_IN_USE: return "DMUS_E_DEVICE_IN_USE";
    case DMUS_E_INSUFFICIENTBUFFER: return "DMUS_E_INSUFFICIENTBUFFER";
    case DMUS_E_BUFFERNOTSET: return "DMUS_E_BUFFERNOTSET";
    case DMUS_E_BUFFERNOTAVAILABLE: return "DMUS_E_BUFFERNOTAVAILABLE";
    case DMUS_E_NOTADLSCOL: return "DMUS_E_NOTADLSCOL";
    case DMUS_E_INVALIDOFFSET: return "DMUS_E_INVALIDOFFSET";
    case DMUS_E_ALREADY_LOADED: return "DMUS_E_ALREADY_LOADED";
    case DMUS_E_INVALIDPOS: return "DMUS_E_INVALIDPOS";
    case DMUS_E_INVALIDPATCH: return "DMUS_E_INVALIDPATCH";
    case DMUS_E_CANNOTSEEK: return "DMUS_E_CANNOTSEEK";
    case DMUS_E_CANNOTWRITE: return "DMUS_E_CANNOTWRITE";
    case DMUS_E_CHUNKNOTFOUND: return "DMUS_E_CHUNKNOTFOUND";
    case DMUS_E_INVALID_DOWNLOADID: return "DMUS_E_INVALID_DOWNLOADID";
    case DMUS_E_NOT_DOWNLOADED_TO_PORT: return "DMUS_E_NOT_DOWNLOADED_TO_PORT";
    case DMUS_E_ALREADY_DOWNLOADED: return "DMUS_E_ALREADY_DOWNLOADED";
    case DMUS_E_UNKNOWN_PROPERTY: return "DMUS_E_UNKNOWN_PROPERTY";
    case DMUS_E_SET_UNSUPPORTED: return "DMUS_E_SET_UNSUPPORTED";
    case DMUS_E_GET_UNSUPPORTED: return "DMUS_E_GET_UNSUPPORTED";
    case DMUS_E_NOTMONO: return "DMUS_E_NOTMONO";
    case DMUS_E_BADARTICULATION: return "DMUS_E_BADARTICULATION";
    case DMUS_E_BADINSTRUMENT: return "DMUS_E_BADINSTRUMENT";
    case DMUS_E_BADWAVELINK: return "DMUS_E_BADWAVELINK";
    case DMUS_E_NOARTICULATION: return "DMUS_E_NOARTICULATION";
    case DMUS_E_NOTPCM: return "DMUS_E_NOTPCM";
    case DMUS_E_BADWAVE: return "DMUS_E_BADWAVE";
    case DMUS_E_BADOFFSETTABLE: return "DMUS_E_BADOFFSETTABLE";
    case DMUS_E_UNKNOWNDOWNLOAD: return "DMUS_E_UNKNOWNDOWNLOAD";
    case DMUS_E_NOSYNTHSINK: return "DMUS_E_NOSYNTHSINK";
    case DMUS_E_ALREADYOPEN: return "DMUS_E_ALREADYOPEN";
    case DMUS_E_ALREADYCLOSED: return "DMUS_E_ALREADYCLOSED";
    case DMUS_E_SYNTHNOTCONFIGURED: return "DMUS_E_SYNTHNOTCONFIGURED";
    case DMUS_E_SYNTHACTIVE: return "DMUS_E_SYNTHACTIVE";
    case DMUS_E_CANNOTREAD: return "DMUS_E_CANNOTREAD";
    case DMUS_E_DMUSIC_RELEASED: return "DMUS_E_DMUSIC_RELEASED";
    case DMUS_E_BUFFER_EMPTY: return "DMUS_E_BUFFER_EMPTY";
    case DMUS_E_BUFFER_FULL: return "DMUS_E_BUFFER_FULL";
    case DMUS_E_PORT_NOT_CAPTURE: return "DMUS_E_PORT_NOT_CAPTURE";
    case DMUS_E_PORT_NOT_RENDER: return "DMUS_E_PORT_NOT_RENDER";
    case DMUS_E_DSOUND_NOT_SET: return "DMUS_E_DSOUND_NOT_SET";
    case DMUS_E_ALREADY_ACTIVATED: return "DMUS_E_ALREADY_ACTIVATED";
    case DMUS_E_INVALIDBUFFER: return "DMUS_E_INVALIDBUFFER";
    case DMUS_E_WAVEFORMATNOTSUPPORTED: return "DMUS_E_WAVEFORMATNOTSUPPORTED";
    case DMUS_E_SYNTHINACTIVE: return "DMUS_E_SYNTHINACTIVE";
    case DMUS_E_DSOUND_ALREADY_SET: return "DMUS_E_DSOUND_ALREADY_SET";
    case DMUS_E_INVALID_EVENT: return "DMUS_E_INVALID_EVENT";
    case DMUS_E_UNSUPPORTED_STREAM: return "DMUS_E_UNSUPPORTED_STREAM";
    case DMUS_E_ALREADY_INITED: return "DMUS_E_ALREADY_INITED";
    case DMUS_E_INVALID_BAND: return "DMUS_E_INVALID_BAND";
    case DMUS_E_TRACK_HDR_NOT_FIRST_CK: return "DMUS_E_TRACK_HDR_NOT_FIRST_CK";
    case DMUS_E_TOOL_HDR_NOT_FIRST_CK: return "DMUS_E_TOOL_HDR_NOT_FIRST_CK";
    case DMUS_E_INVALID_TRACK_HDR: return "DMUS_E_INVALID_TRACK_HDR";
    case DMUS_E_INVALID_TOOL_HDR: return "DMUS_E_INVALID_TOOL_HDR";
    case DMUS_E_ALL_TOOLS_FAILED: return "DMUS_E_ALL_TOOLS_FAILED";
    case DMUS_E_ALL_TRACKS_FAILED: return "DMUS_E_ALL_TRACKS_FAILED";
    case DMUS_E_NOT_FOUND: return "DMUS_E_NOT_FOUND";
    case DMUS_E_NOT_INIT: return "DMUS_E_NOT_INIT";
    case DMUS_E_TYPE_DISABLED: return "DMUS_E_TYPE_DISABLED";
    case DMUS_E_TYPE_UNSUPPORTED: return "DMUS_E_TYPE_UNSUPPORTED";
    case DMUS_E_TIME_PAST: return "DMUS_E_TIME_PAST";
    case DMUS_E_TRACK_NOT_FOUND: return "DMUS_E_TRACK_NOT_FOUND";
    case DMUS_E_TRACK_NO_CLOCKTIME_SUPPORT: return "DMUS_E_TRACK_NO_CLOCKTIME_SUPPORT";
    case DMUS_E_NO_MASTER_CLOCK: return "DMUS_E_NO_MASTER_CLOCK";
    case DMUS_E_LOADER_NOCLASSID: return "DMUS_E_LOADER_NOCLASSID";
    case DMUS_E_LOADER_BADPATH: return "DMUS_E_LOADER_BADPATH";
    case DMUS_E_LOADER_FAILEDOPEN: return "DMUS_E_LOADER_FAILEDOPEN";
    case DMUS_E_LOADER_FORMATNOTSUPPORTED: return "DMUS_E_LOADER_FORMATNOTSUPPORTED";
    case DMUS_E_LOADER_FAILEDCREATE: return "DMUS_E_LOADER_FAILEDCREATE";
    case DMUS_E_LOADER_OBJECTNOTFOUND: return "DMUS_E_LOADER_OBJECTNOTFOUND";
    case DMUS_E_LOADER_NOFILENAME: return "DMUS_E_LOADER_NOFILENAME";
    case DMUS_E_INVALIDFILE: return "DMUS_E_INVALIDFILE";
    case DMUS_E_ALREADY_EXISTS: return "DMUS_E_ALREADY_EXISTS";
    case DMUS_E_OUT_OF_RANGE: return "DMUS_E_OUT_OF_RANGE";
    case DMUS_E_SEGMENT_INIT_FAILED: return "DMUS_E_SEGMENT_INIT_FAILED";
    case DMUS_E_ALREADY_SENT: return "DMUS_E_ALREADY_SENT";
    case DMUS_E_CANNOT_FREE: return "DMUS_E_CANNOT_FREE";
    case DMUS_E_CANNOT_OPEN_PORT: return "DMUS_E_CANNOT_OPEN_PORT";
    case DMUS_E_CANNOT_CONVERT: return "DMUS_E_CANNOT_CONVERT";
    case DMUS_E_DESCEND_CHUNK_FAIL: return "DMUS_E_DESCEND_CHUNK_FAIL";
    case DMUS_E_NOT_LOADED: return "DMUS_E_NOT_LOADED";
    case DMUS_E_SCRIPT_LANGUAGE_INCOMPATIBLE: return "DMUS_E_SCRIPT_LANGUAGE_INCOMPATIBLE";
    case DMUS_E_SCRIPT_UNSUPPORTED_VARTYPE: return "DMUS_E_SCRIPT_UNSUPPORTED_VARTYPE";
    case DMUS_E_SCRIPT_ERROR_IN_SCRIPT: return "DMUS_E_SCRIPT_ERROR_IN_SCRIPT";
    case DMUS_E_SCRIPT_CANTLOAD_OLEAUT32: return "DMUS_E_SCRIPT_CANTLOAD_OLEAUT32";
    case DMUS_E_SCRIPT_LOADSCRIPT_ERROR: return "DMUS_E_SCRIPT_LOADSCRIPT_ERROR";
    case DMUS_E_SCRIPT_INVALID_FILE: return "DMUS_E_SCRIPT_INVALID_FILE";
    case DMUS_E_INVALID_SCRIPTTRACK: return "DMUS_E_INVALID_SCRIPTTRACK";
    case DMUS_E_SCRIPT_VARIABLE_NOT_FOUND: return "DMUS_E_SCRIPT_VARIABLE_NOT_FOUND";
    case DMUS_E_SCRIPT_ROUTINE_NOT_FOUND: return "DMUS_E_SCRIPT_ROUTINE_NOT_FOUND";
    case DMUS_E_SCRIPT_CONTENT_READONLY: return "DMUS_E_SCRIPT_CONTENT_READONLY";
    case DMUS_E_SCRIPT_NOT_A_REFERENCE: return "DMUS_E_SCRIPT_NOT_A_REFERENCE";
    case DMUS_E_SCRIPT_VALUE_NOT_SUPPORTED: return "DMUS_E_SCRIPT_VALUE_NOT_SUPPORTED";
    case DMUS_E_INVALID_SEGMENTTRIGGERTRACK: return "DMUS_E_INVALID_SEGMENTTRIGGERTRACK";
    case DMUS_E_INVALID_LYRICSTRACK: return "DMUS_E_INVALID_LYRICSTRACK";
    case DMUS_E_INVALID_PARAMCONTROLTRACK: return "DMUS_E_INVALID_PARAMCONTROLTRACK";
    case DMUS_E_AUDIOVBSCRIPT_SYNTAXERROR: return "DMUS_E_AUDIOVBSCRIPT_SYNTAXERROR";
    case DMUS_E_AUDIOVBSCRIPT_RUNTIMEERROR: return "DMUS_E_AUDIOVBSCRIPT_RUNTIMEERROR";
    case DMUS_E_AUDIOVBSCRIPT_OPERATIONFAILURE: return "DMUS_E_AUDIOVBSCRIPT_OPERATIONFAILURE";
    case DMUS_E_AUDIOPATHS_NOT_VALID: return "DMUS_E_AUDIOPATHS_NOT_VALID";
    case DMUS_E_AUDIOPATHS_IN_USE: return "DMUS_E_AUDIOPATHS_IN_USE";
    case DMUS_E_NO_AUDIOPATH_CONFIG: return "DMUS_E_NO_AUDIOPATH_CONFIG";
    case DMUS_E_AUDIOPATH_INACTIVE: return "DMUS_E_AUDIOPATH_INACTIVE";
    case DMUS_E_AUDIOPATH_NOBUFFER: return "DMUS_E_AUDIOPATH_NOBUFFER";
    case DMUS_E_AUDIOPATH_NOPORT: return "DMUS_E_AUDIOPATH_NOPORT";
    case DMUS_E_NO_AUDIOPATH: return "DMUS_E_NO_AUDIOPATH";
    case DMUS_E_INVALIDCHUNK: return "DMUS_E_INVALIDCHUNK";
    case DMUS_E_AUDIOPATH_NOGLOBALFXBUFFER: return "DMUS_E_AUDIOPATH_NOGLOBALFXBUFFER";
    case DMUS_E_INVALID_CONTAINER_OBJECT: return "DMUS_E_INVALID_CONTAINER_OBJECT";
    }
    return "?";
}

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
#define ok_ret( e, r ) ok_eq( e, r, UINT_PTR, "%Iu, error %ld", GetLastError() )
#define ok_hr( e, r ) ok_eq( e, r, HRESULT, "%#lx (%s)", debugstr_dmus_err(v) )

#define check_member_( file, line, val, exp, fmt, member )                                         \
    ok_(file, line)( (val).member == (exp).member, "got " #member " " fmt "\n", (val).member )
#define check_member( val, exp, fmt, member )                                                      \
    check_member_( __FILE__, __LINE__, val, exp, fmt, member )

#define check_member_wstr_( file, line, val, exp, member )                                         \
    ok_(file, line)( !wcscmp( (val).member, (exp).member ), "got " #member " %s\n",                \
                     debugstr_w((val).member) )
#define check_member_wstr( val, exp, member )                                                      \
    check_member_wstr_( __FILE__, __LINE__, val, exp, member )

#define check_member_guid_( file, line, val, exp, member )                                         \
    ok_(file, line)( IsEqualGUID( &(val).member, &(exp).member ), "got " #member " %s\n",         \
                     debugstr_guid( &(val).member ) )
#define check_member_guid( val, exp, member )                                                      \
    check_member_guid_( __FILE__, __LINE__, val, exp, member )

#define check_dmus_portcaps( a, b ) check_dmus_portcaps_( __LINE__, a, b )
static void check_dmus_portcaps_( int line, DMUS_PORTCAPS *caps, const DMUS_PORTCAPS *expect )
{
    check_member_( __FILE__, line, *caps, *expect, "%#lx", dwSize );
    check_member_( __FILE__, line, *caps, *expect, "%#lx", dwFlags );
    check_member_guid_( __FILE__, line, *caps, *expect, guidPort );
    check_member_( __FILE__, line, *caps, *expect, "%#lx", dwClass );
    check_member_( __FILE__, line, *caps, *expect, "%#lx", dwType );
    check_member_( __FILE__, line, *caps, *expect, "%#lx", dwMemorySize );
    check_member_( __FILE__, line, *caps, *expect, "%#lx", dwMaxChannelGroups );
    check_member_( __FILE__, line, *caps, *expect, "%#lx", dwMaxVoices );
    check_member_( __FILE__, line, *caps, *expect, "%#lx", dwMaxAudioChannels );
    check_member_( __FILE__, line, *caps, *expect, "%#lx", dwEffectFlags );
    check_member_wstr_( __FILE__, line, *caps, *expect, wszDescription );
}

static void stream_begin_chunk( IStream *stream, const char type[5], ULARGE_INTEGER *offset )
{
    STATSTG stat;
    ok_hr(S_OK, IStream_Write( stream, type, 4, NULL ));
    ok_hr(S_OK, IStream_Stat( stream, &stat, 0 ));
    ok_hr(S_OK, IStream_Write( stream, "\0\0\0\0", 4, NULL ));
    *offset = stat.cbSize;
}

static void stream_end_chunk( IStream *stream, ULARGE_INTEGER *offset )
{
    STATSTG stat;
    UINT size;
    ok_hr(S_OK, IStream_Stat( stream, &stat, 0 ));
    ok_hr(S_OK, IStream_Seek( stream, *(LARGE_INTEGER *)offset, 0, NULL ));
    size = stat.cbSize.QuadPart - offset->QuadPart - 4;
    ok_hr(S_OK, IStream_Write( stream, &size, 4, NULL ));
    ok_hr(S_OK, IStream_Seek( stream, *(LARGE_INTEGER *)&stat.cbSize, 0, NULL ));
}

#define CHUNK_BEGIN( stream, type ) \
    do { \
        ULARGE_INTEGER __off; \
        stream_begin_chunk( stream, type, &__off ); \
        do
#define CHUNK_RIFF( stream, form ) \
    do { \
        ULARGE_INTEGER __off; \
        stream_begin_chunk( stream, "RIFF", &__off ); \
        IStream_Write( stream, form, 4, NULL ); \
        do
#define CHUNK_LIST( stream, form ) \
    do { \
        ULARGE_INTEGER __off; \
        stream_begin_chunk( stream, "LIST", &__off ); \
        IStream_Write( stream, form, 4, NULL ); \
        do
#define CHUNK_END \
        while (0); \
        stream_end_chunk( stream, &__off ); \
    } while (0)

#define CHUNK_DATA( stream, type, data ) \
    CHUNK_BEGIN( stream, type ) \
    IStream_Write( (stream), &(data), sizeof(data), NULL ); \
    CHUNK_END \

#define CHUNK_ARRAY( stream, type, items ) \
    CHUNK_BEGIN( stream, type ) \
    { \
        UINT __size = sizeof(*(items)); \
        IStream_Write( (stream), &__size, 4, NULL ); \
        IStream_Write( (stream), &(items), sizeof(items), NULL ); \
    } \
    CHUNK_END \

struct dsound_wrapper
{
    IDirectSound IDirectSound_iface;
    IDirectSound8 *dsound8;
};

static HRESULT WINAPI dsound_wrapper_QueryInterface(IDirectSound *iface, REFIID riid, void** ppvObject)
{
    ok(0, "unexpected call %s iid %s\n", __func__, debugstr_guid(riid));
    return S_OK;
}

static ULONG WINAPI dsound_wrapper_AddRef(IDirectSound *iface)
{
    return 2;
}

static ULONG WINAPI dsound_wrapper_Release(IDirectSound *iface)
{
    return 1;
}

static HRESULT WINAPI dsound_wrapper_CreateSoundBuffer(IDirectSound *iface, const DSBUFFERDESC *desc, IDirectSoundBuffer **out, IUnknown *outer)
{
    struct dsound_wrapper *wrapper = CONTAINING_RECORD(iface, struct dsound_wrapper, IDirectSound_iface);
    return IDirectSound8_CreateSoundBuffer(wrapper->dsound8, desc, out, outer);
}

static HRESULT WINAPI dsound_wrapper_GetCaps(IDirectSound *iface, DSCAPS *caps)
{
    struct dsound_wrapper *wrapper = CONTAINING_RECORD(iface, struct dsound_wrapper, IDirectSound_iface);
    return IDirectSound8_GetCaps(wrapper->dsound8, caps);
}

static HRESULT WINAPI dsound_wrapper_DuplicateSoundBuffer(IDirectSound *iface, IDirectSoundBuffer *buffer, IDirectSoundBuffer **out)
{
    struct dsound_wrapper *wrapper = CONTAINING_RECORD(iface, struct dsound_wrapper, IDirectSound_iface);
    ok(0, "unexpected call %s\n", __func__);
    return IDirectSound8_DuplicateSoundBuffer(wrapper->dsound8, buffer, out);
}

static HRESULT WINAPI dsound_wrapper_SetCooperativeLevel(IDirectSound *iface, HWND hwnd, DWORD level)
{
    struct dsound_wrapper *wrapper = CONTAINING_RECORD(iface, struct dsound_wrapper, IDirectSound_iface);
    ok(0, "unexpected call %s\n", __func__);
    return IDirectSound8_SetCooperativeLevel(wrapper->dsound8, hwnd, level);
}

static HRESULT WINAPI dsound_wrapper_Compact(IDirectSound *iface)
{
    struct dsound_wrapper *wrapper = CONTAINING_RECORD(iface, struct dsound_wrapper, IDirectSound_iface);
    ok(0, "unexpected call %s\n", __func__);
    return IDirectSound8_Compact(wrapper->dsound8);
}

static HRESULT WINAPI dsound_wrapper_GetSpeakerConfig(IDirectSound *iface, DWORD *config)
{
    struct dsound_wrapper *wrapper = CONTAINING_RECORD(iface, struct dsound_wrapper, IDirectSound_iface);
    ok(0, "unexpected call %s\n", __func__);
    return IDirectSound8_GetSpeakerConfig(wrapper->dsound8, config);
}

static HRESULT WINAPI dsound_wrapper_SetSpeakerConfig(IDirectSound *iface, DWORD config)
{
    struct dsound_wrapper *wrapper = CONTAINING_RECORD(iface, struct dsound_wrapper, IDirectSound_iface);
    ok(0, "unexpected call %s\n", __func__);
    return IDirectSound8_SetSpeakerConfig(wrapper->dsound8, config);
}

static HRESULT WINAPI dsound_wrapper_Initialize(IDirectSound *iface, const GUID *guid)
{
    struct dsound_wrapper *wrapper = CONTAINING_RECORD(iface, struct dsound_wrapper, IDirectSound_iface);
    ok(0, "unexpected call %s\n", __func__);
    return IDirectSound8_Initialize(wrapper->dsound8, guid);
}

static IDirectSoundVtbl dsound_wrapper_vtbl =
{
    dsound_wrapper_QueryInterface,
    dsound_wrapper_AddRef,
    dsound_wrapper_Release,
    dsound_wrapper_CreateSoundBuffer,
    dsound_wrapper_GetCaps,
    dsound_wrapper_DuplicateSoundBuffer,
    dsound_wrapper_SetCooperativeLevel,
    dsound_wrapper_Compact,
    dsound_wrapper_GetSpeakerConfig,
    dsound_wrapper_SetSpeakerConfig,
    dsound_wrapper_Initialize,
};

struct test_stream
{
    IStream IStream_iface;
    IDirectMusicGetLoader IDirectMusicGetLoader_iface;
    LONG refcount;

    IDirectMusicLoader *loader;
    IStream *stream;
};

static struct test_stream *impl_from_IStream(IStream *iface)
{
    return CONTAINING_RECORD(iface, struct test_stream, IStream_iface);
}

static HRESULT WINAPI test_stream_QueryInterface(IStream *iface, REFIID iid, void **out)
{
    struct test_stream *stream = impl_from_IStream(iface);

    if (IsEqualIID(iid, &IID_IUnknown)
            || IsEqualIID(iid, &IID_IStream))
    {
        *out = &stream->IStream_iface;
        IStream_AddRef(&stream->IStream_iface);
        return S_OK;
    }

    if (IsEqualIID(iid, &IID_IUnknown)
            || IsEqualIID(iid, &IID_IDirectMusicGetLoader))
    {
        *out = &stream->IDirectMusicGetLoader_iface;
        IDirectMusicGetLoader_AddRef(&stream->IDirectMusicGetLoader_iface);
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI test_stream_AddRef(IStream *iface)
{
    struct test_stream *stream = impl_from_IStream(iface);
    return InterlockedIncrement(&stream->refcount);
}

static ULONG WINAPI test_stream_Release(IStream *iface)
{
    struct test_stream *stream = impl_from_IStream(iface);
    ULONG ref = InterlockedDecrement(&stream->refcount);

    if (!ref)
    {
        IDirectMusicLoader_Release(stream->loader);
        IStream_Release(stream->stream);
        free(stream);
    }

    return ref;
}

static HRESULT WINAPI test_stream_Read(IStream *iface, void *data, ULONG size, ULONG *read)
{
    struct test_stream *stream = impl_from_IStream(iface);
    return IStream_Read(stream->stream, data, size, read);
}

static HRESULT WINAPI test_stream_Write(IStream *iface, const void *data, ULONG size, ULONG *written)
{
    struct test_stream *stream = impl_from_IStream(iface);
    return IStream_Write(stream->stream, data, size, written);
}

static HRESULT WINAPI test_stream_Seek(IStream *iface, LARGE_INTEGER offset, DWORD method, ULARGE_INTEGER *position)
{
    struct test_stream *stream = impl_from_IStream(iface);
    return IStream_Seek(stream->stream, offset, method, position);
}

static HRESULT WINAPI test_stream_SetSize(IStream *iface, ULARGE_INTEGER size)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI test_stream_CopyTo(IStream *iface, IStream *dest, ULARGE_INTEGER size,
        ULARGE_INTEGER *read_size, ULARGE_INTEGER *write_size)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI test_stream_Commit(IStream *iface, DWORD flags)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI test_stream_Revert(IStream *iface)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI test_stream_LockRegion(IStream *iface, ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI test_stream_UnlockRegion(IStream *iface, ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI test_stream_Stat(IStream *iface, STATSTG *stat, DWORD flags)
{
    struct test_stream *stream = impl_from_IStream(iface);
    return IStream_Stat(stream->stream, stat, flags);
}

static const IStreamVtbl test_stream_vtbl;
static const IDirectMusicGetLoaderVtbl test_stream_get_loader_vtbl;

static HRESULT WINAPI test_stream_Clone(IStream *iface, IStream **out)
{
    struct test_stream *clone, *stream = impl_from_IStream(iface);

    if (!(clone = calloc(1, sizeof(*clone))))
        return E_OUTOFMEMORY;

    clone->IStream_iface.lpVtbl = &test_stream_vtbl;
    clone->IDirectMusicGetLoader_iface.lpVtbl = &test_stream_get_loader_vtbl;
    clone->refcount = 1;

    clone->loader = stream->loader;
    IDirectMusicLoader8_AddRef(clone->loader);
    IStream_Clone(stream->stream, &clone->stream);

    *out = &clone->IStream_iface;
    return S_OK;
}

static const IStreamVtbl test_stream_vtbl =
{
    test_stream_QueryInterface,
    test_stream_AddRef,
    test_stream_Release,
    test_stream_Read,
    test_stream_Write,
    test_stream_Seek,
    test_stream_SetSize,
    test_stream_CopyTo,
    test_stream_Commit,
    test_stream_Revert,
    test_stream_LockRegion,
    test_stream_UnlockRegion,
    test_stream_Stat,
    test_stream_Clone,
};

static HRESULT WINAPI test_stream_get_loader_QueryInterface(IDirectMusicGetLoader *iface, REFIID iid, void **out)
{
    struct test_stream *stream = CONTAINING_RECORD(iface, struct test_stream, IDirectMusicGetLoader_iface);
    return IStream_QueryInterface(&stream->IStream_iface, iid, out);
}

static ULONG WINAPI test_stream_get_loader_AddRef(IDirectMusicGetLoader *iface)
{
    struct test_stream *stream = CONTAINING_RECORD(iface, struct test_stream, IDirectMusicGetLoader_iface);
    return IStream_AddRef(&stream->IStream_iface);
}

static ULONG WINAPI test_stream_get_loader_Release(IDirectMusicGetLoader *iface)
{
    struct test_stream *stream = CONTAINING_RECORD(iface, struct test_stream, IDirectMusicGetLoader_iface);
    return IStream_Release(&stream->IStream_iface);
}

static HRESULT WINAPI test_stream_get_loader_GetLoader(IDirectMusicGetLoader *iface, IDirectMusicLoader **loader)
{
    struct test_stream *stream = CONTAINING_RECORD(iface, struct test_stream, IDirectMusicGetLoader_iface);
    *loader = stream->loader;
    IDirectMusicGetLoader_AddRef(*loader);
    return S_OK;
}

static const IDirectMusicGetLoaderVtbl test_stream_get_loader_vtbl =
{
    test_stream_get_loader_QueryInterface,
    test_stream_get_loader_AddRef,
    test_stream_get_loader_Release,
    test_stream_get_loader_GetLoader,
};

static HRESULT test_stream_create(IDirectMusicLoader *loader, IStream **out)
{
    struct test_stream *stream;

    if (!(stream = calloc(1, sizeof(*stream))))
        return E_OUTOFMEMORY;

    stream->IStream_iface.lpVtbl = &test_stream_vtbl;
    stream->IDirectMusicGetLoader_iface.lpVtbl = &test_stream_get_loader_vtbl;
    stream->refcount = 1;

    ok_hr(S_OK, CreateStreamOnHGlobal(0, TRUE, &stream->stream));
    stream->loader = loader;
    IDirectMusicLoader8_AddRef(stream->loader);

    *out = &stream->IStream_iface;
    return S_OK;
}

static void test_ffviii(void)
{
    DMUS_PORTCAPS caps = {.dwSize = sizeof(DMUS_PORTCAPS)}, expect_caps = caps;
    DMUS_PORTPARAMS params =
    {
        .dwSize = sizeof(params),
        .dwValidParams = DMUS_PORTPARAMS_EFFECTS,
        .dwEffectFlags = 1,
    };
    KSPROPERTY property =
    {
        .Set = GUID_DMUS_PROP_XG_Capable,
        .Id = 0,
        .Flags = KSPROPERTY_TYPE_BASICSUPPORT,
    };
    DSBUFFERDESC buffer_desc =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_PRIMARYBUFFER,
    };
    WAVEFORMATEX waveformat =
    {
        .wFormatTag = 1,
        .nChannels = 2,
        .nSamplesPerSec = 44100,
        .nAvgBytesPerSec = 174600,
        .nBlockAlign = 4,
        .wBitsPerSample = 16,
    };
    DMUS_OBJECTDESC collection_desc =
    {
        .dwSize = sizeof(DMUS_OBJECTDESC),
        .dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME,
        .guidClass = CLSID_DirectMusicCollection,
        .wszFileName = L"ff8.dls",
    };
    DMUS_OBJECTDESC segment_desc =
    {
        .dwSize = sizeof(DMUS_OBJECTDESC),
        .dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME,
        .guidClass = CLSID_DirectMusicSegment,
        .wszFileName = L"079s-demo.sgt",
    };
    struct dsound_wrapper wrapper = {{&dsound_wrapper_vtbl}};
    IDirectMusicPerformance8 *performance;
    IDirectMusicCollection *collection;
    IDirectMusic *dmusic, *tmp_dmusic;
    IDirectMusicSegment *segment;
    IDirectSoundBuffer *dsbuffer;
    IDirectMusicLoader8 *loader;
    IDirectMusicTool8 *tool;
    IDirectMusicPort *port;
    IDirectSound8 *dsound;
    WCHAR path[MAX_PATH];
    IKsControl *control;
    DWORD value, ret;
    HWND hwnd;

    ok_hr(S_OK, test_tool_create(&tool));

    hwnd = CreateWindowW( L"static", L"Title", WS_POPUP | WS_VISIBLE, 10, 10, 200, 200, NULL, NULL, NULL, NULL );
    ok( !!hwnd, "CreateWindowW failed, error %lu\n", GetLastError() );

    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectSound8, (void **)&dsound));
    ok_hr(S_OK, IDirectSound8_Initialize(dsound, NULL));
    ok_hr(S_OK, IDirectSound8_SetCooperativeLevel(dsound, hwnd, 3));
    ok_hr(S_OK, IDirectSound8_CreateSoundBuffer(dsound, &buffer_desc, &dsbuffer, NULL));
    ok_hr(S_OK, IDirectSoundBuffer8_SetFormat(dsbuffer, &waveformat));
    ok_hr(S_OK, IDirectSoundBuffer8_Play(dsbuffer, 0, 0, 1));

    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusic, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusic8, (void **)&dmusic));

    wrapper.dsound8 = dsound;
    ok_hr(S_OK, IDirectMusic8_SetDirectSound(dmusic, &wrapper.IDirectSound_iface, NULL));

    ok_hr(S_OK, IDirectMusic8_EnumPort(dmusic, 0, &caps));
    expect_caps.dwFlags = 0x100;
    expect_caps.dwClass = 1;
    expect_caps.dwMaxChannelGroups = 0x1;
    wcscpy(expect_caps.wszDescription, L"Microsoft MIDI Mapper [Emulated]");
    check_dmus_portcaps( &caps, &expect_caps );
    ok_hr(S_OK, IDirectMusic8_EnumPort(dmusic, 1, &caps));
    expect_caps.dwFlags = 0x100;
    expect_caps.dwClass = 1;
    expect_caps.dwMaxChannelGroups = 0x1;
    wcscpy(expect_caps.wszDescription, L"Microsoft GS Wavetable Synth [Emulated]");
    check_dmus_portcaps( &caps, &expect_caps );
    ok_hr(S_OK, IDirectMusic8_EnumPort(dmusic, 2, &caps));
    expect_caps.dwFlags = 0xe85;
    expect_caps.dwClass = 1;
    expect_caps.guidPort = CLSID_DirectMusicSynth;
    expect_caps.dwType = 0x1;
    expect_caps.dwMemorySize = 0x7fffffff;
    expect_caps.dwMaxChannelGroups = 0x3e8;
    expect_caps.dwMaxVoices = 0x3e8;
    expect_caps.dwMaxAudioChannels = 0x2;
    expect_caps.dwEffectFlags = 0x1;
    wcscpy(expect_caps.wszDescription, L"Microsoft Synthesizer");
    check_dmus_portcaps( &caps, &expect_caps );
    ok_hr(S_FALSE, IDirectMusic8_EnumPort(dmusic, 3, &caps));

    ok_hr(S_OK, IDirectMusic8_CreatePort(dmusic, &CLSID_DirectMusicSynth, &params, &port, NULL));
    ok_hr(S_OK, IDirectMusicPort_Activate(port, TRUE));

    ok_hr(S_OK, IDirectMusicPort_QueryInterface(port, &IID_IKsControl, (void **)&control));
    property.Set = GUID_DMUS_PROP_XG_Capable;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_GS_Capable;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_DLS1;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_DLS2;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_Effects;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_INSTRUMENT2;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(0, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_LegacyCaps;
    ok_hr(DMUS_E_UNKNOWN_PROPERTY, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    property.Set = GUID_DMUS_PROP_SynthSink_DSOUND;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_SynthSink_WAVE;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");

    property.Flags = KSPROPERTY_TYPE_GET;
    property.Set = GUID_DMUS_PROP_XG_Capable;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_GS_Capable;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_DLS1;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_DLS2;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_Effects;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_INSTRUMENT2;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_SynthSink_DSOUND;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    property.Set = GUID_DMUS_PROP_SynthSink_WAVE;
    ok_hr(S_OK, IKsControl_KsProperty(control, &property, sizeof(property), &value, sizeof(value), &ret));
    ok_eq(1, ret, UINT, "%u");
    IKsControl_Release(control);

    ok_hr(S_OK, IDirectMusicPort_SetNumChannelGroups(port, 1));

    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8, (void **)&loader));
    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&performance));

    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/FINAL FANTASY VIII/Data/Music/dmusic/");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/FINAL FANTASY VIII/Data/Music/dmusic/");
    ok_hr(S_OK, IDirectMusicLoader_SetSearchDirectory(loader, &GUID_DirectMusicAllTypes, path, FALSE));

    ok_hr(S_OK, IDirectMusicPerformance8_Init(performance, &tmp_dmusic, 0, 0));
    ok_ne(dmusic, tmp_dmusic, IDirectMusic *, "%p");
    IDirectMusic8_Release(tmp_dmusic);

if (0)
{
IDirectMusicDownloadedInstrument *downloaded;
IDirectMusicInstrument *instrument;

ok_hr(S_OK, test_instrument_create(&instrument));
ok_hr(S_OK, IDirectMusicPort_DownloadInstrument(port, instrument, &downloaded, NULL, 0));
}

if (strcmp(winetest_platform, "wine") && 0)
{
    ok_hr(S_OK, test_port_create(&port));
}

    value = 1;
    ok_hr(S_OK, IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfAutoDownload, &value, sizeof(value)));
    ok_hr(S_OK, IDirectMusicPerformance8_AddPort(performance, port));
    ok_hr(S_OK, IDirectMusicPerformance8_AssignPChannelBlock(performance, 0, port, 1));

if (strcmp(winetest_platform, "wine") && 0)
{
    IDirectMusicGraph *graph;
    ok_hr(S_OK, IDirectMusicPerformance8_QueryInterface(performance, &IID_IDirectMusicGraph, (void **)&graph));
    ok_hr(E_NOTIMPL, IDirectMusicGraph_InsertTool(graph, (IDirectMusicTool *)tool, NULL, 0, -1));
    IDirectMusicGraph_Release(graph);
}

if (strcmp(winetest_platform, "wine") && 0)
{
    IDirectMusicGraph *graph;
    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicGraph, (void **)&graph));
    ok_hr(S_OK, IDirectMusicGraph_InsertTool(graph, (IDirectMusicTool *)tool, NULL, 0, -1));
    ok_hr(S_OK, IDirectMusicPerformance8_SetGraph(performance, graph));
    IDirectMusicGraph_Release(graph);
    ok(0,"%#04lx: set graph\n", GetCurrentThreadId());
}

    value = -1000;
    ok_hr(S_OK, IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &value, sizeof(value)));
    ok_hr(S_OK, IDirectMusicPerformance8_Stop(performance, 0, 0, 0, 0x4000));

    ok_hr(S_OK, IDirectMusicLoader_GetObject(loader, &collection_desc, &IID_IDirectMusicCollection, (void **)&collection));

    ok_hr(S_OK, IDirectMusicCollection_EnumInstrument(collection, 0, &value, path, ARRAY_SIZE(path)));
    ok(0, "value %#lx, path %s\n", value, debugstr_w(path));

if (0)
{
    IDirectMusicInstrument *instrument;
    void *private;

    ok_hr(S_OK, IDirectMusicCollection_GetInstrument(collection, value, &instrument));
    ok_hr(S_OK, IDirectMusicInstrument_QueryInterface(instrument, &IID_IDirectMusicInstrumentPRIVATE, &private));
    ok(0, "instrument %p\n", instrument);
    ok(0, "private %p\n", private);
}

    ok_hr(S_OK, IDirectMusicCollection_EnumInstrument(collection, 1, &value, path, ARRAY_SIZE(path)));
    ok(0, "value %#lx, path %s\n", value, debugstr_w(path));
    ok_hr(S_OK, IDirectMusicCollection_EnumInstrument(collection, 2, &value, path, ARRAY_SIZE(path)));
    ok(0, "value %#lx, path %s\n", value, debugstr_w(path));

    ok_hr(S_OK, IDirectMusicLoader_GetObject(loader, &segment_desc, &IID_IDirectMusicSegment, (void **)&segment));

if (strcmp(winetest_platform, "wine") && 0)
{
    IDirectMusicGraph *graph;
    ok_hr(DMUS_E_NOT_FOUND, IDirectMusicSegment8_GetGraph(segment, &graph));
}

if (strcmp(winetest_platform, "wine") && 0)
{
    GUID guid = {0xe8dbd832,0xbcf0,0x4c8c,{0xa0,0x75,0xa3,0xf1,0x5e,0x67,0xfd,0x63}};
    IDirectMusicSegmentState *state;
    IDirectMusicTrack8 *track;
    void *data;

    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicSegmentState, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegmentState8, (void **)&state));

    ok_hr(S_OK, IDirectMusicSegment8_GetTrack(segment, &GUID_NULL, -1, 0, (IDirectMusicTrack **)&track));
    ok_hr(S_OK, IDirectMusicTrack8_Init(track, segment));
    ok_hr(S_OK, IDirectMusicTrack8_InitPlay(track, state, (IDirectMusicPerformance *)performance, &data, 1, 0x800));
    ok_hr(S_OK, IDirectMusicTrack8_PlayEx(track, data, 0, 1, 0, 0x15, (IDirectMusicPerformance *)performance, state, 1));
    IDirectMusicTrack_Release(track);

    for (UINT i = 0; i < 5; i++)
    {
        ok_hr(S_OK, IDirectMusicSegment8_GetTrack(segment, &GUID_NULL, -1, i, (IDirectMusicTrack **)&track));
        ok_hr(S_OK, IDirectMusicTrack8_IsParamSupported(track, &guid));
        IDirectMusicTrack_Release(track);
    }
}

if (strcmp(winetest_platform, "wine") && 0)
{
    IDirectMusicTrack8 *track;

    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment8, (void **)&segment));

    ok_hr(S_OK, test_track_create(&track));
    ok_hr(S_OK, IDirectMusicSegment8_InsertTrack(segment, (IDirectMusicTrack *)track, 1));
    if (0) IDirectMusicTrack_Release(track);
}

if (strcmp(winetest_platform, "wine") && 0)
{
    static const LARGE_INTEGER zero = {0};
    IDirectMusicTrack8 *track;
    IPersistStream *persist;
    IStream *stream;

    ok_hr(S_OK, test_stream_create((IDirectMusicLoader *)loader, &stream));

    CHUNK_BEGIN(stream, "seqt")
    {
        DMUS_IO_SEQ_ITEM items[] =
        {
            {.mtTime=768,.mtDuration=256,.dwPChannel=0xd,.bStatus=0x90,.bByte1=0x27,.bByte2=0x78},
            {.mtTime=768,.mtDuration=256,.dwPChannel=0xc,.bStatus=0x90,.bByte1=0x33,.bByte2=0x78},
            {.mtTime=1536,.mtDuration=128,.dwPChannel=0xd,.bStatus=0x90,.bByte1=0x27,.bByte2=0x78},
            {.mtTime=1536,.mtDuration=128,.dwPChannel=0xc,.bStatus=0x90,.bByte1=0x33,.bByte2=0x78},
        };
        DMUS_IO_CURVE_ITEM curve_items[] =
        {
        };

        CHUNK_ARRAY(stream, "evtl", items);
        CHUNK_ARRAY(stream, "cuvl", curve_items);
    }
    CHUNK_END;
    ok_hr(S_OK, IStream_Seek(stream, zero, 0, NULL));

    ok_hr(S_OK, IDirectMusicSegment8_GetTrack(segment, &CLSID_DirectMusicSeqTrack, -1, 0, (IDirectMusicTrack **)&track));
    ok_hr(S_OK, IDirectMusicSegment8_RemoveTrack(segment, (IDirectMusicTrack *)track));

    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicSeqTrack, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicTrack, (void **)&track));
    ok_hr(S_OK, IDirectMusicSegment_QueryInterface(track, &IID_IPersistStream, (void **)&persist));
    ok_hr(S_OK, IPersistStream_Load(persist, stream));
    IPersistStream_Release(persist);

    ok_hr(S_OK, IDirectMusicSegment8_InsertTrack(segment, (IDirectMusicTrack *)track, 1));
    IDirectMusicTrack_Release(track);
}


if (strcmp(winetest_platform, "wine") && 0)
{
    static const LARGE_INTEGER zero = {0};
    IDirectMusicTrack8 *track;
    IPersistStream *persist;
    IStream *stream;

    ok_hr(S_OK, test_stream_create((IDirectMusicLoader *)loader, &stream));

    CHUNK_LIST(stream, "wavt")
    {
        DMUS_IO_WAVE_TRACK_HEADER head =
        {
        };
        CHUNK_DATA(stream, "wath", head);
        CHUNK_LIST(stream, "wavp")
        {
            DMUS_IO_WAVE_PART_HEADER head =
            {
            };
            CHUNK_DATA(stream, "waph", head);
            CHUNK_LIST(stream, "wavi")
            {
                CHUNK_LIST(stream, "wave")
                {
                    DMUS_IO_WAVE_ITEM_HEADER head =
                    {
                        .rtDuration = 1024,
                    };
                    CHUNK_DATA(stream, "waih", head);
                    CHUNK_LIST(stream, "DMRF")
                    {
                        DMUS_IO_REFERENCE reference = {.guidClassID = CLSID_DirectSoundWave, .dwValidData = 0x10};
                        WCHAR file[] = L"test.wav";

                        CHUNK_DATA(stream, "refh", reference);
                        CHUNK_DATA(stream, "file", file);
                    }
                    CHUNK_END;
                }
                CHUNK_END;
            }
            CHUNK_END;
        }
        CHUNK_END;
    }
    CHUNK_END;
    ok_hr(S_OK, IStream_Seek(stream, zero, 0, NULL));

    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicWaveTrack, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicTrack, (void **)&track));
    ok_hr(S_OK, IDirectMusicSegment_QueryInterface(track, &IID_IPersistStream, (void **)&persist));
    ok_hr(S_OK, IPersistStream_Load(persist, stream));
    IPersistStream_Release(persist);

    ok_hr(S_OK, IDirectMusicSegment8_InsertTrack(segment, (IDirectMusicTrack *)track, 1));
    IDirectMusicTrack_Release(track);
}


if (strcmp(winetest_platform, "wine") && 0)
{
    static const LARGE_INTEGER zero = {0};
    IPersistStream *persist;
    IStream *stream;

    ok_hr(S_OK, test_stream_create((IDirectMusicLoader *)loader, &stream));

    CHUNK_RIFF(stream, "DMSG")
    {
        DMUS_IO_SEGMENT_HEADER head =
        {
            .mtLength = 9216,
        };
        GUID clsid = CLSID_DirectMusicSegment;
        DMUS_VERSION version = {0};

        CHUNK_DATA(stream, "segh", head);
        CHUNK_DATA(stream, "guid", clsid);
        CHUNK_LIST(stream, "sgdl")
        {
            char segd[0x58] = {0};
            CHUNK_DATA(stream, "segd", segd);
        }
        CHUNK_END;
        CHUNK_DATA(stream, "vers", version);
        CHUNK_LIST(stream, "UNFO")
        {
            CHUNK_DATA(stream, "UNAM", L"Test Segment");
        }
        CHUNK_END;

        CHUNK_LIST(stream, "trkl")
        {
#if 1
            CHUNK_RIFF(stream, "DMTK")
            {
                DMUS_IO_TRACK_HEADER head =
                {
                    .guidClassID = CLSID_DirectMusicSeqTrack,
                    .dwGroup = 1,
                    .ckid = mmioFOURCC('s','e','q','t'),
                    .fccType = 0,
                };
                CHUNK_DATA(stream, "trkh", head);
                CHUNK_BEGIN(stream, "seqt")
                {
                    DMUS_IO_SEQ_ITEM items[] =
                    {
                        {.mtTime=768,.mtDuration=256,.dwPChannel=0xd,.bStatus=0x90,.bByte1=0x27,.bByte2=0x78},
                        {.mtTime=768,.mtDuration=256,.dwPChannel=0xc,.bStatus=0x90,.bByte1=0x33,.bByte2=0x78},
                        {.mtTime=1536,.mtDuration=128,.dwPChannel=0xd,.bStatus=0x90,.bByte1=0x27,.bByte2=0x78},
                        {.mtTime=1536,.mtDuration=128,.dwPChannel=0xc,.bStatus=0x90,.bByte1=0x33,.bByte2=0x78},
                    };
                    DMUS_IO_CURVE_ITEM curve_items[] =
                    {
                    };

                    CHUNK_ARRAY(stream, "evtl", items);
                    CHUNK_ARRAY(stream, "cuvl", curve_items);
                }
                CHUNK_END;
            }
            CHUNK_END;
#else
            CHUNK_RIFF(stream, "DMTK")
            {
                DMUS_IO_TRACK_HEADER head =
                {
                    .guidClassID = CLSID_DirectMusicWaveTrack,
                    .dwGroup = 1,
                    .fccType = mmioFOURCC('w','a','v','t'),
                };
                CHUNK_DATA(stream, "trkh", head);
                CHUNK_LIST(stream, "wavt")
                {
                    DMUS_IO_WAVE_TRACK_HEADER head = {0};
                    CHUNK_DATA(stream, "wath", head);
                    CHUNK_LIST(stream, "wavp")
                    {
                        DMUS_IO_WAVE_PART_HEADER head =
                        {
                            .dwPChannel = 1,
                        };
                        CHUNK_DATA(stream, "waph", head);
                        CHUNK_LIST(stream, "wavi")
                        {
                            CHUNK_LIST(stream, "wave")
                            {
                                DMUS_IO_WAVE_ITEM_HEADER head =
                                {
                                    .rtDuration = 1024,
                                };
                                CHUNK_DATA(stream, "waih", head);
                                CHUNK_LIST(stream, "DMRF")
                                {
                                    DMUS_IO_REFERENCE reference = {.guidClassID = CLSID_DirectSoundWave, .dwValidData = 0x10};
                                    WCHAR file[] = L"test.wav";

                                    CHUNK_DATA(stream, "refh", reference);
                                    CHUNK_DATA(stream, "file", file);
                                }
                                CHUNK_END;
                            }
                            CHUNK_END;
                        }
                        CHUNK_END;
                    }
                    CHUNK_END;
                }
                CHUNK_END;
            }
            CHUNK_END;
#endif
        }
        CHUNK_END;
    }
    CHUNK_END;
    ok_hr(S_OK, IStream_Seek(stream, zero, 0, NULL));

    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment8, (void **)&segment));
    ok_hr(S_OK, IDirectMusicSegment_QueryInterface(segment, &IID_IPersistStream, (void **)&persist));
    ok_hr(S_OK, IPersistStream_Load(persist, stream));
    IPersistStream_Release(persist);
}

    ok_hr(S_OK, IDirectMusicSegment8_SetParam(segment, &GUID_ConnectToDLSCollection, -1, 0, 0, collection));
    ok_hr(S_OK, IDirectMusicPerformance8_IsPlaying(performance, segment, NULL));

    value = -1000;
    ok_hr(S_OK, IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &value, sizeof(value)));

    ok_hr(S_OK, IDirectMusicSegment8_SetStartPoint(segment, 0));
    ok_hr(S_OK, IDirectMusicPerformance8_PlaySegment(performance, segment, 0x800, 0, NULL));

    value = 0;
    ok_hr(S_OK, IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &value, sizeof(value)));


Sleep(5000);


    IDirectMusicPerformance8_Release(performance);
    IDirectMusicLoader_Release(loader);

    IDirectMusicPort_Release(port);

    IDirectMusic8_Release(dmusic);
}

static void test_load(void)
{
    static const LARGE_INTEGER zero = {0};
    IDirectMusicTrack *track;
    IPersistStream *persist;
    IStream *stream;

    ok_hr(S_OK, CreateStreamOnHGlobal(0, TRUE, &stream));

    CHUNK_RIFF(stream, "DMBT")
    {
        CHUNK_LIST(stream, "lbdl")
        {
            CHUNK_LIST(stream, "lbnd")
            {
                DMUS_IO_BAND_ITEM_HEADER head = {0};
                CHUNK_DATA(stream, "bdih", head);

                CHUNK_RIFF(stream, "DMBD")
                {
                    GUID guid = {0};
                    CHUNK_DATA(stream, "guid", guid);

                    CHUNK_LIST(stream, "UNFO")
                    {
                        WCHAR name[16] = {0};
                        CHUNK_DATA(stream, "UNAM", name);
                    }
                    CHUNK_END;

                    CHUNK_LIST(stream, "lbil")
                    {
                        CHUNK_LIST(stream, "lbin")
                        {
                            DMUS_IO_INSTRUMENT instrument = {0};
                            CHUNK_DATA(stream, "bins", instrument);
                        }
                        CHUNK_END;

                        CHUNK_LIST(stream, "lbin")
                        {
                            DMUS_IO_INSTRUMENT instrument = {0};
                            CHUNK_DATA(stream, "bins", instrument);
                        }
                        CHUNK_END;

                        CHUNK_LIST(stream, "lbin")
                        {
                            DMUS_IO_INSTRUMENT instrument = {0};
                            CHUNK_DATA(stream, "bins", instrument);

                            CHUNK_LIST(stream, "DMRF")
                            {
                                DMUS_IO_REFERENCE reference = {0};
                                DMUS_VERSION vers = {0};
                                WCHAR name[16] = {0};
                                WCHAR file[16] = {0};
                                GUID guid = {0};

                                CHUNK_DATA(stream, "refh", reference);
                                CHUNK_DATA(stream, "guid", guid);
                                CHUNK_DATA(stream, "name", name);
                                CHUNK_DATA(stream, "file", file);
                                CHUNK_DATA(stream, "vers", vers);
                            }
                            CHUNK_END;
                        }
                        CHUNK_END;
                    }
                    CHUNK_END;
                }
                CHUNK_END;
            }
            CHUNK_END;
        }
        CHUNK_END;
    }
    CHUNK_END;

    ok_hr(S_OK, IStream_Seek(stream, zero, 0, NULL));
    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicBandTrack, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicTrack, (void **)&track));
    ok_hr(S_OK, IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&persist));
    ok_hr(E_NOTIMPL, IPersistStream_Save(persist, stream, TRUE));
    ok_hr(E_NOTIMPL, IPersistStream_Load(persist, stream));

    IDirectMusicTrack_Release(track);
    IPersistStream_Release(persist);
    IStream_Release(stream);
}


static void test_load_collection(void)
{
    static const LARGE_INTEGER zero = {0};
    IPersistStream *persist;
    IStream *stream;

    ok_hr(S_OK, CreateStreamOnHGlobal(0, TRUE, &stream));

    CHUNK_RIFF( stream, "DLS " )
    {
        CHUNK_BEGIN(stream, "colh")
        {
            DWORD count = 0;
            IStream_Write( stream, &count, 4, NULL );
        }
        CHUNK_END;

        CHUNK_BEGIN(stream, "ptbl")
        {
            DWORD count = 0;
            IStream_Write( stream, &count, 4, NULL );
            IStream_Write( stream, &count, 4, NULL );
        }
        CHUNK_END;
    }
    CHUNK_END;

    ok_hr(S_OK, IStream_Seek(stream, zero, 0, NULL));
    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicCollection, NULL, CLSCTX_INPROC_SERVER, &IID_IPersistStream, (void **)&persist));
    ok_hr(E_NOTIMPL, IPersistStream_Save(persist, stream, TRUE));
    ok_hr(E_NOTIMPL, IPersistStream_Load(persist, stream));
    IPersistStream_Release(persist);
    IStream_Release(stream);

    ok_hr(S_OK, IStream_Seek(stream, zero, 0, NULL));
    ok_hr(S_OK, CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IPersistStream, (void **)&persist));
    ok_hr(E_NOTIMPL, IPersistStream_Save(persist, stream, TRUE));
    ok_hr(E_NOTIMPL, IPersistStream_Load(persist, stream));
    IPersistStream_Release(persist);
    IStream_Release(stream);
}

static void test_blood_island(void)
{
    BYTE data[115015];
    IDirectMusicAudioPath *default_path, *standard_path;
    IDirectMusicPerformance8 *performance;
    IDirectMusicSegment8 *segment;
    IDirectMusicLoader8 *loader;
    IDirectSoundBuffer *buffer;
    DMUS_OBJECTDESC desc =
    {
        .dwSize = sizeof(DMUS_OBJECTDESC),
        .dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_LOADED | DMUS_OBJ_MEMORY,
        .guidClass = CLSID_DirectMusicSegment,
        .llMemLength = sizeof(data),
        .pbMemData = data,
    };
    WCHAR path[MAX_PATH];
    DWORD volume, size;
    HANDLE file;
    HRESULT hr;

    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/AirBkLow.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/AirBkLow.wav");
    file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "failed to create %s, error %lu\n", debugstr_w(path), GetLastError());
    ReadFile(file, data, sizeof(data), &size, NULL);
    desc.llMemLength = size;
    CloseHandle(file);

    hr = CoCreateInstance(&CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8, (void **)&loader);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&performance);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_InitAudio(performance, NULL, NULL, NULL, 1, 64, 0x3f, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_GetDefaultAudioPath(performance, &default_path);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_CreateStandardAudioPath(performance, 6, 64, 1, &standard_path);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicAudioPath_GetObjectInPath(standard_path, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, &GUID_NULL, 0, &IID_IDirectSoundBuffer, (void **)&buffer);
    ok(hr == S_OK, "got %#lx\n", hr);
    volume = -20000;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &volume, sizeof(volume));
    ok(hr == S_OK, "got %#lx\n", hr);
    volume = -1080;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &volume, sizeof(volume));
    ok(hr == S_OK, "got %#lx\n", hr);
    volume = -1080;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &volume, sizeof(volume));
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_StopEx(performance, (IUnknown *)standard_path, 0, 0);
    ok(hr == E_INVALIDARG, "got %#lx\n", hr);
    volume = -1080;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &volume, sizeof(volume));
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_StopEx(performance, (IUnknown *)standard_path, 0, 0);
    ok(hr == E_INVALIDARG, "got %#lx\n", hr);
    hr = IDirectMusicLoader_GetObject(loader, &desc, &IID_IDirectMusicSegment8, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetRepeats(segment, -1);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_PlaySegmentEx(performance, (IUnknown *)segment, NULL, 0, 0, 0, 0, 0, (IUnknown *)standard_path);
    ok(hr == S_OK, "got %#lx\n", hr);
    /* crash */

    hr = IDirectMusicPerformance8_CloseDown(performance);
    ok(hr == S_OK, "got %#lx\n", hr);
    IDirectMusicPerformance8_Release(performance);
}

static void test_eroico(void)
{
    IDirectMusicSegmentState *segment_state;
    IDirectMusicPerformance8 *performance;
    IDirectMusicAudioPath *audio_path;
    IDirectMusicSegment8 *segment;
    IDirectMusicLoader8 *loader;
    IDirectSoundBuffer *buffer;
    DMUS_OBJECTDESC desc =
    {
        /* FIXME */
    };
    DWORD volume;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&performance);
    hr = CoCreateInstance(&CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8, (void **)&loader);
    hr = IDirectMusicPerformance8_InitAudio(performance, NULL, NULL, GetDesktopWindow(), 8, 64, 0x3f, NULL);

    volume = 1000;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &volume, sizeof(volume));
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectMusicPerformance8_CreateStandardAudioPath(performance, 8, 64, -1, &audio_path);
    hr = IDirectMusicLoader_GetObject(loader, &desc, &IID_IDirectMusicSegment8, (void **)&segment);
    hr = IDirectMusicSegment8_SetParam(segment, &GUID_StandardMIDIFile, -1, 0, 0, NULL);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance);
    hr = IDirectMusicAudioPath_SetVolume(audio_path, 0, 0);
    hr = IDirectMusicAudioPath_GetObjectInPath(audio_path, 0xfffffffb, 0x6000, 0, &GUID_NULL, 0, &IID_IDirectSoundBuffer8, (void **)&buffer);

    hr = IDirectMusicPerformance8_IsPlaying(performance, (IDirectMusicSegment *)segment, NULL);
    hr = IDirectMusicAudioPath_SetVolume(audio_path, -9600, 300);

    hr = IDirectMusicSegment8_SetRepeats(segment, -1);
    hr = IDirectMusicPerformance8_PlaySegmentEx(performance, (IUnknown *)segment, NULL, NULL, 128, 0, &segment_state, NULL, (IUnknown *)audio_path);
    hr = IDirectMusicAudioPath_SetVolume(audio_path, -480, 500);
    hr = IDirectMusicPerformance8_IsPlaying(performance, (IDirectMusicSegment *)segment, NULL);
    hr = IDirectMusicAudioPath_SetVolume(audio_path, -1027, 500);
    hr = IDirectMusicSegment8_SetRepeats(segment, 0);

    hr = IDirectMusicPerformance8_CloseDown(performance);
    ok(hr == S_OK, "got %#lx\n", hr);
    IDirectMusicPerformance8_Release(performance);
}

static void test_freefall(void)
{
    IDirectMusicPerformance8 *performance1, *performance2;
    IDirectMusicSegment8 *segment;
    IDirectMusicLoader8 *loader;
    WCHAR path[MAX_PATH];
    DWORD volume;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8, (void **)&loader);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&performance1);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_InitAudio(performance1, NULL, NULL, GetDesktopWindow(), 8, 1, 0x3f, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_InitAudio(performance2, NULL, NULL, GetDesktopWindow(), 8, 96, 0x3f, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/");
    hr = IDirectMusicLoader8_SetSearchDirectory(loader, &GUID_DirectMusicAllTypes, path, FALSE);
    ok(hr == S_OK, "got %#lx\n", hr);
    volume = -108;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance1, &GUID_PerfMasterVolume, &volume, sizeof(volume));
    ok(hr == S_OK, "got %#lx\n", hr);
    volume = 0;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance2, &GUID_PerfMasterVolume, &volume, sizeof(volume));
    ok(hr == S_OK, "got %#lx\n", hr);
    volume = -108;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance1, &GUID_PerfMasterVolume, &volume, sizeof(volume));
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/airbklow.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetRepeats(segment, -1);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/airbrak2.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/airbrak2.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetRepeats(segment, -1);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/cylndnrg.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/cylndnrg.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetRepeats(segment, -1);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/grenade1.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/grenade1.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/grenade2.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/grenade2.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/laser1.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/laser1.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/laser2.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/laser2.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/lsrpwrup.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/lsrpwrup.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/nrglow.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/nrglow.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetRepeats(segment, -1);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/special.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/special.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/speeding.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/speeding.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetRepeats(segment, -1);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/thunder1.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/thunder1.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/thunder2.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/thunder2.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/thunder3.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/thunder3.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/topup.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/topup.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/vertlsr.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/vertlsr.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/windgust.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/windgust.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetRepeats(segment, -1);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/lightarc.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/lightarc.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/txtbleep.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/txtbleep.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/shield.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/shield.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetRepeats(segment, -1);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/whoosh2.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/whoosh2.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/death.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/death.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Freefall 3050AD/audio/level00.wav");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Freefall 3050AD/audio/level00.wav");
    hr = IDirectMusicLoader8_LoadObjectFromFile(loader, &CLSID_DirectMusicSegment, &IID_IDirectMusicSegment8, path, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_Download(segment, (IUnknown *)performance1);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetRepeats(segment, -1);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectMusicPerformance8_PlaySegmentEx(performance1, (IUnknown *)segment, NULL, NULL, 0, 0, NULL, NULL, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    /* crash */

    hr = IDirectMusicPerformance8_CloseDown(performance1);
    ok(hr == S_OK, "got %#lx\n", hr);
    IDirectMusicPerformance8_Release(performance1);

    hr = IDirectMusicPerformance8_CloseDown(performance2);
    ok(hr == S_OK, "got %#lx\n", hr);
    IDirectMusicPerformance8_Release(performance2);
}

static void test_recettear(void)
{
#if 0
    IDirectMusicPerformance8 *performance;
    IDirectMusicLoader8 *loader;
    HRESULT hr;

hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&performance);
dmusic:dmime_main.c:106:ClassFactory_CreateInstance iface 622AF044, pUnkOuter 00000000, ppv performance, ret 0.
dmusic:performance.c:977:IDirectMusicPerformance8_InitAudio iface performance, dmusic 00000000, dsound 00000000, hwnd 00010088, default_path_type 8, num_channels 64, flags 0x3f, params 00000000.
dmusic:performance.c:1041:IDirectMusicPerformance8_InitAudio iface performance, dmusic 00000000, dsound 00000000, params 00000000, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 64, fActivate 1, ppNewPath 09643108 stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 0A228C40, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 64, fActivate 1, ppNewPath 0964310C stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 0A229A20, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 64, fActivate 1, ppNewPath 09643110 stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 0A22A800, ret 0.
hr = CoCreateInstance(&CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8, (void **)&loader);
dmusic:dmloader_main.c:106:ClassFactory_CreateInstance iface 0D0F90B8, pUnkOuter 00000000, ret_iface loader, ret 0.
dmusic:loader.c:586:IDirectMusicLoader_SetSearchDirectory iface loader, class {d2ac2893-b39b-11d1-8704-00600893b1bd}, path L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear", clear 0.
dmusic:loader.c:611:IDirectMusicLoader_SetSearchDirectory iface loader, path L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear", ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/retitle2010.wav", ppObject 09643038.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\retitle2010.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/retitle2010.wav", ppObject 0A22C818, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A22C818, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A22C818, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A22C818, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A22C818, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/town.wav", ppObject 0964303C.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\town.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/town.wav", ppObject 0A22D878, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A22D878, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A22D878, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A22D878, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A22D878, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/sougen.wav", ppObject 09643040.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\sougen.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/sougen.wav", ppObject 0A22E8D8, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A22E8D8, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A22E8D8, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A22E8D8, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A22E8D8, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/cave.wav", ppObject 09643044.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\cave.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/cave.wav", ppObject 0A22F938, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A22F938, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A22F938, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A22F938, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A22F938, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/forest.wav", ppObject 09643048.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\forest.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/forest.wav", ppObject 0A230998, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A230998, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A230998, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A230998, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A230998, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/ruins.wav", ppObject 0964304C.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\ruins.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/ruins.wav", ppObject 0A2319F8, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A2319F8, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A2319F8, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A2319F8, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A2319F8, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/boss.wav", ppObject 09643050.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\boss.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/boss.wav", ppObject 0A232A58, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A232A58, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A232A58, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A232A58, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A232A58, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/over.wav", ppObject 09643054.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\over.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/over.wav", ppObject 0A233AB8, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A233AB8, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A233AB8, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A233AB8, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A233AB8, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/open.wav", ppObject 09643058.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\open.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/open.wav", ppObject 0A234B18, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A234B18, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A234B18, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A234B18, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A234B18, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/close.wav", ppObject 0964305C.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\close.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/close.wav", ppObject 0A235B78, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A235B78, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A235B78, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A235B78, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A235B78, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/treasure.wav", ppObject 09643060.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\treasure.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/treasure.wav", ppObject 0A236BD8, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A236BD8, dwRepeats 0.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A236BD8, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A236BD8, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A236BD8, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/fanfare.wav", ppObject 09643064.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\fanfare.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/fanfare.wav", ppObject 0A238328, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A238328, dwRepeats 0.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A238328, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A238328, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A238328, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/ed.wav", ppObject 09643068.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\ed.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/ed.wav", ppObject 0A239160, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A239160, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A239160, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A239160, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A239160, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/clear.wav", ppObject 0964306C.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\clear.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/clear.wav", ppObject 0A23C860, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A23C860, dwRepeats 0.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A23C860, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A23C860, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A23C860, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/night02.wav", ppObject 09643070.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\night02.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/night02.wav", ppObject 0A23D698, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A23D698, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A23D698, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A23D698, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A23D698, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/rival.wav", ppObject 09643074.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\rival.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/rival.wav", ppObject 0A23E4D0, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A23E4D0, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A23E4D0, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A23E4D0, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A23E4D0, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/lastboss02.wav", ppObject 09643078.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\lastboss02.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/lastboss02.wav", ppObject 0A23F308, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A23F308, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A23F308, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A23F308, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A23F308, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/lastd01.wav", ppObject 0964307C.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\lastd01.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/lastd01.wav", ppObject 0A240158, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A240158, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A240158, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A240158, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A240158, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/feaver.wav", ppObject 09643080.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\feaver.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/feaver.wav", ppObject 0A240538, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A240538, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A240538, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A240538, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A240538, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/staff.wav", ppObject 09643084.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\staff.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/staff.wav", ppObject 0A240918, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A240918, dwRepeats 0.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A240918, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A240918, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A240918, pAudioPath performance, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"bgm/water.wav", ppObject 09643088.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Recettear\\bgm\\water.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"bgm/water.wav", ppObject 0A240CF8, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 0A240CF8, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 0A240CF8, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A240CF8, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A240CF8, pAudioPath performance, ret 0.
dmusic:loader.c:268:IDirectMusicLoader_GetObject iface loader, pDesc 09BAF7E8, riid &IID_IDirectMusicSegment8, ppv 09642E7C.
dmusic:loader.c:450:IDirectMusicLoader_GetObject iface loader, pDesc 09BAF7E8, ppv 0A2410D8, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 0A2410D8, pAudioPath performance.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 0A2410D8, pAudioPath performance, ret 0.
dmusic:loader.c:268:IDirectMusicLoader_GetObject iface loader, pDesc 09BAF7E8, riid &IID_IDirectMusicSegment8, ppv 09642E80.
dmusic:loader.c:406:IDirectMusicLoader_GetObject iface loader, pDesc 09BAF7E8, ppv 00000000, ret 0x80004005.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 0A228C40, lVolume -1458, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 0A228C40, ret 0.
dmusic:performance.c:344:IDirectMusicPerformance8_Stop iface performance, pSegment 00000000, pSegmentState 00000000, mtTime 0, dwFlags 0 stub!
dmusic:performance.c:346:IDirectMusicPerformance8_Stop iface performance, pSegment 00000000, pSegmentState 00000000, ret 0.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A22C818, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A22C818, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A22C818.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A22C818, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A22D878, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A22D878, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A22D878.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A22D878, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A22E8D8, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A22E8D8, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A22E8D8.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A22E8D8, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A22F938, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A22F938, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A22F938.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A22F938, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A230998, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A230998, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A230998.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A230998, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A2319F8, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A2319F8, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A2319F8.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A2319F8, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A232A58, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A232A58, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A232A58.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A232A58, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A233AB8, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A233AB8, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A233AB8.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A233AB8, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A234B18, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A234B18, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A234B18.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A234B18, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A235B78, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A235B78, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A235B78.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A235B78, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A236BD8, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A236BD8, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A236BD8.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A236BD8, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A238328, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A238328, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A238328.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A238328, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A239160, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A239160, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A239160.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A239160, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A23C860, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A23C860, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A23C860.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A23C860, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A23D698, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A23D698, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A23D698.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A23D698, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A23E4D0, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A23E4D0, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A23E4D0.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A23E4D0, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A23F308, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A23F308, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A23F308.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A23F308, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A240158, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A240158, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A240158.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A240158, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A240538, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A240538, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A240538.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A240538, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A240918, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A240918, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A240918.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A240918, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A240CF8, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A240CF8, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A240CF8.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A240CF8, ret 1.
dmusic:segment.c:664:IDirectMusicSegment8_Unload iface 0A2410D8, pAudioPath performance stub!
dmusic:segment.c:665:IDirectMusicSegment8_Unload iface 0A2410D8, pAudioPath performance, ret 0.
dmusic:segment.c:99:IDirectMusicSegment8_Release iface 0A2410D8.
dmusic:segment.c:111:IDirectMusicSegment8_Release iface 0A2410D8, ret 1.
dmusic:loader.c:151:IDirectMusicLoader_Release iface loader.
dmusic:loader.c:163:IDirectMusicLoader_Release iface loader, ret 0.
dmusic:audiopath.c:117:IDirectMusicAudioPath_Release iface 0A228C40.
dmusic:audiopath.c:129:IDirectMusicAudioPath_Release iface 0A228C40, ret 0.
dmusic:audiopath.c:117:IDirectMusicAudioPath_Release iface 0A229A20.
dmusic:audiopath.c:129:IDirectMusicAudioPath_Release iface 0A229A20, ret 0.
dmusic:audiopath.c:117:IDirectMusicAudioPath_Release iface 0A22A800.
dmusic:audiopath.c:129:IDirectMusicAudioPath_Release iface 0A22A800, ret 0.
dmusic:performance.c:896:IDirectMusicPerformance8_CloseDown iface performance stub!
dmusic:performance.c:912:IDirectMusicPerformance8_CloseDown iface performance, ret 0.
dmusic:performance.c:302:IDirectMusicPerformance8_Release iface performance.
dmusic:performance.c:313:IDirectMusicPerformance8_Release iface performance, ret 22.
dmusic:dmime_main.c:152:DllCanUnloadNow 
dmusic:dmime_main.c:154:DllCanUnloadNow ret 0x1.
dmusic:dmusic_main.c:138:DllCanUnloadNow 
dmusic:dmusic_main.c:140:DllCanUnloadNow ret 0.
dmusic:dmsynth_main.c:127:DllCanUnloadNow 
dmusic:dmsynth_main.c:129:DllCanUnloadNow ret 0x1.
dmusic:dmloader_main.c:137:DllCanUnloadNow 
dmusic:dmloader_main.c:139:DllCanUnloadNow ret 0.

    hr = IDirectMusicPerformance8_CloseDown(performance);
    ok(hr == S_OK, "got %#lx\n", hr);
    IDirectMusicPerformance8_Release(performance);
#endif
}

static void test_rise_of_nations(void)
{
#if 0
    IDirectMusicPerformance8 *performance;
    IDirectMusicLoader8 *loader;
    HRESULT hr;

hr = CoCreateInstance(&CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8, (void **)&loader);
dmusic:loader.c:915:create_dmloader lpcGUID &IID_IDirectMusicLoader8, ppobj 01AFD68C.
dmusic:loader.c:464:IDirectMusicLoader_SetObject iface loader, pDesc 01AFD270.
dmusic:dmobject.c:224:dump_DMUS_OBJECTDESC desc 01AFD270.
dmusic:dmobject.c:229:dump_DMUS_OBJECTDESC DMUS_OBJECTDESC (01AFD270): - dwSize = 848
dmusic:dmobject.c:233:dump_DMUS_OBJECTDESC  - dwValidData = 0x000033 ( DMUS_OBJ_OBJECT DMUS_OBJ_CLASS DMUS_OBJ_FILENAME DMUS_OBJ_FULLPATH )
dmusic:dmobject.c:250:dump_DMUS_OBJECTDESC  - guidClass = CLSID_DirectMusicCollection
dmusic:dmobject.c:252:dump_DMUS_OBJECTDESC  - guidObject = {f17e8673-c3b4-11d1-870b-00600893b1bd}
dmusic:dmobject.c:269:dump_DMUS_OBJECTDESC  - wszFileName = L"C:\\windows\\system32\\drivers\\gm.dls"
dmusic:dmobject.c:275:dump_DMUS_OBJECTDESC desc 01AFD270.
dmusic:loaderstream.c:313:DMUSIC_CreateDirectMusicLoaderFileStream ppobj 01AFCD98.
dmusic:loaderstream.c:94:IDirectMusicLoaderFileStream_IStream_QueryInterface iface 020FABB0, riid {0000000c-0000-0000-c000-000000000046}, ppobj 01AFCD98.
dmusic:loaderstream.c:112:IDirectMusicLoaderFileStream_IStream_AddRef iface 020FABB0.
dmusic:loaderstream.c:113:IDirectMusicLoaderFileStream_IStream_AddRef iface 020FABB0, ret 1.
dmusic:loaderstream.c:99:IDirectMusicLoaderFileStream_IStream_QueryInterface iface 020FABB0, ppobj 020FABB0, ret 0.
dmusic:loaderstream.c:323:DMUSIC_CreateDirectMusicLoaderFileStream ppobj 01AFCD98, ret 0.
dmusic:loaderstream.c:75:IDirectMusicLoaderFileStream_Attach iface 020FABB0, wzFile L"C:\\windows\\system32\\drivers\\gm.dls", pLoader loader.
dmusic:loaderstream.c:68:IDirectMusicLoaderFileStream_Detach (020FABB0)
dmusic:loaderstream.c:79:IDirectMusicLoaderFileStream_Attach : failed
dmusic:loaderstream.c:80:IDirectMusicLoaderFileStream_Attach iface 020FABB0, pLoader loader(TypeKind.INVALID), ret 0x88781182.
dmusic:loader.c:493:IDirectMusicLoader_SetObject : could not attach stream to file L"C:\\windows\\system32\\drivers\\gm.dls", make sure it exists
dmusic:loaderstream.c:120:IDirectMusicLoaderFileStream_IStream_Release iface 020FABB0.
dmusic:loaderstream.c:68:IDirectMusicLoaderFileStream_Detach (020FABB0)
dmusic:loaderstream.c:127:IDirectMusicLoaderFileStream_IStream_Release iface 020FABB0, ret 0.
dmusic:loader.c:495:IDirectMusicLoader_SetObject iface loader, pDesc 01AFD270, ret 0x88781182.
dmusic:loader.c:123:IDirectMusicLoader_QueryInterface iface loader, riid &IID_IDirectMusicLoader8, ppobj 01AFD68C.
dmusic:loader.c:141:IDirectMusicLoader_AddRef iface loader.
dmusic:loader.c:143:IDirectMusicLoader_AddRef iface loader, ret 1.
dmusic:loader.c:129:IDirectMusicLoader_QueryInterface iface loader, ppobj loader, ret 0.
dmusic:loader.c:947:create_dmloader lpcGUID &IID_IDirectMusicLoader8, ppobj loader, ret 0.
dmusic:dmloader_main.c:106:ClassFactory_CreateInstance iface 079C90B8, pUnkOuter 00000000, ret_iface loader, ret 0.
hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&performance);
dmusic:dmime_main.c:106:ClassFactory_CreateInstance iface 07A2F044, pUnkOuter 00000000, ppv performance, ret 0.
dmusic:performance.c:977:IDirectMusicPerformance8_InitAudio iface performance, dmusic 04B85528, dsound 04B8553C, hwnd 0008007E, default_path_type 8, num_channels 2, flags 0x30, params 00000000.
dmusic:performance.c:1041:IDirectMusicPerformance8_InitAudio iface performance, dmusic 02106BB0, dsound 02106BE4, params 00000000, ret 0.
dmusic:loader.c:586:IDirectMusicLoader_SetSearchDirectory iface loader, class {d2ac2893-b39b-11d1-8704-00600893b1bd}, path L"..\\", clear 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 0DABFED8, pmtNow 00000000.
dmusic:loader.c:611:IDirectMusicLoader_SetSearchDirectory iface loader, path L"..\\", ret 0.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 134290000, pmtNow 0, ret 0.
dmusic:performance.c:1243:IDirectMusicPerformance8_GetDefaultAudioPath iface performance, ppAudioPath 04B85534 stub!
dmusic:audiopath.c:106:IDirectMusicAudioPath_AddRef iface 06F07450.
dmusic:audiopath.c:109:IDirectMusicAudioPath_AddRef iface 06F07450, ret 2.
dmusic:performance.c:1251:IDirectMusicPerformance8_GetDefaultAudioPath iface performance, ppAudioPath 06F07450, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06F07450, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 04B85538 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06F07450, ppObject 020FABB0, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 2, fActivate 1, ppNewPath 04A33364 stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 06EB0DB0, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06EB0DB0, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 04A33368 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06EB0DB0, ppObject 06F04168, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06EB0DB0, dwPChannel 0, dwStage 8704, dwBuffer 0, guidObject {00000000-0000-0000-0000-000000000000}, dwIndex 0, iidInterface {2befc277-5497-11d2-bccb-00a0c922e6eb}, ppObject 01AFD728 stub!
dmusic:graph.c:259:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:dmobject.c:757:dmobject_init dmobj 06E32AD8, class {d2ac2884-b39b-11d1-8704-00600893b1bd}, outer_unk 06E32AD0.
dmusic:dmobject.c:763:dmobject_init dmobj 06E32AD8, outer_unk 06E32AD0.
dmusic:graph.c:47:DirectMusicGraph_QueryInterface iface 06E32AD0, riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E32AD0.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E32AD0, ret 2.
dmusic:graph.c:64:DirectMusicGraph_QueryInterface iface 06E32AD0, ret_iface 06E32AD0, ret 0.
dmusic:graph.c:87:DirectMusicGraph_Release iface 06E32AD0.
dmusic:graph.c:93:DirectMusicGraph_Release iface 06E32AD0, ret 1.
dmusic:graph.c:277:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 06E32AD0, ret 0.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E32AD0.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E32AD0, ret 2.
dmusic:audiopath.c:180:IDirectMusicAudioPath_GetObjectInPath iface 06EB0DB0, ppObject 06E32AD0, ret 0.
dmusic:graph.c:111:DirectMusicGraph_InsertTool iface 06E32AD0, pTool 0C57F67C, pdwPChannels 00000000, cPChannels 0, lIndex 0 stub!
dmusic:graph.c:145:DirectMusicGraph_InsertTool iface 06E32AD0, pTool 0C57F67C, pdwPChannels 0, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 2, fActivate 1, ppNewPath 04A33394 stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 06E734E0, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06E734E0, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 04A33398 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06E734E0, ppObject 021186E8, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06E734E0, dwPChannel 0, dwStage 8704, dwBuffer 0, guidObject {00000000-0000-0000-0000-000000000000}, dwIndex 0, iidInterface {2befc277-5497-11d2-bccb-00a0c922e6eb}, ppObject 01AFD728 stub!
dmusic:graph.c:259:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:dmobject.c:757:dmobject_init dmobj 06E73870, class {d2ac2884-b39b-11d1-8704-00600893b1bd}, outer_unk 06E73868.
dmusic:dmobject.c:763:dmobject_init dmobj 06E73870, outer_unk 06E73868.
dmusic:graph.c:47:DirectMusicGraph_QueryInterface iface 06E73868, riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E73868.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E73868, ret 2.
dmusic:graph.c:64:DirectMusicGraph_QueryInterface iface 06E73868, ret_iface 06E73868, ret 0.
dmusic:graph.c:87:DirectMusicGraph_Release iface 06E73868.
dmusic:graph.c:93:DirectMusicGraph_Release iface 06E73868, ret 1.
dmusic:graph.c:277:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 06E73868, ret 0.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E73868.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E73868, ret 2.
dmusic:audiopath.c:180:IDirectMusicAudioPath_GetObjectInPath iface 06E734E0, ppObject 06E73868, ret 0.
dmusic:graph.c:111:DirectMusicGraph_InsertTool iface 06E73868, pTool 0C57F69C, pdwPChannels 00000000, cPChannels 0, lIndex 0 stub!
dmusic:graph.c:145:DirectMusicGraph_InsertTool iface 06E73868, pTool 0C57F69C, pdwPChannels 0, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 2, fActivate 1, ppNewPath 04A333C4 stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 06E80CF0, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06E80CF0, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 04A333C8 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06E80CF0, ppObject 06F00AB8, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06E80CF0, dwPChannel 0, dwStage 8704, dwBuffer 0, guidObject {00000000-0000-0000-0000-000000000000}, dwIndex 0, iidInterface {2befc277-5497-11d2-bccb-00a0c922e6eb}, ppObject 01AFD728 stub!
dmusic:graph.c:259:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:dmobject.c:757:dmobject_init dmobj 06E81080, class {d2ac2884-b39b-11d1-8704-00600893b1bd}, outer_unk 06E81078.
dmusic:dmobject.c:763:dmobject_init dmobj 06E81080, outer_unk 06E81078.
dmusic:graph.c:47:DirectMusicGraph_QueryInterface iface 06E81078, riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E81078.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E81078, ret 2.
dmusic:graph.c:64:DirectMusicGraph_QueryInterface iface 06E81078, ret_iface 06E81078, ret 0.
dmusic:graph.c:87:DirectMusicGraph_Release iface 06E81078.
dmusic:graph.c:93:DirectMusicGraph_Release iface 06E81078, ret 1.
dmusic:graph.c:277:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 06E81078, ret 0.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E81078.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E81078, ret 2.
dmusic:audiopath.c:180:IDirectMusicAudioPath_GetObjectInPath iface 06E80CF0, ppObject 06E81078, ret 0.
dmusic:graph.c:111:DirectMusicGraph_InsertTool iface 06E81078, pTool 0C57F6BC, pdwPChannels 00000000, cPChannels 0, lIndex 0 stub!
dmusic:graph.c:145:DirectMusicGraph_InsertTool iface 06E81078, pTool 0C57F6BC, pdwPChannels 0, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 2, fActivate 1, ppNewPath 04A333F4 stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 06F080B8, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06F080B8, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 04A333F8 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06F080B8, ppObject 020F4A60, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06F080B8, dwPChannel 0, dwStage 8704, dwBuffer 0, guidObject {00000000-0000-0000-0000-000000000000}, dwIndex 0, iidInterface {2befc277-5497-11d2-bccb-00a0c922e6eb}, ppObject 01AFD728 stub!
dmusic:graph.c:259:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:dmobject.c:757:dmobject_init dmobj 06F08448, class {d2ac2884-b39b-11d1-8704-00600893b1bd}, outer_unk 06F08440.
dmusic:dmobject.c:763:dmobject_init dmobj 06F08448, outer_unk 06F08440.
dmusic:graph.c:47:DirectMusicGraph_QueryInterface iface 06F08440, riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06F08440.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06F08440, ret 2.
dmusic:graph.c:64:DirectMusicGraph_QueryInterface iface 06F08440, ret_iface 06F08440, ret 0.
dmusic:graph.c:87:DirectMusicGraph_Release iface 06F08440.
dmusic:graph.c:93:DirectMusicGraph_Release iface 06F08440, ret 1.
dmusic:graph.c:277:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 06F08440, ret 0.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06F08440.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06F08440, ret 2.
dmusic:audiopath.c:180:IDirectMusicAudioPath_GetObjectInPath iface 06F080B8, ppObject 06F08440, ret 0.
dmusic:graph.c:111:DirectMusicGraph_InsertTool iface 06F08440, pTool 0C57F6DC, pdwPChannels 00000000, cPChannels 0, lIndex 0 stub!
dmusic:graph.c:145:DirectMusicGraph_InsertTool iface 06F08440, pTool 0C57F6DC, pdwPChannels 0, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 2, fActivate 1, ppNewPath 04A33424 stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 06E588A8, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06E588A8, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 04A33428 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06E588A8, ppObject 0211FC10, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06E588A8, dwPChannel 0, dwStage 8704, dwBuffer 0, guidObject {00000000-0000-0000-0000-000000000000}, dwIndex 0, iidInterface {2befc277-5497-11d2-bccb-00a0c922e6eb}, ppObject 01AFD728 stub!
dmusic:graph.c:259:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:dmobject.c:757:dmobject_init dmobj 06E58C38, class {d2ac2884-b39b-11d1-8704-00600893b1bd}, outer_unk 06E58C30.
dmusic:dmobject.c:763:dmobject_init dmobj 06E58C38, outer_unk 06E58C30.
dmusic:graph.c:47:DirectMusicGraph_QueryInterface iface 06E58C30, riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E58C30.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E58C30, ret 2.
dmusic:graph.c:64:DirectMusicGraph_QueryInterface iface 06E58C30, ret_iface 06E58C30, ret 0.
dmusic:graph.c:87:DirectMusicGraph_Release iface 06E58C30.
dmusic:graph.c:93:DirectMusicGraph_Release iface 06E58C30, ret 1.
dmusic:graph.c:277:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 06E58C30, ret 0.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E58C30.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E58C30, ret 2.
dmusic:audiopath.c:180:IDirectMusicAudioPath_GetObjectInPath iface 06E588A8, ppObject 06E58C30, ret 0.
dmusic:graph.c:111:DirectMusicGraph_InsertTool iface 06E58C30, pTool 0C57F6FC, pdwPChannels 00000000, cPChannels 0, lIndex 0 stub!
dmusic:graph.c:145:DirectMusicGraph_InsertTool iface 06E58C30, pTool 0C57F6FC, pdwPChannels 0, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 2, fActivate 1, ppNewPath 04AED12C stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 06ECACD8, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06ECACD8, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 04AED130 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06ECACD8, ppObject 02073858, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06ECACD8, dwPChannel 0, dwStage 8704, dwBuffer 0, guidObject {00000000-0000-0000-0000-000000000000}, dwIndex 0, iidInterface {2befc277-5497-11d2-bccb-00a0c922e6eb}, ppObject 01AFD728 stub!
dmusic:graph.c:259:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:dmobject.c:757:dmobject_init dmobj 06E596A0, class {d2ac2884-b39b-11d1-8704-00600893b1bd}, outer_unk 06E59698.
dmusic:dmobject.c:763:dmobject_init dmobj 06E596A0, outer_unk 06E59698.
dmusic:graph.c:47:DirectMusicGraph_QueryInterface iface 06E59698, riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E59698.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E59698, ret 2.
dmusic:graph.c:64:DirectMusicGraph_QueryInterface iface 06E59698, ret_iface 06E59698, ret 0.
dmusic:graph.c:87:DirectMusicGraph_Release iface 06E59698.
dmusic:graph.c:93:DirectMusicGraph_Release iface 06E59698, ret 1.
dmusic:graph.c:277:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 06E59698, ret 0.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E59698.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E59698, ret 2.
dmusic:audiopath.c:180:IDirectMusicAudioPath_GetObjectInPath iface 06ECACD8, ppObject 06E59698, ret 0.
dmusic:graph.c:111:DirectMusicGraph_InsertTool iface 06E59698, pTool 0C57F71C, pdwPChannels 00000000, cPChannels 0, lIndex 0 stub!
dmusic:graph.c:145:DirectMusicGraph_InsertTool iface 06E59698, pTool 0C57F71C, pdwPChannels 0, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 2, fActivate 1, ppNewPath 04AED15C stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 06ECB078, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06ECB078, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 04AED160 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06ECB078, ppObject 06F14690, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06ECB078, dwPChannel 0, dwStage 8704, dwBuffer 0, guidObject {00000000-0000-0000-0000-000000000000}, dwIndex 0, iidInterface {2befc277-5497-11d2-bccb-00a0c922e6eb}, ppObject 01AFD728 stub!
dmusic:graph.c:259:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:dmobject.c:757:dmobject_init dmobj 06E5A108, class {d2ac2884-b39b-11d1-8704-00600893b1bd}, outer_unk 06E5A100.
dmusic:dmobject.c:763:dmobject_init dmobj 06E5A108, outer_unk 06E5A100.
dmusic:graph.c:47:DirectMusicGraph_QueryInterface iface 06E5A100, riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E5A100.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E5A100, ret 2.
dmusic:graph.c:64:DirectMusicGraph_QueryInterface iface 06E5A100, ret_iface 06E5A100, ret 0.
dmusic:graph.c:87:DirectMusicGraph_Release iface 06E5A100.
dmusic:graph.c:93:DirectMusicGraph_Release iface 06E5A100, ret 1.
dmusic:graph.c:277:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 06E5A100, ret 0.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E5A100.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E5A100, ret 2.
dmusic:audiopath.c:180:IDirectMusicAudioPath_GetObjectInPath iface 06ECB078, ppObject 06E5A100, ret 0.
dmusic:graph.c:111:DirectMusicGraph_InsertTool iface 06E5A100, pTool 0C57F73C, pdwPChannels 00000000, cPChannels 0, lIndex 0 stub!
dmusic:graph.c:145:DirectMusicGraph_InsertTool iface 06E5A100, pTool 0C57F73C, pdwPChannels 0, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 2, fActivate 1, ppNewPath 0C7E8844 stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 06ECB418, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06ECB418, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 0C7E8848 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06ECB418, ppObject 06E5A480, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06ECB418, dwPChannel 0, dwStage 8704, dwBuffer 0, guidObject {00000000-0000-0000-0000-000000000000}, dwIndex 0, iidInterface {2befc277-5497-11d2-bccb-00a0c922e6eb}, ppObject 01AFD728 stub!
dmusic:graph.c:259:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:dmobject.c:757:dmobject_init dmobj 06E5ACC0, class {d2ac2884-b39b-11d1-8704-00600893b1bd}, outer_unk 06E5ACB8.
dmusic:dmobject.c:763:dmobject_init dmobj 06E5ACC0, outer_unk 06E5ACB8.
dmusic:graph.c:47:DirectMusicGraph_QueryInterface iface 06E5ACB8, riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E5ACB8.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E5ACB8, ret 2.
dmusic:graph.c:64:DirectMusicGraph_QueryInterface iface 06E5ACB8, ret_iface 06E5ACB8, ret 0.
dmusic:graph.c:87:DirectMusicGraph_Release iface 06E5ACB8.
dmusic:graph.c:93:DirectMusicGraph_Release iface 06E5ACB8, ret 1.
dmusic:graph.c:277:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 06E5ACB8, ret 0.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06E5ACB8.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06E5ACB8, ret 2.
dmusic:audiopath.c:180:IDirectMusicAudioPath_GetObjectInPath iface 06ECB418, ppObject 06E5ACB8, ret 0.
dmusic:graph.c:111:DirectMusicGraph_InsertTool iface 06E5ACB8, pTool 0C57F75C, pdwPChannels 00000000, cPChannels 0, lIndex 0 stub!
dmusic:graph.c:145:DirectMusicGraph_InsertTool iface 06E5ACB8, pTool 0C57F75C, pdwPChannels 0, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 2, fActivate 1, ppNewPath 0C7E8874 stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 06ECB7B8, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06ECB7B8, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 0C7E8878 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06ECB7B8, ppObject 06E5B1B8, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06ECB7B8, dwPChannel 0, dwStage 8704, dwBuffer 0, guidObject {00000000-0000-0000-0000-000000000000}, dwIndex 0, iidInterface {2befc277-5497-11d2-bccb-00a0c922e6eb}, ppObject 01AFD728 stub!
dmusic:graph.c:259:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:dmobject.c:757:dmobject_init dmobj 06ED1D58, class {d2ac2884-b39b-11d1-8704-00600893b1bd}, outer_unk 06ED1D50.
dmusic:dmobject.c:763:dmobject_init dmobj 06ED1D58, outer_unk 06ED1D50.
dmusic:graph.c:47:DirectMusicGraph_QueryInterface iface 06ED1D50, riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06ED1D50.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06ED1D50, ret 2.
dmusic:graph.c:64:DirectMusicGraph_QueryInterface iface 06ED1D50, ret_iface 06ED1D50, ret 0.
dmusic:graph.c:87:DirectMusicGraph_Release iface 06ED1D50.
dmusic:graph.c:93:DirectMusicGraph_Release iface 06ED1D50, ret 1.
dmusic:graph.c:277:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 06ED1D50, ret 0.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06ED1D50.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06ED1D50, ret 2.
dmusic:audiopath.c:180:IDirectMusicAudioPath_GetObjectInPath iface 06ECB7B8, ppObject 06ED1D50, ret 0.
dmusic:graph.c:111:DirectMusicGraph_InsertTool iface 06ED1D50, pTool 0C57F77C, pdwPChannels 00000000, cPChannels 0, lIndex 0 stub!
dmusic:graph.c:145:DirectMusicGraph_InsertTool iface 06ED1D50, pTool 0C57F77C, pdwPChannels 0, ret 0.
dmusic:performance.c:1134:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, dwType 8, pchannel_count 2, fActivate 1, ppNewPath 0C7E88A4 stub!
dmusic:performance.c:1215:IDirectMusicPerformance8_CreateStandardAudioPath iface performance, ppNewPath 06ECBB58, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06ECBB58, dwPChannel 4294967291, dwStage 24576, dwBuffer 0, guidObject {aa114de5-c262-4169-a1c8-23d698cc73b5}, dwIndex 0, iidInterface {279afa85-4981-11ce-a521-0020af0be560}, ppObject 0C7E88A8 stub!
dmusic:audiopath.c:149:IDirectMusicAudioPath_GetObjectInPath iface 06ECBB58, ppObject 06E5B488, ret 0.
dmusic:audiopath.c:138:IDirectMusicAudioPath_GetObjectInPath iface 06ECBB58, dwPChannel 0, dwStage 8704, dwBuffer 0, guidObject {00000000-0000-0000-0000-000000000000}, dwIndex 0, iidInterface {2befc277-5497-11d2-bccb-00a0c922e6eb}, ppObject 01AFD728 stub!
dmusic:graph.c:259:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:dmobject.c:757:dmobject_init dmobj 06ED20D8, class {d2ac2884-b39b-11d1-8704-00600893b1bd}, outer_unk 06ED20D0.
dmusic:dmobject.c:763:dmobject_init dmobj 06ED20D8, outer_unk 06ED20D0.
dmusic:graph.c:47:DirectMusicGraph_QueryInterface iface 06ED20D0, riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 01AFD6CC.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06ED20D0.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06ED20D0, ret 2.
dmusic:graph.c:64:DirectMusicGraph_QueryInterface iface 06ED20D0, ret_iface 06ED20D0, ret 0.
dmusic:graph.c:87:DirectMusicGraph_Release iface 06ED20D0.
dmusic:graph.c:93:DirectMusicGraph_Release iface 06ED20D0, ret 1.
dmusic:graph.c:277:create_dmgraph riid {2befc277-5497-11d2-bccb-00a0c922e6eb}, ret_iface 06ED20D0, ret 0.
dmusic:graph.c:76:DirectMusicGraph_AddRef iface 06ED20D0.
dmusic:graph.c:79:DirectMusicGraph_AddRef iface 06ED20D0, ret 2.
dmusic:audiopath.c:180:IDirectMusicAudioPath_GetObjectInPath iface 06ECBB58, ppObject 06ED20D0, ret 0.
dmusic:graph.c:111:DirectMusicGraph_InsertTool iface 06ED20D0, pTool 0C43891C, pdwPChannels 00000000, cPChannels 0, lIndex 0 stub!
dmusic:graph.c:145:DirectMusicGraph_InsertTool iface 06ED20D0, pTool 0C43891C, pdwPChannels 0, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"sounds\\tracks\\ArcDeTriomphe.wav", ppObject 128D1470.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Rise of Nations\\sounds\\tracks\\ArcDeTriomphe.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"sounds\\tracks\\ArcDeTriomphe.wav", ppObject 06D63630, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"sounds\\tracks\\Waterloo.wav", ppObject 128D1500.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Rise of Nations\\sounds\\tracks\\Waterloo.wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"sounds\\tracks\\Waterloo.wav", ppObject 06D63A10, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"sounds\\tracks\\TheHague(ruffmix2).wav", ppObject 128D2310.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Rise of Nations\\sounds\\tracks\\TheHague(ruffmix2).wav"
dmusic:loader.c:869:IDirectMusicLoader_LoadObjectFromFile iface loader, pwzFilePath L"sounds\\tracks\\TheHague(ruffmix2).wav", ppObject 06D63DF0, ret 0.
dmusic:loader.c:760:IDirectMusicLoader_EnableCache iface loader, class &CLSID_DirectMusicSegment, enable 0.
dmusic:loader.c:739:IDirectMusicLoader_ClearCache iface loader, class &CLSID_DirectMusicSegment.
dmusic:loader.c:751:IDirectMusicLoader_ClearCache iface loader, ret 0.
dmusic:loader.c:781:IDirectMusicLoader_EnableCache iface loader, ret 0.
dmusic:loader.c:268:IDirectMusicLoader_GetObject iface loader, pDesc 01AFD4F4, riid &IID_IDirectMusicSegment8, ppv 13DC13F0.
dmusic:loader.c:450:IDirectMusicLoader_GetObject iface loader, pDesc 01AFD4F4, ppv 06D641D0, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 06D641D0, pAudioPath performance.
dmusic:performance.c:270:IDirectMusicPerformance8_QueryInterface iface performance, riid &IID_IDirectMusicPerformance8, ppv 01AFD84C.
dmusic:performance.c:290:IDirectMusicPerformance8_AddRef iface performance.
dmusic:performance.c:294:IDirectMusicPerformance8_AddRef iface performance, ret 2.
dmusic:performance.c:278:IDirectMusicPerformance8_QueryInterface iface performance, ppv performance, ret 0.
dmusic:performance.c:260:get_dsound_interface iface performance.
dmusic:performance.c:262:get_dsound_interface iface performance, ret 02106BE4.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 06D641D0, pAudioPath performance, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFD898, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 162960000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D641D0, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D641D0, riid &IID_IDirectMusicSegment8, ret_iface 01AFD85C.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D641D0.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D641D0, ret 2.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D641D0, ret_iface 06D641D0, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D641D0.
dmusic:segment.c:55:get_segment_buffer iface 06D641D0, ret 06E86B30.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D641D0, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0, ret 0.
dmusic:loader.c:760:IDirectMusicLoader_EnableCache iface loader, class &CLSID_DirectMusicSegment, enable 0.
dmusic:loader.c:739:IDirectMusicLoader_ClearCache iface loader, class &CLSID_DirectMusicSegment.
dmusic:loader.c:751:IDirectMusicLoader_ClearCache iface loader, ret 0.
dmusic:loader.c:779:IDirectMusicLoader_EnableCache iface loader, ret 0x1.
dmusic:loader.c:268:IDirectMusicLoader_GetObject iface loader, pDesc 01AFD1A4, riid &IID_IDirectMusicSegment8, ppv 13DC14D0.
dmusic:loader.c:450:IDirectMusicLoader_GetObject iface loader, pDesc 01AFD1A4, ppv 06D645B0, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 06D645B0, pAudioPath performance.
dmusic:performance.c:270:IDirectMusicPerformance8_QueryInterface iface performance, riid &IID_IDirectMusicPerformance8, ppv 01AFD4FC.
dmusic:performance.c:290:IDirectMusicPerformance8_AddRef iface performance.
dmusic:performance.c:294:IDirectMusicPerformance8_AddRef iface performance, ret 3.
dmusic:performance.c:278:IDirectMusicPerformance8_QueryInterface iface performance, ppv performance, ret 0.
dmusic:performance.c:260:get_dsound_interface iface performance.
dmusic:performance.c:262:get_dsound_interface iface performance, ret 02106BE4.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 06D645B0, pAudioPath performance, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFD548, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 172600000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 0 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D645B0, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D645B0, riid &IID_IDirectMusicSegment8, ret_iface 01AFD50C.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D645B0.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D645B0, ret 2.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D645B0, ret_iface 06D645B0, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D645B0.
dmusic:segment.c:55:get_segment_buffer iface 06D645B0, ret 06EDBD30.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D645B0, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 06D63DF0, pAudioPath performance.
dmusic:performance.c:270:IDirectMusicPerformance8_QueryInterface iface performance, riid &IID_IDirectMusicPerformance8, ppv 01AFF47C.
dmusic:performance.c:290:IDirectMusicPerformance8_AddRef iface performance.
dmusic:performance.c:294:IDirectMusicPerformance8_AddRef iface performance, ret 4.
dmusic:performance.c:278:IDirectMusicPerformance8_QueryInterface iface performance, ppv performance, ret 0.
dmusic:performance.c:260:get_dsound_interface iface performance.
dmusic:performance.c:262:get_dsound_interface iface performance, ret 02106BE4.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 06D63DF0, pAudioPath performance, ret 0.
dmusic:performance.c:479:IDirectMusicPerformance8_IsPlaying iface performance, pSegment 06D63DF0, pSegState 00000000 stub!
dmusic:performance.c:480:IDirectMusicPerformance8_IsPlaying iface performance, pSegment 06D63DF0, pSegState 00000000, ret 0x1.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFF4A8, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 176430000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06ECB418, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06ECB418, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06ECB418, lVolume -1099, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06ECB418, ret 0.
dmusic:segment.c:155:IDirectMusicSegment8_SetRepeats iface 06D63DF0, dwRepeats 4294967295.
dmusic:segment.c:157:IDirectMusicSegment8_SetRepeats iface 06D63DF0, ret 0.
dmusic:segment.c:486:IDirectMusicSegment8_SetStartPoint iface 06D63DF0, mtStart 0.
dmusic:segment.c:488:IDirectMusicSegment8_SetStartPoint iface 06D63DF0, ret 0x88781202.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 128D2364, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 176430000, pmtNow 0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D63DF0, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06ECB418 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D63DF0, riid &IID_IDirectMusicSegment8, ret_iface 01AFF46C.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D63DF0.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D63DF0, ret 3.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D63DF0, ret_iface 06D63DF0, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D63DF0.
dmusic:segment.c:55:get_segment_buffer iface 06D63DF0, ret 06EE83E8.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D63DF0, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06ECB418, ret 0.
635615.809842:01e8:1385878:01ec:1385878:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
dmusic:loader.c:760:IDirectMusicLoader_EnableCache iface loader, class &CLSID_DirectMusicSegment, enable 0.
dmusic:loader.c:739:IDirectMusicLoader_ClearCache iface loader, class &CLSID_DirectMusicSegment.
dmusic:loader.c:751:IDirectMusicLoader_ClearCache iface loader, ret 0.
dmusic:loader.c:779:IDirectMusicLoader_EnableCache iface loader, ret 0x1.
dmusic:loader.c:268:IDirectMusicLoader_GetObject iface loader, pDesc 01AFEF78, riid &IID_IDirectMusicSegment8, ppv 13DC1770.
dmusic:loader.c:450:IDirectMusicLoader_GetObject iface loader, pDesc 01AFEF78, ppv 06D64990, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 06D64990, pAudioPath performance.
dmusic:performance.c:270:IDirectMusicPerformance8_QueryInterface iface performance, riid &IID_IDirectMusicPerformance8, ppv 01AFF2CC.
dmusic:performance.c:290:IDirectMusicPerformance8_AddRef iface performance.
dmusic:performance.c:294:IDirectMusicPerformance8_AddRef iface performance, ret 5.
dmusic:performance.c:278:IDirectMusicPerformance8_QueryInterface iface performance, ppv performance, ret 0.
dmusic:performance.c:260:get_dsound_interface iface performance.
dmusic:performance.c:262:get_dsound_interface iface performance, ret 02106BE4.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 06D64990, pAudioPath performance, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFF320, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 179280000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 0 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D64990, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D64990, riid &IID_IDirectMusicSegment8, ret_iface 01AFF2DC.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D64990.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D64990, ret 2.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D64990, ret_iface 06D64990, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D64990.
dmusic:segment.c:55:get_segment_buffer iface 06D64990, ret 06EE8538.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D64990, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0, ret 0.
635616.661100:01e8:1385878:0474:1386183:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
635616.661690:01e8:1385878:0474:1386183:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
635616.837622:01e8:1385878:01f8:1385900:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
635617.879452:01e8:1385878:047c:1386185:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
635618.052040:01e8:1385878:0490:1386203:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
635618.055357:01e8:1385878:0490:1386203:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
635618.055582:01e8:1385878:0474:1386183:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
635618.226537:01e8:1385878:0490:1386203:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
dmusic:loader.c:760:IDirectMusicLoader_EnableCache iface loader, class &CLSID_DirectMusicSegment, enable 0.
dmusic:loader.c:739:IDirectMusicLoader_ClearCache iface loader, class &CLSID_DirectMusicSegment.
dmusic:loader.c:751:IDirectMusicLoader_ClearCache iface loader, ret 0.
dmusic:loader.c:779:IDirectMusicLoader_EnableCache iface loader, ret 0x1.
dmusic:loader.c:268:IDirectMusicLoader_GetObject iface loader, pDesc 01AFEB70, riid &IID_IDirectMusicSegment8, ppv 13DC1690.
dmusic:loader.c:450:IDirectMusicLoader_GetObject iface loader, pDesc 01AFEB70, ppv 06D65530, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 06D65530, pAudioPath performance.
dmusic:performance.c:270:IDirectMusicPerformance8_QueryInterface iface performance, riid &IID_IDirectMusicPerformance8, ppv 01AFEECC.
dmusic:performance.c:290:IDirectMusicPerformance8_AddRef iface performance.
dmusic:performance.c:294:IDirectMusicPerformance8_AddRef iface performance, ret 6.
dmusic:performance.c:278:IDirectMusicPerformance8_QueryInterface iface performance, ppv performance, ret 0.
dmusic:performance.c:260:get_dsound_interface iface performance.
dmusic:performance.c:262:get_dsound_interface iface performance, ret 02106BE4.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 06D65530, pAudioPath performance, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFEF18, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 204440000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 0 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D65530, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D65530, riid &IID_IDirectMusicSegment8, ret_iface 01AFEECC.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D65530.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D65530, ret 2.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D65530, ret_iface 06D65530, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D65530.
dmusic:segment.c:55:get_segment_buffer iface 06D65530, ret 06FC7AF0.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D65530, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0, ret 0.
dmusic:loader.c:760:IDirectMusicLoader_EnableCache iface loader, class &CLSID_DirectMusicSegment, enable 0.
dmusic:loader.c:739:IDirectMusicLoader_ClearCache iface loader, class &CLSID_DirectMusicSegment.
dmusic:loader.c:751:IDirectMusicLoader_ClearCache iface loader, ret 0.
dmusic:loader.c:779:IDirectMusicLoader_EnableCache iface loader, ret 0x1.
dmusic:loader.c:268:IDirectMusicLoader_GetObject iface loader, pDesc 01AFEF78, riid &IID_IDirectMusicSegment8, ppv 13DC1850.
dmusic:loader.c:450:IDirectMusicLoader_GetObject iface loader, pDesc 01AFEF78, ppv 06D65910, ret 0.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 06D65910, pAudioPath performance.
dmusic:performance.c:270:IDirectMusicPerformance8_QueryInterface iface performance, riid &IID_IDirectMusicPerformance8, ppv 01AFF2CC.
dmusic:performance.c:290:IDirectMusicPerformance8_AddRef iface performance.
dmusic:performance.c:294:IDirectMusicPerformance8_AddRef iface performance, ret 7.
dmusic:performance.c:278:IDirectMusicPerformance8_QueryInterface iface performance, ppv performance, ret 0.
dmusic:performance.c:260:get_dsound_interface iface performance.
dmusic:performance.c:262:get_dsound_interface iface performance, ret 02106BE4.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 06D65910, pAudioPath performance, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFF320, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 204560000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 0 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D65910, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D65910, riid &IID_IDirectMusicSegment8, ret_iface 01AFF2DC.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D65910.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D65910, ret 2.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D65910, ret_iface 06D65910, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D65910.
dmusic:segment.c:55:get_segment_buffer iface 06D65910, ret 06FC7C40.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D65910, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AF9F98, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 208270000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 0 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D64990, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D64990, riid &IID_IDirectMusicSegment8, ret_iface 01AF9F4C.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D64990.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D64990, ret 3.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D64990, ret_iface 06D64990, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D64990.
dmusic:segment.c:55:get_segment_buffer iface 06D64990, ret 06EE8538.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D64990, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0, ret 0.
635620.017321:01e8:1385878:01f8:1385900:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AF9B88, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 222050000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 0 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D65530, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D65530, riid &IID_IDirectMusicSegment8, ret_iface 01AF9B3C.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D65530.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D65530, ret 3.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D65530, ret_iface 06D65530, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D65530.
dmusic:segment.c:55:get_segment_buffer iface 06D65530, ret 06FC7AF0.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D65530, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AF9F98, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 222060000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 0 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D65910, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D65910, riid &IID_IDirectMusicSegment8, ret_iface 01AF9F4C.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D65910.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D65910, ret 3.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D65910, ret_iface 06D65910, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D65910.
dmusic:segment.c:55:get_segment_buffer iface 06D65910, ret 06FC7C40.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D65910, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFDA00, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 226200000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 0 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D641D0, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D641D0, riid &IID_IDirectMusicSegment8, ret_iface 01AFD9BC.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D641D0.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D641D0, ret 3.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D641D0, ret_iface 06D641D0, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D641D0.
dmusic:segment.c:55:get_segment_buffer iface 06D641D0, ret 06E86B30.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D641D0, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFD6A8, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 234060000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 0 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06EB0DB0, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D645B0, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D645B0, riid &IID_IDirectMusicSegment8, ret_iface 01AFD66C.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D645B0.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D645B0, ret 3.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D645B0, ret_iface 06D645B0, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D645B0.
dmusic:segment.c:55:get_segment_buffer iface 06D645B0, ret 06EDBD30.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D645B0, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06EB0DB0, ret 0.
dmusic:loader.c:846:IDirectMusicLoader_LoadObjectFromFile iface loader, rguidClassID &CLSID_DirectMusicSegment, iidInterfaceID &IID_IDirectMusicSegment8, pwzFilePath L"scenario\\Tutorial5\\tut5_intro.wav", ppObject 12EC9560.
dmusic:loader.c:867:IDirectMusicLoader_LoadObjectFromFile : full file path = L"Z:\\home\\rbernon\\.steam\\debian-installation\\steamapps\\common\\Rise of Nations\\scenario\\Tutorial5\\tut5_intro.wav"
dmusic:performance.c:480:IDirectMusicPerformance8_IsPlaying iface performance, pSegment 06D65CF0, pSegState 00000000, ret 0x1.
dmusic:segment.c:587:IDirectMusicSegment8_Download iface 06D65CF0, pAudioPath performance.
dmusic:performance.c:270:IDirectMusicPerformance8_QueryInterface iface performance, riid &IID_IDirectMusicPerformance8, ppv 01AFE84C.
dmusic:performance.c:290:IDirectMusicPerformance8_AddRef iface performance.
dmusic:performance.c:294:IDirectMusicPerformance8_AddRef iface performance, ret 8.
dmusic:performance.c:278:IDirectMusicPerformance8_QueryInterface iface performance, ppv performance, ret 0.
dmusic:performance.c:260:get_dsound_interface iface performance.
dmusic:performance.c:262:get_dsound_interface iface performance, ret 02106BE4.
dmusic:segment.c:644:IDirectMusicSegment8_Download CreateSoundBuffer successful
dmusic:segment.c:647:IDirectMusicSegment8_Download IDirectSoundBuffer_Lock hr 0x00000000
dmusic:segment.c:652:IDirectMusicSegment8_Download IDirectSoundBuffer_Unlock hr 0x00000000
dmusic:segment.c:657:IDirectMusicSegment8_Download iface 06D65CF0, pAudioPath performance, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06ECB418, lVolume -1648, dwDuration 500 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06ECB418, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, lVolume -1373, dwDuration 500 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06EB0DB0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFE888, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 239790000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06ECACD8, i64StopTime 0, dwFlags 64 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06ECACD8, ret 0.
dmusic:audiopath.c:250:IDirectMusicAudioPath_SetVolume iface 06ECACD8, lVolume -550, dwDuration 0 stub!
dmusic:audiopath.c:251:IDirectMusicAudioPath_SetVolume iface 06ECACD8, ret 0.
dmusic:segment.c:486:IDirectMusicSegment8_SetStartPoint iface 06D65CF0, mtStart 0.
dmusic:segment.c:488:IDirectMusicSegment8_SetStartPoint iface 06D65CF0, ret 0x88781202.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 12EC95B4, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 239790000, pmtNow 0, ret 0.
dmusic:performance.c:1065:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D65CF0, pwzSegmentName (null), pTransition 00000000, dwFlags 128, i64StartTime 0, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06ECACD8 stub!
dmusic:segment.c:63:IDirectMusicSegment8_QueryInterface iface 06D65CF0, riid &IID_IDirectMusicSegment8, ret_iface 01AFE84C.
dmusic:segment.c:89:IDirectMusicSegment8_AddRef iface 06D65CF0.
dmusic:segment.c:91:IDirectMusicSegment8_AddRef iface 06D65CF0, ret 2.
dmusic:segment.c:81:IDirectMusicSegment8_QueryInterface iface 06D65CF0, ret_iface 06D65CF0, ret 0.
dmusic:segment.c:53:get_segment_buffer iface 06D65CF0.
dmusic:segment.c:55:get_segment_buffer iface 06D65CF0, ret 06FC7D90.
dmusic:performance.c:1079:IDirectMusicPerformance8_PlaySegmentEx iface performance, pSource 06D65CF0, pwzSegmentName (null), pTransition 00000000, ppSegmentState 00000000, pFrom 00000000, pAudioPath 06ECACD8, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFE2C8, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 249700000, pmtNow 0, ret 0.
dmusic:performance.c:1087:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06ECACD8, i64StopTime 0, dwFlags 0 stub!
dmusic:performance.c:1089:IDirectMusicPerformance8_StopEx iface performance, pObjectToStop 06ECACD8, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFE2C8, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 249750000, pmtNow 0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFE2C8, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 249750000, pmtNow 0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFE368, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 249750000, pmtNow 0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFE230, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 249890000, pmtNow 0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFE230, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 249890000, pmtNow 0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFE230, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 249890000, pmtNow 0, ret 0.
dmusic:performance.c:491:IDirectMusicPerformance8_GetTime iface performance, prtNow 01AFE2D0, pmtNow 00000000.
dmusic:performance.c:504:IDirectMusicPerformance8_GetTime iface performance, prtNow 249890000, pmtNow 0, ret 0.
635630.048437:01e8:1385878:047c:1386185:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
635640.075321:01e8:1385878:0230:1385937:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
635650.093714:01e8:1385878:0490:1386203:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"
635660.101520:01e8:1385878:0490:1386203:000000462dee:warn:debugstr:debugger.c:57:OutputDebugStringA "\n"

    hr = IDirectMusicPerformance8_CloseDown(performance);
    ok(hr == S_OK, "got %#lx\n", hr);
    IDirectMusicPerformance8_Release(performance);
#endif
}

static void test_ffviii_init(void)
{
    DMUS_PORTPARAMS params =
    {
        .dwSize = sizeof(params),
        .dwValidParams = DMUS_PORTPARAMS_EFFECTS,
        .dwEffectFlags = 1,
    };
    DSBUFFERDESC buffer_desc0 =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D,
    };
    WAVEFORMATEX buffer_format1 =
    {
    };
    DSBUFFERDESC buffer_desc1 =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_LOCHARDWARE | DSBCAPS_CTRL3D | DSBCAPS_GLOBALFOCUS,
        .dwBufferBytes = 32,
        .lpwfxFormat = &buffer_format1,
    };
    DSBUFFERDESC buffer_desc2 =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_PRIMARYBUFFER,
    };
    WAVEFORMATEX buffer_format3 =
    {
        .wFormatTag = 1,
        .nChannels = 2,
        .nSamplesPerSec = 22050,
        .nAvgBytesPerSec = 88200,
        .nBlockAlign = 4,
        .wBitsPerSample = 16,
    };
    DSBUFFERDESC buffer_desc3 =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2,
        .dwBufferBytes = 176400,
        .lpwfxFormat = &buffer_format3,
    };
    WAVEFORMATEX buffer_format4 =
    {
        .wFormatTag = 1,
        .nChannels = 2,
        .nSamplesPerSec = 48000,
        .nAvgBytesPerSec = 192000,
        .nBlockAlign = 4,
        .wBitsPerSample = 16,
    };
    DSBUFFERDESC buffer_desc4 =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_CTRLVOLUME,
        .dwBufferBytes = 960000,
        .lpwfxFormat = &buffer_format4,
    };
    WAVEFORMATEX waveformat =
    {
        .wFormatTag = 1,
        .nChannels = 2,
        .nSamplesPerSec = 44100,
        .nAvgBytesPerSec = 174600,
        .nBlockAlign = 4,
        .wBitsPerSample = 16,
    };
    DMUS_OBJECTDESC collection_desc =
    {
        .dwSize = sizeof(DMUS_OBJECTDESC),
        .dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME,
        .guidClass = CLSID_DirectMusicCollection,
        .wszFileName = L"ff8.dls",
    };
    DMUS_OBJECTDESC segment_desc =
    {
        .dwSize = sizeof(DMUS_OBJECTDESC),
        .dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME,
        .guidClass = CLSID_DirectMusicSegment,
        .wszFileName = L"079s-demo.sgt",
    };
    DMUS_OBJECTDESC segment2_desc =
    {
        .dwSize = sizeof(DMUS_OBJECTDESC),
        .dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME,
        .guidClass = CLSID_DirectMusicSegment,
        .wszFileName = L"041s-field.sgt",
    };
    DMUS_OBJECTDESC segment3_desc =
    {
        .dwSize = sizeof(DMUS_OBJECTDESC),
        .dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME,
        .guidClass = CLSID_DirectMusicSegment,
        .wszFileName = L"026s-garden.sgt",
    };
    struct dsound_wrapper wrapper = {{&dsound_wrapper_vtbl}};
    IDirectMusicPerformance *performance;
    IDirectMusicCollection *collection;
    IDirectMusicSegment *segment;
    IDirectSoundBuffer *dsbuffer;
    IDirectMusicLoader *loader;
    IDirectMusicPort *port;
    IDirectSound8 *dsound;
    IDirectMusic *dmusic;
    WCHAR path[MAX_PATH];
    DWORD value;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectSound8, (void **)&dsound);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSound8_Initialize(dsound, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSound8_CreateSoundBuffer(dsound, &buffer_desc0, &dsbuffer, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSound8_CreateSoundBuffer(dsound, &buffer_desc1, &dsbuffer, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSound8_SetCooperativeLevel(dsound, GetDesktopWindow(), DSSCL_PRIORITY);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectSound8_CreateSoundBuffer(dsound, &buffer_desc2, &dsbuffer, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSoundBuffer8_SetFormat(dsbuffer, &waveformat);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSoundBuffer8_Play(dsbuffer, 0, 0, 1);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = CoCreateInstance(&CLSID_DirectMusic, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusic, (void **)&dmusic);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = CoCreateInstance(&CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader, (void **)&loader);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&performance);
    ok(hr == S_OK, "got %#lx\n", hr);

    wrapper.dsound8 = dsound;
    hr = IDirectMusic_SetDirectSound(dmusic, &wrapper.IDirectSound_iface, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusic_CreatePort(dmusic, &CLSID_DirectMusicSynth, &params, &port, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectSound8_CreateSoundBuffer(dsound, &buffer_desc3, &dsbuffer, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectMusicPort_Activate(port, 1);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPort_SetNumChannelGroups(port, 1);
    ok(hr == S_OK, "got %#lx\n", hr);

    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/FINAL FANTASY VIII/Data/Music/dmusic/");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/FINAL FANTASY VIII/Data/Music/dmusic/");
    hr = IDirectMusicLoader_SetSearchDirectory(loader, &GUID_DirectMusicAllTypes, path, FALSE);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance_Init(performance, &dmusic, NULL, 0);
    ok(hr == S_OK, "got %#lx\n", hr);

    value = 1;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfAutoDownload, &value, sizeof(value));
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_AddPort(performance, port);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_AssignPChannelBlock(performance, 0, port, 1);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectSound8_CreateSoundBuffer(dsound, &buffer_desc4, &dsbuffer, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);

    value = -10000;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &value, sizeof(value));
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance_Stop(performance, NULL, NULL, 0, 0);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectMusicLoader_GetObject(loader, &collection_desc, &IID_IDirectMusicCollection, (void **)&collection);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicLoader_GetObject(loader, &segment_desc, &IID_IDirectMusicSegment, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetParam(segment, &GUID_ConnectToDLSCollection, -1, 0, 0, collection);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectMusicPerformance8_IsPlaying(performance, segment, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    value = -10000;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &value, sizeof(value));
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetStartPoint(segment, 0);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_PlaySegment(performance, segment, 0x800, 0, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    value = 0;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &value, sizeof(value));
    ok(hr == S_OK, "got %#lx\n", hr);

    Sleep(30000);

    hr = IDirectMusicPerformance8_Stop(performance, NULL, NULL, 0, 0x4000);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectMusicLoader_GetObject(loader, &segment2_desc, &IID_IDirectMusicSegment, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetParam(segment, &GUID_ConnectToDLSCollection, -1, 0, 0, collection);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectMusicPerformance8_IsPlaying(performance, segment, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetStartPoint(segment, 0);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_PlaySegment(performance, segment, 0x800, 0, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    value = 0;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &value, sizeof(value));
    ok(hr == S_OK, "got %#lx\n", hr);

    Sleep(10000);

    hr = IDirectMusicPerformance8_Stop(performance, NULL, NULL, 0, 0x4000);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectMusicLoader_GetObject(loader, &segment3_desc, &IID_IDirectMusicSegment, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetParam(segment, &GUID_ConnectToDLSCollection, -1, 0, 0, collection);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectMusicPerformance8_IsPlaying(performance, segment, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicSegment8_SetStartPoint(segment, 0);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_PlaySegment(performance, segment, 0x800, 0, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    value = 0;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &value, sizeof(value));
    ok(hr == S_OK, "got %#lx\n", hr);

    Sleep(10000);


    hr = IDirectMusicPerformance8_CloseDown(performance);
    ok(hr == S_OK, "got %#lx\n", hr);
    IDirectMusicPerformance8_Release(performance);
}


static void test_gothic(void)
{
    DMUS_PORTPARAMS params =
    {
        .dwSize = sizeof(params),
        .dwValidParams = DMUS_PORTPARAMS_EFFECTS,
        .dwEffectFlags = 1,
    };
    DSBUFFERDESC buffer_desc0 =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D,
    };
    WAVEFORMATEX buffer_format1 =
    {
    };
    DSBUFFERDESC buffer_desc1 =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_LOCHARDWARE | DSBCAPS_CTRL3D | DSBCAPS_GLOBALFOCUS,
        .dwBufferBytes = 32,
        .lpwfxFormat = &buffer_format1,
    };
    DSBUFFERDESC buffer_desc2 =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_PRIMARYBUFFER,
    };
    WAVEFORMATEX buffer_format3 =
    {
        .wFormatTag = 1,
        .nChannels = 2,
        .nSamplesPerSec = 22050,
        .nAvgBytesPerSec = 88200,
        .nBlockAlign = 4,
        .wBitsPerSample = 16,
    };
    DSBUFFERDESC buffer_desc3 =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2,
        .dwBufferBytes = 176400,
        .lpwfxFormat = &buffer_format3,
    };
    WAVEFORMATEX waveformat =
    {
        .wFormatTag = 1,
        .nChannels = 2,
        .nSamplesPerSec = 44100,
        .nAvgBytesPerSec = 174600,
        .nBlockAlign = 4,
        .wBitsPerSample = 16,
    };
    DMUS_OBJECTDESC segment_desc =
    {
        .dwSize = sizeof(DMUS_OBJECTDESC),
        .dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME,
        .guidClass = CLSID_DirectMusicSegment,
        .wszFileName = L"MENU.SGT",
    };
    DMUS_OBJECTDESC segment2_desc =
    {
        .dwSize = sizeof(DMUS_OBJECTDESC),
        .dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME,
        .guidClass = CLSID_DirectMusicSegment,
        .wszFileName = L"BAN_DAY_STD.SGT",
    };
    struct dsound_wrapper wrapper = {{&dsound_wrapper_vtbl}};
    IDirectMusicPerformance *performance;
    IDirectMusicSegment *segment;
    IDirectSoundBuffer *dsbuffer;
    IDirectMusicLoader *loader;
    IDirectMusicPort *port;
    IDirectSound8 *dsound;
    IDirectMusic *dmusic;
    WCHAR path[MAX_PATH];
    DWORD value;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectSound8, (void **)&dsound);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSound8_Initialize(dsound, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSound8_CreateSoundBuffer(dsound, &buffer_desc0, &dsbuffer, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSound8_CreateSoundBuffer(dsound, &buffer_desc1, &dsbuffer, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSound8_SetCooperativeLevel(dsound, GetDesktopWindow(), DSSCL_PRIORITY);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectSound8_CreateSoundBuffer(dsound, &buffer_desc2, &dsbuffer, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSoundBuffer8_SetFormat(dsbuffer, &waveformat);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectSoundBuffer8_Play(dsbuffer, 0, 0, 1);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = CoCreateInstance(&CLSID_DirectMusic, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusic, (void **)&dmusic);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = CoCreateInstance(&CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader, (void **)&loader);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&performance);
    ok(hr == S_OK, "got %#lx\n", hr);

    wrapper.dsound8 = dsound;
    hr = IDirectMusic_SetDirectSound(dmusic, &wrapper.IDirectSound_iface, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusic_CreatePort(dmusic, &CLSID_DirectMusicSynth, &params, &port, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectSound8_CreateSoundBuffer(dsound, &buffer_desc3, &dsbuffer, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);

    hr = IDirectMusicPort_Activate(port, 1);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPort_SetNumChannelGroups(port, 1);
    ok(hr == S_OK, "got %#lx\n", hr);


    hr = IDirectMusicLoader_EnableCache(loader, &GUID_DirectMusicAllTypes, 1);
    ok(hr == S_OK, "got %#lx\n", hr);

    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Gothic/_work/DATA/music/menu_men/");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Gothic/_work/DATA/music/menu_men/");
    hr = IDirectMusicLoader_SetSearchDirectory(loader, &GUID_DirectMusicAllTypes, path, FALSE);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance_Init(performance, &dmusic, NULL, 0);
    ok(hr == S_OK, "got %#lx\n", hr);


    value = 1;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfAutoDownload, &value, sizeof(value));
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_AddPort(performance, port);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_AssignPChannelBlock(performance, 0, port, 1);
    ok(hr == S_OK, "got %#lx\n", hr);

    value = -500;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &value, sizeof(value));
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance_Stop(performance, NULL, NULL, 0, 0);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicLoader_GetObject(loader, &segment_desc, &IID_IDirectMusicSegment, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_PlaySegment(performance, segment, 0x800, 0, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);

    Sleep(10000);

    hr = IDirectMusicPerformance8_Stop(performance, NULL, NULL, 0, 0x4000);
    ok(hr == S_OK, "got %#lx\n", hr);



    if (strcmp(winetest_platform, "wine")) wcscpy(path, L"Y:/Games/Gothic/_work/DATA/music/orchestra/");
    else wcscpy(path, L"Z:/media/rbernon/LaCie/Games/Gothic/_work/DATA/music/orchestra/");
    hr = IDirectMusicLoader_SetSearchDirectory(loader, &GUID_DirectMusicAllTypes, path, FALSE);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance_Init(performance, &dmusic, NULL, 0);
    ok(hr == S_OK, "got %#lx\n", hr);


    value = 1;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfAutoDownload, &value, sizeof(value));
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_AddPort(performance, port);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_AssignPChannelBlock(performance, 0, port, 1);
    ok(hr == S_OK, "got %#lx\n", hr);

    value = -500;
    hr = IDirectMusicPerformance8_SetGlobalParam(performance, &GUID_PerfMasterVolume, &value, sizeof(value));
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance_Stop(performance, NULL, NULL, 0, 0);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicLoader_GetObject(loader, &segment2_desc, &IID_IDirectMusicSegment, (void **)&segment);
    ok(hr == S_OK, "got %#lx\n", hr);
    hr = IDirectMusicPerformance8_PlaySegment(performance, segment, 0x800, 0, NULL);
    ok(hr == S_OK, "got %#lx\n", hr);

    Sleep(10000);

    hr = IDirectMusicPerformance8_Stop(performance, NULL, NULL, 0, 0x4000);
    ok(hr == S_OK, "got %#lx\n", hr);


    hr = IDirectMusicPerformance8_CloseDown(performance);
    ok(hr == S_OK, "got %#lx\n", hr);
    IDirectMusicPerformance8_Release(performance);
}

START_TEST(ffviii)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (0) test_load_collection();
    if (0) test_load();
    if (0) test_ffviii();

    if (0) test_blood_island();
    if (0) test_eroico();
    if (0) test_freefall();
    if (0) test_recettear();
    if (0) test_rise_of_nations();
    if (0) test_ffviii_init();
    test_gothic();

    CoUninitialize();
}
