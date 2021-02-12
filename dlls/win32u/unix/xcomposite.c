#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#include "x11drv.h"
#include "xcomposite.h"
#include "winternl.h"
#include "wine/debug.h"

BOOL usexcomposite = FALSE;

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);

#ifdef SONAME_LIBXCOMPOSITE

#define MAKE_FUNCPTR(f) typeof(f) * p##f;
MAKE_FUNCPTR(XCompositeQueryExtension)
MAKE_FUNCPTR(XCompositeQueryVersion)
MAKE_FUNCPTR(XCompositeVersion)
MAKE_FUNCPTR(XCompositeRedirectWindow)
MAKE_FUNCPTR(XCompositeRedirectSubwindows)
MAKE_FUNCPTR(XCompositeUnredirectWindow)
MAKE_FUNCPTR(XCompositeUnredirectSubwindows)
MAKE_FUNCPTR(XCompositeCreateRegionFromBorderClip)
MAKE_FUNCPTR(XCompositeNameWindowPixmap)
#undef MAKE_FUNCPTR

static int xcomp_event_base;
static int xcomp_error_base;

static void X11DRV_XComposite_Init(void)
{
    void *xcomposite_handle = dlopen(SONAME_LIBXCOMPOSITE, RTLD_NOW);
    if (!xcomposite_handle)
    {
        TRACE("Unable to open %s, XComposite disabled\n", SONAME_LIBXCOMPOSITE);
        usexcomposite = FALSE;
        return;
    }

#define LOAD_FUNCPTR(f) \
    if((p##f = dlsym(xcomposite_handle, #f)) == NULL) goto sym_not_found
    LOAD_FUNCPTR(XCompositeQueryExtension);
    LOAD_FUNCPTR(XCompositeQueryVersion);
    LOAD_FUNCPTR(XCompositeVersion);
    LOAD_FUNCPTR(XCompositeRedirectWindow);
    LOAD_FUNCPTR(XCompositeRedirectSubwindows);
    LOAD_FUNCPTR(XCompositeUnredirectWindow);
    LOAD_FUNCPTR(XCompositeUnredirectSubwindows);
    LOAD_FUNCPTR(XCompositeCreateRegionFromBorderClip);
    LOAD_FUNCPTR(XCompositeNameWindowPixmap);
#undef LOAD_FUNCPTR

    if(!pXCompositeQueryExtension(gdi_display, &xcomp_event_base,
                                  &xcomp_error_base)) {
        TRACE("XComposite extension could not be queried; disabled\n");
        dlclose(xcomposite_handle);
        xcomposite_handle = NULL;
        usexcomposite = FALSE;
        return;
    }
    TRACE("XComposite is up and running error_base = %d\n", xcomp_error_base);
    return;

sym_not_found:
    TRACE("Unable to load function pointers from %s, XComposite disabled\n", SONAME_LIBXCOMPOSITE);
    dlclose(xcomposite_handle);
    xcomposite_handle = NULL;
    usexcomposite = FALSE;
}
#endif /* defined(SONAME_LIBXCOMPOSITE) */
