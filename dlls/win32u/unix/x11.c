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

Display *gdi_display;  /* display to use for all GDI functions */
Window root_window;
XVisualInfo default_visual = { 0 };

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);

Window create_client_window( HWND hwnd, const XVisualInfo *visual )
{
    FIXME("stub!\n");
    return 0;
}
