/*
 * Server-side D3DKMT resource management
 *
 * Copyright 2024 Rémi Bernon for CodeWeavers
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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "ddk/wdm.h"

#include "file.h"
#include "handle.h"
#include "request.h"
#include "security.h"

static const WCHAR d3dkmt_object_name[] = {'D','3','D','K','M','T'};

struct type_descr d3dkmt_object_type =
{
    { d3dkmt_object_name, sizeof(d3dkmt_object_name) },        /* name */
    STANDARD_RIGHTS_REQUIRED|SYNCHRONIZE|SPECIFIC_RIGHTS_ALL,  /* valid_access */
    {                                                          /* mapping */
        STANDARD_RIGHTS_READ,
        STANDARD_RIGHTS_WRITE,
        STANDARD_RIGHTS_EXECUTE,
        STANDARD_RIGHTS_REQUIRED|SYNCHRONIZE|SPECIFIC_RIGHTS_ALL
    },
};

struct d3dkmt_object
{
    struct object obj;
    enum d3dkmt_type type;
    d3dkmt_handle_t global;
    struct d3dkmt_desc desc;
    struct fd *fd;
    mem_size_t private_size;
    void *private;
};

static void d3dkmt_object_dump( struct object *obj, int verbose );
static struct fd *d3dkmt_object_get_fd( struct object *obj );
static void d3dkmt_object_destroy( struct object *obj );

static const struct object_ops d3dkmt_object_ops =
{
    sizeof(struct d3dkmt_object),   /* size */
    &d3dkmt_object_type,            /* type */
    d3dkmt_object_dump,             /* dump */
    no_add_queue,                   /* add_queue */
    NULL,                           /* remove_queue */
    NULL,                           /* signaled */
    NULL,                           /* satisfied */
    no_signal,                      /* signal */
    d3dkmt_object_get_fd,           /* get_fd */
    default_map_access,             /* map_access */
    default_get_sd,                 /* get_sd */
    default_set_sd,                 /* set_sd */
    no_get_full_name,               /* get_full_name */
    no_lookup_name,                 /* lookup_name */
    no_link_name,                   /* link_name */
    NULL,                           /* unlink_name */
    no_open_file,                   /* open_file */
    no_kernel_obj_list,             /* get_kernel_obj_list */
    no_close_handle,                /* close_handle */
    d3dkmt_object_destroy,          /* destroy */
};

static const WCHAR d3dkmt_shared_name[] = {'D','3','D','K','M','T'};

struct type_descr d3dkmt_shared_type =
{
    { d3dkmt_shared_name, sizeof(d3dkmt_shared_name) },        /* name */
    STANDARD_RIGHTS_REQUIRED|SYNCHRONIZE|SPECIFIC_RIGHTS_ALL,  /* valid_access */
    {                                                          /* mapping */
        STANDARD_RIGHTS_READ,
        STANDARD_RIGHTS_WRITE,
        STANDARD_RIGHTS_EXECUTE,
        STANDARD_RIGHTS_REQUIRED|SYNCHRONIZE|SPECIFIC_RIGHTS_ALL
    },
};

struct d3dkmt_shared
{
    struct object obj;
    struct d3dkmt_object *resource;
    struct d3dkmt_object *keyed_mutex;
    struct d3dkmt_object *sync_object;
};

static void d3dkmt_shared_dump( struct object *obj, int verbose );
static void d3dkmt_shared_destroy( struct object *obj );

static const struct object_ops d3dkmt_shared_ops =
{
    sizeof(struct d3dkmt_shared),   /* size */
    &d3dkmt_shared_type,            /* type */
    d3dkmt_shared_dump,             /* dump */
    no_add_queue,                   /* add_queue */
    NULL,                           /* remove_queue */
    NULL,                           /* signaled */
    NULL,                           /* satisfied */
    no_signal,                      /* signal */
    no_get_fd,                      /* get_fd */
    default_map_access,             /* map_access */
    default_get_sd,                 /* get_sd */
    default_set_sd,                 /* set_sd */
    no_get_full_name,               /* get_full_name */
    no_lookup_name,                 /* lookup_name */
    no_link_name,                   /* link_name */
    NULL,                           /* unlink_name */
    no_open_file,                   /* open_file */
    no_kernel_obj_list,             /* get_kernel_obj_list */
    no_close_handle,                /* close_handle */
    d3dkmt_shared_destroy,          /* destroy */
};

static void d3dkmt_shared_dump( struct object *obj, int verbose )
{
    struct d3dkmt_shared *object = (struct d3dkmt_shared *)obj;
    fprintf( stderr, "resource=%p keyed_mutex=%p sync_object=%p\n", object->resource, object->keyed_mutex, object->sync_object );
}

static void d3dkmt_shared_destroy( struct object *obj )
{
    struct d3dkmt_shared *object = (struct d3dkmt_shared *)obj;
    if (object->resource) release_object( object->resource );
    if (object->keyed_mutex) release_object( object->keyed_mutex );
    if (object->sync_object) release_object( object->sync_object );
}

static struct d3dkmt_object **objects;
static unsigned int object_count, object_capacity, next_index;

/* return the position of the first object whose handle is not less than the
 * given global handle. */
static unsigned int lookup_d3dkmt_object_pos( d3dkmt_handle_t global )
{
    unsigned int begin = 0, end = object_count, mid;

    while (begin < end)
    {
        mid = begin + (end - begin) / 2;
        if (objects[mid]->global < global) begin = mid + 1;
        else end = mid;
    }

    return begin;
}

/* allocate a d3dkmt object with a global handle */
static d3dkmt_handle_t alloc_object_handle( struct d3dkmt_object *object )
{
    d3dkmt_handle_t handle = 0;
    unsigned int index;

    if (object_count >= object_capacity)
    {
        unsigned int capacity = max( 32, object_capacity * 3 / 2 );
        struct d3dkmt_object **tmp;
        assert( capacity > object_capacity );

        if (capacity >= 0xffff) goto done;
        if (!(tmp = realloc( objects, capacity * sizeof(*objects) ))) goto done;
        object_capacity = capacity;
        objects = tmp;
    }

    handle = object->global = ((next_index++ << 6) & ~0xc000003f) | 0x40000002;
    index = lookup_d3dkmt_object_pos( object->global );
    if (index < object_count) memmove( objects + index + 1, objects, (object_count - index) * sizeof(*object) );
    objects[index] = object;
    object_count++;

done:
    return handle;
}

/* free a d3dkmt global object handle */
static void free_object_handle( struct d3dkmt_object *object )
{
    unsigned int index;

    index = lookup_d3dkmt_object_pos( object->global );
    assert( index < object_count && objects[index] == object );
    object_count--;
    memmove( objects + index, objects + index + 1, (object_count - index) * sizeof(*object) );
}

static void d3dkmt_object_dump( struct object *obj, int verbose )
{
    struct d3dkmt_object *object = (struct d3dkmt_object *)obj;
    fprintf( stderr, "global=%#x\n", object->global );
}

static struct fd *d3dkmt_object_get_fd( struct object *obj )
{
    struct d3dkmt_object *object = (struct d3dkmt_object *)obj;
    assert( obj->ops == &d3dkmt_object_ops );

    if (object->fd) return (struct fd *)grab_object( object->fd );
    set_error( STATUS_OBJECT_TYPE_MISMATCH );
    return NULL;
}

static void d3dkmt_object_destroy( struct object *obj )
{
    struct d3dkmt_object *object = (struct d3dkmt_object *)obj;
    if (object->global) free_object_handle( object );
    if (object->fd) release_object( object->fd );
}

static struct d3dkmt_object *create_d3dkmt_object( enum d3dkmt_type type, const void *private, mem_size_t private_size )
{
    struct d3dkmt_object *object;

    if (!(object = alloc_object( &d3dkmt_object_ops ))) return NULL;
    object->type = type;
    object->fd = NULL;
    object->private = memdup( private, private_size );
    object->private_size = private_size;

    if (!alloc_object_handle( object ))
    {
        release_object( object );
        set_error( STATUS_NO_MEMORY );
        return NULL;
    }

    return object;
}

/* return a pointer to a d3dkmt object from its global handle */
static void *open_d3dkmt_object( d3dkmt_handle_t global, enum d3dkmt_type type )
{
    struct d3dkmt_object *object;
    unsigned int index;

    index = lookup_d3dkmt_object_pos( global );
    if (index >= object_count) object = NULL;
    else object = objects[index];

    if (!object || object->global != global || object->type != type)
    {
        set_error( STATUS_INVALID_PARAMETER );
        return NULL;
    }
    return grab_object( object );
}

static struct d3dkmt_shared *get_d3dkmt_shared( struct process *process, obj_handle_t handle, unsigned int access )
{
    return (struct d3dkmt_shared *)get_handle_obj( process, handle, access, &d3dkmt_shared_ops );
}

/* create a global d3dkmt object */
DECL_HANDLER(create_d3dkmt_object)
{
    struct d3dkmt_object *object;

    if (!(object = create_d3dkmt_object( req->type, get_req_data(), get_req_data_size() ))) return;
    reply->handle = alloc_handle( current->process, object, 0, OBJ_INHERIT );
    reply->global = object->global;
    release_object( object );
}

/* open a global d3dkmt object */
DECL_HANDLER(open_d3dkmt_object)
{
    struct d3dkmt_object *object;

    if (!(object = open_d3dkmt_object( req->global, req->type ))) return;
    reply->handle = alloc_handle( current->process, object, 0, OBJ_INHERIT );
    if (object->private_size > get_reply_max_size()) set_error( STATUS_INVALID_PARAMETER );
    else
    {
        set_reply_data( object->private, object->private_size );
        reply->private_size = object->private_size;
    }
    release_object( object );
}

/* share global d3dkmt objects together */
DECL_HANDLER(share_d3dkmt_objects)
{
    struct d3dkmt_object *resource = NULL, *keyed_mutex = NULL, *sync_object = NULL;
    struct d3dkmt_shared *object;

    if (req->resource && !(resource = open_d3dkmt_object( req->resource, D3DKMT_TYPE_RESOURCE ))) goto done;
    if (req->keyed_mutex && !(keyed_mutex = open_d3dkmt_object( req->keyed_mutex, D3DKMT_TYPE_KEYED_MUTEX ))) goto done;
    if (req->sync_object && !(sync_object = open_d3dkmt_object( req->sync_object, D3DKMT_TYPE_KEYED_MUTEX ))) goto done;

    if (!(object = alloc_object( &d3dkmt_shared_ops ))) goto done;
    if ((object->sync_object = sync_object)) grab_object( sync_object );
    if ((object->keyed_mutex = keyed_mutex)) grab_object( keyed_mutex );
    if ((object->resource = resource)) grab_object( resource );
    reply->handle = alloc_handle( current->process, object, 0, OBJ_INHERIT );
    release_object( object );

done:
    if (sync_object) release_object( sync_object );
    if (keyed_mutex) release_object( keyed_mutex );
    if (resource) release_object( resource );
}

/* get a single shared d3dkmt object */
DECL_HANDLER(get_shared_d3dkmt_object)
{
    struct d3dkmt_shared *shared;
    struct d3dkmt_object *object;

    if (!(shared = get_d3dkmt_shared( current->process, req->handle, 0 ))) return;

    if (req->type == D3DKMT_TYPE_RESOURCE) object = shared->resource;
    else if (req->type == D3DKMT_TYPE_KEYED_MUTEX) object = shared->keyed_mutex;
    else if (req->type == D3DKMT_TYPE_SYNC_OBJECT) object = shared->sync_object;
    else object = NULL;

    if (!object) set_error( STATUS_INVALID_PARAMETER );
    else if (object->private_size > get_reply_max_size()) set_error( STATUS_INVALID_PARAMETER );
    else
    {
        reply->handle = alloc_handle( current->process, object, 0, OBJ_INHERIT );
        reply->global = object->global;
        set_reply_data( object->private, object->private_size );
        reply->private_size = object->private_size;
    }

    release_object( shared );
}

/* set the object fd and descriptor */
DECL_HANDLER(set_d3dkmt_object_fd)
{
    struct d3dkmt_object *object;
    struct fd *fd;

    if (!(fd = get_handle_fd_obj( current->process, req->handle, 0 )))
    {
        set_error( STATUS_INVALID_HANDLE );
        return;
    }

    if ((object = open_d3dkmt_object( req->global, req->type )))
    {
        if (object->desc.type) set_error( STATUS_INVALID_HANDLE );
        else if (object->desc.type != req->desc.type) set_error( STATUS_INVALID_PARAMETER );
        else
        {
            object->fd = (struct fd *)grab_object( fd );
            object->desc = req->desc;
        }
        release_object( object );
    }

    release_object( fd );
}

/* get the object fd and descriptor */
DECL_HANDLER(get_d3dkmt_object_fd)
{
    struct d3dkmt_object *object;

    if (!(object = open_d3dkmt_object( req->global, req->type ))) return;

    if (!object->desc.type) set_error( STATUS_INVALID_PARAMETER );
    else
    {
        reply->handle = alloc_handle( current->process, object, 0, OBJ_INHERIT );
        reply->desc = object->desc;
    }

    release_object( object );
}

/* lock access to the resource */
DECL_HANDLER(lock_resource)
{
}

/* unlock access to the resource */
DECL_HANDLER(unlock_resource)
{
}
