/*
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

#ifndef __WINE_VCRUNTIME_TYPEINFO_H
#define __WINE_VCRUNTIME_TYPEINFO_H

#include "vcruntime_exception.h"

struct __std_type_info_data
{
    char *name;
    char mangled[1];
};

#ifdef __cplusplus

struct __type_info_node {};
extern struct __type_info_node __type_info_root_node;

extern "C"
{
extern int __cdecl __std_type_info_compare(struct __std_type_info_data const* l, struct __std_type_info_data const* r);
extern size_t __cdecl __std_type_info_hash(struct __std_type_info_data const* ti);
extern char const* __cdecl __std_type_info_name(struct __std_type_info_data* ti, struct __type_info_node* header);
}

class type_info
{
public:
    size_t hash_code() const throw()
    {
        return __std_type_info_hash(&data);
    }
    bool operator==(type_info const& other) const
    {
        return __std_type_info_compare(&data, &other.data) == 0;
    }
    bool operator!=(type_info const& other) const
    {
        return __std_type_info_compare(&data, &other.data) != 0;
    }
    bool before(type_info const& other) const
    {
        return __std_type_info_compare(&data, &other.data) < 0;
    }
    char const* name() const
    {
        return __std_type_info_name(&data, &__type_info_root_node);
    }
    char const* raw_name() const
    {
        return data.mangled;
    }
    virtual ~type_info() throw();

private:
    type_info(type_info const&) = delete;
    type_info& operator=(type_info const&) = delete;
    mutable struct __std_type_info_data data;
};

namespace std
{
using ::type_info;
}

#endif /* __cplusplus */

#endif /* __WINE_VCRUNTIME_TYPEINFO_H */
