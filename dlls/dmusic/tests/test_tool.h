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

#include "wine/test.h"

struct test_tool
{
    IDirectMusicTool8 IDirectMusicTool8_iface;
    LONG ref;
};

static struct test_tool *impl_from_IDirectMusicTool8(IDirectMusicTool8 *iface)
{
    return CONTAINING_RECORD(iface, struct test_tool, IDirectMusicTool8_iface);
}

static HRESULT WINAPI test_tool_QueryInterface(IDirectMusicTool8 *iface, REFIID iid, void **out)
{
    if (IsEqualIID(iid, &IID_IUnknown)
            || IsEqualIID(iid, &IID_IDirectMusicTool8))
    {
        IDirectMusicTool8_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    ok(0,"unexpected %s, iid %s\n", __func__, debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI test_tool_AddRef(IDirectMusicTool8 *iface)
{
    struct test_tool *tool = impl_from_IDirectMusicTool8(iface);
    return InterlockedIncrement(&tool->ref);
}

static ULONG WINAPI test_tool_Release(IDirectMusicTool8 *iface)
{
    struct test_tool *tool = impl_from_IDirectMusicTool8(iface);
    ULONG ref = InterlockedDecrement(&tool->ref);

    if (!ref)
        free(tool);

    return ref;
}

static HRESULT WINAPI test_tool_Init(IDirectMusicTool8 *iface, struct IDirectMusicGraph *pGraph)
{
    ok(0,"%#04lx: unexpected %s\n", GetCurrentThreadId(), __func__);
    return S_OK;
}

static HRESULT WINAPI test_tool_GetMsgDeliveryType(IDirectMusicTool8 *iface, DWORD *type)
{
    ok(0,"unexpected %s\n", __func__);
    *type = DMUS_PMSGF_TOOL_IMMEDIATE;
    return S_OK;
}

static HRESULT WINAPI test_tool_GetMediaTypeArraySize(IDirectMusicTool8 *iface, DWORD *size)
{
    ok(0,"unexpected %s, size %p\n", __func__, size);
    *size = 16;
    return S_OK;
}

static HRESULT WINAPI test_tool_GetMediaTypes(IDirectMusicTool8 *iface, DWORD **types, DWORD size)
{
    UINT i = 0;
    ok(0, "%#04lx: unexpected %s, types %p, size %lu\n", GetCurrentThreadId(), __func__, types, size);
    (*types)[i++] = DMUS_PMSGT_MIDI;
    (*types)[i++] = DMUS_PMSGT_NOTE;
    (*types)[i++] = DMUS_PMSGT_SYSEX;
    (*types)[i++] = DMUS_PMSGT_NOTIFICATION;
    (*types)[i++] = DMUS_PMSGT_TEMPO;
    (*types)[i++] = DMUS_PMSGT_CURVE;
    (*types)[i++] = DMUS_PMSGT_TIMESIG;
    (*types)[i++] = DMUS_PMSGT_PATCH;
    (*types)[i++] = DMUS_PMSGT_TRANSPOSE;
    (*types)[i++] = DMUS_PMSGT_CHANNEL_PRIORITY;
    (*types)[i++] = DMUS_PMSGT_STOP;
    (*types)[i++] = DMUS_PMSGT_DIRTY;
    (*types)[i++] = DMUS_PMSGT_WAVE;
    (*types)[i++] = DMUS_PMSGT_LYRIC;
    (*types)[i++] = DMUS_PMSGT_SCRIPTLYRIC;
    (*types)[i++] = DMUS_PMSGT_USER;
    return S_OK;
}

static const char *debugstr_dmus_pmsg(DMUS_PMSG *msg)
{
    char buffer[1024], *buf = buffer;

    buf += sprintf(buf, "sz %ld ", msg->dwSize);
    buf += sprintf(buf, "rtime %I64d ", msg->rtTime);
    buf += sprintf(buf, "mtime %ld ", msg->mtTime);
    buf += sprintf(buf, "ch %ld ", msg->dwPChannel);
    buf += sprintf(buf, "tk %ld ", msg->dwVirtualTrackID);
    buf += sprintf(buf, "voice %ld ", msg->dwVoiceID);
    buf += sprintf(buf, "group %ld ", msg->dwGroupID);

    switch (msg->dwType)
    {
    default:                          buf += sprintf(buf, "type %#lx ", msg->dwType); break;
    case DMUS_PMSGT_MIDI:
    {
        DMUS_MIDI_PMSG *full = (DMUS_MIDI_PMSG *)msg;
        buf += sprintf(buf, "type MIDI ");
        buf += sprintf(buf, "status %#x byte1 %#x byte2 %#x", full->bStatus, full->bByte1, full->bByte2);
        break;
    }
    case DMUS_PMSGT_NOTE:
    {
        DMUS_NOTE_PMSG *full = (DMUS_NOTE_PMSG *)msg;
        buf += sprintf(buf, "type NOTE ");
        buf += sprintf(buf, "dur %ld ", full->mtDuration);
        buf += sprintf(buf, "val %d ", full->wMusicValue);
        buf += sprintf(buf, "me %d ", full->wMeasure);
        buf += sprintf(buf, "of %d ", full->nOffset);
        buf += sprintf(buf, "b %d ", full->bBeat);
        buf += sprintf(buf, "g %d ", full->bGrid);
        buf += sprintf(buf, "v %d ", full->bVelocity);
        buf += sprintf(buf, "f %d ", full->bFlags);
        buf += sprintf(buf, "tr %d ", full->bTimeRange);
        buf += sprintf(buf, "dr %d ", full->bDurRange);
        buf += sprintf(buf, "vr %d ", full->bVelRange);
        buf += sprintf(buf, "pf %d ", full->bPlayModeFlags);
        buf += sprintf(buf, "sc %d ", full->bSubChordLevel);
        buf += sprintf(buf, "mi %d ", full->bMidiValue);
        buf += sprintf(buf, "tr %d ", full->cTranspose);
        break;
    }
    case DMUS_PMSGT_SYSEX:            buf += sprintf(buf, "type SYSE "); break;
    case DMUS_PMSGT_NOTIFICATION:     buf += sprintf(buf, "type NOTI "); break;
    case DMUS_PMSGT_TEMPO:
    {
        DMUS_TEMPO_PMSG *full = (DMUS_TEMPO_PMSG *)msg;
        buf += sprintf(buf, "type TEMPO ");
        buf += sprintf(buf, "tempo %f", full->dblTempo);
        break;
    }
    case DMUS_PMSGT_CURVE:
    {
        DMUS_CURVE_PMSG *full = (DMUS_CURVE_PMSG *)msg;
        buf += sprintf(buf, "type CURVE ");
        buf += sprintf(buf, "dur %ld ", full->mtDuration);
        buf += sprintf(buf, "st %ld ", full->mtOriginalStart);
        buf += sprintf(buf, "re %ld ", full->mtResetDuration);
        buf += sprintf(buf, "stv %d ", full->nStartValue);
        buf += sprintf(buf, "env %d ", full->nEndValue);
        buf += sprintf(buf, "rev %d ", full->nResetValue);
        buf += sprintf(buf, "me %d ", full->wMeasure);
        buf += sprintf(buf, "of %d ", full->nOffset);
        buf += sprintf(buf, "b %d ", full->bBeat);
        buf += sprintf(buf, "g %d ", full->bGrid);
        buf += sprintf(buf, "t %d ", full->bType);
        buf += sprintf(buf, "s %d ", full->bCurveShape);
        buf += sprintf(buf, "c %d ", full->bCCData);
        buf += sprintf(buf, "fl %d ", full->bFlags);
        buf += sprintf(buf, "pm %d ", full->wParamType);
        buf += sprintf(buf, "mg %d ", full->wMergeIndex);
        break;
    }
    case DMUS_PMSGT_TIMESIG:          buf += sprintf(buf, "type TIME "); break;
    case DMUS_PMSGT_PATCH:
    {
        DMUS_PATCH_PMSG *full = (DMUS_PATCH_PMSG *)msg;
        buf += sprintf(buf, "type PATCH ");
        buf += sprintf(buf, "instrument %#x msb %#x lsb %#x", full->byInstrument, full->byMSB, full->byLSB);
        break;
    }
    case DMUS_PMSGT_TRANSPOSE:
    {
        DMUS_TRANSPOSE_PMSG *full = (DMUS_TRANSPOSE_PMSG *)msg;
        buf += sprintf(buf, "type TRANSPOSE ");
        buf += sprintf(buf, "transpose %#x, merge %#x", full->nTranspose, full->wMergeIndex);
        break;
    }
    case DMUS_PMSGT_CHANNEL_PRIORITY: buf += sprintf(buf, "type CHAN "); break;
    case DMUS_PMSGT_STOP:             buf += sprintf(buf, "type STOP "); break;
    case DMUS_PMSGT_DIRTY:            buf += sprintf(buf, "type DIRT "); break;
    case DMUS_PMSGT_WAVE:             buf += sprintf(buf, "type WAVE "); break;
    case DMUS_PMSGT_LYRIC:            buf += sprintf(buf, "type LYRI "); break;
    case DMUS_PMSGT_SCRIPTLYRIC:      buf += sprintf(buf, "type SCRI "); break;
    case DMUS_PMSGT_USER:             buf += sprintf(buf, "type USER "); break;
    }

    return wine_dbg_sprintf("%s", buffer);
}

static HRESULT WINAPI test_tool_ProcessPMsg(IDirectMusicTool8 *iface, IDirectMusicPerformance *performance, DMUS_PMSG *msg)
{
    ok(0, "%#04lx: unexpected %s, performance %p, msg %p %s\n", GetCurrentThreadId(), __func__, performance, msg, debugstr_dmus_pmsg(msg));

    ok(msg->dwFlags == (DMUS_PMSGF_REFTIME|DMUS_PMSGF_MUSICTIME|DMUS_PMSGF_TOOL_IMMEDIATE) ||
       msg->dwFlags == (DMUS_PMSGF_REFTIME|DMUS_PMSGF_MUSICTIME|DMUS_PMSGF_TOOL_IMMEDIATE|DMUS_PMSGF_DX8),
       "got %#lx\n", msg->dwFlags);
    ok(msg->punkUser == NULL, "got %p\n", msg->punkUser);
    ok(msg->pTool == (IDirectMusicTool *)iface, "got %p\n", msg->pTool);
    ok(msg->pGraph != NULL, "got %p\n", msg->pGraph);

    if (!msg->pGraph || (FAILED(IDirectMusicGraph_StampPMsg(msg->pGraph, msg))))
        return DMUS_S_FREE;

    return DMUS_S_REQUEUE;
}

static HRESULT WINAPI test_tool_Flush(IDirectMusicTool8 *iface, IDirectMusicPerformance *pPerf, DMUS_PMSG *msg, REFERENCE_TIME rtTime)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_tool_Clone(IDirectMusicTool8 *iface, IDirectMusicTool **ppTool)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static IDirectMusicTool8Vtbl test_tool_vtbl =
{
    test_tool_QueryInterface,
    test_tool_AddRef,
    test_tool_Release,
    test_tool_Init,
    test_tool_GetMsgDeliveryType,
    test_tool_GetMediaTypeArraySize,
    test_tool_GetMediaTypes,
    test_tool_ProcessPMsg,
    test_tool_Flush,
    test_tool_Clone,
};

static HRESULT test_tool_create(IDirectMusicTool8 **out)
{
    struct test_tool *tool;

    *out = NULL;
    if (!(tool = calloc(1, sizeof(*tool)))) return E_OUTOFMEMORY;
    tool->IDirectMusicTool8_iface.lpVtbl = &test_tool_vtbl;
    tool->ref = 1;

    *out = &tool->IDirectMusicTool8_iface;
    return S_OK;
}
