/*
 * IDirectMusicInstrument Implementation
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dmusic_private.h"
#include "soundfont.h"
#include "dls2.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmusic);

static const GUID IID_IDirectMusicInstrumentPRIVATE = { 0xbcb20080, 0xa40c, 0x11d1, { 0x86, 0xbc, 0x00, 0xc0, 0x4f, 0xbf, 0x8f, 0xef } };

#define CONN_SRC_CC2  0x0082
#define CONN_SRC_RPN0 0x0100

#define CONN_TRN_BIPOLAR (1<<4)
#define CONN_TRN_INVERT  (1<<5)

#define CONN_TRANSFORM(src, ctrl, dst) (((src) & 0x3f) << 10) | (((ctrl) & 0x3f) << 4) | ((dst) & 0xf)

struct articulation
{
    struct list entry;
    CONNECTIONLIST list;
    CONNECTION connections[];
};

C_ASSERT(sizeof(struct articulation) == offsetof(struct articulation, connections[0]));

static const char *debugstr_conn_src(UINT src)
{
    switch (src)
    {
    case CONN_SRC_NONE: return "SRC_NONE";
    case CONN_SRC_LFO: return "SRC_LFO";
    case CONN_SRC_KEYONVELOCITY: return "SRC_KEYONVELOCITY";
    case CONN_SRC_KEYNUMBER: return "SRC_KEYNUMBER";
    case CONN_SRC_EG1: return "SRC_EG1";
    case CONN_SRC_EG2: return "SRC_EG2";
    case CONN_SRC_PITCHWHEEL: return "SRC_PITCHWHEEL";
    case CONN_SRC_CC1: return "SRC_CC1";
    case CONN_SRC_CC7: return "SRC_CC7";
    case CONN_SRC_CC10: return "SRC_CC10";
    case CONN_SRC_CC11: return "SRC_CC11";
    case CONN_SRC_POLYPRESSURE: return "SRC_POLYPRESSURE";
    case CONN_SRC_CHANNELPRESSURE: return "SRC_CHANNELPRESSURE";
    case CONN_SRC_VIBRATO: return "SRC_VIBRATO";
    case CONN_SRC_MONOPRESSURE: return "SRC_MONOPRESSURE";
    case CONN_SRC_CC91: return "SRC_CC91";
    case CONN_SRC_CC93: return "SRC_CC93";

    case CONN_SRC_CC2: return "SRC_CC2";
    case CONN_SRC_RPN0: return "SRC_RPN0";
    }

    return wine_dbg_sprintf("%#x", src);
}

static const char *debugstr_conn_dst(UINT dst)
{
    switch (dst)
    {
    case CONN_DST_NONE: return "DST_NONE";
    /* case CONN_DST_ATTENUATION: return "DST_ATTENUATION"; Same as CONN_DST_GAIN */
    case CONN_DST_PITCH: return "DST_PITCH";
    case CONN_DST_PAN: return "DST_PAN";
    case CONN_DST_LFO_FREQUENCY: return "DST_LFO_FREQUENCY";
    case CONN_DST_LFO_STARTDELAY: return "DST_LFO_STARTDELAY";
    case CONN_DST_EG1_ATTACKTIME: return "DST_EG1_ATTACKTIME";
    case CONN_DST_EG1_DECAYTIME: return "DST_EG1_DECAYTIME";
    case CONN_DST_EG1_RELEASETIME: return "DST_EG1_RELEASETIME";
    case CONN_DST_EG1_SUSTAINLEVEL: return "DST_EG1_SUSTAINLEVEL";
    case CONN_DST_EG2_ATTACKTIME: return "DST_EG2_ATTACKTIME";
    case CONN_DST_EG2_DECAYTIME: return "DST_EG2_DECAYTIME";
    case CONN_DST_EG2_RELEASETIME: return "DST_EG2_RELEASETIME";
    case CONN_DST_EG2_SUSTAINLEVEL: return "DST_EG2_SUSTAINLEVEL";
    case CONN_DST_GAIN: return "DST_GAIN";
    case CONN_DST_KEYNUMBER: return "DST_KEYNUMBER";
    case CONN_DST_LEFT: return "DST_LEFT";
    case CONN_DST_RIGHT: return "DST_RIGHT";
    case CONN_DST_CENTER: return "DST_CENTER";
    case CONN_DST_LEFTREAR: return "DST_LEFTREAR";
    case CONN_DST_RIGHTREAR: return "DST_RIGHTREAR";
    case CONN_DST_LFE_CHANNEL: return "DST_LFE_CHANNEL";
    case CONN_DST_CHORUS: return "DST_CHORUS";
    case CONN_DST_REVERB: return "DST_REVERB";
    case CONN_DST_VIB_FREQUENCY: return "DST_VIB_FREQUENCY";
    case CONN_DST_VIB_STARTDELAY: return "DST_VIB_STARTDELAY";
    case CONN_DST_EG1_DELAYTIME: return "DST_EG1_DELAYTIME";
    case CONN_DST_EG1_HOLDTIME: return "DST_EG1_HOLDTIME";
    case CONN_DST_EG1_SHUTDOWNTIME: return "DST_EG1_SHUTDOWNTIME";
    case CONN_DST_EG2_DELAYTIME: return "DST_EG2_DELAYTIME";
    case CONN_DST_EG2_HOLDTIME: return "DST_EG2_HOLDTIME";
    case CONN_DST_FILTER_CUTOFF: return "DST_FILTER_CUTOFF";
    case CONN_DST_FILTER_Q: return "DST_FILTER_Q";
    }

    return wine_dbg_sprintf("%#x", dst);
}

static const char *debugstr_connection(const CONNECTION *conn)
{
    return wine_dbg_sprintf("%s (%#x) x %s (%#x) -> %s (%#x): %ld", debugstr_conn_src(conn->usSource),
            (conn->usTransform >> 10) & 0x3f, debugstr_conn_src(conn->usControl), (conn->usTransform >> 4) & 0x3f,
            debugstr_conn_dst(conn->usDestination), (conn->usTransform & 0xf), conn->lScale);
}

struct region
{
    struct list entry;
    struct list articulations;
    RGNHEADER header;
    WAVELINK wave_link;
    WSMPL wave_sample;
    WLOOP wave_loop;
    BOOL loop_present;
};

static void region_destroy(struct region *region)
{
    struct articulation *articulation, *next;

    LIST_FOR_EACH_ENTRY_SAFE(articulation, next, &region->articulations, struct articulation, entry)
    {
        list_remove(&articulation->entry);
        free(articulation);
    }

    free(region);
}

struct instrument
{
    IDirectMusicInstrument IDirectMusicInstrument_iface;
    IDirectMusicDownloadedInstrument IDirectMusicDownloadedInstrument_iface;
    LONG ref;

    INSTHEADER header;
    IDirectMusicDownload *download;
    struct collection *collection;
    struct list articulations;
    struct list regions;
};

static inline struct instrument *impl_from_IDirectMusicInstrument(IDirectMusicInstrument *iface)
{
    return CONTAINING_RECORD(iface, struct instrument, IDirectMusicInstrument_iface);
}

static HRESULT WINAPI instrument_QueryInterface(LPDIRECTMUSICINSTRUMENT iface, REFIID riid, LPVOID *ret_iface)
{
    TRACE("(%p)->(%s, %p)\n", iface, debugstr_dmguid(riid), ret_iface);

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IDirectMusicInstrument))
    {
        *ret_iface = iface;
        IDirectMusicInstrument_AddRef(iface);
        return S_OK;
    }
    else if (IsEqualIID(riid, &IID_IDirectMusicInstrumentPRIVATE))
    {
        /* it seems to me that this interface is only basic IUnknown, without any
         * other inherited functions... *sigh* this is the worst scenario, since it means
         * that whoever calls it knows the layout of original implementation table and therefore
         * tries to get data by direct access... expect crashes
         */
        FIXME("*sigh*... requested private/unspecified interface\n");

        *ret_iface = iface;
        IDirectMusicInstrument_AddRef(iface);
        return S_OK;
    }

    WARN("(%p)->(%s, %p): not found\n", iface, debugstr_dmguid(riid), ret_iface);

    return E_NOINTERFACE;
}

static ULONG WINAPI instrument_AddRef(LPDIRECTMUSICINSTRUMENT iface)
{
    struct instrument *This = impl_from_IDirectMusicInstrument(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p): new ref = %lu\n", iface, ref);

    return ref;
}

static ULONG WINAPI instrument_Release(LPDIRECTMUSICINSTRUMENT iface)
{
    struct instrument *This = impl_from_IDirectMusicInstrument(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p): new ref = %lu\n", iface, ref);

    if (!ref)
    {
        struct articulation *articulation, *next_articulation;
        struct region *region, *next_region;

        LIST_FOR_EACH_ENTRY_SAFE(articulation, next_articulation, &This->articulations, struct articulation, entry)
        {
            list_remove(&articulation->entry);
            free(articulation);
        }

        LIST_FOR_EACH_ENTRY_SAFE(region, next_region, &This->regions, struct region, entry)
        {
            list_remove(&region->entry);
            region_destroy(region);
        }

        collection_internal_release(This->collection);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI instrument_GetPatch(LPDIRECTMUSICINSTRUMENT iface, DWORD* pdwPatch)
{
    struct instrument *This = impl_from_IDirectMusicInstrument(iface);

    TRACE("(%p)->(%p)\n", This, pdwPatch);

    *pdwPatch = MIDILOCALE2Patch(&This->header.Locale);

    return S_OK;
}

static HRESULT WINAPI instrument_SetPatch(LPDIRECTMUSICINSTRUMENT iface, DWORD dwPatch)
{
    struct instrument *This = impl_from_IDirectMusicInstrument(iface);

    TRACE("(%p, %ld): stub\n", This, dwPatch);

    Patch2MIDILOCALE(dwPatch, &This->header.Locale);

    return S_OK;
}

static const IDirectMusicInstrumentVtbl instrument_vtbl =
{
    instrument_QueryInterface,
    instrument_AddRef,
    instrument_Release,
    instrument_GetPatch,
    instrument_SetPatch,
};

static inline struct instrument* impl_from_IDirectMusicDownloadedInstrument(IDirectMusicDownloadedInstrument *iface)
{
    return CONTAINING_RECORD(iface, struct instrument, IDirectMusicDownloadedInstrument_iface);
}

static HRESULT WINAPI downloaded_instrument_QueryInterface(IDirectMusicDownloadedInstrument *iface, REFIID riid, VOID **ret_iface)
{
    TRACE("(%p, %s, %p)\n", iface, debugstr_dmguid(riid), ret_iface);

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectMusicDownloadedInstrument))
    {
        IDirectMusicDownloadedInstrument_AddRef(iface);
        *ret_iface = iface;
        return S_OK;
    }

    WARN("(%p, %s, %p): not found\n", iface, debugstr_dmguid(riid), ret_iface);

    return E_NOINTERFACE;
}

static ULONG WINAPI downloaded_instrument_AddRef(IDirectMusicDownloadedInstrument *iface)
{
    struct instrument *This = impl_from_IDirectMusicDownloadedInstrument(iface);
    return IDirectMusicInstrument_AddRef(&This->IDirectMusicInstrument_iface);
}

static ULONG WINAPI downloaded_instrument_Release(IDirectMusicDownloadedInstrument *iface)
{
    struct instrument *This = impl_from_IDirectMusicDownloadedInstrument(iface);
    return IDirectMusicInstrument_Release(&This->IDirectMusicInstrument_iface);
}

static const IDirectMusicDownloadedInstrumentVtbl downloaded_instrument_vtbl =
{
    downloaded_instrument_QueryInterface,
    downloaded_instrument_AddRef,
    downloaded_instrument_Release,
};

static HRESULT instrument_create(struct collection *collection, IDirectMusicInstrument **ret_iface)
{
    struct instrument *instrument;

    *ret_iface = NULL;
    if (!(instrument = calloc(1, sizeof(*instrument)))) return E_OUTOFMEMORY;
    instrument->IDirectMusicInstrument_iface.lpVtbl = &instrument_vtbl;
    instrument->IDirectMusicDownloadedInstrument_iface.lpVtbl = &downloaded_instrument_vtbl;
    instrument->ref = 1;
    collection_internal_addref((instrument->collection = collection));
    list_init(&instrument->articulations);
    list_init(&instrument->regions);

    TRACE("Created DirectMusicInstrument %p\n", instrument);
    *ret_iface = &instrument->IDirectMusicInstrument_iface;
    return S_OK;
}

static HRESULT append_articparams_articulation(struct list *articulations, DMUS_ARTICPARAMS *params)
{
    struct articulation *articulation;
    CONNECTION *conn;

    UINT size = offsetof(struct articulation, connections[20]);
    if (!(articulation = malloc(size))) return E_OUTOFMEMORY;
    articulation->list.cbSize = sizeof(articulation->list);
    articulation->list.cConnections = 20;
    conn = articulation->connections;

#define ADD_CONNECTION(conn, src, dst, value) \
    (conn)->usSource = (src); \
    (conn)->usDestination = (dst); \
    (conn)->lScale = (value); \
    (conn)++; \

    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_LFO_FREQUENCY, params->LFO.pcFrequency);
    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_LFO_STARTDELAY, params->LFO.tcDelay);
    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_GAIN, params->LFO.gcVolumeScale); /* FIXME: is this right? */
    ADD_CONNECTION(conn, CONN_SRC_VIBRATO, CONN_DST_PITCH, params->LFO.pcPitchScale);
    ADD_CONNECTION(conn, CONN_SRC_LFO, CONN_DST_PITCH, params->LFO.pcMWToPitch);
    ADD_CONNECTION(conn, CONN_SRC_LFO, CONN_DST_GAIN, params->LFO.gcMWToVolume);

    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_EG1_ATTACKTIME, params->VolEG.tcAttack);
    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_EG1_DECAYTIME, params->VolEG.tcDecay);
    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_EG1_SUSTAINLEVEL, params->VolEG.ptSustain);
    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_EG1_RELEASETIME, params->VolEG.tcRelease);
    ADD_CONNECTION(conn, CONN_SRC_KEYONVELOCITY, CONN_DST_EG1_ATTACKTIME, params->PitchEG.tcVel2Attack);
    ADD_CONNECTION(conn, CONN_SRC_KEYNUMBER, CONN_DST_EG1_DECAYTIME, params->PitchEG.tcKey2Decay);

    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_EG2_ATTACKTIME, params->PitchEG.tcAttack);
    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_EG2_DECAYTIME, params->PitchEG.tcDecay);
    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_EG2_SUSTAINLEVEL, params->PitchEG.ptSustain);
    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_EG2_RELEASETIME, params->PitchEG.tcRelease);
    ADD_CONNECTION(conn, CONN_SRC_KEYONVELOCITY, CONN_DST_EG2_ATTACKTIME, params->PitchEG.tcVel2Attack);
    ADD_CONNECTION(conn, CONN_SRC_KEYNUMBER, CONN_DST_EG2_DECAYTIME, params->PitchEG.tcKey2Decay);
    ADD_CONNECTION(conn, CONN_SRC_EG2, CONN_DST_PITCH, params->PitchEG.pcRange);

    ADD_CONNECTION(conn, CONN_SRC_NONE, CONN_DST_PAN, params->Misc.ptDefaultPan);
#undef APPEND_CONNECTION

    list_add_tail(articulations, &articulation->entry);
    return S_OK;
}

static HRESULT parse_art1_chunk(struct instrument *This, IStream *stream, struct chunk_entry *chunk,
        struct list *articulations)
{
    struct articulation *articulation;
    CONNECTIONLIST list;
    HRESULT hr;
    UINT size;

    if (chunk->size < sizeof(list)) return E_INVALIDARG;
    if (FAILED(hr = stream_read(stream, &list, sizeof(list)))) return hr;
    if (chunk->size != list.cbSize + sizeof(CONNECTION) * list.cConnections) return E_INVALIDARG;
    if (list.cbSize != sizeof(list)) return E_INVALIDARG;

    size = offsetof(struct articulation, connections[list.cConnections]);
    if (!(articulation = malloc(size))) return E_OUTOFMEMORY;
    articulation->list = list;

    size = sizeof(CONNECTION) * list.cConnections;
    if (FAILED(hr = stream_read(stream, articulation->connections, size))) free(articulation);
    else list_add_tail(articulations, &articulation->entry);

    return hr;
}

static HRESULT parse_lart_list(struct instrument *This, IStream *stream, struct chunk_entry *parent,
        struct list *articulations)
{
    struct chunk_entry chunk = {.parent = parent};
    HRESULT hr;

    while ((hr = stream_next_chunk(stream, &chunk)) == S_OK)
    {
        switch (MAKE_IDTYPE(chunk.id, chunk.type))
        {
        case FOURCC_ART1:
            hr = parse_art1_chunk(This, stream, &chunk, articulations);
            break;

        default:
            FIXME("Ignoring chunk %s %s\n", debugstr_fourcc(chunk.id), debugstr_fourcc(chunk.type));
            break;
        }

        if (FAILED(hr)) break;
    }

    return hr;
}

static HRESULT parse_rgn_chunk(struct instrument *This, IStream *stream, struct chunk_entry *parent)
{
    struct chunk_entry chunk = {.parent = parent};
    struct region *region;
    HRESULT hr;

    if (!(region = malloc(sizeof(*region)))) return E_OUTOFMEMORY;
    list_init(&region->articulations);

    while ((hr = stream_next_chunk(stream, &chunk)) == S_OK)
    {
        switch (MAKE_IDTYPE(chunk.id, chunk.type))
        {
        case FOURCC_RGNH:
            hr = stream_chunk_get_data(stream, &chunk, &region->header, sizeof(region->header));
            break;

        case FOURCC_WSMP:
            if (chunk.size < sizeof(region->wave_sample)) hr = E_INVALIDARG;
            else hr = stream_read(stream, &region->wave_sample, sizeof(region->wave_sample));
            if (SUCCEEDED(hr) && region->wave_sample.cSampleLoops)
            {
                if (region->wave_sample.cSampleLoops > 1) FIXME("More than one wave loop is not implemented\n");
                if (chunk.size != sizeof(WSMPL) + region->wave_sample.cSampleLoops * sizeof(WLOOP)) hr = E_INVALIDARG;
                else hr = stream_read(stream, &region->wave_loop, sizeof(region->wave_loop));
            }
            break;

        case FOURCC_WLNK:
            hr = stream_chunk_get_data(stream, &chunk, &region->wave_link, sizeof(region->wave_link));
            break;

        case MAKE_IDTYPE(FOURCC_LIST, FOURCC_LART):
            hr = parse_lart_list(This, stream, &chunk, &region->articulations);
            break;

        default:
            FIXME("Ignoring chunk %s %s\n", debugstr_fourcc(chunk.id), debugstr_fourcc(chunk.type));
            break;
        }

        if (FAILED(hr)) break;
    }

    if (FAILED(hr)) region_destroy(region);
    else list_add_tail(&This->regions, &region->entry);

    return hr;
}

static HRESULT parse_lrgn_list(struct instrument *This, IStream *stream, struct chunk_entry *parent)
{
    struct chunk_entry chunk = {.parent = parent};
    HRESULT hr;

    while ((hr = stream_next_chunk(stream, &chunk)) == S_OK)
    {
        switch (MAKE_IDTYPE(chunk.id, chunk.type))
        {
        case MAKE_IDTYPE(FOURCC_LIST, FOURCC_RGN):
            hr = parse_rgn_chunk(This, stream, &chunk);
            break;

        default:
            FIXME("Ignoring chunk %s %s\n", debugstr_fourcc(chunk.id), debugstr_fourcc(chunk.type));
            break;
        }

        if (FAILED(hr)) break;
    }

    return hr;
}

static HRESULT parse_ins_chunk(struct instrument *This, IStream *stream, struct chunk_entry *parent,
        DMUS_OBJECTDESC *desc)
{
    struct chunk_entry chunk = {.parent = parent};
    HRESULT hr;

    if (FAILED(hr = dmobj_parsedescriptor(stream, parent, desc, DMUS_OBJ_NAME_INFO|DMUS_OBJ_GUID_DLID))
            || FAILED(hr = stream_reset_chunk_data(stream, parent)))
        return hr;

    while ((hr = stream_next_chunk(stream, &chunk)) == S_OK)
    {
        switch (MAKE_IDTYPE(chunk.id, chunk.type))
        {
        case FOURCC_INSH:
            hr = stream_chunk_get_data(stream, &chunk, &This->header, sizeof(This->header));
            break;

        case FOURCC_DLID:
        case MAKE_IDTYPE(FOURCC_LIST, DMUS_FOURCC_INFO_LIST):
            /* already parsed by dmobj_parsedescriptor */
            break;

        case MAKE_IDTYPE(FOURCC_LIST, FOURCC_LRGN):
            hr = parse_lrgn_list(This, stream, &chunk);
            break;

        case MAKE_IDTYPE(FOURCC_LIST, FOURCC_LART):
            hr = parse_lart_list(This, stream, &chunk, &This->articulations);
            break;

        default:
            FIXME("Ignoring chunk %s %s\n", debugstr_fourcc(chunk.id), debugstr_fourcc(chunk.type));
            break;
        }

        if (FAILED(hr)) break;
    }

    return hr;
}

HRESULT instrument_create_from_chunk(IStream *stream, struct chunk_entry *parent,
        struct collection *collection, DMUS_OBJECTDESC *desc, IDirectMusicInstrument **ret_iface)
{
    IDirectMusicInstrument *iface;
    struct instrument *This;
    HRESULT hr;

    TRACE("(%p, %p, %p, %p, %p)\n", stream, parent, collection, desc, ret_iface);

    if (FAILED(hr = instrument_create(collection, &iface))) return hr;
    This = impl_from_IDirectMusicInstrument(iface);

    if (FAILED(hr = parse_ins_chunk(This, stream, parent, desc)))
    {
        IDirectMusicInstrument_Release(iface);
        return DMUS_E_UNSUPPORTED_STREAM;
    }

    if (TRACE_ON(dmusic))
    {
        struct region *region;
        UINT i;

        TRACE("Created DirectMusicInstrument %p\n", This);
        TRACE(" - header:\n");
        TRACE("    - regions: %ld\n", This->header.cRegions);
        TRACE("    - locale: {bank: %#lx, instrument: %#lx} (patch %#lx)\n",
                This->header.Locale.ulBank, This->header.Locale.ulInstrument,
                MIDILOCALE2Patch(&This->header.Locale));
        if (desc->dwValidData & DMUS_OBJ_OBJECT) TRACE(" - guid: %s\n", debugstr_dmguid(&desc->guidObject));
        if (desc->dwValidData & DMUS_OBJ_NAME) TRACE(" - name: %s\n", debugstr_w(desc->wszName));

        TRACE(" - regions:\n");
        LIST_FOR_EACH_ENTRY(region, &This->regions, struct region, entry)
        {
            TRACE("   - region:\n");
            TRACE("     - header: {key: %u - %u, vel: %u - %u, options: %#x, group: %#x}\n",
                    region->header.RangeKey.usLow, region->header.RangeKey.usHigh,
                    region->header.RangeVelocity.usLow, region->header.RangeVelocity.usHigh,
                    region->header.fusOptions, region->header.usKeyGroup);
            TRACE("     - wave_link: {options: %#x, group: %u, channel: %lu, index: %lu}\n",
                    region->wave_link.fusOptions, region->wave_link.usPhaseGroup,
                    region->wave_link.ulChannel, region->wave_link.ulTableIndex);
            TRACE("     - wave_sample: {size: %lu, unity_note: %u, fine_tune: %d, attenuation: %ld, options: %#lx, loops: %lu}\n",
                    region->wave_sample.cbSize, region->wave_sample.usUnityNote,
                    region->wave_sample.sFineTune, region->wave_sample.lAttenuation,
                    region->wave_sample.fulOptions, region->wave_sample.cSampleLoops);
            for (i = 0; i < region->wave_sample.cSampleLoops; i++)
                TRACE("     - wave_loop[%u]: {size: %lu, type: %lu, start: %lu, length: %lu}\n", i,
                        region->wave_loop.cbSize, region->wave_loop.ulType,
                        region->wave_loop.ulStart, region->wave_loop.ulLength);
        }
    }

    *ret_iface = iface;
    return S_OK;
}

struct sf_generators
{
    union sf_amount amount[SF_GEN_END_OPER];
};

static const struct sf_generators SF_DEFAULT_GENERATORS =
{
    .amount =
    {
        [SF_GEN_INITIAL_FILTER_FC] = {.value = 13500},
        [SF_GEN_DELAY_MOD_LFO] = {.value = -12000},
        [SF_GEN_DELAY_VIB_LFO] = {.value = -12000},
        [SF_GEN_DELAY_MOD_ENV] = {.value = -12000},
        [SF_GEN_ATTACK_MOD_ENV] = {.value = -12000},
        [SF_GEN_HOLD_MOD_ENV] = {.value = -12000},
        [SF_GEN_DECAY_MOD_ENV] = {.value = -12000},
        [SF_GEN_RELEASE_MOD_ENV] = {.value = -12000},
        [SF_GEN_DELAY_VOL_ENV] = {.value = -12000},
        [SF_GEN_ATTACK_VOL_ENV] = {.value = -12000},
        [SF_GEN_HOLD_VOL_ENV] = {.value = -12000},
        [SF_GEN_DECAY_VOL_ENV] = {.value = -12000},
        [SF_GEN_RELEASE_VOL_ENV] = {.value = -12000},
        [SF_GEN_KEY_RANGE] = {.range = {.low = 0, .high = 127}},
        [SF_GEN_VEL_RANGE] = {.range = {.low = 0, .high = 127}},
        [SF_GEN_KEYNUM] = {.value = -1},
        [SF_GEN_VELOCITY] = {.value = -1},
        [SF_GEN_SCALE_TUNING] = {.value = 100},
        [SF_GEN_OVERRIDING_ROOT_KEY] = {.value = -1},
    }
};

static BOOL parse_soundfont_generators(struct soundfont *soundfont, UINT index,
        struct sf_generators *preset_generators, struct sf_generators *generators)
{
    struct sf_bag *bag = (preset_generators ? soundfont->ibag : soundfont->pbag) + index;
    struct sf_gen *gen, *gens = preset_generators ? soundfont->igen : soundfont->pgen;

    for (gen = gens + bag->gen_ndx; gen < gens + (bag + 1)->gen_ndx; gen++)
    {
        switch (gen->oper)
        {
        case SF_GEN_START_ADDRS_OFFSET:
        case SF_GEN_END_ADDRS_OFFSET:
        case SF_GEN_STARTLOOP_ADDRS_OFFSET:
        case SF_GEN_ENDLOOP_ADDRS_OFFSET:
        case SF_GEN_START_ADDRS_COARSE_OFFSET:
        case SF_GEN_END_ADDRS_COARSE_OFFSET:
        case SF_GEN_STARTLOOP_ADDRS_COARSE_OFFSET:
        case SF_GEN_KEYNUM:
        case SF_GEN_VELOCITY:
        case SF_GEN_ENDLOOP_ADDRS_COARSE_OFFSET:
        case SF_GEN_SAMPLE_MODES:
        case SF_GEN_EXCLUSIVE_CLASS:
        case SF_GEN_OVERRIDING_ROOT_KEY:
            if (preset_generators) generators->amount[gen->oper] = gen->amount;
            else WARN("Ignoring invalid preset generator %s\n", debugstr_sf_gen(gen));
            break;

        case SF_GEN_INSTRUMENT:
            if (!preset_generators) generators->amount[gen->oper] = gen->amount;
            else WARN("Ignoring invalid instrument generator %s\n", debugstr_sf_gen(gen));
            /* should always be the last generator */
            return FALSE;

        case SF_GEN_SAMPLE_ID:
            if (preset_generators) generators->amount[gen->oper] = gen->amount;
            else WARN("Ignoring invalid preset generator %s\n", debugstr_sf_gen(gen));
            /* should always be the last generator */
            return FALSE;

        default:
            generators->amount[gen->oper] = gen->amount;
            if (preset_generators) generators->amount[gen->oper].value += preset_generators->amount[gen->oper].value;
            break;
        }
    }

    return TRUE;
}

static const union
{
    struct articulation articulation;
    struct
    {
        struct list entry;
        CONNECTIONLIST list;
        CONNECTION connections[10];
    };
} SF_DEFAULT_MODULATORS =
{
    .list = {.cbSize = sizeof(CONNECTIONLIST), .cConnections = ARRAY_SIZE(SF_DEFAULT_MODULATORS.connections)},
    .connections =
    {
        {
            .usSource = CONN_SRC_KEYONVELOCITY,
            .usDestination = CONN_DST_ATTENUATION,
            .usTransform = CONN_TRANSFORM(CONN_TRN_INVERT | CONN_TRN_CONCAVE, CONN_TRN_NONE, CONN_TRN_NONE),
            .lScale = 960,
        },
        {
            .usSource = CONN_SRC_KEYONVELOCITY,
            .usDestination = CONN_DST_FILTER_CUTOFF,
            .usTransform = CONN_TRANSFORM(CONN_TRN_INVERT, CONN_TRN_NONE, CONN_TRN_NONE),
            .lScale = -2400,
        },
        {
            .usSource = CONN_SRC_VIBRATO,
            .usControl = CONN_SRC_CHANNELPRESSURE,
            .usDestination = CONN_DST_PITCH,
            .lScale = 50,
        },
        {
            .usSource = CONN_SRC_VIBRATO,
            .usControl = CONN_SRC_CC1,
            .usDestination = CONN_DST_PITCH,
            .lScale = 50,
        },
        {
            .usSource = CONN_SRC_CC2,
            .usDestination = CONN_DST_ATTENUATION,
            .usTransform = CONN_TRANSFORM(CONN_TRN_INVERT | CONN_TRN_CONCAVE, CONN_TRN_NONE, CONN_TRN_NONE),
            .lScale = 960,
        },
        {
            .usSource = CONN_SRC_CC10,
            .usDestination = CONN_DST_ATTENUATION,
            .usTransform = CONN_TRANSFORM(CONN_TRN_BIPOLAR, CONN_TRN_NONE, CONN_TRN_NONE),
            .lScale = 1000,
        },
        {
            .usSource = CONN_SRC_CC11,
            .usDestination = CONN_DST_ATTENUATION,
            .usTransform = CONN_TRANSFORM(CONN_TRN_INVERT | CONN_TRN_CONCAVE, CONN_TRN_NONE, CONN_TRN_NONE),
            .lScale = 960,
        },
        {
            .usSource = CONN_SRC_CC91,
            .usDestination = CONN_DST_REVERB,
            .lScale = 200,
        },
        {
            .usSource = CONN_SRC_CC93,
            .usDestination = CONN_DST_CHORUS,
            .lScale = 200,
        },
        {
            .usSource = CONN_SRC_PITCHWHEEL,
            .usDestination = CONN_DST_PITCH,
            .usControl = CONN_SRC_RPN0,
            .usTransform = CONN_TRANSFORM(CONN_TRN_BIPOLAR, CONN_TRN_NONE, CONN_TRN_NONE),
            .lScale = 12700,
        },
    },
};

static int connection_cmp(const void *a, const void *b)
{
    const CONNECTION *ac = a, *bc = b;
    int ret;

    if ((ret = (ac->usSource - bc->usSource))) return ret;
    if ((ret = (ac->usControl - bc->usControl))) return ret;
    if ((ret = (ac->usDestination - bc->usDestination))) return ret;
    if ((ret = (ac->usTransform - bc->usTransform))) return ret;

    return 0;
}

static BOOL sf_modulator_to_source_transform(sf_modulator mod, USHORT *source, USHORT *transform)
{
    UINT trn = 0;

    if (mod & SF_MOD_CTRL_MIDI) *source = mod & 0x8f;
    else switch (mod & 0x7f)
    {
    case SF_MOD_CTRL_GEN_NONE: *source = CONN_SRC_NONE; break;
    case SF_MOD_CTRL_GEN_VELOCITY: *source = CONN_SRC_KEYONVELOCITY; break;
    case SF_MOD_CTRL_GEN_KEY: *source = CONN_SRC_KEYNUMBER; break;
    case SF_MOD_CTRL_GEN_POLY_PRESSURE: *source = CONN_SRC_POLYPRESSURE; break;
    case SF_MOD_CTRL_GEN_CHAN_PRESSURE: *source = CONN_SRC_CHANNELPRESSURE; break;
    case SF_MOD_CTRL_GEN_PITCH_WHEEL: *source = CONN_SRC_PITCHWHEEL; break;
    case SF_MOD_CTRL_GEN_PITCH_WHEEL_SENSITIVITY: *source = CONN_SRC_RPN0; break;
    case SF_MOD_CTRL_GEN_LINK: FIXME("Unsupported linked modulator\n"); return FALSE;
    }

    if (mod & SF_MOD_DIR_DECREASING) trn |= CONN_TRN_INVERT;
    if (mod & SF_MOD_POL_BIPOLAR) trn |= CONN_TRN_BIPOLAR;
    switch (mod & SF_MOD_SRC_SWITCH)
    {
    case SF_MOD_SRC_LINEAR: trn |= CONN_TRN_NONE;
    case SF_MOD_SRC_CONCAVE: trn |= CONN_TRN_CONCAVE;
    case SF_MOD_SRC_CONVEX: trn |= CONN_TRN_CONVEX;
    case SF_MOD_SRC_SWITCH: trn |= CONN_TRN_SWITCH;
    }

    *transform = trn;
    return TRUE;
}

static BOOL connection_from_modulator(CONNECTION *conn, struct sf_mod *mod)
{
    USHORT src_trn = 0, ctrl_trn = 0;

    memset(conn, 0, sizeof(*conn));

    switch (mod->dest_gen)
    {
    case SF_GEN_MOD_LFO_TO_FILTER_FC:
    case SF_GEN_MOD_LFO_TO_VOLUME:
    case SF_GEN_MOD_LFO_TO_PITCH:
        conn->usSource = CONN_SRC_LFO;
        break;
    case SF_GEN_VIB_LFO_TO_PITCH:
        conn->usSource = CONN_SRC_VIBRATO;
        break;
    case SF_GEN_MOD_ENV_TO_PITCH:
    case SF_GEN_MOD_ENV_TO_FILTER_FC:
        conn->usSource = CONN_SRC_EG2;
        break;
    case SF_GEN_KEYNUM_TO_MOD_ENV_HOLD:
    case SF_GEN_KEYNUM_TO_MOD_ENV_DECAY:
    case SF_GEN_KEYNUM_TO_VOL_ENV_HOLD:
    case SF_GEN_KEYNUM_TO_VOL_ENV_DECAY:
        conn->usSource = CONN_SRC_KEYNUMBER;
        break;
    default:
        if (!sf_modulator_to_source_transform(mod->src_mod, &conn->usSource, &src_trn))
        {
            FIXME("Unsupported modulator %s\n", debugstr_sf_mod(mod));
            return FALSE;
        }
        break;
    }

    switch (mod->dest_gen)
    {
    case SF_GEN_MOD_LFO_TO_FILTER_FC:
    case SF_GEN_MOD_LFO_TO_VOLUME:
    case SF_GEN_MOD_LFO_TO_PITCH:
    case SF_GEN_VIB_LFO_TO_PITCH:
        if (mod->src_mod == (SF_MOD_CTRL_MIDI | 1))
            conn->usControl = CONN_SRC_CC1;
        else if (mod->src_mod == SF_MOD_CTRL_GEN_CHAN_PRESSURE)
            conn->usControl = CONN_SRC_CHANNELPRESSURE;
        else
        {
            FIXME("Unsupported modulator source %s\n", debugstr_sf_mod(mod));
            return FALSE;
        }
        break;
    default:
        if (!sf_modulator_to_source_transform(mod->amount_src_mod, &conn->usControl, &ctrl_trn))
        {
            FIXME("Unsupported modulator %s\n", debugstr_sf_mod(mod));
            return FALSE;
        }
        break;
    }

    switch (mod->dest_gen)
    {
    case SF_GEN_MOD_LFO_TO_FILTER_FC: conn->usDestination = CONN_DST_FILTER_CUTOFF; break;
    case SF_GEN_MOD_LFO_TO_VOLUME: conn->usDestination = CONN_DST_GAIN; break;
    case SF_GEN_MOD_LFO_TO_PITCH: conn->usDestination = CONN_DST_PITCH; break;
    case SF_GEN_VIB_LFO_TO_PITCH: conn->usDestination = CONN_DST_PITCH; break;
    case SF_GEN_MOD_ENV_TO_PITCH: conn->usDestination = CONN_DST_PITCH; break;
    case SF_GEN_MOD_ENV_TO_FILTER_FC: conn->usDestination = CONN_DST_FILTER_CUTOFF; break;
    case SF_GEN_KEYNUM_TO_MOD_ENV_HOLD: conn->usDestination = CONN_DST_EG2_HOLDTIME; break;
    case SF_GEN_KEYNUM_TO_MOD_ENV_DECAY: conn->usDestination = CONN_DST_EG2_DECAYTIME; break;
    case SF_GEN_KEYNUM_TO_VOL_ENV_HOLD: conn->usDestination = CONN_DST_EG1_HOLDTIME; break;
    case SF_GEN_KEYNUM_TO_VOL_ENV_DECAY: conn->usDestination = CONN_DST_EG1_DECAYTIME; break;
    case SF_GEN_INITIAL_FILTER_FC: conn->usDestination = CONN_DST_FILTER_CUTOFF; break;
    case SF_GEN_INITIAL_FILTER_Q: conn->usDestination = CONN_DST_FILTER_CUTOFF; break;
    case SF_GEN_CHORUS_EFFECTS_SEND: conn->usDestination = CONN_DST_CHORUS; break;
    case SF_GEN_REVERB_EFFECTS_SEND: conn->usDestination = CONN_DST_REVERB; break;
    case SF_GEN_PAN: conn->usDestination = CONN_DST_PAN; break;
    case SF_GEN_DELAY_MOD_LFO: conn->usDestination = CONN_DST_LFO_STARTDELAY; break;
    case SF_GEN_FREQ_MOD_LFO: conn->usDestination = CONN_DST_LFO_FREQUENCY; break;
    case SF_GEN_DELAY_VIB_LFO: conn->usDestination = CONN_DST_VIB_STARTDELAY; break;
    case SF_GEN_FREQ_VIB_LFO: conn->usDestination = CONN_DST_VIB_FREQUENCY; break;
    case SF_GEN_DELAY_MOD_ENV: conn->usDestination = CONN_DST_EG2_DELAYTIME; break;
    case SF_GEN_ATTACK_MOD_ENV: conn->usDestination = CONN_DST_EG2_ATTACKTIME; break;
    case SF_GEN_HOLD_MOD_ENV: conn->usDestination = CONN_DST_EG2_HOLDTIME; break;
    case SF_GEN_DECAY_MOD_ENV: conn->usDestination = CONN_DST_EG2_DECAYTIME; break;
    case SF_GEN_SUSTAIN_MOD_ENV: conn->usDestination = CONN_DST_EG2_SUSTAINLEVEL; break;
    case SF_GEN_RELEASE_MOD_ENV: conn->usDestination = CONN_DST_EG2_RELEASETIME; break;
    case SF_GEN_DELAY_VOL_ENV: conn->usDestination = CONN_DST_EG1_DELAYTIME; break;
    case SF_GEN_ATTACK_VOL_ENV: conn->usDestination = CONN_DST_EG1_ATTACKTIME; break;
    case SF_GEN_HOLD_VOL_ENV: conn->usDestination = CONN_DST_EG1_HOLDTIME; break;
    case SF_GEN_DECAY_VOL_ENV: conn->usDestination = CONN_DST_EG1_DECAYTIME; break;
    case SF_GEN_SUSTAIN_VOL_ENV: conn->usDestination = CONN_DST_EG1_SUSTAINLEVEL; break;
    case SF_GEN_RELEASE_VOL_ENV: conn->usDestination = CONN_DST_EG1_RELEASETIME; break;
    case SF_GEN_INITIAL_ATTENUATION: conn->usDestination = CONN_DST_ATTENUATION; break;
    default:
        FIXME("Unsupported modulator: %s\n", debugstr_sf_mod(mod));
        return FALSE;
    }

    if (mod->transform == SF_TRAN_ABSOLUTE) FIXME("Unsupported SF_TRAN_ABSOLUTE\n");
    conn->usTransform = CONN_TRANSFORM(src_trn, ctrl_trn, 0);

    conn->lScale = mod->amount;
    TRACE("modulator %s to %s\n", debugstr_sf_mod(mod), debugstr_connection(conn));
    return TRUE;
}

static BOOL connection_from_generator(CONNECTION *conn, sf_generator gen, union sf_amount amount)
{
    memset(conn, 0, sizeof(*conn));

    switch (gen)
    {
    case SF_GEN_MOD_LFO_TO_PITCH:
    case SF_GEN_MOD_LFO_TO_FILTER_FC:
    case SF_GEN_MOD_LFO_TO_VOLUME:
        conn->usSource = CONN_SRC_LFO;
        break;
    case SF_GEN_VIB_LFO_TO_PITCH:
        conn->usSource = CONN_SRC_VIBRATO;
        break;
    case SF_GEN_MOD_ENV_TO_PITCH:
    case SF_GEN_MOD_ENV_TO_FILTER_FC:
        conn->usSource = CONN_SRC_EG2;
        break;
    case SF_GEN_KEYNUM_TO_MOD_ENV_HOLD:
    case SF_GEN_KEYNUM_TO_MOD_ENV_DECAY:
    case SF_GEN_KEYNUM_TO_VOL_ENV_HOLD:
    case SF_GEN_KEYNUM_TO_VOL_ENV_DECAY:
        conn->usSource = CONN_SRC_KEYNUMBER;
        break;
    case SF_GEN_SCALE_TUNING:
        conn->usSource = CONN_SRC_KEYNUMBER;
        break;
    }

    switch (gen)
    {
    case SF_GEN_INITIAL_FILTER_FC: conn->usDestination = CONN_DST_FILTER_CUTOFF; break;
    case SF_GEN_INITIAL_FILTER_Q: conn->usDestination = CONN_DST_FILTER_CUTOFF; break;
    case SF_GEN_CHORUS_EFFECTS_SEND: conn->usDestination = CONN_DST_CHORUS; break;
    case SF_GEN_REVERB_EFFECTS_SEND: conn->usDestination = CONN_DST_REVERB; break;
    case SF_GEN_PAN: conn->usDestination = CONN_DST_PAN; break;
    case SF_GEN_DELAY_MOD_LFO: conn->usDestination = CONN_DST_LFO_STARTDELAY; break;
    case SF_GEN_FREQ_MOD_LFO: conn->usDestination = CONN_DST_LFO_FREQUENCY; break;
    case SF_GEN_DELAY_VIB_LFO: conn->usDestination = CONN_DST_VIB_STARTDELAY; break;
    case SF_GEN_FREQ_VIB_LFO: conn->usDestination = CONN_DST_VIB_FREQUENCY; break;
    case SF_GEN_DELAY_MOD_ENV: conn->usDestination = CONN_DST_EG2_DELAYTIME; break;
    case SF_GEN_ATTACK_MOD_ENV: conn->usDestination = CONN_DST_EG2_ATTACKTIME; break;
    case SF_GEN_HOLD_MOD_ENV: conn->usDestination = CONN_DST_EG2_HOLDTIME; break;
    case SF_GEN_DECAY_MOD_ENV: conn->usDestination = CONN_DST_EG2_DECAYTIME; break;
    case SF_GEN_SUSTAIN_MOD_ENV: conn->usDestination = CONN_DST_EG2_SUSTAINLEVEL; break;
    case SF_GEN_RELEASE_MOD_ENV: conn->usDestination = CONN_DST_EG2_RELEASETIME; break;
    case SF_GEN_DELAY_VOL_ENV: conn->usDestination = CONN_DST_EG1_DELAYTIME; break;
    case SF_GEN_ATTACK_VOL_ENV: conn->usDestination = CONN_DST_EG1_ATTACKTIME; break;
    case SF_GEN_HOLD_VOL_ENV: conn->usDestination = CONN_DST_EG1_HOLDTIME; break;
    case SF_GEN_DECAY_VOL_ENV: conn->usDestination = CONN_DST_EG1_DECAYTIME; break;
    case SF_GEN_SUSTAIN_VOL_ENV: conn->usDestination = CONN_DST_EG1_SUSTAINLEVEL; break;
    case SF_GEN_RELEASE_VOL_ENV: conn->usDestination = CONN_DST_EG1_RELEASETIME; break;
    case SF_GEN_INITIAL_ATTENUATION: conn->usDestination = CONN_DST_ATTENUATION; break;
    case SF_GEN_MOD_LFO_TO_VOLUME: conn->usDestination = CONN_DST_GAIN; break;
    case SF_GEN_KEYNUM_TO_MOD_ENV_HOLD: conn->usDestination = CONN_DST_EG2_HOLDTIME; break;
    case SF_GEN_KEYNUM_TO_MOD_ENV_DECAY: conn->usDestination = CONN_DST_EG2_DECAYTIME; break;
    case SF_GEN_KEYNUM_TO_VOL_ENV_HOLD: conn->usDestination = CONN_DST_EG1_HOLDTIME; break;
    case SF_GEN_KEYNUM_TO_VOL_ENV_DECAY: conn->usDestination = CONN_DST_EG1_DECAYTIME; break;

    case SF_GEN_MOD_LFO_TO_PITCH:
    case SF_GEN_VIB_LFO_TO_PITCH:
    case SF_GEN_MOD_ENV_TO_PITCH:
    case SF_GEN_SCALE_TUNING:
        conn->usDestination = CONN_DST_PITCH;
        break;
    case SF_GEN_MOD_LFO_TO_FILTER_FC:
    case SF_GEN_MOD_ENV_TO_FILTER_FC:
        conn->usDestination = CONN_DST_FILTER_CUTOFF;
        break;

    default:
        FIXME("Unsupported generator %s\n", debugstr_sf_generator(gen));
    case SF_GEN_KEYNUM:
    case SF_GEN_VELOCITY:
    case SF_GEN_INSTRUMENT:
    case SF_GEN_SAMPLE_ID:
    case SF_GEN_KEY_RANGE:
    case SF_GEN_VEL_RANGE:
    case SF_GEN_FINE_TUNE:
    case SF_GEN_COARSE_TUNE:
    case SF_GEN_EXCLUSIVE_CLASS:
    case SF_GEN_OVERRIDING_ROOT_KEY:
    case SF_GEN_START_ADDRS_OFFSET:
    case SF_GEN_END_ADDRS_OFFSET:
    case SF_GEN_STARTLOOP_ADDRS_OFFSET:
    case SF_GEN_ENDLOOP_ADDRS_OFFSET:
    case SF_GEN_START_ADDRS_COARSE_OFFSET:
    case SF_GEN_END_ADDRS_COARSE_OFFSET:
    case SF_GEN_STARTLOOP_ADDRS_COARSE_OFFSET:
    case SF_GEN_ENDLOOP_ADDRS_COARSE_OFFSET:
    case SF_GEN_SAMPLE_MODES:
        return FALSE;
    }

    conn->lScale = (short)amount.value;
    TRACE("generator %s to %s\n", debugstr_sf_generator(gen), debugstr_connection(conn));
    return TRUE;
}

static HRESULT parse_soundfont_instrument_modulators(struct soundfont *soundfont, UINT index,
        struct sf_generators *generators, const struct articulation *global_mods,
        struct articulation *preset_mods, struct articulation **ret)
{
    struct sf_bag *bag = soundfont->ibag + index;
    CONNECTIONLIST list = {.cbSize = sizeof(list)};
    struct articulation *articulation;
    CONNECTION *connection, *other;
    UINT i, size;

    list.cConnections += global_mods->list.cConnections;
    for (i = 0; generators && i < ARRAY_SIZE(generators->amount); i++)
        if (generators->amount[i].value) list.cConnections++;
    list.cConnections += (bag + 1)->mod_ndx - bag->mod_ndx;
    if (preset_mods) list.cConnections += preset_mods->list.cConnections;

    *ret = NULL;
    if (list.cConnections == 0) return S_OK;
    size = offsetof(struct articulation, connections[list.cConnections]);
    if (!(articulation = malloc(size))) return E_OUTOFMEMORY;
    articulation->list = list;
    connection = articulation->connections;

    for (i = 0; i < global_mods->list.cConnections; i++)
        *connection++ = global_mods->connections[i];

    for (i = 0; generators && i < ARRAY_SIZE(generators->amount); i++)
    {
        if (!generators->amount[i].value) continue;
        if (!connection_from_generator(connection, i, generators->amount[i])) continue;

        /* replace any identical default value with the instrument level mod */
        for (other = articulation->connections; other < connection; other++)
            if (!connection_cmp(other, connection)) break;
        if (other != connection) *other = *connection;
        else connection++;
    }

    for (index = bag->mod_ndx; index < (bag + 1)->mod_ndx; index++)
    {
        struct sf_mod *mod = soundfont->imod + index;
        if (!connection_from_modulator(connection, mod)) continue;

        /* replace any identical default value with the instrument level mod */
        for (other = articulation->connections; other < connection; other++)
            if (!connection_cmp(other, connection)) break;
        if (other != connection) *other = *connection;
        else connection++;
    }

    for (i = 0; preset_mods && i < preset_mods->list.cConnections; i++)
    {
        CONNECTION *preset = preset_mods->connections + i;

        /* add any identical preset mod value to the instrument level mod */
        for (other = articulation->connections; other < connection; other++)
            if (!connection_cmp(other, preset)) break;
        if (other != connection) other->lScale += preset->lScale;
        else *connection++ = *preset;
    }

    articulation->list.cConnections = connection - articulation->connections;

    *ret = articulation;
    return S_OK;
}

static HRESULT parse_soundfont_preset_modulators(struct soundfont *soundfont, UINT index,
        struct articulation *global_mods, struct articulation **ret)
{
    struct sf_bag *bag = soundfont->pbag + index;
    CONNECTIONLIST list = {.cbSize = sizeof(list)};
    struct articulation *articulation;
    CONNECTION *connection, *other;
    UINT i, size;

    list.cConnections = (bag + 1)->mod_ndx - bag->mod_ndx;
    if (global_mods) list.cConnections += global_mods->list.cConnections;

    *ret = NULL;
    if (list.cConnections == 0) return S_OK;
    size = offsetof(struct articulation, connections[list.cConnections]);
    if (!(articulation = malloc(size))) return E_OUTOFMEMORY;
    articulation->list = list;
    connection = articulation->connections;

    for (i = 0; global_mods && i < global_mods->list.cConnections; i++, connection++)
        *connection = global_mods->connections[i];

    for (index = bag->mod_ndx; index < (bag + 1)->mod_ndx; index++)
    {
        struct sf_mod *mod = soundfont->pmod + index;
        if (!connection_from_modulator(connection, mod)) continue;

        /* replace any identical global value with the preset level mod */
        for (other = articulation->connections; other < connection; other++)
            if (!connection_cmp(other, connection)) break;
        if (other != connection) *other = *connection;
        else connection++;
    }

    articulation->list.cConnections = connection - articulation->connections;

    *ret = articulation;
    return S_OK;
}

static HRESULT instrument_add_soundfont_region(struct instrument *This, struct soundfont *soundfont,
        struct sf_generators *generators, struct articulation *modulators)
{
    UINT start_loop, end_loop, unity_note, sample_index = generators->amount[SF_GEN_SAMPLE_ID].value;
    struct sf_sample *sample = soundfont->shdr + sample_index;
    struct region *region;

    if (!(region = calloc(1, sizeof(*region)))) return E_OUTOFMEMORY;
    list_init(&region->articulations);
    if (modulators) list_add_tail(&region->articulations, &modulators->entry);

    region->header.RangeKey.usLow = generators->amount[SF_GEN_KEY_RANGE].range.low;
    region->header.RangeKey.usHigh = generators->amount[SF_GEN_KEY_RANGE].range.high;
    region->header.RangeVelocity.usLow = generators->amount[SF_GEN_VEL_RANGE].range.low;
    region->header.RangeVelocity.usHigh = generators->amount[SF_GEN_VEL_RANGE].range.high;

    region->wave_link.ulTableIndex = sample_index;

    unity_note = generators->amount[SF_GEN_OVERRIDING_ROOT_KEY].value;
    if (unity_note == -1) unity_note = sample->original_key;
    region->wave_sample.usUnityNote = unity_note;
    region->wave_sample.sFineTune = generators->amount[SF_GEN_FINE_TUNE].value;
    region->wave_sample.lAttenuation = sample->correction;

    start_loop = generators->amount[SF_GEN_STARTLOOP_ADDRS_OFFSET].value;
    end_loop = generators->amount[SF_GEN_ENDLOOP_ADDRS_OFFSET].value;
    if (start_loop || end_loop)
    {
        region->loop_present = TRUE;
        region->wave_sample.cSampleLoops = 1;
        region->wave_loop.ulStart = start_loop;
        region->wave_loop.ulLength = end_loop - start_loop;
    }

    list_add_tail(&This->regions, &region->entry);
    This->header.cRegions++;
    return S_OK;
}

static HRESULT instrument_add_soundfont_instrument(struct instrument *This, struct soundfont *soundfont,
        UINT index, struct sf_generators *preset_generators, struct articulation *preset_modulators)
{
    struct sf_generators global_generators = SF_DEFAULT_GENERATORS;
    struct sf_instrument *instrument = soundfont->inst + index;
    struct articulation *global_modulators = NULL;
    UINT i = instrument->bag_ndx;
    HRESULT hr = S_OK;

    for (i = instrument->bag_ndx; SUCCEEDED(hr) && i < (instrument + 1)->bag_ndx; i++)
    {
        struct sf_generators generators = global_generators;
        struct articulation *modulators = NULL;

        if (parse_soundfont_generators(soundfont, i, preset_generators, &generators))
        {
            if (i > instrument->bag_ndx)
                WARN("Ignoring instrument zone without a sample id\n");
            else if (SUCCEEDED(hr = parse_soundfont_instrument_modulators(soundfont, i, NULL,
                    &SF_DEFAULT_MODULATORS.articulation, preset_modulators, &global_modulators)))
                global_generators = generators;
            continue;
        }

        if (SUCCEEDED(hr = parse_soundfont_instrument_modulators(soundfont, i, &generators,
                global_modulators ? global_modulators : &SF_DEFAULT_MODULATORS.articulation,
                preset_modulators, &modulators)))
        {
            hr = instrument_add_soundfont_region(This, soundfont, &generators, modulators);
            if (FAILED(hr)) free(modulators);
        }
    }

    free(global_modulators);
    return hr;
}

HRESULT instrument_create_from_soundfont(struct soundfont *soundfont, UINT index,
        struct collection *collection, DMUS_OBJECTDESC *desc, IDirectMusicInstrument **ret_iface)
{
    struct sf_preset *preset = soundfont->phdr + index;
    struct articulation *global_modulators = NULL;
    struct sf_generators global_generators = {0};
    IDirectMusicInstrument *iface;
    struct instrument *This;
    HRESULT hr;
    UINT i;

    TRACE("(%p, %u, %p, %p, %p)\n", soundfont, index, collection, desc, ret_iface);

    if (FAILED(hr = instrument_create(collection, &iface))) return hr;
    This = impl_from_IDirectMusicInstrument(iface);

    This->header.Locale.ulBank = (preset->bank & 0x7f) | ((preset->bank << 1) & 0x7f00);
    This->header.Locale.ulInstrument = preset->preset;
    MultiByteToWideChar(CP_ACP, 0, preset->name, strlen(preset->name) + 1,
            desc->wszName, sizeof(desc->wszName));

    for (i = preset->bag_ndx; SUCCEEDED(hr) && i < (preset + 1)->bag_ndx; i++)
    {
        struct sf_generators generators = global_generators;
        struct articulation *modulators = NULL;

        if (parse_soundfont_generators(soundfont, i, NULL, &generators))
        {
            if (i > preset->bag_ndx)
                WARN("Ignoring preset zone without an instrument\n");
            else if (SUCCEEDED(hr = parse_soundfont_preset_modulators(soundfont, i, NULL, &global_modulators)))
                global_generators = generators;
            continue;
        }

        if (SUCCEEDED(hr = parse_soundfont_preset_modulators(soundfont, i, global_modulators, &modulators)))
        {
            UINT instrument = generators.amount[SF_GEN_INSTRUMENT].value;
            hr = instrument_add_soundfont_instrument(This, soundfont, instrument, &generators, modulators);
        }

        free(modulators);
    }
    free(global_modulators);

    if (FAILED(hr))
    {
        IDirectMusicInstrument_Release(iface);
        return hr;
    }

    if (TRACE_ON(dmusic))
    {
        struct articulation *articulation;
        struct region *region;
        UINT i;

        TRACE("Created DirectMusicInstrument %p\n", This);
        TRACE(" - header:\n");
        TRACE("    - regions: %ld\n", This->header.cRegions);
        TRACE("    - locale: {bank: %#lx, instrument: %#lx} (patch %#lx)\n",
                This->header.Locale.ulBank, This->header.Locale.ulInstrument,
                MIDILOCALE2Patch(&This->header.Locale));
        if (desc->dwValidData & DMUS_OBJ_OBJECT) TRACE(" - guid: %s\n", debugstr_dmguid(&desc->guidObject));
        if (desc->dwValidData & DMUS_OBJ_NAME) TRACE(" - name: %s\n", debugstr_w(desc->wszName));

        TRACE(" - regions:\n");
        LIST_FOR_EACH_ENTRY(region, &This->regions, struct region, entry)
        {
            TRACE("   - region:\n");
            TRACE("     - header: {key: %u - %u, vel: %u - %u, options: %#x, group: %#x}\n",
                    region->header.RangeKey.usLow, region->header.RangeKey.usHigh,
                    region->header.RangeVelocity.usLow, region->header.RangeVelocity.usHigh,
                    region->header.fusOptions, region->header.usKeyGroup);
            TRACE("     - wave_link: {options: %#x, group: %u, channel: %lu, index: %lu}\n",
                    region->wave_link.fusOptions, region->wave_link.usPhaseGroup,
                    region->wave_link.ulChannel, region->wave_link.ulTableIndex);
            TRACE("     - wave_sample: {size: %lu, unity_note: %u, fine_tune: %d, attenuation: %ld, options: %#lx, loops: %lu}\n",
                    region->wave_sample.cbSize, region->wave_sample.usUnityNote,
                    region->wave_sample.sFineTune, region->wave_sample.lAttenuation,
                    region->wave_sample.fulOptions, region->wave_sample.cSampleLoops);
            for (i = 0; i < region->wave_sample.cSampleLoops; i++)
                TRACE("     - wave_loop[%u]: {size: %lu, type: %lu, start: %lu, length: %lu}\n", i,
                        region->wave_loop.cbSize, region->wave_loop.ulType,
                        region->wave_loop.ulStart, region->wave_loop.ulLength);
            TRACE("     - articulations:\n");
            LIST_FOR_EACH_ENTRY(articulation, &region->articulations, struct articulation, entry)
            {
                for (i = 0; i < articulation->list.cConnections; i++)
                {
                    CONNECTION *conn = articulation->connections + i;
                    TRACE("         - connection[%u]: %s\n", i, debugstr_connection(conn));
                }
            }
        }

        TRACE(" - articulations:\n");
        LIST_FOR_EACH_ENTRY(articulation, &This->articulations, struct articulation, entry)
        {
            for (i = 0; i < articulation->list.cConnections; i++)
            {
                CONNECTION *conn = articulation->connections + i;
                TRACE("     - connection[%u]: %s\n", i, debugstr_connection(conn));
            }
        }
    }

    *ret_iface = iface;
    return S_OK;
}

static void write_articulation_download(struct list *articulations, ULONG *offsets,
        BYTE **ptr, UINT index, DWORD *first, UINT *end)
{
    DMUS_ARTICULATION2 *dmus_articulation2 = NULL;
    struct articulation *articulation;
    CONNECTIONLIST *list;
    UINT size;

    LIST_FOR_EACH_ENTRY(articulation, articulations, struct articulation, entry)
    {
        if (dmus_articulation2) dmus_articulation2->ulNextArtIdx = index;
        else *first = index;

        offsets[index++] = sizeof(DMUS_DOWNLOADINFO) + *ptr - (BYTE *)offsets;
        dmus_articulation2 = (DMUS_ARTICULATION2 *)*ptr;
        (*ptr) += sizeof(DMUS_ARTICULATION2);

        dmus_articulation2->ulArtIdx = index;
        dmus_articulation2->ulFirstExtCkIdx = 0;
        dmus_articulation2->ulNextArtIdx = 0;

        size = articulation->list.cConnections * sizeof(CONNECTION);
        offsets[index++] = sizeof(DMUS_DOWNLOADINFO) + *ptr - (BYTE *)offsets;
        list = (CONNECTIONLIST *)*ptr;
        (*ptr) += sizeof(CONNECTIONLIST) + size;

        *list = articulation->list;
        memcpy(list + 1, articulation->connections, size);
    }

    *end = index;
}

struct download_buffer
{
    DMUS_DOWNLOADINFO info;
    ULONG offsets[];
};

C_ASSERT(sizeof(struct download_buffer) == offsetof(struct download_buffer, offsets[0]));

HRESULT instrument_download_to_port(IDirectMusicInstrument *iface, IDirectMusicPortDownload *port,
        IDirectMusicDownloadedInstrument **downloaded)
{
    struct instrument *This = impl_from_IDirectMusicInstrument(iface);
    struct articulation *articulation;
    struct download_buffer *buffer;
    IDirectMusicDownload *download;
    DWORD size, offset_count;
    struct region *region;
    IDirectMusicObject *wave;
    HRESULT hr;

    if (This->download) goto done;

    size = sizeof(DMUS_DOWNLOADINFO);
    size += sizeof(ULONG) + sizeof(DMUS_INSTRUMENT);
    offset_count = 1;

    LIST_FOR_EACH_ENTRY(articulation, &This->articulations, struct articulation, entry)
    {
        size += sizeof(ULONG) + sizeof(DMUS_ARTICULATION2);
        size += sizeof(ULONG) + sizeof(CONNECTIONLIST);
        size += articulation->list.cConnections * sizeof(CONNECTION);
        offset_count += 2;
    }

    LIST_FOR_EACH_ENTRY(region, &This->regions, struct region, entry)
    {
        size += sizeof(ULONG) + sizeof(DMUS_REGION);
        offset_count++;

        LIST_FOR_EACH_ENTRY(articulation, &region->articulations, struct articulation, entry)
        {
            size += sizeof(ULONG) + sizeof(DMUS_ARTICULATION2);
            size += sizeof(ULONG) + sizeof(CONNECTIONLIST);
            size += articulation->list.cConnections * sizeof(CONNECTION);
            offset_count += 2;
        }
    }

    if (FAILED(hr = IDirectMusicPortDownload_AllocateBuffer(port, size, &download))) return hr;

    if (SUCCEEDED(hr = IDirectMusicDownload_GetBuffer(download, (void **)&buffer, &size))
            && SUCCEEDED(hr = IDirectMusicPortDownload_GetDLId(port, &buffer->info.dwDLId, 1)))
    {
        BYTE *ptr = (BYTE *)&buffer->offsets[offset_count];
        DMUS_INSTRUMENT *dmus_instrument;
        DMUS_REGION *dmus_region = NULL;
        UINT index = 0;

        buffer->info.dwDLType = DMUS_DOWNLOADINFO_INSTRUMENT2;
        buffer->info.dwNumOffsetTableEntries = offset_count;
        buffer->info.cbSize = size;

        buffer->offsets[index++] = ptr - (BYTE *)buffer;
        dmus_instrument = (DMUS_INSTRUMENT *)ptr;
        ptr += sizeof(DMUS_INSTRUMENT);

        dmus_instrument->ulPatch = MIDILOCALE2Patch(&This->header.Locale);
        dmus_instrument->ulFirstRegionIdx = 0;
        dmus_instrument->ulCopyrightIdx = 0;
        dmus_instrument->ulGlobalArtIdx = 0;

        write_articulation_download(&This->articulations, buffer->offsets, &ptr, index,
                &dmus_instrument->ulGlobalArtIdx, &index);

        LIST_FOR_EACH_ENTRY(region, &This->regions, struct region, entry)
        {
            if (dmus_region) dmus_region->ulNextRegionIdx = index;
            else dmus_instrument->ulFirstRegionIdx = index;

            buffer->offsets[index++] = ptr - (BYTE *)buffer;
            dmus_region = (DMUS_REGION *)ptr;
            ptr += sizeof(DMUS_REGION);

            dmus_region->RangeKey = region->header.RangeKey;
            dmus_region->RangeVelocity = region->header.RangeVelocity;
            dmus_region->fusOptions = region->header.fusOptions;
            dmus_region->usKeyGroup = region->header.usKeyGroup;
            dmus_region->ulRegionArtIdx = 0;
            dmus_region->ulNextRegionIdx = 0;
            dmus_region->ulFirstExtCkIdx = 0;
            dmus_region->WaveLink = region->wave_link;
            dmus_region->WSMP = region->wave_sample;
            dmus_region->WLOOP[0] = region->wave_loop;

            if (SUCCEEDED(hr = collection_get_wave(This->collection, region->wave_link.ulTableIndex, &wave)))
            {
                hr = wave_download_to_port(wave, port, &dmus_region->WaveLink.ulTableIndex);
                IDirectMusicObject_Release(wave);
            }
            if (FAILED(hr)) goto failed;

            write_articulation_download(&region->articulations, buffer->offsets, &ptr, index,
                    &dmus_region->ulRegionArtIdx, &index);
        }

        if (FAILED(hr = IDirectMusicPortDownload_Download(port, download))) goto failed;
    }

    This->download = download;

done:
    *downloaded = &This->IDirectMusicDownloadedInstrument_iface;
    IDirectMusicDownloadedInstrument_AddRef(*downloaded);
    return S_OK;

failed:
    WARN("Failed to download instrument to port, hr %#lx\n", hr);
    IDirectMusicDownload_Release(download);
    return hr;
}

HRESULT instrument_unload_from_port(IDirectMusicDownloadedInstrument *iface, IDirectMusicPortDownload *port)
{
    struct instrument *This = impl_from_IDirectMusicDownloadedInstrument(iface);
    struct download_buffer *buffer;
    DWORD size;
    HRESULT hr;

    if (!This->download) return DMUS_E_NOT_DOWNLOADED_TO_PORT;

    if (FAILED(hr = IDirectMusicPortDownload_Unload(port, This->download)))
        WARN("Failed to unload instrument download buffer, hr %#lx\n", hr);
    else if (SUCCEEDED(hr = IDirectMusicDownload_GetBuffer(This->download, (void **)&buffer, &size)))
    {
        IDirectMusicDownload *wave_download;
        DMUS_INSTRUMENT *instrument;
        BYTE *ptr = (BYTE *)buffer;
        DMUS_REGION *region;
        UINT index;

        instrument = (DMUS_INSTRUMENT *)(ptr + buffer->offsets[0]);
        for (index = instrument->ulFirstRegionIdx; index; index = region->ulNextRegionIdx)
        {
            region = (DMUS_REGION *)(ptr + buffer->offsets[index]);

            if (FAILED(hr = IDirectMusicPortDownload_GetBuffer(port, region->WaveLink.ulTableIndex, &wave_download)))
                WARN("Failed to get wave download with id %#lx, hr %#lx\n", region->WaveLink.ulTableIndex, hr);
            else
            {
                if (FAILED(hr = IDirectMusicPortDownload_Unload(port, wave_download)))
                    WARN("Failed to unload wave download buffer, hr %#lx\n", hr);
                IDirectMusicDownload_Release(wave_download);
            }
        }
    }

    IDirectMusicDownload_Release(This->download);
    This->download = NULL;

    return hr;
}
