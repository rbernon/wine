/*
 * USER Input processing
 *
 * Copyright 1993 Bob Amstadt
 * Copyright 1996 Albrecht Kleine
 * Copyright 1997 David Faure
 * Copyright 1998 Morten Welinder
 * Copyright 1998 Ulrich Weigand
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

#include "win32u_private.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win);
WINE_DECLARE_DEBUG_CHANNEL(keyboard);


/**********************************************************************
 *	     NtUserAttachThreadInput    (win32u.@)
 */
BOOL WINAPI NtUserAttachThreadInput( DWORD from, DWORD to, BOOL attach )
{
    BOOL ret;

    SERVER_START_REQ( attach_thread_input )
    {
        req->tid_from = from;
        req->tid_to   = to;
        req->attach   = attach;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           get_locale_kbd_layout
 */
static HKL get_locale_kbd_layout(void)
{
    LCID layout;
    LANGID langid;

    /* FIXME:
     *
     * layout = main_key_tab[kbd_layout].lcid;
     *
     * Winword uses return value of GetKeyboardLayout as a codepage
     * to translate ANSI keyboard messages to unicode. But we have
     * a problem with it: for instance Polish keyboard layout is
     * identical to the US one, and therefore instead of the Polish
     * locale id we return the US one.
     */

    NtQueryDefaultLocale( TRUE, &layout );

    /*
     * Microsoft Office expects this value to be something specific
     * for Japanese and Korean Windows with an IME the value is 0xe001
     * We should probably check to see if an IME exists and if so then
     * set this word properly.
     */
    langid = PRIMARYLANGID( LANGIDFROMLCID( layout ) );
    if (langid == LANG_CHINESE || langid == LANG_JAPANESE || langid == LANG_KOREAN)
        layout = MAKELONG( layout, 0xe001 ); /* IME */
    else
        layout = MAKELONG( layout, layout );

    return ULongToHandle( layout );
}

/***********************************************************************
 *	     NtUserGetKeyboardLayout    (win32u.@)
 *
 * Device handle for keyboard layout defaulted to
 * the language id. This is the way Windows default works.
 */
HKL WINAPI NtUserGetKeyboardLayout( DWORD thread_id )
{
    struct user_thread_info *thread = get_user_thread_info();
    HKL layout = thread->kbd_layout;

    if (thread_id && thread_id != GetCurrentThreadId())
        FIXME( "couldn't return keyboard layout for thread %04x\n", thread_id );

    if (!layout) return get_locale_kbd_layout();
    return layout;
}

/**********************************************************************
 *	     NtUserGetKeyState    (win32u.@)
 *
 * An application calls the GetKeyState function in response to a
 * keyboard-input message.  This function retrieves the state of the key
 * at the time the input message was generated.
 */
SHORT WINAPI NtUserGetKeyState( INT vkey )
{
    SHORT retval = 0;

    SERVER_START_REQ( get_key_state )
    {
        req->key = vkey;
        if (!wine_server_call( req )) retval = (signed char)(reply->state & 0x81);
    }
    SERVER_END_REQ;
    TRACE("key (0x%x) -> %x\n", vkey, retval);
    return retval;
}

/**********************************************************************
 *	     NtUserGetKeyboardState    (win32u.@)
 */
BOOL WINAPI NtUserGetKeyboardState( BYTE *state )
{
    BOOL ret;
    UINT i;

    TRACE("(%p)\n", state);

    memset( state, 0, 256 );
    SERVER_START_REQ( get_key_state )
    {
        req->key = -1;
        wine_server_set_reply( req, state, 256 );
        ret = !wine_server_call_err( req );
        for (i = 0; i < 256; i++) state[i] &= 0x81;
    }
    SERVER_END_REQ;
    return ret;
}

/**********************************************************************
 *	     NtUserSetKeyboardState    (win32u.@)
 */
BOOL WINAPI NtUserSetKeyboardState( BYTE *state )
{
    BOOL ret;

    SERVER_START_REQ( set_key_state )
    {
        wine_server_add_data( req, state, 256 );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *	     NtUserVkKeyScanEx    (win32u.@)
 */
WORD WINAPI NtUserVkKeyScanEx( WCHAR chr, HKL layout )
{
    WORD shift = 0x100, ctrl = 0x200;
    SHORT ret;

    TRACE_(keyboard)( "chr %s, layout %p\n", debugstr_wn(&chr, 1), layout );

    if ((ret = user_driver->pVkKeyScanEx( chr, layout )) != -256) return ret;

    /* FIXME: English keyboard layout specific */

    if (chr == VK_CANCEL || chr == VK_BACK || chr == VK_TAB || chr == VK_RETURN ||
        chr == VK_ESCAPE || chr == VK_SPACE) ret = chr;
    else if (chr >= '0' && chr <= '9') ret = chr;
    else if (chr == ')') ret = shift + '0';
    else if (chr == '!') ret = shift + '1';
    else if (chr == '@') ret = shift + '2';
    else if (chr == '#') ret = shift + '3';
    else if (chr == '$') ret = shift + '4';
    else if (chr == '%') ret = shift + '5';
    else if (chr == '^') ret = shift + '6';
    else if (chr == '&') ret = shift + '7';
    else if (chr == '*') ret = shift + '8';
    else if (chr == '(') ret = shift + '9';
    else if (chr >= 'a' && chr <= 'z') ret = chr - 'a' + 'A';
    else if (chr >= 'A' && chr <= 'Z') ret = shift + chr;
    else if (chr == ';') ret = VK_OEM_1;
    else if (chr == '=') ret = VK_OEM_PLUS;
    else if (chr == ',') ret = VK_OEM_COMMA;
    else if (chr == '-') ret = VK_OEM_MINUS;
    else if (chr == '.') ret = VK_OEM_PERIOD;
    else if (chr == '/') ret = VK_OEM_2;
    else if (chr == '`') ret = VK_OEM_3;
    else if (chr == '[') ret = VK_OEM_4;
    else if (chr == '\\') ret = VK_OEM_5;
    else if (chr == ']') ret = VK_OEM_6;
    else if (chr == '\'') ret = VK_OEM_7;
    else if (chr == ':') ret = shift + VK_OEM_1;
    else if (chr == '+') ret = shift + VK_OEM_PLUS;
    else if (chr == '<') ret = shift + VK_OEM_COMMA;
    else if (chr == '_') ret = shift + VK_OEM_MINUS;
    else if (chr == '>') ret = shift + VK_OEM_PERIOD;
    else if (chr == '?') ret = shift + VK_OEM_2;
    else if (chr == '~') ret = shift + VK_OEM_3;
    else if (chr == '{') ret = shift + VK_OEM_4;
    else if (chr == '|') ret = shift + VK_OEM_5;
    else if (chr == '}') ret = shift + VK_OEM_6;
    else if (chr == '\"') ret = shift + VK_OEM_7;
    else if (chr == 0x7f) ret = ctrl + VK_BACK;
    else if (chr == '\n') ret = ctrl + VK_RETURN;
    else if (chr == 0xf000) ret = ctrl + '2';
    else if (chr == 0x0000) ret = ctrl + shift + '2';
    else if (chr >= 0x0001 && chr <= 0x001a) ret = ctrl + 'A' + chr - 1;
    else if (chr >= 0x001c && chr <= 0x001d) ret = ctrl + VK_OEM_3 + chr;
    else if (chr == 0x001e) ret = ctrl + shift + '6';
    else if (chr == 0x001f) ret = ctrl + shift + VK_OEM_MINUS;
    else ret = -1;

    TRACE_(keyboard)( "ret %04x\n", ret );
    return ret;
}

/* English keyboard layout (0x0409) */
static const UINT kbd_en_vsc2vk[] =
{
    0x00, 0x1b, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0xbd, 0xbb, 0x08, 0x09,
    0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4f, 0x50, 0xdb, 0xdd, 0x0d, 0xa2, 0x41, 0x53,
    0x44, 0x46, 0x47, 0x48, 0x4a, 0x4b, 0x4c, 0xba, 0xde, 0xc0, 0xa0, 0xdc, 0x5a, 0x58, 0x43, 0x56,
    0x42, 0x4e, 0x4d, 0xbc, 0xbe, 0xbf, 0xa1, 0x6a, 0xa4, 0x20, 0x14, 0x70, 0x71, 0x72, 0x73, 0x74,
    0x75, 0x76, 0x77, 0x78, 0x79, 0x90, 0x91, 0x24, 0x26, 0x21, 0x6d, 0x25, 0x0c, 0x27, 0x6b, 0x23,
    0x28, 0x22, 0x2d, 0x2e, 0x2c, 0x00, 0xe2, 0x7a, 0x7b, 0x0c, 0xee, 0xf1, 0xea, 0xf9, 0xf5, 0xf3,
    0x00, 0x00, 0xfb, 0x2f, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0xed,
    0x00, 0xe9, 0x00, 0xc1, 0x00, 0x00, 0x87, 0x00, 0x00, 0x00, 0x00, 0xeb, 0x09, 0x00, 0xc2, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0xe000 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xb1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x0d, 0xa3, 0x00, 0x00,
    0xad, 0xb7, 0xb3, 0x00, 0xb2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xae, 0x00,
    0xaf, 0x00, 0xac, 0x00, 0x00, 0x6f, 0x00, 0x2c, 0xa5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x24, 0x26, 0x21, 0x00, 0x25, 0x00, 0x27, 0x00, 0x23,
    0x28, 0x22, 0x2d, 0x2e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x5c, 0x5d, 0x00, 0x5f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0xab, 0xa8, 0xa9, 0xa7, 0xa6, 0xb6, 0xb4, 0xb5, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0xe100 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const UINT kbd_en_vk2char[] =
{
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x08, 0x09, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x00,
     ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7', '8',  '9', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,  'A',  'B',  'C',  'D',  'E',  'F',  'G', 'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
     'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W', 'X',  'Y',  'Z', 0x00, 0x00, 0x00, 0x00, 0x00,
     '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7', '8',  '9',  '*',  '+', 0x00,  '-',  '.',  '/',
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  ';',  '=',  ',',  '-',  '.',  '/',
     '`', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  '[', '\\',  ']', '\'', 0x00,
    0x00, 0x00, '\\', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const char *kbd_en_vscname[] =
{
    0, "Esc", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Backspace", "Tab",
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Enter", "Ctrl", 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Shift", 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, "Right Shift", "Num *", "Alt", "Space", "Caps Lock", "F1", "F2", "F3", "F4", "F5",
    "F6", "F7", "F8", "F9", "F10", "Pause", "Scroll Lock", "Num 7", "Num 8", "Num 9", "Num -", "Num 4", "Num 5", "Num 6", "Num +", "Num 1",
    "Num 2", "Num 3", "Num 0", "Num Del", "Sys Req", 0, 0, "F11", "F12", 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "F13", "F14", "F15", "F16",
    "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* extended */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Num Enter", "Right Ctrl", 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, "Num /", 0, "Prnt Scrn", "Right Alt", 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, "Num Lock", "Break", "Home", "Up", "Page Up", 0, "Left", 0, "Right", 0, "End",
    "Down", "Page Down", "Insert", "Delete", "<00>", 0, "Help", 0, 0, 0, 0, "Left Windows", "Right Windows", "Application", 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/******************************************************************************
 *	     NtUserMapVirtualKeyEx    (win32u.@)
 */
UINT WINAPI NtUserMapVirtualKeyEx( UINT code, UINT type, HKL layout )
{
    const UINT *vsc2vk, *vk2char;
    UINT vsc2vk_size, vk2char_size;
    UINT ret;

    TRACE_(keyboard)( "code %u, type %u, layout %p.\n", code, type, layout );

    if ((ret = user_driver->pMapVirtualKeyEx( code, type, layout )) != -1) return ret;

    /* FIXME: English keyboard layout specific */

    vsc2vk = kbd_en_vsc2vk;
    vsc2vk_size = ARRAYSIZE(kbd_en_vsc2vk);
    vk2char = kbd_en_vk2char;
    vk2char_size = ARRAYSIZE(kbd_en_vk2char);

    switch (type)
    {
    case MAPVK_VK_TO_VSC_EX:
    case MAPVK_VK_TO_VSC:
        switch (code)
        {
        case VK_SHIFT:   code = VK_LSHIFT; break;
        case VK_CONTROL: code = VK_LCONTROL; break;
        case VK_MENU:    code = VK_LMENU; break;
        case VK_NUMPAD0: code = VK_INSERT; break;
        case VK_NUMPAD1: code = VK_END; break;
        case VK_NUMPAD2: code = VK_DOWN; break;
        case VK_NUMPAD3: code = VK_NEXT; break;
        case VK_NUMPAD4: code = VK_LEFT; break;
        case VK_NUMPAD5: code = VK_CLEAR; break;
        case VK_NUMPAD6: code = VK_RIGHT; break;
        case VK_NUMPAD7: code = VK_HOME; break;
        case VK_NUMPAD8: code = VK_UP; break;
        case VK_NUMPAD9: code = VK_PRIOR; break;
        case VK_DECIMAL: code = VK_DELETE; break;
        }

        for (ret = 0; ret < vsc2vk_size; ++ret) if (vsc2vk[ret] == code) break;
        if (ret >= vsc2vk_size) ret = 0;

        if (type == MAPVK_VK_TO_VSC)
        {
            if (ret >= 0x200) ret = 0;
            else ret &= 0xff;
        }
        else if (ret >= 0x100) ret += 0xdf00;
        break;
    case MAPVK_VSC_TO_VK:
    case MAPVK_VSC_TO_VK_EX:
        if (code & 0xe000) code -= 0xdf00;
        if (code >= vsc2vk_size) ret = 0;
        else ret = vsc2vk[code];

        if (type == MAPVK_VSC_TO_VK)
        {
            switch (ret)
            {
            case VK_LSHIFT:   case VK_RSHIFT:   ret = VK_SHIFT; break;
            case VK_LCONTROL: case VK_RCONTROL: ret = VK_CONTROL; break;
            case VK_LMENU:    case VK_RMENU:    ret = VK_MENU; break;
            }
        }
        break;
    case MAPVK_VK_TO_CHAR:
        if (code >= vk2char_size) ret = 0;
        else ret = vk2char[code];
        break;
    default:
        FIXME_(keyboard)( "unknown type %d\n", type );
        return 0;
    }

    TRACE_(keyboard)( "returning 0x%04x\n", ret );
    return ret;
}

/****************************************************************************
 *	     NtUserGetKeyNameText    (win32u.@)
 */
INT WINAPI NtUserGetKeyNameText( LONG lparam, WCHAR *buffer, INT size )
{
    INT code = ((lparam >> 16) & 0x1ff), vkey, len;
    UINT vsc2vk_size, vscname_size;
    const char *const *vscname;
    const UINT *vsc2vk;

    TRACE_(keyboard)( "lparam %d, buffer %p, size %d.\n", lparam, buffer, size );

    if (!buffer || !size) return 0;
    if ((len = user_driver->pGetKeyNameText( lparam, buffer, size )) >= 0) return len;

    /* FIXME: English keyboard layout specific */

    vsc2vk = kbd_en_vsc2vk;
    vsc2vk_size = ARRAYSIZE(kbd_en_vsc2vk);
    vscname = kbd_en_vscname;
    vscname_size = ARRAYSIZE(kbd_en_vscname);

    if (lparam & 0x2000000)
    {
        switch ((vkey = vsc2vk[code]))
        {
        case VK_RSHIFT:
        case VK_RCONTROL:
        case VK_RMENU:
            for (code = 0; code < vsc2vk_size; ++code)
                if (vsc2vk[code] == (vkey - 1)) break;
            break;
        }
    }

    if (code < vscname_size)
    {
        if (vscname[code])
        {
            len = min( size - 1, strlen(vscname[code]) );
            ascii_to_unicode( buffer, vscname[code], len );
        }
        else if (size > 1)
        {
            HKL hkl = NtUserGetKeyboardLayout( 0 );
            vkey = NtUserMapVirtualKeyEx( code & 0xff, MAPVK_VSC_TO_VK, hkl );
            buffer[0] = NtUserMapVirtualKeyEx( vkey, MAPVK_VK_TO_CHAR, hkl );
            len = 1;
        }
    }
    buffer[len] = 0;

    TRACE_(keyboard)( "ret %d, str %s.\n", len, debugstr_w(buffer) );
    return len;
}

/****************************************************************************
 *	     NtUserToUnicodeEx    (win32u.@)
 */
INT WINAPI NtUserToUnicodeEx( UINT virt, UINT scan, const BYTE *state,
                              WCHAR *str, int size, UINT flags, HKL layout )
{
    BOOL shift, ctrl, alt, numlock;
    WCHAR buffer[2];
    INT len;

    TRACE_(keyboard)( "virt %u, scan %u, state %p, str %p, size %d, flags %x, layout %p.\n",
                      virt, scan, state, str, size, flags, layout );

    if (!state) return 0;
    if ((len = user_driver->pToUnicodeEx( virt, scan, state, str, size, flags, layout )) >= -1)
        return len;

    alt = state[VK_MENU] & 0x80;
    shift = state[VK_SHIFT] & 0x80;
    ctrl = state[VK_CONTROL] & 0x80;
    numlock = state[VK_NUMLOCK] & 0x01;

    /* FIXME: English keyboard layout specific */

    if (scan & 0x8000) buffer[0] = 0; /* key up */
    else if (virt == VK_ESCAPE) buffer[0] = VK_ESCAPE;
    else if (!ctrl)
    {
        switch (virt)
        {
        case VK_BACK:       buffer[0] = '\b'; break;
        case VK_OEM_1:      buffer[0] = shift ? ':' : ';'; break;
        case VK_OEM_2:      buffer[0] = shift ? '?' : '/'; break;
        case VK_OEM_3:      buffer[0] = shift ? '~' : '`'; break;
        case VK_OEM_4:      buffer[0] = shift ? '{' : '['; break;
        case VK_OEM_5:      buffer[0] = shift ? '|' : '\\'; break;
        case VK_OEM_6:      buffer[0] = shift ? '}' : ']'; break;
        case VK_OEM_7:      buffer[0] = shift ? '"' : '\''; break;
        case VK_OEM_COMMA:  buffer[0] = shift ? '<' : ','; break;
        case VK_OEM_MINUS:  buffer[0] = shift ? '_' : '-'; break;
        case VK_OEM_PERIOD: buffer[0] = shift ? '>' : '.'; break;
        case VK_OEM_PLUS:   buffer[0] = shift ? '+' : '='; break;
        case VK_RETURN:     buffer[0] = '\r'; break;
        case VK_SPACE:      buffer[0] = ' '; break;
        case VK_TAB:        buffer[0] = '\t'; break;
        case VK_MULTIPLY:   buffer[0] = '*'; break;
        case VK_ADD:        buffer[0] = '+'; break;
        case VK_SUBTRACT:   buffer[0] = '-'; break;
        case VK_DIVIDE:     buffer[0] = '/'; break;
        default:
            if (virt >= '0' && virt <= '9')
                buffer[0] = shift ? ")!@#$%^&*("[virt - '0'] : virt;
            else if (virt >= 'A' && virt <= 'Z')
                buffer[0] = shift || (state[VK_CAPITAL] & 0x01) ? virt : virt + 'a' - 'A';
            else if (virt >= VK_NUMPAD0 && virt <= VK_NUMPAD9 && numlock && !shift)
                buffer[0] = '0' + virt - VK_NUMPAD0;
            else if (virt == VK_DECIMAL && numlock && !shift)
                buffer[0] = '.';
            else
                buffer[0] = 0;
            break;
        }
    }
    else if (!alt) /* Control codes */
    {
        switch (virt)
        {
        case VK_OEM_4:     buffer[0] = 0x1b; break;
        case VK_OEM_5:     buffer[0] = 0x1c; break;
        case VK_OEM_6:     buffer[0] = 0x1d; break;
        case '6':          buffer[0] = shift ? 0x1e : 0; break;
        case VK_OEM_MINUS: buffer[0] = shift ? 0x1f : 0; break;
        case VK_BACK:      buffer[0] = 0x7f; break;
        case VK_RETURN:    buffer[0] = shift ? 0 : '\n'; break;
        case '2':          buffer[0] = shift ? 0xffff : 0xf000; break;
        case VK_SPACE:     buffer[0] = ' '; break;
        default:
            if (virt >= 'A' && virt <= 'Z') buffer[0] = virt - 'A' + 1;
            else buffer[0] = 0;
            break;
        }
    }
    else buffer[0] = 0;
    buffer[1] = 0;
    len = lstrlenW( buffer );
    if (buffer[0] == 0xffff) buffer[0] = 0;
    lstrcpynW( str, buffer, size );

    TRACE_(keyboard)( "ret %d, str %s.\n", len, debugstr_w(str) );
    return len;
}

/**********************************************************************
 *	     NtUserActivateKeyboardLayout    (win32u.@)
 */
HKL WINAPI NtUserActivateKeyboardLayout( HKL layout, UINT flags )
{
    struct user_thread_info *info = get_user_thread_info();
    HKL old_layout;

    TRACE_(keyboard)( "layout %p, flags %x\n", layout, flags );

    if (flags) FIXME_(keyboard)( "flags %x not supported\n", flags );

    if (layout == (HKL)HKL_NEXT || layout == (HKL)HKL_PREV)
    {
        SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
        FIXME_(keyboard)( "HKL_NEXT and HKL_PREV not supported\n" );
        return 0;
    }

    if (!user_driver->pActivateKeyboardLayout( layout, flags ))
        return 0;

    old_layout = info->kbd_layout;
    info->kbd_layout = layout;
    if (old_layout != layout) info->kbd_layout_id = 0;

    if (!old_layout) return get_locale_kbd_layout();
    return old_layout;
}
