/*
 * Copyright 2008 Juan Lang
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
 *
 */

#include <assert.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wincrypt.h"
#include "wine/debug.h"
#include "crypt32_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(crypt);

typedef BOOL (*CtlCompareFunc)(PCCTL_CONTEXT pCtlContext, DWORD dwType,
 DWORD dwFlags, const void *pvPara);

static BOOL compare_ctl_any(PCCTL_CONTEXT pCtlContext, DWORD dwType,
 DWORD dwFlags, const void *pvPara)
{
    return TRUE;
}

static BOOL compare_ctl_by_md5_hash(PCCTL_CONTEXT pCtlContext, DWORD dwType,
 DWORD dwFlags, const void *pvPara)
{
    BOOL ret;
    BYTE hash[16];
    DWORD size = sizeof(hash);

    ret = CertGetCTLContextProperty(pCtlContext, CERT_MD5_HASH_PROP_ID, hash,
     &size);
    if (ret)
    {
        const CRYPT_HASH_BLOB *pHash = pvPara;

        if (size == pHash->cbData)
            ret = !memcmp(pHash->pbData, hash, size);
        else
            ret = FALSE;
    }
    return ret;
}

static BOOL compare_ctl_by_sha1_hash(PCCTL_CONTEXT pCtlContext, DWORD dwType,
 DWORD dwFlags, const void *pvPara)
{
    BOOL ret;
    BYTE hash[20];
    DWORD size = sizeof(hash);

    ret = CertGetCTLContextProperty(pCtlContext, CERT_SHA1_HASH_PROP_ID, hash,
     &size);
    if (ret)
    {
        const CRYPT_HASH_BLOB *pHash = pvPara;

        if (size == pHash->cbData)
            ret = !memcmp(pHash->pbData, hash, size);
        else
            ret = FALSE;
    }
    return ret;
}

static BOOL compare_ctl_existing(PCCTL_CONTEXT pCtlContext, DWORD dwType,
 DWORD dwFlags, const void *pvPara)
{
    BOOL ret;

    if (pvPara)
    {
        PCCTL_CONTEXT ctl = pvPara;

        if (pCtlContext->cbCtlContext == ctl->cbCtlContext)
        {
            if (ctl->cbCtlContext)
                ret = !memcmp(pCtlContext->pbCtlContext, ctl->pbCtlContext,
                 ctl->cbCtlContext);
            else
                ret = TRUE;
        }
        else
            ret = FALSE;
    }
    else
        ret = FALSE;
    return ret;
}

PCCTL_CONTEXT WINAPI CertFindCTLInStore(HCERTSTORE hCertStore,
 DWORD dwCertEncodingType, DWORD dwFindFlags, DWORD dwFindType,
 const void *pvFindPara, PCCTL_CONTEXT pPrevCtlContext)
{
    PCCTL_CONTEXT ret;
    CtlCompareFunc compare;

    TRACE("(%p, %ld, %ld, %ld, %p, %p)\n", hCertStore, dwCertEncodingType,
	 dwFindFlags, dwFindType, pvFindPara, pPrevCtlContext);

    switch (dwFindType)
    {
    case CTL_FIND_ANY:
        compare = compare_ctl_any;
        break;
    case CTL_FIND_SHA1_HASH:
        compare = compare_ctl_by_sha1_hash;
        break;
    case CTL_FIND_MD5_HASH:
        compare = compare_ctl_by_md5_hash;
        break;
    case CTL_FIND_EXISTING:
        compare = compare_ctl_existing;
        break;
    default:
        FIXME("find type %08lx unimplemented\n", dwFindType);
        compare = NULL;
    }

    if (compare)
    {
        BOOL matches = FALSE;

        ret = pPrevCtlContext;
        do {
            ret = CertEnumCTLsInStore(hCertStore, ret);
            if (ret)
                matches = compare(ret, dwFindType, dwFindFlags, pvFindPara);
        } while (ret != NULL && !matches);
        if (!ret)
            SetLastError(CRYPT_E_NOT_FOUND);
    }
    else
    {
        SetLastError(CRYPT_E_NOT_FOUND);
        ret = NULL;
    }
    return ret;
}
