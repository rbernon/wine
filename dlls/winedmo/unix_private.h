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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"

#include "unixlib.h"
#include "wine/debug.h"

#ifdef ENABLE_FFMPEG

#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>

static inline const char *debugstr_averr( int err )
{
    return wine_dbg_sprintf("%d (%s)", err, av_err2str(err) );
}

/* unixlib.c */
extern int64_t unix_seek_callback( void *opaque, int64_t offset, int whence );
extern int unix_read_callback( void *opaque, uint8_t *buffer, int size );

/* unix_demuxer.c */
extern NTSTATUS demuxer_check( void * );
extern NTSTATUS demuxer_create( void * );
extern NTSTATUS demuxer_destroy( void * );

#endif /* ENABLE_FFMPEG */
