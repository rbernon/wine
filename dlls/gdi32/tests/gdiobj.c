/*
 * Unit test suite for GDI objects
 *
 * Copyright 2002 Mike McCormack
 * Copyright 2004 Dmitry Timoshkov
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
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winnt.h"
#include "winternl.h"
#include "winuser.h"

#include "wine/test.h"

static BOOL is_wow64;
static BOOL (WINAPI *pIsWow64Process)(HANDLE,PBOOL);

static void test_gdi_objects(void)
{
    BYTE buff[256];
    HDC hdc = GetDC(NULL);
    HPEN hp;
    int i;
    BOOL ret;

    /* SelectObject() with a NULL DC returns 0 and sets ERROR_INVALID_HANDLE.
     * Note: Under XP at least invalid ptrs can also be passed, not just NULL;
     *       Don't test that here in case it crashes earlier win versions.
     */
    SetLastError(0);
    hp = SelectObject(NULL, GetStockObject(BLACK_PEN));
    ok(!hp && (GetLastError() == ERROR_INVALID_HANDLE || broken(!GetLastError())),
       "SelectObject(NULL DC) expected 0, ERROR_INVALID_HANDLE, got %p, %u\n",
       hp, GetLastError());

    /* With a valid DC and a NULL object, the call returns 0 but does not SetLastError() */
    SetLastError(0);
    hp = SelectObject(hdc, NULL);
    ok(!hp && !GetLastError(),
       "SelectObject(NULL obj) expected 0, NO_ERROR, got %p, %u\n",
       hp, GetLastError());

    /* The DC is unaffected by the NULL SelectObject */
    SetLastError(0);
    hp = SelectObject(hdc, GetStockObject(BLACK_PEN));
    ok(hp && !GetLastError(),
       "SelectObject(post NULL) expected non-null, NO_ERROR, got %p, %u\n",
       hp, GetLastError());

    /* GetCurrentObject does not SetLastError() on a null object */
    SetLastError(0);
    hp = GetCurrentObject(NULL, OBJ_PEN);
    ok(!hp && !GetLastError(),
       "GetCurrentObject(NULL DC) expected 0, NO_ERROR, got %p, %u\n",
       hp, GetLastError());

    /* DeleteObject does not SetLastError() on a null object */
    ret = DeleteObject(NULL);
    ok( !ret && !GetLastError(),
       "DeleteObject(NULL obj), expected 0, NO_ERROR, got %d, %u\n",
       ret, GetLastError());

    /* GetObject does not SetLastError() on a null object */
    SetLastError(0);
    i = GetObjectA(NULL, sizeof(buff), buff);
    ok (!i && (GetLastError() == 0 || GetLastError() == ERROR_INVALID_PARAMETER),
        "GetObject(NULL obj), expected 0, NO_ERROR, got %d, %u\n",
	i, GetLastError());

    /* GetObject expects ERROR_NOACCESS when passed an invalid buffer */
    hp = SelectObject(hdc, GetStockObject(BLACK_PEN));
    SetLastError(0);
    i = GetObjectA(hp, (INT_PTR)buff, (LPVOID)sizeof(buff));
    ok (!i && (GetLastError() == 0 || GetLastError() == ERROR_NOACCESS),
        "GetObject(invalid buff), expected 0, ERROR_NOACCESS, got %d, %u\n",
    i, GetLastError());

    /* GetObjectType does SetLastError() on a null object */
    SetLastError(0);
    i = GetObjectType(NULL);
    ok (!i && GetLastError() == ERROR_INVALID_HANDLE,
        "GetObjectType(NULL obj), expected 0, ERROR_INVALID_HANDLE, got %d, %u\n",
        i, GetLastError());

    /* UnrealizeObject does not SetLastError() on a null object */
    SetLastError(0);
    i = UnrealizeObject(NULL);
    ok (!i && !GetLastError(),
        "UnrealizeObject(NULL obj), expected 0, NO_ERROR, got %d, %u\n",
        i, GetLastError());

    ReleaseDC(NULL, hdc);
}

struct hgdiobj_event
{
    HDC hdc;
    HGDIOBJ hgdiobj1;
    HGDIOBJ hgdiobj2;
    HANDLE stop_event;
    HANDLE ready_event;
};

static DWORD WINAPI thread_proc(void *param)
{
    LOGPEN lp;
    DWORD status;
    struct hgdiobj_event *hgdiobj_event = param;

    hgdiobj_event->hdc = CreateDCA("display", NULL, NULL, NULL);
    ok(hgdiobj_event->hdc != NULL, "CreateDC error %u\n", GetLastError());

    hgdiobj_event->hgdiobj1 = CreatePen(PS_DASHDOTDOT, 17, RGB(1, 2, 3));
    ok(hgdiobj_event->hgdiobj1 != 0, "Failed to create pen\n");

    hgdiobj_event->hgdiobj2 = CreateRectRgn(0, 1, 12, 17);
    ok(hgdiobj_event->hgdiobj2 != 0, "Failed to create pen\n");

    SetEvent(hgdiobj_event->ready_event);
    status = WaitForSingleObject(hgdiobj_event->stop_event, INFINITE);
    ok(status == WAIT_OBJECT_0, "WaitForSingleObject error %u\n", GetLastError());

    ok(!GetObjectA(hgdiobj_event->hgdiobj1, sizeof(lp), &lp), "GetObject should fail\n");

    ok(!GetDeviceCaps(hgdiobj_event->hdc, TECHNOLOGY), "GetDeviceCaps(TECHNOLOGY) should fail\n");

    return 0;
}

static void test_thread_objects(void)
{
    LOGPEN lp;
    DWORD tid, type;
    HANDLE hthread;
    struct hgdiobj_event hgdiobj_event;
    INT ret;
    DWORD status;
    BOOL bRet;

    hgdiobj_event.stop_event = CreateEventA(NULL, 0, 0, NULL);
    ok(hgdiobj_event.stop_event != NULL, "CreateEvent error %u\n", GetLastError());
    hgdiobj_event.ready_event = CreateEventA(NULL, 0, 0, NULL);
    ok(hgdiobj_event.ready_event != NULL, "CreateEvent error %u\n", GetLastError());

    hthread = CreateThread(NULL, 0, thread_proc, &hgdiobj_event, 0, &tid);
    ok(hthread != NULL, "CreateThread error %u\n", GetLastError());

    status = WaitForSingleObject(hgdiobj_event.ready_event, INFINITE);
    ok(status == WAIT_OBJECT_0, "WaitForSingleObject error %u\n", GetLastError());

    ret = GetObjectA(hgdiobj_event.hgdiobj1, sizeof(lp), &lp);
    ok(ret == sizeof(lp), "GetObject error %u\n", GetLastError());
    ok(lp.lopnStyle == PS_DASHDOTDOT, "wrong pen style %d\n", lp.lopnStyle);
    ok(lp.lopnWidth.x == 17, "wrong pen width.y %d\n", lp.lopnWidth.x);
    ok(lp.lopnWidth.y == 0, "wrong pen width.y %d\n", lp.lopnWidth.y);
    ok(lp.lopnColor == RGB(1, 2, 3), "wrong pen width.y %08x\n", lp.lopnColor);

    ret = GetDeviceCaps(hgdiobj_event.hdc, TECHNOLOGY);
    ok(ret == DT_RASDISPLAY, "GetDeviceCaps(TECHNOLOGY) should return DT_RASDISPLAY not %d\n", ret);

    bRet = DeleteObject(hgdiobj_event.hgdiobj1);
    ok(bRet, "DeleteObject error %u\n", GetLastError());
    bRet = DeleteDC(hgdiobj_event.hdc);
    ok(bRet, "DeleteDC error %u\n", GetLastError());

    type = GetObjectType(hgdiobj_event.hgdiobj2);
    ok(type == OBJ_REGION, "GetObjectType returned %u\n", type);

    SetEvent(hgdiobj_event.stop_event);
    status = WaitForSingleObject(hthread, INFINITE);
    ok(status == WAIT_OBJECT_0, "WaitForSingleObject error %u\n", GetLastError());
    CloseHandle(hthread);

    type = GetObjectType(hgdiobj_event.hgdiobj2);
    ok(type == OBJ_REGION, "GetObjectType returned %u\n", type);
    bRet = DeleteObject(hgdiobj_event.hgdiobj2);
    ok(bRet, "DeleteObject error %u\n", GetLastError());

    CloseHandle(hgdiobj_event.stop_event);
    CloseHandle(hgdiobj_event.ready_event);
}

static void test_GetCurrentObject(void)
{
    DWORD type;
    HPEN hpen;
    HBRUSH hbrush;
    HPALETTE hpal;
    HFONT hfont;
    HBITMAP hbmp;
    HRGN hrgn;
    HDC hdc;
    HCOLORSPACE hcs;
    HGDIOBJ hobj;
    LOGBRUSH lb;
    LOGCOLORSPACEA lcs;

    hdc = CreateCompatibleDC(0);
    assert(hdc != 0);

    type = GetObjectType(hdc);
    ok(type == OBJ_MEMDC, "GetObjectType returned %u\n", type);

    hpen = CreatePen(PS_SOLID, 10, RGB(10, 20, 30));
    assert(hpen != 0);
    SelectObject(hdc, hpen);
    hobj = GetCurrentObject(hdc, OBJ_PEN);
    ok(hobj == hpen, "OBJ_PEN is wrong: %p\n", hobj);
    hobj = GetCurrentObject(hdc, OBJ_EXTPEN);
    ok(hobj == hpen, "OBJ_EXTPEN is wrong: %p\n", hobj);

    hbrush = CreateSolidBrush(RGB(10, 20, 30));
    assert(hbrush != 0);
    SelectObject(hdc, hbrush);
    hobj = GetCurrentObject(hdc, OBJ_BRUSH);
    ok(hobj == hbrush, "OBJ_BRUSH is wrong: %p\n", hobj);

    hpal = CreateHalftonePalette(hdc);
    assert(hpal != 0);
    SelectPalette(hdc, hpal, FALSE);
    hobj = GetCurrentObject(hdc, OBJ_PAL);
    ok(hobj == hpal, "OBJ_PAL is wrong: %p\n", hobj);

    hfont = CreateFontA(10, 5, 0, 0, FW_DONTCARE, 0, 0, 0, ANSI_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                        DEFAULT_PITCH, "MS Sans Serif");
    assert(hfont != 0);
    SelectObject(hdc, hfont);
    hobj = GetCurrentObject(hdc, OBJ_FONT);
    ok(hobj == hfont, "OBJ_FONT is wrong: %p\n", hobj);

    hbmp = CreateBitmap(100, 100, 1, 1, NULL);
    assert(hbmp != 0);
    SelectObject(hdc, hbmp);
    hobj = GetCurrentObject(hdc, OBJ_BITMAP);
    ok(hobj == hbmp, "OBJ_BITMAP is wrong: %p\n", hobj);

    assert(GetObjectA(hbrush, sizeof(lb), &lb) == sizeof(lb));
    hpen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_SQUARE | PS_JOIN_BEVEL,
                        10, &lb, 0, NULL);
    assert(hpen != 0);
    SelectObject(hdc, hpen);
    hobj = GetCurrentObject(hdc, OBJ_PEN);
    ok(hobj == hpen, "OBJ_PEN is wrong: %p\n", hobj);
    hobj = GetCurrentObject(hdc, OBJ_EXTPEN);
    ok(hobj == hpen, "OBJ_EXTPEN is wrong: %p\n", hobj);

    hcs = GetColorSpace(hdc);
    if (hcs)
    {
        trace("current color space is not NULL\n");
        ok(GetLogColorSpaceA(hcs, &lcs, sizeof(lcs)), "GetLogColorSpace failed\n");
        hcs = CreateColorSpaceA(&lcs);
        ok(hcs != 0, "CreateColorSpace failed\n");
        SelectObject(hdc, hcs);
        hobj = GetCurrentObject(hdc, OBJ_COLORSPACE);
        ok(hobj == hcs, "OBJ_COLORSPACE is wrong: %p\n", hobj);
    }

    hrgn = CreateRectRgn(1, 1, 100, 100);
    assert(hrgn != 0);
    SelectObject(hdc, hrgn);
    hobj = GetCurrentObject(hdc, OBJ_REGION);
    ok(!hobj, "OBJ_REGION is wrong: %p\n", hobj);

    DeleteDC(hdc);
}

static void test_region(void)
{
    HRGN hrgn = CreateRectRgn(10, 10, 20, 20);
    RECT rc = { 5, 5, 15, 15 };
    BOOL ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap left and right */
    SetRect( &rc, 15, 5, 5, 15 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap top and bottom */
    SetRect( &rc, 5, 15, 15, 5 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap both */
    SetRect( &rc, 15, 15, 5, 5 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    DeleteObject(hrgn);
    /* swap left and right in the region */
    hrgn = CreateRectRgn(20, 10, 10, 20);
    SetRect( &rc, 5, 5, 15, 15 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap left and right */
    SetRect( &rc, 15, 5, 5, 15 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap top and bottom */
    SetRect( &rc, 5, 15, 15, 5 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap both */
    SetRect( &rc, 15, 15, 5, 5 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    DeleteObject(hrgn);
}

static void test_handles_on_win64(void)
{
    int i;
    BOOL ret;
    DWORD type;
    HRGN hrgn, hrgn_test;

    static const struct
    {
        ULONG high;
        ULONG low;
        BOOL  ret;
    } cases[] =
    {
        { 0x00000000, 0x00000000, TRUE  },
        { 0x00000000, 0x0000ffe0, FALSE }, /* just over MAX_LARGE_HANDLES */
        { 0x00000000, 0x0000ffb0, FALSE }, /* just under MAX_LARGE_HANDLES */
        { 0xffffffff, 0xffff0000, FALSE },
        { 0xffffffff, 0x00000000, TRUE  },
        { 0xdeadbeef, 0x00000000, TRUE  },
        { 0xcccccccc, 0xcccccccc, FALSE }
    };

    if (sizeof(void*) != 8)
        return;

    for (i = 0; i < ARRAY_SIZE(cases); i++)
    {
        hrgn = CreateRectRgn(10, 10, 20, 20);
        hrgn_test = (HRGN)(ULONG_PTR)((ULONG_PTR)hrgn | ((ULONGLONG)cases[i].high << 32) | cases[i].low);
        type = GetObjectType( hrgn_test );
        if (cases[i].ret)
            ok( type == OBJ_REGION, "wrong type %u\n", type );
        else
            ok( type == 0, "wrong type %u\n", type );
        ret = DeleteObject(hrgn_test);
        ok( cases[i].ret == ret, "DeleteObject should return %s (%p)\n",
            cases[i].ret ? "TRUE" : "FALSE", hrgn_test);
        /* actually free it if above is expected to fail */
        if (!ret) DeleteObject(hrgn);
    }
}

/* Adapted from: https://docs.microsoft.com/en-us/archive/msdn-magazine/2003/january/detect-and-plug-gdi-leaks-with-two-powerful-tools-for-windows-xp */

struct gdi_handle_32
{
    LONG32 kernel;
    USHORT pid;
    USHORT count;
    USHORT upper;
    USHORT type;
    LONG32 user;
};

struct gdi_handle_64
{
    LONG64 kernel;
    USHORT pid;
    USHORT count;
    USHORT upper;
    USHORT type;
    LONG64 user;
};

#define MAX_GDI_HANDLES  0x4000

static void get_gdi_handle_64( void *table, DWORD index, struct gdi_handle_64 *out )
{
#ifdef _WIN64
    *out = ((struct gdi_handle_64 *)table)[index];
#else
    struct gdi_handle_32 tmp;
    if (is_wow64) *out = ((struct gdi_handle_64 *)table)[index];
    else
    {
        tmp = ((struct gdi_handle_32 *)table)[index];
        out->kernel = tmp.kernel;
        out->pid = tmp.pid;
        out->count = tmp.count;
        out->upper = tmp.upper;
        out->type = tmp.type;
        out->user = tmp.user;
    }
#endif
}

static void test_GdiSharedHandleTable(void)
{
    char *peb, *table, *copy;
    DWORD i, table_size;

#ifdef _WIN64
    peb = (void *)NtCurrentTeb()->Peb;
    table_size = MAX_GDI_HANDLES * sizeof(struct gdi_handle_64);
    table = *(void **)(peb + FIELD_OFFSET(PEB, GdiSharedHandleTable));
#else
    if (is_wow64)
    {
        peb = (void *)(UINT_PTR)((TEB64 *)NtCurrentTeb()->GdiBatchCount)->Peb;
        table_size = MAX_GDI_HANDLES * sizeof(struct gdi_handle_64);
        table = *(void **)(peb + FIELD_OFFSET(PEB64, GdiSharedHandleTable));
    }
    else
    {
        peb = (void *)NtCurrentTeb()->Peb;
        table_size = MAX_GDI_HANDLES * sizeof(struct gdi_handle_32);
        table = *(void **)(peb + FIELD_OFFSET(PEB, GdiSharedHandleTable));
    }
#endif

    todo_wine ok(table != NULL, "Peb->GdiSharedHandleTable is NULL\n");
    if (!table) return;

    copy = HeapAlloc( GetProcessHeap(), 0, table_size );
    memcpy( copy, table, table_size );

    for (i = 0; i < MAX_GDI_HANDLES; ++i)
    {
        struct gdi_handle_64 entry;
        get_gdi_handle_64( copy, i, &entry );

        if (entry.pid == 0 || (entry.type & 0x7f) == 0) continue;

        ok( entry.kernel < 0, "%04x: Unexpected entry kernel %I64x\n", i, entry.kernel );
        ok( entry.count == 0 || entry.count == 0xffff || entry.count == 0x8000,
            "%04x: Unexpected entry count %x, expected 0 or 0xffff\n", i, entry.count );
        ok( entry.type != 0, "%04x: Unexpected entry type %x\n", i, entry.type );
    }

    HeapFree( GetProcessHeap(), 0, copy );
}

START_TEST(gdiobj)
{
    HMODULE kernel32 = LoadLibraryA( "kernel32.dll" );
    pIsWow64Process = (void *)GetProcAddress( kernel32, "IsWow64Process" );
    if (!pIsWow64Process || !pIsWow64Process( GetCurrentProcess(), &is_wow64 )) is_wow64 = FALSE;
    FreeLibrary( kernel32 );

    test_gdi_objects();
    test_thread_objects();
    test_GetCurrentObject();
    test_region();
    test_handles_on_win64();
    test_GdiSharedHandleTable();
}
