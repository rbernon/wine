/*
 * Copyright 2023 Rémi Bernon for CodeWeavers
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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include <gst/base/base.h>
#include <gst/tag/tag.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "mferror.h"

#include "unix_private.h"

#define WG_SOURCE_MAX_STREAMS 32

struct source_stream
{
    GstPad *pad;
    GstStream *stream;
};

struct wg_source
{
    gchar *url;
    GstPad *src_pad;
    GstElement *container;
    GstSegment segment;
    GstAtomicQueue *seek_queue;
    pthread_t push_thread;

    guint64 max_duration;
    guint stream_count;
    struct source_stream streams[WG_SOURCE_MAX_STREAMS];
};

static struct wg_source *get_source(wg_source_t source)
{
    return (struct wg_source *)(ULONG_PTR)source;
}

static struct source_stream *source_stream_from_pad(struct wg_source *source, GstPad *pad)
{
    struct source_stream *stream, *end;
    for (stream = source->streams, end = stream + source->stream_count; stream != end; stream++)
        if (stream->pad == pad) return stream;
    return NULL;
}

static const char *mime_type_from_caps(GstCaps *caps)
{
    GstStructure *structure;
    if (!caps || !(structure = gst_caps_get_structure(caps, 0)))
        return "";
    return gst_structure_get_name(structure);
}

static GstCaps *detect_caps_from_data(const char *url, const void *data, guint size)
{
    const char *extension = url ? strrchr(url, '.') : NULL;
    GstTypeFindProbability probability;
    GstCaps *caps;

    GST_LOG("url %s, data %p, size %#x", url, data, size);

    if (!(caps = gst_type_find_helper_for_data_with_extension(NULL, data, size,
            extension ? extension + 1 : NULL, &probability)))
    {
        GST_ERROR("Failed to detect caps for url %s", url);
        return NULL;
    }

    if (probability > GST_TYPE_FIND_POSSIBLE)
        GST_INFO("Got probability %u for caps %" GST_PTR_FORMAT, probability, caps);
    else
        GST_FIXME("Got probability %u for caps %" GST_PTR_FORMAT, probability, caps);

    return caps;
}

static GstEvent *create_stream_start_event(const char *stream_id)
{
    GstEvent *event;

    if ((event = gst_event_new_stream_start(stream_id)))
    {
        GstStream *stream = gst_stream_new(stream_id, NULL, GST_STREAM_TYPE_UNKNOWN, 0);
        gst_event_set_stream(event, stream);
        gst_event_set_group_id(event, 1);
        gst_object_unref(stream);
    }

    return event;
}

static GstPad *create_pad_with_caps(GstPadDirection direction, GstCaps *caps)
{
    GstCaps *pad_caps = caps ? gst_caps_ref(caps) : gst_caps_new_any();
    const char *name = direction == GST_PAD_SRC ? "src" : "sink";
    GstPadTemplate *template;
    GstPad *pad = NULL;

    if (!pad_caps)
        return NULL;
    if ((template = gst_pad_template_new(name, direction, GST_PAD_ALWAYS, pad_caps)))
    {
        pad = gst_pad_new_from_template(template, "src");
        g_object_unref(template);
    }
    gst_caps_unref(pad_caps);
    return pad;
}

static GstBuffer *create_buffer_from_bytes(guint64 offset, guint64 size, const void *data)
{
    GstBuffer *buffer;

    if (!(buffer = gst_buffer_new_and_alloc(size)))
        GST_ERROR("Failed to allocate buffer for %#" G_GINT64_MODIFIER "x bytes", size);
    else
    {
        gst_buffer_fill(buffer, 0, data, size);
        gst_buffer_set_size(buffer, size);
        GST_BUFFER_OFFSET(buffer) = offset;
        GST_BUFFER_OFFSET_END(buffer) = offset + size;
    }

    return buffer;
}

static GstStream *source_get_stream(struct wg_source *source, guint index)
{
    if (index >= source->stream_count)
        return NULL;
    return gst_object_ref(source->streams[index].stream);
}

static GstCaps *source_get_stream_caps(struct wg_source *source, guint index)
{
    GstStream *stream;
    GstCaps *caps;
    if (!(stream = source_get_stream(source, index)))
        return NULL;
    caps = gst_stream_get_caps(stream);
    gst_object_unref(stream);
    return caps;
}

static GstTagList *source_get_stream_tags(struct wg_source *source, guint index)
{
    GstStream *stream;
    GstTagList *tags;
    if (!(stream = source_get_stream(source, index)))
        return NULL;
    tags = gst_stream_get_tags(stream);
    gst_object_unref(stream);
    return tags;
}

static void source_handle_seek(struct wg_source *source, GstEvent *event)
{
    guint32 seqnum = gst_event_get_seqnum(event);
    GstSeekFlags flags;
    gboolean eos;
    gint64 cur;

    gst_event_parse_seek(event, NULL, NULL, &flags, NULL, &cur, NULL, NULL);

    if (flags & GST_SEEK_FLAG_FLUSH)
    {
        event = gst_event_new_flush_start();
        gst_event_set_seqnum(event, seqnum);
        push_event(source->src_pad, event);
    }

    if ((eos = cur >= source->segment.stop))
        source->segment.start = source->segment.stop;
    else
        source->segment.start = cur;

    if (flags & GST_SEEK_FLAG_FLUSH)
    {
        event = gst_event_new_flush_stop(true);
        gst_event_set_seqnum(event, seqnum);
        push_event(source->src_pad, event);

        event = gst_event_new_segment(&source->segment);
        gst_event_set_seqnum(event, seqnum);
        push_event(source->src_pad, event);
    }

    if (source->segment.start == source->segment.stop)
        push_event(source->src_pad, gst_event_new_eos());
}

static gboolean src_event_seek(struct wg_source *source, GstEvent *event)
{
    GstFormat format;

    GST_TRACE("source %p, %"GST_PTR_FORMAT, source, event);

    gst_event_parse_seek(event, NULL, &format, NULL, NULL, NULL, NULL, NULL);
    if (format != GST_FORMAT_BYTES)
    {
        gst_event_unref(event);
        return false;
    }

    /* Even in push mode, oggdemux uses a separate thread to request seeks, we have to handle
     * these asynchronously from wg_source_get_position.
     *
     * On the other hand, other demuxers emit seeks synchronously during gst_pad_push_buffer,
     * and expect to see flush events being pushed synchronously as well, we have to handle
     * these directly here.
     */
    if (source->push_thread != pthread_self())
        gst_atomic_queue_push(source->seek_queue, event);
    else
    {
        source_handle_seek(source, event);
        gst_event_unref(event);
    }

    return true;
}

static gboolean src_event_cb(GstPad *pad, GstObject *parent, GstEvent *event)
{
    struct wg_source *source = gst_pad_get_element_private(pad);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_SEEK:
        return src_event_seek(source, event);
    default:
        GST_TRACE("source %p, ignoring %" GST_PTR_FORMAT, source, event);
        return gst_pad_event_default(pad, parent, event);
    }
}

static gboolean src_query_duration(struct wg_source *source, GstQuery *query)
{
    GstFormat format;

    GST_LOG("source %p, query %" GST_PTR_FORMAT, source, query);

    gst_query_parse_duration(query, &format, NULL);
    if (format != GST_FORMAT_BYTES)
        return false;

    gst_query_set_duration(query, format, source->segment.stop);
    return true;
}

static gboolean src_query_scheduling(struct wg_source *source, GstQuery *query)
{
    GST_LOG("source %p, query %" GST_PTR_FORMAT, source, query);

    gst_query_set_scheduling(query, GST_SCHEDULING_FLAG_SEEKABLE, 1, -1, 0);
    gst_query_add_scheduling_mode(query, GST_PAD_MODE_PUSH);

    return true;
}

static gboolean src_query_seeking(struct wg_source *source, GstQuery *query)
{
    GstFormat format;

    GST_LOG("source %p, query %" GST_PTR_FORMAT, source, query);

    gst_query_parse_seeking(query, &format, NULL, NULL, NULL);
    if (format != GST_FORMAT_BYTES)
        return false;

    gst_query_set_seeking(query, GST_FORMAT_BYTES, 1, 0, source->segment.stop);
    return true;
}

static gboolean src_query_uri(struct wg_source *source, GstQuery *query)
{
    gchar *uri;

    GST_LOG("source %p, query %" GST_PTR_FORMAT, source, query);

    gst_query_parse_uri(query, &uri);
    gst_query_set_uri(query, source->url);

    return true;
}

static gboolean src_query_cb(GstPad *pad, GstObject *parent, GstQuery *query)
{
    struct wg_source *source = gst_pad_get_element_private(pad);

    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_DURATION:
        return src_query_duration(source, query);
    case GST_QUERY_SCHEDULING:
        return src_query_scheduling(source, query);
    case GST_QUERY_SEEKING:
        return src_query_seeking(source, query);
    case GST_QUERY_URI:
        if (!source->url)
            return false;
        return src_query_uri(source, query);
    default:
        GST_TRACE("source %p, ignoring %" GST_PTR_FORMAT, source, query);
        return gst_pad_query_default(pad, parent, query);
    }
}

static GstFlowReturn sink_chain_cb(GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
    struct wg_soutce *source = gst_pad_get_element_private(pad);

    GST_LOG("source %p, pad %" GST_PTR_FORMAT ", buffer %" GST_PTR_FORMAT, source, pad, buffer);

    gst_buffer_unref(buffer);
    return GST_FLOW_EOS;
}

static gboolean sink_event_caps(struct wg_source *source, GstPad *pad, GstEvent *event)
{
    struct source_stream *stream = source_stream_from_pad(source, pad);
    GstCaps *caps;

    GST_LOG("source %p, pad %" GST_PTR_FORMAT ", event %" GST_PTR_FORMAT, source, pad, event);

    gst_event_parse_caps(event, &caps);

    gst_stream_set_caps(stream->stream, gst_caps_copy(caps));
    gst_stream_set_stream_type(stream->stream, stream_type_from_caps(caps));

    gst_event_unref(event);
    return true;
}

static gboolean sink_event_tag(struct wg_source *source, GstPad *pad, GstEvent *event)
{
    struct source_stream *stream = source_stream_from_pad(source, pad);
    GstTagList *new_tags, *old_tags = gst_stream_get_tags(stream->stream);

    GST_LOG("source %p, pad %" GST_PTR_FORMAT ", event %" GST_PTR_FORMAT, source, pad, event);

    gst_event_parse_tag(event, &new_tags);

    if ((new_tags = gst_tag_list_merge(old_tags, new_tags, GST_TAG_MERGE_REPLACE)))
    {
        gst_stream_set_tags(stream->stream, new_tags);
        gst_tag_list_unref(new_tags);
    }
    if (old_tags)
        gst_tag_list_unref(old_tags);

    gst_event_unref(event);
    return true;
}

static gboolean sink_event_stream_start(struct wg_source *source, GstPad *pad, GstEvent *event)
{
    struct source_stream *stream = source_stream_from_pad(source, pad);
    const gchar *new_id, *old_id = gst_stream_get_stream_id(stream->stream);
    GstStream *new_stream, *old_stream = stream->stream;
    guint group, flags;
    gint64 duration;

    GST_LOG("source %p, pad %" GST_PTR_FORMAT ", event %" GST_PTR_FORMAT, source, pad, event);

    gst_event_parse_stream_start(event, &new_id);
    gst_event_parse_stream(event, &new_stream);
    gst_event_parse_stream_flags(event, &flags);
    if (!gst_event_parse_group_id(event, &group))
        group = -1;

    if (strcmp(old_id, new_id))
    {
        if (!(stream->stream = new_stream))
            stream->stream = gst_stream_new(new_id, NULL, GST_STREAM_TYPE_UNKNOWN, 0);
        else
            gst_object_ref(stream->stream);
        gst_object_unref(old_stream);
    }

    if (gst_pad_peer_query_duration(pad, GST_FORMAT_TIME, &duration) && GST_CLOCK_TIME_IS_VALID(duration))
    {
        GST_TRACE("Got duration %" GST_TIME_FORMAT " for source %p, pad %" GST_PTR_FORMAT ", stream %" GST_PTR_FORMAT,
                GST_TIME_ARGS(duration), source, pad, stream);
        source->max_duration = max(source->max_duration, duration);
    }

    gst_event_unref(event);
    return true;
}

static gboolean sink_event_cb(GstPad *pad, GstObject *parent, GstEvent *event)
{
    struct wg_source *source = gst_pad_get_element_private(pad);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
        return sink_event_caps(source, pad, event);
    case GST_EVENT_TAG:
        return sink_event_tag(source, pad, event);
    case GST_EVENT_STREAM_START:
        return sink_event_stream_start(source, pad, event);
    default:
        GST_TRACE("source %p, pad %" GST_PTR_FORMAT ", ignoring %" GST_PTR_FORMAT, source, pad, event);
        return gst_pad_event_default(pad, parent, event);
    }
}

static void pad_added_cb(GstElement *element, GstPad *pad, gpointer user)
{
    struct wg_source *source = user;
    struct source_stream *stream;
    char *id;

    GST_LOG("source %p, element %" GST_PTR_FORMAT ", pad %" GST_PTR_FORMAT, source, element, pad);

    stream = source->streams + source->stream_count++;
    if (stream >= source->streams + ARRAY_SIZE(source->streams))
    {
        GST_FIXME("Not enough sink pads, need %u", source->stream_count);
        return;
    }

    if (gst_pad_link(pad, stream->pad) < 0 || !gst_pad_set_active(stream->pad, true))
        GST_ERROR("Failed to link new pad to sink pad %" GST_PTR_FORMAT, stream->pad);

    if ((stream->stream = gst_pad_get_stream(pad)))
        GST_TRACE("Found stream %" GST_PTR_FORMAT " for pad %" GST_PTR_FORMAT, stream->stream, pad);
    else
    {
        if (!(id = gst_pad_get_stream_id(pad)))
        {
            char buffer[256];
            snprintf(buffer, ARRAY_SIZE(buffer), "wg_source/%03zu", stream - source->streams);
            id = g_strdup(buffer);
        }

        if (!(stream->stream = gst_stream_new(id, NULL, GST_STREAM_TYPE_UNKNOWN, 0)))
            GST_ERROR("Failed to create stream event for sink pad %" GST_PTR_FORMAT, stream->pad);
        else
            GST_TRACE("Created stream %" GST_PTR_FORMAT " for pad %" GST_PTR_FORMAT, stream->stream, stream->pad);

        g_free(id);
    }
}

NTSTATUS wg_source_create(void *args)
{
    struct wg_source_create_params *params = args;
    GstElement *first = NULL, *last = NULL, *element;
    struct wg_source *source;
    const gchar *media_type;
    GstCaps *src_caps;
    GstPad *peer;
    guint i;

    if (!(src_caps = detect_caps_from_data(params->url, params->data, params->size)))
        return STATUS_UNSUCCESSFUL;
    if (!(source = calloc(1, sizeof(*source))))
    {
        gst_caps_unref(src_caps);
        return STATUS_UNSUCCESSFUL;
    }
    source->url = params->url ? strdup(params->url) : NULL;
    gst_segment_init(&source->segment, GST_FORMAT_BYTES);
    source->segment.stop = params->file_size;

    media_type = gst_structure_get_name(gst_caps_get_structure(src_caps, 0));
    if (!strcmp(media_type, "video/quicktime"))
        strcpy(params->mime_type, "video/mp4");
    else if (!strcmp(media_type, "video/x-msvideo"))
        strcpy(params->mime_type, "video/avi");
    else
        lstrcpynA(params->mime_type, media_type, ARRAY_SIZE(params->mime_type));

    if (!(source->container = gst_bin_new("wg_source")))
        goto error;
    GST_OBJECT_FLAG_SET(source->container, GST_BIN_FLAG_STREAMS_AWARE);

    if (!(source->src_pad = create_pad_with_caps(GST_PAD_SRC, src_caps)))
        goto error;
    gst_pad_set_element_private(source->src_pad, source);
    gst_pad_set_query_function(source->src_pad, src_query_cb);
    gst_pad_set_event_function(source->src_pad, src_event_cb);
    if (!(source->seek_queue = gst_atomic_queue_new(1)))
        goto error;

    for (i = 0; i < ARRAY_SIZE(source->streams); i++)
    {
        if (!(source->streams[i].pad = create_pad_with_caps(GST_PAD_SINK, NULL)))
            goto error;
        gst_pad_set_element_private(source->streams[i].pad, source);
        gst_pad_set_chain_function(source->streams[i].pad, sink_chain_cb);
        gst_pad_set_event_function(source->streams[i].pad, sink_event_cb);
    }

    if (!(element = find_element(GST_ELEMENT_FACTORY_TYPE_DECODABLE, src_caps, GST_CAPS_ANY))
            || !append_element(source->container, element, &first, &last))
        goto error;
    g_signal_connect(element, "pad-added", G_CALLBACK(pad_added_cb), source);

    if (!link_src_to_element(source->src_pad, first))
        goto error;
    if (!gst_pad_set_active(source->src_pad, true))
        goto error;

    /* try to link the first output pad, some demuxers only have static pads */
    if ((peer = gst_element_get_static_pad(last, "src")))
    {
        pad_added_cb(last, peer, source);
        gst_object_unref(peer);
    }

    gst_element_set_state(source->container, GST_STATE_PAUSED);
    if (!gst_element_get_state(source->container, NULL, NULL, -1))
        goto error;

    if (!push_event(source->src_pad, create_stream_start_event("wg_source")))
        goto error;
    if (!push_event(source->src_pad, gst_event_new_segment(&source->segment)))
        goto error;

    gst_caps_unref(src_caps);

    params->source = (wg_source_t)(ULONG_PTR)source;
    GST_INFO("Created winegstreamer source %p.", source);
    return STATUS_SUCCESS;

error:
    if (source->container)
    {
        gst_element_set_state(source->container, GST_STATE_NULL);
        gst_object_unref(source->container);
    }
    for (i = 0; i < ARRAY_SIZE(source->streams); i++)
    {
        if (source->streams[i].pad)
            gst_object_unref(source->streams[i].pad);
    }
    if (source->seek_queue)
    {
        GstEvent *event;
        while ((event = gst_atomic_queue_pop(source->seek_queue)))
            gst_event_unref(event);
        gst_atomic_queue_unref(source->seek_queue);
    }
    if (source->src_pad)
        gst_object_unref(source->src_pad);
    free(source->url);
    free(source);

    gst_caps_unref(src_caps);

    GST_ERROR("Failed to create winegstreamer source.");
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS wg_source_destroy(void *args)
{
    struct wg_source *source = get_source(*(wg_source_t *)args);
    GstEvent *event;
    guint i;

    GST_TRACE("source %p", source);

    while ((event = gst_atomic_queue_pop(source->seek_queue)))
        gst_event_unref(event);
    gst_atomic_queue_unref(source->seek_queue);

    gst_element_set_state(source->container, GST_STATE_NULL);
    gst_object_unref(source->container);
    for (i = 0; i < ARRAY_SIZE(source->streams); i++)
        gst_object_unref(source->streams[i].pad);
    gst_object_unref(source->src_pad);
    free(source->url);
    free(source);

    return STATUS_SUCCESS;
}

NTSTATUS wg_source_get_stream_count(void *args)
{
    struct wg_source_get_stream_count_params *params = args;
    struct wg_source *source = get_source(params->source);
    UINT i, stream_count;
    GstCaps *caps;

    GST_TRACE("source %p", source);

    for (i = 0, stream_count = source->stream_count; i < stream_count; i++)
    {
        if (!(caps = source_get_stream_caps(source, i)))
            return STATUS_PENDING;
        gst_caps_unref(caps);
    }

    params->stream_count = stream_count;
    return stream_count ? STATUS_SUCCESS : STATUS_PENDING;
}

NTSTATUS wg_source_get_duration(void *args)
{
    struct wg_source_get_duration_params *params = args;
    struct wg_source *source = get_source(params->source);

    GST_TRACE("source %p", source);

    params->duration = source->max_duration / 100;
    return STATUS_SUCCESS;
}

NTSTATUS wg_source_get_position(void *args)
{
    struct wg_source_get_position_params *params = args;
    struct wg_source *source = get_source(params->source);
    GstEvent *event;

    GST_TRACE("source %p", source);

    while ((event = gst_atomic_queue_pop(source->seek_queue)))
    {
        source_handle_seek(source, event);
        gst_event_unref(event);
    }

    params->read_offset = source->segment.start;
    return STATUS_SUCCESS;
}

NTSTATUS wg_source_push_data(void *args)
{
    struct wg_source_push_data_params *params = args;
    struct wg_source *source = get_source(params->source);
    GstFlowReturn ret = GST_FLOW_OK;
    GstBuffer *buffer;

    GST_TRACE("source %p, offset %#" G_GINT64_MODIFIER "x, size %#" G_GINT64_MODIFIER "x, data %p",
            source, params->offset, params->size, params->data);

    if (!params->size)
    {
        push_event(source->src_pad, gst_event_new_eos());
        return STATUS_SUCCESS;
    }

    if (!(buffer = create_buffer_from_bytes(params->offset, params->size, params->data)))
    {
        GST_WARNING("Failed to allocate buffer for data");
        return STATUS_UNSUCCESSFUL;
    }

    if (params->offset > source->segment.start)
    {
        source->segment.start = params->offset;
        gst_buffer_set_flags(buffer, GST_BUFFER_FLAG_DISCONT);
    }
    else if (params->offset < source->segment.start)
    {
        source->segment.start = params->offset;
        push_event(source->src_pad, gst_event_new_segment(&source->segment));
    }

    source->push_thread = pthread_self();
    source->segment.start += params->size;
    if ((ret = gst_pad_push(source->src_pad, buffer)) && ret != GST_FLOW_EOS)
    {
        GST_ERROR("Failed to push data buffer, ret %d", ret);
        source->segment.start -= params->size;
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS wg_source_get_stream_type(void *args)
{
    struct wg_source_get_stream_type_params *params = args;
    struct wg_source *source = get_source(params->source);
    guint index = params->index;
    NTSTATUS status;
    GstCaps *caps;

    GST_TRACE("source %p, index %u", source, index);

    if (!(caps = source_get_stream_caps(source, index)))
        return STATUS_UNSUCCESSFUL;
    status = caps_to_media_type(caps, &params->media_type, 0);
    gst_caps_unref(caps);
    return status;
}

NTSTATUS wg_source_get_stream_name(void *args)
{
    struct wg_source_get_stream_name_params *params = args;
    struct wg_source *source = get_source(params->source);
    NTSTATUS status = STATUS_NOT_FOUND;
    guint index = params->index;
    guint i, tag_count;
    GstTagList *tags;

    GST_TRACE("source %p, index %u", source, index);

    if (!(tags = source_get_stream_tags(source, index)))
        return STATUS_UNSUCCESSFUL;

    /* Extract stream name from Quick Time demuxer private tag where it puts unrecognized chunks. */
    tag_count = gst_tag_list_get_tag_size(tags, "private-qt-tag");
    for (i = 0; status && i < tag_count; ++i)
    {
        const gchar *name;
        const GValue *val;
        GstSample *sample;
        GstBuffer *buf;
        int size;

        if (!(val = gst_tag_list_get_value_index(tags, "private-qt-tag", i)))
            continue;
        if (!GST_VALUE_HOLDS_SAMPLE(val) || !(sample = gst_value_get_sample(val)))
            continue;
        if (!(name = gst_structure_get_name(gst_sample_get_info(sample))) || strcmp(name, "application/x-gst-qt-name-tag")
                || !(buf = gst_sample_get_buffer(sample)) || (size = gst_buffer_get_size(buf)) <= 8)
            continue;

        if ((size = gst_buffer_extract(buf, 8, params->buffer, min(size - 8, sizeof(params->buffer) - 1))))
        {
            params->buffer[size] = 0;
            status = STATUS_SUCCESS;
        }
    }

    gst_tag_list_unref(tags);
    return status;
}

NTSTATUS wg_source_get_stream_lang(void *args)
{
    struct wg_source_get_stream_lang_params *params = args;
    struct wg_source *source = get_source(params->source);
    NTSTATUS status = STATUS_NOT_FOUND;
    guint index = params->index;
    bool is_quicktime = false;
    const gchar *lang_code;
    gchar *value, *tmp;
    GstTagList *tags;
    GstCaps *caps;

    GST_TRACE("source %p, index %u", source, index);

    if (!(tags = source_get_stream_tags(source, index)))
        return STATUS_UNSUCCESSFUL;

    if ((caps = gst_pad_get_current_caps(source->src_pad)))
    {
        is_quicktime = !strcmp(mime_type_from_caps(caps), "video/quicktime");
        gst_caps_unref(caps);
    }

    if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &value) && value)
    {
        if (is_quicktime && (lang_code = gst_tag_get_language_code_iso_639_1(value))
                && (tmp = g_strdup(lang_code)))
        {
            /* For QuickTime media, we convert the language tags to ISO 639-1. */
            g_free(value);
            value = tmp;
        }

        lstrcpynA(params->buffer, value, sizeof(params->buffer));
        status = STATUS_SUCCESS;
        g_free(value);
    }

    gst_tag_list_unref(tags);
    return status;
}
