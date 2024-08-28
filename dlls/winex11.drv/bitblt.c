/*
 * GDI bit-blit operations
 *
 * Copyright 1993, 1994, 2011 Alexandre Julliard
 * Copyright 2006 Damjan Jovanovic
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
#include <stdlib.h>

#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
#include <X11/extensions/shape.h>
#endif
#ifdef HAVE_X11_EXTENSIONS_XSHM_H
# include <X11/extensions/XShm.h>
# ifdef HAVE_SYS_SHM_H
#  include <sys/shm.h>
# endif
# ifdef HAVE_SYS_IPC_H
#  include <sys/ipc.h>
# endif
#endif

#ifdef HAVE_X11_XLIB_XCB_H
#include <X11/Xlib-xcb.h>
#endif
#ifdef HAVE_XCB_SHM_H
#include <xcb/shm.h>
#endif

#include "x11drv.h"
#include "winternl.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(bitblt);

static inline BOOL call_xcb( xcb_connection_t *xcb, xcb_void_cookie_t cookie, const char *func, BOOL check )
{
    xcb_generic_error_t *error;

    if (check)
    {
        xcb_discard_reply( xcb, cookie.sequence );
        return TRUE;
    }

    if ((error = xcb_request_check( xcb, cookie )))
    {
        ERR( "%s returned error %d sequence %u resource %#x major %u minor %u\n", func, error->error_code,
             error->sequence, error->resource_id, error->minor_code, error->major_code );
        free( error );
        return TRUE;
    }

    return FALSE;
}

#define XCB_CALL( func, xcb, ... ) \
    call_xcb( xcb, ERR_ON(bitblt) ? (func ## _checked)( (xcb), __VA_ARGS__ ) \
                                  : (func)( (xcb), __VA_ARGS__ ), \
              #func, ERR_ON(bitblt) )

#define DST 0   /* Destination drawable */
#define SRC 1   /* Source drawable */
#define TMP 2   /* Temporary drawable */
#define PAT 3   /* Pattern (brush) in destination DC */

#define OP(src,dst,rop)   (OP_ARGS(src,dst) << 4 | (rop))
#define OP_ARGS(src,dst)  (((src) << 2) | (dst))

#define OP_SRC(opcode)    ((opcode) >> 6)
#define OP_DST(opcode)    (((opcode) >> 4) & 3)
#define OP_SRCDST(opcode) ((opcode) >> 4)
#define OP_ROP(opcode)    ((opcode) & 0x0f)

#define MAX_OP_LEN  6  /* Longest opcode + 1 for the terminating 0 */

static const unsigned char BITBLT_Opcodes[256][MAX_OP_LEN] =
{
    { OP(PAT,DST,GXclear) },                         /* 0x00  0              */
    { OP(PAT,SRC,GXor), OP(SRC,DST,GXnor) },         /* 0x01  ~(D|(P|S))     */
    { OP(PAT,SRC,GXnor), OP(SRC,DST,GXand) },        /* 0x02  D&~(P|S)       */
    { OP(PAT,SRC,GXnor) },                           /* 0x03  ~(P|S)         */
    { OP(PAT,DST,GXnor), OP(SRC,DST,GXand) },        /* 0x04  S&~(D|P)       */
    { OP(PAT,DST,GXnor) },                           /* 0x05  ~(D|P)         */
    { OP(SRC,DST,GXequiv), OP(PAT,DST,GXnor), },     /* 0x06  ~(P|~(D^S))    */
    { OP(SRC,DST,GXand), OP(PAT,DST,GXnor) },        /* 0x07  ~(P|(D&S))     */
    { OP(PAT,DST,GXandInverted), OP(SRC,DST,GXand) },/* 0x08  S&D&~P         */
    { OP(SRC,DST,GXxor), OP(PAT,DST,GXnor) },        /* 0x09  ~(P|(D^S))     */
    { OP(PAT,DST,GXandInverted) },                   /* 0x0a  D&~P           */
    { OP(SRC,DST,GXandReverse), OP(PAT,DST,GXnor) }, /* 0x0b  ~(P|(S&~D))    */
    { OP(PAT,SRC,GXandInverted) },                   /* 0x0c  S&~P           */
    { OP(SRC,DST,GXandInverted), OP(PAT,DST,GXnor) },/* 0x0d  ~(P|(D&~S))    */
    { OP(SRC,DST,GXnor), OP(PAT,DST,GXnor) },        /* 0x0e  ~(P|~(D|S))    */
    { OP(PAT,DST,GXcopyInverted) },                  /* 0x0f  ~P             */
    { OP(SRC,DST,GXnor), OP(PAT,DST,GXand) },        /* 0x10  P&~(S|D)       */
    { OP(SRC,DST,GXnor) },                           /* 0x11  ~(D|S)         */
    { OP(PAT,DST,GXequiv), OP(SRC,DST,GXnor) },      /* 0x12  ~(S|~(D^P))    */
    { OP(PAT,DST,GXand), OP(SRC,DST,GXnor) },        /* 0x13  ~(S|(D&P))     */
    { OP(PAT,SRC,GXequiv), OP(SRC,DST,GXnor) },      /* 0x14  ~(D|~(P^S))    */
    { OP(PAT,SRC,GXand), OP(SRC,DST,GXnor) },        /* 0x15  ~(D|(P&S))     */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXnand),
      OP(SRC,DST,GXand), OP(TMP,DST,GXxor),
      OP(PAT,DST,GXxor) },                           /* 0x16  P^S^(D&~(P&S)  */
    { OP(SRC,TMP,GXcopy), OP(SRC,DST,GXxor),
      OP(PAT,SRC,GXxor), OP(SRC,DST,GXand),
      OP(TMP,DST,GXequiv) },                         /* 0x17 ~S^((S^P)&(S^D))*/
    { OP(PAT,SRC,GXxor), OP(PAT,DST,GXxor),
        OP(SRC,DST,GXand) },                         /* 0x18  (S^P)&(D^P)    */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXnand),
      OP(SRC,DST,GXand), OP(TMP,DST,GXequiv) },      /* 0x19  ~S^(D&~(P&S))  */
    { OP(PAT,SRC,GXand), OP(SRC,DST,GXor),
      OP(PAT,DST,GXxor) },                           /* 0x1a  P^(D|(S&P))    */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXxor),
      OP(SRC,DST,GXand), OP(TMP,DST,GXequiv) },      /* 0x1b  ~S^(D&(P^S))   */
    { OP(PAT,DST,GXand), OP(SRC,DST,GXor),
      OP(PAT,DST,GXxor) },                           /* 0x1c  P^(S|(D&P))    */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXxor),
      OP(SRC,DST,GXand), OP(TMP,DST,GXequiv) },      /* 0x1d  ~D^(S&(D^P))   */
    { OP(SRC,DST,GXor), OP(PAT,DST,GXxor) },         /* 0x1e  P^(D|S)        */
    { OP(SRC,DST,GXor), OP(PAT,DST,GXnand) },        /* 0x1f  ~(P&(D|S))     */
    { OP(PAT,SRC,GXandReverse), OP(SRC,DST,GXand) }, /* 0x20  D&(P&~S)       */
    { OP(PAT,DST,GXxor), OP(SRC,DST,GXnor) },        /* 0x21  ~(S|(D^P))     */
    { OP(SRC,DST,GXandInverted) },                   /* 0x22  ~S&D           */
    { OP(PAT,DST,GXandReverse), OP(SRC,DST,GXnor) }, /* 0x23  ~(S|(P&~D))    */
    { OP(SRC,DST,GXxor), OP(PAT,SRC,GXxor),
      OP(SRC,DST,GXand) },                           /* 0x24   (S^P)&(S^D)   */
    { OP(PAT,SRC,GXnand), OP(SRC,DST,GXand),
      OP(PAT,DST,GXequiv) },                         /* 0x25  ~P^(D&~(S&P))  */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXand),
      OP(SRC,DST,GXor), OP(TMP,DST,GXxor) },         /* 0x26  S^(D|(S&P))    */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXequiv),
      OP(SRC,DST,GXor), OP(TMP,DST,GXxor) },         /* 0x27  S^(D|~(P^S))   */
    { OP(PAT,SRC,GXxor), OP(SRC,DST,GXand) },        /* 0x28  D&(P^S)        */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXand),
      OP(SRC,DST,GXor), OP(TMP,DST,GXxor),
      OP(PAT,DST,GXequiv) },                         /* 0x29  ~P^S^(D|(P&S)) */
    { OP(PAT,SRC,GXnand), OP(SRC,DST,GXand) },       /* 0x2a  D&~(P&S)       */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXxor),
      OP(PAT,DST,GXxor), OP(SRC,DST,GXand),
      OP(TMP,DST,GXequiv) },                         /* 0x2b ~S^((P^S)&(P^D))*/
    { OP(SRC,DST,GXor), OP(PAT,DST,GXand),
      OP(SRC,DST,GXxor) },                           /* 0x2c  S^(P&(S|D))    */
    { OP(SRC,DST,GXorReverse), OP(PAT,DST,GXxor) },  /* 0x2d  P^(S|~D)       */
    { OP(PAT,DST,GXxor), OP(SRC,DST,GXor),
      OP(PAT,DST,GXxor) },                           /* 0x2e  P^(S|(D^P))    */
    { OP(SRC,DST,GXorReverse), OP(PAT,DST,GXnand) }, /* 0x2f  ~(P&(S|~D))    */
    { OP(PAT,SRC,GXandReverse) },                    /* 0x30  P&~S           */
    { OP(PAT,DST,GXandInverted), OP(SRC,DST,GXnor) },/* 0x31  ~(S|(D&~P))    */
    { OP(SRC,DST,GXor), OP(PAT,DST,GXor),
      OP(SRC,DST,GXxor) },                           /* 0x32  S^(D|P|S)      */
    { OP(SRC,DST,GXcopyInverted) },                  /* 0x33  ~S             */
    { OP(SRC,DST,GXand), OP(PAT,DST,GXor),
      OP(SRC,DST,GXxor) },                           /* 0x34  S^(P|(D&S))    */
    { OP(SRC,DST,GXequiv), OP(PAT,DST,GXor),
      OP(SRC,DST,GXxor) },                           /* 0x35  S^(P|~(D^S))   */
    { OP(PAT,DST,GXor), OP(SRC,DST,GXxor) },         /* 0x36  S^(D|P)        */
    { OP(PAT,DST,GXor), OP(SRC,DST,GXnand) },        /* 0x37  ~(S&(D|P))     */
    { OP(PAT,DST,GXor), OP(SRC,DST,GXand),
      OP(PAT,DST,GXxor) },                           /* 0x38  P^(S&(D|P))    */
    { OP(PAT,DST,GXorReverse), OP(SRC,DST,GXxor) },  /* 0x39  S^(P|~D)       */
    { OP(SRC,DST,GXxor), OP(PAT,DST,GXor),
      OP(SRC,DST,GXxor) },                           /* 0x3a  S^(P|(D^S))    */
    { OP(PAT,DST,GXorReverse), OP(SRC,DST,GXnand) }, /* 0x3b  ~(S&(P|~D))    */
    { OP(PAT,SRC,GXxor) },                           /* 0x3c  P^S            */
    { OP(SRC,DST,GXnor), OP(PAT,DST,GXor),
      OP(SRC,DST,GXxor) },                           /* 0x3d  S^(P|~(D|S))   */
    { OP(SRC,DST,GXandInverted), OP(PAT,DST,GXor),
      OP(SRC,DST,GXxor) },                           /* 0x3e  S^(P|(D&~S))   */
    { OP(PAT,SRC,GXnand) },                          /* 0x3f  ~(P&S)         */
    { OP(SRC,DST,GXandReverse), OP(PAT,DST,GXand) }, /* 0x40  P&S&~D         */
    { OP(PAT,SRC,GXxor), OP(SRC,DST,GXnor) },        /* 0x41  ~(D|(P^S))     */
    { OP(DST,SRC,GXxor), OP(PAT,DST,GXxor),
      OP(SRC,DST,GXand) },                           /* 0x42  (S^D)&(P^D)    */
    { OP(SRC,DST,GXnand), OP(PAT,DST,GXand),
      OP(SRC,DST,GXequiv) },                         /* 0x43  ~S^(P&~(D&S))  */
    { OP(SRC,DST,GXandReverse) },                    /* 0x44  S&~D           */
    { OP(PAT,SRC,GXandReverse), OP(SRC,DST,GXnor) }, /* 0x45  ~(D|(P&~S))    */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXand),
      OP(SRC,DST,GXor), OP(TMP,DST,GXxor) },         /* 0x46  D^(S|(P&D))    */
    { OP(PAT,DST,GXxor), OP(SRC,DST,GXand),
      OP(PAT,DST,GXequiv) },                         /* 0x47  ~P^(S&(D^P))   */
    { OP(PAT,DST,GXxor), OP(SRC,DST,GXand) },        /* 0x48  S&(P^D)        */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXand),
      OP(SRC,DST,GXor), OP(TMP,DST,GXxor),
      OP(PAT,DST,GXequiv) },                         /* 0x49  ~P^D^(S|(P&D)) */
    { OP(DST,SRC,GXor), OP(PAT,SRC,GXand),
      OP(SRC,DST,GXxor) },                           /* 0x4a  D^(P&(S|D))    */
    { OP(SRC,DST,GXorInverted), OP(PAT,DST,GXxor) }, /* 0x4b  P^(D|~S)       */
    { OP(PAT,DST,GXnand), OP(SRC,DST,GXand) },       /* 0x4c  S&~(D&P)       */
    { OP(SRC,TMP,GXcopy), OP(SRC,DST,GXxor),
      OP(PAT,SRC,GXxor), OP(SRC,DST,GXor),
      OP(TMP,DST,GXequiv) },                         /* 0x4d ~S^((S^P)|(S^D))*/
    { OP(PAT,SRC,GXxor), OP(SRC,DST,GXor),
      OP(PAT,DST,GXxor) },                           /* 0x4e  P^(D|(S^P))    */
    { OP(SRC,DST,GXorInverted), OP(PAT,DST,GXnand) },/* 0x4f  ~(P&(D|~S))    */
    { OP(PAT,DST,GXandReverse) },                    /* 0x50  P&~D           */
    { OP(PAT,SRC,GXandInverted), OP(SRC,DST,GXnor) },/* 0x51  ~(D|(S&~P))    */
    { OP(DST,SRC,GXand), OP(PAT,SRC,GXor),
      OP(SRC,DST,GXxor) },                           /* 0x52  D^(P|(S&D))    */
    { OP(SRC,DST,GXxor), OP(PAT,DST,GXand),
      OP(SRC,DST,GXequiv) },                         /* 0x53  ~S^(P&(D^S))   */
    { OP(PAT,SRC,GXnor), OP(SRC,DST,GXnor) },        /* 0x54  ~(D|~(P|S))    */
    { OP(PAT,DST,GXinvert) },                        /* 0x55  ~D             */
    { OP(PAT,SRC,GXor), OP(SRC,DST,GXxor) },         /* 0x56  D^(P|S)        */
    { OP(PAT,SRC,GXor), OP(SRC,DST,GXnand) },        /* 0x57  ~(D&(P|S))     */
    { OP(PAT,SRC,GXor), OP(SRC,DST,GXand),
      OP(PAT,DST,GXxor) },                           /* 0x58  P^(D&(P|S))    */
    { OP(PAT,SRC,GXorReverse), OP(SRC,DST,GXxor) },  /* 0x59  D^(P|~S)       */
    { OP(PAT,DST,GXxor) },                           /* 0x5a  D^P            */
    { OP(DST,SRC,GXnor), OP(PAT,SRC,GXor),
      OP(SRC,DST,GXxor) },                           /* 0x5b  D^(P|~(S|D))   */
    { OP(DST,SRC,GXxor), OP(PAT,SRC,GXor),
      OP(SRC,DST,GXxor) },                           /* 0x5c  D^(P|(S^D))    */
    { OP(PAT,SRC,GXorReverse), OP(SRC,DST,GXnand) }, /* 0x5d  ~(D&(P|~S))    */
    { OP(DST,SRC,GXandInverted), OP(PAT,SRC,GXor),
      OP(SRC,DST,GXxor) },                           /* 0x5e  D^(P|(S&~D))   */
    { OP(PAT,DST,GXnand) },                          /* 0x5f  ~(D&P)         */
    { OP(SRC,DST,GXxor), OP(PAT,DST,GXand) },        /* 0x60  P&(D^S)        */
    { OP(DST,TMP,GXcopy), OP(SRC,DST,GXand),
      OP(PAT,DST,GXor), OP(SRC,DST,GXxor),
      OP(TMP,DST,GXequiv) },                         /* 0x61  ~D^S^(P|(D&S)) */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXor),
      OP(SRC,DST,GXand), OP(TMP,DST,GXxor) },        /* 0x62  D^(S&(P|D))    */
    { OP(PAT,DST,GXorInverted), OP(SRC,DST,GXxor) }, /* 0x63  S^(D|~P)       */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXor),
      OP(SRC,DST,GXand), OP(TMP,DST,GXxor) },        /* 0x64  S^(D&(P|S))    */
    { OP(PAT,SRC,GXorInverted), OP(SRC,DST,GXxor) }, /* 0x65  D^(S|~P)       */
    { OP(SRC,DST,GXxor) },                           /* 0x66  S^D            */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXnor),
      OP(SRC,DST,GXor), OP(TMP,DST,GXxor) },         /* 0x67  S^(D|~(S|P)    */
    { OP(DST,TMP,GXcopy), OP(SRC,DST,GXnor),
      OP(PAT,DST,GXor), OP(SRC,DST,GXxor),
      OP(TMP,DST,GXequiv) },                         /* 0x68  ~D^S^(P|~(D|S))*/
    { OP(SRC,DST,GXxor), OP(PAT,DST,GXequiv) },      /* 0x69  ~P^(D^S)       */
    { OP(PAT,SRC,GXand), OP(SRC,DST,GXxor) },        /* 0x6a  D^(P&S)        */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXor),
      OP(SRC,DST,GXand), OP(TMP,DST,GXxor),
      OP(PAT,DST,GXequiv) },                         /* 0x6b  ~P^S^(D&(P|S)) */
    { OP(PAT,DST,GXand), OP(SRC,DST,GXxor) },        /* 0x6c  S^(D&P)        */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXor),
      OP(SRC,DST,GXand), OP(TMP,DST,GXxor),
      OP(PAT,DST,GXequiv) },                         /* 0x6d  ~P^D^(S&(P|D)) */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXorReverse),
      OP(SRC,DST,GXand), OP(TMP,DST,GXxor) },        /* 0x6e  S^(D&(P|~S))   */
    { OP(SRC,DST,GXequiv), OP(PAT,DST,GXnand) },     /* 0x6f  ~(P&~(S^D))    */
    { OP(SRC,DST,GXnand), OP(PAT,DST,GXand) },       /* 0x70  P&~(D&S)       */
    { OP(SRC,TMP,GXcopy), OP(DST,SRC,GXxor),
      OP(PAT,DST,GXxor), OP(SRC,DST,GXand),
      OP(TMP,DST,GXequiv) },                         /* 0x71 ~S^((S^D)&(P^D))*/
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXxor),
      OP(SRC,DST,GXor), OP(TMP,DST,GXxor) },         /* 0x72  S^(D|(P^S))    */
    { OP(PAT,DST,GXorInverted), OP(SRC,DST,GXnand) },/* 0x73  ~(S&(D|~P))    */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXxor),
      OP(SRC,DST,GXor), OP(TMP,DST,GXxor) },         /* 0x74   D^(S|(P^D))   */
    { OP(PAT,SRC,GXorInverted), OP(SRC,DST,GXnand) },/* 0x75  ~(D&(S|~P))    */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXandReverse),
      OP(SRC,DST,GXor), OP(TMP,DST,GXxor) },         /* 0x76  S^(D|(P&~S))   */
    { OP(SRC,DST,GXnand) },                          /* 0x77  ~(S&D)         */
    { OP(SRC,DST,GXand), OP(PAT,DST,GXxor) },        /* 0x78  P^(D&S)        */
    { OP(DST,TMP,GXcopy), OP(SRC,DST,GXor),
      OP(PAT,DST,GXand), OP(SRC,DST,GXxor),
      OP(TMP,DST,GXequiv) },                         /* 0x79  ~D^S^(P&(D|S)) */
    { OP(DST,SRC,GXorInverted), OP(PAT,SRC,GXand),
      OP(SRC,DST,GXxor) },                           /* 0x7a  D^(P&(S|~D))   */
    { OP(PAT,DST,GXequiv), OP(SRC,DST,GXnand) },     /* 0x7b  ~(S&~(D^P))    */
    { OP(SRC,DST,GXorInverted), OP(PAT,DST,GXand),
      OP(SRC,DST,GXxor) },                           /* 0x7c  S^(P&(D|~S))   */
    { OP(PAT,SRC,GXequiv), OP(SRC,DST,GXnand) },     /* 0x7d  ~(D&~(P^S))    */
    { OP(SRC,DST,GXxor), OP(PAT,SRC,GXxor),
      OP(SRC,DST,GXor) },                            /* 0x7e  (S^P)|(S^D)    */
    { OP(PAT,SRC,GXand), OP(SRC,DST,GXnand) },       /* 0x7f  ~(D&P&S)       */
    { OP(PAT,SRC,GXand), OP(SRC,DST,GXand) },        /* 0x80  D&P&S          */
    { OP(SRC,DST,GXxor), OP(PAT,SRC,GXxor),
      OP(SRC,DST,GXnor) },                           /* 0x81  ~((S^P)|(S^D)) */
    { OP(PAT,SRC,GXequiv), OP(SRC,DST,GXand) },      /* 0x82  D&~(P^S)       */
    { OP(SRC,DST,GXorInverted), OP(PAT,DST,GXand),
      OP(SRC,DST,GXequiv) },                         /* 0x83  ~S^(P&(D|~S))  */
    { OP(PAT,DST,GXequiv), OP(SRC,DST,GXand) },      /* 0x84  S&~(D^P)       */
    { OP(PAT,SRC,GXorInverted), OP(SRC,DST,GXand),
      OP(PAT,DST,GXequiv) },                         /* 0x85  ~P^(D&(S|~P))  */
    { OP(DST,TMP,GXcopy), OP(SRC,DST,GXor),
      OP(PAT,DST,GXand), OP(SRC,DST,GXxor),
      OP(TMP,DST,GXxor) },                           /* 0x86  D^S^(P&(D|S))  */
    { OP(SRC,DST,GXand), OP(PAT,DST,GXequiv) },      /* 0x87  ~P^(D&S)       */
    { OP(SRC,DST,GXand) },                           /* 0x88  S&D            */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXandReverse),
      OP(SRC,DST,GXor), OP(TMP,DST,GXequiv) },       /* 0x89  ~S^(D|(P&~S))  */
    { OP(PAT,SRC,GXorInverted), OP(SRC,DST,GXand) }, /* 0x8a  D&(S|~P)       */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXxor),
      OP(SRC,DST,GXor), OP(TMP,DST,GXequiv) },       /* 0x8b  ~D^(S|(P^D))   */
    { OP(PAT,DST,GXorInverted), OP(SRC,DST,GXand) }, /* 0x8c  S&(D|~P)       */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXxor),
      OP(SRC,DST,GXor), OP(TMP,DST,GXequiv) },       /* 0x8d  ~S^(D|(P^S))   */
    { OP(SRC,TMP,GXcopy), OP(DST,SRC,GXxor),
      OP(PAT,DST,GXxor), OP(SRC,DST,GXand),
      OP(TMP,DST,GXxor) },                           /* 0x8e  S^((S^D)&(P^D))*/
    { OP(SRC,DST,GXnand), OP(PAT,DST,GXnand) },      /* 0x8f  ~(P&~(D&S))    */
    { OP(SRC,DST,GXequiv), OP(PAT,DST,GXand) },      /* 0x90  P&~(D^S)       */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXorReverse),
      OP(SRC,DST,GXand), OP(TMP,DST,GXequiv) },      /* 0x91  ~S^(D&(P|~S))  */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXor),
      OP(SRC,DST,GXand), OP(PAT,DST,GXxor),
      OP(TMP,DST,GXxor) },                           /* 0x92  D^P^(S&(D|P))  */
    { OP(PAT,DST,GXand), OP(SRC,DST,GXequiv) },      /* 0x93  ~S^(P&D)       */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXor),
      OP(SRC,DST,GXand), OP(PAT,DST,GXxor),
      OP(TMP,DST,GXxor) },                           /* 0x94  S^P^(D&(P|S))  */
    { OP(PAT,SRC,GXand), OP(SRC,DST,GXequiv) },      /* 0x95  ~D^(P&S)       */
    { OP(PAT,SRC,GXxor), OP(SRC,DST,GXxor) },        /* 0x96  D^P^S          */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXnor),
      OP(SRC,DST,GXor), OP(PAT,DST,GXxor),
      OP(TMP,DST,GXxor) },                           /* 0x97  S^P^(D|~(P|S)) */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXnor),
      OP(SRC,DST,GXor), OP(TMP,DST,GXequiv) },       /* 0x98  ~S^(D|~(P|S))  */
    { OP(SRC,DST,GXequiv) },                         /* 0x99  ~S^D           */
    { OP(PAT,SRC,GXandReverse), OP(SRC,DST,GXxor) }, /* 0x9a  D^(P&~S)       */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXor),
      OP(SRC,DST,GXand), OP(TMP,DST,GXequiv) },      /* 0x9b  ~S^(D&(P|S))   */
    { OP(PAT,DST,GXandReverse), OP(SRC,DST,GXxor) }, /* 0x9c  S^(P&~D)       */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXor),
      OP(SRC,DST,GXand), OP(TMP,DST,GXequiv) },      /* 0x9d  ~D^(S&(P|D))   */
    { OP(DST,TMP,GXcopy), OP(SRC,DST,GXand),
      OP(PAT,DST,GXor), OP(SRC,DST,GXxor),
      OP(TMP,DST,GXxor) },                           /* 0x9e  D^S^(P|(D&S))  */
    { OP(SRC,DST,GXxor), OP(PAT,DST,GXnand) },       /* 0x9f  ~(P&(D^S))     */
    { OP(PAT,DST,GXand) },                           /* 0xa0  D&P            */
    { OP(PAT,SRC,GXandInverted), OP(SRC,DST,GXor),
      OP(PAT,DST,GXequiv) },                         /* 0xa1  ~P^(D|(S&~P))  */
    { OP(PAT,SRC,GXorReverse), OP(SRC,DST,GXand) },  /* 0xa2  D&(P|~S)       */
    { OP(DST,SRC,GXxor), OP(PAT,SRC,GXor),
      OP(SRC,DST,GXequiv) },                         /* 0xa3  ~D^(P|(S^D))   */
    { OP(PAT,SRC,GXnor), OP(SRC,DST,GXor),
      OP(PAT,DST,GXequiv) },                         /* 0xa4  ~P^(D|~(S|P))  */
    { OP(PAT,DST,GXequiv) },                         /* 0xa5  ~P^D           */
    { OP(PAT,SRC,GXandInverted), OP(SRC,DST,GXxor) },/* 0xa6  D^(S&~P)       */
    { OP(PAT,SRC,GXor), OP(SRC,DST,GXand),
      OP(PAT,DST,GXequiv) },                         /* 0xa7  ~P^(D&(S|P))   */
    { OP(PAT,SRC,GXor), OP(SRC,DST,GXand) },         /* 0xa8  D&(P|S)        */
    { OP(PAT,SRC,GXor), OP(SRC,DST,GXequiv) },       /* 0xa9  ~D^(P|S)       */
    { OP(PAT,DST,GXnoop) },                          /* 0xaa  D              */
    { OP(PAT,SRC,GXnor), OP(SRC,DST,GXor) },         /* 0xab  D|~(P|S)       */
    { OP(SRC,DST,GXxor), OP(PAT,DST,GXand),
      OP(SRC,DST,GXxor) },                           /* 0xac  S^(P&(D^S))    */
    { OP(DST,SRC,GXand), OP(PAT,SRC,GXor),
      OP(SRC,DST,GXequiv) },                         /* 0xad  ~D^(P|(S&D))   */
    { OP(PAT,SRC,GXandInverted), OP(SRC,DST,GXor) }, /* 0xae  D|(S&~P)       */
    { OP(PAT,DST,GXorInverted) },                    /* 0xaf  D|~P           */
    { OP(SRC,DST,GXorInverted), OP(PAT,DST,GXand) }, /* 0xb0  P&(D|~S)       */
    { OP(PAT,SRC,GXxor), OP(SRC,DST,GXor),
      OP(PAT,DST,GXequiv) },                         /* 0xb1  ~P^(D|(S^P))   */
    { OP(SRC,TMP,GXcopy), OP(SRC,DST,GXxor),
      OP(PAT,SRC,GXxor), OP(SRC,DST,GXor),
      OP(TMP,DST,GXxor) },                           /* 0xb2  S^((S^P)|(S^D))*/
    { OP(PAT,DST,GXnand), OP(SRC,DST,GXnand) },      /* 0xb3  ~(S&~(D&P))    */
    { OP(SRC,DST,GXandReverse), OP(PAT,DST,GXxor) }, /* 0xb4  P^(S&~D)       */
    { OP(DST,SRC,GXor), OP(PAT,SRC,GXand),
      OP(SRC,DST,GXequiv) },                         /* 0xb5  ~D^(P&(S|D))   */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXand),
      OP(SRC,DST,GXor), OP(PAT,DST,GXxor),
      OP(TMP,DST,GXxor) },                           /* 0xb6  D^P^(S|(D&P))  */
    { OP(PAT,DST,GXxor), OP(SRC,DST,GXnand) },       /* 0xb7  ~(S&(D^P))     */
    { OP(PAT,DST,GXxor), OP(SRC,DST,GXand),
      OP(PAT,DST,GXxor) },                           /* 0xb8  P^(S&(D^P))    */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXand),
      OP(SRC,DST,GXor), OP(TMP,DST,GXequiv) },       /* 0xb9  ~D^(S|(P&D))   */
    { OP(PAT,SRC,GXandReverse), OP(SRC,DST,GXor) },  /* 0xba  D|(P&~S)       */
    { OP(SRC,DST,GXorInverted) },                    /* 0xbb  ~S|D           */
    { OP(SRC,DST,GXnand), OP(PAT,DST,GXand),
      OP(SRC,DST,GXxor) },                           /* 0xbc  S^(P&~(D&S))   */
    { OP(DST,SRC,GXxor), OP(PAT,DST,GXxor),
      OP(SRC,DST,GXnand) },                          /* 0xbd  ~((S^D)&(P^D)) */
    { OP(PAT,SRC,GXxor), OP(SRC,DST,GXor) },         /* 0xbe  D|(P^S)        */
    { OP(PAT,SRC,GXnand), OP(SRC,DST,GXor) },        /* 0xbf  D|~(P&S)       */
    { OP(PAT,SRC,GXand) },                           /* 0xc0  P&S            */
    { OP(SRC,DST,GXandInverted), OP(PAT,DST,GXor),
      OP(SRC,DST,GXequiv) },                         /* 0xc1  ~S^(P|(D&~S))  */
    { OP(SRC,DST,GXnor), OP(PAT,DST,GXor),
      OP(SRC,DST,GXequiv) },                         /* 0xc2  ~S^(P|~(D|S))  */
    { OP(PAT,SRC,GXequiv) },                         /* 0xc3  ~P^S           */
    { OP(PAT,DST,GXorReverse), OP(SRC,DST,GXand) },  /* 0xc4  S&(P|~D)       */
    { OP(SRC,DST,GXxor), OP(PAT,DST,GXor),
      OP(SRC,DST,GXequiv) },                         /* 0xc5  ~S^(P|(D^S))   */
    { OP(PAT,DST,GXandInverted), OP(SRC,DST,GXxor) },/* 0xc6  S^(D&~P)       */
    { OP(PAT,DST,GXor), OP(SRC,DST,GXand),
      OP(PAT,DST,GXequiv) },                         /* 0xc7  ~P^(S&(D|P))   */
    { OP(PAT,DST,GXor), OP(SRC,DST,GXand) },         /* 0xc8  S&(D|P)        */
    { OP(PAT,DST,GXor), OP(SRC,DST,GXequiv) },       /* 0xc9  ~S^(P|D)       */
    { OP(DST,SRC,GXxor), OP(PAT,SRC,GXand),
      OP(SRC,DST,GXxor) },                           /* 0xca  D^(P&(S^D))    */
    { OP(SRC,DST,GXand), OP(PAT,DST,GXor),
      OP(SRC,DST,GXequiv) },                         /* 0xcb  ~S^(P|(D&S))   */
    { OP(SRC,DST,GXcopy) },                          /* 0xcc  S              */
    { OP(PAT,DST,GXnor), OP(SRC,DST,GXor) },         /* 0xcd  S|~(D|P)       */
    { OP(PAT,DST,GXandInverted), OP(SRC,DST,GXor) }, /* 0xce  S|(D&~P)       */
    { OP(PAT,SRC,GXorInverted) },                    /* 0xcf  S|~P           */
    { OP(SRC,DST,GXorReverse), OP(PAT,DST,GXand) },  /* 0xd0  P&(S|~D)       */
    { OP(PAT,DST,GXxor), OP(SRC,DST,GXor),
      OP(PAT,DST,GXequiv) },                         /* 0xd1  ~P^(S|(D^P))   */
    { OP(SRC,DST,GXandInverted), OP(PAT,DST,GXxor) },/* 0xd2  P^(D&~S)       */
    { OP(SRC,DST,GXor), OP(PAT,DST,GXand),
      OP(SRC,DST,GXequiv) },                         /* 0xd3  ~S^(P&(D|S))   */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXxor),
      OP(PAT,DST,GXxor), OP(SRC,DST,GXand),
      OP(TMP,DST,GXxor) },                           /* 0xd4  S^((S^P)&(D^P))*/
    { OP(PAT,SRC,GXnand), OP(SRC,DST,GXnand) },      /* 0xd5  ~(D&~(P&S))    */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXand),
      OP(SRC,DST,GXor), OP(PAT,DST,GXxor),
      OP(TMP,DST,GXxor) },                           /* 0xd6  S^P^(D|(P&S))  */
    { OP(PAT,SRC,GXxor), OP(SRC,DST,GXnand) },       /* 0xd7  ~(D&(P^S))     */
    { OP(PAT,SRC,GXxor), OP(SRC,DST,GXand),
      OP(PAT,DST,GXxor) },                           /* 0xd8  P^(D&(S^P))    */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXand),
      OP(SRC,DST,GXor), OP(TMP,DST,GXequiv) },       /* 0xd9  ~S^(D|(P&S))   */
    { OP(DST,SRC,GXnand), OP(PAT,SRC,GXand),
      OP(SRC,DST,GXxor) },                           /* 0xda  D^(P&~(S&D))   */
    { OP(SRC,DST,GXxor), OP(PAT,SRC,GXxor),
      OP(SRC,DST,GXnand) },                          /* 0xdb  ~((S^P)&(S^D)) */
    { OP(PAT,DST,GXandReverse), OP(SRC,DST,GXor) },  /* 0xdc  S|(P&~D)       */
    { OP(SRC,DST,GXorReverse) },                     /* 0xdd  S|~D           */
    { OP(PAT,DST,GXxor), OP(SRC,DST,GXor) },         /* 0xde  S|(D^P)        */
    { OP(PAT,DST,GXnand), OP(SRC,DST,GXor) },        /* 0xdf  S|~(D&P)       */
    { OP(SRC,DST,GXor), OP(PAT,DST,GXand) },         /* 0xe0  P&(D|S)        */
    { OP(SRC,DST,GXor), OP(PAT,DST,GXequiv) },       /* 0xe1  ~P^(D|S)       */
    { OP(DST,TMP,GXcopy), OP(PAT,DST,GXxor),
      OP(SRC,DST,GXand), OP(TMP,DST,GXxor) },        /* 0xe2  D^(S&(P^D))    */
    { OP(PAT,DST,GXand), OP(SRC,DST,GXor),
      OP(PAT,DST,GXequiv) },                         /* 0xe3  ~P^(S|(D&P))   */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXxor),
      OP(SRC,DST,GXand), OP(TMP,DST,GXxor) },        /* 0xe4  S^(D&(P^S))    */
    { OP(PAT,SRC,GXand), OP(SRC,DST,GXor),
      OP(PAT,DST,GXequiv) },                         /* 0xe5  ~P^(D|(S&P))   */
    { OP(SRC,TMP,GXcopy), OP(PAT,SRC,GXnand),
      OP(SRC,DST,GXand), OP(TMP,DST,GXxor) },        /* 0xe6  S^(D&~(P&S))   */
    { OP(PAT,SRC,GXxor), OP(PAT,DST,GXxor),
      OP(SRC,DST,GXnand) },                          /* 0xe7  ~((S^P)&(D^P)) */
    { OP(SRC,TMP,GXcopy), OP(SRC,DST,GXxor),
      OP(PAT,SRC,GXxor), OP(SRC,DST,GXand),
      OP(TMP,DST,GXxor) },                           /* 0xe8  S^((S^P)&(S^D))*/
    { OP(DST,TMP,GXcopy), OP(SRC,DST,GXnand),
      OP(PAT,DST,GXand), OP(SRC,DST,GXxor),
      OP(TMP,DST,GXequiv) },                         /* 0xe9  ~D^S^(P&~(S&D))*/
    { OP(PAT,SRC,GXand), OP(SRC,DST,GXor) },         /* 0xea  D|(P&S)        */
    { OP(PAT,SRC,GXequiv), OP(SRC,DST,GXor) },       /* 0xeb  D|~(P^S)       */
    { OP(PAT,DST,GXand), OP(SRC,DST,GXor) },         /* 0xec  S|(D&P)        */
    { OP(PAT,DST,GXequiv), OP(SRC,DST,GXor) },       /* 0xed  S|~(D^P)       */
    { OP(SRC,DST,GXor) },                            /* 0xee  S|D            */
    { OP(PAT,DST,GXorInverted), OP(SRC,DST,GXor) },  /* 0xef  S|D|~P         */
    { OP(PAT,DST,GXcopy) },                          /* 0xf0  P              */
    { OP(SRC,DST,GXnor), OP(PAT,DST,GXor) },         /* 0xf1  P|~(D|S)       */
    { OP(SRC,DST,GXandInverted), OP(PAT,DST,GXor) }, /* 0xf2  P|(D&~S)       */
    { OP(PAT,SRC,GXorReverse) },                     /* 0xf3  P|~S           */
    { OP(SRC,DST,GXandReverse), OP(PAT,DST,GXor) },  /* 0xf4  P|(S&~D)       */
    { OP(PAT,DST,GXorReverse) },                     /* 0xf5  P|~D           */
    { OP(SRC,DST,GXxor), OP(PAT,DST,GXor) },         /* 0xf6  P|(D^S)        */
    { OP(SRC,DST,GXnand), OP(PAT,DST,GXor) },        /* 0xf7  P|~(S&D)       */
    { OP(SRC,DST,GXand), OP(PAT,DST,GXor) },         /* 0xf8  P|(D&S)        */
    { OP(SRC,DST,GXequiv), OP(PAT,DST,GXor) },       /* 0xf9  P|~(D^S)       */
    { OP(PAT,DST,GXor) },                            /* 0xfa  D|P            */
    { OP(PAT,SRC,GXorReverse), OP(SRC,DST,GXor) },   /* 0xfb  D|P|~S         */
    { OP(PAT,SRC,GXor) },                            /* 0xfc  P|S            */
    { OP(SRC,DST,GXorReverse), OP(PAT,DST,GXor) },   /* 0xfd  P|S|~D         */
    { OP(SRC,DST,GXor), OP(PAT,DST,GXor) },          /* 0xfe  P|D|S          */
    { OP(PAT,DST,GXset) }                            /* 0xff  1              */
};

static const unsigned char bit_swap[256] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

#ifdef WORDS_BIGENDIAN
static const unsigned int zeropad_masks[32] =
{
    0xffffffff, 0x80000000, 0xc0000000, 0xe0000000, 0xf0000000, 0xf8000000, 0xfc000000, 0xfe000000,
    0xff000000, 0xff800000, 0xffc00000, 0xffe00000, 0xfff00000, 0xfff80000, 0xfffc0000, 0xfffe0000,
    0xffff0000, 0xffff8000, 0xffffc000, 0xffffe000, 0xfffff000, 0xfffff800, 0xfffffc00, 0xfffffe00,
    0xffffff00, 0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0, 0xfffffff8, 0xfffffffc, 0xfffffffe
};
#else
static const unsigned int zeropad_masks[32] =
{
    0xffffffff, 0x00000080, 0x000000c0, 0x000000e0, 0x000000f0, 0x000000f8, 0x000000fc, 0x000000fe,
    0x000000ff, 0x000080ff, 0x0000c0ff, 0x0000e0ff, 0x0000f0ff, 0x0000f8ff, 0x0000fcff, 0x0000feff,
    0x0000ffff, 0x0080ffff, 0x00c0ffff, 0x00e0ffff, 0x00f0ffff, 0x00f8ffff, 0x00fcffff, 0x00feffff,
    0x00ffffff, 0x80ffffff, 0xc0ffffff, 0xe0ffffff, 0xf0ffffff, 0xf8ffffff, 0xfcffffff, 0xfeffffff
};
#endif

#ifdef BITBLT_TEST  /* Opcodes test */

static int do_bitop( int s, int d, int rop )
{
    int res;
    switch(rop)
    {
    case GXclear:        res = 0; break;
    case GXand:          res = s & d; break;
    case GXandReverse:   res = s & ~d; break;
    case GXcopy:         res = s; break;
    case GXandInverted:  res = ~s & d; break;
    case GXnoop:         res = d; break;
    case GXxor:          res = s ^ d; break;
    case GXor:           res = s | d; break;
    case GXnor:          res = ~(s | d); break;
    case GXequiv:        res = ~s ^ d; break;
    case GXinvert:       res = ~d; break;
    case GXorReverse:    res = s | ~d; break;
    case GXcopyInverted: res = ~s; break;
    case GXorInverted:   res = ~s | d; break;
    case GXnand:         res = ~(s & d); break;
    case GXset:          res = 1; break;
    }
    return res & 1;
}

int main()
{
    int rop, i, res, src, dst, pat, tmp, dstUsed;
    const unsigned char *opcode;

    for (rop = 0; rop < 256; rop++)
    {
        res = dstUsed = 0;
        for (i = 0; i < 8; i++)
        {
            pat = (i >> 2) & 1;
            src = (i >> 1) & 1;
            dst = i & 1;
            for (opcode = BITBLT_Opcodes[rop]; *opcode; opcode++)
            {
                switch(*opcode >> 4)
                {
                case OP_ARGS(DST,TMP):
                    tmp = do_bitop( dst, tmp, *opcode & 0xf );
                    break;
                case OP_ARGS(DST,SRC):
                    src = do_bitop( dst, src, *opcode & 0xf );
                    break;
                case OP_ARGS(SRC,TMP):
                    tmp = do_bitop( src, tmp, *opcode & 0xf );
                    break;
                case OP_ARGS(SRC,DST):
                    dst = do_bitop( src, dst, *opcode & 0xf );
                    dstUsed = 1;
                    break;
                case OP_ARGS(PAT,DST):
                    dst = do_bitop( pat, dst, *opcode & 0xf );
                    dstUsed = 1;
                    break;
                case OP_ARGS(PAT,SRC):
                    src = do_bitop( pat, src, *opcode & 0xf );
                    break;
                case OP_ARGS(TMP,DST):
                    dst = do_bitop( tmp, dst, *opcode & 0xf );
                    dstUsed = 1;
                    break;
                case OP_ARGS(TMP,SRC):
                    src = do_bitop( tmp, src, *opcode & 0xf );
                    break;
                default:
                    printf( "Invalid opcode %x\n", *opcode );
                }
            }
            if (!dstUsed) dst = src;
            if (dst) res |= 1 << i;
        }
        if (res != rop) printf( "%02x: ERROR, res=%02x\n", rop, res );
    }

    return 0;
}

#endif  /* BITBLT_TEST */


/* handler for XGetImage BadMatch errors */
static int XGetImage_handler( Display *dpy, XErrorEvent *event, void *arg )
{
    return (event->request_code == X_GetImage && event->error_code == BadMatch);
}

/***********************************************************************
 *           BITBLT_GetDstArea
 *
 * Retrieve an area from the destination DC, mapping all the
 * pixels to Windows colors.
 */
static int BITBLT_GetDstArea(X11DRV_PDEVICE *physDev, Pixmap pixmap, GC gc, const RECT *visRectDst)
{
    int exposures = 0;
    INT width  = visRectDst->right - visRectDst->left;
    INT height = visRectDst->bottom - visRectDst->top;

    if (!X11DRV_PALETTE_XPixelToPalette || (physDev->depth == 1) ||
	(X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_VIRTUAL) )
    {
        XCopyArea( gdi_display, physDev->drawable, pixmap, gc,
                   physDev->dc_rect.left + visRectDst->left, physDev->dc_rect.top + visRectDst->top,
                   width, height, 0, 0 );
        exposures++;
    }
    else
    {
        INT x, y;
        XImage *image;

        /* Make sure we don't get a BadMatch error */
        XCopyArea( gdi_display, physDev->drawable, pixmap, gc,
                   physDev->dc_rect.left + visRectDst->left,
                   physDev->dc_rect.top + visRectDst->top,
                   width, height, 0, 0);
        exposures++;
        image = XGetImage( gdi_display, pixmap, 0, 0, width, height,
                           AllPlanes, ZPixmap );
        if (image)
        {
            for (y = 0; y < height; y++)
                for (x = 0; x < width; x++)
                    XPutPixel( image, x, y,
                               X11DRV_PALETTE_XPixelToPalette[XGetPixel( image, x, y )]);
            XPutImage( gdi_display, pixmap, gc, image, 0, 0, 0, 0, width, height );
            XDestroyImage( image );
        }
    }
    return exposures;
}


/***********************************************************************
 *           BITBLT_PutDstArea
 *
 * Put an area back into the destination DC, mapping the pixel
 * colors to X pixels.
 */
static int BITBLT_PutDstArea(X11DRV_PDEVICE *physDev, Pixmap pixmap, const RECT *visRectDst)
{
    int exposures = 0;
    INT width  = visRectDst->right - visRectDst->left;
    INT height = visRectDst->bottom - visRectDst->top;

    /* !X11DRV_PALETTE_PaletteToXPixel is _NOT_ enough */

    if (!X11DRV_PALETTE_PaletteToXPixel || (physDev->depth == 1) ||
        (X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_VIRTUAL) )
    {
        XCopyArea( gdi_display, pixmap, physDev->drawable, physDev->gc, 0, 0, width, height,
                   physDev->dc_rect.left + visRectDst->left,
                   physDev->dc_rect.top + visRectDst->top );
        exposures++;
    }
    else
    {
        register INT x, y;
        XImage *image = XGetImage( gdi_display, pixmap, 0, 0, width, height,
                                   AllPlanes, ZPixmap );
        for (y = 0; y < height; y++)
            for (x = 0; x < width; x++)
            {
                XPutPixel( image, x, y,
                           X11DRV_PALETTE_PaletteToXPixel[XGetPixel( image, x, y )]);
            }
        XPutImage( gdi_display, physDev->drawable, physDev->gc, image, 0, 0,
                   physDev->dc_rect.left + visRectDst->left,
                   physDev->dc_rect.top + visRectDst->top, width, height );
        XDestroyImage( image );
    }
    return exposures;
}

static BOOL same_format(X11DRV_PDEVICE *physDevSrc, X11DRV_PDEVICE *physDevDst)
{
    if (physDevSrc->depth != physDevDst->depth) return FALSE;
    if (!physDevSrc->color_shifts && !physDevDst->color_shifts) return TRUE;
    if (physDevSrc->color_shifts && physDevDst->color_shifts)
        return !memcmp(physDevSrc->color_shifts, physDevDst->color_shifts, sizeof(ColorShifts));
    return FALSE;
}

void execute_rop( X11DRV_PDEVICE *physdev, Pixmap src_pixmap, GC gc, const RECT *visrect, DWORD rop )
{
    Pixmap pixmaps[3];
    Pixmap result = src_pixmap;
    BOOL null_brush;
    const BYTE *opcode = BITBLT_Opcodes[(rop >> 16) & 0xff];
    BOOL use_pat = (((rop >> 4) & 0x0f0000) != (rop & 0x0f0000));
    BOOL use_dst = (((rop >> 1) & 0x550000) != (rop & 0x550000));
    int width  = visrect->right - visrect->left;
    int height = visrect->bottom - visrect->top;

    pixmaps[SRC] = src_pixmap;
    pixmaps[TMP] = 0;
    pixmaps[DST] = XCreatePixmap( gdi_display, root_window, width, height, physdev->depth );

    if (use_dst) BITBLT_GetDstArea( physdev, pixmaps[DST], gc, visrect );
    null_brush = use_pat && !X11DRV_SetupGCForPatBlt( physdev, gc, TRUE );

    for ( ; *opcode; opcode++)
    {
        if (OP_DST(*opcode) == DST) result = pixmaps[DST];
        XSetFunction( gdi_display, gc, OP_ROP(*opcode) );
        switch(OP_SRCDST(*opcode))
        {
        case OP_ARGS(DST,TMP):
        case OP_ARGS(SRC,TMP):
            if (!pixmaps[TMP])
                pixmaps[TMP] = XCreatePixmap( gdi_display, root_window, width, height, physdev->depth );
            /* fall through */
        case OP_ARGS(DST,SRC):
        case OP_ARGS(SRC,DST):
        case OP_ARGS(TMP,SRC):
        case OP_ARGS(TMP,DST):
            XCopyArea( gdi_display, pixmaps[OP_SRC(*opcode)], pixmaps[OP_DST(*opcode)], gc,
                       0, 0, width, height, 0, 0 );
            break;
        case OP_ARGS(PAT,DST):
        case OP_ARGS(PAT,SRC):
            if (!null_brush)
                XFillRectangle( gdi_display, pixmaps[OP_DST(*opcode)], gc, 0, 0, width, height );
            break;
        }
    }
    XSetFunction( gdi_display, physdev->gc, GXcopy );
    physdev->exposures += BITBLT_PutDstArea( physdev, result, visrect );
    XFreePixmap( gdi_display, pixmaps[DST] );
    if (pixmaps[TMP]) XFreePixmap( gdi_display, pixmaps[TMP] );
    add_device_bounds( physdev, visrect );
}

/***********************************************************************
 *           X11DRV_PatBlt
 */
BOOL X11DRV_PatBlt( PHYSDEV dev, struct bitblt_coords *dst, DWORD rop )
{
    X11DRV_PDEVICE *physDev = get_x11drv_dev( dev );
    BOOL usePat = (((rop >> 4) & 0x0f0000) != (rop & 0x0f0000));
    const BYTE *opcode = BITBLT_Opcodes[(rop >> 16) & 0xff];

    if (usePat && !X11DRV_SetupGCForBrush( physDev )) return TRUE;

    XSetFunction( gdi_display, physDev->gc, OP_ROP(*opcode) );

    switch(rop)  /* a few special cases */
    {
    case BLACKNESS:  /* 0x00 */
    case WHITENESS:  /* 0xff */
        if ((physDev->depth != 1) && X11DRV_PALETTE_PaletteToXPixel)
        {
            XSetFunction( gdi_display, physDev->gc, GXcopy );
            if (rop == BLACKNESS)
                XSetForeground( gdi_display, physDev->gc, X11DRV_PALETTE_PaletteToXPixel[0] );
            else
                XSetForeground( gdi_display, physDev->gc,
                                WhitePixel( gdi_display, DefaultScreen(gdi_display) ));
            XSetFillStyle( gdi_display, physDev->gc, FillSolid );
        }
        break;
    case DSTINVERT:  /* 0x55 */
        if (!(X11DRV_PALETTE_PaletteFlags & (X11DRV_PALETTE_PRIVATE | X11DRV_PALETTE_VIRTUAL)))
        {
            /* Xor is much better when we do not have full colormap.   */
            /* Using white^black ensures that we invert at least black */
            /* and white. */
            unsigned long xor_pix = (WhitePixel( gdi_display, DefaultScreen(gdi_display) ) ^
                                     BlackPixel( gdi_display, DefaultScreen(gdi_display) ));
            XSetFunction( gdi_display, physDev->gc, GXxor );
            XSetForeground( gdi_display, physDev->gc, xor_pix);
            XSetFillStyle( gdi_display, physDev->gc, FillSolid );
        }
        break;
    }
    XFillRectangle( gdi_display, physDev->drawable, physDev->gc,
                    physDev->dc_rect.left + dst->visrect.left,
                    physDev->dc_rect.top + dst->visrect.top,
                    dst->visrect.right - dst->visrect.left,
                    dst->visrect.bottom - dst->visrect.top );
    add_device_bounds( physDev, &dst->visrect );
    return TRUE;
}


/***********************************************************************
 *           X11DRV_StretchBlt
 */
BOOL X11DRV_StretchBlt( PHYSDEV dst_dev, struct bitblt_coords *dst,
                        PHYSDEV src_dev, struct bitblt_coords *src, DWORD rop )
{
    X11DRV_PDEVICE *physDevDst = get_x11drv_dev( dst_dev );
    X11DRV_PDEVICE *physDevSrc = get_x11drv_dev( src_dev );
    INT width, height;
    const BYTE *opcode;
    Pixmap src_pixmap;
    GC gc;

    if (src_dev->funcs != dst_dev->funcs ||
        src->width != dst->width || src->height != dst->height ||  /* no stretching with core X11 */
        (physDevDst->depth == 1 && physDevSrc->depth != 1) ||  /* color -> mono done by hand */
        (X11DRV_PALETTE_XPixelToPalette && physDevSrc->depth != 1))  /* needs palette mapping */
    {
        dst_dev = GET_NEXT_PHYSDEV( dst_dev, pStretchBlt );
        return dst_dev->funcs->pStretchBlt( dst_dev, dst, src_dev, src, rop );
    }

    width  = dst->visrect.right - dst->visrect.left;
    height = dst->visrect.bottom - dst->visrect.top;
    opcode = BITBLT_Opcodes[(rop >> 16) & 0xff];

    add_device_bounds( physDevDst, &dst->visrect );

    /* a few optimizations for single-op ROPs */
    if (!opcode[1] && OP_SRCDST(opcode[0]) == OP_ARGS(SRC,DST))
    {
        if (same_format(physDevSrc, physDevDst))
        {
            XSetFunction( gdi_display, physDevDst->gc, OP_ROP(*opcode) );
            XCopyArea( gdi_display, physDevSrc->drawable,
                       physDevDst->drawable, physDevDst->gc,
                       physDevSrc->dc_rect.left + src->visrect.left,
                       physDevSrc->dc_rect.top + src->visrect.top,
                       width, height,
                       physDevDst->dc_rect.left + dst->visrect.left,
                       physDevDst->dc_rect.top + dst->visrect.top );
            physDevDst->exposures++;
            return TRUE;
        }
        if (physDevSrc->depth == 1)
        {
            DWORD text_color, bk_color;
            int text_pixel, bkgnd_pixel;

            NtGdiGetDCDword( physDevDst->dev.hdc, NtGdiGetTextColor, &text_color );
            text_pixel = X11DRV_PALETTE_ToPhysical( physDevDst, text_color );

            NtGdiGetDCDword( physDevDst->dev.hdc, NtGdiGetBkColor, &bk_color );
            bkgnd_pixel = X11DRV_PALETTE_ToPhysical( physDevDst, bk_color );

            XSetBackground( gdi_display, physDevDst->gc, text_pixel );
            XSetForeground( gdi_display, physDevDst->gc, bkgnd_pixel );
            XSetFunction( gdi_display, physDevDst->gc, OP_ROP(*opcode) );
            XCopyPlane( gdi_display, physDevSrc->drawable,
                        physDevDst->drawable, physDevDst->gc,
                        physDevSrc->dc_rect.left + src->visrect.left,
                        physDevSrc->dc_rect.top + src->visrect.top,
                        width, height,
                        physDevDst->dc_rect.left + dst->visrect.left,
                        physDevDst->dc_rect.top + dst->visrect.top, 1 );
            physDevDst->exposures++;
            return TRUE;
        }
    }

    gc = XCreateGC( gdi_display, physDevDst->drawable, 0, NULL );
    XSetSubwindowMode( gdi_display, gc, IncludeInferiors );
    XSetGraphicsExposures( gdi_display, gc, False );

    /* retrieve the source */

    src_pixmap = XCreatePixmap( gdi_display, root_window, width, height, physDevDst->depth );
    if (physDevSrc->depth == 1)
    {
        /* MSDN says if StretchBlt must convert a bitmap from monochrome
           to color or vice versa, the foreground and background color of
           the device context are used.  In fact, it also applies to the
           case when it is converted from mono to mono. */
        DWORD text_color, bk_color;
        int text_pixel, bkgnd_pixel;

        NtGdiGetDCDword( physDevDst->dev.hdc, NtGdiGetTextColor, &text_color );
        text_pixel = X11DRV_PALETTE_ToPhysical( physDevDst, text_color );

        NtGdiGetDCDword( physDevDst->dev.hdc, NtGdiGetBkColor, &bk_color );
        bkgnd_pixel = X11DRV_PALETTE_ToPhysical( physDevDst, bk_color );

        if (X11DRV_PALETTE_XPixelToPalette && physDevDst->depth != 1)
        {
            XSetBackground( gdi_display, gc, X11DRV_PALETTE_XPixelToPalette[text_pixel] );
            XSetForeground( gdi_display, gc, X11DRV_PALETTE_XPixelToPalette[bkgnd_pixel]);
        }
        else
        {
            XSetBackground( gdi_display, gc, text_pixel );
            XSetForeground( gdi_display, gc, bkgnd_pixel );
        }
        XCopyPlane( gdi_display, physDevSrc->drawable, src_pixmap, gc,
                    physDevSrc->dc_rect.left + src->visrect.left,
                    physDevSrc->dc_rect.top + src->visrect.top,
                    width, height, 0, 0, 1 );
    }
    else  /* color -> color */
    {
        XCopyArea( gdi_display, physDevSrc->drawable, src_pixmap, gc,
                   physDevSrc->dc_rect.left + src->visrect.left,
                   physDevSrc->dc_rect.top + src->visrect.top,
                   width, height, 0, 0 );
    }

    execute_rop( physDevDst, src_pixmap, gc, &dst->visrect, rop );

    XFreePixmap( gdi_display, src_pixmap );
    XFreeGC( gdi_display, gc );
    return TRUE;
}


static void free_heap_bits( struct gdi_image_bits *bits )
{
    free( bits->ptr );
}

static void free_ximage_bits( struct gdi_image_bits *bits )
{
    XFree( bits->ptr );
}

static inline int get_dib_info_size( const BITMAPINFO *info, UINT coloruse )
{
    if (info->bmiHeader.biCompression == BI_BITFIELDS)
        return sizeof(BITMAPINFOHEADER) + 3 * sizeof(DWORD);
    if (coloruse == DIB_PAL_COLORS)
        return sizeof(BITMAPINFOHEADER) + info->bmiHeader.biClrUsed * sizeof(WORD);
    if (!info->bmiHeader.biClrUsed && info->bmiHeader.biBitCount <= 8)
        return FIELD_OFFSET( BITMAPINFO, bmiColors[1 << info->bmiHeader.biBitCount] );
    return FIELD_OFFSET( BITMAPINFO, bmiColors[info->bmiHeader.biClrUsed] );
}

static inline int get_dib_stride( int width, int bpp )
{
    return ((width * bpp + 31) >> 3) & ~3;
}

static inline int get_dib_image_size( const BITMAPINFO *info )
{
    return get_dib_stride( info->bmiHeader.biWidth, info->bmiHeader.biBitCount )
        * abs( info->bmiHeader.biHeight );
}

static const char *debugstr_bitmapinfo( const BITMAPINFO *info )
{
    DWORD *colors = (DWORD *)((char *)info + info->bmiHeader.biSize);
    return wine_dbg_sprintf( "sz %d w %4d h %4d p %d bpp %2d c %u szi %#8x xp %d yp %d clr %d imp %d clr %#8x:%#8x:%#8x",
        (int)info->bmiHeader.biSize,
        (int)info->bmiHeader.biWidth,
        (int)info->bmiHeader.biHeight,
        (int)info->bmiHeader.biPlanes,
        (int)info->bmiHeader.biBitCount,
        (int)info->bmiHeader.biCompression,
        (int)info->bmiHeader.biSizeImage,
        (int)info->bmiHeader.biXPelsPerMeter,
        (int)info->bmiHeader.biYPelsPerMeter,
        (int)info->bmiHeader.biClrUsed,
        (int)info->bmiHeader.biClrImportant,
        (int)colors[0],
        (int)colors[1],
        (int)colors[2] );

}

/* store the palette or color mask data in the bitmap info structure */
static void set_color_info( const XVisualInfo *vis, BITMAPINFO *info, BOOL has_alpha )
{
    DWORD *colors = (DWORD *)((char *)info + info->bmiHeader.biSize);

    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biClrUsed = 0;

    switch (info->bmiHeader.biBitCount)
    {
    case 4:
    case 8:
    {
        RGBQUAD *rgb = (RGBQUAD *)colors;
        PALETTEENTRY palette[256];
        UINT i, count;

        info->bmiHeader.biClrUsed = 1 << info->bmiHeader.biBitCount;
        count = X11DRV_GetSystemPaletteEntries( NULL, 0, info->bmiHeader.biClrUsed, palette );
        for (i = 0; i < count; i++)
        {
            rgb[i].rgbRed   = palette[i].peRed;
            rgb[i].rgbGreen = palette[i].peGreen;
            rgb[i].rgbBlue  = palette[i].peBlue;
            rgb[i].rgbReserved = 0;
        }
        memset( &rgb[count], 0, (info->bmiHeader.biClrUsed - count) * sizeof(*rgb) );
        break;
    }
    case 16:
        colors[(ImageByteOrder(gdi_display) == LSBFirst) ? 0 : 2] = vis->red_mask;
        colors[(ImageByteOrder(gdi_display) == LSBFirst) ? 1 : 1] = vis->green_mask;
        colors[(ImageByteOrder(gdi_display) == LSBFirst) ? 2 : 0] = vis->blue_mask;
        info->bmiHeader.biCompression = BI_BITFIELDS;
        break;
    case 32:
        colors[(ImageByteOrder(gdi_display) == LSBFirst) ? 0 : 2] = vis->red_mask;
        colors[(ImageByteOrder(gdi_display) == LSBFirst) ? 1 : 1] = vis->green_mask;
        colors[(ImageByteOrder(gdi_display) == LSBFirst) ? 2 : 0] = vis->blue_mask;
        if (colors[0] != 0xff0000 || colors[1] != 0x00ff00 || colors[2] != 0x0000ff || !has_alpha)
            info->bmiHeader.biCompression = BI_BITFIELDS;
        break;
    }
}

static inline BOOL is_r8g8b8( const XVisualInfo *vis )
{
    const XPixmapFormatValues *format = pixmap_formats[vis->depth];
    return format->bits_per_pixel == 24 && vis->red_mask == 0xff0000 && vis->blue_mask == 0x0000ff;
}

/* copy image bits with byte swapping and/or pixel mapping */
static void copy_image_byteswap( const BITMAPINFO *info, const unsigned char *src, unsigned char *dst,
                                 int src_stride, int dst_stride, int height,
                                 const int *mapping, unsigned int zeropad_mask, unsigned int alpha_bits )
{
    int x, y, padding_pos = abs(dst_stride) / sizeof(unsigned int) - 1;

    if (!mapping)  /* simply copy */
    {
        if (src != dst)
        {
            for (y = 0; y < height; y++, src += src_stride, dst += dst_stride)
            {
                memcpy( dst, src, src_stride );
                ((unsigned int *)dst)[padding_pos] &= zeropad_mask;
            }
        }
        else if (zeropad_mask != ~0u)  /* only need to clear the padding */
        {
            for (y = 0; y < height; y++, dst += dst_stride)
                ((unsigned int *)dst)[padding_pos] &= zeropad_mask;
        }
        return;
    }

    switch (info->bmiHeader.biBitCount)
    {
    case 1:
        for (y = 0; y < height; y++, src += src_stride, dst += dst_stride)
        {
            for (x = 0; x < src_stride; x++) dst[x] = bit_swap[src[x]];
            ((unsigned int *)dst)[padding_pos] &= zeropad_mask;
        }
        break;
    case 4:
        for (y = 0; y < height; y++, src += src_stride, dst += dst_stride)
        {
            if (mapping)
                for (x = 0; x < src_stride; x++)
                    dst[x] = mapping[src[x] & 0x0f] | (mapping[src[x] >> 4] << 4);
            else
                for (x = 0; x < src_stride; x++)
                    dst[x] = (src[x] << 4) | (src[x] >> 4);
            ((unsigned int *)dst)[padding_pos] &= zeropad_mask;
        }
        break;
    case 8:
        for (y = 0; y < height; y++, src += src_stride, dst += dst_stride)
        {
            for (x = 0; x < src_stride; x++) dst[x] = mapping[src[x]];
            ((unsigned int *)dst)[padding_pos] &= zeropad_mask;
        }
        break;
    case 16:
        for (y = 0; y < height; y++, src += src_stride, dst += dst_stride)
        {
            for (x = 0; x < info->bmiHeader.biWidth; x++)
                ((USHORT *)dst)[x] = RtlUshortByteSwap( ((const USHORT *)src)[x] );
            ((unsigned int *)dst)[padding_pos] &= zeropad_mask;
        }
        break;
    case 24:
        for (y = 0; y < height; y++, src += src_stride, dst += dst_stride)
        {
            for (x = 0; x < info->bmiHeader.biWidth; x++)
            {
                unsigned char tmp = src[3 * x];
                dst[3 * x]     = src[3 * x + 2];
                dst[3 * x + 1] = src[3 * x + 1];
                dst[3 * x + 2] = tmp;
            }
            ((unsigned int *)dst)[padding_pos] &= zeropad_mask;
        }
        break;
    case 32:
        for (y = 0; y < height; y++, src += src_stride, dst += dst_stride)
            for (x = 0; x < info->bmiHeader.biWidth; x++)
                ((ULONG *)dst)[x] = RtlUlongByteSwap( ((const ULONG *)src)[x] | alpha_bits );
        break;
    }
}

/* copy the image bits, fixing up alignment and byte swapping as necessary */
DWORD copy_image_bits( BITMAPINFO *info, BOOL is_r8g8b8, XImage *image,
                       const struct gdi_image_bits *src_bits, struct gdi_image_bits *dst_bits,
                       struct bitblt_coords *coords, const int *mapping, unsigned int zeropad_mask )
{
    int height = coords->visrect.bottom - coords->visrect.top;
    int width_bytes = image->bytes_per_line;
    unsigned char *src, *dst;

    src = src_bits->ptr;
    if (info->bmiHeader.biHeight > 0)
        src += (info->bmiHeader.biHeight - coords->visrect.bottom) * width_bytes;
    else
        src += coords->visrect.top * width_bytes;

    if ((zeropad_mask != ~0u && !src_bits->is_copy) ||  /* need to clear padding bytes */
        (mapping && !src_bits->is_copy) ||  /* need to remap pixels */
        (width_bytes & 3) /* need to fixup line alignment */)
    {
        width_bytes = (width_bytes + 3) & ~3;
        info->bmiHeader.biSizeImage = height * width_bytes;
        if (!(dst_bits->ptr = malloc( info->bmiHeader.biSizeImage )))
            return ERROR_OUTOFMEMORY;
        dst_bits->is_copy = TRUE;
        dst_bits->free = free_heap_bits;
    }
    else
    {
        /* swap bits in place */
        dst_bits->ptr = src;
        dst_bits->is_copy = src_bits->is_copy;
        dst_bits->free = NULL;
        if (zeropad_mask == ~0u && !mapping) return ERROR_SUCCESS;  /* nothing to do */
    }

    dst = dst_bits->ptr;

    if (info->bmiHeader.biHeight > 0)
    {
        dst += (height - 1) * width_bytes;
        width_bytes = -width_bytes;
    }

    copy_image_byteswap( info, src, dst, image->bytes_per_line, width_bytes, height,
                         mapping, zeropad_mask, 0 );
    return ERROR_SUCCESS;
}

/***********************************************************************
 *           X11DRV_PutImage
 */
DWORD X11DRV_PutImage( PHYSDEV dev, HRGN clip, BITMAPINFO *info,
                       const struct gdi_image_bits *bits, struct bitblt_coords *src,
                       struct bitblt_coords *dst, DWORD rop )
{
    char dst_buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *dst_info = (BITMAPINFO *)dst_buffer;
    X11DRV_PDEVICE *physdev = get_x11drv_dev( dev );
    DWORD ret;
    XImage *image;
    XVisualInfo vis = default_visual;
    struct gdi_image_bits dst_bits;
    const XPixmapFormatValues *format;
    const BYTE *opcode = BITBLT_Opcodes[(rop >> 16) & 0xff];
    const int *mapping = NULL;

    vis.depth = physdev->depth;
    if (physdev->color_shifts)
    {
        vis.red_mask   = physdev->color_shifts->logicalRed.max   << physdev->color_shifts->logicalRed.shift;
        vis.green_mask = physdev->color_shifts->logicalGreen.max << physdev->color_shifts->logicalGreen.shift;
        vis.blue_mask  = physdev->color_shifts->logicalBlue.max  << physdev->color_shifts->logicalBlue.shift;
    }
    format = pixmap_formats[vis.depth];

    dst_info->bmiHeader = info->bmiHeader;
    dst_info->bmiHeader.biPlanes = 1;
    dst_info->bmiHeader.biBitCount = format->bits_per_pixel;
    dst_info->bmiHeader.biHeight = -abs( info->bmiHeader.biHeight );
    set_color_info( &vis, dst_info, FALSE );

    ERR( "src_info %s bits %p\n", debugstr_bitmapinfo( info ), bits );
    ERR( "dst_info %s\n", debugstr_bitmapinfo( dst_info ) );
    if (memcmp( dst_info, info, get_dib_info_size( dst_info, DIB_RGB_COLORS ) )) goto update_format;
    if (!bits) return ERROR_SUCCESS;  /* just querying the format */
    if ((src->width != dst->width) || (src->height != dst->height)) return ERROR_TRANSFORM_NOT_SUPPORTED;

    image = XCreateImage( gdi_display, vis.visual, vis.depth, ZPixmap, 0, NULL,
                          info->bmiHeader.biWidth, src->visrect.bottom - src->visrect.top, 32, 0 );
    if (!image) return ERROR_OUTOFMEMORY;

    if (image->bits_per_pixel == 4 || image->bits_per_pixel == 8)
    {
        if (!opcode[1] && OP_SRCDST(opcode[0]) == OP_ARGS(SRC,DST))
            mapping = X11DRV_PALETTE_PaletteToXPixel;
    }

    ret = copy_image_bits( info, is_r8g8b8(&vis), image, bits, &dst_bits, src, mapping, ~0u );

    if (!ret)
    {
        BOOL restore_region = add_extra_clipping_region( physdev, clip );
        int width = dst->visrect.right - dst->visrect.left;
        int height = dst->visrect.bottom - dst->visrect.top;

        image->data = dst_bits.ptr;

        /* optimization for single-op ROPs */
        if (!opcode[1] && OP_SRCDST(opcode[0]) == OP_ARGS(SRC,DST))
        {
            XSetFunction( gdi_display, physdev->gc, OP_ROP(*opcode) );
            XPutImage( gdi_display, physdev->drawable, physdev->gc, image, src->visrect.left, 0,
                       physdev->dc_rect.left + dst->visrect.left,
                       physdev->dc_rect.top + dst->visrect.top, width, height );
        }
        else
        {
            GC gc = XCreateGC( gdi_display, physdev->drawable, 0, NULL );
            Pixmap src_pixmap = XCreatePixmap( gdi_display, root_window, width, height, vis.depth );

            XSetSubwindowMode( gdi_display, gc, IncludeInferiors );
            XSetGraphicsExposures( gdi_display, gc, False );
            XPutImage( gdi_display, src_pixmap, gc, image, src->visrect.left, 0, 0, 0, width, height );

            execute_rop( physdev, src_pixmap, gc, &dst->visrect, rop );

            XFreePixmap( gdi_display, src_pixmap );
            XFreeGC( gdi_display, gc );
        }

        if (restore_region) restore_clipping_region( physdev );
        add_device_bounds( physdev, &dst->visrect );
        image->data = NULL;
    }

    XDestroyImage( image );
    if (dst_bits.free) dst_bits.free( &dst_bits );
    return ret;

update_format:
    ERR("Need update format\n");
    memcpy( info, dst_info, get_dib_info_size( dst_info, DIB_RGB_COLORS ) );
    return ERROR_BAD_FORMAT;
}

/***********************************************************************
 *           X11DRV_GetImage
 */
DWORD X11DRV_GetImage( PHYSDEV dev, BITMAPINFO *info,
                       struct gdi_image_bits *bits, struct bitblt_coords *src )
{
    X11DRV_PDEVICE *physdev = get_x11drv_dev( dev );
    DWORD ret = ERROR_SUCCESS;
    XImage *image;
    XVisualInfo vis = default_visual;
    UINT align, x, y, width, height;
    struct gdi_image_bits src_bits;
    const XPixmapFormatValues *format;
    const int *mapping = NULL;

    vis.depth = physdev->depth;
    if (physdev->color_shifts)
    {
        vis.red_mask   = physdev->color_shifts->logicalRed.max   << physdev->color_shifts->logicalRed.shift;
        vis.green_mask = physdev->color_shifts->logicalGreen.max << physdev->color_shifts->logicalGreen.shift;
        vis.blue_mask  = physdev->color_shifts->logicalBlue.max  << physdev->color_shifts->logicalBlue.shift;
    }
    format = pixmap_formats[vis.depth];

    /* align start and width to 32-bit boundary */
    switch (format->bits_per_pixel)
    {
    case 1:  align = 32; break;
    case 4:  align = 8;  mapping = X11DRV_PALETTE_XPixelToPalette; break;
    case 8:  align = 4;  mapping = X11DRV_PALETTE_XPixelToPalette; break;
    case 16: align = 2;  break;
    case 24: align = 4;  break;
    case 32: align = 1;  break;
    default:
        FIXME( "depth %u bpp %u not supported yet\n", vis.depth, format->bits_per_pixel );
        return ERROR_BAD_FORMAT;
    }

    info->bmiHeader.biSize          = sizeof(info->bmiHeader);
    info->bmiHeader.biPlanes        = 1;
    info->bmiHeader.biBitCount      = format->bits_per_pixel;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrImportant  = 0;
    set_color_info( &vis, info, FALSE );

    if (!bits) return ERROR_SUCCESS;  /* just querying the color information */

    x = src->visrect.left & ~(align - 1);
    y = src->visrect.top;
    width = src->visrect.right - x;
    height = src->visrect.bottom - src->visrect.top;
    if (format->scanline_pad != 32) width = (width + (align - 1)) & ~(align - 1);
    /* make the source rectangle relative to the returned bits */
    src->x -= x;
    src->y -= y;
    OffsetRect( &src->visrect, -x, -y );

    X11DRV_expect_error( gdi_display, XGetImage_handler, NULL );
    image = XGetImage( gdi_display, physdev->drawable,
                       physdev->dc_rect.left + x, physdev->dc_rect.top + y,
                       width, height, AllPlanes, ZPixmap );
    if (X11DRV_check_error())
    {
        /* use a temporary pixmap to avoid the BadMatch error */
        Pixmap pixmap = XCreatePixmap( gdi_display, root_window, width, height, vis.depth );
        GC gc = XCreateGC( gdi_display, pixmap, 0, NULL );

        XSetGraphicsExposures( gdi_display, gc, False );
        XCopyArea( gdi_display, physdev->drawable, pixmap, gc,
                   physdev->dc_rect.left + x, physdev->dc_rect.top + y, width, height, 0, 0 );
        image = XGetImage( gdi_display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap );
        XFreePixmap( gdi_display, pixmap );
        XFreeGC( gdi_display, gc );
    }

    if (!image) return ERROR_OUTOFMEMORY;

    info->bmiHeader.biWidth     = width;
    info->bmiHeader.biHeight    = -height;
    info->bmiHeader.biSizeImage = height * image->bytes_per_line;

    src_bits.ptr     = image->data;
    src_bits.is_copy = TRUE;
    ret = copy_image_bits( info, is_r8g8b8(&vis), image, &src_bits, bits, src, mapping,
                           zeropad_masks[(width * image->bits_per_pixel) & 31] );

    if (!ret && bits->ptr == image->data)
    {
        bits->free = free_ximage_bits;
        image->data = NULL;
    }
    XDestroyImage( image );
    return ret;
}


/***********************************************************************
 *           put_pixmap_image
 *
 * Simplified equivalent of X11DRV_PutImage that writes directly to a pixmap.
 */
static DWORD put_pixmap_image( Pixmap pixmap, const XVisualInfo *vis,
                               BITMAPINFO *info, const struct gdi_image_bits *bits )
{
    char dst_buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *dst_info = (BITMAPINFO *)dst_buffer;
    DWORD ret;
    XImage *image;
    GC gc;
    struct bitblt_coords coords;
    struct gdi_image_bits dst_bits;
    const XPixmapFormatValues *format = pixmap_formats[vis->depth];
    const int *mapping = NULL;

    if (!format) return ERROR_INVALID_PARAMETER;

    dst_info->bmiHeader = info->bmiHeader;
    dst_info->bmiHeader.biPlanes = 1;
    dst_info->bmiHeader.biBitCount = format->bits_per_pixel;
    dst_info->bmiHeader.biHeight = -abs( info->bmiHeader.biHeight );
    set_color_info( vis, dst_info, FALSE );

    ERR( "src_info %s bits %p\n", debugstr_bitmapinfo( info ), bits );
    ERR( "dst_info %s\n", debugstr_bitmapinfo( dst_info ) );
    if (memcmp( dst_info, info, get_dib_info_size( dst_info, DIB_RGB_COLORS ) )) goto update_format;
    if (!bits) return ERROR_SUCCESS;  /* just querying the format */

    coords.x = 0;
    coords.y = 0;
    coords.width = info->bmiHeader.biWidth;
    coords.height = abs( info->bmiHeader.biHeight );
    SetRect( &coords.visrect, 0, 0, coords.width, coords.height );

    image = XCreateImage( gdi_display, vis->visual, vis->depth, ZPixmap, 0, NULL,
                          coords.width, coords.height, 32, 0 );
    if (!image) return ERROR_OUTOFMEMORY;

    if (image->bits_per_pixel == 4 || image->bits_per_pixel == 8)
        mapping = X11DRV_PALETTE_PaletteToXPixel;

    if (!(ret = copy_image_bits( info, is_r8g8b8(vis), image, bits, &dst_bits, &coords, mapping, ~0u )))
    {
        image->data = dst_bits.ptr;
        gc = XCreateGC( gdi_display, pixmap, 0, NULL );
        XPutImage( gdi_display, pixmap, gc, image, 0, 0, 0, 0, coords.width, coords.height );
        XFreeGC( gdi_display, gc );
        image->data = NULL;
        if (dst_bits.free) dst_bits.free( &dst_bits );
    }

    XDestroyImage( image );
    return ret;

update_format:
    ERR("Need update format\n");
    memcpy( info, dst_info, get_dib_info_size( info, DIB_RGB_COLORS ) );
    return ERROR_BAD_FORMAT;
}


/***********************************************************************
 *           create_pixmap_from_image
 */
Pixmap create_pixmap_from_image( HDC hdc, const XVisualInfo *vis, const BITMAPINFO *info,
                                 const struct gdi_image_bits *bits, UINT coloruse )
{
    static const RGBQUAD default_colortable[2] = { { 0x00, 0x00, 0x00 }, { 0xff, 0xff, 0xff } };
    char dst_buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    char src_buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *dst_info = (BITMAPINFO *)dst_buffer;
    BITMAPINFO *src_info = (BITMAPINFO *)src_buffer;
    struct gdi_image_bits dst_bits;
    Pixmap pixmap;
    DWORD err;
    HBITMAP dib;

    pixmap = XCreatePixmap( gdi_display, root_window,
                            info->bmiHeader.biWidth, abs(info->bmiHeader.biHeight), vis->depth );
    if (!pixmap) return 0;

    memcpy( src_info, info, get_dib_info_size( info, coloruse ));
    memcpy( dst_info, info, get_dib_info_size( info, coloruse ));

    if (coloruse == DIB_PAL_COLORS ||
        (err = put_pixmap_image( pixmap, vis, dst_info, bits )) == ERROR_BAD_FORMAT)
    {
        if (dst_info->bmiHeader.biBitCount == 1)  /* set a default color table for 1-bpp */
            memcpy( dst_info->bmiColors, default_colortable, sizeof(default_colortable) );
        dib = NtGdiCreateDIBSection( hdc, 0, 0, dst_info, coloruse, 0, 0, 0, &dst_bits.ptr );
        if (dib)
        {
            if (src_info->bmiHeader.biBitCount == 1 && !src_info->bmiHeader.biClrUsed)
                memcpy( src_info->bmiColors, default_colortable, sizeof(default_colortable) );
            NtGdiSetDIBitsToDeviceInternal( hdc, 0, 0, 0, 0, 0, 0, 0,
                                            abs(info->bmiHeader.biHeight), bits->ptr, src_info, coloruse,
                                            0, 0, FALSE, dib ); /* SetDIBits */
            dst_bits.free = NULL;
            dst_bits.is_copy = TRUE;
            err = put_pixmap_image( pixmap, vis, dst_info, &dst_bits );
            NtGdiDeleteObjectApp( dib );
        }
        else err = ERROR_OUTOFMEMORY;
    }

    if (!err) return pixmap;

    XFreePixmap( gdi_display, pixmap );
    return 0;

}


/***********************************************************************
 *           get_pixmap_image
 *
 * Equivalent of X11DRV_GetImage that reads directly from a pixmap.
 */
DWORD get_pixmap_image( Pixmap pixmap, int width, int height, const XVisualInfo *vis,
                        BITMAPINFO *info, struct gdi_image_bits *bits )
{
    DWORD ret = ERROR_SUCCESS;
    XImage *image;
    struct gdi_image_bits src_bits;
    struct bitblt_coords coords;
    const XPixmapFormatValues *format = pixmap_formats[vis->depth];
    const int *mapping = NULL;

    if (!format) return ERROR_INVALID_PARAMETER;

    info->bmiHeader.biSize          = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth         = width;
    info->bmiHeader.biHeight        = -height;
    info->bmiHeader.biPlanes        = 1;
    info->bmiHeader.biBitCount      = format->bits_per_pixel;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrImportant  = 0;
    set_color_info( vis, info, FALSE );

    if (!bits) return ERROR_SUCCESS;  /* just querying the color information */

    coords.x = 0;
    coords.y = 0;
    coords.width = width;
    coords.height = height;
    SetRect( &coords.visrect, 0, 0, width, height );

    image = XGetImage( gdi_display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap );
    if (!image) return ERROR_OUTOFMEMORY;

    info->bmiHeader.biSizeImage = height * image->bytes_per_line;

    src_bits.ptr     = image->data;
    src_bits.is_copy = TRUE;
    ret = copy_image_bits( info, is_r8g8b8(vis), image, &src_bits, bits, &coords, mapping,
                           zeropad_masks[(width * image->bits_per_pixel) & 31] );

    if (!ret && bits->ptr == image->data)
    {
        bits->free = free_ximage_bits;
        image->data = NULL;
    }
    XDestroyImage( image );
    return ret;
}

struct x11drv_window_surface
{
    struct window_surface header;
    Window                window;
    xcb_gcontext_t        xcb_gc;
    xcb_shm_seg_t         xcb_seg;
    UINT                  offsets[3];
};

static struct x11drv_window_surface *get_x11_surface( struct window_surface *surface )
{
    return (struct x11drv_window_surface *)surface;
}

static XRectangle *xrectangles_from_rects( const RECT *rects, UINT count )
{
    XRectangle *xrects;
    if (!(xrects = malloc( count * sizeof(*xrects) ))) return NULL;
    while (count--)
    {
        if (rects[count].left > SHRT_MAX) continue;
        if (rects[count].top > SHRT_MAX) continue;
        if (rects[count].right < SHRT_MIN) continue;
        if (rects[count].bottom < SHRT_MIN) continue;
        xrects[count].x      = max( min( rects[count].left, SHRT_MAX), SHRT_MIN);
        xrects[count].y      = max( min( rects[count].top, SHRT_MAX), SHRT_MIN);
        xrects[count].width  = max( min( rects[count].right, SHRT_MAX ) - xrects[count].x, 0);
        xrects[count].height = max( min( rects[count].bottom, SHRT_MAX ) - xrects[count].y, 0);
    }
    return xrects;
}

/***********************************************************************
 *           x11drv_surface_set_clip
 */
static void x11drv_surface_set_clip( struct window_surface *window_surface, const RECT *rects, UINT count )
{
    struct x11drv_window_surface *surface = get_x11_surface( window_surface );
#if HAVE_X11_XLIB_XCB_H
{
    xcb_connection_t *xcb = XGetXCBConnection( gdi_display );
    XRectangle *xrects = NULL;

    ERR( "surface %p, rects %p, count %u\n", surface, rects, count );

    if (count) xrects = xrectangles_from_rects( rects, count );
    if (!xrects) count = 0;

    XCB_CALL( xcb_set_clip_rectangles, xcb, XCB_CLIP_ORDERING_YX_BANDED,
              surface->xcb_gc, 0, 0, count, (xcb_rectangle_t *)xrects );
    free( xrects );
}
#endif
}

/***********************************************************************
 *           x11drv_surface_flush
 */
static BOOL x11drv_surface_flush( struct window_surface *window_surface, const RECT *rect, const RECT *dirty,
                                  const BITMAPINFO *color_info, const void *color_bits, BOOL shape_changed,
                                  const BITMAPINFO *shape_info, const void *shape_bits )
{
    UINT alpha_mask = window_surface->alpha_mask, alpha_bits = window_surface->alpha_bits;
    struct x11drv_window_surface *surface = get_x11_surface( window_surface );

ERR("\n");

    if (alpha_bits == -1)
    {
        if (alpha_mask || color_info->bmiHeader.biBitCount != 32) alpha_bits = 0;
        else if (color_info->bmiHeader.biCompression == BI_RGB) alpha_bits = 0xff000000;
        else
        {
            DWORD *colors = (DWORD *)color_info->bmiColors;
            alpha_bits = ~(colors[0] | colors[1] | colors[2]);
        }
    }

#ifdef HAVE_XCB_SHM_H
    if (!surface->xcb_seg)
    {
        xcb_connection_t *xcb = XGetXCBConnection( gdi_display );
ERR("\n");
        XCB_CALL( xcb_put_image, xcb, XCB_IMAGE_FORMAT_Z_PIXMAP, surface->window, surface->xcb_gc, dirty->right - dirty->left, dirty->bottom - dirty->top,
                  rect->left + dirty->left, rect->top + dirty->top, 0, color_info->bmiHeader.biBitCount, color_info->bmiHeader.biSizeImage,
                  color_bits );
        xcb_flush( xcb );
    }
#endif

    if (shape_changed)
    {
#ifdef HAVE_LIBXSHAPE
        if (!shape_bits)
            XShapeCombineMask( gdi_display, surface->window, ShapeBounding, 0, 0, None, ShapeSet );
        else
        {
            struct gdi_image_bits bits = {.ptr = (void *)shape_bits};
            XVisualInfo vis = default_visual;
            Pixmap shape;

            vis.depth = 1;
            shape = create_pixmap_from_image( 0, &vis, shape_info, &bits, DIB_RGB_COLORS );
            XShapeCombineMask( gdi_display, surface->window, ShapeBounding, 0, 0, shape, ShapeSet );
            XFreePixmap( gdi_display, shape );
        }
#endif /* HAVE_LIBXSHAPE */
    }

    if (!put_shm_image( ximage, &surface->image->shminfo, surface->window, surface->gc, rect, dirty ))
        XPutImage( gdi_display, surface->window, surface->gc, ximage, dirty->left,
                   dirty->top, rect->left + dirty->left, rect->top + dirty->top,
                   dirty->right - dirty->left, dirty->bottom - dirty->top );

    XFlush( gdi_display );

    return TRUE;
}

/***********************************************************************
 *           x11drv_surface_destroy
 */
static void x11drv_surface_destroy( struct window_surface *window_surface )
{
    struct x11drv_window_surface *surface = get_x11_surface( window_surface );

    TRACE( "freeing %p\n", surface );

#ifdef HAVE_X11_XLIB_XCB_H
{
    xcb_connection_t *xcb = XGetXCBConnection( gdi_display );
    XCB_CALL( xcb_free_gc, xcb, surface->xcb_gc );
    XCB_CALL( xcb_shm_detach, xcb, surface->xcb_seg );
}
#endif

    free( surface );
}

static void x11drv_surface_create_images( struct window_surface *window_surface, int fd, UINT size,
                                          const BITMAPINFO *info, UINT count )
{
    struct x11drv_window_surface *surface = get_x11_surface( window_surface );
#if HAVE_XCB_SHM_H
{
    xcb_connection_t *xcb = XGetXCBConnection( gdi_display );
    xcb_shm_query_version_reply_t *version;
    xcb_generic_error_t *error;

    version = xcb_shm_query_version_reply( xcb, xcb_shm_query_version( xcb ), &error ); /* closes the fd */
ERR( "version %p error %p\n", version, error );
if (version) ERR( "%u.%u shared %u fmt %u\n", version->major_version, version->minor_version, version->shared_pixmaps, version->pixmap_format );
    surface->xcb_seg = xcb_generate_id( xcb );
    if (!call_xcb( xcb, xcb_shm_attach_fd_checked( xcb, surface->xcb_seg, dup( fd ), FALSE ), "xcb_shm_attach_fd", TRUE )) surface->xcb_seg = 0;
    for (UINT i = 0; i < count; i++) surface->offsets[i] = i * info->bmiHeader.biSizeImage;
}
#endif
}

static UINT x11drv_surface_acquire_image( struct window_surface *window_surface )
{
    return 0;
}

static void x11drv_surface_present_image( struct window_surface *window_surface, UINT index, const RECT *dirty, const BITMAPINFO *color_info )
{
    struct x11drv_window_surface *surface = get_x11_surface( window_surface );
#ifdef HAVE_XCB_SHM_H
    if (surface->xcb_seg)
    {
        xcb_connection_t *xcb = XGetXCBConnection( gdi_display );
        XCB_CALL( xcb_shm_put_image, xcb, surface->window, surface->xcb_gc, color_info->bmiHeader.biWidth, abs( color_info->bmiHeader.biHeight ),
                   dirty->left, dirty->top, dirty->right - dirty->left, dirty->bottom - dirty->top,
                   dirty->left, dirty->top, color_info->bmiHeader.biBitCount,
                   XCB_IMAGE_FORMAT_Z_PIXMAP, FALSE, surface->xcb_seg, surface->offsets[index] );
        xcb_flush( xcb );
    }
#endif
}

static const struct window_surface_funcs x11drv_surface_funcs =
{
    .set_clip = x11drv_surface_set_clip,
    .flush = x11drv_surface_flush,
    .destroy = x11drv_surface_destroy,
    .create_images = x11drv_surface_create_images,
    .acquire_image = x11drv_surface_acquire_image,
    .present_image = x11drv_surface_present_image,
};

/***********************************************************************
 *           create_surface
 */
static struct window_surface *create_surface( HWND hwnd, Window window, const XVisualInfo *vis, const RECT *rect,
                                              BOOL use_alpha )
{
    const XPixmapFormatValues *format = pixmap_formats[vis->depth];
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    struct x11drv_window_surface *surface;
    int width = rect->right - rect->left, height = rect->bottom - rect->top;
    struct window_surface *window_surface;

    memset( info, 0, sizeof(*info) );
    info->bmiHeader.biSize        = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth       = width;
    info->bmiHeader.biHeight      = -height; /* top-down */
    info->bmiHeader.biPlanes      = 1;
    info->bmiHeader.biBitCount    = format->bits_per_pixel;
    info->bmiHeader.biSizeImage   = get_dib_image_size( info );
    set_color_info( vis, info, use_alpha );

    if ((window_surface = window_surface_create( sizeof(*surface), &x11drv_surface_funcs, hwnd, rect, info, 0 )))
    {
        surface = get_x11_surface( window_surface );
        surface->window = window;

#ifdef HAVE_X11_XLIB_XCB_H
{
        struct xcb_create_gc_value_list_t values = {.subwindow_mode = XCB_SUBWINDOW_MODE_INCLUDE_INFERIORS};
        xcb_connection_t *xcb = XGetXCBConnection( gdi_display );
        surface->xcb_gc = xcb_generate_id( xcb );
        XCB_CALL( xcb_create_gc_aux, xcb, surface->xcb_gc, window, XCB_GC_SUBWINDOW_MODE, &values );
}
#endif
    }

    return window_surface;
}

struct x11drv_scaled_surface
{
    struct window_surface header;
    Window window;
    float scale;
    HDC hdc;
};

static struct x11drv_scaled_surface *get_x11drv_scaled_surface( struct window_surface *surface )
{
    return (struct x11drv_scaled_surface *)surface;
}

static void x11drv_scaled_surface_set_clip( struct window_surface *window_surface, const RECT *rects, UINT count )
{
}

static BOOL x11drv_scaled_surface_flush( struct window_surface *window_surface, const RECT *rect, const RECT *dirty,
                                         const BITMAPINFO *color_info, const void *color_bits, BOOL shape_changed,
                                         const BITMAPINFO *shape_info, const void *shape_bits )
{
    struct x11drv_scaled_surface *surface = get_x11drv_scaled_surface( window_surface );
    struct x11drv_win_data *data;
    RECT rect_tmp, rect_dst;
    HDC hdc_src;

    if (!(data = get_win_data( window_surface->hwnd ))) return FALSE;
    rect_dst = data->rects.visible;
    release_win_data( data );

    OffsetRect( &rect_dst, -rect_dst.left, -rect_dst.top );
    if (get_dc_drawable( surface->hdc, &rect_tmp ) != surface->window || !EqualRect( &rect_tmp, &rect_dst ))
        set_dc_drawable( surface->hdc, surface->window, &rect_dst, ClipByChildren );

    hdc_src = NtGdiCreateCompatibleDC( 0 );
    NtGdiSelectBitmap( hdc_src, window_surface->color_bitmap );

    NtGdiStretchBlt( surface->hdc, rect->left * surface->scale, rect->top * surface->scale,
                     rect->right * surface->scale, rect->bottom * surface->scale,
                     hdc_src, 0, 0, rect->right, rect->bottom, SRCCOPY, 0 );

    NtGdiDeleteObjectApp( hdc_src );
    return TRUE;
}

static void x11drv_scaled_surface_destroy( struct window_surface *window_surface )
{
    struct x11drv_scaled_surface *surface = get_x11drv_scaled_surface( window_surface );
    NtGdiDeleteObjectApp( surface->hdc );
}

static const struct window_surface_funcs x11drv_scaled_surface_funcs =
{
    x11drv_scaled_surface_set_clip,
    x11drv_scaled_surface_flush,
    x11drv_scaled_surface_destroy,
};

static struct window_surface *create_scaled_surface( HWND hwnd, Window window, const RECT *surface_rect, float scale )
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    struct window_surface *window_surface;
    struct x11drv_scaled_surface *surface;

    memset( info, 0, sizeof(*info) );
    info->bmiHeader.biSize        = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth       = surface_rect->right;
    info->bmiHeader.biHeight      = -surface_rect->bottom; /* top-down */
    info->bmiHeader.biPlanes      = 1;
    info->bmiHeader.biBitCount    = 32;
    info->bmiHeader.biSizeImage   = get_dib_image_size( info );
    info->bmiHeader.biCompression = BI_RGB;

    if ((window_surface = window_surface_create( sizeof(*surface), &x11drv_scaled_surface_funcs, hwnd, surface_rect, info, 0 )))
    {
        static const WCHAR displayW[] = {'D','I','S','P','L','A','Y'};
        UNICODE_STRING device_str = RTL_CONSTANT_STRING(displayW);

        surface = get_x11drv_scaled_surface( window_surface );
        surface->window = window;
        surface->scale = scale;
        surface->hdc = NtGdiOpenDCW( &device_str, NULL, NULL, 0, TRUE, NULL, NULL, NULL );
    }

    return window_surface;
}

static BOOL enable_direct_drawing( struct x11drv_win_data *data, BOOL layered )
{
    const XPixmapFormatValues *format = pixmap_formats[data->vis.depth];
    if (layered) return FALSE;
    if (data->embedded) return TRUE; /* draw directly to the window */
    if (data->whole_window == root_window) return TRUE; /* draw directly to the window */
    if (data->client_window) return TRUE; /* draw directly to the window */
    if (!client_side_graphics) return TRUE; /* draw directly to the window */
    if (format->bits_per_pixel <= 8) return TRUE; /* draw directly to the window */
    return FALSE;
}

/***********************************************************************
 *      CreateWindowSurface   (X11DRV.@)
 */
BOOL X11DRV_CreateWindowSurface( HWND hwnd, BOOL layered, float scale, const RECT *surface_rect,
                                 struct window_surface **surface )
{
    struct window_surface *previous;
    struct x11drv_win_data *data;

    TRACE( "hwnd %p, layered %u, surface_rect %s, surface %p\n", hwnd, layered, wine_dbgstr_rect( surface_rect ), surface );

    if (scale != 1.0 && layered) return FALSE; /* FIXME: XRender doesn't seem to like alpha much */

    if (!(data = get_win_data( hwnd ))) return TRUE; /* use default surface */
    if ((previous = *surface) && scale == 1.0 && previous->funcs == &x11drv_surface_funcs)
    {
        Window window = get_x11_surface(previous)->window;
        if (data->whole_window == window && !enable_direct_drawing( data, layered )) goto done; /* use default surface */
        /* re-create window surface is window has changed, which can happen when changing visual */
        TRACE( "re-creating hwnd %p surface with new window %lx\n", data->hwnd, data->whole_window );
    }
    else if (previous && scale != 1.0 && previous->funcs == &x11drv_scaled_surface_funcs)
    {
        struct x11drv_scaled_surface *scaled_surface = get_x11drv_scaled_surface(previous);
        scaled_surface->window = data->whole_window;
        scaled_surface->scale = scale;
        if (!enable_direct_drawing( data, layered )) goto done; /* use default surface */
    }
    if (previous) window_surface_release( previous );

    if (layered)
    {
        data->layered = TRUE;
        if (!data->embedded && argb_visual.visualid) set_window_visual( data, &argb_visual, TRUE );
    }
    else if (enable_direct_drawing( data, layered ))
    {
        *surface = NULL;  /* indicate that we want to draw directly to the window */
        goto done; /* draw directly to the window */
    }

    if (scale == 1.0)
        *surface = create_surface( data->hwnd, data->whole_window, &data->vis, surface_rect,
                                   layered ? data->use_alpha : FALSE );
    else
        *surface = create_scaled_surface( data->hwnd, data->whole_window, surface_rect, scale );

done:
    release_win_data( data );
    return TRUE;
}
