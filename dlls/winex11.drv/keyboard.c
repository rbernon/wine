/*
 * X11 keyboard driver
 *
 * Copyright 1993 Bob Amstadt
 * Copyright 1996 Albrecht Kleine
 * Copyright 1997 David Faure
 * Copyright 1998 Morten Welinder
 * Copyright 1998 Ulrich Weigand
 * Copyright 1999 Ove Kåven
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

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "x11drv.h"

#include "wingdi.h"
#include "winuser.h"
#include "winreg.h"
#include "winnls.h"
#include "kbd.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "wine/list.h"

#include <linux/input.h>

/* log format (add 0-padding as appropriate):
    keycode  %u  as in output from xev
    keysym   %lx as in X11/keysymdef.h
    vkey     %X  as in winuser.h
    scancode %x
*/
WINE_DEFAULT_DEBUG_CHANNEL(keyboard);
WINE_DECLARE_DEBUG_CHANNEL(key);

/* keep in sync with winewayland and server/host_x11.c */
static WORD key2scan( UINT key )
{
    /* base keys can be mapped directly */
    if (key <= KEY_KPDOT) return key;

    /* map keys found in KBDTABLES definitions (Txx Xxx Yxx macros) */
    switch (key)
    {
    case 84 /* ISO_Level3_Shift */: return 0x005a; /* T5A / VK_OEM_WSCTRL */
    case KEY_SYSRQ: return 0x0054; /* T54 / VK_SNAPSHOT */
    case KEY_102ND: return 0x0056; /* T56 / VK_OEM_102 */
    case KEY_F11: return 0x0057; /* T57 / VK_F11 */
    case KEY_F12: return 0x0058; /* T58 / VK_F12 */
    case KEY_LINEFEED: return 0x0059; /* T59 / VK_CLEAR */
    case KEY_EXIT: return 0x005b; /* T5B / VK_OEM_FINISH */
    case KEY_OPEN: return 0x005c; /* T5C / VK_OEM_JUMP */
    /* FIXME: map a KEY to T5D / VK_EREOF */
    /* FIXME: map a KEY to T5E / VK_OEM_BACKTAB */
    case KEY_COMPOSE: return 0x005f; /* T5F / VK_OEM_AUTO */
    case KEY_SCALE: return 0x0062; /* T62 / VK_ZOOM */
    case KEY_HELP: return 0x0063; /* T63 / VK_HELP */
    case KEY_F13: return 0x0064; /* T64 / VK_F13 */
    case KEY_F14: return 0x0065; /* T65 / VK_F14 */
    case KEY_F15: return 0x0066; /* T66 / VK_F15 */
    case KEY_F16: return 0x0067; /* T67 / VK_F16 */
    case KEY_F17: return 0x0068; /* T68 / VK_F17 */
    case KEY_F18: return 0x0069; /* T69 / VK_F18 */
    case KEY_F19: return 0x006a; /* T6A / VK_F19 */
    case KEY_F20: return 0x006b; /* T6B / VK_F20 */
    case KEY_F21: return 0x006c; /* T6C / VK_F21 */
    case KEY_F22: return 0x006d; /* T6D / VK_F22 */
    case KEY_F23: return 0x006e; /* T6E / VK_F23 */
    /* FIXME: map a KEY to T6F / VK_OEM_PA3 */
    case KEY_COMPUTER: return 0x0071; /* T71 / VK_OEM_RESET */
    /* FIXME: map a KEY to T73 / VK_ABNT_C1 */
    case KEY_F24: return 0x0076; /* T76 / VK_F24 */
    case KEY_KPPLUSMINUS: return 0x007b; /* T7B / VK_OEM_PA1 */
    /* FIXME: map a KEY to T7C / VK_TAB */
    /* FIXME: map a KEY to T7E / VK_ABNT_C2 */
    /* FIXME: map a KEY to T7F / VK_OEM_PA2 */
    case KEY_PREVIOUSSONG: return 0x0110; /* X10 / VK_MEDIA_PREV_TRACK */
    case KEY_NEXTSONG: return 0x0119; /* X19 / VK_MEDIA_NEXT_TRACK */
    case KEY_KPENTER: return 0x011c; /* X1C / VK_RETURN */
    case KEY_RIGHTCTRL: return 0x011d; /* X1D / VK_RCONTROL */
    case KEY_MUTE: return 0x0120; /* X20 / VK_VOLUME_MUTE */
    case KEY_PROG2: return 0x0121; /* X21 / VK_LAUNCH_APP2 */
    case KEY_PLAYPAUSE: return 0x0122; /* X22 / VK_MEDIA_PLAY_PAUSE */
    case KEY_STOPCD: return 0x0124; /* X24 / VK_MEDIA_STOP */
    case KEY_VOLUMEDOWN: return 0x012e; /* X2E / VK_VOLUME_DOWN */
    case KEY_VOLUMEUP: return 0x0130; /* X30 / VK_VOLUME_UP */
    case KEY_HOMEPAGE: return 0x0132; /* X32 / VK_BROWSER_HOME */
    case KEY_KPSLASH: return 0x0135; /* X35 / VK_DIVIDE */
    case KEY_PRINT: return 0x0137; /* X37 / VK_SNAPSHOT */
    case KEY_RIGHTALT: return 0x0138; /* X38 / VK_RMENU */
    case KEY_CANCEL: return 0x0146; /* X46 / VK_CANCEL */
    case KEY_HOME: return 0x0147; /* X47 / VK_HOME */
    case KEY_UP: return 0x0148; /* X48 / VK_UP */
    case KEY_PAGEUP: return 0x0149; /* X49 / VK_PRIOR */
    case KEY_LEFT: return 0x014b; /* X4B / VK_LEFT */
    case KEY_RIGHT: return 0x014d; /* X4D / VK_RIGHT */
    case KEY_END: return 0x014f; /* X4F / VK_END */
    case KEY_DOWN: return 0x0150; /* X50 / VK_DOWN */
    case KEY_PAGEDOWN: return 0x0151; /* X51 / VK_NEXT */
    case KEY_INSERT: return 0x0152; /* X52 / VK_INSERT */
    case KEY_DELETE: return 0x0153; /* X53 / VK_DELETE */
    case KEY_LEFTMETA: return 0x015b; /* X5B / VK_LWIN */
    case KEY_RIGHTMETA: return 0x015c; /* X5C / VK_RWIN */
    case KEY_MENU: return 0x015d; /* X5D / VK_APPS */
    case KEY_POWER: return 0x015e; /* X5E / VK_POWER */
    case KEY_SLEEP: return 0x015f; /* X5F / VK_SLEEP */
    case KEY_FIND: return 0x0165; /* X65 / VK_BROWSER_SEARCH */
    case KEY_BOOKMARKS: return 0x0166; /* X66 / VK_BROWSER_FAVORITES */
    case KEY_REFRESH: return 0x0167; /* X67 / VK_BROWSER_REFRESH */
    case KEY_STOP: return 0x0168; /* X68 / VK_BROWSER_STOP */
    case KEY_FORWARD: return 0x0169; /* X69 / VK_BROWSER_FORWARD */
    case KEY_BACK: return 0x016a; /* X6A / VK_BROWSER_BACK */
    case KEY_PROG1: return 0x016b; /* X6B / VK_LAUNCH_APP1 */
    case KEY_MAIL: return 0x016c; /* X6C / VK_LAUNCH_MAIL */
    case KEY_MEDIA: return 0x016d; /* X6D / VK_LAUNCH_MEDIA_SELECT */
    case KEY_PAUSE: return 0x021d; /* Y1D / VK_PAUSE */
    }

    /* otherwise just make up some extended scancode */
    return 0x200 | (key & 0x7f);
}

struct layout
{
    struct list entry;

    int xkb_group;
    char *xkb_layout;

    LANGID lang;
    WORD index;
    /* "Layout Id", used by NtUserGetKeyboardLayoutName / LoadKeyboardLayoutW */
    WORD layout_id;
    const USHORT *scan2vk;

    KBDTABLES tables;
    VSC_LPWSTR key_names[0x100];
    VSC_LPWSTR key_names_ext[0x200];
    WCHAR *key_names_str;

    USHORT vsc2vk[0x100];
    VSC_VK vsc2vk_e0[0x100];
    VSC_VK vsc2vk_e1[0x100];
};

static const unsigned int ControlMask = 1 << 2;

static int min_keycode, max_keycode, keysyms_per_keycode;

#define EXTRA_SCAN2VK \
    T36 | KBDEXT, T37 | KBDMULTIVK, \
    T38, T39, T3A, T3B, T3C, T3D, T3E, T3F, \
    T40, T41, T42, T43, T44, T45 | KBDEXT | KBDMULTIVK, T46 | KBDMULTIVK, T47 | KBDNUMPAD | KBDSPECIAL, \
    T48 | KBDNUMPAD | KBDSPECIAL, T49 | KBDNUMPAD | KBDSPECIAL, T4A, T4B | KBDNUMPAD | KBDSPECIAL, \
    T4C | KBDNUMPAD | KBDSPECIAL, T4D | KBDNUMPAD | KBDSPECIAL, T4E, T4F | KBDNUMPAD | KBDSPECIAL, \
    T50 | KBDNUMPAD | KBDSPECIAL, T51 | KBDNUMPAD | KBDSPECIAL, T52 | KBDNUMPAD | KBDSPECIAL, \
    T53 | KBDNUMPAD | KBDSPECIAL, T54, T55, T56, T57, \
    T58, T59, T5A, T5B, T5C, T5D, T5E, T5F, \
    T60, T61, T62, T63, T64, T65, T66, T67, \
    T68, T69, T6A, T6B, T6C, T6D, T6E, T6F, \
    T70, T71, T72, T73, T74, T75, T76, T77, \
    T78, T79, T7A, T7B, T7C, T7D, T7E, \
    [0x110] = X10 | KBDEXT, [0x119] = X19 | KBDEXT, [0x11d] = X1D | KBDEXT, [0x120] = X20 | KBDEXT, \
    [0x121] = X21 | KBDEXT, [0x122] = X22 | KBDEXT, [0x124] = X24 | KBDEXT, [0x12e] = X2E | KBDEXT, \
    [0x130] = X30 | KBDEXT, [0x132] = X32 | KBDEXT, [0x135] = X35 | KBDEXT, [0x137] = X37 | KBDEXT, \
    [0x138] = X38 | KBDEXT, [0x147] = X47 | KBDEXT, [0x148] = X48 | KBDEXT, [0x149] = X49 | KBDEXT, \
    [0x14b] = X4B | KBDEXT, [0x14d] = X4D | KBDEXT, [0x14f] = X4F | KBDEXT, [0x150] = X50 | KBDEXT, \
    [0x151] = X51 | KBDEXT, [0x152] = X52 | KBDEXT, [0x153] = X53 | KBDEXT, [0x15b] = X5B | KBDEXT, \
    [0x15c] = X5C | KBDEXT, [0x15d] = X5D | KBDEXT, [0x15f] = X5F | KBDEXT, [0x165] = X65 | KBDEXT, \
    [0x166] = X66 | KBDEXT, [0x167] = X67 | KBDEXT, [0x168] = X68 | KBDEXT, [0x169] = X69 | KBDEXT, \
    [0x16a] = X6A | KBDEXT, [0x16b] = X6B | KBDEXT, [0x16c] = X6C | KBDEXT, [0x16d] = X6D | KBDEXT, \
    [0x11c] = X1C | KBDEXT, [0x146] = X46 | KBDEXT, [0x21d] = Y1D,

static const USHORT scan2vk_qwerty[0x280] =
{
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09, T0A, T0B, T0C, T0D, T0E,
    T0F, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T1A, T1B, T1C,
    T1D, T1E, T1F, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29,
    T2A, T2B, T2C, T2D, T2E, T2F, T30, T31, T32, T33, T34, T35,
    EXTRA_SCAN2VK
};

static const USHORT scan2vk_qwerty_sw[0x280] =
{
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09, T0A, T0B, VK_OEM_PLUS, VK_OEM_4, T0E,
    T0F, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, VK_OEM_6, VK_OEM_1, T1C,
    T1D, T1E, T1F, T20, T21, T22, T23, T24, T25, T26, VK_OEM_3, T28, VK_OEM_5,
    T2A, VK_OEM_2, T2C, T2D, T2E, T2F, T30, T31, T32, T33, T34, VK_OEM_MINUS,
    EXTRA_SCAN2VK
};

static const USHORT scan2vk_qwerty_jp106[0x280] =
{
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09, T0A, T0B, T0C, VK_OEM_7, T0E,
    T0F, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, VK_OEM_3, VK_OEM_4, T1C,
    T1D, T1E, T1F, T20, T21, T22, T23, T24, T25, T26, VK_OEM_PLUS, VK_OEM_1, VK_OEM_AUTO,
    T2A, VK_OEM_6, T2C, T2D, T2E, T2F, T30, T31, T32, T33, T34, T35,
    EXTRA_SCAN2VK
};

static const USHORT scan2vk_azerty[0x280] =
{
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09, T0A, T0B, VK_OEM_4, T0D, T0E,
    T0F, 'A', 'Z', T12, T13, T14, T15, T16, T17, T18, T19, VK_OEM_6, VK_OEM_1, T1C,
    T1D, 'Q', T1F, T20, T21, T22, T23, T24, T25, T26, 'M', VK_OEM_3, VK_OEM_7,
    T2A, T2B, 'W', T2D, T2E, T2F, T30, T31, VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_2, VK_OEM_8,
    EXTRA_SCAN2VK
};

static const USHORT scan2vk_qwertz[0x280] =
{
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09, T0A, T0B, VK_OEM_4, VK_OEM_6, T0E,
    T0F, T10, T11, T12, T13, T14, 'Z', T16, T17, T18, T19, VK_OEM_1, VK_OEM_3, T1C,
    T1D, T1E, T1F, T20, T21, T22, T23, T24, T25, T26, VK_OEM_7, VK_OEM_5, VK_OEM_2,
    T2A, VK_OEM_8, 'Y', T2D, T2E, T2F, T30, T31, T32, T33, T34, VK_OEM_MINUS,
    EXTRA_SCAN2VK
};

static const USHORT scan2vk_dvorak[0x280] =
{
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09, T0A, T0B, VK_OEM_4, VK_OEM_6, T0E,
    T0F, VK_OEM_7, VK_OEM_COMMA, VK_OEM_PERIOD, 'P', 'Y', 'F', 'G', 'C', 'R', 'L', VK_OEM_2, VK_OEM_PLUS, T1C,
    T1D, T1E, 'O', 'E', 'U', 'I', 'D', 'H', 'T', 'N', 'S', VK_OEM_MINUS, T29,
    T2A, T2B, VK_OEM_1, 'Q', 'J', 'K', 'X', 'B', 'M', 'W', 'V', 'Z',
    EXTRA_SCAN2VK
};

static WORD keyc2scan( UINT keycode )
{
    UINT key = keycode - 8;

    /* base keys can be mapped directly */
    if (key <= KEY_KPDOT) return key;

    /* map keys found in KBDTABLES definitions (Txx Xxx Yxx macros) */
    switch (key)
    {
    case KEY_SYSRQ: return 0x0054; /* T54 / VK_SNAPSHOT */
    case KEY_102ND: return 0x0056; /* T56 / VK_OEM_102 */
    case KEY_F11: return 0x0057; /* T57 / VK_F11 */
    case KEY_F12: return 0x0058; /* T58 / VK_F12 */
    case KEY_LINEFEED: return 0x0059; /* T59 / VK_CLEAR */
    case 0x84 /* ISO_Level3_Shift */: return 0x005a; /* T5A / VK_OEM_WSCTRL */
    case KEY_EXIT: return 0x005b; /* T5B / VK_OEM_FINISH */
    case KEY_OPEN: return 0x005c; /* T5C / VK_OEM_JUMP */
    /* case KEY_EREOF: return 0x005d; */ /* T5D / VK_EREOF */
    /* case KEY_OEM_BACKTAB: return 0x005e; */ /* T5E / VK_OEM_BACKTAB */
    case KEY_COMPOSE: return 0x005f; /* T5F / VK_OEM_AUTO */
    case KEY_SCALE: return 0x0062; /* T62 / VK_ZOOM */
    case KEY_HELP: return 0x0063; /* T63 / VK_HELP */
    case KEY_F13: return 0x0064; /* T64 / VK_F13 */
    case KEY_F14: return 0x0065; /* T65 / VK_F14 */
    case KEY_F15: return 0x0066; /* T66 / VK_F15 */
    case KEY_F16: return 0x0067; /* T67 / VK_F16 */
    case KEY_F17: return 0x0068; /* T68 / VK_F17 */
    case KEY_F18: return 0x0069; /* T69 / VK_F18 */
    case KEY_F19: return 0x006a; /* T6A / VK_F19 */
    case KEY_F20: return 0x006b; /* T6B / VK_F20 */
    case KEY_F21: return 0x006c; /* T6C / VK_F21 */
    case KEY_F22: return 0x006d; /* T6D / VK_F22 */
    case KEY_F23: return 0x006e; /* T6E / VK_F23 */
    /* case KEY_OEM_PA3: return 0x006f; */ /* T6F / VK_OEM_PA3 */
    case KEY_COMPUTER: return 0x0071; /* T71 / VK_OEM_RESET */
    /* case KEY_ABNT_C1: return 0x0073; */ /* T73 / VK_ABNT_C1 */
    case KEY_F24: return 0x0076; /* T76 / VK_F24 */
    case KEY_KPPLUSMINUS: return 0x007b; /* T7B / VK_OEM_PA1 */
    case KEY_TAB: return 0x007c; /* T7C / VK_TAB */
    /* case KEY_ABNT_C2: return 0x007e; */ /* T7E / VK_ABNT_C2 */
    /* case KEY_OEM_PA2: return 0x007f; */ /* T7F / VK_OEM_PA2 */
    case KEY_PREVIOUSSONG: return 0x0110; /* X10 / VK_MEDIA_PREV_TRACK */
    case KEY_NEXTSONG: return 0x0119; /* X19 / VK_MEDIA_NEXT_TRACK */
    case KEY_KPENTER: return 0x011c; /* X1C / VK_RETURN */
    case KEY_RIGHTCTRL: return 0x011d; /* X1D / VK_RCONTROL */
    case KEY_MUTE: return 0x0120; /* X20 / VK_VOLUME_MUTE */
    case KEY_PROG2: return 0x0121; /* X21 / VK_LAUNCH_APP2 */
    case KEY_PLAYPAUSE: return 0x0122; /* X22 / VK_MEDIA_PLAY_PAUSE */
    case KEY_STOPCD: return 0x0124; /* X24 / VK_MEDIA_STOP */
    case KEY_VOLUMEDOWN: return 0x012e; /* X2E / VK_VOLUME_DOWN */
    case KEY_VOLUMEUP: return 0x0130; /* X30 / VK_VOLUME_UP */
    case KEY_HOMEPAGE: return 0x0132; /* X32 / VK_BROWSER_HOME */
    case KEY_KPSLASH: return 0x0135; /* X35 / VK_DIVIDE */
    case KEY_PRINT: return 0x0137; /* X37 / VK_SNAPSHOT */
    case KEY_RIGHTALT: return 0x0138; /* X38 / VK_RMENU */
    case KEY_CANCEL: return 0x0146; /* X46 / VK_CANCEL */
    case KEY_HOME: return 0x0147; /* X47 / VK_HOME */
    case KEY_UP: return 0x0148; /* X48 / VK_UP */
    case KEY_PAGEUP: return 0x0149; /* X49 / VK_PRIOR */
    case KEY_LEFT: return 0x014b; /* X4B / VK_LEFT */
    case KEY_RIGHT: return 0x014d; /* X4D / VK_RIGHT */
    case KEY_END: return 0x014f; /* X4F / VK_END */
    case KEY_DOWN: return 0x0150; /* X50 / VK_DOWN */
    case KEY_PAGEDOWN: return 0x0151; /* X51 / VK_NEXT */
    case KEY_INSERT: return 0x0152; /* X52 / VK_INSERT */
    case KEY_DELETE: return 0x0153; /* X53 / VK_DELETE */
    case KEY_LEFTMETA: return 0x015b; /* X5B / VK_LWIN */
    case KEY_RIGHTMETA: return 0x015c; /* X5C / VK_RWIN */
    case KEY_MENU: return 0x015d; /* X5D / VK_APPS */
    case KEY_POWER: return 0x015e; /* X5E / VK_POWER */
    case KEY_SLEEP: return 0x015f; /* X5F / VK_SLEEP */
    case KEY_FIND: return 0x0165; /* X65 / VK_BROWSER_SEARCH */
    case KEY_BOOKMARKS: return 0x0166; /* X66 / VK_BROWSER_FAVORITES */
    case KEY_REFRESH: return 0x0167; /* X67 / VK_BROWSER_REFRESH */
    case KEY_STOP: return 0x0168; /* X68 / VK_BROWSER_STOP */
    case KEY_FORWARD: return 0x0169; /* X69 / VK_BROWSER_FORWARD */
    case KEY_BACK: return 0x016a; /* X6A / VK_BROWSER_BACK */
    case KEY_PROG1: return 0x016b; /* X6B / VK_LAUNCH_APP1 */
    case KEY_MAIL: return 0x016c; /* X6C / VK_LAUNCH_MAIL */
    case KEY_MEDIA: return 0x016d; /* X6D / VK_LAUNCH_MEDIA_SELECT */
    case KEY_PAUSE: return 0x021d; /* Y1D / VK_PAUSE */
    }

    /* FIXME: we could map some more keys to available scancodes
     * here, and add matching vkeys into EXTRA_SCAN2VK above.
     */

    /* otherwise just make up some extended scancode */
    return 0x200 | (key & 0x7f);
}

static int NumLockMask, ScrollLockMask, AltGrMask; /* mask in the XKeyEvent state */

static pthread_mutex_t kbd_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct list xkb_layouts = LIST_INIT( xkb_layouts );
static const USHORT *scan2vk = scan2vk_qwerty;

static char KEYBOARD_MapDeadKeysym(KeySym keysym);

static inline LANGID langid_from_xkb_layout( const char *layout, size_t layout_len )
{
#define MAKEINDEX(c0, c1) (MAKEWORD(c0, c1) - MAKEWORD('a', 'a'))
    static const LANGID langids[] =
    {
        [MAKEINDEX('a','f')] = MAKELANGID(LANG_DARI, SUBLANG_DEFAULT),
        [MAKEINDEX('a','l')] = MAKELANGID(LANG_ALBANIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('a','m')] = MAKELANGID(LANG_ARMENIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('a','t')] = MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN_AUSTRIAN),
        [MAKEINDEX('a','z')] = MAKELANGID(LANG_AZERBAIJANI, SUBLANG_DEFAULT),
        [MAKEINDEX('b','a')] = MAKELANGID(LANG_BOSNIAN, SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC),
        [MAKEINDEX('b','d')] = MAKELANGID(LANG_BANGLA, SUBLANG_DEFAULT),
        [MAKEINDEX('b','e')] = MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH_BELGIAN),
        [MAKEINDEX('b','g')] = MAKELANGID(LANG_BULGARIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('b','r')] = MAKELANGID(LANG_PORTUGUESE, 2),
        [MAKEINDEX('b','t')] = MAKELANGID(LANG_TIBETAN, 3),
        [MAKEINDEX('b','w')] = MAKELANGID(LANG_TSWANA, SUBLANG_TSWANA_BOTSWANA),
        [MAKEINDEX('b','y')] = MAKELANGID(LANG_BELARUSIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('c','a')] = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_CAN),
        [MAKEINDEX('c','d')] = MAKELANGID(LANG_FRENCH, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('c','h')] = MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN_SWISS),
        [MAKEINDEX('c','m')] = MAKELANGID(LANG_FRENCH, 11),
        [MAKEINDEX('c','n')] = MAKELANGID(LANG_CHINESE, SUBLANG_DEFAULT),
        [MAKEINDEX('c','z')] = MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT),
        [MAKEINDEX('d','e')] = MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT),
        [MAKEINDEX('d','k')] = MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT),
        [MAKEINDEX('d','z')] = MAKELANGID(LANG_TAMAZIGHT, SUBLANG_TAMAZIGHT_ALGERIA_LATIN),
        [MAKEINDEX('e','e')] = MAKELANGID(LANG_ESTONIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('e','s')] = MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT),
        [MAKEINDEX('e','t')] = MAKELANGID(LANG_AMHARIC, SUBLANG_DEFAULT),
        [MAKEINDEX('f','i')] = MAKELANGID(LANG_FINNISH, SUBLANG_DEFAULT),
        [MAKEINDEX('f','o')] = MAKELANGID(LANG_FAEROESE, SUBLANG_DEFAULT),
        [MAKEINDEX('f','r')] = MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT),
        [MAKEINDEX('g','b')] = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_UK),
        [MAKEINDEX('g','e')] = MAKELANGID(LANG_GEORGIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('g','h')] = MAKELANGID(LANG_ENGLISH, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('g','n')] = MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT),
        [MAKEINDEX('g','r')] = MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT),
        [MAKEINDEX('h','r')] = MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('h','u')] = MAKELANGID(LANG_HUNGARIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('i','d')] = MAKELANGID(LANG_INDONESIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('i','e')] = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_EIRE),
        [MAKEINDEX('i','l')] = MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT),
        [MAKEINDEX('i','n')] = MAKELANGID(LANG_HINDI, SUBLANG_DEFAULT),
        [MAKEINDEX('i','q')] = MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_IRAQ),
        [MAKEINDEX('i','r')] = MAKELANGID(LANG_PERSIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('i','s')] = MAKELANGID(LANG_ICELANDIC, SUBLANG_DEFAULT),
        [MAKEINDEX('i','t')] = MAKELANGID(LANG_ITALIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('j','p')] = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),
        [MAKEINDEX('j','v')] = MAKELANGID(LANG_INDONESIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('k','e')] = MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT),
        [MAKEINDEX('k','g')] = MAKELANGID(LANG_KYRGYZ, SUBLANG_DEFAULT),
        [MAKEINDEX('k','h')] = MAKELANGID(LANG_KHMER, SUBLANG_DEFAULT),
        [MAKEINDEX('k','r')] = MAKELANGID(LANG_KOREAN, SUBLANG_DEFAULT),
        [MAKEINDEX('k','z')] = MAKELANGID(LANG_KAZAK, SUBLANG_DEFAULT),
        [MAKEINDEX('l','a')] = MAKELANGID(LANG_LAO, SUBLANG_DEFAULT),
        [MAKEINDEX('l','k')] = MAKELANGID(LANG_SINHALESE, SUBLANG_DEFAULT),
        [MAKEINDEX('l','t')] = MAKELANGID(LANG_LITHUANIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('l','v')] = MAKELANGID(LANG_LATVIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('m','a')] = MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_MOROCCO),
        [MAKEINDEX('m','d')] = MAKELANGID(LANG_ROMANIAN, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('m','e')] = MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_MONTENEGRO_LATIN),
        [MAKEINDEX('m','k')] = MAKELANGID(LANG_MACEDONIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('m','l')] = MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT),
        [MAKEINDEX('m','m')] = MAKELANGID(0x55 /*LANG_BURMESE*/, SUBLANG_DEFAULT),
        [MAKEINDEX('m','n')] = MAKELANGID(LANG_MONGOLIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('m','t')] = MAKELANGID(LANG_MALTESE, SUBLANG_DEFAULT),
        [MAKEINDEX('m','v')] = MAKELANGID(LANG_DIVEHI, SUBLANG_DEFAULT),
        [MAKEINDEX('m','y')] = MAKELANGID(LANG_MALAY, SUBLANG_DEFAULT),
        [MAKEINDEX('n','g')] = MAKELANGID(LANG_ENGLISH, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('n','l')] = MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT),
        [MAKEINDEX('n','p')] = MAKELANGID(LANG_NEPALI, SUBLANG_DEFAULT),
        [MAKEINDEX('p','h')] = MAKELANGID(LANG_FILIPINO, SUBLANG_DEFAULT),
        [MAKEINDEX('p','k')] = MAKELANGID(LANG_URDU, SUBLANG_DEFAULT),
        [MAKEINDEX('p','l')] = MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT),
        [MAKEINDEX('p','t')] = MAKELANGID(LANG_PORTUGUESE, SUBLANG_DEFAULT),
        [MAKEINDEX('r','o')] = MAKELANGID(LANG_ROMANIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('r','s')] = MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_LATIN),
        [MAKEINDEX('r','u')] = MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('s','e')] = MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT),
        [MAKEINDEX('s','i')] = MAKELANGID(LANG_SLOVENIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('s','k')] = MAKELANGID(LANG_SLOVAK, SUBLANG_DEFAULT),
        [MAKEINDEX('s','n')] = MAKELANGID(LANG_WOLOF, SUBLANG_DEFAULT),
        [MAKEINDEX('s','y')] = MAKELANGID(LANG_SYRIAC, SUBLANG_DEFAULT),
        [MAKEINDEX('t','g')] = MAKELANGID(LANG_FRENCH, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('t','h')] = MAKELANGID(LANG_THAI, SUBLANG_DEFAULT),
        [MAKEINDEX('t','j')] = MAKELANGID(LANG_TAJIK, SUBLANG_DEFAULT),
        [MAKEINDEX('t','m')] = MAKELANGID(LANG_TURKMEN, SUBLANG_DEFAULT),
        [MAKEINDEX('t','r')] = MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT),
        [MAKEINDEX('t','w')] = MAKELANGID(LANG_CHINESE, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('t','z')] = MAKELANGID(LANG_SWAHILI, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('u','a')] = MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('u','s')] = MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
        [MAKEINDEX('u','z')] = MAKELANGID(LANG_UZBEK, 2),
        [MAKEINDEX('v','n')] = MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT),
        [MAKEINDEX('z','a')] = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_SOUTH_AFRICA),
    };
    LANGID langid;

    if (layout_len == 2 && (langid = langids[MAKEINDEX(layout[0], layout[1])])) return langid;
    if (layout_len == 3 && !memcmp( layout, "ara", layout_len )) return MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT);
    if (layout_len == 3 && !memcmp( layout, "epo", layout_len )) return MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT);
    if (layout_len == 3 && !memcmp( layout, "mao", layout_len )) return MAKELANGID(LANG_MAORI, SUBLANG_DEFAULT);
    if (layout_len == 4 && !memcmp( layout, "brai", layout_len )) return MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT);
    if (layout_len == 5 && !memcmp( layout, "latam", layout_len )) return MAKELANGID(LANG_SPANISH, SUBLANG_CUSTOM_UNSPECIFIED);

    FIXME( "Unknown layout language %s\n", debugstr_a(layout) );
    return MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_UNSPECIFIED);
#undef MAKEINDEX
};

static void create_layout_from_xkb( Display *display, int xkb_group, const char *xkb_layout, LANGID lang )
{
    static WORD next_layout_id = 1;

    VSC_LPWSTR *names_entry, *names_ext_entry;
    VSC_VK *vsc2vk_e0_entry, *vsc2vk_e1_entry;
    unsigned int keyc, len, names_len;
    struct layout *layout;
    WCHAR *names_str;
    WORD index = 0;
    char *ptr;

    TRACE( "lang %04x, display %p, xkb_group %u, xkb_layout %s\n", lang, display, xkb_group, xkb_layout );

    LIST_FOR_EACH_ENTRY( layout, &xkb_layouts, struct layout, entry )
    {
        if (!strcmp( layout->xkb_layout, xkb_layout ))
        {
            TRACE( "Found existing layout entry %p, hkl %04x%04x id %04x\n",
                   layout, layout->index, layout->lang, layout->layout_id );
            if (layout->xkb_group == -1) layout->xkb_group = xkb_group;
            return;
        }
        if (layout->lang == lang) index++;
    }

    for (names_len = 0, keyc = min_keycode; keyc <= max_keycode; keyc++)
    {
        unsigned int dummy;
        const char *name;
        KeySym keysym;

        XkbLookupKeySym( display, keyc, xkb_group * 0x2000, &dummy, &keysym );
        if ((name = XKeysymToString( keysym ))) names_len += strlen( name ) + 1;
    }

    names_len *= sizeof(WCHAR);
    len = strlen( xkb_layout ) + 1;
    if (!(layout = calloc( 1, sizeof(*layout) + names_len + len )))
    {
        WARN( "Failed to allocate memory for Xkb layout entry\n" );
        return;
    }
    list_add_tail( &xkb_layouts, &layout->entry );
    ptr = (char *)(layout + 1);

    layout->xkb_group = xkb_group;
    layout->xkb_layout = strcpy( ptr, xkb_layout );
    ptr += len;

    layout->lang = lang;
    layout->index = index;
    if (index) layout->layout_id = next_layout_id++;
    layout->key_names_str = names_str = (void *)ptr;

    switch (lang)
    {
    case MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT): layout->scan2vk = scan2vk_azerty;
    case MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT): layout->scan2vk = scan2vk_qwertz;
    case MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN_SWISS): layout->scan2vk = scan2vk_qwertz;
    case MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT): layout->scan2vk = scan2vk_qwerty_sw;
    case MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT): layout->scan2vk = scan2vk_qwerty_jp106;
    default: layout->scan2vk = scan2vk_qwerty;
    }
    if (strstr( xkb_layout, "dvorak" )) layout->scan2vk = scan2vk_dvorak;

    layout->tables.pKeyNames = layout->key_names;
    layout->tables.pKeyNamesExt = layout->key_names_ext;
    layout->tables.bMaxVSCtoVK = 0xff;
    layout->tables.pusVSCtoVK = layout->vsc2vk;
    layout->tables.pVSCtoVK_E0 = layout->vsc2vk_e0;
    layout->tables.pVSCtoVK_E1 = layout->vsc2vk_e1;

    names_entry = layout->tables.pKeyNames;
    names_ext_entry = layout->tables.pKeyNamesExt;
    vsc2vk_e0_entry = layout->tables.pVSCtoVK_E0;
    vsc2vk_e1_entry = layout->tables.pVSCtoVK_E1;

    for (keyc = min_keycode; keyc <= max_keycode; keyc++)
    {
        WORD scan = keyc2scan( keyc );
        unsigned int dummy;
        VSC_LPWSTR *entry;
        const char *name;
        KeySym keysym;

        XkbLookupKeySym( display, keyc, xkb_group * 0x2000, &dummy, &keysym );
        name = XKeysymToString( keysym );

        if (!(scan & 0xff) || !name || !(len = strlen( name ))) continue;
        if (!(scan & 0x300)) entry = names_entry++;
        else entry = names_ext_entry++;

        entry->vsc = (BYTE)scan;
        entry->pwsz = names_str;
        names_str += ntdll_umbstowcs( name, len + 1, entry->pwsz, len + 1 );

        TRACE( "keyc %#04x, scan %#04x -> name %s\n", keyc, entry->vsc, debugstr_w(entry->pwsz) );
    }

    for (keyc = min_keycode; keyc <= max_keycode; keyc++)
    {
        WORD scan = keyc2scan( keyc ), vkey = layout->scan2vk[scan];
        VSC_VK *entry = NULL;

        if (!(scan & 0xff) || !vkey) continue;
        if (scan & 0x100) entry = vsc2vk_e0_entry++;
        else if (scan & 0x200) entry = vsc2vk_e1_entry++;
        else layout->tables.pusVSCtoVK[scan & 0xff] = vkey;

        if (entry)
        {
            entry->Vsc = scan & 0xff;
            entry->Vk = vkey;
        }

        TRACE( "keyc %#04x, scan %#05x -> vkey %#06x\n", keyc, scan, vkey );
    }

    TRACE( "Created layout entry %p, hkl %04x%04x id %04x\n", layout, layout->index, layout->lang, layout->layout_id );
}

static void set_current_xkb_group( HWND hwnd, int xkb_group )
{
    struct x11drv_thread_data *thread_data = x11drv_init_thread_data();
    struct layout *layout;
    LCID locale;
    HKL hkl;

    LIST_FOR_EACH_ENTRY( layout, &xkb_layouts, struct layout, entry )
        if (layout->xkb_group == xkb_group) break;
    if (&layout->entry == &xkb_layouts)
    {
        WARN( "Failed to find Xkb Layout for group %d\n", xkb_group );
        return;
    }

    scan2vk = layout->scan2vk;

    locale = LOWORD(NtUserGetKeyboardLayout( 0 ));
    if (!layout->layout_id) hkl = (HKL)(UINT_PTR)MAKELONG(locale, layout->lang);
    else hkl = (HKL)(UINT_PTR)MAKELONG(locale, 0xf000 | layout->layout_id);

    if (hkl == thread_data->last_layout) return;
    thread_data->last_layout = hkl;

    TRACE( "Changing keyboard layout to %p\n", hkl );
    send_message( hwnd, WM_INPUTLANGCHANGEREQUEST, 0 /*FIXME*/, (LPARAM)hkl );
}


/***********************************************************************
 *           X11DRV_send_keyboard_input
 */
static void X11DRV_send_keyboard_input( HWND hwnd, WORD vkey, WORD scan, UINT flags, UINT time )
{
    INPUT input;

    TRACE_(key)( "hwnd %p vkey=%04x scan=%04x flags=%04x\n", hwnd, vkey, scan, flags );

    input.type           = INPUT_KEYBOARD;
    input.ki.wVk         = vkey;
    input.ki.wScan       = scan;
    input.ki.dwFlags     = flags;
    input.ki.time        = time;
    input.ki.dwExtraInfo = 0;

    NtUserSendHardwareInput( hwnd, 0, &input, 0 );
}


/***********************************************************************
 *           set_async_key_state
 */
static void set_async_key_state( const BYTE state[256] )
{
    SERVER_START_REQ( set_key_state )
    {
        req->async = 1;
        wine_server_add_data( req, state, 256 );
        wine_server_call( req );
    }
    SERVER_END_REQ;
}

static void update_key_state( BYTE *keystate, BYTE key, int down )
{
    if (down)
    {
        if (!(keystate[key] & 0x80)) keystate[key] ^= 0x01;
        keystate[key] |= 0x80;
    }
    else keystate[key] &= ~0x80;
}

/***********************************************************************
 *           X11DRV_KeymapNotify
 *
 * Update modifiers state (Ctrl, Alt, Shift) when window is activated.
 *
 * This handles the case where one uses Ctrl+... Alt+... or Shift+.. to switch
 * from wine to another application and back.
 * Toggle keys are handled in HandleEvent.
 */
BOOL X11DRV_KeymapNotify( HWND hwnd, XEvent *event )
{
    int i, j;
    BYTE keystate[256];
    WORD vkey;
    DWORD flags;
    HWND keymapnotify_hwnd;
    BOOL changed = FALSE;
    struct {
        WORD vkey;
        WORD scan;
        WORD pressed;
    } keys[256];
    struct x11drv_thread_data *thread_data = x11drv_thread_data();

    keymapnotify_hwnd = thread_data->keymapnotify_hwnd;
    thread_data->keymapnotify_hwnd = NULL;

    if (!NtUserGetAsyncKeyboardState( keystate )) return FALSE;

    memset(keys, 0, sizeof(keys));

    pthread_mutex_lock( &kbd_mutex );

    /* the minimum keycode is always greater or equal to 8, so we can
     * skip the first 8 values, hence start at 1
     */
    for (i = 1; i < 32; i++)
    {
        for (j = 0; j < 8; j++)
        {
            WORD scan = keyc2scan( (i * 8) + j );
            vkey = scan2vk[scan];

            /* If multiple keys map to the same vkey, we want to report it as
             * pressed iff any of them are pressed. */
            if (!keys[vkey & 0xff].vkey)
            {
                keys[vkey & 0xff].vkey = vkey;
                keys[vkey & 0xff].scan = scan & 0xff;
            }

            if (event->xkeymap.key_vector[i] & (1<<j)) keys[vkey & 0xff].pressed = TRUE;
        }
    }

    for (vkey = 1; vkey <= 0xff; vkey++)
    {
        if (keys[vkey].vkey && !(keystate[vkey] & 0x80) != !keys[vkey].pressed)
        {
            TRACE( "Adjusting state for vkey %#.2x. State before %#.2x\n",
                   keys[vkey].vkey, keystate[vkey]);

            /* This KeymapNotify follows a FocusIn(mode=NotifyUngrab) event,
             * which is caused by a keyboard grab being released.
             * See XGrabKeyboard().
             *
             * We might have missed some key press/release events while the
             * keyboard was grabbed, but keyboard grab doesn't generate focus
             * lost / focus gained events on the Windows side, so the affected
             * program is not aware that it needs to resync the keyboard state.
             *
             * This, for example, may cause Alt being stuck after using Alt+Tab.
             *
             * To work around that problem we sync any possible key releases.
             *
             * Syncing key presses is not feasible as window managers differ in
             * event sequences, e.g. KDE performs two keyboard grabs for
             * Alt+Tab, which would sync the Tab press.
             */
            if (keymapnotify_hwnd && !keys[vkey].pressed)
            {
                TRACE( "Sending KEYUP for a modifier %#.2x\n", vkey);
                flags = KEYEVENTF_KEYUP;
                if (keys[vkey].vkey & 0x1000) flags |= KEYEVENTF_EXTENDEDKEY;
                X11DRV_send_keyboard_input( keymapnotify_hwnd, vkey, keys[vkey].scan, flags, NtGetTickCount() );
            }

            update_key_state( keystate, vkey, keys[vkey].pressed );
            changed = TRUE;
        }
    }

    pthread_mutex_unlock( &kbd_mutex );
    if (!changed) return FALSE;

    update_key_state( keystate, VK_CONTROL, (keystate[VK_LCONTROL] | keystate[VK_RCONTROL]) & 0x80 );
    update_key_state( keystate, VK_MENU, (keystate[VK_LMENU] | keystate[VK_RMENU]) & 0x80 );
    update_key_state( keystate, VK_SHIFT, (keystate[VK_LSHIFT] | keystate[VK_RSHIFT]) & 0x80 );
    set_async_key_state( keystate );
    return TRUE;
}

static void adjust_lock_state( BYTE *keystate, HWND hwnd, WORD vkey, WORD scan, DWORD flags, DWORD time )
{
    BYTE prev_state = keystate[vkey] & 0x01;

    X11DRV_send_keyboard_input( hwnd, vkey, scan, flags, time );
    X11DRV_send_keyboard_input( hwnd, vkey, scan, flags ^ KEYEVENTF_KEYUP, time );

    /* Keyboard hooks may have blocked processing lock keys causing our state
     * to be different than state on X server side. Although Windows allows hooks
     * to block changing state, we can't prevent it on X server side. Having
     * different states would cause us to try to adjust it again on the next
     * key event. We prevent that by overriding hooks and setting key states here. */
    if (NtUserGetAsyncKeyboardState( keystate ) && (keystate[vkey] & 0x01) == prev_state)
    {
        WARN("keystate %x not changed (%#.2x), probably blocked by hooks\n", vkey, keystate[vkey]);
        keystate[vkey] ^= 0x01;
        set_async_key_state( keystate );
    }
}

static void update_lock_state( HWND hwnd, WORD vkey, UINT state, UINT time )
{
    BYTE keystate[256];

    /* Note: X sets the below states on key down and clears them on key up.
       Windows triggers them on key down. */

    if (!NtUserGetAsyncKeyboardState( keystate )) return;

    /* Adjust the CAPSLOCK state if it has been changed outside wine */
    if (!(keystate[VK_CAPITAL] & 0x01) != !(state & LockMask) && vkey != VK_CAPITAL)
    {
        DWORD flags = 0;
        if (keystate[VK_CAPITAL] & 0x80) flags ^= KEYEVENTF_KEYUP;
        TRACE("Adjusting CapsLock state (%#.2x)\n", keystate[VK_CAPITAL]);
        adjust_lock_state( keystate, hwnd, VK_CAPITAL, 0x3a, flags, time );
    }

    /* Adjust the NUMLOCK state if it has been changed outside wine */
    if (!(keystate[VK_NUMLOCK] & 0x01) != !(state & NumLockMask) && (vkey & 0xff) != VK_NUMLOCK)
    {
        DWORD flags = KEYEVENTF_EXTENDEDKEY;
        if (keystate[VK_NUMLOCK] & 0x80) flags ^= KEYEVENTF_KEYUP;
        TRACE("Adjusting NumLock state (%#.2x)\n", keystate[VK_NUMLOCK]);
        adjust_lock_state( keystate, hwnd, VK_NUMLOCK, 0x45, flags, time );
    }

    /* Adjust the SCROLLLOCK state if it has been changed outside wine */
    if (!(keystate[VK_SCROLL] & 0x01) != !(state & ScrollLockMask) && vkey != VK_SCROLL)
    {
        DWORD flags = 0;
        if (keystate[VK_SCROLL] & 0x80) flags ^= KEYEVENTF_KEYUP;
        TRACE("Adjusting ScrLock state (%#.2x)\n", keystate[VK_SCROLL]);
        adjust_lock_state( keystate, hwnd, VK_SCROLL, 0x46, flags, time );
    }
}

/***********************************************************************
 *           X11DRV_KeyEvent
 *
 * Handle a X key event
 */
BOOL X11DRV_KeyEvent( HWND hwnd, XEvent *xev )
{
    XKeyEvent *event = &xev->xkey;
    KeySym keysym = 0;
    WORD vkey = 0, scan;
    DWORD dwFlags;
    XIC xic = X11DRV_get_ic( hwnd );
    DWORD event_time = EVENT_x11_time_to_win32_time(event->time);

    TRACE_(key)("type %d, window %lx, state 0x%04x, keycode %u\n",
		event->type, event->window, event->state, event->keycode);

    if (event->type == KeyPress) update_user_time( event->time );

    pthread_mutex_lock( &kbd_mutex );

    set_current_xkb_group( hwnd, event->state >> 13 );

    /* If XKB extensions are used, the state mask for AltGr will use the group
       index instead of the modifier mask. The group index is set in bits
       13-14 of the state field in the XKeyEvent structure. So if AltGr is
       pressed, look if the group index is different than 0. From XKB
       extension documentation, the group index for AltGr should be 2
       (event->state = 0x2000). It's probably better to not assume a
       predefined group index and find it dynamically

       Ref: X Keyboard Extension: Library specification (section 14.1.1 and 17.1.1) */
    /* Save also all possible modifier states. */
    AltGrMask = event->state & (0x6000 | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask);

    if (TRACE_ON(key)){
	const char *ksname;

        ksname = XKeysymToString(keysym);
	if (!ksname)
	  ksname = "No Name";
	TRACE_(key)("%s : keysym=%lx (%s)\n", (event->type == KeyPress) ? "KeyPress" : "KeyRelease", keysym, ksname);
    }

    scan = keyc2scan( event->keycode );
    vkey = scan2vk[scan];

    TRACE_(key)("keycode %u converted to vkey 0x%X scan %04x\n",
                event->keycode, vkey, scan);

    pthread_mutex_unlock( &kbd_mutex );

    if (!vkey) return FALSE;

    dwFlags = 0;
    if ( event->type == KeyRelease ) dwFlags |= KEYEVENTF_KEYUP;
    if ( scan & 0x100 )             dwFlags |= KEYEVENTF_EXTENDEDKEY;

    update_lock_state( hwnd, vkey, event->state, event_time );

    X11DRV_send_keyboard_input( hwnd, vkey & 0xff, scan & 0xff, dwFlags, event_time );
    return TRUE;
}

static void init_xkb_layouts( Display *display )
{
    const char *rules, *model, *layouts, *layout, *variants, *variant, *options;
    unsigned long count, remaining;
    struct layout *entry;
    unsigned char *data;
    int i, format;
    Atom type;

    if (XGetWindowProperty( display, DefaultRootWindow( display ), x11drv_atom(_XKB_RULES_NAMES), 0,
                            1024, False, XA_STRING, &type, &format, &count, &remaining, &data ))
        data = NULL;

    if (!data || type != XA_STRING || format != 8 || remaining)
    {
        rules = model = layouts = variants = options = "";
        ERR( "Failed to read _XKB_RULES_NAMES property, assuming en-US QWERTY keyboard\n" );
    }
    else
    {
        int pos = 0;

        rules = (char *)data + pos;
        pos += strlen( rules ) + 1;
        model = (char *)data + pos;
        pos += strlen( model ) + 1;
        layouts = (char *)data + pos;
        pos += strlen( layouts ) + 1;
        variants = (char *)data + pos;
        pos += strlen( variants ) + 1;
        options = (char *)data + pos;
        pos += strlen( options ) + 1;

        TRACE( "Found rules %s, model %s, layouts %s, variants %s, options %s\n", debugstr_a(rules),
               debugstr_a(model), debugstr_a(layouts), debugstr_a(variants), debugstr_a(options) );
    }

    /* Flag any previously created Xkb layout as invalid */
    LIST_FOR_EACH_ENTRY( entry, &xkb_layouts, struct layout, entry )
        entry->xkb_group = -1;

    /* There can be up to 4 active layouts, exposed as Xkb groups,
     * with round robin if there is less than 4 configured layouts
     */
    for (layout = layouts, variant = variants, i = 0; i < 4; ++i)
    {
        const char *next_layout = strchr( layout, ',' ), *next_variant = strchr( variant, ',' );
        int layout_len, variant_len;
        char buffer[1024];
        LANGID lang;

        if (!next_layout) next_layout = layout + strlen( layout );
        if (!next_variant) next_variant = variant + strlen( variant );

        layout_len = next_layout - layout;
        variant_len = next_variant - variant;

        lang = langid_from_xkb_layout( layout, layout_len );
        snprintf( buffer, ARRAY_SIZE(buffer), "%.*s:%.*s:%s", layout_len, layout, variant_len, variant, options );
        create_layout_from_xkb( display, i, buffer, lang );

        layout = *next_layout ? next_layout + 1 : layouts;
        variant = *next_layout ? (*next_variant ? next_variant + 1 : next_variant) : variants;
    }

    XFree( data );
}


/**********************************************************************
 *		X11DRV_InitKeyboard
 */
void X11DRV_InitKeyboard( Display *display )
{
    XModifierKeymap *mmp;
    KeyCode *kcp;
    int i;

    if (use_server_x11) return;

    pthread_mutex_lock( &kbd_mutex );
    XDisplayKeycodes(display, &min_keycode, &max_keycode);
    XFree( XGetKeyboardMapping( display, min_keycode, max_keycode + 1 - min_keycode, &keysyms_per_keycode ) );

    if (min_keycode + 0xff < max_keycode)
    {
        FIXME( "Unsupported keycode min %u, max %u\n", min_keycode, max_keycode );
        max_keycode = min_keycode + 0xff;
    }

    mmp = XGetModifierMapping(display);
    kcp = mmp->modifiermap;
    for (i = 0; i < 8; i += 1) /* There are 8 modifier keys */
    {
        int j;

        for (j = 0; j < mmp->max_keypermod; j += 1, kcp += 1)
	    if (*kcp)
            {
		int k;

		for (k = 0; k < keysyms_per_keycode; k += 1)
                    if (XkbKeycodeToKeysym( display, *kcp, 0, k ) == XK_Num_Lock)
		    {
                        NumLockMask = 1 << i;
                        TRACE_(key)("NumLockMask is %x\n", NumLockMask);
		    }
                    else if (XkbKeycodeToKeysym( display, *kcp, 0, k ) == XK_Scroll_Lock)
		    {
                        ScrollLockMask = 1 << i;
                        TRACE_(key)("ScrollLockMask is %x\n", ScrollLockMask);
		    }
            }
    }
    XFreeModifiermap(mmp);

    init_xkb_layouts( display );

    pthread_mutex_unlock( &kbd_mutex );
}


/***********************************************************************
 *		ActivateKeyboardLayout (X11DRV.@)
 */
BOOL X11DRV_ActivateKeyboardLayout(HKL hkl, UINT flags)
{
    FIXME("%p, %04x: semi-stub!\n", hkl, flags);

    if (use_server_x11) return TRUE;

    if (flags & KLF_SETFORPROCESS)
    {
        RtlSetLastWin32Error( ERROR_CALL_NOT_IMPLEMENTED );
        FIXME("KLF_SETFORPROCESS not supported\n");
        return FALSE;
    }

    return TRUE;
}

static BOOL X11DRV_KeyboardMappingNotify( HWND dummy, XEvent *event )
{
    XRefreshKeyboardMapping(&event->xmapping);
    return FALSE;
}

/***********************************************************************
 *           X11DRV_MappingNotify
 */
BOOL X11DRV_MappingNotify( HWND dummy, XEvent *event )
{
    switch (event->xmapping.request)
    {
    case MappingModifier:
    case MappingKeyboard:
        return X11DRV_KeyboardMappingNotify( dummy, event );
    case MappingPointer:
        X11DRV_InitMouse( event->xmapping.display );
        break;
    }

    return FALSE;
}


/***********************************************************************
 *		VkKeyScanEx (X11DRV.@)
 *
 * Note: Windows ignores HKL parameter and uses current active layout instead
 */
SHORT X11DRV_VkKeyScanEx( WCHAR wChar, HKL hkl )
{
    Display *display = thread_init_display();
    KeyCode keycode;
    KeySym keysym;
    int index;
    CHAR cChar;
    SHORT ret;

    if (use_server_x11) return -256;

    /* FIXME: what happens if wChar is not a Latin1 character and CP_UNIXCP
     * is UTF-8 (multibyte encoding)?
     */
    if (!ntdll_wcstoumbs( &wChar, 1, &cChar, 1, FALSE ))
    {
        WARN("no translation from unicode to CP_UNIXCP for 0x%02x\n", wChar);
        return -1;
    }

    TRACE("wChar 0x%02x -> cChar '%c'\n", wChar, cChar);

    /* char->keysym (same for ANSI chars) */
    keysym = (unsigned char)cChar; /* (!) cChar is signed */
    if (keysym <= 27) keysym += 0xFF00; /* special chars : return, backspace... */

    keycode = XKeysymToKeycode(display, keysym);  /* keysym -> keycode */
    if (!keycode)
    {
        if (keysym >= 0xFF00) /* Windows returns 0x0240 + cChar in this case */
        {
            ret = 0x0240 + cChar; /* 0x0200 indicates a control character */
            TRACE(" ... returning ctrl char %#.2x\n", ret);
            return ret;
        }
        /* It didn't work ... let's try with deadchar code. */
        TRACE("retrying with | 0xFE00\n");
        keycode = XKeysymToKeycode(display, keysym | 0xFE00);
    }

    TRACE("'%c'(%lx): got keycode %u\n", cChar, keysym, keycode);
    if (!keycode) return -1;

    pthread_mutex_lock( &kbd_mutex );

    /* keycode -> (keyc2vkey) vkey */
    ret = scan2vk[keyc2scan( keycode )];
    if (!ret)
    {
        pthread_mutex_unlock( &kbd_mutex );
        TRACE("keycode for '%c' not found, returning -1\n", cChar);
        return -1;
    }

    for (index = 0; index < 4; index++) /* find shift state */
        if (XkbKeycodeToKeysym( display, keycode, 0, index ) == keysym) break;

    pthread_mutex_unlock( &kbd_mutex );

    switch (index)
    {
        case 0: break;
        case 1: ret += 0x0100; break;
        case 2: ret += 0x0600; break;
        case 3: ret += 0x0700; break;
        default:
            WARN("Keysym %lx not found while parsing the keycode table\n", keysym);
            return -1;
    }
    /*
      index : 0     adds 0x0000
      index : 1     adds 0x0100 (shift)
      index : ?     adds 0x0200 (ctrl)
      index : 2     adds 0x0600 (ctrl+alt)
      index : 3     adds 0x0700 (ctrl+alt+shift)
     */

    TRACE(" ... returning %#.2x\n", ret);
    return ret;
}

/***********************************************************************
 *		MapVirtualKeyEx (X11DRV.@)
 */
UINT X11DRV_MapVirtualKeyEx( UINT wCode, UINT wMapType, HKL hkl )
{
    UINT ret = 0;
    int keyc;
    Display *display = thread_init_display();

    if (use_server_x11) return -1;
    if (wMapType != MAPVK_VK_TO_CHAR) return -1;

    TRACE("wCode=0x%x, wMapType=%d, hkl %p\n", wCode, wMapType, hkl);

    pthread_mutex_lock( &kbd_mutex );

    switch(wMapType)
    {
        case MAPVK_VK_TO_CHAR: /* vkey-code to unshifted ANSI code */
        {
            /* we still don't know what "unshifted" means. in windows VK_W (0x57)
             * returns 0x57, which is uppercase 'W'. So we have to return the uppercase
             * key.. Looks like something is wrong with the MS docs?
             * This is only true for letters, for example VK_0 returns '0' not ')'.
             * - hence we use the lock mask to ensure this happens.
             */
            /* let's do vkey -> keycode -> (XLookupString) ansi char */
            XKeyEvent e;
            KeySym keysym;
            int len;
            char s[10];

            e.display = display;
            e.state = 0;
            e.keycode = 0;
            e.type = KeyPress;

            /* We exit on the first keycode found, to speed up the thing. */
            for (keyc=min_keycode; (keyc<=max_keycode) && (!e.keycode) ; keyc++)
            { /* Find a keycode that could have generated this virtual key */
                WORD scan = keyc2scan( keyc );
                if  ((scan2vk[scan] & 0xFF) == wCode)
                { /* We filter the extended bit, we don't know it */
                    e.keycode = keyc; /* Store it temporarily */
                    if ((scan2vk[keyc2scan( keyc )] & 0xff) != wCode) {
                        e.keycode = 0; /* Wrong one (ex: because of the NumLock
                                          state), so set it to 0, we'll find another one */
                    }
                }
            }

            if ((wCode>=VK_NUMPAD0) && (wCode<=VK_NUMPAD9))
                e.keycode = XKeysymToKeycode(e.display, wCode-VK_NUMPAD0+XK_KP_0);

            /* Windows always generates VK_DECIMAL for Del/. on keypad while some
             * X11 keyboard layouts generate XK_KP_Separator instead of XK_KP_Decimal
             * in order to produce a locale dependent numeric separator.
             */
            if (wCode == VK_DECIMAL || wCode == VK_SEPARATOR)
            {
                e.keycode = XKeysymToKeycode(e.display, XK_KP_Separator);
                if (!e.keycode)
                    e.keycode = XKeysymToKeycode(e.display, XK_KP_Decimal);
            }

            if (!e.keycode)
            {
                WARN("Unknown virtual key %X !!!\n", wCode);
                break;
            }
            TRACE("Found keycode %u\n",e.keycode);

            len = XLookupString(&e, s, sizeof(s), &keysym, NULL);
            if (len)
            {
                WCHAR wch;
                if (ntdll_umbstowcs( s, len, &wch, 1 )) ret = RtlUpcaseUnicodeChar( wch );
            }
            break;
        }
    }

    pthread_mutex_unlock( &kbd_mutex );
    TRACE( "returning 0x%x.\n", ret );
    return ret;
}


/***********************************************************************
 *		X11DRV_KEYBOARD_MapDeadKeysym
 */
static char KEYBOARD_MapDeadKeysym(KeySym keysym)
{
	switch (keysym)
	    {
	/* symbolic ASCII is the same as defined in rfc1345 */
#ifdef XK_dead_tilde
	    case XK_dead_tilde :
#endif
	    case 0x1000FE7E : /* Xfree's XK_Dtilde */
		return '~';	/* '? */
#ifdef XK_dead_acute
	    case XK_dead_acute :
#endif
	    case 0x1000FE27 : /* Xfree's XK_Dacute_accent */
		return 0xb4;	/* '' */
#ifdef XK_dead_circumflex
	    case XK_dead_circumflex:
#endif
	    case 0x1000FE5E : /* Xfree's XK_Dcircumflex_accent */
		return '^';	/* '> */
#ifdef XK_dead_grave
	    case XK_dead_grave :
#endif
	    case 0x1000FE60 : /* Xfree's XK_Dgrave_accent */
		return '`';	/* '! */
#ifdef XK_dead_diaeresis
	    case XK_dead_diaeresis :
#endif
	    case 0x1000FE22 : /* Xfree's XK_Ddiaeresis */
		return 0xa8;	/* ': */
#ifdef XK_dead_cedilla
	    case XK_dead_cedilla :
	        return 0xb8;	/* ', */
#endif
#ifdef XK_dead_macron
	    case XK_dead_macron :
	        return '-';	/* 'm isn't defined on iso-8859-x */
#endif
#ifdef XK_dead_breve
	    case XK_dead_breve :
	        return 0xa2;	/* '( */
#endif
#ifdef XK_dead_abovedot
	    case XK_dead_abovedot :
	        return 0xff;	/* '. */
#endif
#ifdef XK_dead_abovering
	    case XK_dead_abovering :
	        return '0';	/* '0 isn't defined on iso-8859-x */
#endif
#ifdef XK_dead_doubleacute
	    case XK_dead_doubleacute :
	        return 0xbd;	/* '" */
#endif
#ifdef XK_dead_caron
	    case XK_dead_caron :
	        return 0xb7;	/* '< */
#endif
#ifdef XK_dead_ogonek
	    case XK_dead_ogonek :
	        return 0xb2;	/* '; */
#endif
/* FIXME: I don't know this three.
	    case XK_dead_iota :
	        return 'i';
	    case XK_dead_voiced_sound :
	        return 'v';
	    case XK_dead_semivoiced_sound :
	        return 's';
*/
	    }
	TRACE("no character for dead keysym 0x%08lx\n",keysym);
	return 0;
}

/***********************************************************************
 *		ToUnicodeEx (X11DRV.@)
 *
 * The ToUnicode function translates the specified virtual-key code and keyboard
 * state to the corresponding Windows character or characters.
 *
 * If the specified key is a dead key, the return value is negative. Otherwise,
 * it is one of the following values:
 * Value	Meaning
 * 0	The specified virtual key has no translation for the current state of the keyboard.
 * 1	One Windows character was copied to the buffer.
 * 2	Two characters were copied to the buffer. This usually happens when a
 *      dead-key character (accent or diacritic) stored in the keyboard layout cannot
 *      be composed with the specified virtual key to form a single character.
 *
 * FIXME : should do the above (return 2 for non matching deadchar+char combinations)
 *
 */
INT X11DRV_ToUnicodeEx( UINT virtKey, UINT scanCode, const BYTE *lpKeyState,
                        LPWSTR bufW, int bufW_size, UINT flags, HKL hkl )
{
    Display *display = thread_init_display();
    XKeyEvent e;
    KeySym keysym = 0;
    INT ret;
    int keyc;
    char buf[10];
    char *lpChar = buf;
    HWND focus;
    XIC xic;
    Status status = 0;

    if (use_server_x11) return -2;

    if (scanCode & 0x8000)
    {
        TRACE_(key)("Key UP, doing nothing\n" );
        return 0;
    }

    if ((lpKeyState[VK_MENU] & 0x80) && (lpKeyState[VK_CONTROL] & 0x80))
    {
        TRACE_(key)("Ctrl+Alt+[key] won't generate a character\n");
        return 0;
    }

    e.display = display;
    e.keycode = 0;
    e.state = 0;
    e.type = KeyPress;

    focus = x11drv_thread_data()->last_xic_hwnd;
    if (!focus)
    {
        focus = get_focus();
        if (focus) focus = NtUserGetAncestor( focus, GA_ROOT );
        if (!focus) focus = get_active_window();
    }
    e.window = X11DRV_get_whole_window( focus );
    xic = X11DRV_get_ic( focus );

    pthread_mutex_lock( &kbd_mutex );

    if (lpKeyState[VK_SHIFT] & 0x80)
    {
	TRACE_(key)("ShiftMask = %04x\n", ShiftMask);
	e.state |= ShiftMask;
    }
    if (lpKeyState[VK_CAPITAL] & 0x01)
    {
	TRACE_(key)("LockMask = %04x\n", LockMask);
	e.state |= LockMask;
    }
    if (lpKeyState[VK_CONTROL] & 0x80)
    {
	TRACE_(key)("ControlMask = %04x\n", ControlMask);
	e.state |= ControlMask;
    }
    if (lpKeyState[VK_NUMLOCK] & 0x01)
    {
	TRACE_(key)("NumLockMask = %04x\n", NumLockMask);
	e.state |= NumLockMask;
    }

    /* Restore saved AltGr state */
    TRACE_(key)("AltGrMask = %04x\n", AltGrMask);
    e.state |= AltGrMask;

    TRACE_(key)("(%04X, %04X) : faked state = 0x%04x\n",
		virtKey, scanCode, e.state);

    /* We exit on the first keycode found, to speed up the thing. */
    for (keyc=min_keycode; (keyc<=max_keycode) && (!e.keycode) ; keyc++)
      { /* Find a keycode that could have generated this virtual key */
          WORD scan = keyc2scan( keyc );
          if  ((scan2vk[scan] & 0xFF) == virtKey)
          { /* We filter the extended bit, we don't know it */
              e.keycode = keyc; /* Store it temporarily */
              if ((scan2vk[keyc2scan( keyc )] & 0xff) != virtKey) {
                  e.keycode = 0; /* Wrong one (ex: because of the NumLock
                         state), so set it to 0, we'll find another one */
              }
	  }
      }

    if (virtKey >= VK_LEFT && virtKey <= VK_DOWN)
        e.keycode = XKeysymToKeycode(e.display, virtKey - VK_LEFT + XK_Left);

    if ((virtKey>=VK_NUMPAD0) && (virtKey<=VK_NUMPAD9))
        e.keycode = XKeysymToKeycode(e.display, virtKey-VK_NUMPAD0+XK_KP_0);

    /* Windows always generates VK_DECIMAL for Del/. on keypad while some
     * X11 keyboard layouts generate XK_KP_Separator instead of XK_KP_Decimal
     * in order to produce a locale dependent numeric separator.
     */
    if (virtKey == VK_DECIMAL || virtKey == VK_SEPARATOR)
    {
        e.keycode = XKeysymToKeycode(e.display, XK_KP_Separator);
        if (!e.keycode)
            e.keycode = XKeysymToKeycode(e.display, XK_KP_Decimal);
    }

    /* Ctrl-Space generates space on Windows */
    if (e.state == ControlMask && virtKey == VK_SPACE)
    {
        bufW[0] = ' ';
        ret = 1;
        goto found;
    }

    if (!e.keycode && virtKey != VK_NONAME)
      {
	WARN_(key)("Unknown virtual key %X !!!\n", virtKey);
        pthread_mutex_unlock( &kbd_mutex );
	return 0;
      }
    else TRACE_(key)("Found keycode %u\n",e.keycode);

    TRACE_(key)("type %d, window %lx, state 0x%04x, keycode %u\n",
		e.type, e.window, e.state, e.keycode);

    /* Clients should pass only KeyPress events to XmbLookupString,
     * e.type was set to KeyPress above.
     */
    if (xic)
    {
        ret = XmbLookupString(xic, &e, buf, sizeof(buf), &keysym, &status);
        TRACE_(key)("XmbLookupString needs %d byte(s)\n", ret);
        if (status == XBufferOverflow)
        {
            lpChar = malloc( ret );
            if (lpChar == NULL)
            {
                ERR_(key)("Failed to allocate memory!\n");
                pthread_mutex_unlock( &kbd_mutex );
                return 0;
            }
            ret = XmbLookupString(xic, &e, lpChar, ret, &keysym, &status);
        }
    }
    else
        ret = XLookupString(&e, buf, sizeof(buf), &keysym, NULL);

    TRACE_(key)("nbyte = %d, status 0x%x\n", ret, status);

    if (TRACE_ON(key))
    {
        const char *ksname;

        ksname = XKeysymToString(keysym);
        if (!ksname) ksname = "No Name";
        TRACE_(key)("%s : keysym=%lx (%s), # of chars=%d / %s\n",
                    (e.type == KeyPress) ? "KeyPress" : "KeyRelease",
                    keysym, ksname, ret, debugstr_an(lpChar, ret));
    }

    if (ret == 0)
    {
	char dead_char;

#ifdef XK_EuroSign
        /* An ugly hack for EuroSign: X can't translate it to a character
           for some locales. */
        if (keysym == XK_EuroSign)
        {
            bufW[0] = 0x20AC;
            ret = 1;
            goto found;
        }
#endif
        /* Special case: X turns shift-tab into ISO_Left_Tab. */
        /* Here we change it back. */
        if (keysym == XK_ISO_Left_Tab && !(e.state & ControlMask))
        {
            bufW[0] = 0x09;
            ret = 1;
            goto found;
        }

	dead_char = KEYBOARD_MapDeadKeysym(keysym);
	if (dead_char)
        {
	    ntdll_umbstowcs( &dead_char, 1, bufW, bufW_size );
	    ret = -1;
            goto found;
        }

        if (keysym >= 0x01000100 && keysym <= 0x0100ffff)
        {
            /* Unicode direct mapping */
            bufW[0] = keysym & 0xffff;
            ret = 1;
            goto found;
        }
        else if ((keysym >> 8) == 0x1008FF) {
            bufW[0] = 0;
            ret = 0;
            goto found;
        }
	else
	    {
	    const char *ksname;

	    ksname = XKeysymToString(keysym);
	    if (!ksname)
		ksname = "No Name";
	    if ((keysym >> 8) != 0xff)
		{
		WARN_(key)("no char for keysym %04lx (%s) :\n",
                    keysym, ksname);
		WARN_(key)("virtKey=%X, scanCode=%X, keycode=%u, state=%X\n",
                    virtKey, scanCode, e.keycode, e.state);
		}
	    }
	}
    else {  /* ret != 0 */
        /* We have a special case to handle : Shift + arrow, shift + home, ...
           X returns a char for it, but Windows doesn't. Let's eat it. */
        if (!(e.state & NumLockMask)  /* NumLock is off */
            && (e.state & ShiftMask) /* Shift is pressed */
            && (keysym>=XK_KP_0) && (keysym<=XK_KP_9))
        {
            lpChar[0] = 0;
            ret = 0;
        }

        /* more areas where X returns characters but Windows does not
           CTRL + number or CTRL + symbol */
        if (e.state & ControlMask)
        {
            if (((keysym>=33) && (keysym < '@')) ||
                (keysym == '`') ||
                (keysym == XK_Tab))
            {
                lpChar[0] = 0;
                ret = 0;
            }
        }

        /* We have another special case for delete key (XK_Delete) on an
         extended keyboard. X returns a char for it, but Windows doesn't */
        if (keysym == XK_Delete)
        {
            lpChar[0] = 0;
            ret = 0;
        }
	else if((lpKeyState[VK_SHIFT] & 0x80) /* Shift is pressed */
		&& (keysym == XK_KP_Decimal))
        {
            lpChar[0] = 0;
            ret = 0;
        }
	else if((lpKeyState[VK_CONTROL] & 0x80) /* Control is pressed */
		&& (keysym == XK_Return || keysym == XK_KP_Enter))
        {
            if (lpKeyState[VK_SHIFT] & 0x80)
            {
                lpChar[0] = 0;
                ret = 0;
            }
            else
            {
                lpChar[0] = '\n';
                ret = 1;
            }
        }

        /* Hack to detect an XLookupString hard-coded to Latin1 */
        if (ret == 1 && keysym >= 0x00a0 && keysym <= 0x00ff && (BYTE)lpChar[0] == keysym)
        {
            bufW[0] = (BYTE)lpChar[0];
            goto found;
        }

	/* perform translation to unicode */
	if(ret)
	{
	    TRACE_(key)("Translating char 0x%02x to unicode\n", *(BYTE *)lpChar);
            ret = ntdll_umbstowcs( lpChar, ret, bufW, bufW_size );
	}
    }

found:
    if (buf != lpChar)
        free( lpChar );

    pthread_mutex_unlock( &kbd_mutex );

    /* Null-terminate the buffer, if there's room.  MSDN clearly states that the
       caller must not assume this is done, but some programs (e.g. Audiosurf) do. */
    if (1 <= ret && ret < bufW_size)
        bufW[ret] = 0;

    TRACE_(key)("returning %d with %s\n", ret, debugstr_wn(bufW, ret));
    return ret;
}

/***********************************************************************
 *		Beep (X11DRV.@)
 */
void X11DRV_Beep(void)
{
    XBell(gdi_display, 0);
}

struct layout
{
    struct list entry;
    LONG ref;
    HKL hkl;

    UINT group_mask;
    UINT shift_mask;
    UINT ctrl_mask;
    UINT alt_mask;
    UINT altgr_mask;
    UINT caps_mask;
    UINT num_mask;

    union
    {
        KBDTABLES tables;
        char buffer[32 * 1024];
    };
};

static pthread_mutex_t layout_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct list layouts = LIST_INIT( layouts );

static void layout_addref( struct layout *layout )
{
    InterlockedIncrement( &layout->ref );
}

static void layout_release( struct layout *layout )
{
    if (!InterlockedDecrement( &layout->ref )) free( layout );
}

static struct layout *find_layout_from_hkl( HKL hkl )
{
    struct layout *layout;
    VSC_LPWSTR *names;

    pthread_mutex_lock( &layout_mutex );

    LIST_FOR_EACH_ENTRY( layout, &layouts, struct layout, entry )
        if (HIWORD(hkl) == HIWORD(layout->hkl)) break;
    if (&layout->entry == &layouts) layout = NULL;
    else layout_addref( layout );

    pthread_mutex_unlock( &layout_mutex );

    if (layout) return layout;
    if (!(layout = malloc( sizeof(*layout) ))) return NULL;
    layout->ref = 1;
    layout->hkl = hkl;

    SERVER_START_REQ( x11_kbdtables )
    {
        req->layout = HandleToULong( hkl );
        wine_server_set_reply( req, &layout->buffer, sizeof(layout->buffer) );
        if (!wine_server_call_err( req ))
        {
            layout->group_mask = reply->group_mask;
            layout->shift_mask = reply->shift_mask;
            layout->ctrl_mask = reply->ctrl_mask;
            layout->alt_mask = reply->alt_mask;
            layout->altgr_mask = reply->altgr_mask;
            layout->caps_mask = reply->caps_mask;
            layout->num_mask = reply->num_mask;
        }
        else
        {
            free( layout );
            layout = NULL;
        }
    }
    SERVER_END_REQ;

    if (!layout)
    {
        ERR( "Failed to find layout for HKL %p\n", hkl );
        return NULL;
    }

#define KBDTABLES_FIXUP_POINTER( base, ptr ) (ptr) = (void *)((char *)(base) + (size_t)(ptr))

    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pKeyNames );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pKeyNamesExt );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pusVSCtoVK );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pVSCtoVK_E0 );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pVSCtoVK_E1 );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pCharModifiers );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pVkToWcharTable );

    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pCharModifiers->pVkToBit );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pVkToWcharTable[0].pVkToWchars );

    for (names = layout->tables.pKeyNames; names->vsc; names++)
        KBDTABLES_FIXUP_POINTER( &layout->tables, names->pwsz );
    for (names = layout->tables.pKeyNamesExt; names->vsc; names++)
        KBDTABLES_FIXUP_POINTER( &layout->tables, names->pwsz );

#undef KBDTABLES_FIXUP_POINTER

    pthread_mutex_lock( &layout_mutex );
    layout->ref++;
    list_add_tail( &layouts, &layout->entry );
    pthread_mutex_unlock( &layout_mutex );

    return layout;
}

/***********************************************************************
 *    KbdLayerDescriptor (X11DRV.@)
 */
const KBDTABLES *X11DRV_KbdLayerDescriptor( HKL hkl )
{
    struct layout *layout;

    if (!use_server_x11) return NULL;

    TRACE( "hkl %p\n", hkl );

    if (!(layout = find_layout_from_hkl( hkl ))) return NULL;
    return &layout->tables;
}

/***********************************************************************
 *    ReleaseKbdTables (X11DRV.@)
 */
void X11DRV_ReleaseKbdTables( const KBDTABLES *tables )
{
    struct layout *layout = CONTAINING_RECORD( tables, struct layout, tables );

    if (!use_server_x11) return;

    TRACE( "tables %p\n", tables );

    layout_release( layout );
}

/***********************************************************************
 *              ImeProcessKey (X11DRV.@)
 */
UINT X11DRV_ImeProcessKey( HIMC himc, UINT wparam, UINT lparam, const BYTE *key_state )
{
    WORD i, scan = HIWORD(lparam) & 0x1ff, vkey = LOWORD(wparam);
    BOOL repeat = !!(lparam >> 30), pressed = !(lparam >> 31);
    XKeyEvent event = {.type = pressed ? KeyPress : KeyRelease};
    struct x11drv_win_data *data;
    HWND hwnd;

    TRACE( "himc %p, scan %#x, vkey %#x, repeat %u, pressed %u\n", himc, scan, vkey, repeat, pressed );

    if (!(hwnd = NtUserGetAncestor( get_focus(), GA_ROOT ))) return FALSE;

    if (!(data = get_win_data( hwnd ))) return FALSE;
    event.display = data->display;
    event.window = data->whole_window;
    release_win_data( data );

    if (use_server_x11)
    {
        struct layout *layout;

        for (i = 0; i < 256; i++) if (key2scan( i ) == scan) break;
        if (i == 256) return FALSE;
        event.keycode = i + 8;

        if (!(layout = find_layout_from_hkl( NtUserGetKeyboardLayout( 0 ) ))) return FALSE;
        event.state = layout->group_mask;
        if (key_state[VK_SHIFT] & 0x80) event.state |= layout->shift_mask;
        if (key_state[VK_CAPITAL] & 0x01) event.state |= layout->caps_mask;
        if (key_state[VK_CONTROL] & 0x80 && key_state[VK_MENU]) event.state |= layout->altgr_mask;
        else if (key_state[VK_CONTROL] & 0x80) event.state |= layout->ctrl_mask;
        else if (key_state[VK_MENU] & 0x80) event.state |= layout->alt_mask;
        if (key_state[VK_NUMLOCK] & 0x01) event.state |= layout->num_mask;
        /* FIXME: should we look for VK_SCROLL / Scroll Lock mask somehow? */
        layout_release( layout );

        return xim_process_key( hwnd, event );
    }

    for (i = 0; i < ARRAY_SIZE(keyc2scan); i++) if (keyc2scan[i] == scan) break;
    if (i == ARRAY_SIZE(keyc2scan)) return FALSE;
    event.keycode = i;

    event.state |= AltGrMask & 0x6000; /* restore current Xkb group */
    if (key_state[VK_SHIFT] & 0x80) event.state |= ShiftMask;
    if (key_state[VK_CAPITAL] & 0x01) event.state |= LockMask;
    if (key_state[VK_CONTROL] & 0x80 && key_state[VK_MENU]) event.state |= AltGrMask; /* FIXME: this isn't really AltGr */
    else if (key_state[VK_CONTROL] & 0x80) event.state |= ControlMask;
    else if (key_state[VK_MENU] & 0x80) event.state |= Mod5Mask;
    if (key_state[VK_NUMLOCK] & 0x01) event.state |= NumLockMask;
    if (key_state[VK_SCROLL] & 0x01) event.state |= ScrollLockMask;

    return xim_process_key( hwnd, event );
}


/***********************************************************************
 *    KbdLayerDescriptor (X11DRV.@)
 */
const KBDTABLES *X11DRV_KbdLayerDescriptor( HKL hkl )
{
    struct layout *layout;

    TRACE( "hkl %p\n", hkl );

    pthread_mutex_lock( &kbd_mutex );
    LIST_FOR_EACH_ENTRY( layout, &xkb_layouts, struct layout, entry )
    {
        if (!layout->layout_id && layout->lang == HIWORD(hkl)) break;
        if (layout->layout_id && (layout->layout_id | 0xf000) == HIWORD(hkl)) break;
    }
    if (&layout->entry == &xkb_layouts) layout = NULL;
    pthread_mutex_unlock( &kbd_mutex );

    if (!layout)
    {
        WARN( "Failed to find Xkb layout for HKL %p\n", hkl );
        return NULL;
    }

    TRACE( "Found layout entry %p, hkl %04x%04x id %04x\n",
           layout, layout->index, layout->lang, layout->layout_id );
    return &layout->tables;
}

/***********************************************************************
 *    ReleaseKbdTables (X11DRV.@)
 */
void X11DRV_ReleaseKbdTables( const KBDTABLES *tables )
{
}
