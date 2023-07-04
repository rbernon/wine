/*
 * Unit tests for mf.dll.
 *
 * Copyright 2017 Nikolay Sivov
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
#include <string.h>
#include <float.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"

#include "d3d9.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
#include "mmdeviceapi.h"
#include "uuids.h"
#include "wmcodecdsp.h"

#include "mf_test.h"

#include "wine/test.h"

static void test_bloodhunt(void)
{
    const WCHAR *url = L"file://Z:/home/rbernon/Code/build-wine/test.mp4";
    IMFAttributes *attributes;
    IMFMediaSession *session;
    IMFSourceResolver *resolver;
    MF_OBJECT_TYPE type;
    IUnknown *object;
    IMFMediaSource *source;
    IMFPresentationDescriptor *presentation;
    DWORD stream_count, type_count;
    BOOL selected;
    IMFStreamDescriptor *audio_stream, *video_stream;
    IMFMediaTypeHandler *handler;
    GUID major, subtype, format;
    UINT32 protected, bitrate, length, num_channels, rate, bits;
    IMFMediaType *current_type, *video_type, *audio_type;
    IMFSampleGrabberSinkCallback *audio_grabber, *video_grabber;
    UINT64 frame_rate, frame_size, duration;
    WCHAR *lang, *name;
    IMFTopology *topology;
    IMFTopologyNode *src_node, *sink_node;
    IMFActivate *activate;
    IMFAsyncCallback *callback;
    PROPVARIANT propvar;
    IMFClock *clock;
    IMFRateControl *rate_control;
    IMFRateSupport *rate_support;
    float playback_rate;
    MFCLOCK_STATE clock_state;
    UINT time = 0;
    HRESULT hr;

CloseHandle(CreateThread(NULL, 0, test_thread, NULL, 0, NULL));
CloseHandle(CreateThread(NULL, 0, test_thread, NULL, 0, NULL));
CloseHandle(CreateThread(NULL, 0, test_thread, NULL, 0, NULL));
CloseHandle(CreateThread(NULL, 0, test_thread, NULL, 0, NULL));
CloseHandle(CreateThread(NULL, 0, test_thread, NULL, 0, NULL));
CloseHandle(CreateThread(NULL, 0, test_thread, NULL, 0, NULL));
CloseHandle(CreateThread(NULL, 0, test_thread, NULL, 0, NULL));
CloseHandle(CreateThread(NULL, 0, test_thread, NULL, 0, NULL));

    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    ok( hr == S_OK, "got hr %#lx\n", hr );

    audio_grabber = create_test_grabber_callback();
    video_grabber = create_test_grabber_callback();


hr = MFCreateAttributes( &attributes, 2 );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = MFCreateMediaSession( attributes, &session );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFAttributes_Release( attributes );
ok( hr == S_OK, "got hr %#lx\n", hr );


if (strcmp( winetest_platform, "wine" )) url = L"file://Y:/Games/Bloodhunt/Tiger/Content/Movies/V_INTRO_INTRO_01.mp4";

hr = MFCreateSourceResolver( &resolver );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFSourceResolver_CreateObjectFromURL( resolver, url, 1, NULL, &type, &object );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFSourceResolver_Release( resolver );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IUnknown_QueryInterface( object, &IID_IMFMediaSource, (void **)&source );
ok( hr == S_OK, "got hr %#lx\n", hr );
IUnknown_Release( object );

hr = IMFMediaSource_CreatePresentationDescriptor( source, &presentation );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFPresentationDescriptor_GetStreamDescriptorCount( presentation, &stream_count );
ok( hr == S_OK, "got hr %#lx\n", hr );


hr = IMFPresentationDescriptor_GetStreamDescriptorByIndex( presentation, 0, &selected, &video_stream );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFPresentationDescriptor_DeselectStream( presentation, 0 );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = IMFStreamDescriptor_GetMediaTypeHandler( video_stream, &handler );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaTypeHandler_GetMajorType( handler, &major );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFStreamDescriptor_GetUINT32( video_stream, &MF_SD_PROTECTED, &protected );
ok( hr == MF_E_ATTRIBUTENOTFOUND, "got hr %#lx\n", hr );

hr = IMFMediaTypeHandler_GetMediaTypeCount( handler, &type_count );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaTypeHandler_GetCurrentMediaType( handler, &current_type );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaTypeHandler_GetMediaTypeByIndex( handler, 0, &current_type );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetGUID( current_type, &MF_MT_SUBTYPE, &subtype );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetGUID( current_type, &MF_MT_MAJOR_TYPE, &major );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetGUID( current_type, &MF_MT_SUBTYPE, &subtype );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = MFCreateMediaType( &video_type );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_SetUINT32( video_type, &MF_MT_ALL_SAMPLES_INDEPENDENT, 1 );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_SetGUID( video_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_SetGUID( video_type, &MF_MT_SUBTYPE, &MFVideoFormat_YUY2 );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = IMFMediaType_GetGUID( current_type, &MF_MT_SUBTYPE, &subtype );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetUINT32( current_type, &MF_MT_AVG_BITRATE, &bitrate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetUINT64( current_type, &MF_MT_FRAME_RATE, &frame_rate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetUINT64( current_type, &MF_MT_FRAME_RATE_RANGE_MIN, &frame_rate );
ok( hr == MF_E_ATTRIBUTENOTFOUND, "got hr %#lx\n", hr );
hr = IMFMediaType_GetUINT64( current_type, &MF_MT_FRAME_RATE_RANGE_MAX, &frame_rate );
ok( hr == MF_E_ATTRIBUTENOTFOUND, "got hr %#lx\n", hr );
hr = IMFMediaType_GetUINT64( current_type, &MF_MT_FRAME_SIZE, &frame_size );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetGUID( current_type, &MF_MT_AM_FORMAT_TYPE, &format );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = IMFMediaTypeHandler_SetCurrentMediaType( handler, current_type );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFMediaTypeHandler_Release( handler );
IMFMediaType_Release( current_type );

hr = IMFPresentationDescriptor_SelectStream( presentation, 0 );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFStreamDescriptor_GetAllocatedString( video_stream, &MF_SD_LANGUAGE, &lang, &length );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFStreamDescriptor_GetAllocatedString( video_stream, &MF_SD_STREAM_NAME, &name, &length );
ok( hr == MF_E_ATTRIBUTENOTFOUND, "got hr %#lx\n", hr );


if (0) {
hr = IMFPresentationDescriptor_GetStreamDescriptorByIndex( presentation, 0, &selected, &audio_stream );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFPresentationDescriptor_DeselectStream( presentation, 0 );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFStreamDescriptor_GetMediaTypeHandler( audio_stream, &handler );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaTypeHandler_GetMajorType( handler, &major );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFStreamDescriptor_GetUINT32( audio_stream, &MF_SD_PROTECTED, &protected );
ok( hr == MF_E_ATTRIBUTENOTFOUND, "got hr %#lx\n", hr );
hr = IMFMediaTypeHandler_GetMediaTypeCount( handler, &type_count );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaTypeHandler_GetCurrentMediaType( handler, &current_type );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaTypeHandler_GetMediaTypeByIndex( handler, 0, &current_type );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetGUID( current_type, &MF_MT_SUBTYPE, &subtype );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetGUID( current_type, &MF_MT_MAJOR_TYPE, &major );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetGUID( current_type, &MF_MT_SUBTYPE, &subtype );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = MFCreateMediaType( &audio_type );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_SetUINT32( audio_type, &MF_MT_ALL_SAMPLES_INDEPENDENT, 1 );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_SetGUID( audio_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_SetGUID( audio_type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_SetUINT32( audio_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetUINT32( current_type, &MF_MT_AUDIO_NUM_CHANNELS, &num_channels );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_SetUINT32( audio_type, &MF_MT_AUDIO_NUM_CHANNELS, num_channels );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetUINT32( current_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_SetUINT32( audio_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, rate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetUINT32( current_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &bits );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetUINT32( current_type, &MF_MT_AUDIO_NUM_CHANNELS, &num_channels );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaType_GetUINT32( current_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = IMFMediaTypeHandler_SetCurrentMediaType( handler, current_type );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFMediaTypeHandler_Release( handler );
IMFMediaType_Release( current_type );

hr = IMFPresentationDescriptor_SelectStream( presentation, 0 );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFStreamDescriptor_GetAllocatedString( audio_stream, &MF_SD_LANGUAGE, &lang, &length );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFStreamDescriptor_GetAllocatedString( audio_stream, &MF_SD_STREAM_NAME, &name, &length );
ok( hr == MF_E_ATTRIBUTENOTFOUND, "got hr %#lx\n", hr );
}

hr = MFCreateTopology( &topology );
ok( hr == S_OK, "got hr %#lx\n", hr );

if (0) {
hr = MFCreateTopologyNode( 0, &sink_node );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = MFCreateSampleGrabberSinkActivate( audio_type, audio_grabber, &activate );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFMediaType_Release( audio_type );

IMFActivate_SetUINT32(activate, &MF_SAMPLEGRABBERSINK_IGNORE_CLOCK, 1);

hr = IMFTopologyNode_SetObject( sink_node, (IUnknown *)activate );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFActivate_Release( activate );
hr = IMFTopologyNode_SetUINT32( sink_node, &MF_TOPONODE_STREAMID, 0 );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFTopology_AddNode( topology, sink_node );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = MFCreateTopologyNode( 1, &src_node );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFTopologyNode_SetUnknown( src_node, &MF_TOPONODE_SOURCE, (IUnknown *)source );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFTopologyNode_SetUnknown( src_node, &MF_TOPONODE_PRESENTATION_DESCRIPTOR, (IUnknown *)presentation );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFTopologyNode_SetUnknown( src_node, &MF_TOPONODE_STREAM_DESCRIPTOR, (IUnknown *)audio_stream );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFStreamDescriptor_Release( audio_stream );
hr = IMFTopology_AddNode( topology, src_node );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = IMFTopologyNode_ConnectOutput( src_node, 0, sink_node, 0 );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFTopologyNode_Release( src_node );
IMFTopologyNode_Release( sink_node );
}


hr = MFCreateTopologyNode( 0, &sink_node );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = MFCreateSampleGrabberSinkActivate( video_type, video_grabber, &activate );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFMediaType_Release( video_type );

IMFActivate_SetUINT32(activate, &MF_SAMPLEGRABBERSINK_IGNORE_CLOCK, 1);

hr = IMFTopologyNode_SetObject( sink_node, (IUnknown *)activate );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFActivate_Release( activate );
hr = IMFTopologyNode_SetUINT32( sink_node, &MF_TOPONODE_STREAMID, 0 );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFTopology_AddNode( topology, sink_node );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = MFCreateTopologyNode( 1, &src_node );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFTopologyNode_SetUnknown( src_node, &MF_TOPONODE_SOURCE, (IUnknown *)source );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFTopologyNode_SetUnknown( src_node, &MF_TOPONODE_PRESENTATION_DESCRIPTOR, (IUnknown *)presentation );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFTopologyNode_SetUnknown( src_node, &MF_TOPONODE_STREAM_DESCRIPTOR, (IUnknown *)video_stream );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFStreamDescriptor_Release( video_stream );
hr = IMFTopology_AddNode( topology, src_node );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = IMFTopologyNode_ConnectOutput( src_node, 0, sink_node, 0 );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFTopologyNode_Release( src_node );
IMFTopologyNode_Release( sink_node );

IMFMediaSource_Release( source );

hr = IMFPresentationDescriptor_GetUINT64( presentation, &MF_PD_DURATION, &duration );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFMediaSession_SetTopology( session, 1, topology );
ok( hr == S_OK, "got hr %#lx\n", hr );
IMFTopology_Release( topology );


callback = create_test_callback(TRUE);
PropVariantInit( &propvar );
hr = wait_media_event( session, callback, MESessionTopologySet, INFINITE, &propvar );
ok( hr == S_OK, "got hr %#lx\n", hr );

PropVariantClear( &propvar );
hr = wait_media_event( session, callback, MESessionNotifyPresentationTime, INFINITE, &propvar );
ok( hr == S_OK, "got hr %#lx\n", hr );
PropVariantClear( &propvar );
hr = wait_media_event( session, callback, MESessionCapabilitiesChanged, INFINITE, &propvar );
ok( hr == S_OK, "got hr %#lx\n", hr );
PropVariantClear( &propvar );
hr = wait_media_event( session, callback, MESessionTopologyStatus, INFINITE, &propvar );
ok( hr == S_OK, "got hr %#lx\n", hr );
PropVariantClear( &propvar );

hr = IMFMediaSession_GetClock( session, &clock );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = IMFClock_QueryInterface( clock, &IID_IMFPresentationClock, (void **)&presentation_clock );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = MFGetService( (IUnknown *)session, &MF_RATE_CONTROL_SERVICE, &IID_IMFRateControl, (void **)&rate_control );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFRateControl_GetRate( rate_control, NULL, &playback_rate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = MFGetService( (IUnknown *)session, &MF_RATE_CONTROL_SERVICE, &IID_IMFRateSupport, (void **)&rate_support );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFRateSupport_IsRateSupported( rate_support, 1, 0., NULL );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFRateSupport_GetSlowestRate( rate_support, 0, 1, &playback_rate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFRateSupport_GetFastestRate( rate_support, 0, 1, &playback_rate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFRateSupport_GetSlowestRate( rate_support, 1, 1, &playback_rate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFRateSupport_GetFastestRate( rate_support, 1, 1, &playback_rate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFRateSupport_GetSlowestRate( rate_support, 0, 0, &playback_rate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFRateSupport_GetFastestRate( rate_support, 0, 0, &playback_rate );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFRateSupport_GetSlowestRate( rate_support, 1, 0, &playback_rate );
ok( hr == MF_E_REVERSE_UNSUPPORTED, "got hr %#lx\n", hr );
hr = IMFRateSupport_GetFastestRate( rate_support, 1, 0, &playback_rate );
ok( hr == MF_E_REVERSE_UNSUPPORTED, "got hr %#lx\n", hr );
hr = IMFRateSupport_IsRateSupported( rate_support, 0, 0., NULL );
ok( hr == S_OK, "got hr %#lx\n", hr );

hr = IMFPresentationClock_GetState( presentation_clock, 0, &clock_state );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFPresentationClock_GetState( presentation_clock, 0, &clock_state );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFPresentationClock_GetState( presentation_clock, 0, &clock_state );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFPresentationClock_GetState( presentation_clock, 0, &clock_state );
ok( hr == S_OK, "got hr %#lx\n", hr );
hr = IMFPresentationClock_GetState( presentation_clock, 0, &clock_state );
ok( hr == S_OK, "got hr %#lx\n", hr );

PropVariantInit( &propvar );
hr = IMFMediaSession_Start( session, &GUID_NULL, &propvar );
ok( hr == S_OK, "got hr %#lx\n", hr );

PropVariantInit( &propvar );
hr = wait_media_event( session, callback, MESessionStarted, INFINITE, &propvar );
time -= GetTickCount();
ok( hr == S_OK, "got hr %#lx\n", hr );
PropVariantClear( &propvar );

PropVariantInit( &propvar );
hr = wait_media_event( session, callback, MEEndOfPresentation, INFINITE, &propvar );
time += GetTickCount();
ok( hr == S_OK, "got hr %#lx\n", hr );
PropVariantClear( &propvar );

IMFMediaSession_Shutdown( session );
IMFMediaSession_Release( session );

ok( 0, "took %u.%us, duration %u.%us\n", time / 1000, time % 1000, (UINT)duration / 10000000, ((UINT)duration / 10000) % 1000);
}

START_TEST(bloodhunt)
{
    init_functions();
    test_bloodhunt();
}
