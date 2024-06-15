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

#ifndef __WINE_VCRUNTIME_EXCEPTION_H
#define __WINE_VCRUNTIME_EXCEPTION_H

struct __std_exception_data
{
    char *what;
    char dofree;
};

#ifdef __cplusplus

extern "C"
{
void __cdecl __std_exception_copy(const struct __std_exception_data *src,
				  struct __std_exception_data *dst);
void __cdecl __std_exception_destroy(struct __std_exception_data *data);
}

namespace std
{

class exception
{
public:
    exception() noexcept : data() {}
    explicit exception(char const* what) noexcept : data()
    {
        struct __std_exception_data new_data = {const_cast<char *>(what), 0};
        __std_exception_copy(&new_data, &this->data);
    }
    exception& operator=(exception const& other) noexcept
    {
        __std_exception_copy(&other.data, &this->data);
        return *this;
    }
    exception(exception const& other) noexcept { *this = other; }
    virtual ~exception() noexcept { __std_exception_destroy(&this->data); }
    virtual char const* what() const noexcept { return this->data.what ? this->data.what : "Unknown exception"; }

private:
    struct __std_exception_data data;
};

class bad_alloc : public exception
{
public:
    bad_alloc() noexcept : exception("bad allocation") {}
private:
    friend class bad_array_new_length;
    bad_alloc(char const* const what) noexcept : exception(what) {}
};

class bad_array_new_length : public bad_alloc
{
public:
    bad_array_new_length() noexcept : bad_alloc("bad array new length") {}
};

}

#endif /* __cplusplus */

#endif /* __WINE_VCRUNTIME_EXCEPTION_H */
