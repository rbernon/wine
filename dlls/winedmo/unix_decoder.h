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

#include "config.h"

#include <stddef.h>
#include <stdarg.h>

struct decoder;
struct decoder_ops
{
    int (*destroy)(struct decoder *iface);
    int (*process_input)(struct decoder *iface, AVPacket *packet);
    int (*process_output)(struct decoder *iface, AVFrame *frame);
    int (*flush)(struct decoder *iface);
};

struct decoder
{
    const struct decoder_ops *ops;
    AVCodecContext *context;
    AVPacket *input_packet;
    unsigned draining : 1;
    unsigned flushing : 1;
};

extern int vt_decoder_create( const AVCodecParameters *par, struct decoder **out );
