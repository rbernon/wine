/*
 * Server-side USER handles
 *
 * Copyright (C) 2001 Alexandre Julliard
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

#include "object.h"
#include "thread.h"
#include "user.h"
#include "request.h"
#include "process.h"

struct user_handle
{
    void          *ptr;          /* pointer to object */
    unsigned short type;         /* object type (0 if free) */
    unsigned short generation;   /* generation counter */
    unsigned short pid;          /* owner process pid */
};

static struct user_handle *handles;
static struct user_handle *freelist;
static int nb_handles;
static int allocated_handles;

void *gdi_shared_handle_table;
static int gdi_nb_handles;
static int gdi_next_free = -1;

static int gdi_handle_get_kernel_data( struct gdi_handle *ptr )
{
    data_size_t ptr_size = get_prefix_ptr_size();
    return *(int *)((char *)ptr - ptr_size);
}

static void gdi_handle_set_kernel_data( struct gdi_handle *ptr, int data )
{
    data_size_t ptr_size = get_prefix_ptr_size();
    *(int *)((char *)ptr - ptr_size) = data;
    if (ptr_size == sizeof(int)) return;
    *(int *)((char *)ptr - ptr_size + sizeof(int)) = (data >= -1) ? 0 : ~0u;
}

static void gdi_handle_set_user_data( struct gdi_handle *ptr, client_ptr_t user )
{
    *(client_ptr_t *)((char *)ptr + sizeof(*ptr)) = user;
}

static struct gdi_handle *index_to_gdi_entry( unsigned int index )
{
    data_size_t ptr_size = get_prefix_ptr_size(), entry_size = sizeof(struct gdi_handle) + 2 * ptr_size;
    return (struct gdi_handle *)((char *)gdi_shared_handle_table + index * entry_size + ptr_size);
}

static unsigned int gdi_handle_to_index( struct gdi_handle *ptr )
{
    data_size_t ptr_size = get_prefix_ptr_size(), entry_size = sizeof(struct gdi_handle) + 2 * ptr_size;
    return (((char *)ptr - ptr_size) - (char *)gdi_shared_handle_table) / entry_size;
}

static struct gdi_handle *handle_to_gdi_entry( gdi_handle_t handle )
{
    struct gdi_handle *ptr;
    unsigned short upper;
    int index = (handle & 0xffff) - FIRST_GDI_HANDLE;
    if (index < 0) return NULL;
    ptr = index_to_gdi_entry( index );
    if (!ptr->type) return NULL;
    upper = handle >> 16;
    if (upper == ptr->upper || !upper || upper == 0xffff) return ptr;
    return NULL;
}

static inline gdi_handle_t gdi_entry_to_handle( struct gdi_handle *ptr )
{
    unsigned int index = gdi_handle_to_index( ptr );
    return (index + FIRST_GDI_HANDLE) | (ptr->upper << 16);
}

static inline struct gdi_handle *alloc_gdi_entry(void)
{
    struct gdi_handle *ptr;
    if (gdi_next_free == -1) return index_to_gdi_entry( gdi_nb_handles++ );
    ptr = index_to_gdi_entry( gdi_next_free );
    gdi_next_free = gdi_handle_get_kernel_data( ptr );
    return ptr;
}

static inline void free_gdi_entry( struct gdi_handle *ptr )
{
    struct process *process;
    if ((process = get_process_from_id( ptr->pid )))
    {
        process->nb_gdi_handle--;
        release_object( process );
    }

    ptr->pid = 0;
    ptr->type = 0;
    gdi_handle_set_kernel_data( ptr, gdi_next_free );
    gdi_handle_set_user_data( ptr, 0 );
    gdi_next_free = gdi_handle_to_index( ptr );
}

/* allocate a gdi handle for a given object */
static gdi_handle_t alloc_gdi_handle( struct process *process, enum gdi_object type )
{
    struct gdi_handle *ptr = alloc_gdi_entry();
    if (!ptr) return 0;
    ptr->pid = process->id;
    ptr->count = 0;
    if (++ptr->upper >= 0xffff) ptr->upper = 1;
    ptr->type = type;
    gdi_handle_set_kernel_data( ptr, 0x80000000 );
    gdi_handle_set_user_data( ptr, 0 );
    return gdi_entry_to_handle( ptr );
}

static struct user_handle *handle_to_entry( user_handle_t handle )
{
    unsigned short generation;
    int index = ((handle & 0xffff) - FIRST_USER_HANDLE) >> 1;
    if (index < 0 || index >= nb_handles) return NULL;
    if (!handles[index].type) return NULL;
    generation = handle >> 16;
    if (generation == handles[index].generation || !generation || generation == 0xffff)
        return &handles[index];
    return NULL;
}

static inline user_handle_t entry_to_handle( struct user_handle *ptr )
{
    unsigned int index = ptr - handles;
    return (index << 1) + FIRST_USER_HANDLE + (ptr->generation << 16);
}

static inline struct user_handle *alloc_user_entry(void)
{
    struct user_handle *handle;

    if (freelist)
    {
        handle = freelist;
        freelist = handle->ptr;
        return handle;
    }
    if (nb_handles >= allocated_handles)  /* need to grow the array */
    {
        struct user_handle *new_handles;
        /* grow array by 50% (but at minimum 32 entries) */
        int growth = max( 32, allocated_handles / 2 );
        int new_size = min( allocated_handles + growth, (LAST_USER_HANDLE-FIRST_USER_HANDLE+1) >> 1 );
        if (new_size <= allocated_handles) return NULL;
        if (!(new_handles = realloc( handles, new_size * sizeof(*handles) )))
            return NULL;
        handles = new_handles;
        allocated_handles = new_size;
    }
    handle = &handles[nb_handles++];
    handle->generation = 0;
    return handle;
}

static inline void *free_user_entry( struct user_handle *ptr )
{
    void *ret;
    struct process *process;

    if ((process = get_process_from_id( ptr->pid )))
    {
        process->nb_user_handle--;
        release_object( process );
    }

    ret = ptr->ptr;
    ptr->ptr  = freelist;
    ptr->type = 0;
    freelist  = ptr;
    return ret;
}

/* allocate a user handle for a given object */
user_handle_t alloc_user_handle( void *ptr, enum user_obj_type type )
{
    struct user_handle *entry = alloc_user_entry();
    if (!entry) return 0;
    entry->ptr  = ptr;
    entry->type = type;
    entry->pid  = 0;
    if (++entry->generation >= 0xffff) entry->generation = 1;
    return entry_to_handle( entry );
}

/* return a pointer to a user object from its handle */
void *get_user_object( user_handle_t handle, enum user_obj_type type )
{
    struct user_handle *entry;

    if (!(entry = handle_to_entry( handle )) || entry->type != type) return NULL;
    return entry->ptr;
}

/* get the full handle for a possibly truncated handle */
user_handle_t get_user_full_handle( user_handle_t handle )
{
    struct user_handle *entry;

    if (handle >> 16) return handle;
    if (!(entry = handle_to_entry( handle ))) return handle;
    return entry_to_handle( entry );
}

/* same as get_user_object plus set the handle to the full 32-bit value */
void *get_user_object_handle( user_handle_t *handle, enum user_obj_type type )
{
    struct user_handle *entry;

    if (!(entry = handle_to_entry( *handle )) || entry->type != type) return NULL;
    *handle = entry_to_handle( entry );
    return entry->ptr;
}

/* free a user handle and return a pointer to the object */
void *free_user_handle( user_handle_t handle )
{
    struct user_handle *entry;

    if (!(entry = handle_to_entry( handle )))
    {
        set_error( STATUS_INVALID_HANDLE );
        return NULL;
    }
    return free_user_entry( entry );
}

/* return the next user handle after 'handle' that is of a given type */
void *next_user_handle( user_handle_t *handle, enum user_obj_type type )
{
    struct user_handle *entry;

    if (!*handle) entry = handles;
    else
    {
        int index = ((*handle & 0xffff) - FIRST_USER_HANDLE) >> 1;
        if (index < 0 || index >= nb_handles) return NULL;
        entry = handles + index + 1;  /* start from the next one */
    }
    while (entry < handles + nb_handles)
    {
        if (!type || entry->type == type)
        {
            *handle = entry_to_handle( entry );
            return entry->ptr;
        }
        entry++;
    }
    return NULL;
}

/* free client-side user handles managed by the process */
void free_process_user_handles( struct process *process )
{
    unsigned int i;

    for (i = 0; i < nb_handles; i++)
        if (handles[i].type == USER_CLIENT && handles[i].ptr == process)
            free_user_entry( &handles[i] );

    for (i = 0; i < gdi_nb_handles; i++)
    {
        struct gdi_handle *entry;
        if ((entry = index_to_gdi_entry(i)) && entry->pid == process->id)
            free_gdi_entry( entry );
    }
}

/* allocate an arbitrary user handle */
DECL_HANDLER(alloc_user_handle)
{
    struct user_handle *entry;

    reply->handle = alloc_user_handle( current->process, USER_CLIENT );
    if ((entry = handle_to_entry( reply->handle )))
    {
        entry->pid = current->process->id;
        current->process->nb_user_handle++;
        current->process->max_user_handle = max(current->process->max_user_handle, current->process->nb_user_handle);
    }
}


/* free an arbitrary user handle */
DECL_HANDLER(free_user_handle)
{
    struct user_handle *entry;

    if ((entry = handle_to_entry( req->handle )) && entry->type == USER_CLIENT)
        free_user_entry( entry );
    else
        set_error( STATUS_INVALID_HANDLE );
}


/* allocate an arbitrary gdi handle */
DECL_HANDLER(alloc_gdi_handle)
{
    reply->handle = alloc_gdi_handle( current->process, GDI_OBJECT );
    current->process->nb_gdi_handle++;
    current->process->max_gdi_handle = max(current->process->max_gdi_handle, current->process->nb_gdi_handle);
}


/* free an arbitrary gdi handle */
DECL_HANDLER(free_gdi_handle)
{
    struct gdi_handle *entry;

    if ((entry = handle_to_gdi_entry( req->handle )) && entry->type == GDI_OBJECT)
        free_gdi_entry( entry );
    else
        set_error( STATUS_INVALID_HANDLE );
}
