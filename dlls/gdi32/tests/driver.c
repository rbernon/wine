/*
 * Unit test suite for kernel mode graphics driver
 *
 * Copyright 2019 Zhiyi Zhang
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winternl.h"
#include "dwmapi.h"
#include "ddk/d3dkmthk.h"
#include "initguid.h"
#include "setupapi.h"
#include "ntddvdeo.h"
#include "devpkey.h"
#include "cfgmgr32.h"

#include "wine/test.h"

static const WCHAR display1W[] = L"\\\\.\\DISPLAY1";

DEFINE_DEVPROPKEY(DEVPROPKEY_GPU_LUID, 0x60b193cb, 0x5276, 0x4d0f, 0x96, 0xfc, 0xf1, 0x73, 0xab, 0xad, 0x3e, 0xc6, 2);

static NTSTATUS (WINAPI *pD3DKMTCheckOcclusion)(const D3DKMT_CHECKOCCLUSION *);
static NTSTATUS (WINAPI *pD3DKMTCheckVidPnExclusiveOwnership)(const D3DKMT_CHECKVIDPNEXCLUSIVEOWNERSHIP *);
static NTSTATUS (WINAPI *pD3DKMTCloseAdapter)(const D3DKMT_CLOSEADAPTER *);
static NTSTATUS (WINAPI *pD3DKMTCreateAllocation)(D3DKMT_CREATEALLOCATION *);
static NTSTATUS (WINAPI *pD3DKMTCreateAllocation2)(D3DKMT_CREATEALLOCATION *);
static NTSTATUS (WINAPI *pD3DKMTCreateDevice)(D3DKMT_CREATEDEVICE *);
static NTSTATUS (WINAPI *pD3DKMTCreateKeyedMutex)(D3DKMT_CREATEKEYEDMUTEX *);
static NTSTATUS (WINAPI *pD3DKMTCreateKeyedMutex2)(D3DKMT_CREATEKEYEDMUTEX2 *);
static NTSTATUS (WINAPI *pD3DKMTCreateSynchronizationObject)(D3DKMT_CREATESYNCHRONIZATIONOBJECT *);
static NTSTATUS (WINAPI *pD3DKMTCreateSynchronizationObject2)(D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *);
static NTSTATUS (WINAPI *pD3DKMTDestroyAllocation)(const D3DKMT_DESTROYALLOCATION *);
static NTSTATUS (WINAPI *pD3DKMTDestroyAllocation2)(const D3DKMT_DESTROYALLOCATION2 *);
static NTSTATUS (WINAPI *pD3DKMTDestroyDevice)(const D3DKMT_DESTROYDEVICE *);
static NTSTATUS (WINAPI *pD3DKMTDestroyKeyedMutex)(const D3DKMT_DESTROYKEYEDMUTEX *);
static NTSTATUS (WINAPI *pD3DKMTDestroySynchronizationObject)(const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *);
static NTSTATUS (WINAPI *pD3DKMTEnumAdapters2)(D3DKMT_ENUMADAPTERS2 *);
static NTSTATUS (WINAPI *pD3DKMTOpenAdapterFromDeviceName)(D3DKMT_OPENADAPTERFROMDEVICENAME *);
static NTSTATUS (WINAPI *pD3DKMTOpenAdapterFromGdiDisplayName)(D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME *);
static NTSTATUS (WINAPI *pD3DKMTOpenAdapterFromHdc)(D3DKMT_OPENADAPTERFROMHDC *);
static NTSTATUS (WINAPI *pD3DKMTOpenKeyedMutex)(D3DKMT_OPENKEYEDMUTEX *);
static NTSTATUS (WINAPI *pD3DKMTOpenKeyedMutex2)(D3DKMT_OPENKEYEDMUTEX2 *);
static NTSTATUS (WINAPI *pD3DKMTOpenKeyedMutexFromNtHandle)(D3DKMT_OPENKEYEDMUTEXFROMNTHANDLE *);
static NTSTATUS (WINAPI *pD3DKMTOpenResource)(D3DKMT_OPENRESOURCE *);
static NTSTATUS (WINAPI *pD3DKMTOpenResource2)(D3DKMT_OPENRESOURCE *);
static NTSTATUS (WINAPI *pD3DKMTOpenResourceFromNtHandle)(D3DKMT_OPENRESOURCEFROMNTHANDLE *);
static NTSTATUS (WINAPI *pD3DKMTOpenSynchronizationObject)(D3DKMT_OPENSYNCHRONIZATIONOBJECT *);
static NTSTATUS (WINAPI *pD3DKMTOpenSyncObjectFromNtHandle)(D3DKMT_OPENSYNCOBJECTFROMNTHANDLE *);
static NTSTATUS (WINAPI *pD3DKMTOpenSyncObjectFromNtHandle2)(D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *);
static NTSTATUS (WINAPI *pD3DKMTOpenSyncObjectNtHandleFromName)(D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *);
static NTSTATUS (WINAPI *pD3DKMTQueryResourceInfo)(D3DKMT_QUERYRESOURCEINFO *);
static NTSTATUS (WINAPI *pD3DKMTQueryResourceInfoFromNtHandle)(D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *);
static NTSTATUS (WINAPI *pD3DKMTQueryAdapterInfo)(D3DKMT_QUERYADAPTERINFO *);
static NTSTATUS (WINAPI *pD3DKMTQueryVideoMemoryInfo)(D3DKMT_QUERYVIDEOMEMORYINFO *);
static NTSTATUS (WINAPI *pD3DKMTSetVidPnSourceOwner)(const D3DKMT_SETVIDPNSOURCEOWNER *);
static NTSTATUS (WINAPI *pD3DKMTShareObjects)( UINT count, const D3DKMT_HANDLE *handles, OBJECT_ATTRIBUTES *attr, UINT access, HANDLE *handle );
static HRESULT  (WINAPI *pDwmEnableComposition)(UINT);

static BOOL get_primary_adapter_name(WCHAR *name)
{
    DISPLAY_DEVICEW dd;
    DWORD adapter_idx;

    dd.cb = sizeof(dd);
    for (adapter_idx = 0; EnumDisplayDevicesW(NULL, adapter_idx, &dd, 0); ++adapter_idx)
    {
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            lstrcpyW(name, dd.DeviceName);
            return TRUE;
        }
    }

    return FALSE;
}

static void test_D3DKMTOpenAdapterFromGdiDisplayName(void)
{
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter_gdi_desc;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    DISPLAY_DEVICEW display_device = {sizeof(display_device)};
    NTSTATUS status;
    DWORD i;

    lstrcpyW(open_adapter_gdi_desc.DeviceName, display1W);
    if (!pD3DKMTOpenAdapterFromGdiDisplayName
        || pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc) == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTOpenAdapterFromGdiDisplayName() is unavailable.\n");
        return;
    }

    close_adapter_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    /* Invalid parameters */
    status = pD3DKMTOpenAdapterFromGdiDisplayName(NULL);
    ok(status == STATUS_UNSUCCESSFUL, "Got unexpected return code %#lx.\n", status);

    memset(&open_adapter_gdi_desc, 0, sizeof(open_adapter_gdi_desc));
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc);
    ok(status == STATUS_UNSUCCESSFUL, "Got unexpected return code %#lx.\n", status);

    /* Open adapter */
    for (i = 0; EnumDisplayDevicesW(NULL, i, &display_device, 0); ++i)
    {
        lstrcpyW(open_adapter_gdi_desc.DeviceName, display_device.DeviceName);
        status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc);
        if (display_device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
            ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
        else
        {
            ok(status == STATUS_UNSUCCESSFUL, "Got unexpected return code %#lx.\n", status);
            continue;
        }

        ok(open_adapter_gdi_desc.hAdapter, "Expect not null.\n");
        ok(open_adapter_gdi_desc.AdapterLuid.LowPart || open_adapter_gdi_desc.AdapterLuid.HighPart,
           "Expect LUID not zero.\n");

        close_adapter_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
        status = pD3DKMTCloseAdapter(&close_adapter_desc);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    }
}

static void test_D3DKMTOpenAdapterFromHdc(void)
{
    DISPLAY_DEVICEW display_device = {sizeof(display_device)};
    D3DKMT_OPENADAPTERFROMHDC open_adapter_hdc_desc;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    INT adapter_count = 0;
    NTSTATUS status;
    HDC hdc;
    DWORD i;

    if (!pD3DKMTOpenAdapterFromHdc)
    {
        win_skip("D3DKMTOpenAdapterFromHdc() is missing.\n");
        return;
    }

    /* Invalid parameters */
    /* Passing a NULL pointer crashes on Windows 10 >= 2004 */
    if (0) status = pD3DKMTOpenAdapterFromHdc(NULL);

    memset(&open_adapter_hdc_desc, 0, sizeof(open_adapter_hdc_desc));
    status = pD3DKMTOpenAdapterFromHdc(&open_adapter_hdc_desc);
    if (status == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTOpenAdapterFromHdc() is not supported.\n");
        return;
    }
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    /* Open adapter */
    for (i = 0; EnumDisplayDevicesW(NULL, i, &display_device, 0); ++i)
    {
        if (!(display_device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
            continue;

        adapter_count++;

        hdc = CreateDCW(0, display_device.DeviceName, 0, NULL);
        open_adapter_hdc_desc.hDc = hdc;
        status = pD3DKMTOpenAdapterFromHdc(&open_adapter_hdc_desc);
        todo_wine ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
        todo_wine ok(open_adapter_hdc_desc.hAdapter, "Expect not null.\n");
        DeleteDC(hdc);

        if (status == STATUS_SUCCESS)
        {
            close_adapter_desc.hAdapter = open_adapter_hdc_desc.hAdapter;
            status = pD3DKMTCloseAdapter(&close_adapter_desc);
            ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
        }
    }

    /* HDC covering more than two adapters is invalid for D3DKMTOpenAdapterFromHdc */
    hdc = GetDC(0);
    open_adapter_hdc_desc.hDc = hdc;
    status = pD3DKMTOpenAdapterFromHdc(&open_adapter_hdc_desc);
    ReleaseDC(0, hdc);
    todo_wine ok(status == (adapter_count > 1 ? STATUS_INVALID_PARAMETER : STATUS_SUCCESS),
                 "Got unexpected return code %#lx.\n", status);
    if (status == STATUS_SUCCESS)
    {
        close_adapter_desc.hAdapter = open_adapter_hdc_desc.hAdapter;
        status = pD3DKMTCloseAdapter(&close_adapter_desc);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    }
}

static void test_D3DKMTEnumAdapters2(void)
{
    D3DKMT_ENUMADAPTERS2 enum_adapters_2_desc = {0};
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    NTSTATUS status;
    UINT i;

    if (!pD3DKMTEnumAdapters2 || pD3DKMTEnumAdapters2(&enum_adapters_2_desc) == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTEnumAdapters2() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTEnumAdapters2(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    /* Query the array to allocate */
    memset(&enum_adapters_2_desc, 0, sizeof(enum_adapters_2_desc));
    status = pD3DKMTEnumAdapters2(&enum_adapters_2_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    ok(enum_adapters_2_desc.NumAdapters == 32 /* win10 and older */ || enum_adapters_2_desc.NumAdapters == 34 /* win11 */,
       "Got unexpected value %lu.\n", enum_adapters_2_desc.NumAdapters);

    /* Allocate the array */
    enum_adapters_2_desc.pAdapters = calloc(enum_adapters_2_desc.NumAdapters, sizeof(D3DKMT_ADAPTERINFO));
    ok(!!enum_adapters_2_desc.pAdapters, "Expect not null.\n");

    /* Enumerate adapters */
    status = pD3DKMTEnumAdapters2(&enum_adapters_2_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    ok(enum_adapters_2_desc.NumAdapters, "Expect not zero.\n");

    for (i = 0; i < enum_adapters_2_desc.NumAdapters; ++i)
    {
        ok(enum_adapters_2_desc.pAdapters[i].hAdapter, "Expect not null.\n");
        ok(enum_adapters_2_desc.pAdapters[i].AdapterLuid.LowPart || enum_adapters_2_desc.pAdapters[i].AdapterLuid.HighPart,
           "Expect LUID not zero.\n");

        close_adapter_desc.hAdapter = enum_adapters_2_desc.pAdapters[i].hAdapter;
        status = pD3DKMTCloseAdapter(&close_adapter_desc);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    }

    /* Check for insufficient buffer */
    enum_adapters_2_desc.NumAdapters = 0;
    status = pD3DKMTEnumAdapters2(&enum_adapters_2_desc);
    ok(status == STATUS_BUFFER_TOO_SMALL, "Got unexpected return code %#lx.\n", status);

    free(enum_adapters_2_desc.pAdapters);
}

static void test_D3DKMTCloseAdapter(void)
{
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    NTSTATUS status;

    if (!pD3DKMTCloseAdapter || pD3DKMTCloseAdapter(NULL) == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTCloseAdapter() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTCloseAdapter(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    memset(&close_adapter_desc, 0, sizeof(close_adapter_desc));
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);
}

static void test_D3DKMTCreateDevice(void)
{
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter_gdi_desc;
    D3DKMT_CREATEDEVICE create_device_desc;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    D3DKMT_DESTROYDEVICE destroy_device_desc;
    NTSTATUS status;

    if (!pD3DKMTCreateDevice || pD3DKMTCreateDevice(NULL) == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTCreateDevice() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTCreateDevice(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    memset(&create_device_desc, 0, sizeof(create_device_desc));
    status = pD3DKMTCreateDevice(&create_device_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    lstrcpyW(open_adapter_gdi_desc.DeviceName, display1W);
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    /* Create device */
    create_device_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCreateDevice(&create_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    ok(create_device_desc.hDevice, "Expect not null.\n");
    ok(create_device_desc.pCommandBuffer == NULL, "Expect null.\n");
    ok(create_device_desc.CommandBufferSize == 0, "Got wrong value %#x.\n", create_device_desc.CommandBufferSize);
    ok(create_device_desc.pAllocationList == NULL, "Expect null.\n");
    ok(create_device_desc.AllocationListSize == 0, "Got wrong value %#x.\n", create_device_desc.AllocationListSize);
    ok(create_device_desc.pPatchLocationList == NULL, "Expect null.\n");
    ok(create_device_desc.PatchLocationListSize == 0, "Got wrong value %#x.\n", create_device_desc.PatchLocationListSize);

    destroy_device_desc.hDevice = create_device_desc.hDevice;
    status = pD3DKMTDestroyDevice(&destroy_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    close_adapter_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
}

static void test_D3DKMTDestroyDevice(void)
{
    D3DKMT_DESTROYDEVICE destroy_device_desc;
    NTSTATUS status;

    if (!pD3DKMTDestroyDevice || pD3DKMTDestroyDevice(NULL) == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTDestroyDevice() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTDestroyDevice(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    memset(&destroy_device_desc, 0, sizeof(destroy_device_desc));
    status = pD3DKMTDestroyDevice(&destroy_device_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);
}

static void test_D3DKMTCheckVidPnExclusiveOwnership(void)
{
    static const DWORD timeout = 1000;
    static const DWORD wait_step = 100;
    D3DKMT_CREATEDEVICE create_device_desc, create_device_desc2;
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter_gdi_desc;
    D3DKMT_CHECKVIDPNEXCLUSIVEOWNERSHIP check_owner_desc;
    D3DKMT_DESTROYDEVICE destroy_device_desc;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    D3DKMT_VIDPNSOURCEOWNER_TYPE owner_type;
    D3DKMT_SETVIDPNSOURCEOWNER set_owner_desc;
    DWORD total_time;
    NTSTATUS status;
    INT i;

    /* Test cases using single device */
    static const struct test_data1
    {
        D3DKMT_VIDPNSOURCEOWNER_TYPE owner_type;
        NTSTATUS expected_set_status;
        NTSTATUS expected_check_status;
    } tests1[] = {
        /* 0 */
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVEGDI, STATUS_INVALID_PARAMETER, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        /* 10 */
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        /* 20 */
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        /* 30 */
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_INVALID_PARAMETER, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        /* 40 */
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_INVALID_PARAMETER, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        /* 50 */
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_INVALID_PARAMETER, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED + 1, STATUS_INVALID_PARAMETER, STATUS_SUCCESS},
    };

    /* Test cases using two devices consecutively */
    static const struct test_data2
    {
        D3DKMT_VIDPNSOURCEOWNER_TYPE set_owner_type1;
        D3DKMT_VIDPNSOURCEOWNER_TYPE set_owner_type2;
        NTSTATUS expected_set_status1;
        NTSTATUS expected_set_status2;
        NTSTATUS expected_check_status;
    } tests2[] = {
        /* 0 */
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        /* 10 */
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {-1, D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, -1, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, -1, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
    };

    if (!pD3DKMTCheckVidPnExclusiveOwnership || pD3DKMTCheckVidPnExclusiveOwnership(NULL) == STATUS_PROCEDURE_NOT_FOUND)
    {
        /* This is a stub in some drivers (e.g. nulldrv) */
        skip("D3DKMTCheckVidPnExclusiveOwnership() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTCheckVidPnExclusiveOwnership(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    memset(&check_owner_desc, 0, sizeof(check_owner_desc));
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    /* Test cases */
    lstrcpyW(open_adapter_gdi_desc.DeviceName, display1W);
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    memset(&create_device_desc, 0, sizeof(create_device_desc));
    create_device_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCreateDevice(&create_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    check_owner_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    check_owner_desc.VidPnSourceId = open_adapter_gdi_desc.VidPnSourceId;
    for (i = 0; i < ARRAY_SIZE(tests1); ++i)
    {
        set_owner_desc.hDevice = create_device_desc.hDevice;
        if (tests1[i].owner_type != -1)
        {
            owner_type = tests1[i].owner_type;
            set_owner_desc.pType = &owner_type;
            set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
            set_owner_desc.VidPnSourceCount = 1;
        }
        else
        {
            set_owner_desc.pType = NULL;
            set_owner_desc.pVidPnSourceId = NULL;
            set_owner_desc.VidPnSourceCount = 0;
        }
        status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
        ok(status == tests1[i].expected_set_status ||
               /* win8 doesn't support D3DKMT_VIDPNSOURCEOWNER_EMULATED */
               (status == STATUS_INVALID_PARAMETER && tests1[i].owner_type == D3DKMT_VIDPNSOURCEOWNER_EMULATED)
               || (status == STATUS_SUCCESS && tests1[i].owner_type == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE
                   && tests1[i - 1].owner_type == D3DKMT_VIDPNSOURCEOWNER_EMULATED),
           "Got unexpected return code %#lx at test %d.\n", status, i);

        status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
        /* If don't sleep, D3DKMTCheckVidPnExclusiveOwnership may get STATUS_GRAPHICS_PRESENT_UNOCCLUDED instead
         * of STATUS_SUCCESS */
        if ((tests1[i].expected_check_status == STATUS_SUCCESS && status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED))
        {
            total_time = 0;
            do
            {
                Sleep(wait_step);
                total_time += wait_step;
                status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
            } while (status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED && total_time < timeout);
        }
        ok(status == tests1[i].expected_check_status
               || (status == STATUS_GRAPHICS_PRESENT_OCCLUDED                               /* win8 */
                   && tests1[i].owner_type == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE
                   && tests1[i - 1].owner_type == D3DKMT_VIDPNSOURCEOWNER_EMULATED),
           "Got unexpected return code %#lx at test %d.\n", status, i);
    }

    /* Set owner and unset owner using different devices */
    memset(&create_device_desc2, 0, sizeof(create_device_desc2));
    create_device_desc2.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCreateDevice(&create_device_desc2);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    /* Set owner with the first device */
    set_owner_desc.hDevice = create_device_desc.hDevice;
    owner_type = D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE;
    set_owner_desc.pType = &owner_type;
    set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
    set_owner_desc.VidPnSourceCount = 1;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    ok(status == STATUS_GRAPHICS_PRESENT_OCCLUDED, "Got unexpected return code %#lx.\n", status);

    /* Unset owner with the second device */
    set_owner_desc.hDevice = create_device_desc2.hDevice;
    set_owner_desc.pType = NULL;
    set_owner_desc.pVidPnSourceId = NULL;
    set_owner_desc.VidPnSourceCount = 0;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    /* No effect */
    ok(status == STATUS_GRAPHICS_PRESENT_OCCLUDED, "Got unexpected return code %#lx.\n", status);

    /* Unset owner with the first device */
    set_owner_desc.hDevice = create_device_desc.hDevice;
    set_owner_desc.pType = NULL;
    set_owner_desc.pVidPnSourceId = NULL;
    set_owner_desc.VidPnSourceCount = 0;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    /* Proves that the correct device is needed to unset owner */
    ok(status == STATUS_SUCCESS || status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED, "Got unexpected return code %#lx.\n",
       status);

    /* Set owner with the first device, set owner again with the second device */
    for (i = 0; i < ARRAY_SIZE(tests2); ++i)
    {
        if (tests2[i].set_owner_type1 != -1)
        {
            set_owner_desc.hDevice = create_device_desc.hDevice;
            owner_type = tests2[i].set_owner_type1;
            set_owner_desc.pType = &owner_type;
            set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
            set_owner_desc.VidPnSourceCount = 1;
            /* If don't sleep, D3DKMTSetVidPnSourceOwner may return STATUS_OK for D3DKMT_VIDPNSOURCEOWNER_SHARED.
             * Other owner type doesn't seems to be affected. */
            if (tests2[i].set_owner_type1 == D3DKMT_VIDPNSOURCEOWNER_SHARED)
                Sleep(timeout);
            status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
            ok(status == tests2[i].expected_set_status1
                   || (status == STATUS_INVALID_PARAMETER                                    /* win8 */
                       && tests2[i].set_owner_type1 == D3DKMT_VIDPNSOURCEOWNER_EMULATED),
               "Got unexpected return code %#lx at test %d.\n", status, i);
        }

        if (tests2[i].set_owner_type2 != -1)
        {
            set_owner_desc.hDevice = create_device_desc2.hDevice;
            owner_type = tests2[i].set_owner_type2;
            set_owner_desc.pType = &owner_type;
            set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
            set_owner_desc.VidPnSourceCount = 1;
            status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
            ok(status == tests2[i].expected_set_status2
                   || (status == STATUS_INVALID_PARAMETER                                   /* win8 */
                       && tests2[i].set_owner_type2 == D3DKMT_VIDPNSOURCEOWNER_EMULATED)
                   || (status == STATUS_SUCCESS && tests2[i].set_owner_type1 == D3DKMT_VIDPNSOURCEOWNER_EMULATED
                       && tests2[i].set_owner_type2 == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE),
               "Got unexpected return code %#lx at test %d.\n", status, i);
        }

        status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
        if ((tests2[i].expected_check_status == STATUS_SUCCESS && status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED))
        {
            total_time = 0;
            do
            {
                Sleep(wait_step);
                total_time += wait_step;
                status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
            } while (status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED && total_time < timeout);
        }
        ok(status == tests2[i].expected_check_status
               || (status == STATUS_GRAPHICS_PRESENT_OCCLUDED                               /* win8 */
                   && tests2[i].set_owner_type2 == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE
                   && tests2[i].set_owner_type1 == D3DKMT_VIDPNSOURCEOWNER_EMULATED),
           "Got unexpected return code %#lx at test %d.\n", status, i);

        /* Unset owner with first device */
        if (tests2[i].set_owner_type1 != -1)
        {
            set_owner_desc.hDevice = create_device_desc.hDevice;
            set_owner_desc.pType = NULL;
            set_owner_desc.pVidPnSourceId = NULL;
            set_owner_desc.VidPnSourceCount = 0;
            status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
            ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx at test %d.\n", status, i);
        }

        /* Unset owner with second device */
        if (tests2[i].set_owner_type2 != -1)
        {
            set_owner_desc.hDevice = create_device_desc2.hDevice;
            set_owner_desc.pType = NULL;
            set_owner_desc.pVidPnSourceId = NULL;
            set_owner_desc.VidPnSourceCount = 0;
            status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
            ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx at test %d.\n", status, i);
        }
    }

    /* Destroy devices holding ownership */
    set_owner_desc.hDevice = create_device_desc.hDevice;
    owner_type = D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE;
    set_owner_desc.pType = &owner_type;
    set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
    set_owner_desc.VidPnSourceCount = 1;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    destroy_device_desc.hDevice = create_device_desc.hDevice;
    status = pD3DKMTDestroyDevice(&destroy_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    set_owner_desc.hDevice = create_device_desc2.hDevice;
    owner_type = D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE;
    set_owner_desc.pType = &owner_type;
    set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
    set_owner_desc.VidPnSourceCount = 1;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    /* So ownership is released when device is destroyed. otherwise the return code should be
     * STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE */
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    destroy_device_desc.hDevice = create_device_desc2.hDevice;
    status = pD3DKMTDestroyDevice(&destroy_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    close_adapter_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
}

static void test_D3DKMTSetVidPnSourceOwner(void)
{
    D3DKMT_SETVIDPNSOURCEOWNER set_owner_desc = {0};
    NTSTATUS status;

    if (!pD3DKMTSetVidPnSourceOwner || pD3DKMTSetVidPnSourceOwner(&set_owner_desc) == STATUS_PROCEDURE_NOT_FOUND)
    {
        /* This is a stub in some drivers (e.g. nulldrv) */
        skip("D3DKMTSetVidPnSourceOwner() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);
}

static void test_D3DKMTCheckOcclusion(void)
{
    DISPLAY_DEVICEW display_device = {sizeof(display_device)};
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter_gdi_desc;
    D3DKMT_CHECKVIDPNEXCLUSIVEOWNERSHIP check_owner_desc;
    D3DKMT_SETVIDPNSOURCEOWNER set_owner_desc;
    D3DKMT_DESTROYDEVICE destroy_device_desc;
    D3DKMT_VIDPNSOURCEOWNER_TYPE owner_type;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    D3DKMT_CREATEDEVICE create_device_desc;
    D3DKMT_CHECKOCCLUSION occlusion_desc;
    NTSTATUS expected_occlusion, status;
    INT i, adapter_count = 0;
    HWND hwnd, hwnd2;
    HRESULT hr;

    if (!pD3DKMTCheckOcclusion || pD3DKMTCheckOcclusion(NULL) == STATUS_PROCEDURE_NOT_FOUND)
    {
        todo_wine win_skip("D3DKMTCheckOcclusion() is unavailable.\n");
        return;
    }

    /* NULL parameter check */
    status = pD3DKMTCheckOcclusion(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    occlusion_desc.hWnd = NULL;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    hwnd = CreateWindowA("static", "static1", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    ok(hwnd != NULL, "Failed to create window.\n");

    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    /* Minimized state doesn't affect D3DKMTCheckOcclusion */
    ShowWindow(hwnd, SW_MINIMIZE);
    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    flaky
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    ShowWindow(hwnd, SW_SHOWNORMAL);

    /* Invisible state doesn't affect D3DKMTCheckOcclusion */
    ShowWindow(hwnd, SW_HIDE);
    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    ShowWindow(hwnd, SW_SHOW);

    /* hwnd2 covers hwnd */
    hwnd2 = CreateWindowA("static", "static2", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 200, 200, 0, 0, 0, 0);
    ok(hwnd2 != NULL, "Failed to create window.\n");

    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    occlusion_desc.hWnd = hwnd2;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    /* Composition doesn't affect D3DKMTCheckOcclusion */
    if (pDwmEnableComposition)
    {
        hr = pDwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
        ok(hr == S_OK, "Failed to disable composition.\n");

        occlusion_desc.hWnd = hwnd;
        status = pD3DKMTCheckOcclusion(&occlusion_desc);
        /* This result means that D3DKMTCheckOcclusion doesn't check composition status despite MSDN says it will */
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

        occlusion_desc.hWnd = hwnd2;
        status = pD3DKMTCheckOcclusion(&occlusion_desc);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

        ShowWindow(hwnd, SW_MINIMIZE);
        occlusion_desc.hWnd = hwnd;
        status = pD3DKMTCheckOcclusion(&occlusion_desc);
        flaky
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
        ShowWindow(hwnd, SW_SHOWNORMAL);

        ShowWindow(hwnd, SW_HIDE);
        occlusion_desc.hWnd = hwnd;
        status = pD3DKMTCheckOcclusion(&occlusion_desc);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
        ShowWindow(hwnd, SW_SHOW);

        hr = pDwmEnableComposition(DWM_EC_ENABLECOMPOSITION);
        ok(hr == S_OK, "Failed to enable composition.\n");
    }
    else
        skip("Skip testing composition.\n");

    lstrcpyW(open_adapter_gdi_desc.DeviceName, display1W);
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    memset(&create_device_desc, 0, sizeof(create_device_desc));
    create_device_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCreateDevice(&create_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    check_owner_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    check_owner_desc.VidPnSourceId = open_adapter_gdi_desc.VidPnSourceId;
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    /* D3DKMTCheckVidPnExclusiveOwnership gets STATUS_GRAPHICS_PRESENT_UNOCCLUDED sometimes and with some delay,
     * it will always return STATUS_SUCCESS. So there are some timing issues here. */
    ok(status == STATUS_SUCCESS || status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED, "Got unexpected return code %#lx.\n", status);

    /* Test D3DKMTCheckOcclusion relationship with video present source owner */
    set_owner_desc.hDevice = create_device_desc.hDevice;
    owner_type = D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE;
    set_owner_desc.pType = &owner_type;
    set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
    set_owner_desc.VidPnSourceCount = 1;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    for (i = 0; EnumDisplayDevicesW(NULL, i, &display_device, 0); ++i)
    {
        if ((display_device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
            adapter_count++;
    }
    /* STATUS_GRAPHICS_PRESENT_OCCLUDED on single monitor system. STATUS_SUCCESS on multiple monitor system. */
    expected_occlusion = adapter_count > 1 ? STATUS_SUCCESS : STATUS_GRAPHICS_PRESENT_OCCLUDED;

    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == expected_occlusion, "Got unexpected return code %#lx.\n", status);

    /* Note hwnd2 is not actually occluded but D3DKMTCheckOcclusion reports STATUS_GRAPHICS_PRESENT_OCCLUDED as well */
    SetWindowPos(hwnd2, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(hwnd2, SW_SHOW);
    occlusion_desc.hWnd = hwnd2;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == expected_occlusion, "Got unexpected return code %#lx.\n", status);

    /* Now hwnd is HWND_TOPMOST. Still reports STATUS_GRAPHICS_PRESENT_OCCLUDED */
    ok(SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE), "Failed to SetWindowPos.\n");
    ok(GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST, "No WS_EX_TOPMOST style.\n");
    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == expected_occlusion, "Got unexpected return code %#lx.\n", status);

    DestroyWindow(hwnd2);
    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == expected_occlusion, "Got unexpected return code %#lx.\n", status);

    check_owner_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    check_owner_desc.VidPnSourceId = open_adapter_gdi_desc.VidPnSourceId;
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    ok(status == STATUS_GRAPHICS_PRESENT_OCCLUDED, "Got unexpected return code %#lx.\n", status);

    /* Unset video present source owner */
    set_owner_desc.hDevice = create_device_desc.hDevice;
    set_owner_desc.pType = NULL;
    set_owner_desc.pVidPnSourceId = NULL;
    set_owner_desc.VidPnSourceCount = 0;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    flaky
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    check_owner_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    check_owner_desc.VidPnSourceId = open_adapter_gdi_desc.VidPnSourceId;
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    flaky
    ok(status == STATUS_SUCCESS || status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED, "Got unexpected return code %#lx.\n", status);

    destroy_device_desc.hDevice = create_device_desc.hDevice;
    status = pD3DKMTDestroyDevice(&destroy_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    close_adapter_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    DestroyWindow(hwnd);
}

static void test_D3DKMTOpenAdapterFromDeviceName_deviface(const GUID *devinterface_guid,
        NTSTATUS expected_status, BOOL todo)
{
    BYTE iface_detail_buffer[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) + 256 * sizeof(WCHAR)];
    SP_DEVINFO_DATA device_data = {sizeof(device_data)};
    SP_DEVICE_INTERFACE_DATA iface = {sizeof(iface)};
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *iface_data;
    D3DKMT_OPENADAPTERFROMDEVICENAME device_name;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    DEVPROPTYPE type;
    NTSTATUS status;
    unsigned int i;
    HDEVINFO set;
    LUID luid;
    BOOL ret;

    set = SetupDiGetClassDevsW(devinterface_guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    ok(set != INVALID_HANDLE_VALUE, "SetupDiGetClassDevs failed, error %lu.\n", GetLastError());

    iface_data = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *)iface_detail_buffer;
    iface_data->cbSize = sizeof(*iface_data);
    device_name.pDeviceName = iface_data->DevicePath;

    i = 0;
    while (SetupDiEnumDeviceInterfaces(set, NULL, devinterface_guid, i, &iface))
    {
        ret = SetupDiGetDeviceInterfaceDetailW(set, &iface, iface_data,
                sizeof(iface_detail_buffer), NULL, &device_data );
        ok(ret, "Got unexpected ret %d, GetLastError() %lu.\n", ret, GetLastError());

        status = pD3DKMTOpenAdapterFromDeviceName(&device_name);
        todo_wine_if(todo) ok(status == expected_status, "Got status %#lx, expected %#lx.\n", status, expected_status);

        if (!status)
        {
            ret = SetupDiGetDevicePropertyW(set, &device_data, &DEVPROPKEY_GPU_LUID, &type,
                    (BYTE *)&luid, sizeof(luid), NULL, 0);
            ok(ret || GetLastError() == ERROR_NOT_FOUND, "Got unexpected ret %d, GetLastError() %lu.\n",
                    ret, GetLastError());

            if (ret)
            {
                ret = RtlEqualLuid( &luid, &device_name.AdapterLuid);
                todo_wine ok(ret, "Luid does not match.\n");
            }
            else
            {
                skip("Luid not found.\n");
            }

            close_adapter_desc.hAdapter = device_name.hAdapter;
            status = pD3DKMTCloseAdapter(&close_adapter_desc);
            ok(!status, "Got unexpected status %#lx.\n", status);
        }
        ++i;
    }
    if (!i)
        win_skip("No devices found.\n");

    SetupDiDestroyDeviceInfoList( set );
}

static void test_D3DKMTOpenAdapterFromDeviceName(void)
{
    D3DKMT_OPENADAPTERFROMDEVICENAME device_name;
    NTSTATUS status;

    /* Make sure display devices are initialized. */
    SendMessageW(GetDesktopWindow(), WM_NULL, 0, 0);

    status = pD3DKMTOpenAdapterFromDeviceName(NULL);
    if (status == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTOpenAdapterFromDeviceName() is not supported.\n");
        return;
    }
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected status %#lx.\n", status);

    memset(&device_name, 0, sizeof(device_name));
    status = pD3DKMTOpenAdapterFromDeviceName(&device_name);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected status %#lx.\n", status);

    winetest_push_context("GUID_DEVINTERFACE_DISPLAY_ADAPTER");
    test_D3DKMTOpenAdapterFromDeviceName_deviface(&GUID_DEVINTERFACE_DISPLAY_ADAPTER, STATUS_INVALID_PARAMETER, TRUE);
    winetest_pop_context();

    winetest_push_context("GUID_DISPLAY_DEVICE_ARRIVAL");
    test_D3DKMTOpenAdapterFromDeviceName_deviface(&GUID_DISPLAY_DEVICE_ARRIVAL, STATUS_SUCCESS, FALSE);
    winetest_pop_context();
}

static void test_D3DKMTQueryVideoMemoryInfo(void)
{
    static const D3DKMT_MEMORY_SEGMENT_GROUP groups[] = {D3DKMT_MEMORY_SEGMENT_GROUP_LOCAL,
                                                         D3DKMT_MEMORY_SEGMENT_GROUP_NON_LOCAL};
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter_desc;
    D3DKMT_QUERYVIDEOMEMORYINFO query_memory_info;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    NTSTATUS status;
    unsigned int i;
    BOOL ret;

    if (!pD3DKMTQueryVideoMemoryInfo)
    {
        win_skip("D3DKMTQueryVideoMemoryInfo() is unavailable.\n");
        return;
    }

    ret = get_primary_adapter_name(open_adapter_desc.DeviceName);
    ok(ret, "Failed to get primary adapter name.\n");
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    /* Normal query */
    for (i = 0; i < ARRAY_SIZE(groups); ++i)
    {
        winetest_push_context("group %d", groups[i]);

        query_memory_info.hProcess = NULL;
        query_memory_info.hAdapter = open_adapter_desc.hAdapter;
        query_memory_info.PhysicalAdapterIndex = 0;
        query_memory_info.MemorySegmentGroup = groups[i];
        status = pD3DKMTQueryVideoMemoryInfo(&query_memory_info);
        todo_wine_if (status == STATUS_INVALID_PARAMETER)  /* fails on Wine without a Vulkan adapter */
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
        ok(query_memory_info.Budget >= query_memory_info.AvailableForReservation,
           "Unexpected budget %I64u and reservation %I64u.\n", query_memory_info.Budget,
           query_memory_info.AvailableForReservation);
        ok(query_memory_info.CurrentUsage <= query_memory_info.Budget,
           "Unexpected current usage %I64u.\n", query_memory_info.CurrentUsage);
        ok(query_memory_info.CurrentReservation == 0,
           "Unexpected current reservation %I64u.\n", query_memory_info.CurrentReservation);

        winetest_pop_context();
    }

    /* Query using the current process handle */
    query_memory_info.hProcess = GetCurrentProcess();
    status = pD3DKMTQueryVideoMemoryInfo(&query_memory_info);
    todo_wine_if (status == STATUS_INVALID_PARAMETER)  /* fails on Wine without a Vulkan adapter */
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    /* Query using a process handle without PROCESS_QUERY_INFORMATION privilege */
    query_memory_info.hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, GetCurrentProcessId());
    ok(!!query_memory_info.hProcess, "OpenProcess failed, error %ld.\n", GetLastError());
    status = pD3DKMTQueryVideoMemoryInfo(&query_memory_info);
    ok(status == STATUS_ACCESS_DENIED, "Got unexpected return code %#lx.\n", status);
    CloseHandle(query_memory_info.hProcess);
    query_memory_info.hProcess = NULL;

    /* Query using an invalid process handle */
    query_memory_info.hProcess = (HANDLE)0xdeadbeef;
    status = pD3DKMTQueryVideoMemoryInfo(&query_memory_info);
    ok(status == STATUS_INVALID_HANDLE, "Got unexpected return code %#lx.\n", status);
    query_memory_info.hProcess = NULL;

    /* Query using an invalid adapter handle */
    query_memory_info.hAdapter = (D3DKMT_HANDLE)0xdeadbeef;
    status = pD3DKMTQueryVideoMemoryInfo(&query_memory_info);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);
    query_memory_info.hAdapter = open_adapter_desc.hAdapter;

    /* Query using an invalid adapter index */
    query_memory_info.PhysicalAdapterIndex = 99;
    status = pD3DKMTQueryVideoMemoryInfo(&query_memory_info);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);
    query_memory_info.PhysicalAdapterIndex = 0;

    /* Query using an invalid memory segment group */
    query_memory_info.MemorySegmentGroup = D3DKMT_MEMORY_SEGMENT_GROUP_NON_LOCAL + 1;
    status = pD3DKMTQueryVideoMemoryInfo(&query_memory_info);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

    close_adapter_desc.hAdapter = open_adapter_desc.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
}

static void test_D3DKMTCreateAllocation(void)
{
    static D3DKMT_HANDLE next_local = -1;
    OBJECT_ATTRIBUTES attr = {.Length = sizeof(attr)};
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter = {0};
    D3DKMT_CREATESTANDARDALLOCATION standard[2] = {0};
    D3DDDI_OPENALLOCATIONINFO2 open_alloc2 = {0};
    D3DDDI_OPENALLOCATIONINFO open_alloc = {0};
    D3DKMT_DESTROYDEVICE destroy_device = {0};
    D3DKMT_DESTROYALLOCATION2 destroy2 = {0};
    D3DKMT_CREATEDEVICE create_device = {0};
    D3DKMT_CLOSEADAPTER close_adapter = {0};
    D3DDDI_ALLOCATIONINFO2 allocs2[2] = {0};
    D3DKMT_DESTROYALLOCATION destroy = {0};
    D3DDDI_ALLOCATIONINFO allocs[2] = {0};
    D3DKMT_CREATEALLOCATION create = {0};
    D3DKMT_QUERYRESOURCEINFO query = {0};
    D3DKMT_OPENRESOURCE open = {0};
    const char expect_driver_data[] = {1,2,3,4,5,6};
    const char expect_runtime_data[] = {7,8,9,10};
    char driver_data[128], runtime_data[128], alloc_data[128], total_driver[128];
    NTSTATUS status;

#define CHECK_D3DKMT_HANDLE(a, b) \
    do { \
        D3DKMT_HANDLE handle = (a); \
        todo_wine ok(handle & 0xc0000000, "got %#x\n", handle); \
        if (b) todo_wine ok((handle & 0x3f) == 2, "got %#x\n", handle); \
        else \
        { \
            todo_wine_if(handle) ok(!(handle & 0x3f), "got %#x\n", handle); \
            if (next_local != -1) todo_wine ok(handle == next_local, "got %#x, expected %#x\n", handle, next_local); \
            next_local = handle + 0x40; \
        } \
    } while (0)

/* static NTSTATUS (WINAPI *pD3DKMTOpenResourceFromNtHandle)( D3DKMT_OPENRESOURCEFROMNTHANDLE *params ); */
/* static NTSTATUS (WINAPI *pD3DKMTQueryResourceInfoFromNtHandle)( D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *params ); */

    if (!pD3DKMTCreateAllocation)
    {
        win_skip("D3DKMTCreateAllocation() is unavailable.\n");
        return;
    }

    if (0) /* crashes */ status = pD3DKMTCreateAllocation(NULL);

    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    wcscpy(open_adapter.DeviceName, L"\\\\.\\DISPLAY1");
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    create_device.hAdapter = open_adapter.hAdapter;
    status = pD3DKMTCreateDevice(&create_device);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);


    allocs[0].pSystemMem = allocs2[0].pSystemMem = VirtualAlloc( NULL, 0x10000, MEM_COMMIT, PAGE_READWRITE );
    allocs[1].pSystemMem = allocs2[1].pSystemMem = VirtualAlloc( NULL, 0x10000, MEM_COMMIT, PAGE_READWRITE );
    standard[0].Type = D3DKMT_STANDARDALLOCATIONTYPE_EXISTINGHEAP;
    standard[0].ExistingHeapData.Size = 0x10000;
    standard[1].Type = D3DKMT_STANDARDALLOCATIONTYPE_EXISTINGHEAP;
    standard[1].ExistingHeapData.Size = 0x10000;


    create.hDevice = create_device.hDevice;
    create.Flags.ExistingSysMem = 1;
    create.Flags.StandardAllocation = 1;
    create.pStandardAllocation = standard;
    create.NumAllocations = 1;
    create.pAllocationInfo = allocs;
    allocs[0].hAllocation = create.hGlobalShare = 0x1eadbeed;
    create.hPrivateRuntimeResourceHandle = (HANDLE)0xdeadbeef;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    todo_wine ok(!create.hGlobalShare, "got hGlobalShare %#x\n", create.hGlobalShare);
    ok(!create.hResource, "got hResource %#x\n", create.hResource);
    CHECK_D3DKMT_HANDLE(allocs[0].hAllocation, FALSE);
    ok(create.hPrivateRuntimeResourceHandle == (HANDLE)0xdeadbeef, "got hPrivateRuntimeResourceHandle %p\n",
       create.hPrivateRuntimeResourceHandle);

    destroy.hDevice = create_device.hDevice;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    destroy.AllocationCount = 1;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    destroy.phAllocationList = &allocs[0].hAllocation;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    /* allocation has already been destroyed */
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);


    /* D3DKMTCreateAllocation2 also works with the same parameters, with extra alloc info */
    create.pAllocationInfo2 = allocs2;
    allocs2[0].hAllocation = create.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation2(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    todo_wine ok(!create.hGlobalShare, "got hGlobalShare %#x\n", create.hGlobalShare);
    ok(!create.hResource, "got hResource %#x\n", create.hResource);
    CHECK_D3DKMT_HANDLE(allocs2[0].hAllocation, FALSE);
    ok(allocs2[0].GpuVirtualAddress == 0, "got GpuVirtualAddress %#I64x\n", allocs2[0].GpuVirtualAddress);
    ok(create.PrivateRuntimeDataSize == 0, "got PrivateRuntimeDataSize %u\n", create.PrivateRuntimeDataSize);
    ok(create.PrivateDriverDataSize == 0, "got PrivateDriverDataSize %u\n", create.PrivateDriverDataSize);
    ok(allocs2[0].PrivateDriverDataSize == 0, "got PrivateDriverDataSize %u\n", allocs2[0].PrivateDriverDataSize);
    create.pAllocationInfo = allocs;

    destroy.phAllocationList = &allocs2[0].hAllocation;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    destroy.phAllocationList = &allocs[0].hAllocation;


    /* D3DKMTDestroyAllocation2 works as well */
    allocs[0].hAllocation = create.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    todo_wine ok(!create.hGlobalShare, "got hGlobalShare %#x\n", create.hGlobalShare);
    ok(!create.hResource, "got hResource %#x\n", create.hResource);
    CHECK_D3DKMT_HANDLE(allocs[0].hAllocation, FALSE);

    destroy2.hDevice = create_device.hDevice;
    destroy2.AllocationCount = 1;
    destroy2.phAllocationList = &allocs[0].hAllocation;
    status = pD3DKMTDestroyAllocation2(&destroy2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


    /* alloc PrivateDriverDataSize can be set */
    allocs[0].pPrivateDriverData = (char *)expect_driver_data;
    allocs[0].PrivateDriverDataSize = sizeof(expect_driver_data);
    allocs[0].hAllocation = create.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    todo_wine ok(!create.hGlobalShare, "got hGlobalShare %#x\n", create.hGlobalShare);
    ok(!create.hResource, "got hResource %#x\n", create.hResource);
    CHECK_D3DKMT_HANDLE(allocs[0].hAllocation, FALSE);
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    /* PrivateRuntimeDataSize can be set */
    create.pPrivateRuntimeData = expect_runtime_data;
    create.PrivateRuntimeDataSize = sizeof(expect_runtime_data);
    allocs[0].hAllocation = create.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    todo_wine ok(!create.hGlobalShare, "got hGlobalShare %#x\n", create.hGlobalShare);
    ok(!create.hResource, "got hResource %#x\n", create.hResource);
    CHECK_D3DKMT_HANDLE(allocs[0].hAllocation, FALSE);
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


    /* PrivateDriverDataSize must be 0 for standard allocations */
    create.PrivateDriverDataSize = 64;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.PrivateDriverDataSize = 0;

    /* device handle is required */
    create.hDevice = 0;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.hDevice = create_device.hDevice;

    /* hResource must be valid or 0 */
    create.hResource = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_HANDLE, "got %#lx\n", status);
    create.hResource = 0;

    /* NumAllocations is required */
    create.NumAllocations = 0;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.NumAllocations = 1;

    /* standard.Type must be set */
    standard[0].Type = 0;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    standard[0].Type = D3DKMT_STANDARDALLOCATIONTYPE_EXISTINGHEAP;

    /* pSystemMem must be set */
    allocs[0].pSystemMem = 0;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    allocs[0].pSystemMem = allocs2[0].pSystemMem;

    /* creating multiple allocations doesn't work */
    create.NumAllocations = 2;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.NumAllocations = 1;

    /* ExistingHeapData.Size must be page aligned */
    standard[0].ExistingHeapData.Size = 0x1100;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    standard[0].ExistingHeapData.Size = 0x10000;

    /* specific flags are required for standard allocations */
    create.Flags.ExistingSysMem = 0;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.Flags.ExistingSysMem = 1;
    create.Flags.StandardAllocation = 0;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.Flags.StandardAllocation = 1;

    /* CreateShared doesn't work without CreateResource */
    create.Flags.CreateShared = 1;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.Flags.CreateShared = 0;


    /* test creating a resource with the allocations */
    create.Flags.CreateResource = 1;
    allocs[0].hAllocation = create.hGlobalShare = create.hResource = 0x1eadbeed;
    create.hPrivateRuntimeResourceHandle = (HANDLE)0xdeadbeef;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_HANDLE, "got %#lx\n", status);
    create.hResource = 0; /* hResource must be set to 0, even with CreateResource */
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    todo_wine ok(!create.hGlobalShare, "got hGlobalShare %#x\n", create.hGlobalShare);
    CHECK_D3DKMT_HANDLE(create.hResource, FALSE);
    CHECK_D3DKMT_HANDLE(allocs[0].hAllocation, FALSE);
    ok(create.hPrivateRuntimeResourceHandle == (HANDLE)0xdeadbeef, "got hPrivateRuntimeResourceHandle %p\n",
       create.hPrivateRuntimeResourceHandle);

    /* destroying the allocation doesn't destroys the resource */
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    destroy.hResource = create.hResource;
    destroy.AllocationCount = 0;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    destroy.AllocationCount = 1;
    create.hResource = 0;


    create.Flags.CreateResource = 1;
    allocs[0].hAllocation = create.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    todo_wine ok(!create.hGlobalShare, "got hGlobalShare %#x\n", create.hGlobalShare);
    CHECK_D3DKMT_HANDLE(create.hResource, FALSE);
    CHECK_D3DKMT_HANDLE(allocs[0].hAllocation, FALSE);

    /* cannot create allocations with an existing resource */
    create.Flags.CreateResource = 0;
    create.pAllocationInfo = &allocs[1];
    allocs[1].hAllocation = create.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.pAllocationInfo = &allocs[0];

    /* destroying resource destroys its allocations */
    destroy.hResource = create.hResource;
    destroy.AllocationCount = 0;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    destroy.hResource = 0;
    destroy.AllocationCount = 1;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.hResource = 0;


    /* cannot create a resource without allocations */
    create.Flags.CreateResource = 1;
    create.NumAllocations = 0;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.NumAllocations = 1;

    /* destroy resource at once from here */
    destroy.AllocationCount = 0;


    allocs[0].hAllocation = create.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    todo_wine ok(!create.hGlobalShare, "got hGlobalShare %#x\n", create.hGlobalShare);
    CHECK_D3DKMT_HANDLE(create.hResource, FALSE);
    CHECK_D3DKMT_HANDLE(allocs[0].hAllocation, FALSE);

    /* D3DKMTQueryResourceInfo requires a global handle */
    query.hDevice = create_device.hDevice;
    query.hGlobalShare = create.hResource;
    status = pD3DKMTQueryResourceInfo(&query);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    /* D3DKMTOpenResource requires a global handle */
    open.hDevice = create_device.hDevice;
    open.hGlobalShare = create.hResource;
    status = pD3DKMTOpenResource(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    destroy.hResource = create.hResource;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    create.hResource = 0;


    /* test creating global shared resource */
    create.Flags.CreateShared = 1;
    allocs[0].hAllocation = create.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create.hGlobalShare, TRUE);
    CHECK_D3DKMT_HANDLE(create.hResource, FALSE);
    CHECK_D3DKMT_HANDLE(allocs[0].hAllocation, FALSE);

    /* D3DKMTQueryResourceInfo works with global handle */
    memset(runtime_data, 0xcd, sizeof(runtime_data));
    query.hDevice = create_device.hDevice;
    query.hGlobalShare = create.hGlobalShare;
    query.pPrivateRuntimeData = runtime_data;
    query.PrivateRuntimeDataSize = sizeof(runtime_data);
    status = pD3DKMTQueryResourceInfo(&query);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    todo_wine ok(query.PrivateRuntimeDataSize == sizeof(expect_runtime_data),
       "got PrivateRuntimeDataSize %u\n", query.PrivateRuntimeDataSize);
    todo_wine ok(query.TotalPrivateDriverDataSize == 96,
       "got TotalPrivateDriverDataSize %u\n", query.TotalPrivateDriverDataSize);
    ok(query.ResourcePrivateDriverDataSize == 0,
       "got ResourcePrivateDriverDataSize %u\n", query.ResourcePrivateDriverDataSize);
    todo_wine ok(query.NumAllocations == 1, "got NumAllocations %u\n", query.NumAllocations);
    /* runtime data doesn't get updated ? */
    ok(runtime_data[0] == (char)0xcd, "got data %d\n", runtime_data[0]);

    /* D3DKMTOpenResource works with a global handle */
    memset(runtime_data, 0xcd, sizeof(runtime_data));
    memset(total_driver, 0xcd, sizeof(total_driver));
    memset(driver_data, 0xcd, sizeof(driver_data));
    memset(alloc_data, 0xcd, sizeof(alloc_data));
    open.hDevice = create_device.hDevice;
    open.hGlobalShare = create.hGlobalShare;
    open.pPrivateRuntimeData = runtime_data;
    open.PrivateRuntimeDataSize = query.PrivateRuntimeDataSize;
    open.pTotalPrivateDriverDataBuffer = total_driver;
    open.TotalPrivateDriverDataBufferSize = sizeof(total_driver);
    open.pResourcePrivateDriverData = driver_data;
    open.ResourcePrivateDriverDataSize = query.ResourcePrivateDriverDataSize;
    open.NumAllocations = 1;
    open.pOpenAllocationInfo = &open_alloc;
    open_alloc.pPrivateDriverData = alloc_data;
    open_alloc.PrivateDriverDataSize = sizeof(alloc_data);
    open.hResource = 0x1eadbeed;
    status = pD3DKMTOpenResource(&open);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(open.hGlobalShare == create.hGlobalShare, "got hGlobalShare %#x\n", open.hGlobalShare);
    CHECK_D3DKMT_HANDLE(open.hResource, FALSE);
    todo_wine ok(open.PrivateRuntimeDataSize == sizeof(expect_runtime_data),
       "got PrivateRuntimeDataSize %u\n", open.PrivateRuntimeDataSize);
    todo_wine ok(open.TotalPrivateDriverDataBufferSize == 96,
       "got TotalPrivateDriverDataBufferSize %u\n", open.TotalPrivateDriverDataBufferSize);
    ok(open.ResourcePrivateDriverDataSize == 0,
       "got ResourcePrivateDriverDataSize %u\n", open.ResourcePrivateDriverDataSize);
    ok(open.NumAllocations == 1, "got NumAllocations %u\n", open.NumAllocations);
    CHECK_D3DKMT_HANDLE(open_alloc.hAllocation, FALSE);
    todo_wine ok(open_alloc.PrivateDriverDataSize == 96,
       "got PrivateDriverDataSize %u\n", open_alloc.PrivateDriverDataSize);
    todo_wine ok(!memcmp(runtime_data, expect_runtime_data, sizeof(expect_runtime_data)),
       "got data %#x\n", runtime_data[0]);
    todo_wine ok(total_driver[0] != (char)0xcd, "got data %d\n", total_driver[0]);
    ok(driver_data[0] == (char)0xcd, "got data %d\n", driver_data[0]);
    ok(alloc_data[0] == (char)0xcd, "got data %d\n", alloc_data[0]);

    destroy.hResource = open.hResource;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    open.hResource = 0;

    /* NumAllocations must be set */
    open.NumAllocations = 0;
    status = pD3DKMTOpenResource(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open.NumAllocations = 1;

    /* buffer sizes must match exactly */
    open.PrivateRuntimeDataSize += 1;
    status = pD3DKMTOpenResource(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open.PrivateRuntimeDataSize -= 1;
    open.ResourcePrivateDriverDataSize += 1;
    status = pD3DKMTOpenResource(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open.ResourcePrivateDriverDataSize -= 1;

    /* D3DKMTOpenResource2 works as well */
    open.pOpenAllocationInfo2 = &open_alloc2;
    open_alloc2.pPrivateDriverData = driver_data;
    open_alloc2.PrivateDriverDataSize = sizeof(driver_data);
    open_alloc2.hAllocation = 0x1eadbeed;
    status = pD3DKMTOpenResource2(&open);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(open.hGlobalShare == create.hGlobalShare,
       "got hGlobalShare %#x\n", open.hGlobalShare);
    CHECK_D3DKMT_HANDLE(open.hResource, FALSE);
    todo_wine ok(open.PrivateRuntimeDataSize == sizeof(expect_runtime_data),
       "got PrivateRuntimeDataSize %u\n", open.PrivateRuntimeDataSize);
    todo_wine ok(open.TotalPrivateDriverDataBufferSize == 96,
       "got TotalPrivateDriverDataBufferSize %u\n", open.TotalPrivateDriverDataBufferSize);
    ok(open.ResourcePrivateDriverDataSize == 0,
       "got ResourcePrivateDriverDataSize %u\n", open.ResourcePrivateDriverDataSize);
    ok(open.NumAllocations == 1, "got NumAllocations %u\n", open.NumAllocations);
    CHECK_D3DKMT_HANDLE(open_alloc2.hAllocation, FALSE);
    todo_wine ok(open_alloc2.PrivateDriverDataSize == 96,
       "got PrivateDriverDataSize %u\n", open_alloc2.PrivateDriverDataSize);
    open.pOpenAllocationInfo = &open_alloc;

    destroy.hResource = open.hResource;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    open.hResource = 0;

    destroy.hResource = create.hResource;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    create.hResource = 0;


    /* test creating nt shared resource */
    create.Flags.NtSecuritySharing = 1;
    create.Flags.CreateShared = 1;
    allocs[0].hAllocation = create.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    todo_wine ok(!create.hGlobalShare, "got hGlobalShare %#x\n", create.hGlobalShare);
    CHECK_D3DKMT_HANDLE(create.hResource, FALSE);
    CHECK_D3DKMT_HANDLE(allocs[0].hAllocation, FALSE);

    destroy.hResource = create.hResource;
    status = pD3DKMTDestroyAllocation(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    create.hResource = 0;


    VirtualFree((void *)allocs[0].pSystemMem, 0, MEM_RELEASE);
    VirtualFree((void *)allocs[1].pSystemMem, 0, MEM_RELEASE);


    destroy_device.hDevice = create_device.hDevice;
    pD3DKMTDestroyDevice(&destroy_device);
    close_adapter.hAdapter = open_adapter.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

#undef CHECK_D3DKMT_HANDLE
}

static void test_D3DKMTCreateKeyedMutex(void)
{
    static D3DKMT_HANDLE next_local = -1;
    D3DKMT_DESTROYKEYEDMUTEX destroy = {0};
    D3DKMT_CREATEKEYEDMUTEX2 create2 = {0};
    D3DKMT_CREATEKEYEDMUTEX create = {0};
    D3DKMT_OPENKEYEDMUTEX2 open2 = {0};
    D3DKMT_OPENKEYEDMUTEX open = {0};
    char runtime_data[] = {1,2,3,4,5,6}, buffer[64];
    NTSTATUS status;

#define CHECK_D3DKMT_HANDLE(a, b) \
    do { \
        D3DKMT_HANDLE handle = (a); \
        todo_wine ok(handle & 0xc0000000, "got %#x\n", handle); \
        if (b) todo_wine ok((handle & 0x3f) == 2, "got %#x\n", handle); \
        else \
        { \
            todo_wine ok(!(handle & 0x3f), "got %#x\n", handle); \
            if (next_local != -1) todo_wine ok(handle == next_local, "got %#x, expected %#x\n", handle, next_local); \
            next_local = handle + 0x40; \
        } \
    } while (0)

    if (!pD3DKMTCreateKeyedMutex)
    {
        win_skip("D3DKMTCreateKeyedMutex() is unavailable.\n");
        return;
    }

    status = pD3DKMTCreateKeyedMutex(NULL);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.hKeyedMutex = create.hSharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateKeyedMutex(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create.hKeyedMutex, FALSE);
    CHECK_D3DKMT_HANDLE(create.hSharedHandle, TRUE);

    status = pD3DKMTOpenKeyedMutex(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open.hKeyedMutex = create.hKeyedMutex;
    status = pD3DKMTOpenKeyedMutex(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open.hKeyedMutex = 0x1eadbeed;
    open.hSharedHandle = create.hSharedHandle;
    status = pD3DKMTOpenKeyedMutex(&open);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(open.hKeyedMutex, FALSE);

    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    if (0)
    {
        /* older W10 lets you destroy the global D3DKMT_HANDLE, it causes random failures in the tests below */
        destroy.hKeyedMutex = create.hSharedHandle;
        status = pD3DKMTDestroyKeyedMutex(&destroy);
        todo_wine ok(status == STATUS_INVALID_PARAMETER || broken(!status), "got %#lx\n", status);
    }

    destroy.hKeyedMutex = open.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    /* destroying multiple times fails */
    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    destroy.hKeyedMutex = create.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    /* the global D3DKMT_HANDLE is destroyed with last reference */
    status = pD3DKMTOpenKeyedMutex(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);


    if (!pD3DKMTCreateKeyedMutex2)
    {
        win_skip("D3DKMTCreateKeyedMutex2() is unavailable.\n");
        return;
    }

    status = pD3DKMTCreateKeyedMutex2(NULL);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create2.hKeyedMutex = create2.hSharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateKeyedMutex2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hKeyedMutex, FALSE);
    CHECK_D3DKMT_HANDLE(create2.hSharedHandle, TRUE);
    destroy.hKeyedMutex = create2.hKeyedMutex;

    create2.hKeyedMutex = create2.hSharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateKeyedMutex2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hKeyedMutex, FALSE);
    CHECK_D3DKMT_HANDLE(create2.hSharedHandle, TRUE);

    status = pD3DKMTOpenKeyedMutex2(&open2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open2.hKeyedMutex = create2.hKeyedMutex;
    status = pD3DKMTOpenKeyedMutex2(&open2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open2.hKeyedMutex = 0x1eadbeed;
    open2.hSharedHandle = create2.hSharedHandle;
    status = pD3DKMTOpenKeyedMutex2(&open2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(open2.hKeyedMutex, FALSE);

    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    destroy.hKeyedMutex = create2.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    destroy.hKeyedMutex = open2.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


    /* PrivateRuntimeDataSize must be 0 if no buffer is provided */

    status = pD3DKMTCreateKeyedMutex2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hKeyedMutex, FALSE);
    CHECK_D3DKMT_HANDLE(create2.hSharedHandle, TRUE);

    open2.hKeyedMutex = 0x1eadbeed;
    open2.hSharedHandle = create2.hSharedHandle;
    open2.PrivateRuntimeDataSize = sizeof(buffer);
    status = pD3DKMTOpenKeyedMutex2(&open2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open2.pPrivateRuntimeData = buffer;
    status = pD3DKMTOpenKeyedMutex2(&open2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(open2.hKeyedMutex, FALSE);
    ok(open2.PrivateRuntimeDataSize == sizeof(buffer),
       "got PrivateRuntimeDataSize %#x\n", open2.PrivateRuntimeDataSize);

    destroy.hKeyedMutex = open2.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    destroy.hKeyedMutex = create2.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


    create2.PrivateRuntimeDataSize = sizeof(runtime_data);
    create2.pPrivateRuntimeData = runtime_data;
    status = pD3DKMTCreateKeyedMutex2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hKeyedMutex, FALSE);
    CHECK_D3DKMT_HANDLE(create2.hSharedHandle, TRUE);

    open2.hKeyedMutex = 0x1eadbeed;
    open2.hSharedHandle = create2.hSharedHandle;
    open2.PrivateRuntimeDataSize = 0;
    open2.pPrivateRuntimeData = NULL;
    status = pD3DKMTOpenKeyedMutex2(&open2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(open2.hKeyedMutex, FALSE);
    ok(open2.PrivateRuntimeDataSize == 0,
       "got PrivateRuntimeDataSize %#x\n", open2.PrivateRuntimeDataSize);

    open2.PrivateRuntimeDataSize = sizeof(buffer);
    status = pD3DKMTOpenKeyedMutex2(&open2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open2.PrivateRuntimeDataSize = sizeof(runtime_data) - 1;
    open2.pPrivateRuntimeData = buffer;
    status = pD3DKMTOpenKeyedMutex2(&open2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open2.PrivateRuntimeDataSize = sizeof(runtime_data);
    memset(buffer, 0xcd, sizeof(buffer));
    status = pD3DKMTOpenKeyedMutex2(&open2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(open2.hKeyedMutex, FALSE);
    ok(open2.PrivateRuntimeDataSize == sizeof(runtime_data),
       "got PrivateRuntimeDataSize %#x\n", open2.PrivateRuntimeDataSize);
    ok(buffer[0] == (char)0xcd, "got data %d\n", buffer[0]);

    destroy.hKeyedMutex = open2.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    destroy.hKeyedMutex = create2.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


    /* doesn't return a global D3DKMT_HANDLE with NtSecuritySharing = 1 */
    create2.Flags.NtSecuritySharing = 1;
    create2.hKeyedMutex = create2.hSharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateKeyedMutex2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hKeyedMutex, FALSE);
    todo_wine ok(!create2.hSharedHandle, "got hSharedHandle %#x\n", create2.hSharedHandle);

    destroy.hKeyedMutex = create2.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

#undef CHECK_D3DKMT_HANDLE
}

static void test_D3DKMTCreateSynchronizationObject(void)
{
    static D3DKMT_HANDLE next_local = -1;
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter = {0};
    D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroy = {0};
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 create2 = {0};
    D3DKMT_CREATESYNCHRONIZATIONOBJECT create = {0};
    D3DKMT_OPENSYNCHRONIZATIONOBJECT open = {0};
    D3DKMT_DESTROYDEVICE destroy_device = {0};
    D3DKMT_CREATEDEVICE create_device = {0};
    D3DKMT_CLOSEADAPTER close_adapter = {0};
    NTSTATUS status;

#define CHECK_D3DKMT_HANDLE(a, b) \
    do { \
        D3DKMT_HANDLE handle = (a); \
        todo_wine ok(handle & 0xc0000000, "got %#x\n", handle); \
        if (b) todo_wine ok((handle & 0x3f) == 2, "got %#x\n", handle); \
        else \
        { \
            todo_wine ok(!(handle & 0x3f), "got %#x\n", handle); \
            if (next_local != -1) todo_wine ok(handle == next_local, "got %#x, expected %#x\n", handle, next_local); \
            next_local = handle + 0x40; \
        } \
    } while (0)

    if (!pD3DKMTCreateSynchronizationObject)
    {
        win_skip("D3DKMTCreateSynchronizationObject() is unavailable.\n");
        return;
    }

    wcscpy(open_adapter.DeviceName, L"\\\\.\\DISPLAY1");
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    create_device.hAdapter = open_adapter.hAdapter;
    status = pD3DKMTCreateDevice(&create_device);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    /* pD3DKMTCreateSynchronizationObject creates a local D3DKMT_HANDLE */
    status = pD3DKMTCreateSynchronizationObject(NULL);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    status = pD3DKMTCreateSynchronizationObject(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.hDevice = create_device.hDevice;
    status = pD3DKMTCreateSynchronizationObject(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.hDevice = 0;
    create.Info.Type = D3DDDI_SYNCHRONIZATION_MUTEX;
    status = pD3DKMTCreateSynchronizationObject(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create.hDevice = create_device.hDevice;
    create.Info.Type = D3DDDI_SYNCHRONIZATION_MUTEX;
    create.hSyncObject = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create.hSyncObject, FALSE);
    destroy.hSyncObject = create.hSyncObject;

    /* local handles are monotonically increasing */
    create.hSyncObject = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create.hSyncObject, FALSE);

    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    /* destroying multiple times fails */
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    destroy.hSyncObject = create.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    create.Info.Type = D3DDDI_SEMAPHORE;
    create.hSyncObject = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject(&create);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create.hSyncObject, FALSE);
    destroy.hSyncObject = create.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    create.Info.Type = D3DDDI_FENCE;
    status = pD3DKMTCreateSynchronizationObject(&create);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);


    if (!pD3DKMTCreateSynchronizationObject2)
    {
        win_skip("D3DKMTCreateSynchronizationObject2() is unavailable.\n");
        goto done;
    }


    /* pD3DKMTCreateSynchronizationObject2 can create local D3DKMT_HANDLE */
    status = pD3DKMTCreateSynchronizationObject2(NULL);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create2.hDevice = create_device.hDevice;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create2.hDevice = 0;
    create2.Info.Type = D3DDDI_SYNCHRONIZATION_MUTEX;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    create2.hDevice = create_device.hDevice;
    create2.Info.Type = D3DDDI_SYNCHRONIZATION_MUTEX;
    create2.hSyncObject = create2.Info.SharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hSyncObject, FALSE);
    ok(create2.Info.SharedHandle == 0x1eadbeed, "got Info.SharedHandle %#x\n", create2.Info.SharedHandle);

    destroy.hSyncObject = create2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


    create2.Info.Type = D3DDDI_SEMAPHORE;
    create2.hSyncObject = create2.Info.SharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hSyncObject, FALSE);
    destroy.hSyncObject = create2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    create2.Info.Type = D3DDDI_FENCE;
    create2.hSyncObject = create2.Info.SharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hSyncObject, FALSE);
    destroy.hSyncObject = create2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    create2.Info.Type = D3DDDI_CPU_NOTIFICATION;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_INVALID_HANDLE, "got %#lx\n", status);
    create2.Info.CPUNotification.Event = CreateEventW(NULL, FALSE, FALSE, NULL);
    create2.hSyncObject = create2.Info.SharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hSyncObject, FALSE);
    destroy.hSyncObject = create2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CloseHandle(create2.Info.CPUNotification.Event);
    create2.Info.CPUNotification.Event = NULL;

    create2.Info.Type = D3DDDI_MONITORED_FENCE;
    create2.hSyncObject = create2.Info.SharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_SUCCESS || broken(status == STATUS_INVALID_PARAMETER), "got %#lx\n", status);
    if (status == STATUS_SUCCESS)
    {
        CHECK_D3DKMT_HANDLE(create2.hSyncObject, FALSE);
        destroy.hSyncObject = create2.hSyncObject;
        status = pD3DKMTDestroySynchronizationObject(&destroy);
        todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    }


    create2.Info.Type = D3DDDI_SYNCHRONIZATION_MUTEX;
    create2.Info.Flags.Shared = 1;
    create2.hSyncObject = create2.Info.SharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hSyncObject, FALSE);
    CHECK_D3DKMT_HANDLE(create2.Info.SharedHandle, TRUE);
    destroy.hSyncObject = create2.hSyncObject;

    create2.hSyncObject = create2.Info.SharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hSyncObject, FALSE);
    CHECK_D3DKMT_HANDLE(create2.Info.SharedHandle, TRUE);

    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    /* cannot destroy the global D3DKMT_HANDLE */
    destroy.hSyncObject = create2.Info.SharedHandle;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);


    /* D3DKMTOpenSynchronizationObject creates a new local D3DKMT_HANDLE */
    open.hSharedHandle = 0x1eadbeed;
    status = pD3DKMTOpenSynchronizationObject(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open.hSharedHandle = 0;
    status = pD3DKMTOpenSynchronizationObject(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open.hSyncObject = create2.hSyncObject;
    status = pD3DKMTOpenSynchronizationObject(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open.hSyncObject = 0x1eadbeed;
    open.hSharedHandle = create2.Info.SharedHandle;
    status = pD3DKMTOpenSynchronizationObject(&open);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(open.hSyncObject, FALSE);

    destroy.hSyncObject = open.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    /* destroying multiple times fails */
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    /* the D3DKMT object can still be opened */
    open.hSyncObject = 0x1eadbeed;
    status = pD3DKMTOpenSynchronizationObject(&open);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(open.hSyncObject, FALSE);

    destroy.hSyncObject = open.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    destroy.hSyncObject = create2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    /* the global D3DKMT_HANDLE is destroyed with last reference */
    status = pD3DKMTOpenSynchronizationObject(&open);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);


    /* NtSecuritySharing requires Shared, doesn't creates a global handle */
    create2.Info.Flags.Shared = 0;
    create2.Info.Flags.NtSecuritySharing = 1;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER || broken(!status) /* W8 */, "got %#lx\n", status);
    if (broken(!status))
    {
        CHECK_D3DKMT_HANDLE(create2.hSyncObject, FALSE);
        destroy.hSyncObject = create2.hSyncObject;
        status = pD3DKMTDestroySynchronizationObject(&destroy);
        todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    }

    create2.Info.Flags.Shared = 1;
    create2.Info.Flags.NtSecuritySharing = 1;
    create2.hSyncObject = create2.Info.SharedHandle = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject2(&create2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create2.hSyncObject, FALSE);
    ok(create2.Info.SharedHandle == 0x1eadbeed || !create2.Info.SharedHandle,
       "got Info.SharedHandle %#x\n", create2.Info.SharedHandle);

    destroy.hSyncObject = create2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


done:
    destroy_device.hDevice = create_device.hDevice;
    status = pD3DKMTDestroyDevice(&destroy_device);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    close_adapter.hAdapter = open_adapter.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

#undef CHECK_D3DKMT_HANDLE
}

static void test_D3DKMTShareObjects(void)
{
    OBJECT_ATTRIBUTES attr = {.Length = sizeof(attr)};
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\BaseNamedObjects\\__WineTest_D3DKMT");
    UNICODE_STRING name2 = RTL_CONSTANT_STRING(L"\\BaseNamedObjects\\__WineTest_D3DKMT_2");
    UNICODE_STRING name_lower = RTL_CONSTANT_STRING(L"\\BaseNamedObjects\\__winetest_d3dkmt");
    UNICODE_STRING name_invalid = RTL_CONSTANT_STRING(L"\\BaseNamedObjects\\__winetest_invalid");
    D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME open_sync_name = {0};
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter = {0};
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 create_sync2 = {0};
    D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroy_sync = {0};
    D3DKMT_CREATESYNCHRONIZATIONOBJECT create_sync = {0};
    D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 open_sync2 = {0};
    D3DKMT_OPENSYNCOBJECTFROMNTHANDLE open_sync = {0};
    D3DKMT_DESTROYKEYEDMUTEX destroy_mutex = {0};
    D3DKMT_CREATEKEYEDMUTEX2 create_mutex2 = {0};
    D3DKMT_CREATEKEYEDMUTEX create_mutex = {0};
    D3DKMT_DESTROYDEVICE destroy_device = {0};
    D3DKMT_CREATEDEVICE create_device = {0};
    D3DKMT_CLOSEADAPTER close_adapter = {0};

    D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE query_resource = {0};
    D3DKMT_OPENRESOURCEFROMNTHANDLE open_resource = {0};
    D3DKMT_CREATESTANDARDALLOCATION standard = {0};
    D3DKMT_DESTROYALLOCATION destroy_alloc = {0};
    D3DDDI_OPENALLOCATIONINFO2 open_alloc = {0};
    D3DKMT_CREATEALLOCATION create_alloc = {0};
    D3DDDI_ALLOCATIONINFO alloc = {0};
    char expect_resource_data[] = {9,8,7,6};
    char expect_mutex_data[] = {0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9};
    char expect_alloc_data[] = {0,1,2,3,4,5,6,7,8,9};
    char resource_data[128], mutex_data[128], driver_data[128];
    D3DKMT_HANDLE objects[4];

    NTSTATUS status;
    HANDLE handle;

#define CHECK_D3DKMT_HANDLE(a, b) \
    do { \
        D3DKMT_HANDLE handle = (a); \
        todo_wine ok(handle & 0xc0000000, "got %#x\n", handle); \
        if (b) todo_wine ok((handle & 0x3f) == 2, "got %#x\n", handle); \
        else todo_wine ok(!(handle & 0x3f), "got %#x\n", handle); \
    } while (0)


    if (!pD3DKMTShareObjects)
    {
        win_skip("D3DKMTShareObjects() is unavailable.\n");
        return;
    }

    wcscpy(open_adapter.DeviceName, L"\\\\.\\DISPLAY1");
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
    create_device.hAdapter = open_adapter.hAdapter;
    status = pD3DKMTCreateDevice(&create_device);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);


    /* D3DKMTShareObjects doesn't work with D3DKMTCreateSynchronizationObject created objects */
    create_sync.hDevice = create_device.hDevice;
    create_sync.Info.Type = D3DDDI_SYNCHRONIZATION_MUTEX;
    create_sync.hSyncObject = 0x1eadbeed;
    status = pD3DKMTCreateSynchronizationObject(&create_sync);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    handle = (HANDLE)0xdeadbeef;
    status = pD3DKMTShareObjects(1, &create_sync.hSyncObject, &attr, STANDARD_RIGHTS_WRITE, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER || broken(!status), "got %#lx\n", status);
    if (broken(!status)) CloseHandle(handle);
    destroy_sync.hSyncObject = create_sync.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy_sync);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    /* D3DKMTShareObjects doesn't work with Shared = 1/0 alone */
    create_sync2.hDevice = create_device.hDevice;
    create_sync2.Info.Type = D3DDDI_SYNCHRONIZATION_MUTEX;
    status = pD3DKMTCreateSynchronizationObject2(&create_sync2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    handle = (HANDLE)0xdeadbeef;
    status = pD3DKMTShareObjects(1, &create_sync2.hSyncObject, &attr, STANDARD_RIGHTS_WRITE, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER || broken(!status), "got %#lx\n", status);
    if (broken(!status)) CloseHandle(handle);
    destroy_sync.hSyncObject = create_sync2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy_sync);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    create_sync2.Info.Flags.Shared = 1;
    status = pD3DKMTCreateSynchronizationObject2(&create_sync2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    handle = (HANDLE)0xdeadbeef;
    status = pD3DKMTShareObjects(1, &create_sync2.hSyncObject, &attr, STANDARD_RIGHTS_WRITE, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER || broken(!status), "got %#lx\n", status);
    if (broken(!status)) CloseHandle(handle);
    destroy_sync.hSyncObject = create_sync2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy_sync);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    /* D3DKMTShareObjects requires NtSecuritySharing (which requires Shared = 1) */
    create_sync2.Info.Flags.NtSecuritySharing = 1;
    status = pD3DKMTCreateSynchronizationObject2(&create_sync2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    InitializeObjectAttributes(&attr, &name, 0, 0, NULL);
    handle = (HANDLE)0xdeadbeef;
    status = pD3DKMTShareObjects(1, &create_sync2.hSyncObject, &attr, STANDARD_RIGHTS_WRITE, &handle);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    /* handle isn't a D3DKMT_HANDLE */
    todo_wine ok(!((UINT_PTR)handle & 0xc0000000), "got %p\n", handle);

    /* cannot destroy the handle */
    destroy_sync.hSyncObject = (UINT_PTR)handle;
    status = pD3DKMTDestroySynchronizationObject(&destroy_sync);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    destroy_sync.hSyncObject = create_sync2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy_sync);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


    /* the sync object can be opened from the NT handle */
    status = pD3DKMTOpenSyncObjectFromNtHandle(&open_sync);
    todo_wine ok(status == STATUS_INVALID_HANDLE, "got %#lx\n", status);
    open_sync.hNtHandle = handle;
    open_sync.hSyncObject = 0x1eadbeed;
    status = pD3DKMTOpenSyncObjectFromNtHandle(&open_sync);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(open_sync.hSyncObject, FALSE);

    /* objects opened with D3DKMTDestroySynchronizationObject cannot be reshared */
    InitializeObjectAttributes(&attr, &name, 0, 0, NULL);
    status = pD3DKMTShareObjects(1, &create_sync2.hSyncObject, &attr, STANDARD_RIGHTS_WRITE, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    destroy_sync.hSyncObject = open_sync.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy_sync);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


    if (!pD3DKMTOpenSyncObjectFromNtHandle2)
    {
        win_skip("D3DKMTOpenSyncObjectFromNtHandle2() is unavailable.\n");
        CloseHandle(handle);
        goto skip_tests;
    }

    status = pD3DKMTOpenSyncObjectFromNtHandle2(&open_sync2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open_sync2.hNtHandle = handle;
    status = pD3DKMTOpenSyncObjectFromNtHandle2(&open_sync2);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open_sync2.hDevice = create_device.hDevice;
    open_sync2.hSyncObject = 0x1eadbeed;
    status = pD3DKMTOpenSyncObjectFromNtHandle2(&open_sync2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(open_sync2.hSyncObject, FALSE);

    /* objects opened with Shared/NtSecuritySharing flags don't seem to matter */
    ok(open_sync2.Flags.Shared == 0, "got Flags.Shared %u\n", open_sync2.Flags.Shared);
    ok(open_sync2.Flags.NtSecuritySharing == 0, "got Flags.NtSecuritySharing %u\n", open_sync2.Flags.NtSecuritySharing);
    InitializeObjectAttributes(&attr, &name2, 0, 0, NULL);
    handle = (HANDLE)0xdeadbeef;
    status = pD3DKMTShareObjects(1, &open_sync2.hSyncObject, &attr, STANDARD_RIGHTS_READ, &handle);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CloseHandle(handle);

    destroy_sync.hSyncObject = open_sync2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy_sync);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


    status = pD3DKMTOpenSyncObjectNtHandleFromName(&open_sync_name);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    InitializeObjectAttributes(&attr, &name_invalid, 0, 0, NULL);
    open_sync_name.pObjAttrib = &attr;
    status = pD3DKMTOpenSyncObjectNtHandleFromName(&open_sync_name);
    todo_wine ok(status == STATUS_OBJECT_NAME_NOT_FOUND, "got %#lx\n", status);
    InitializeObjectAttributes(&attr, &name_lower, 0, 0, NULL);
    open_sync_name.pObjAttrib = &attr;
    status = pD3DKMTOpenSyncObjectNtHandleFromName(&open_sync_name);
    todo_wine ok(status == STATUS_ACCESS_DENIED, "got %#lx\n", status);
    open_sync_name.dwDesiredAccess = STANDARD_RIGHTS_WRITE;
    status = pD3DKMTOpenSyncObjectNtHandleFromName(&open_sync_name);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(!((UINT_PTR)open_sync_name.hNtHandle & 0xc0000000), "got %p\n", open_sync_name.hNtHandle);


    CloseHandle(open_sync2.hNtHandle);

    status = pD3DKMTOpenSyncObjectFromNtHandle2(&open_sync2);
    todo_wine ok(status == STATUS_INVALID_HANDLE, "got %#lx\n", status);
    open_sync2.hNtHandle = open_sync_name.hNtHandle;
    open_sync2.hSyncObject = 0x1eadbeed;
    status = pD3DKMTOpenSyncObjectFromNtHandle2(&open_sync2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(open_sync2.hSyncObject, FALSE);

    CloseHandle(open_sync_name.hNtHandle);


    /* cannot open the object anymore after all the NT handles have been closed */
    InitializeObjectAttributes(&attr, &name_lower, 0, 0, NULL);
    open_sync_name.pObjAttrib = &attr;
    status = pD3DKMTOpenSyncObjectNtHandleFromName(&open_sync_name);
    todo_wine ok(status == STATUS_OBJECT_NAME_NOT_FOUND, "got %#lx\n", status);

    /* but object still exists and can be re-shared */
    InitializeObjectAttributes(&attr, &name, 0, 0, NULL);
    handle = (HANDLE)0xdeadbeef;
    status = pD3DKMTShareObjects(1, &open_sync2.hSyncObject, &attr, STANDARD_RIGHTS_READ, &handle);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    /* can be opened again by name */
    open_sync_name.pObjAttrib = &attr;
    open_sync_name.dwDesiredAccess = STANDARD_RIGHTS_READ;
    status = pD3DKMTOpenSyncObjectNtHandleFromName(&open_sync_name);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    CloseHandle(open_sync_name.hNtHandle);
    CloseHandle(handle);

    destroy_sync.hSyncObject = open_sync2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy_sync);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


skip_tests:
    /* D3DKMTShareObjects doesn't work with keyed mutex objects alone */
    status = pD3DKMTCreateKeyedMutex(&create_mutex);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    status = pD3DKMTShareObjects(1, &create_mutex.hKeyedMutex, &attr, STANDARD_RIGHTS_WRITE, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    status = pD3DKMTShareObjects(1, &create_mutex.hSharedHandle, &attr, STANDARD_RIGHTS_WRITE, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    destroy_mutex.hKeyedMutex = create_mutex.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy_mutex);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    create_mutex2.Flags.NtSecuritySharing = 1;
    status = pD3DKMTCreateKeyedMutex2(&create_mutex2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    status = pD3DKMTShareObjects(1, &create_mutex2.hKeyedMutex, &attr, STANDARD_RIGHTS_WRITE, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    destroy_mutex.hKeyedMutex = create_mutex2.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy_mutex);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);


    /* NtSecuritySharing = 1 is required for D3DKMTShareObjects */

    alloc.pSystemMem = VirtualAlloc(NULL, 0x10000, MEM_COMMIT, PAGE_READWRITE);
    standard.Type = D3DKMT_STANDARDALLOCATIONTYPE_EXISTINGHEAP;
    standard.ExistingHeapData.Size = 0x10000;
    create_alloc.hDevice = create_device.hDevice;
    create_alloc.Flags.ExistingSysMem = 1;
    create_alloc.Flags.StandardAllocation = 1;
    create_alloc.Flags.CreateResource = 1;
    create_alloc.pStandardAllocation = &standard;
    create_alloc.NumAllocations = 1;
    create_alloc.pAllocationInfo = &alloc;
    create_alloc.pPrivateRuntimeData = expect_resource_data;
    create_alloc.PrivateRuntimeDataSize = sizeof(expect_resource_data);
    create_alloc.hPrivateRuntimeResourceHandle = (HANDLE)0xdeadbeef;
    alloc.pPrivateDriverData = expect_alloc_data;
    alloc.PrivateDriverDataSize = sizeof(expect_alloc_data);
    alloc.hAllocation = create_alloc.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create_alloc);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(!create_alloc.hGlobalShare, "got hGlobalShare %#x\n", create_alloc.hGlobalShare);
    CHECK_D3DKMT_HANDLE(create_alloc.hResource, FALSE);
    CHECK_D3DKMT_HANDLE(alloc.hAllocation, FALSE);

    status = pD3DKMTShareObjects(1, &alloc.hAllocation, &attr, STANDARD_RIGHTS_READ, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    status = pD3DKMTShareObjects(1, &create_alloc.hResource, &attr, STANDARD_RIGHTS_READ, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    destroy_alloc.hDevice = create_device.hDevice;
    destroy_alloc.hResource = create_alloc.hResource;
    status = pD3DKMTDestroyAllocation(&destroy_alloc);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    create_alloc.hResource = 0;


    create_alloc.Flags.CreateShared = 1;
    alloc.hAllocation = create_alloc.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create_alloc);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    CHECK_D3DKMT_HANDLE(create_alloc.hGlobalShare, TRUE);
    CHECK_D3DKMT_HANDLE(create_alloc.hResource, FALSE);
    CHECK_D3DKMT_HANDLE(alloc.hAllocation, FALSE);

    status = pD3DKMTShareObjects(1, &alloc.hAllocation, &attr, STANDARD_RIGHTS_READ, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    status = pD3DKMTShareObjects(1, &create_alloc.hResource, &attr, STANDARD_RIGHTS_READ, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    destroy_alloc.hResource = create_alloc.hResource;
    status = pD3DKMTDestroyAllocation(&destroy_alloc);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    create_alloc.hResource = 0;


    create_alloc.Flags.NtSecuritySharing = 1;
    alloc.hAllocation = create_alloc.hGlobalShare = 0x1eadbeed;
    status = pD3DKMTCreateAllocation(&create_alloc);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(!create_alloc.hGlobalShare, "got hGlobalShare %#x\n", create_alloc.hGlobalShare);
    CHECK_D3DKMT_HANDLE(create_alloc.hResource, FALSE);
    CHECK_D3DKMT_HANDLE(alloc.hAllocation, FALSE);

    /* can only share resources, not allocations */
    status = pD3DKMTShareObjects(1, &alloc.hAllocation, &attr, STANDARD_RIGHTS_READ, &handle);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    status = pD3DKMTShareObjects(1, &create_alloc.hResource, &attr, STANDARD_RIGHTS_READ, &handle);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    query_resource.hDevice = create_device.hDevice;
    query_resource.hNtHandle = handle;
    query_resource.PrivateRuntimeDataSize = 0xdeadbeef;
    query_resource.TotalPrivateDriverDataSize = 0xdeadbeef;
    query_resource.ResourcePrivateDriverDataSize = 0xdeadbeef;
    query_resource.NumAllocations = 0xdeadbeef;
    status = pD3DKMTQueryResourceInfoFromNtHandle(&query_resource);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(query_resource.PrivateRuntimeDataSize == sizeof(expect_resource_data),
       "got PrivateRuntimeDataSize %#x\n", query_resource.PrivateRuntimeDataSize);
    ok(query_resource.TotalPrivateDriverDataSize == 96,
       "got TotalPrivateDriverDataSize %#x\n", query_resource.TotalPrivateDriverDataSize);
    ok(query_resource.ResourcePrivateDriverDataSize == 0,
       "got ResourcePrivateDriverDataSize %#x\n", query_resource.ResourcePrivateDriverDataSize);
    ok(query_resource.NumAllocations == 1, "got NumAllocations %#x\n", query_resource.NumAllocations);

    memset(resource_data, 0xcd, sizeof(resource_data));
    query_resource.pPrivateRuntimeData = resource_data;
    query_resource.PrivateRuntimeDataSize = 0; /* sizeof(resource_data); */
    status = pD3DKMTQueryResourceInfoFromNtHandle(&query_resource);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(query_resource.PrivateRuntimeDataSize == sizeof(expect_resource_data),
       "got PrivateRuntimeDataSize %#x\n", query_resource.PrivateRuntimeDataSize);
    ok(resource_data[0] == (char)0xcd, "got data %d\n", resource_data[0]);

    memset(resource_data, 0xcd, sizeof(resource_data));
    memset(driver_data, 0xcd, sizeof(driver_data));
    open_resource.hDevice = create_device.hDevice;
    open_resource.hNtHandle = handle;
    open_resource.hResource = open_resource.hKeyedMutex = open_resource.hSyncObject = 0x1eadbeed;
    open_resource.NumAllocations = 1;
    open_resource.pOpenAllocationInfo2 = &open_alloc;
    open_alloc.hAllocation = 0x1eadbeed;
    open_resource.pPrivateRuntimeData = resource_data;
    open_resource.PrivateRuntimeDataSize = query_resource.PrivateRuntimeDataSize;
    open_resource.pTotalPrivateDriverDataBuffer = driver_data;
    open_resource.TotalPrivateDriverDataBufferSize = query_resource.TotalPrivateDriverDataSize;
    status = pD3DKMTOpenResourceFromNtHandle(&open_resource);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(open_resource.NumAllocations == 1, "got NumAllocations %#x\n", open_resource.NumAllocations);
    ok(open_resource.PrivateRuntimeDataSize == sizeof(expect_resource_data),
       "got PrivateRuntimeDataSize %#x\n", open_resource.PrivateRuntimeDataSize);
    ok(open_resource.ResourcePrivateDriverDataSize == 0,
       "got ResourcePrivateDriverDataSize %#x\n", open_resource.ResourcePrivateDriverDataSize);
    ok(open_resource.TotalPrivateDriverDataBufferSize == 96,
       "got TotalPrivateDriverDataBufferSize %#x\n", open_resource.TotalPrivateDriverDataBufferSize);
    CHECK_D3DKMT_HANDLE(open_resource.hResource, FALSE);
    ok(open_resource.hKeyedMutex == 0, "got hKeyedMutex %#x\n", open_resource.hKeyedMutex);
    ok(open_resource.KeyedMutexPrivateRuntimeDataSize == 0,
       "got KeyedMutexPrivateRuntimeDataSize %#x\n", open_resource.KeyedMutexPrivateRuntimeDataSize);
    ok(open_resource.hSyncObject == 0, "got hSyncObject %#x\n", open_resource.hSyncObject);
    CHECK_D3DKMT_HANDLE(open_alloc.hAllocation, FALSE);
    ok(open_alloc.PrivateDriverDataSize == 96, "got PrivateDriverDataSize %#x\n", open_alloc.PrivateDriverDataSize);
    ok(!memcmp(resource_data, expect_resource_data, sizeof(expect_resource_data)),
       "got data %#x\n", resource_data[0]);
    ok(driver_data[0] != (char)0xcd, "got data %d\n", driver_data[0]);

    destroy_alloc.hResource = open_resource.hResource;
    status = pD3DKMTDestroyAllocation(&destroy_alloc);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    open_resource.hResource = 0;

    open_resource.pOpenAllocationInfo2 = NULL;
    status = pD3DKMTOpenResourceFromNtHandle(&open_resource);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open_resource.pOpenAllocationInfo2 = &open_alloc;

    open_resource.NumAllocations = 0;
    status = pD3DKMTOpenResourceFromNtHandle(&open_resource);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open_resource.NumAllocations = 1;

    open_resource.pPrivateRuntimeData = NULL;
    status = pD3DKMTOpenResourceFromNtHandle(&open_resource);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open_resource.pPrivateRuntimeData = resource_data;

    open_resource.PrivateRuntimeDataSize += 1;
    status = pD3DKMTOpenResourceFromNtHandle(&open_resource);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open_resource.PrivateRuntimeDataSize = query_resource.PrivateRuntimeDataSize;

    open_resource.pTotalPrivateDriverDataBuffer = NULL;
    status = pD3DKMTOpenResourceFromNtHandle(&open_resource);
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);
    open_resource.pTotalPrivateDriverDataBuffer = driver_data;

    open_resource.TotalPrivateDriverDataBufferSize -= 1;
    status = pD3DKMTOpenResourceFromNtHandle(&open_resource);
    todo_wine ok(status == STATUS_NO_MEMORY, "got %#lx\n", status);
    open_resource.TotalPrivateDriverDataBufferSize = query_resource.TotalPrivateDriverDataSize;

    memset(&open_resource, 0, sizeof(open_resource));
    CloseHandle(handle);

    /* with combined objects only resource/mutex/sync order and combination works */

    create_sync2.hDevice = create_device.hDevice;
    create_sync2.Info.Type = D3DDDI_SYNCHRONIZATION_MUTEX;
    status = pD3DKMTCreateSynchronizationObject2(&create_sync2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    handle = (HANDLE)0xdeadbeef;

    create_mutex2.Flags.NtSecuritySharing = 1;
    create_mutex2.PrivateRuntimeDataSize = sizeof(expect_mutex_data);
    create_mutex2.pPrivateRuntimeData = expect_mutex_data;
    status = pD3DKMTCreateKeyedMutex2(&create_mutex2);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    objects[0] = create_alloc.hResource;
    objects[1] = create_alloc.hResource;
    objects[2] = create_alloc.hResource;
    status = pD3DKMTShareObjects(3, objects, &attr, STANDARD_RIGHTS_ALL, &handle);
    ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    objects[0] = create_alloc.hResource;
    objects[1] = create_mutex2.hKeyedMutex;
    status = pD3DKMTShareObjects(2, objects, &attr, STANDARD_RIGHTS_ALL, &handle);
    ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    objects[0] = create_alloc.hResource;
    objects[1] = create_sync2.hSyncObject;
    status = pD3DKMTShareObjects(2, objects, &attr, STANDARD_RIGHTS_ALL, &handle);
    ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    objects[0] = create_mutex2.hKeyedMutex;
    objects[1] = create_sync2.hSyncObject;
    status = pD3DKMTShareObjects(2, objects, &attr, STANDARD_RIGHTS_ALL, &handle);
    ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    objects[0] = create_alloc.hResource;
    objects[1] = create_mutex2.hKeyedMutex;
    objects[2] = create_sync2.hSyncObject;
    objects[3] = create_sync2.hSyncObject;
    status = pD3DKMTShareObjects(4, objects, &attr, STANDARD_RIGHTS_ALL, &handle);
    ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    objects[0] = create_alloc.hResource;
    objects[1] = create_sync2.hSyncObject;
    objects[2] = create_mutex2.hKeyedMutex;
    status = pD3DKMTShareObjects(3, objects, &attr, STANDARD_RIGHTS_ALL, &handle);
    ok(status == STATUS_INVALID_PARAMETER, "got %#lx\n", status);

    objects[0] = create_alloc.hResource;
    objects[1] = create_mutex2.hKeyedMutex;
    objects[2] = create_sync2.hSyncObject;
    handle = (HANDLE)0xdeadbeef;
    status = pD3DKMTShareObjects(3, objects, &attr, STANDARD_RIGHTS_ALL, &handle);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);

    destroy_mutex.hKeyedMutex = create_mutex2.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy_mutex);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    create_mutex2.hKeyedMutex = 0;

    destroy_sync.hSyncObject = create_sync2.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy_sync);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    create_sync2.hSyncObject = 0;

    destroy_alloc.hResource = create_alloc.hResource;
    status = pD3DKMTDestroyAllocation(&destroy_alloc);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    create_alloc.hResource = 0;


    query_resource.hDevice = create_device.hDevice;
    query_resource.hNtHandle = handle;
    query_resource.PrivateRuntimeDataSize = 0xdeadbeef;
    query_resource.TotalPrivateDriverDataSize = 0xdeadbeef;
    query_resource.ResourcePrivateDriverDataSize = 0xdeadbeef;
    query_resource.NumAllocations = 0xdeadbeef;
    status = pD3DKMTQueryResourceInfoFromNtHandle(&query_resource);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(query_resource.PrivateRuntimeDataSize == sizeof(expect_resource_data),
       "got PrivateRuntimeDataSize %#x\n", query_resource.PrivateRuntimeDataSize);
    ok(query_resource.TotalPrivateDriverDataSize == 96,
       "got TotalPrivateDriverDataSize %#x\n", query_resource.TotalPrivateDriverDataSize);
    ok(query_resource.ResourcePrivateDriverDataSize == 0,
       "got ResourcePrivateDriverDataSize %#x\n", query_resource.ResourcePrivateDriverDataSize);
    ok(query_resource.NumAllocations == 1, "got NumAllocations %#x\n", query_resource.NumAllocations);

    memset(resource_data, 0xcd, sizeof(resource_data));
    query_resource.pPrivateRuntimeData = resource_data;
    query_resource.PrivateRuntimeDataSize = sizeof(resource_data);
    status = pD3DKMTQueryResourceInfoFromNtHandle(&query_resource);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(query_resource.PrivateRuntimeDataSize == sizeof(expect_resource_data),
       "got PrivateRuntimeDataSize %#x\n", query_resource.PrivateRuntimeDataSize);
    ok(resource_data[0] == (char)0xcd, "got data %d\n", resource_data[0]);

    memset(resource_data, 0xcd, sizeof(resource_data));
    memset(driver_data, 0xcd, sizeof(driver_data));
    memset(mutex_data, 0xcd, sizeof(mutex_data));
    open_resource.hDevice = create_device.hDevice;
    open_resource.hNtHandle = handle;
    open_resource.hResource = open_resource.hKeyedMutex = open_resource.hSyncObject = 0x1eadbeed;
    open_resource.NumAllocations = 1;
    open_resource.pOpenAllocationInfo2 = &open_alloc;
    open_alloc.hAllocation = 0x1eadbeed;
    open_resource.pPrivateRuntimeData = resource_data;
    open_resource.PrivateRuntimeDataSize = query_resource.PrivateRuntimeDataSize;
    open_resource.pTotalPrivateDriverDataBuffer = driver_data;
    open_resource.TotalPrivateDriverDataBufferSize = query_resource.TotalPrivateDriverDataSize;
    open_resource.pKeyedMutexPrivateRuntimeData = mutex_data;
    open_resource.KeyedMutexPrivateRuntimeDataSize = sizeof(expect_mutex_data);
    status = pD3DKMTOpenResourceFromNtHandle(&open_resource);
    todo_wine ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    ok(open_resource.NumAllocations == 1, "got NumAllocations %#x\n", open_resource.NumAllocations);
    ok(open_resource.PrivateRuntimeDataSize == sizeof(expect_resource_data),
       "got PrivateRuntimeDataSize %#x\n", open_resource.PrivateRuntimeDataSize);
    ok(open_resource.ResourcePrivateDriverDataSize == 0,
       "got ResourcePrivateDriverDataSize %#x\n", open_resource.ResourcePrivateDriverDataSize);
    ok(open_resource.TotalPrivateDriverDataBufferSize == 96,
       "got TotalPrivateDriverDataBufferSize %#x\n", open_resource.TotalPrivateDriverDataBufferSize);
    CHECK_D3DKMT_HANDLE(open_resource.hResource, FALSE);
    CHECK_D3DKMT_HANDLE(open_resource.hKeyedMutex, FALSE);
    ok(open_resource.KeyedMutexPrivateRuntimeDataSize == sizeof(expect_mutex_data),
       "got KeyedMutexPrivateRuntimeDataSize %#x\n", open_resource.KeyedMutexPrivateRuntimeDataSize);
    CHECK_D3DKMT_HANDLE(open_resource.hSyncObject, FALSE);
    CHECK_D3DKMT_HANDLE(open_alloc.hAllocation, FALSE);
    ok(open_alloc.PrivateDriverDataSize == 96, "got PrivateDriverDataSize %#x\n", open_alloc.PrivateDriverDataSize);

    destroy_alloc.hResource = open_resource.hResource;
    status = pD3DKMTDestroyAllocation(&destroy_alloc);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    open_resource.hResource = 0;

    destroy_mutex.hKeyedMutex = open_resource.hKeyedMutex;
    status = pD3DKMTDestroyKeyedMutex(&destroy_mutex);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    open_resource.hKeyedMutex = 0;

    destroy_sync.hSyncObject = open_resource.hSyncObject;
    status = pD3DKMTDestroySynchronizationObject(&destroy_sync);
    ok(status == STATUS_SUCCESS, "got %#lx\n", status);
    open_resource.hSyncObject = 0;

    memset(&open_resource, 0, sizeof(open_resource));
    CloseHandle(handle);


    VirtualFree((void *)alloc.pSystemMem, 0, MEM_RELEASE);

    destroy_device.hDevice = create_device.hDevice;
    pD3DKMTDestroyDevice(&destroy_device);
    close_adapter.hAdapter = open_adapter.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

#undef CHECK_D3DKMT_HANDLE
}

static void test_gpu_device_properties_guid(const GUID *devinterface_guid)
{
    BYTE iface_detail_buffer[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) + 256 * sizeof(WCHAR)];
    SP_DEVINFO_DATA device_data = {sizeof(device_data)};
    SP_DEVICE_INTERFACE_DATA iface = {sizeof(iface)};
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *iface_data;
    WCHAR device_id[256];
    DEVPROPTYPE type;
    unsigned int i;
    UINT32 value;
    HDEVINFO set;
    BOOL ret;

    /* Make sure display devices are initialized. */
    SendMessageW(GetDesktopWindow(), WM_NULL, 0, 0);

    set = SetupDiGetClassDevsW(devinterface_guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    ok(set != INVALID_HANDLE_VALUE, "SetupDiGetClassDevs failed, error %lu.\n", GetLastError());

    iface_data = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *)iface_detail_buffer;
    iface_data->cbSize = sizeof(*iface_data);

    i = 0;
    while (SetupDiEnumDeviceInterfaces(set, NULL, devinterface_guid, i, &iface))
    {
        ret = SetupDiGetDeviceInterfaceDetailW(set, &iface, iface_data,
                sizeof(iface_detail_buffer), NULL, &device_data );
        ok(ret, "Got unexpected ret %d, GetLastError() %lu.\n", ret, GetLastError());

        ret = SetupDiGetDevicePropertyW(set, &device_data, &DEVPKEY_Device_MatchingDeviceId, &type,
                (BYTE *)device_id, sizeof(device_id), NULL, 0);
        ok(ret, "Got unexpected ret %d, GetLastError() %lu.\n", ret, GetLastError());
        ok(type == DEVPROP_TYPE_STRING, "Got type %ld.\n", type);

        ret = SetupDiGetDevicePropertyW(set, &device_data, &DEVPKEY_Device_BusNumber, &type,
                (BYTE *)&value, sizeof(value), NULL, 0);
        if (!wcsicmp(device_id, L"root\\basicrender") || !wcsicmp(device_id, L"root\\basicdisplay"))
        {
            ok(!ret, "Found Bus Id.\n");
        }
        else
        {
            ok(ret, "Got unexpected ret %d, GetLastError() %lu, %s.\n", ret, GetLastError(), debugstr_w(device_id));
            ok(type == DEVPROP_TYPE_UINT32, "Got type %ld.\n", type);
        }

        ret = SetupDiGetDevicePropertyW(set, &device_data, &DEVPKEY_Device_RemovalPolicy, &type,
                (BYTE *)&value, sizeof(value), NULL, 0);
        ok(ret, "Got unexpected ret %d, GetLastError() %lu, %s.\n", ret, GetLastError(), debugstr_w(device_id));
        ok(value == CM_REMOVAL_POLICY_EXPECT_NO_REMOVAL || value == CM_REMOVAL_POLICY_EXPECT_ORDERLY_REMOVAL
                || value == CM_REMOVAL_POLICY_EXPECT_SURPRISE_REMOVAL, "Got value %d.\n", value);
        ok(type == DEVPROP_TYPE_UINT32, "Got type %ld.\n", type);
        ++i;
    }
    SetupDiDestroyDeviceInfoList(set);
}

static void test_gpu_device_properties(void)
{
    winetest_push_context("GUID_DEVINTERFACE_DISPLAY_ADAPTER");
    test_gpu_device_properties_guid(&GUID_DEVINTERFACE_DISPLAY_ADAPTER);
    winetest_pop_context();
    winetest_push_context("GUID_DISPLAY_DEVICE_ARRIVAL");
    test_gpu_device_properties_guid(&GUID_DISPLAY_DEVICE_ARRIVAL);
    winetest_pop_context();
}

static void test_D3DKMTQueryAdapterInfo(void)
{
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter_desc;
    D3DKMT_QUERYADAPTERINFO query_adapter_info;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    unsigned char buffer[1024];
    NTSTATUS status;
    unsigned int i;
    BOOL ret;

    static const struct
    {
        KMTQUERYADAPTERINFOTYPE type;
        UINT size;
    }
    tests[] =
    {
        {KMTQAITYPE_CHECKDRIVERUPDATESTATUS, sizeof(BOOL)},
        {KMTQAITYPE_DRIVERVERSION, sizeof(D3DKMT_DRIVERVERSION)},
    };

    if (!pD3DKMTQueryAdapterInfo)
    {
        win_skip("D3DKMTQueryAdapterInfo() is unavailable.\n");
        return;
    }

    ret = get_primary_adapter_name(open_adapter_desc.DeviceName);
    ok(ret, "Failed to get primary adapter name.\n");
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_desc);
    if (status == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTOpenAdapterFromGdiDisplayName() is not supported.\n");
        return;
    }
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);

    for (i = 0; i < ARRAY_SIZE(tests); i++)
    {
        winetest_push_context("type %d", tests[i].type);

        /* NULL buffer */
        query_adapter_info.hAdapter = open_adapter_desc.hAdapter;
        query_adapter_info.Type = tests[i].type;
        query_adapter_info.pPrivateDriverData = NULL;
        query_adapter_info.PrivateDriverDataSize = tests[i].size;
        status = pD3DKMTQueryAdapterInfo(&query_adapter_info);
        ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

        /* Insufficient buffer size */
        query_adapter_info.pPrivateDriverData = buffer;
        query_adapter_info.PrivateDriverDataSize = tests[i].size - 1;
        status = pD3DKMTQueryAdapterInfo(&query_adapter_info);
        ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#lx.\n", status);

        /* Normal */
        query_adapter_info.pPrivateDriverData = buffer;
        query_adapter_info.PrivateDriverDataSize = tests[i].size;
        status = pD3DKMTQueryAdapterInfo(&query_adapter_info);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
        if (status != STATUS_SUCCESS)
        {
            winetest_pop_context();
            continue;
        }

        switch (tests[i].type)
        {
        case KMTQAITYPE_CHECKDRIVERUPDATESTATUS:
        {
            BOOL *value = query_adapter_info.pPrivateDriverData;
            ok(*value == FALSE, "Expected %d, got %d.\n", FALSE, *value);
            break;
        }
        case KMTQAITYPE_DRIVERVERSION:
        {
            D3DKMT_DRIVERVERSION *value = query_adapter_info.pPrivateDriverData;
            ok(*value >= KMT_DRIVERVERSION_WDDM_1_3, "Expected %d >= %d.\n", *value,
               KMT_DRIVERVERSION_WDDM_1_3);
            break;
        }
        default:
        {
            ok(0, "Type %d is not handled.\n", tests[i].type);
        }
        }

        winetest_pop_context();
    }

    close_adapter_desc.hAdapter = open_adapter_desc.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#lx.\n", status);
}

START_TEST(driver)
{
    HMODULE gdi32 = GetModuleHandleA("gdi32.dll");
    HMODULE dwmapi = LoadLibraryA("dwmapi.dll");

#define LOAD_FUNCPTR(f) p##f = (void *)GetProcAddress(gdi32, #f)
    LOAD_FUNCPTR(D3DKMTCheckOcclusion);
    LOAD_FUNCPTR(D3DKMTCheckVidPnExclusiveOwnership);
    LOAD_FUNCPTR(D3DKMTCloseAdapter);
    LOAD_FUNCPTR(D3DKMTCreateAllocation);
    LOAD_FUNCPTR(D3DKMTCreateAllocation2);
    LOAD_FUNCPTR(D3DKMTCreateDevice);
    LOAD_FUNCPTR(D3DKMTCreateKeyedMutex);
    LOAD_FUNCPTR(D3DKMTCreateKeyedMutex2);
    LOAD_FUNCPTR(D3DKMTCreateSynchronizationObject);
    LOAD_FUNCPTR(D3DKMTCreateSynchronizationObject2);
    LOAD_FUNCPTR(D3DKMTDestroyAllocation);
    LOAD_FUNCPTR(D3DKMTDestroyAllocation2);
    LOAD_FUNCPTR(D3DKMTDestroyDevice);
    LOAD_FUNCPTR(D3DKMTDestroyKeyedMutex);
    LOAD_FUNCPTR(D3DKMTDestroySynchronizationObject);
    LOAD_FUNCPTR(D3DKMTEnumAdapters2);
    LOAD_FUNCPTR(D3DKMTOpenAdapterFromDeviceName);
    LOAD_FUNCPTR(D3DKMTOpenAdapterFromGdiDisplayName);
    LOAD_FUNCPTR(D3DKMTOpenAdapterFromHdc);
    LOAD_FUNCPTR(D3DKMTOpenKeyedMutex);
    LOAD_FUNCPTR(D3DKMTOpenKeyedMutex2);
    LOAD_FUNCPTR(D3DKMTOpenKeyedMutexFromNtHandle);
    LOAD_FUNCPTR(D3DKMTOpenResource);
    LOAD_FUNCPTR(D3DKMTOpenResource2);
    LOAD_FUNCPTR(D3DKMTOpenResourceFromNtHandle);
    LOAD_FUNCPTR(D3DKMTOpenSynchronizationObject);
    LOAD_FUNCPTR(D3DKMTOpenSyncObjectFromNtHandle);
    LOAD_FUNCPTR(D3DKMTOpenSyncObjectFromNtHandle2);
    LOAD_FUNCPTR(D3DKMTOpenSyncObjectNtHandleFromName);
    LOAD_FUNCPTR(D3DKMTQueryResourceInfo);
    LOAD_FUNCPTR(D3DKMTQueryResourceInfoFromNtHandle);
    LOAD_FUNCPTR(D3DKMTQueryAdapterInfo);
    LOAD_FUNCPTR(D3DKMTQueryVideoMemoryInfo);
    LOAD_FUNCPTR(D3DKMTSetVidPnSourceOwner);
    LOAD_FUNCPTR(D3DKMTShareObjects);
#undef LOAD_FUNCPTR

    if (dwmapi)
        pDwmEnableComposition = (void *)GetProcAddress(dwmapi, "DwmEnableComposition");

    test_D3DKMTOpenAdapterFromGdiDisplayName();
    test_D3DKMTOpenAdapterFromHdc();
    test_D3DKMTEnumAdapters2();
    test_D3DKMTCloseAdapter();
    test_D3DKMTCreateDevice();
    test_D3DKMTDestroyDevice();
    test_D3DKMTCheckVidPnExclusiveOwnership();
    test_D3DKMTSetVidPnSourceOwner();
    test_D3DKMTCheckOcclusion();
    test_D3DKMTOpenAdapterFromDeviceName();
    test_D3DKMTQueryAdapterInfo();
    test_D3DKMTQueryVideoMemoryInfo();
    test_D3DKMTCreateAllocation();
    test_D3DKMTCreateKeyedMutex();
    test_D3DKMTCreateSynchronizationObject();
    test_D3DKMTShareObjects();
    test_gpu_device_properties();

    FreeLibrary(dwmapi);
}
