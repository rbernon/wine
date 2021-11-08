/*
 * Copyright 2021 Rémi Bernon for CodeWeavers
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

#include <stdarg.h>
#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "ddk/hidtypes.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unixlib.h"

#include "unix_private.h"

BOOL is_xbox_gamepad(WORD vid, WORD pid)
{
    if (vid != 0x045e) return FALSE;
    if (pid == 0x0202) return TRUE; /* Xbox Controller */
    if (pid == 0x0285) return TRUE; /* Xbox Controller S */
    if (pid == 0x0289) return TRUE; /* Xbox Controller S */
    if (pid == 0x028e) return TRUE; /* Xbox360 Controller */
    if (pid == 0x028f) return TRUE; /* Xbox360 Wireless Controller */
    if (pid == 0x02d1) return TRUE; /* Xbox One Controller */
    if (pid == 0x02dd) return TRUE; /* Xbox One Controller (Covert Forces/Firmware 2015) */
    if (pid == 0x02e0) return TRUE; /* Xbox One X Controller */
    if (pid == 0x02e3) return TRUE; /* Xbox One Elite Controller */
    if (pid == 0x02e6) return TRUE; /* Wireless XBox Controller Dongle */
    if (pid == 0x02ea) return TRUE; /* Xbox One S Controller */
    if (pid == 0x02fd) return TRUE; /* Xbox One S Controller (Firmware 2017) */
    if (pid == 0x0b00) return TRUE; /* Xbox Elite 2 */
    if (pid == 0x0b05) return TRUE; /* Xbox Elite 2 Wireless */
    if (pid == 0x0b12) return TRUE; /* Xbox Series */
    if (pid == 0x0b13) return TRUE; /* Xbox Series Wireless */
    if (pid == 0x0719) return TRUE; /* Xbox 360 Wireless Adapter */
    return FALSE;
}

BOOL is_dualshock4_gamepad(WORD vid, WORD pid)
{
    if (vid != 0x054c) return FALSE;
    if (pid == 0x05c4) return TRUE; /* DualShock 4 [CUH-ZCT1x] */
    if (pid == 0x09cc) return TRUE; /* DualShock 4 [CUH-ZCT2x] */
    if (pid == 0x0ba0) return TRUE; /* Dualshock 4 Wireless Adaptor */
    return FALSE;
}

struct mouse_device
{
    struct unix_device unix_device;
};

static void mouse_destroy(struct unix_device *iface)
{
}

static NTSTATUS mouse_start(struct unix_device *iface)
{
    if (!hid_device_begin_report_descriptor(iface, HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE))
        return STATUS_NO_MEMORY;
    if (!hid_device_add_buttons(iface, HID_USAGE_PAGE_BUTTON, 1, 3))
        return STATUS_NO_MEMORY;
    if (!hid_device_end_report_descriptor(iface))
        return STATUS_NO_MEMORY;

    return STATUS_SUCCESS;
}

static void mouse_stop(struct unix_device *iface)
{
}

static NTSTATUS mouse_haptics_start(struct unix_device *iface, DWORD duration,
                                    USHORT rumble_intensity, USHORT buzz_intensity)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS mouse_physical_device_control(struct unix_device *iface, USAGE control)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS mouse_physical_device_set_gain(struct unix_device *iface, BYTE percent)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS mouse_physical_effect_control(struct unix_device *iface, BYTE index,
                                              USAGE control, BYTE iterations)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS mouse_physical_effect_update(struct unix_device *iface, BYTE index,
                                             struct effect_params *params)
{
    return STATUS_NOT_SUPPORTED;
}

static const struct hid_device_vtbl mouse_vtbl =
{
    mouse_destroy,
    mouse_start,
    mouse_stop,
    mouse_haptics_start,
    mouse_physical_device_control,
    mouse_physical_device_set_gain,
    mouse_physical_effect_control,
    mouse_physical_effect_update,
};

static const struct device_desc mouse_device_desc =
{
    .vid = 0x845e,
    .pid = 0x0001,
    .input = -1,
    .manufacturer = {'T','h','e',' ','W','i','n','e',' ','P','r','o','j','e','c','t',0},
    .product = {'W','i','n','e',' ','H','I','D',' ','m','o','u','s','e',0},
    .serialnumber = {'0','0','0','0',0},
};

static NTSTATUS mouse_device_create(void *args)
{
    struct device_create_params *params = args;
    params->desc = mouse_device_desc;
    params->device = (ULONG_PTR)hid_device_create(&mouse_vtbl, sizeof(struct mouse_device));
    return STATUS_SUCCESS;
}

struct keyboard_device
{
    struct unix_device unix_device;
};

static void keyboard_destroy(struct unix_device *iface)
{
}

static NTSTATUS keyboard_start(struct unix_device *iface)
{
    if (!hid_device_begin_report_descriptor(iface, HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD))
        return STATUS_NO_MEMORY;
    if (!hid_device_add_buttons(iface, HID_USAGE_PAGE_KEYBOARD, 0, 101))
        return STATUS_NO_MEMORY;
    if (!hid_device_end_report_descriptor(iface))
        return STATUS_NO_MEMORY;

    return STATUS_SUCCESS;
}

static void keyboard_stop(struct unix_device *iface)
{
}

static NTSTATUS keyboard_haptics_start(struct unix_device *iface, DWORD duration,
                                       USHORT rumble_intensity, USHORT buzz_intensity)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS keyboard_physical_device_control(struct unix_device *iface, USAGE control)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS keyboard_physical_device_set_gain(struct unix_device *iface, BYTE percent)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS keyboard_physical_effect_control(struct unix_device *iface, BYTE index,
                                                 USAGE control, BYTE iterations)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS keyboard_physical_effect_update(struct unix_device *iface, BYTE index,
                                                struct effect_params *params)
{
    return STATUS_NOT_SUPPORTED;
}

static const struct hid_device_vtbl keyboard_vtbl =
{
    keyboard_destroy,
    keyboard_start,
    keyboard_stop,
    keyboard_haptics_start,
    keyboard_physical_device_control,
    keyboard_physical_device_set_gain,
    keyboard_physical_effect_control,
    keyboard_physical_effect_update,
};

static const struct device_desc keyboard_device_desc =
{
    .vid = 0x845e,
    .pid = 0x0002,
    .input = -1,
    .manufacturer = {'T','h','e',' ','W','i','n','e',' ','P','r','o','j','e','c','t',0},
    .product = {'W','i','n','e',' ','H','I','D',' ','k','e','y','b','o','a','r','d',0},
    .serialnumber = {'0','0','0','0',0},
};

static NTSTATUS keyboard_device_create(void *args)
{
    struct device_create_params *params = args;
    params->desc = keyboard_device_desc;
    params->device = (ULONG_PTR)hid_device_create(&keyboard_vtbl, sizeof(struct keyboard_device));
    return STATUS_SUCCESS;
}

void *raw_device_create(const struct raw_device_vtbl *vtbl, SIZE_T size)
{
    struct unix_device *iface;

    if (!(iface = calloc(1, size))) return NULL;
    iface->vtbl = vtbl;
    iface->ref = 1;

    return iface;
}

static void unix_device_decref(struct unix_device *iface)
{
    if (!InterlockedDecrement(&iface->ref))
    {
        iface->vtbl->destroy(iface);
        free(iface);
    }
}

static ULONG unix_device_incref(struct unix_device *iface)
{
    return InterlockedIncrement(&iface->ref);
}

static NTSTATUS unix_device_remove(void *args)
{
    struct device_params *params = args;
    struct unix_device *iface = (struct unix_device *)(ULONG_PTR)params->device;
    iface->vtbl->stop(iface);
    unix_device_decref(iface);
    return STATUS_SUCCESS;
}

static NTSTATUS unix_device_start(void *args)
{
    struct device_params *params = args;
    struct unix_device *iface = (struct unix_device *)(ULONG_PTR)params->device;
    return iface->vtbl->start(iface);
}

static NTSTATUS unix_device_get_report_descriptor(void *args)
{
    struct device_descriptor_params *params = args;
    struct unix_device *iface = (struct unix_device *)(ULONG_PTR)params->device;
    return iface->vtbl->get_report_descriptor(iface, params->buffer, params->length, params->out_length);
}

static NTSTATUS unix_device_set_output_report(void *args)
{
    struct device_report_params *params = args;
    struct unix_device *iface = (struct unix_device *)(ULONG_PTR)params->device;
    iface->vtbl->set_output_report(iface, params->packet, params->io);
    return STATUS_SUCCESS;
}

static NTSTATUS unix_device_get_feature_report(void *args)
{
    struct device_report_params *params = args;
    struct unix_device *iface = (struct unix_device *)(ULONG_PTR)params->device;
    iface->vtbl->get_feature_report(iface, params->packet, params->io);
    return STATUS_SUCCESS;
}

static NTSTATUS unix_device_set_feature_report(void *args)
{
    struct device_report_params *params = args;
    struct unix_device *iface = (struct unix_device *)(ULONG_PTR)params->device;
    iface->vtbl->set_feature_report(iface, params->packet, params->io);
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    sdl_bus_init,
    sdl_bus_wait,
    sdl_bus_stop,
    udev_bus_init,
    udev_bus_wait,
    udev_bus_stop,
    iohid_bus_init,
    iohid_bus_wait,
    iohid_bus_stop,
    mouse_device_create,
    keyboard_device_create,
    unix_device_remove,
    unix_device_start,
    unix_device_get_report_descriptor,
    unix_device_set_output_report,
    unix_device_get_feature_report,
    unix_device_set_feature_report,
};

#ifdef _WIN64

typedef ULONG PTR32;

struct sdl_bus_options32
{
    DWORD map_controllers;
    /* freed after bus_init */
    DWORD mappings_count;
    PTR32 mappings;
};

static NTSTATUS wow64_sdl_bus_init(void *args)
{
    struct sdl_bus_options32 const *params32 = args;
    struct sdl_bus_options params =
    {
        params32->map_controllers,
        params32->mappings_count,
        ULongToPtr(params32->mappings),
    };
    return sdl_bus_init(&params);
}

struct device_descriptor_params32
{
    UINT64 device;
    PTR32 buffer;
    DWORD length;
    PTR32 out_length;
};

static NTSTATUS wow64_unix_device_get_report_descriptor(void *args)
{
    struct device_descriptor_params32 const *params32 = args;
    struct device_descriptor_params params =
    {
        params32->device,
        ULongToPtr(params32->buffer),
        params32->length,
        ULongToPtr(params32->out_length),
    };
    return unix_device_get_report_descriptor(&params);
}

struct device_report_params32
{
    UINT64 device;
    PTR32 packet;
    PTR32 io;
};

static NTSTATUS wow64_unix_device_set_output_report(void *args)
{
    struct device_report_params32 const *params32 = args;
    struct device_report_params params =
    {
        params32->device,
        ULongToPtr(params32->packet),
        ULongToPtr(params32->io),
    };
    return unix_device_set_output_report(&params);
}

static NTSTATUS wow64_unix_device_get_feature_report(void *args)
{
    struct device_report_params32 const *params32 = args;
    struct device_report_params params =
    {
        params32->device,
        ULongToPtr(params32->packet),
        ULongToPtr(params32->io),
    };
    return unix_device_set_output_report(&params);
}

static NTSTATUS wow64_unix_device_set_feature_report(void *args)
{
    struct device_report_params32 const *params32 = args;
    struct device_report_params params =
    {
        params32->device,
        ULongToPtr(params32->packet),
        ULongToPtr(params32->io),
    };
    return unix_device_set_output_report(&params);
}

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    wow64_sdl_bus_init,
    sdl_bus_wait,
    sdl_bus_stop,
    udev_bus_init,
    udev_bus_wait,
    udev_bus_stop,
    iohid_bus_init,
    iohid_bus_wait,
    iohid_bus_stop,
    mouse_device_create,
    keyboard_device_create,
    unix_device_remove,
    unix_device_start,
    wow64_unix_device_get_report_descriptor,
    wow64_unix_device_set_output_report,
    wow64_unix_device_get_feature_report,
    wow64_unix_device_set_feature_report,
};

#endif  /* _WIN64 */

void bus_event_cleanup(struct bus_event *event)
{
    if (event->type == BUS_EVENT_TYPE_NONE) return;
    unix_device_decref((struct unix_device *)(ULONG_PTR)event->device);
}

struct bus_event_entry
{
    struct list entry;
    struct bus_event event;
};

void bus_event_queue_destroy(struct list *queue)
{
    struct bus_event_entry *entry, *next;

    LIST_FOR_EACH_ENTRY_SAFE(entry, next, queue, struct bus_event_entry, entry)
    {
        bus_event_cleanup(&entry->event);
        list_remove(&entry->entry);
        free(entry);
    }
}

BOOL bus_event_queue_device_removed(struct list *queue, struct unix_device *device)
{
    ULONG size = sizeof(struct bus_event_entry);
    struct bus_event_entry *entry = malloc(size);
    if (!entry) return FALSE;

    if (unix_device_incref(device) == 1) /* being destroyed */
    {
        free(entry);
        return FALSE;
    }

    entry->event.type = BUS_EVENT_TYPE_DEVICE_REMOVED;
    entry->event.device = (ULONG_PTR)device;
    list_add_tail(queue, &entry->entry);

    return TRUE;
}

BOOL bus_event_queue_device_created(struct list *queue, struct unix_device *device, struct device_desc *desc)
{
    ULONG size = sizeof(struct bus_event_entry);
    struct bus_event_entry *entry = malloc(size);
    if (!entry) return FALSE;

    if (unix_device_incref(device) == 1) /* being destroyed */
    {
        free(entry);
        return FALSE;
    }

    entry->event.type = BUS_EVENT_TYPE_DEVICE_CREATED;
    entry->event.device = (ULONG_PTR)device;
    entry->event.device_created.desc = *desc;
    list_add_tail(queue, &entry->entry);

    return TRUE;
}

BOOL bus_event_queue_input_report(struct list *queue, struct unix_device *device, BYTE *report, USHORT length)
{
    ULONG size = offsetof(struct bus_event_entry, event.input_report.buffer[length]);
    struct bus_event_entry *entry = malloc(size);
    if (!entry) return FALSE;

    if (unix_device_incref(device) == 1) /* being destroyed */
    {
        free(entry);
        return FALSE;
    }

    entry->event.type = BUS_EVENT_TYPE_INPUT_REPORT;
    entry->event.device = (ULONG_PTR)device;
    entry->event.input_report.length = length;
    memcpy(entry->event.input_report.buffer, report, length);
    list_add_tail(queue, &entry->entry);

    return TRUE;
}

BOOL bus_event_queue_pop(struct list *queue, struct bus_event *event)
{
    struct list *head = list_head(queue);
    struct bus_event_entry *entry;
    ULONG size;

    if (!head) return FALSE;

    entry = LIST_ENTRY(head, struct bus_event_entry, entry);
    list_remove(&entry->entry);

    if (entry->event.type != BUS_EVENT_TYPE_INPUT_REPORT) size = sizeof(entry->event);
    else size = offsetof(struct bus_event, input_report.buffer[entry->event.input_report.length]);

    memcpy(event, &entry->event, size);
    free(entry);

    return TRUE;
}
