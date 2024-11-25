/*
 * Definitions for the client side of the Wine server communication
 *
 * Copyright (C) 1998 Alexandre Julliard
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

#ifndef __WINE_WINE_SERVER_H
#define __WINE_WINE_SERVER_H

#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winternl.h>
#include <wine/server_protocol.h>
#include <wine/debug.h>

/* client communication functions */

struct __server_iovec
{
    const void  *ptr;
    data_size_t  size;
};

#define __SERVER_MAX_DATA 5

struct __server_request_info
{
    union
    {
        union generic_request req;    /* request structure */
        union generic_reply   reply;  /* reply structure */
    } u;
    unsigned int          data_count; /* count of request data pointers */
    void                 *reply_data; /* reply data pointer */
    struct __server_iovec data[__SERVER_MAX_DATA];  /* request variable size data */
};

NTSYSAPI unsigned int CDECL wine_server_call( void *req_ptr );
NTSYSAPI NTSTATUS CDECL wine_server_fd_to_handle( int fd, unsigned int access, unsigned int attributes, HANDLE *handle );
NTSYSAPI NTSTATUS CDECL wine_server_handle_to_fd( HANDLE handle, unsigned int access, int *unix_fd, unsigned int *options );

/* do a server call and set the last error code */
static inline unsigned int wine_server_call_err( void *req_ptr )
{
    unsigned int res = wine_server_call( req_ptr );
    if (res) RtlSetLastWin32Error( RtlNtStatusToDosError(res) );
    return res;
}

/* get the size of the variable part of the returned reply */
static inline data_size_t wine_server_reply_size( const void *reply )
{
    return ((const struct reply_header *)reply)->reply_size;
}

/* add some data to be sent along with the request */
static inline void wine_server_add_data( void *req_ptr, const void *ptr, data_size_t size )
{
    struct __server_request_info * const req = req_ptr;
    if (size)
    {
        req->data[req->data_count].ptr = ptr;
        req->data[req->data_count++].size = size;
        req->u.req.request_header.request_size += size;
    }
}

/* set the pointer and max size for the reply var data */
static inline void wine_server_set_reply( void *req_ptr, void *ptr, data_size_t max_size )
{
    struct __server_request_info * const req = req_ptr;
    req->reply_data = ptr;
    req->u.req.request_header.reply_size = max_size;
}

/* convert an object handle to a server handle */
static inline obj_handle_t wine_server_obj_handle( HANDLE handle )
{
    if ((int)(INT_PTR)handle != (INT_PTR)handle) return 0xfffffff0;  /* some invalid handle */
    return (INT_PTR)handle;
}

/* convert a user handle to a server handle */
static inline user_handle_t wine_server_user_handle( HANDLE handle )
{
    return (UINT_PTR)handle;
}

/* convert a server handle to a generic handle */
static inline HANDLE wine_server_ptr_handle( obj_handle_t handle )
{
    return (HANDLE)(INT_PTR)(int)handle;
}

/* convert a client pointer to a server client_ptr_t */
static inline client_ptr_t wine_server_client_ptr( const void *ptr )
{
    return (client_ptr_t)(ULONG_PTR)ptr;
}

/* convert a server client_ptr_t to a real pointer */
static inline void *wine_server_get_ptr( client_ptr_t ptr )
{
    return (void *)(ULONG_PTR)ptr;
}

/* convert a server struct rectangle to a RECT */
static inline RECT wine_server_get_rect( struct rectangle rectangle )
{
    RECT rect =
    {
        .left = rectangle.left,
        .top = rectangle.top,
        .right = rectangle.right,
        .bottom = rectangle.bottom,
    };
    return rect;
}

/* convert a RECT to a server struct rectangle */
static inline struct rectangle wine_server_rectangle( RECT rect )
{
    struct rectangle rectangle =
    {
        .left = rect.left,
        .top = rect.top,
        .right = rect.right,
        .bottom = rect.bottom,
    };
    return rectangle;
}

#ifdef STATUS_SUCCESS
/* create a struct security_descriptor and contained information in one contiguous piece of memory */
static inline unsigned int wine_server_object_attr( const OBJECT_ATTRIBUTES *attr, struct object_attributes **ret,
                                                    data_size_t *ret_len )
{
    unsigned int len = sizeof(**ret);
    SID *owner = NULL, *group = NULL;
    ACL *dacl = NULL, *sacl = NULL;
    SECURITY_DESCRIPTOR *sd;

    *ret = NULL;
    *ret_len = 0;

    if (!attr) return STATUS_SUCCESS;

    if (attr->Length != sizeof(*attr)) return STATUS_INVALID_PARAMETER;

    if ((sd = attr->SecurityDescriptor))
    {
        len += sizeof(struct security_descriptor);
        if (sd->Revision != SECURITY_DESCRIPTOR_REVISION) return STATUS_UNKNOWN_REVISION;
        if (sd->Control & SE_SELF_RELATIVE)
        {
            SECURITY_DESCRIPTOR_RELATIVE *rel = (SECURITY_DESCRIPTOR_RELATIVE *)sd;
            if (rel->Owner) owner = (PSID)((BYTE *)rel + rel->Owner);
            if (rel->Group) group = (PSID)((BYTE *)rel + rel->Group);
            if ((sd->Control & SE_SACL_PRESENT) && rel->Sacl) sacl = (PSID)((BYTE *)rel + rel->Sacl);
            if ((sd->Control & SE_DACL_PRESENT) && rel->Dacl) dacl = (PSID)((BYTE *)rel + rel->Dacl);
        }
        else
        {
            owner = sd->Owner;
            group = sd->Group;
            if (sd->Control & SE_SACL_PRESENT) sacl = sd->Sacl;
            if (sd->Control & SE_DACL_PRESENT) dacl = sd->Dacl;
        }

        if (owner) len += offsetof( SID, SubAuthority[owner->SubAuthorityCount] );
        if (group) len += offsetof( SID, SubAuthority[group->SubAuthorityCount] );
        if (sacl) len += sacl->AclSize;
        if (dacl) len += dacl->AclSize;

        /* fix alignment for the Unicode name that follows the structure */
        len = (len + sizeof(WCHAR) - 1) & ~(sizeof(WCHAR) - 1);
    }

    if (attr->ObjectName)
    {
        if ((ULONG_PTR)attr->ObjectName->Buffer & (sizeof(WCHAR) - 1)) return STATUS_DATATYPE_MISALIGNMENT;
        if (attr->ObjectName->Length & (sizeof(WCHAR) - 1)) return STATUS_OBJECT_NAME_INVALID;
        len += attr->ObjectName->Length;
    }
    else if (attr->RootDirectory) return STATUS_OBJECT_NAME_INVALID;

    len = (len + 3) & ~3;  /* DWORD-align the entire structure */

    if (!(*ret = calloc( len, 1 ))) return STATUS_NO_MEMORY;

    (*ret)->rootdir = wine_server_obj_handle( attr->RootDirectory );
    (*ret)->attributes = attr->Attributes;

    if (attr->SecurityDescriptor)
    {
        struct security_descriptor *descr = (struct security_descriptor *)(*ret + 1);
        unsigned char *ptr = (unsigned char *)(descr + 1);

        descr->control = sd->Control & ~SE_SELF_RELATIVE;
        if (owner) descr->owner_len = offsetof( SID, SubAuthority[owner->SubAuthorityCount] );
        if (group) descr->group_len = offsetof( SID, SubAuthority[group->SubAuthorityCount] );
        if (sacl) descr->sacl_len = sacl->AclSize;
        if (dacl) descr->dacl_len = dacl->AclSize;

        memcpy( ptr, owner, descr->owner_len );
        ptr += descr->owner_len;
        memcpy( ptr, group, descr->group_len );
        ptr += descr->group_len;
        memcpy( ptr, sacl, descr->sacl_len );
        ptr += descr->sacl_len;
        memcpy( ptr, dacl, descr->dacl_len );
        (*ret)->sd_len = (sizeof(*descr) + descr->owner_len + descr->group_len + descr->sacl_len +
                          descr->dacl_len + sizeof(WCHAR) - 1) & ~(sizeof(WCHAR) - 1);
    }

    if (attr->ObjectName)
    {
        unsigned char *ptr = (unsigned char *)(*ret + 1) + (*ret)->sd_len;
        (*ret)->name_len = attr->ObjectName->Length;
        memcpy( ptr, attr->ObjectName->Buffer, (*ret)->name_len );
    }

    *ret_len = len;
    return STATUS_SUCCESS;
}
#endif

/* macros for server requests */

#define SERVER_START_REQ(type) \
    do { \
        WINE_DECLARE_DEBUG_CHANNEL(client); \
        static const char *const __req_name = #type; \
        struct __server_request_info __req; \
        struct type##_request * const req = &__req.u.req.type##_request; \
        const struct type##_reply * const reply = &__req.u.reply.type##_reply; \
        memset( &__req.u.req, 0, sizeof(__req.u.req) ); \
        __req.u.req.request_header.req = REQ_##type; \
        __req.data_count = 0; \
        (void)reply; \
        TRACE_(client)("%s start\n", __req_name); \
        do

#define SERVER_END_REQ \
        while(0); \
        TRACE_(client)("%s end\n", __req_name); \
    } while(0)


#endif  /* __WINE_WINE_SERVER_H */
