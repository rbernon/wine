/*
 * Copyright 2012 Austin English
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

#include "wmvcore_private.h"
#include "wmsdk.h"

#include "wine/winedmo.h"

WINE_DEFAULT_DEBUG_CHANNEL(wmvcore);

extern HRESULT WINAPI winegstreamer_create_wm_sync_reader(IUnknown *outer, void **out);

HRESULT sync_reader_create(IUnknown *outer, void **out)
{
    NTSTATUS status;

    if ((status = winedmo_demuxer_check("video/x-ms-asf")))
    {
        WARN("Unsupported demuxer, status %#lx.\n", status);
        return winegstreamer_create_wm_sync_reader(outer, out);
    }

    FIXME("stub!\n");
    return S_OK;
}

HRESULT WINAPI WMCreateSyncReader(IUnknown *reserved, DWORD rights, IWMSyncReader **reader)
{
    TRACE("reserved %p, rights %#lx, reader %p.\n", reserved, rights, reader);
    return sync_reader_create(NULL, (void **)reader);
}

HRESULT WINAPI WMCreateSyncReaderPriv(IWMSyncReader **reader)
{
    TRACE("reader %p.\n", reader);
    return sync_reader_create(NULL, (void **)reader);
}
