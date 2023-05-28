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

DWORD WINAPI CertEnumCTLContextProperties(PCCTL_CONTEXT pCTLContext,
 DWORD dwPropId)
{
    ctl_t *ctl = ctl_from_ptr(pCTLContext);
    TRACE("(%p, %ld)\n", pCTLContext, dwPropId);
    return properties_enum_ids( ctl->base.properties, dwPropId );
}

static BOOL CTLContext_GetHashProp(ctl_t *ctl, DWORD dwPropId,
 ALG_ID algID, const BYTE *toHash, DWORD toHashLen, void *pvData,
 DWORD *pcbData)
{
    BOOL ret = CryptHashCertificate(0, algID, 0, toHash, toHashLen, pvData,
     pcbData);
    if (ret && pvData)
    {
        CRYPT_DATA_BLOB blob = { *pcbData, pvData };

        ret = CertSetCTLContextProperty(&ctl->ctx, dwPropId, 0, &blob);
    }
    return ret;
}

static BOOL CTLContext_GetProperty(ctl_t *ctl, DWORD dwPropId,
                                   void *pvData, DWORD *pcbData)
{
    BOOL ret;
    CRYPT_DATA_BLOB blob;

    TRACE("(%p, %ld, %p, %p)\n", ctl, dwPropId, pvData, pcbData);

    if ((ret = properties_lookup( ctl->base.properties, dwPropId, &blob )))
    {
        if (!pvData)
            *pcbData = blob.cbData;
        else if (*pcbData < blob.cbData)
        {
            SetLastError(ERROR_MORE_DATA);
            *pcbData = blob.cbData;
            ret = FALSE;
        }
        else
        {
            memcpy(pvData, blob.pbData, blob.cbData);
            *pcbData = blob.cbData;
        }
    }
    else
    {
        /* Implicit properties */
        switch (dwPropId)
        {
        case CERT_SHA1_HASH_PROP_ID:
            ret = CTLContext_GetHashProp(ctl, dwPropId, CALG_SHA1,
             ctl->ctx.pbCtlEncoded, ctl->ctx.cbCtlEncoded, pvData, pcbData);
            break;
        case CERT_MD5_HASH_PROP_ID:
            ret = CTLContext_GetHashProp(ctl, dwPropId, CALG_MD5,
             ctl->ctx.pbCtlEncoded, ctl->ctx.cbCtlEncoded, pvData, pcbData);
            break;
        default:
            SetLastError(CRYPT_E_NOT_FOUND);
        }
    }
    TRACE("returning %d\n", ret);
    return ret;
}

BOOL WINAPI CertGetCTLContextProperty(PCCTL_CONTEXT pCTLContext,
 DWORD dwPropId, void *pvData, DWORD *pcbData)
{
    BOOL ret;

    TRACE("(%p, %ld, %p, %p)\n", pCTLContext, dwPropId, pvData, pcbData);

    switch (dwPropId)
    {
    case 0:
    case CERT_CERT_PROP_ID:
    case CERT_CRL_PROP_ID:
    case CERT_CTL_PROP_ID:
        SetLastError(E_INVALIDARG);
        ret = FALSE;
        break;
    case CERT_ACCESS_STATE_PROP_ID:
        if (!pvData)
        {
            *pcbData = sizeof(DWORD);
            ret = TRUE;
        }
        else if (*pcbData < sizeof(DWORD))
        {
            SetLastError(ERROR_MORE_DATA);
            *pcbData = sizeof(DWORD);
            ret = FALSE;
        }
        else
        {
            ret = CertGetStoreProperty(pCTLContext->hCertStore, dwPropId, pvData, pcbData);
        }
        break;
    default:
        ret = CTLContext_GetProperty(ctl_from_ptr(pCTLContext), dwPropId, pvData,
         pcbData);
    }
    return ret;
}

static BOOL CTLContext_SetProperty(ctl_t *ctl, DWORD dwPropId,
 DWORD dwFlags, const void *pvData)
{
    BOOL ret;

    TRACE("(%p, %ld, %08lx, %p)\n", ctl, dwPropId, dwFlags, pvData);

    if (!pvData)
    {
        properties_remove( ctl->base.properties, dwPropId );
        ret = TRUE;
    }
    else
    {
        switch (dwPropId)
        {
        case CERT_AUTO_ENROLL_PROP_ID:
        case CERT_CTL_USAGE_PROP_ID: /* same as CERT_ENHKEY_USAGE_PROP_ID */
        case CERT_DESCRIPTION_PROP_ID:
        case CERT_FRIENDLY_NAME_PROP_ID:
        case CERT_HASH_PROP_ID:
        case CERT_KEY_IDENTIFIER_PROP_ID:
        case CERT_MD5_HASH_PROP_ID:
        case CERT_NEXT_UPDATE_LOCATION_PROP_ID:
        case CERT_PUBKEY_ALG_PARA_PROP_ID:
        case CERT_PVK_FILE_PROP_ID:
        case CERT_SIGNATURE_HASH_PROP_ID:
        case CERT_ISSUER_PUBLIC_KEY_MD5_HASH_PROP_ID:
        case CERT_SUBJECT_NAME_MD5_HASH_PROP_ID:
        case CERT_SUBJECT_PUBLIC_KEY_MD5_HASH_PROP_ID:
        case CERT_ENROLLMENT_PROP_ID:
        case CERT_CROSS_CERT_DIST_POINTS_PROP_ID:
        case CERT_RENEWAL_PROP_ID:
        {
            PCRYPT_DATA_BLOB blob = (PCRYPT_DATA_BLOB)pvData;

            ret = properties_insert( ctl->base.properties, dwPropId, blob->pbData, blob->cbData );
            break;
        }
        case CERT_DATE_STAMP_PROP_ID:
            ret = properties_insert( ctl->base.properties, dwPropId, pvData, sizeof(FILETIME) );
            break;
        default:
            FIXME("%ld: stub\n", dwPropId);
            ret = FALSE;
        }
    }
    TRACE("returning %d\n", ret);
    return ret;
}

BOOL WINAPI CertSetCTLContextProperty(PCCTL_CONTEXT pCTLContext,
 DWORD dwPropId, DWORD dwFlags, const void *pvData)
{
    BOOL ret;

    TRACE("(%p, %ld, %08lx, %p)\n", pCTLContext, dwPropId, dwFlags, pvData);

    /* Handle special cases for "read-only"/invalid prop IDs.  Windows just
     * crashes on most of these, I'll be safer.
     */
    switch (dwPropId)
    {
    case 0:
    case CERT_ACCESS_STATE_PROP_ID:
    case CERT_CERT_PROP_ID:
    case CERT_CRL_PROP_ID:
    case CERT_CTL_PROP_ID:
        SetLastError(E_INVALIDARG);
        return FALSE;
    }
    ret = CTLContext_SetProperty(ctl_from_ptr(pCTLContext), dwPropId, dwFlags, pvData);
    TRACE("returning %d\n", ret);
    return ret;
}
