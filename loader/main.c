/*
 * Emulator initialisation code
 *
 * Copyright 2000 Alexandre Julliard
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

#include "config.h"

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"
#include "winnt.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <limits.h>
#ifdef HAVE_SYS_SYSCTL_H
# include <sys/sysctl.h>
#endif
#ifdef HAVE_LINK_H
# include <link.h>
#endif
#ifdef HAVE_SYS_LINK_H
# include <sys/link.h>
#endif

#include "wine/list.h"

#include "main.h"

extern char **environ;

/* the preloader will set this variable */
const __attribute((visibility("default"))) struct wine_preload_info *wine_main_preload_info = NULL;

#ifdef __linux__

/* the preloader will set these variables */
typedef void (*rtld_init_func)( struct link_map *map );
__attribute((visibility("default"))) rtld_init_func wine_rtld_init = NULL;
typedef void (*rtld_notify_func)(void);
__attribute((visibility("default"))) rtld_notify_func wine_rtld_map_start = NULL;
__attribute((visibility("default"))) rtld_notify_func wine_rtld_map_complete = NULL;
__attribute((visibility("default"))) rtld_notify_func wine_rtld_unmap_start = NULL;
__attribute((visibility("default"))) rtld_notify_func wine_rtld_unmap_complete = NULL;

struct link_map_entry
{
    struct link_map map;
    const void *module;
    struct list entry;
};

static pthread_mutex_t link_map_lock;
static struct link_map link_map = {0};
static struct list dll_map_entries = LIST_INIT( dll_map_entries );

static char *link_realpath( const char *path )
{
    char *real;
    if (!path) return NULL;
    if (!(real = realpath( path, NULL ))) return strdup( path );
    return real;
}

static void sync_wine_link_map(void)
{
    static struct r_debug *_r_debug;
    struct link_map *next = &link_map, *last = NULL, **rtld_map, **wine_map;
    struct link_map_entry *pe_entry = NULL;
    struct list *ptr;

    if (!_r_debug) _r_debug = dlsym( RTLD_NEXT, "_r_debug" );
    rtld_map = &_r_debug->r_map;
    wine_map = &next;

    /* detach the PE link map */
    if ((ptr = list_head( &dll_map_entries )))
    {
        pe_entry = LIST_ENTRY( ptr, struct link_map_entry, entry );
        if (pe_entry->map.l_prev) pe_entry->map.l_prev->l_next = NULL;
    }

    while (*rtld_map)
    {
        if (!*wine_map)
        {
            if (!(*wine_map = calloc( 1, sizeof(struct link_map) ))) break;
            (*wine_map)->l_prev = last;
        }

        last = *wine_map;
        free( (*wine_map)->l_name );
        (*wine_map)->l_addr = (*rtld_map)->l_addr;
        (*wine_map)->l_name = link_realpath( (*rtld_map)->l_name );
        (*wine_map)->l_ld = (*rtld_map)->l_ld;
        rtld_map = &(*rtld_map)->l_next;
        wine_map = &(*wine_map)->l_next;
    }

    /* remove the remaining wine entries */
    next = *wine_map;
    *wine_map = NULL;

    while (next)
    {
        struct link_map *tmp = next;
        wine_map = &next->l_next;
        next = *wine_map;
        *wine_map = NULL;
        free( tmp->l_name );
        free( tmp );
    }

    if (pe_entry)
    {
        /* attach PE link map back */
        pe_entry->map.l_prev = last;
        last->l_next = &pe_entry->map;
    }
}

static void add_dll_to_pe_link_map( const void *module, const char *unix_path, INT_PTR offset )
{
    struct link_map_entry *entry, *last_entry;
    struct list *ptr;

    if (!(entry = calloc( 1, sizeof(*entry) ))) return;

    entry->module = module;
    entry->map.l_addr = offset;
    entry->map.l_name = link_realpath( unix_path );

    if ((ptr = list_tail( &dll_map_entries )))
    {
        last_entry = LIST_ENTRY( ptr, struct link_map_entry, entry );
        entry->map.l_prev = &last_entry->map;
        last_entry->map.l_next = &entry->map;
    }

    list_add_tail( &dll_map_entries, &entry->entry );
    if (!entry->map.l_prev) sync_wine_link_map();
}

__attribute((visibility("default"))) void wine_gdb_dll_loaded( const void *module, const char *unix_path, INT_PTR offset )
{
    struct link_map_entry *entry;

    pthread_mutex_lock( &link_map_lock );

    LIST_FOR_EACH_ENTRY( entry, &dll_map_entries, struct link_map_entry, entry )
        if (entry->module == module) break;

    if (&entry->entry == &dll_map_entries)
        add_dll_to_pe_link_map( module, unix_path, offset );
    else
        entry->map.l_addr = offset;

    if (wine_rtld_map_start) wine_rtld_map_start();
    if (wine_rtld_map_complete) wine_rtld_map_complete();

    pthread_mutex_unlock( &link_map_lock );
}

__attribute((visibility("default"))) void wine_gdb_dll_unload( const void *module )
{
    struct link_map *prev, *next;
    struct link_map_entry *entry;

    pthread_mutex_lock( &link_map_lock );

    LIST_FOR_EACH_ENTRY( entry, &dll_map_entries, struct link_map_entry, entry )
        if (entry->module == module) break;

    if (&entry->entry == &dll_map_entries)
    {
        pthread_mutex_unlock( &link_map_lock );
        return;
    }

    list_remove( &entry->entry );
    if ((prev = entry->map.l_prev)) prev->l_next = entry->map.l_next;
    if ((next = entry->map.l_next)) next->l_prev = entry->map.l_prev;

    if (wine_rtld_unmap_start) wine_rtld_unmap_start();
    if (wine_rtld_unmap_complete) wine_rtld_unmap_complete();

    pthread_mutex_unlock( &link_map_lock );

    free( entry->map.l_name );
    free( entry );
}

__attribute((visibility("default"))) void *dlopen( const char *file, int mode )
{
    static typeof(dlopen) *rtld_dlopen;
    void *ret;

    pthread_mutex_lock( &link_map_lock );

    if (!rtld_dlopen) rtld_dlopen = dlsym( RTLD_NEXT, "dlopen" );
    ret = rtld_dlopen( file, mode );

    if (wine_rtld_map_start) wine_rtld_map_start();
    sync_wine_link_map();
    if (wine_rtld_map_complete) wine_rtld_map_complete();

    pthread_mutex_unlock( &link_map_lock );

    return ret;
}

__attribute((visibility("default"))) int dlclose( void *handle )
{
    static typeof(dlclose) *rtld_dlclose;
    int ret;

    pthread_mutex_lock( &link_map_lock );

    if (!rtld_dlclose) rtld_dlclose = dlsym( RTLD_NEXT, "dlclose" );
    ret = rtld_dlclose( handle );

    if (wine_rtld_unmap_start) wine_rtld_unmap_start();
    sync_wine_link_map();
    if (wine_rtld_unmap_complete) wine_rtld_unmap_complete();

    pthread_mutex_unlock( &link_map_lock );

    return ret;
}

#endif /* __linux__ */

/* canonicalize path and return its directory name */
static char *realpath_dirname( const char *name )
{
    char *p, *fullpath = realpath( name, NULL );

    if (fullpath)
    {
        p = strrchr( fullpath, '/' );
        if (p == fullpath) p++;
        if (p) *p = 0;
    }
    return fullpath;
}

/* if string ends with tail, remove it */
static char *remove_tail( const char *str, const char *tail )
{
    size_t len = strlen( str );
    size_t tail_len = strlen( tail );
    char *ret;

    if (len < tail_len) return NULL;
    if (strcmp( str + len - tail_len, tail )) return NULL;
    ret = malloc( len - tail_len + 1 );
    memcpy( ret, str, len - tail_len );
    ret[len - tail_len] = 0;
    return ret;
}

/* build a path from the specified dir and name */
static char *build_path( const char *dir, const char *name )
{
    size_t len = strlen( dir );
    char *ret = malloc( len + strlen( name ) + 2 );

    memcpy( ret, dir, len );
    if (len && ret[len - 1] != '/') ret[len++] = '/';
    strcpy( ret + len, name );
    return ret;
}

static const char *get_self_exe( char *argv0 )
{
#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
    return "/proc/self/exe";
#elif defined (__FreeBSD__) || defined(__DragonFly__)
    static int pathname[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    size_t path_size = PATH_MAX;
    char *path = malloc( path_size );
    if (path && !sysctl( pathname, sizeof(pathname)/sizeof(pathname[0]), path, &path_size, NULL, 0 ))
        return path;
    free( path );
#endif

    if (!strchr( argv0, '/' )) /* search in PATH */
    {
        char *p, *path = getenv( "PATH" );

        if (!path || !(path = strdup(path))) return NULL;
        for (p = strtok( path, ":" ); p; p = strtok( NULL, ":" ))
        {
            char *name = build_path( p, argv0 );
            if (!access( name, X_OK ))
            {
                free( path );
                return name;
            }
            free( name );
        }
        free( path );
        return NULL;
    }
    return argv0;
}

static void *try_dlopen( const char *dir, const char *name )
{
    char *path = build_path( dir, name );
    void *handle = dlopen( path, RTLD_NOW );
    free( path );
    return handle;
}

static void *load_ntdll( char *argv0 )
{
#ifdef __i386__
#define SO_DIR "i386-unix/"
#elif defined(__x86_64__)
#define SO_DIR "x86_64-unix/"
#elif defined(__arm__)
#define SO_DIR "arm-unix/"
#elif defined(__aarch64__)
#define SO_DIR "aarch64-unix/"
#else
#define SO_DIR ""
#endif
    const char *self = get_self_exe( argv0 );
    char *path, *p;
    void *handle = NULL;

    if (self && ((path = realpath_dirname( self ))))
    {
        if ((p = remove_tail( path, "/loader" )))
        {
            handle = try_dlopen( p, "dlls/ntdll/ntdll.so" );
            free( p );
        }
        else handle = try_dlopen( path, BIN_TO_DLLDIR "/" SO_DIR "ntdll.so" );
        free( path );
    }

    if (!handle && (path = getenv( "WINEDLLPATH" )))
    {
        path = strdup( path );
        for (p = strtok( path, ":" ); p; p = strtok( NULL, ":" ))
        {
            handle = try_dlopen( p, SO_DIR "ntdll.so" );
            if (!handle) handle = try_dlopen( p, "ntdll.so" );
            if (handle) break;
        }
        free( path );
    }

    if (!handle && !self) handle = try_dlopen( DLLDIR, SO_DIR "ntdll.so" );

    return handle;
}


/**********************************************************************
 *           main
 */
int main( int argc, char *argv[] )
{
    void *handle;
#ifdef __linux__
    pthread_mutexattr_t attr;
    pthread_mutexattr_init( &attr );
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
    pthread_mutex_init( &link_map_lock, &attr );
    pthread_mutexattr_destroy( &attr );

    if (wine_rtld_init) wine_rtld_init( &link_map );
#endif /* __linux__ */

    if ((handle = load_ntdll( argv[0] )))
    {
        void (*init_func)(int, char **, char **) = dlsym( handle, "__wine_main" );
        if (init_func) init_func( argc, argv, environ );
        fprintf( stderr, "wine: __wine_main function not found in ntdll.so\n" );
        exit(1);
    }

    fprintf( stderr, "wine: could not load ntdll.so: %s\n", dlerror() );
    pthread_detach( pthread_self() );  /* force importing libpthread for OpenGL */
    exit(1);
}
