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

#include "wine/test.h"

struct test_port
{
    IDirectMusicPort IDirectMusicPort_iface;
    LONG ref;

    IDirectMusicSynth8 *synth;
};

static struct test_port *impl_from_IDirectMusicPort(IDirectMusicPort *iface)
{
    return CONTAINING_RECORD(iface, struct test_port, IDirectMusicPort_iface);
}

static HRESULT WINAPI test_port_QueryInterface(IDirectMusicPort *iface, REFIID iid, void **out)
{
    if (IsEqualIID(iid, &IID_IUnknown)
            || IsEqualIID(iid, &IID_IDirectMusicPort))
    {
        IDirectMusicPort_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    ok(0,"unexpected %s, iid %s\n", __func__, debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI test_port_AddRef(IDirectMusicPort *iface)
{
    struct test_port *port = impl_from_IDirectMusicPort(iface);
    return InterlockedIncrement(&port->ref);
}

static ULONG WINAPI test_port_Release(IDirectMusicPort *iface)
{
    struct test_port *port = impl_from_IDirectMusicPort(iface);
    ULONG ref = InterlockedDecrement(&port->ref);

    if (!ref)
        free(port);

    return ref;
}

static HRESULT WINAPI test_port_PlayBuffer(IDirectMusicPort *iface, IDirectMusicBuffer *buffer)
{
    REFERENCE_TIME time;
    DWORD count, group;
    BYTE *data;

    ok(0, "unexpected %s\n", __func__);

    IDirectMusicBuffer_ResetReadPtr(buffer);

    while (IDirectMusicBuffer_GetNextEvent(buffer, &time, &group, &count, &data) == S_OK)
    {
        ok(0, "time %I64d group %lu count %lu data %p\n", time, group, count, data);
        do
        {
            const unsigned char *ptr = (void *)data, *end = ptr + count;
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
    }

    return S_OK;
}

static HRESULT WINAPI test_port_SetReadNotificationHandle(IDirectMusicPort *iface, HANDLE hEvent)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_Read(IDirectMusicPort *iface, LPDIRECTMUSICBUFFER pBuffer)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_DownloadInstrument(IDirectMusicPort *iface, IDirectMusicInstrument *instrument,
                                                   IDirectMusicDownloadedInstrument **downloaded,
                                                   DMUS_NOTERANGE *ranges, DWORD ranges_count)
{
    ok(0, "unexpected %s, instrument %p, downloaded %p, ranges %p, ranges_count %lu\n", __func__,
        instrument, downloaded, ranges, ranges_count);
    return S_OK;
}

static HRESULT WINAPI test_port_UnloadInstrument(IDirectMusicPort *iface, IDirectMusicDownloadedInstrument *pDownloadedInstrument)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_GetLatencyClock(IDirectMusicPort *iface, struct IReferenceClock **ppClock)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI test_port_GetRunningStats(IDirectMusicPort *iface, LPDMUS_SYNTHSTATS pStats)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_Compact(IDirectMusicPort *iface)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_GetCaps(IDirectMusicPort *iface, DMUS_PORTCAPS *caps)
{
    struct test_port *port = impl_from_IDirectMusicPort(iface);
    ok(0, "unexpected %s\n", __func__);
    return IDirectMusicSynth8_GetPortCaps(port->synth, caps);
}

static HRESULT WINAPI test_port_DeviceIoControl(IDirectMusicPort *iface, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_SetNumChannelGroups(IDirectMusicPort *iface, DWORD dwChannelGroups)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_GetNumChannelGroups(IDirectMusicPort *iface, DWORD *groups)
{
    ok(0, "unexpected %s\n", __func__);
    *groups = 1;
    return S_OK;
}

static HRESULT WINAPI test_port_Activate(IDirectMusicPort *iface, BOOL fActive)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_SetChannelPriority(IDirectMusicPort *iface, DWORD dwChannelGroup, DWORD dwChannel, DWORD dwPriority)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_GetChannelPriority(IDirectMusicPort *iface, DWORD dwChannelGroup, DWORD dwChannel, LPDWORD pdwPriority)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_SetDirectSound(IDirectMusicPort *iface, LPDIRECTSOUND pDirectSound, LPDIRECTSOUNDBUFFER pDirectSoundBuffer)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_port_GetFormat(IDirectMusicPort *iface, LPWAVEFORMATEX pWaveFormatEx, LPDWORD pdwWaveFormatExSize, LPDWORD pdwBufferSize)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static IDirectMusicPortVtbl test_port_vtbl =
{
    test_port_QueryInterface,
    test_port_AddRef,
    test_port_Release,
    test_port_PlayBuffer,
    test_port_SetReadNotificationHandle,
    test_port_Read,
    test_port_DownloadInstrument,
    test_port_UnloadInstrument,
    test_port_GetLatencyClock,
    test_port_GetRunningStats,
    test_port_Compact,
    test_port_GetCaps,
    test_port_DeviceIoControl,
    test_port_SetNumChannelGroups,
    test_port_GetNumChannelGroups,
    test_port_Activate,
    test_port_SetChannelPriority,
    test_port_GetChannelPriority,
    test_port_SetDirectSound,
    test_port_GetFormat,
};

static HRESULT test_port_create(IDirectMusicPort **out)
{
    struct test_port *port;

    *out = NULL;
    if (!(port = calloc(1, sizeof(*port)))) return E_OUTOFMEMORY;
    port->IDirectMusicPort_iface.lpVtbl = &test_port_vtbl;
    port->ref = 1;

    CoCreateInstance(&CLSID_DirectMusicSynth, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicSynth8, (void **)&port->synth);

    *out = &port->IDirectMusicPort_iface;
    return S_OK;
}
