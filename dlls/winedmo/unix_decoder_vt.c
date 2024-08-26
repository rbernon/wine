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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#if defined(ENABLE_FFMPEG) && defined(ENABLE_VIDEO_TOOLBOX)

#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>

#include "unix_decoder.h"

#include <VideoToolbox/VideoToolbox.h>

#define GetProcessInformation _GetProcessInformation
#define LoadResource _LoadResource
#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

static int is_start_code( const uint8_t *data )
{
    return !data[0] && !data[1] && data[2] == 1;
}

struct nal
{
    const uint8_t *ptr;
    unsigned int size;
};

static int nal_type( struct nal nal )
{
    return *nal.ptr & 0x1f;
}

static void nal_copy( struct nal *dst, const struct nal *src )
{
    dst->ptr = realloc( (void *)dst->ptr, src->size );
    memcpy( (void *)dst->ptr, src->ptr, src->size );
    dst->size = src->size;
}

static void nal_free( struct nal *nal )
{
    free( (void *)nal->ptr );
    memset( nal, 0, sizeof(*nal) );
}

static int count_nals( const uint8_t *data, const uint8_t *end )
{
    size_t count = 0;
    if (data >= end) return 0;
    while (data + 3 <= end)
    {
        if (!is_start_code( data++ )) continue;
        data += 2;
        count++;
    }
    return count;
}

static void list_nals( const uint8_t *beg, const uint8_t *end, struct nal *nals )
{
    const uint8_t *data = beg;
    struct nal nal = {0};

    while (data + 3 <= end)
    {
        if (!is_start_code( data++ )) continue;
        data += 2;

        if (nal.ptr)
        {
            nal.size = data - 3 - nal.ptr;
            if (!data[-4]) nal.size--;
            *nals++ = nal;
        }

        nal.ptr = data;
    }

    if (!nal.ptr) nal.ptr = beg;
    nal.size = end - nal.ptr;
    *nals = nal;
}

struct vt_decoder
{
    struct decoder iface;
    CMVideoFormatDescriptionRef format;
    VTDecompressionSessionRef session;
    CVImageBufferRef output_frame;

    BOOL is_avc;
    size_t avc_size;
    uint8_t *avc_data;

    struct nal sps;
    struct nal pps;
    struct nal ext;
};

static struct vt_decoder *vt_decoder_from_decoder( struct decoder *iface )
{
    return CONTAINING_RECORD( iface, struct vt_decoder, iface );
}

static void free_pixel_buffer( void *opaque, uint8_t *data )
{
    CVPixelBufferUnlockBaseAddress( opaque, 0 );
    CVPixelBufferRelease( opaque );
}

static void vt_decoder_callback( void *context, void *source, OSStatus status, VTDecodeInfoFlags flags,
                                 CVImageBufferRef image, CMTime pts, CMTime duration )
{
    struct vt_decoder *decoder = context;
    CGSize frame_size;

    if (!image) return;

    CVPixelBufferRelease( decoder->output_frame );
    decoder->output_frame = CVPixelBufferRetain( image );

    frame_size = CVImageBufferGetDisplaySize( image );
    decoder->iface.context->sw_pix_fmt = AV_PIX_FMT_YUVJ420P;
    decoder->iface.context->width = frame_size.width;
    decoder->iface.context->height = frame_size.height;
    decoder->iface.context->coded_width = ((int)frame_size.width + 15) & ~15;
    decoder->iface.context->coded_height = ((int)frame_size.height + 15) & ~15;
}

static void dict_set_int( CFMutableDictionaryRef dict, CFStringRef key, int value )
{
    CFNumberRef number = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &value );
    CFDictionarySetValue( dict, key, number );
    CFRelease( number );
}

static void dict_set_data( CFMutableDictionaryRef dict, CFStringRef key, const void *data, size_t size )
{
    CFDataRef value = CFDataCreate( NULL, data, size );
    CFDictionarySetValue( dict, key, value );
    CFRelease( value );
}

static int decompression_session_create( struct vt_decoder *decoder, CFDictionaryRef extensions )
{
    VTDecompressionOutputCallbackRecord callback;
    CFMutableDictionaryRef attrs;
    OSStatus ret;

    if (extensions)
    {
        int width = decoder->iface.context->width, height = decoder->iface.context->height;
        ret = CMVideoFormatDescriptionCreate( kCFAllocatorDefault, kCMVideoCodecType_H264, width,
                                              height, extensions, &decoder->format );
        if (ret) ERR( "CMVideoFormatDescriptionCreate failed, ret %d\n", ret );
    }
    else if (!decoder->ext.ptr)
    {
        const uint8_t *ptrs[] = {decoder->sps.ptr, decoder->pps.ptr};
        size_t sizes[] = {decoder->sps.size, decoder->pps.size};
        ret = CMVideoFormatDescriptionCreateFromH264ParameterSets( kCFAllocatorDefault, 2, ptrs,
                                                                   sizes, 4, &decoder->format );
        if (ret) ERR( "CMVideoFormatDescriptionCreateFromH264ParameterSets failed, ret %d\n", ret );
        nal_free( &decoder->sps );
        nal_free( &decoder->pps );
    }
    else
    {
        const uint8_t *ptrs[] = {decoder->sps.ptr, decoder->ext.ptr, decoder->pps.ptr};
        size_t sizes[] = {decoder->sps.size, decoder->ext.size, decoder->pps.size};
        ret = CMVideoFormatDescriptionCreateFromH264ParameterSets( kCFAllocatorDefault, 3, ptrs,
                                                                   sizes, 4, &decoder->format );
        if (ret) ERR( "CMVideoFormatDescriptionCreateFromH264ParameterSets failed, ret %d\n", ret );
        nal_free( &decoder->sps );
        nal_free( &decoder->ext );
        nal_free( &decoder->pps );
    }

    attrs = CFDictionaryCreateMutable( kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    dict_set_int( attrs, kCVPixelBufferPixelFormatTypeKey, kCVPixelFormatType_420YpCbCr8Planar );

    callback.decompressionOutputCallback = vt_decoder_callback;
    callback.decompressionOutputRefCon = decoder;
    ret = VTDecompressionSessionCreate( kCFAllocatorDefault, decoder->format, extensions, attrs,
                                        &callback, &decoder->session );
    if (ret) ERR( "CMVideoFormatDescriptionCreateFromH264ParameterSets failed, ret %d\n", ret );
    CFRelease( attrs );

    return ret;
}

int vt_decoder_destroy( struct decoder *iface )
{
    struct vt_decoder *decoder = vt_decoder_from_decoder( iface );

    CVPixelBufferRelease( decoder->output_frame );
    if (decoder->session)
    {
        VTDecompressionSessionInvalidate( decoder->session );
        CFRelease( decoder->session );
    }

    free( decoder );
    return 0;
}

int vt_decoder_process_input( struct decoder *iface, AVPacket *packet )
{
    struct vt_decoder *decoder = vt_decoder_from_decoder( iface );
    CMSampleBufferRef sample;
    CMBlockBufferRef buffer;
    const uint8_t *avc_data;
    struct nal *nals;
    int i, nal_count;
    size_t avc_size;
    uint8_t *tmp;
    OSStatus ret;

    TRACE( "packet %p, size %#x\n", packet, packet ? packet->size : 0 );

    if (decoder->output_frame) return AVERROR(EAGAIN);
    if (!packet) return 0;

    if (decoder->is_avc)
    {
        avc_data = packet->data;
        avc_size = packet->size;
    }
    else
    {
        nal_count = count_nals( packet->data, packet->data + packet->size );
        if (!nal_count || !(nals = malloc( nal_count * sizeof(*nals) ))) return -1;

        /* lookup NALs in the packet */
        list_nals( packet->data, packet->data + packet->size, nals );

        for (i = 0, avc_size = nal_count * 4; i < nal_count; i++)
        {
            int type = nal_type( nals[i] );
            if (type == 7 /* SPS */) nal_copy( &decoder->sps, nals + i );
            if (type == 13 /* SPSEXT */) nal_copy( &decoder->ext, nals + i );
            if (type == 8 /* PPS */) nal_copy( &decoder->pps, nals + i );
            avc_size += nals[i].size;
        }

        /* convert NALs to AVC format */
        if ((tmp = realloc( decoder->avc_data, decoder->avc_size + avc_size )))
        {
            decoder->avc_data = tmp;
            tmp += decoder->avc_size;

            for (i = 0; i < nal_count; i++)
            {
                *(unsigned int *)tmp = htonl( nals[i].size );
                memcpy( tmp + 4, nals[i].ptr, nals[i].size );
                tmp += 4 + nals[i].size;
            }

            decoder->avc_size += avc_size;
        }

        free( nals );

        /* wait for SPS and PPS before creating the session */
        if (decoder->session || !decoder->sps.size || !decoder->pps.size) ret = 0;
        else ret = decompression_session_create( decoder, NULL );

        avc_data = decoder->avc_data;
        avc_size = decoder->avc_size;
    }

    if (!decoder->session) return 0;

    ret = CMBlockBufferCreateWithMemoryBlock( kCFAllocatorDefault, (void *)avc_data, avc_size,
                                              kCFAllocatorNull, NULL, 0, avc_size, 0, &buffer );
    if (ret) ERR( "CMBlockBufferCreateWithMemoryBlock failed, ret %d\n", ret );
    ret = CMSampleBufferCreateReady( kCFAllocatorDefault, buffer, decoder->format, 1, 0, NULL, 1,
                                     &decoder->avc_size, &sample );
    if (ret) ERR( "CMSampleBufferCreateReady failed, ret %d\n", ret );
    CFRelease( buffer );

    ret = VTDecompressionSessionDecodeFrame( decoder->session, sample, 0, NULL, NULL );
    if (ret) ERR( "VTDecompressionSessionDecodeFrame failed, ret %d\n", ret );
    CFRelease( sample );

    if (!decoder->is_avc)
    {
        free( decoder->avc_data );
        decoder->avc_data = NULL;
        decoder->avc_size = 0;
    }

    return ret;
}

int vt_decoder_process_output( struct decoder *iface, AVFrame *frame )
{
    struct vt_decoder *decoder = vt_decoder_from_decoder( iface );
    CVPixelBufferRef image;
    CGSize frame_size;
    size_t size;
    void *data;

    TRACE( "decoder %p, frame %p, output_frame %p\n", decoder, frame, decoder->output_frame );

    if (!(image = decoder->output_frame)) return AVERROR(EAGAIN);
    decoder->output_frame = NULL;

    av_frame_unref( frame );
    size = CVPixelBufferGetDataSize( image );
    frame_size = CVImageBufferGetDisplaySize( image );

    CVPixelBufferLockBaseAddress( image, kCVPixelBufferLock_ReadOnly );
    data = CVPixelBufferGetBaseAddress( image );
    frame->buf[0] = av_buffer_create( data, size, free_pixel_buffer, image, AV_BUFFER_FLAG_READONLY );

    frame->format = AV_PIX_FMT_YUVJ420P;
    frame->width = frame_size.width;
    frame->height = frame_size.height;
    frame->data[0] = CVPixelBufferGetBaseAddressOfPlane( image, 0 );
    frame->data[1] = CVPixelBufferGetBaseAddressOfPlane( image, 1 );
    frame->data[2] = CVPixelBufferGetBaseAddressOfPlane( image, 2 );
    frame->data[3] = CVPixelBufferGetBaseAddressOfPlane( image, 3 );
    frame->linesize[0] = CVPixelBufferGetBytesPerRowOfPlane( image, 0 );
    frame->linesize[1] = CVPixelBufferGetBytesPerRowOfPlane( image, 1 );
    frame->linesize[2] = CVPixelBufferGetBytesPerRowOfPlane( image, 2 );
    frame->linesize[3] = CVPixelBufferGetBytesPerRowOfPlane( image, 3 );
    frame->extended_data = frame->data;

    return 0;
}

int vt_decoder_flush( struct decoder *iface )
{
    return 0;
}

static const struct decoder_ops vt_decoder_ops =
{
    vt_decoder_destroy,
    vt_decoder_process_input,
    vt_decoder_process_output,
    vt_decoder_flush,
};

static int context_create_from_parameters( const AVCodecParameters *par, AVCodecContext **context )
{
    int ret;

    if (!(*context = avcodec_alloc_context3( NULL ))) return -1;
    if ((ret = avcodec_parameters_to_context( *context, par )) < 0)
    {
        avcodec_free_context( context );
        return -1;
    }

    return ret;
}

int vt_decoder_create( const AVCodecParameters *par, struct decoder **out )
{
    struct vt_decoder *decoder;
    OSStatus ret;

    if (par->codec_id != AV_CODEC_ID_H264) return -1;

    if (!(decoder = calloc( 1, sizeof(*decoder) ))) return -1;
    decoder->iface.ops = &vt_decoder_ops;

    if ((ret = context_create_from_parameters( par, &decoder->iface.context )))
    {
        free( decoder );
        return ret;
    }

    if (par->extradata)
    {
        CFMutableDictionaryRef extensions, atoms;

        extensions = CFDictionaryCreateMutable( kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks );
        atoms = CFDictionaryCreateMutable( kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks );
        dict_set_data( atoms, CFSTR("avcC"), par->extradata, par->extradata_size );
        CFDictionarySetValue( extensions, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, atoms );
        CFRelease( atoms );

        ret = decompression_session_create( decoder, extensions );
        CFRelease( extensions );
        if (ret)
        {
            free( decoder );
            return ret;
        }

        decoder->is_avc = TRUE;
    }

    *out = &decoder->iface;
    return 0;
}

#else /* ENABLE_FFMPEG && ENABLE_VIDEO_TOOLBOX */

struct decoder;
typedef struct AVCodecParameters AVCodecParameters;

int vt_decoder_create( const AVCodecParameters *par, struct decoder **out )
{
    return -1;
}

#endif /* ENABLE_FFMPEG && ENABLE_VIDEO_TOOLBOX */
