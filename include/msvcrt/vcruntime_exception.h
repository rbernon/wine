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
void __cdecl __std_exception_copy(const struct __std_exception_data *src, struct __std_exception_data *dst);
void __cdecl __std_exception_destroy(struct __std_exception_data *data);
} /* extern "C" */

namespace std
{

class exception
{
private:
    struct __std_exception_data data;
public:
    exception() throw() : data() {}
    exception(exception const& other) throw()
    {
        __std_exception_copy(&other.data, &this->data);
    }
    explicit exception(char const* what) throw() : data()
    {
        struct __std_exception_data new_data = {const_cast<char *>(what), 0};
        __std_exception_copy(&new_data, &this->data);
    }
    exception& operator=(exception const& other) throw()
    {
        if (this != &other)
        {
            __std_exception_destroy(&this->data);
            __std_exception_copy(&other.data, &this->data);
        }
        return *this;
    }
    virtual ~exception() throw() { __std_exception_destroy(&this->data); }
    virtual char const* what() const throw() { return this->data.what ? this->data.what : "unknown exception"; }
};

class bad_alloc : public exception
{
private:
    friend class bad_array_new_length;
    bad_alloc(char const* const what) throw() : exception(what) {}
public:
    bad_alloc() throw() : exception("bad allocation") {}
};

class bad_array_new_length : public bad_alloc
{
public:
    bad_array_new_length() noexcept : bad_alloc("bad array new length") {}
};

} /* namespace std */

#endif /* __cplusplus */

#endif /* __WINE_VCRUNTIME_EXCEPTION_H */
