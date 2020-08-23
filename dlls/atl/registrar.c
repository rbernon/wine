/*
 * Copyright 2005 Jacek Caban
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

#define COBJMACROS

#include "atlbase.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(atl);

/**************************************************************
 * ATLRegistrar implementation
 */

static const struct {
    WCHAR name[22];
    HKEY  key;
} root_keys[] = {
    {L"HKEY_CLASSES_ROOT",     HKEY_CLASSES_ROOT},
    {L"HKEY_CURRENT_USER",     HKEY_CURRENT_USER},
    {L"HKEY_LOCAL_MACHINE",    HKEY_LOCAL_MACHINE},
    {L"HKEY_USERS",            HKEY_USERS},
    {L"HKEY_PERFORMANCE_DATA", HKEY_PERFORMANCE_DATA},
    {L"HKEY_DYN_DATA",         HKEY_DYN_DATA},
    {L"HKEY_CURRENT_CONFIG",   HKEY_CURRENT_CONFIG},
    {L"HKCR",                  HKEY_CLASSES_ROOT},
    {L"HKCU",                  HKEY_CURRENT_USER},
    {L"HKLM",                  HKEY_LOCAL_MACHINE},
    {L"HKU",                   HKEY_USERS},
    {L"HKPD",                  HKEY_PERFORMANCE_DATA},
    {L"HKDD",                  HKEY_DYN_DATA},
    {L"HKCC",                  HKEY_CURRENT_CONFIG}
};

typedef struct rep_list_str {
    LPOLESTR key;
    LPOLESTR item;
    int key_len;
    struct rep_list_str *next;
} rep_list;

typedef struct {
    IRegistrar IRegistrar_iface;
    LONG ref;
    rep_list *rep;
} Registrar;

typedef struct {
    LPOLESTR str;
    DWORD alloc;
    DWORD len;
} strbuf;

static inline Registrar *impl_from_IRegistrar(IRegistrar *iface)
{
    return CONTAINING_RECORD(iface, Registrar, IRegistrar_iface);
}

static void strbuf_init(strbuf *buf)
{
    buf->str = HeapAlloc(GetProcessHeap(), 0, 128*sizeof(WCHAR));
    buf->alloc = 128;
    buf->len = 0;
}

static void strbuf_write(LPCOLESTR str, strbuf *buf, int len)
{
    if(len == -1)
        len = lstrlenW(str);
    if(buf->len+len+1 >= buf->alloc) {
        buf->alloc = (buf->len+len)<<1;
        buf->str = HeapReAlloc(GetProcessHeap(), 0, buf->str, buf->alloc*sizeof(WCHAR));
    }
    memcpy(buf->str+buf->len, str, len*sizeof(OLECHAR));
    buf->len += len;
    buf->str[buf->len] = '\0';
}

static HRESULT get_word(LPCOLESTR *str, strbuf *buf)
{
    LPCOLESTR iter, iter2 = *str;

    buf->len = 0;
    buf->str[0] = '\0';

    while(iswspace(*iter2))
        iter2++;
    iter = iter2;
    if(!*iter) {
        *str = iter;
        return S_OK;
    }

    if(*iter == '}' || *iter == '=') {
        strbuf_write(iter++, buf, 1);
    }else if(*iter == '\'') {
        iter2 = ++iter;
        iter = wcschr(iter, '\'');
        if(!iter) {
            WARN("Unexpected end of script\n");
            *str = iter;
            return DISP_E_EXCEPTION;
        }
        strbuf_write(iter2, buf, iter-iter2);
        iter++;
    }else {
        while(*iter && !iswspace(*iter))
            iter++;
        strbuf_write(iter2, buf, iter-iter2);
    }

    while(iswspace(*iter))
        iter++;
    *str = iter;
    return S_OK;
}

static HRESULT do_preprocess(const Registrar *This, LPCOLESTR data, strbuf *buf)
{
    LPCOLESTR iter, iter2 = data;
    rep_list *rep_iter;

    iter = wcschr(data, '%');
    while(iter) {
        strbuf_write(iter2, buf, iter-iter2);

        iter2 = ++iter;
        if(!*iter2)
            return DISP_E_EXCEPTION;
        iter = wcschr(iter2, '%');
        if(!iter)
            return DISP_E_EXCEPTION;

        if(iter == iter2) {
            strbuf_write(L"%", buf, 1);
        }else {
            for(rep_iter = This->rep; rep_iter; rep_iter = rep_iter->next) {
                if(rep_iter->key_len == iter-iter2
                        && !wcsnicmp(iter2, rep_iter->key, rep_iter->key_len))
                    break;
            }
            if(!rep_iter) {
                WARN("Could not find replacement: %s\n", debugstr_wn(iter2, iter-iter2));
                return DISP_E_EXCEPTION;
            }

            strbuf_write(rep_iter->item, buf, -1);
        }

        iter2 = ++iter;
        iter = wcschr(iter, '%');
    }

    strbuf_write(iter2, buf, -1);
    TRACE("%s\n", debugstr_w(buf->str));

    return S_OK;
}

typedef struct {
    char *str;
    DWORD alloc;
    DWORD len;
} outbuf;

static void outbuf_init(outbuf *buf)
{
    buf->str = HeapAlloc(GetProcessHeap(), 0, 128*sizeof(WCHAR));
    buf->alloc = 128;
    buf->len = 0;
}

static void outbuf_append(outbuf *buf, const char *str, int len)
{
    if(len == -1) len = strlen(str);
    if(buf->len + len + 1 >= buf->alloc) {
        buf->alloc = (buf->len + len) * 2;
        buf->str = HeapReAlloc(GetProcessHeap(), 0, buf->str, buf->alloc);
    }
    memcpy(buf->str + buf->len, str, len);
    buf->len += len;
    buf->str[buf->len] = '\0';
}

static void output_escaped(outbuf *buf, WCHAR *str, int n)
{
    static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    char buffer[1024], *dst;
    while (n > 0)
    {
        dst = buffer;
        while (n-- > 0 && dst <= buffer + sizeof(buffer) - 10)
        {
            WCHAR c = *str++;
            switch (c)
            {
            case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
            case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
            case '\t': *dst++ = '\\'; *dst++ = 't'; break;
            case '"':  *dst++ = '\\'; *dst++ = '"'; break;
            case ']':  *dst++ = '\\'; *dst++ = ']'; break;
            case '[':  *dst++ = '\\'; *dst++ = '['; break;
            case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
            default:
                if (c < ' ' || c >= 127)
                {
                    *dst++ = '\\';
                    *dst++ = hex[(c >> 12) & 0x0f];
                    *dst++ = hex[(c >> 8) & 0x0f];
                    *dst++ = hex[(c >> 4) & 0x0f];
                    *dst++ = hex[c & 0x0f];
                }
                else *dst++ = (char)c;
            }
        }
        *dst = 0;
        outbuf_append(buf, buffer, dst - buffer);
    }
}

static void output_delete_key(outbuf *buf, strbuf *path)
{
    outbuf_append(buf, "[-", 2);
    output_escaped(buf, path->str, path->len);
    outbuf_append(buf, "]\n", 2);
}

static void output_key(outbuf *buf, strbuf *path)
{
    outbuf_append(buf, "[", 1);
    output_escaped(buf, path->str, path->len);
    outbuf_append(buf, "]\n", 2);
}

static void output_value_string(outbuf *buf, strbuf *name, strbuf *value)
{
    if (!name->len) outbuf_append(buf, "@=", 2);
    else
    {
        outbuf_append(buf, "\"", 1);
        output_escaped(buf, name->str, name->len);
        outbuf_append(buf, "\"=", 2);
    }
    outbuf_append(buf, "\"", 1);
    output_escaped(buf, value->str, value->len);
    outbuf_append(buf, "\"\n", 2);
}

static void output_value_dword(outbuf *buf, strbuf *name, DWORD value)
{
    if (!name->len) outbuf_append(buf, "@=", 2);
    else
    {
        outbuf_append(buf, "\"", 1);
        output_escaped(buf, name->str, name->len);
        outbuf_append(buf, "\"=", 2);
    }
    outbuf_append(buf, "dword:00000000\n", 15);
    sprintf(buf->str + buf->len - 9, "%08x\n", value);
}

static void output_value_binary(outbuf *buf, strbuf *name, strbuf *value)
{
    int i;
    char tmp;
    if (!name->len) outbuf_append(buf, "@=", 2);
    else
    {
        outbuf_append(buf, "\"", 1);
        output_escaped(buf, name->str, name->len);
        outbuf_append(buf, "\"=", 2);
    }
    outbuf_append(buf, "hex:", 4);
    for (i = 0; i < value->len; i += 2)
    {
        tmp = value->str[i];
        outbuf_append(buf, &tmp, 1);
        if (i == value->len - 1) break;
        tmp = value->str[i + 1];
        outbuf_append(buf, &tmp, 1);
        if (i <= value->len - 3) outbuf_append(buf, ",", 1);
    }
    outbuf_append(buf, "\n", 1);
}

static HRESULT do_process_key(LPCOLESTR *pstr, HKEY parent_key, strbuf *parent_path,
                              strbuf *buf, BOOL do_register, outbuf *out)
{
    LPCOLESTR iter;
    HRESULT hres;
    HKEY hkey = 0;
    strbuf name;
    strbuf path;

    enum {
        NORMAL,
        NO_REMOVE,
        IS_VAL,
        FORCE_REMOVE,
        DO_DELETE
    } key_type = NORMAL;

    iter = *pstr;
    hres = get_word(&iter, buf);
    if(FAILED(hres))
        return hres;
    strbuf_init(&name);
    strbuf_init(&path);
    if(parent_path) {
        strbuf_write(parent_path->str, &path, parent_path->len);
        strbuf_write(L"\\", &path, 1);
    }

    while(buf->str[1] || buf->str[0] != '}') {
        key_type = NORMAL;
        if(!lstrcmpiW(buf->str, L"NoRemove"))
            key_type = NO_REMOVE;
        else if(!lstrcmpiW(buf->str, L"ForceRemove"))
            key_type = FORCE_REMOVE;
        else if(!lstrcmpiW(buf->str, L"val"))
            key_type = IS_VAL;
        else if(!lstrcmpiW(buf->str, L"Delete"))
            key_type = DO_DELETE;

        if(key_type != NORMAL) {
            hres = get_word(&iter, buf);
            if(FAILED(hres))
                break;
        }
        TRACE("name = %s\n", debugstr_w(buf->str));

        if(parent_path) path.len = parent_path->len + 1;
        else path.len = 0;
        strbuf_write(buf->str, &path, -1);

        if(do_register) {
            if(key_type == IS_VAL) {
                hkey = parent_key;
                output_key(out, parent_path);
                strbuf_write(buf->str, &name, -1);
            }else if(key_type == DO_DELETE) {
                TRACE("Deleting %s\n", debugstr_w(buf->str));
                output_delete_key(out, &path);
            }else {
                if(key_type == FORCE_REMOVE)
                    output_delete_key(out, &path);
                if (*iter == '=') output_key(out, &path);
            }
        }else if(key_type != IS_VAL && key_type != DO_DELETE) {
            strbuf_write(buf->str, &name, -1);
        }

        if(key_type != DO_DELETE && *iter == '=') {
            iter++;
            hres = get_word(&iter, buf);
            if(FAILED(hres))
                break;
            if(buf->len != 1) {
                WARN("Wrong registry type: %s\n", debugstr_w(buf->str));
                hres = DISP_E_EXCEPTION;
                break;
            }
            if(do_register) {
                switch(buf->str[0]) {
                case 's':
                    hres = get_word(&iter, buf);
                    if(FAILED(hres))
                        break;
                    output_value_string(out, &name, buf);
                    break;
                case 'd': {
                    DWORD dw;
                    hres = get_word(&iter, buf);
                    if(FAILED(hres))
                        break;
                    dw = wcstol(buf->str, NULL, 10);
                    output_value_dword(out, &name, dw);
                    break;
                }
                case 'b': {
                    hres = get_word(&iter, buf);
                    if(FAILED(hres))
                        break;
                    output_value_binary(out, &name, buf);
                    break;
                }
                default:
                    WARN("Wrong resource type: %s\n", debugstr_w(buf->str));
                    hres = DISP_E_EXCEPTION;
                };
                if(FAILED(hres))
                    break;
            }else {
                if(*iter == '-')
                    iter++;
                hres = get_word(&iter, buf);
                if(FAILED(hres))
                    break;
            }
        }else if(key_type == IS_VAL) {
            WARN("value not set!\n");
            hres = DISP_E_EXCEPTION;
            break;
        }

        if(key_type != IS_VAL && key_type != DO_DELETE && *iter == '{' && iswspace(iter[1])) {
            hres = get_word(&iter, buf);
            if(FAILED(hres))
                break;
            hres = do_process_key(&iter, hkey, &path, buf, do_register, out);
            if(FAILED(hres))
                break;
        }

        TRACE("%x %x\n", do_register, key_type);
        if(!do_register && (key_type == NORMAL || key_type == FORCE_REMOVE)) {
            TRACE("Deleting %s\n", debugstr_w(name.str));
        }

        hkey = 0;
        name.len = 0;

        hres = get_word(&iter, buf);
        if(FAILED(hres))
            break;
    }

    HeapFree(GetProcessHeap(), 0, path.str);
    HeapFree(GetProcessHeap(), 0, name.str);
    *pstr = iter;
    return hres;
}

static BOOL set_privileges(LPCSTR privilege, BOOL set)
{
    TOKEN_PRIVILEGES tp;
    HANDLE token;
    LUID luid;
    BOOL ret = FALSE;

    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token)) return FALSE;
    if(!LookupPrivilegeValueA(NULL, privilege, &luid)) goto done;

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (set) tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges(token, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    if (GetLastError() != ERROR_SUCCESS) goto done;

    ret = TRUE;

done:
    CloseHandle(token);
    return ret;
}

static HRESULT do_process_root_key(LPCOLESTR data, BOOL do_register)
{
    LPCOLESTR iter = data;
    strbuf buf;
    HRESULT hres;
    unsigned int i;
    DWORD written;
    WCHAR tmppath_w[MAX_PATH];
    HANDLE file;
    outbuf out;

    strbuf_init(&buf);
    outbuf_init(&out);
    hres = get_word(&iter, &buf);
    if(FAILED(hres))
        return hres;

    set_privileges(SE_RESTORE_NAME, TRUE);

    swprintf(tmppath_w, MAX_PATH, L"c:\\windows\\temp\\wine-registrar-%x-%x.reg",
             GetCurrentProcessId(), GetCurrentThreadId());

    while(*iter) {
        if(!buf.len) {
            WARN("ward.len == 0, failed\n");
            hres = DISP_E_EXCEPTION;
            break;
        }
        for(i=0; i<ARRAY_SIZE(root_keys); i++) {
            if(!lstrcmpiW(buf.str, root_keys[i].name))
                break;
        }
        if(i == ARRAY_SIZE(root_keys)) {
            WARN("Wrong root key name: %s\n", debugstr_w(buf.str));
            hres = DISP_E_EXCEPTION;
            break;
        }
        hres = get_word(&iter, &buf);
        if(FAILED(hres))
            break;
        if(buf.str[1] || buf.str[0] != '{') {
            WARN("Failed, expected '{', got %s\n", debugstr_w(buf.str));
            hres = DISP_E_EXCEPTION;
            break;
        }

        out.len = 0;
        outbuf_append(&out, "WINE REGISTRY Version 2\n", 24);
        hres = do_process_key(&iter, root_keys[i].key, NULL, &buf, do_register, &out);

        file = CreateFileW(tmppath_w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (file == INVALID_HANDLE_VALUE) break;
        WriteFile(file, out.str, out.len, &written, NULL);
        CloseHandle(file);

        RegLoadKeyW( root_keys[i].key, NULL, tmppath_w );
        if(FAILED(hres)) {
            WARN("Processing key failed: %08x\n", hres);
            break;
        }
        hres = get_word(&iter, &buf);
        if(FAILED(hres))
            break;
    }
    HeapFree(GetProcessHeap(), 0, out.str);
    HeapFree(GetProcessHeap(), 0, buf.str);
    return hres;
}

static HRESULT string_register(Registrar *This, LPCOLESTR data, BOOL do_register)
{
    strbuf buf;
    HRESULT hres;

    TRACE("(%p %s %x)\n", This, debugstr_w(data), do_register);

    strbuf_init(&buf);
    hres = do_preprocess(This, data, &buf);
    if(FAILED(hres)) {
        WARN("preprocessing failed!\n");
        HeapFree(GetProcessHeap(), 0, buf.str);
        return hres;
    }

    hres = do_process_root_key(buf.str, do_register);
    if(FAILED(hres) && do_register)
        do_process_root_key(buf.str, FALSE);

    HeapFree(GetProcessHeap(), 0, buf.str);
    return hres;
}

static HRESULT resource_register(Registrar *This, LPCOLESTR resFileName,
                        LPCOLESTR szID, LPCOLESTR szType, BOOL do_register)
{
    HINSTANCE hins;
    HRSRC src;
    LPSTR regstra;
    LPWSTR regstrw;
    DWORD len, reslen;
    HRESULT hres;

    hins = LoadLibraryExW(resFileName, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if(hins) {
        src = FindResourceW(hins, szID, szType);
        if(src) {
            regstra = LoadResource(hins, src);
            reslen = SizeofResource(hins, src);
            if(regstra) {
                len = MultiByteToWideChar(CP_ACP, 0, regstra, reslen, NULL, 0)+1;
                regstrw = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len*sizeof(WCHAR));
                MultiByteToWideChar(CP_ACP, 0, regstra, reslen, regstrw, len);
                regstrw[len-1] = '\0';

                hres = string_register(This, regstrw, do_register);

                HeapFree(GetProcessHeap(), 0, regstrw);
            }else {
                WARN("could not load resource\n");
                hres = HRESULT_FROM_WIN32(GetLastError());
            }
        }else {
            WARN("Could not find source\n");
            hres = HRESULT_FROM_WIN32(GetLastError());
        }
        FreeLibrary(hins);
    }else {
        WARN("Could not load resource file\n");
        hres = HRESULT_FROM_WIN32(GetLastError());
    }

    return hres;
}

static HRESULT file_register(Registrar *This, LPCOLESTR fileName, BOOL do_register)
{
    HANDLE file;
    DWORD filelen, len;
    LPWSTR regstrw;
    LPSTR regstra;
    HRESULT hres;

    file = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if(file != INVALID_HANDLE_VALUE) {
        filelen = GetFileSize(file, NULL);
        regstra = HeapAlloc(GetProcessHeap(), 0, filelen);
        if(ReadFile(file, regstra, filelen, NULL, NULL)) {
            len = MultiByteToWideChar(CP_ACP, 0, regstra, filelen, NULL, 0)+1;
            regstrw = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len*sizeof(WCHAR));
            MultiByteToWideChar(CP_ACP, 0, regstra, filelen, regstrw, len);
            regstrw[len-1] = '\0';

            hres = string_register(This, regstrw, do_register);

            HeapFree(GetProcessHeap(), 0, regstrw);
        }else {
            WARN("Failed to read file %s\n", debugstr_w(fileName));
            hres = HRESULT_FROM_WIN32(GetLastError());
        }
        HeapFree(GetProcessHeap(), 0, regstra);
        CloseHandle(file);
    }else {
        WARN("Could not open file %s\n", debugstr_w(fileName));
        hres = HRESULT_FROM_WIN32(GetLastError());
    }

    return hres;
}

static HRESULT WINAPI Registrar_QueryInterface(IRegistrar *iface, REFIID riid, void **ppvObject)
{
    TRACE("(%p)->(%s %p\n", iface, debugstr_guid(riid), ppvObject);

    if(IsEqualGUID(&IID_IUnknown, riid)
       || IsEqualGUID(&IID_IRegistrar, riid)
       || IsEqualGUID(&IID_IRegistrarBase, riid)) {
        IRegistrar_AddRef(iface);
        *ppvObject = iface;
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI Registrar_AddRef(IRegistrar *iface)
{
    Registrar *This = impl_from_IRegistrar(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p) ->%d\n", This, ref);
    return ref;
}

static ULONG WINAPI Registrar_Release(IRegistrar *iface)
{
    Registrar *This = impl_from_IRegistrar(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ->%d\n", This, ref);
    if(!ref) {
        IRegistrar_ClearReplacements(iface);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

static HRESULT WINAPI Registrar_AddReplacement(IRegistrar *iface, LPCOLESTR Key, LPCOLESTR item)
{
    Registrar *This = impl_from_IRegistrar(iface);
    int len;
    rep_list *new_rep;

    TRACE("(%p)->(%s %s)\n", This, debugstr_w(Key), debugstr_w(item));

    new_rep = HeapAlloc(GetProcessHeap(), 0, sizeof(rep_list));

    new_rep->key_len  = lstrlenW(Key);
    new_rep->key = HeapAlloc(GetProcessHeap(), 0, (new_rep->key_len + 1) * sizeof(OLECHAR));
    memcpy(new_rep->key, Key, (new_rep->key_len+1)*sizeof(OLECHAR));

    len = lstrlenW(item)+1;
    new_rep->item = HeapAlloc(GetProcessHeap(), 0, len*sizeof(OLECHAR));
    memcpy(new_rep->item, item, len*sizeof(OLECHAR));

    new_rep->next = This->rep;
    This->rep = new_rep;

    return S_OK;
}

static HRESULT WINAPI Registrar_ClearReplacements(IRegistrar *iface)
{
    Registrar *This = impl_from_IRegistrar(iface);
    rep_list *iter, *iter2;

    TRACE("(%p)\n", This);

    if(!This->rep)
        return S_OK;

    iter = This->rep;
    while(iter) {
        iter2 = iter->next;
        HeapFree(GetProcessHeap(), 0, iter->key);
        HeapFree(GetProcessHeap(), 0, iter->item);
        HeapFree(GetProcessHeap(), 0, iter);
        iter = iter2;
    }

    This->rep = NULL;
    return S_OK;
}

static HRESULT WINAPI Registrar_ResourceRegisterSz(IRegistrar* iface, LPCOLESTR resFileName,
                LPCOLESTR szID, LPCOLESTR szType)
{
    Registrar *This = impl_from_IRegistrar(iface);
    TRACE("(%p)->(%s %s %s)\n", This, debugstr_w(resFileName), debugstr_w(szID), debugstr_w(szType));
    return resource_register(This, resFileName, szID, szType, TRUE);
}

static HRESULT WINAPI Registrar_ResourceUnregisterSz(IRegistrar* iface, LPCOLESTR resFileName,
                LPCOLESTR szID, LPCOLESTR szType)
{
    Registrar *This = impl_from_IRegistrar(iface);
    TRACE("(%p)->(%s %s %s)\n", This, debugstr_w(resFileName), debugstr_w(szID), debugstr_w(szType));
    return resource_register(This, resFileName, szID, szType, FALSE);
}

static HRESULT WINAPI Registrar_FileRegister(IRegistrar* iface, LPCOLESTR fileName)
{
    Registrar *This = impl_from_IRegistrar(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(fileName));
    return file_register(This, fileName, TRUE);
}

static HRESULT WINAPI Registrar_FileUnregister(IRegistrar* iface, LPCOLESTR fileName)
{
    Registrar *This = impl_from_IRegistrar(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(fileName));
    return file_register(This, fileName, FALSE);
}

static HRESULT WINAPI Registrar_StringRegister(IRegistrar* iface, LPCOLESTR data)
{
    Registrar *This = impl_from_IRegistrar(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(data));
    return string_register(This, data, TRUE);
}

static HRESULT WINAPI Registrar_StringUnregister(IRegistrar* iface, LPCOLESTR data)
{
    Registrar *This = impl_from_IRegistrar(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(data));
    return string_register(This, data, FALSE);
}

static HRESULT WINAPI Registrar_ResourceRegister(IRegistrar* iface, LPCOLESTR resFileName,
                UINT nID, LPCOLESTR szType)
{
    Registrar *This = impl_from_IRegistrar(iface);
    TRACE("(%p)->(%s %d %s)\n", iface, debugstr_w(resFileName), nID, debugstr_w(szType));
    return resource_register(This, resFileName, MAKEINTRESOURCEW(nID), szType, TRUE);
}

static HRESULT WINAPI Registrar_ResourceUnregister(IRegistrar* iface, LPCOLESTR resFileName,
                UINT nID, LPCOLESTR szType)
{
    Registrar *This = impl_from_IRegistrar(iface);
    TRACE("(%p)->(%s %d %s)\n", This, debugstr_w(resFileName), nID, debugstr_w(szType));
    return resource_register(This, resFileName, MAKEINTRESOURCEW(nID), szType, FALSE);
}

static const IRegistrarVtbl RegistrarVtbl = {
    Registrar_QueryInterface,
    Registrar_AddRef,
    Registrar_Release,
    Registrar_AddReplacement,
    Registrar_ClearReplacements,
    Registrar_ResourceRegisterSz,
    Registrar_ResourceUnregisterSz,
    Registrar_FileRegister,
    Registrar_FileUnregister,
    Registrar_StringRegister,
    Registrar_StringUnregister,
    Registrar_ResourceRegister,
    Registrar_ResourceUnregister,
};

/***********************************************************************
 *           AtlCreateRegistrar              [atl100.@]
 */
HRESULT WINAPI AtlCreateRegistrar(IRegistrar **ret)
{
    Registrar *registrar;

    registrar = HeapAlloc(GetProcessHeap(), 0, sizeof(*registrar));
    if(!registrar)
        return E_OUTOFMEMORY;

    registrar->IRegistrar_iface.lpVtbl = &RegistrarVtbl;
    registrar->ref = 1;
    registrar->rep = NULL;

    *ret = &registrar->IRegistrar_iface;
    return S_OK;
}

/***********************************************************************
 *           AtlUpdateRegistryFromResourceD         [atl100.@]
 */
HRESULT WINAPI AtlUpdateRegistryFromResourceD(HINSTANCE inst, LPCOLESTR res,
        BOOL bRegister, struct _ATL_REGMAP_ENTRY *pMapEntries, IRegistrar *pReg)
{
    const struct _ATL_REGMAP_ENTRY *iter;
    WCHAR module_name[MAX_PATH];
    IRegistrar *registrar;
    HRESULT hres;

    if(!GetModuleFileNameW(inst, module_name, MAX_PATH)) {
        FIXME("hinst %p: did not get module name\n", inst);
        return E_FAIL;
    }

    TRACE("%p (%s), %s, %d, %p, %p\n", inst, debugstr_w(module_name),
	debugstr_w(res), bRegister, pMapEntries, pReg);

    if(pReg) {
        registrar = pReg;
    }else {
        hres = AtlCreateRegistrar(&registrar);
        if(FAILED(hres))
            return hres;
    }

    IRegistrar_AddReplacement(registrar, L"MODULE", module_name);

    for (iter = pMapEntries; iter && iter->szKey; iter++)
        IRegistrar_AddReplacement(registrar, iter->szKey, iter->szData);

    if(bRegister)
        hres = IRegistrar_ResourceRegisterSz(registrar, module_name, res, L"REGISTRY");
    else
        hres = IRegistrar_ResourceUnregisterSz(registrar, module_name, res, L"REGISTRY");

    if(registrar != pReg)
        IRegistrar_Release(registrar);
    return hres;
}
