/*
 * The Wine project - Xinput Joystick Library
 * Copyright 2018 Aric Stewart
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

#include "winioctl.h"

typedef struct _xinput_controller
{
    CRITICAL_SECTION crit;
    XINPUT_CAPABILITIES caps;
    void *platform_private; /* non-NULL when device is valid; validity may be read without holding crit */
    XINPUT_STATE state;
    XINPUT_VIBRATION vibration;
} xinput_controller;

CRITICAL_SECTION xinput_crit;
xinput_controller controllers[XUSER_MAX_COUNT];

void HID_find_gamepads(xinput_controller *devices) DECLSPEC_HIDDEN;
void HID_destroy_gamepads(xinput_controller *devices) DECLSPEC_HIDDEN;
void HID_update_state(xinput_controller* device, XINPUT_STATE *state) DECLSPEC_HIDDEN;
DWORD HID_set_state(xinput_controller* device, XINPUT_VIBRATION* state) DECLSPEC_HIDDEN;
void HID_enable(xinput_controller* device, BOOL enable) DECLSPEC_HIDDEN;

#define IOCTL_XUSB_GET_INFORMATION         CTL_CODE(FILE_DEVICE_XUSB, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_XUSB_GET_CAPABILITIES        CTL_CODE(FILE_DEVICE_XUSB, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_GET_LED_STATE           CTL_CODE(FILE_DEVICE_XUSB, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_GET_GAMEPAD_STATE       CTL_CODE(FILE_DEVICE_XUSB, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_SET_GAMEPAD_STATE       CTL_CODE(FILE_DEVICE_XUSB, 0x804, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_XUSB_WAIT_FOR_GUIDE_BUTTON   CTL_CODE(FILE_DEVICE_XUSB, 0x805, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_GET_BATTERY_INFORMATION CTL_CODE(FILE_DEVICE_XUSB, 0x806, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_POWER_DOWN_DEVICE       CTL_CODE(FILE_DEVICE_XUSB, 0x807, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_XUSB_GET_AUDIO_INFORMATION   CTL_CODE(FILE_DEVICE_XUSB, 0x808, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
