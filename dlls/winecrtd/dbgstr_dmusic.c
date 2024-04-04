/*
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#define WINBASEAPI
#include "windef.h"
#include "winbase.h"

#define COBJMACROS
#include "objbase.h"

#undef DEFINE_GUID
#undef EXTERN_GUID

#define EXTERN_GUID DEFINE_GUID
#define GUID_DEFINED

#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

#include "dmusici.h"
#include "dmusicf.h"
#include "dmusics.h"

#include "wine/debug.h"

struct guid_def
{
    const GUID *guid;
    const char *name;
};

static int guid_def_cmp(const void *a, const void *b)
{
    const struct guid_def *a_def = a, *b_def = b;
    return memcmp(a_def->guid, b_def->guid, sizeof(GUID));
}

struct sort_guid_defs_params
{
    struct guid_def *array;
    UINT count;
};

static BOOL WINAPI sort_guid_defs(INIT_ONCE *once, void *param, void **context)
{
    struct sort_guid_defs_params *params = param;
    qsort(params->array, params->count, sizeof(struct guid_def), guid_def_cmp);
    return TRUE;
}

const char *debugstr_dmguid(const GUID *guid)
{
    static struct guid_def guid_defs[] =
    {
#define X(g) { &(g), #g }
        X(CLSID_AudioVBScript),
        X(CLSID_DirectMusic),
        X(CLSID_DirectMusicAudioPathConfig),
        X(CLSID_DirectMusicAuditionTrack),
        X(CLSID_DirectMusicBand),
        X(CLSID_DirectMusicBandTrack),
        X(CLSID_DirectMusicChordMapTrack),
        X(CLSID_DirectMusicChordMap),
        X(CLSID_DirectMusicChordTrack),
        X(CLSID_DirectMusicCollection),
        X(CLSID_DirectMusicCommandTrack),
        X(CLSID_DirectMusicComposer),
        X(CLSID_DirectMusicContainer),
        X(CLSID_DirectMusicGraph),
        X(CLSID_DirectMusicLoader),
        X(CLSID_DirectMusicLyricsTrack),
        X(CLSID_DirectMusicMarkerTrack),
        X(CLSID_DirectMusicMelodyFormulationTrack),
        X(CLSID_DirectMusicMotifTrack),
        X(CLSID_DirectMusicMuteTrack),
        X(CLSID_DirectMusicParamControlTrack),
        X(CLSID_DirectMusicPatternTrack),
        X(CLSID_DirectMusicPerformance),
        X(CLSID_DirectMusicScript),
        X(CLSID_DirectMusicScriptAutoImpSegment),
        X(CLSID_DirectMusicScriptAutoImpPerformance),
        X(CLSID_DirectMusicScriptAutoImpSegmentState),
        X(CLSID_DirectMusicScriptAutoImpAudioPathConfig),
        X(CLSID_DirectMusicScriptAutoImpAudioPath),
        X(CLSID_DirectMusicScriptAutoImpSong),
        X(CLSID_DirectMusicScriptSourceCodeLoader),
        X(CLSID_DirectMusicScriptTrack),
        X(CLSID_DirectMusicSection),
        X(CLSID_DirectMusicSegment),
        X(CLSID_DirectMusicSegmentState),
        X(CLSID_DirectMusicSegmentTriggerTrack),
        X(CLSID_DirectMusicSegTriggerTrack),
        X(CLSID_DirectMusicSeqTrack),
        X(CLSID_DirectMusicSignPostTrack),
        X(CLSID_DirectMusicSong),
        X(CLSID_DirectMusicStyle),
        X(CLSID_DirectMusicStyleTrack),
        X(CLSID_DirectMusicSynth),
        X(CLSID_DirectMusicSynthSink),
        X(CLSID_DirectMusicSysExTrack),
        X(CLSID_DirectMusicTemplate),
        X(CLSID_DirectMusicTempoTrack),
        X(CLSID_DirectMusicTimeSigTrack),
        X(CLSID_DirectMusicWaveTrack),
        X(CLSID_DirectSoundWave),
        /* IIDs */
        X(IID_IDirectMusic),
        X(IID_IDirectMusic2),
        X(IID_IDirectMusic8),
        X(IID_IDirectMusicAudioPath),
        X(IID_IDirectMusicBand),
        X(IID_IDirectMusicBuffer),
        X(IID_IDirectMusicChordMap),
        X(IID_IDirectMusicCollection),
        X(IID_IDirectMusicComposer),
        X(IID_IDirectMusicContainer),
        X(IID_IDirectMusicDownload),
        X(IID_IDirectMusicDownloadedInstrument),
        X(IID_IDirectMusicGetLoader),
        X(IID_IDirectMusicGraph),
        X(IID_IDirectMusicInstrument),
        X(IID_IDirectMusicLoader),
        X(IID_IDirectMusicLoader8),
        X(IID_IDirectMusicObject),
        X(IID_IDirectMusicPatternTrack),
        X(IID_IDirectMusicPerformance),
        X(IID_IDirectMusicPerformance2),
        X(IID_IDirectMusicPerformance8),
        X(IID_IDirectMusicPort),
        X(IID_IDirectMusicPortDownload),
        X(IID_IDirectMusicScript),
        X(IID_IDirectMusicSegment),
        X(IID_IDirectMusicSegment2),
        X(IID_IDirectMusicSegment8),
        X(IID_IDirectMusicSegmentState),
        X(IID_IDirectMusicSegmentState8),
        X(IID_IDirectMusicStyle),
        X(IID_IDirectMusicStyle8),
        X(IID_IDirectMusicSynth),
        X(IID_IDirectMusicSynth8),
        X(IID_IDirectMusicSynthSink),
        X(IID_IDirectMusicThru),
        X(IID_IDirectMusicTool),
        X(IID_IDirectMusicTool8),
        X(IID_IDirectMusicTrack),
        X(IID_IDirectMusicTrack8),
        X(IID_IUnknown),
        X(IID_IPersistStream),
        X(IID_IStream),
        X(IID_IClassFactory),
        /* GUIDs */
        X(GUID_DirectMusicAllTypes),
        X(GUID_NOTIFICATION_CHORD),
        X(GUID_NOTIFICATION_COMMAND),
        X(GUID_NOTIFICATION_MEASUREANDBEAT),
        X(GUID_NOTIFICATION_PERFORMANCE),
        X(GUID_NOTIFICATION_RECOMPOSE),
        X(GUID_NOTIFICATION_SEGMENT),
        X(GUID_BandParam),
        X(GUID_ChordParam),
        X(GUID_CommandParam),
        X(GUID_CommandParam2),
        X(GUID_CommandParamNext),
        X(GUID_IDirectMusicBand),
        X(GUID_IDirectMusicChordMap),
        X(GUID_IDirectMusicStyle),
        X(GUID_MuteParam),
        X(GUID_Play_Marker),
        X(GUID_RhythmParam),
        X(GUID_TempoParam),
        X(GUID_TimeSignature),
        X(GUID_Valid_Start_Time),
        X(GUID_Clear_All_Bands),
        X(GUID_ConnectToDLSCollection),
        X(GUID_Disable_Auto_Download),
        X(GUID_DisableTempo),
        X(GUID_DisableTimeSig),
        X(GUID_Download),
        X(GUID_DownloadToAudioPath),
        X(GUID_Enable_Auto_Download),
        X(GUID_EnableTempo),
        X(GUID_EnableTimeSig),
        X(GUID_IgnoreBankSelectForGM),
        X(GUID_SeedVariations),
        X(GUID_StandardMIDIFile),
        X(GUID_Unload),
        X(GUID_UnloadFromAudioPath),
        X(GUID_Variations),
        X(GUID_PerfMasterTempo),
        X(GUID_PerfMasterVolume),
        X(GUID_PerfMasterGrooveLevel),
        X(GUID_PerfAutoDownload),
        X(GUID_DefaultGMCollection),
        X(GUID_Synth_Default),
        X(GUID_Buffer_Reverb),
        X(GUID_Buffer_EnvReverb),
        X(GUID_Buffer_Stereo),
        X(GUID_Buffer_3D_Dry),
        X(GUID_Buffer_Mono),
        X(GUID_DMUS_PROP_GM_Hardware),
        X(GUID_DMUS_PROP_GS_Capable),
        X(GUID_DMUS_PROP_GS_Hardware),
        X(GUID_DMUS_PROP_DLS1),
        X(GUID_DMUS_PROP_DLS2),
        X(GUID_DMUS_PROP_Effects),
        X(GUID_DMUS_PROP_INSTRUMENT2),
        X(GUID_DMUS_PROP_LegacyCaps),
        X(GUID_DMUS_PROP_MemorySize),
        X(GUID_DMUS_PROP_SampleMemorySize),
        X(GUID_DMUS_PROP_SamplePlaybackRate),
        X(GUID_DMUS_PROP_SetSynthSink),
        X(GUID_DMUS_PROP_SinkUsesDSound),
        X(GUID_DMUS_PROP_SynthSink_DSOUND),
        X(GUID_DMUS_PROP_SynthSink_WAVE),
        X(GUID_DMUS_PROP_Volume),
        X(GUID_DMUS_PROP_WavesReverb),
        X(GUID_DMUS_PROP_WriteLatency),
        X(GUID_DMUS_PROP_WritePeriod),
        X(GUID_DMUS_PROP_XG_Capable),
        X(GUID_DMUS_PROP_XG_Hardware),
#undef X
    };
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;

    struct sort_guid_defs_params params = {.array = guid_defs, .count = ARRAY_SIZE(guid_defs)};
    struct guid_def *ret = NULL, tmp = {.guid = guid};

    if (!guid) return "(null)";
    InitOnceExecuteOnce( &once, sort_guid_defs, &params, NULL );

    ret = bsearch(&tmp, guid_defs, ARRAY_SIZE(guid_defs), sizeof(*guid_defs), guid_def_cmp);
    return ret ? wine_dbg_sprintf("%s", ret->name) : wine_dbgstr_guid(guid);
}
