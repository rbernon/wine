/*
 * Copyright 2006 Juan Lang
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

#include <assert.h>
#include <stddef.h>
#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"

#define CRYPT_OID_INFO_HAS_EXTRA_FIELDS
#include "crypt32_private.h"
#include "bcrypt.h"
#include "rpc.h"
#include "snmp.h"

WINE_DEFAULT_DEBUG_CHANNEL(context);

static BYTE *copy_string( BYTE *ptr, char **dst, const char *src )
{
    size_t size;
    if (!src) return ptr;
    size = strlen( src ) + 1;
    *dst = memcpy( ptr, src, size );
    return ptr + size;
}

static BYTE *copy_blob( BYTE *ptr, DATA_BLOB *dst, const DATA_BLOB *src )
{
    *dst = *src;
    dst->pbData = memcpy( ptr, src->pbData, src->cbData );
    return ptr + src->cbData;
}

static BYTE *copy_bit_blob( BYTE *ptr, CRYPT_BIT_BLOB *dst, const CRYPT_BIT_BLOB *src )
{
    *dst = *src;
    dst->pbData = memcpy( ptr, src->pbData, src->cbData );
    return ptr + src->cbData;
}

static BYTE *copy_extension( BYTE *ptr, CERT_EXTENSION *dst, const CERT_EXTENSION *src )
{
    *dst = *src;
    ptr = copy_string( ptr, &dst->pszObjId, src->pszObjId );
    return copy_blob( ptr, &dst->Value, &src->Value );
}

context_t *Context_CreateDataContext(size_t contextSize, const context_vtbl_t *vtbl, WINECRYPT_CERTSTORE *store)
{
    context_t *context;

    context = CryptMemAlloc(sizeof(context_t) + contextSize);
    if (!context)
        return NULL;

    if (!(context->properties = properties_create()))
    {
        CryptMemFree(context);
        return NULL;
    }

    context->vtbl = vtbl;
    context->ref = 1;
    context->linked = NULL;

    store->vtbl->addref(store);
    context->store = store;

    TRACE("returning %p\n", context);
    return context;
}

context_t *Context_CreateLinkContext(unsigned int contextSize, context_t *linked, WINECRYPT_CERTSTORE *store)
{
    context_t *context;

    TRACE("(%d, %p)\n", contextSize, linked);

    context = CryptMemAlloc(sizeof(context_t) + contextSize);
    if (!context)
        return NULL;

    memcpy(context_ptr(context), context_ptr(linked), contextSize);
    context->vtbl = linked->vtbl;
    context->ref = 1;
    context->linked = linked;
    context->properties = linked->properties;
    Context_AddRef(linked);

    store->vtbl->addref(store);
    context->store = store;

    TRACE("returning %p\n", context);
    return context;
}

void Context_AddRef(context_t *context)
{
    LONG ref = InterlockedIncrement(&context->ref);

    TRACE("(%p) ref=%ld\n", context, context->ref);

    if(ref == 1) {
        /* This is the first external (non-store) reference. Increase store ref cnt. */
        context->store->vtbl->addref(context->store);
    }
}

void Context_Free(context_t *context)
{
    TRACE("(%p)\n", context);

    assert(!context->ref);

    if (!context->linked) {
        properties_destroy( context->properties );
        context->vtbl->free(context);
    }else {
        Context_Release(context->linked);
    }

    CryptMemFree(context);
}

void Context_Release(context_t *context)
{
    LONG ref = InterlockedDecrement(&context->ref);

    TRACE("(%p) ref=%ld\n", context, ref);
    assert(ref >= 0);

    if (!ref) {
        WINECRYPT_CERTSTORE *store = context->store;

        /* This is the last reference, but the context still may be in a store.
         * We release our store reference, but leave it up to store to free or keep the context. */
        store->vtbl->releaseContext(store, context);
        store->vtbl->release(store, 0);
    }
}

void Context_CopyProperties(const void *to, const void *from)
{
    struct properties *toProperties, *fromProperties;

    toProperties = context_from_ptr(to)->properties;
    fromProperties = context_from_ptr(from)->properties;
    assert(toProperties && fromProperties);
    properties_copy( toProperties, fromProperties );
}

static const context_vtbl_t cert_vtbl;

static void Cert_free( context_t *context )
{
    cert_t *cert = (cert_t *)context;

    CryptMemFree( cert->ctx.pbCertEncoded );
    LocalFree( cert->ctx.pCertInfo );
}

static context_t *Cert_link( context_t *context, WINECRYPT_CERTSTORE *store )
{
    cert_t *cert;
    if (!(cert = (cert_t *)Context_CreateLinkContext( sizeof(CERT_CONTEXT), context, store ))) return NULL;
    cert->ctx.hCertStore = store;
    return &cert->base;
}

static CERT_INFO *cert_info_clone( const CERT_INFO *src, UINT size )
{
    CERT_INFO *dst;
    BYTE *ptr;
    UINT i;

    if (!(dst = LocalAlloc( LPTR, size ))) return NULL;
    ptr = (BYTE *)(dst + 1);

    *dst = *src;
    ptr = copy_blob( ptr, &dst->SerialNumber, &src->SerialNumber );
    ptr = copy_string( ptr, &dst->SignatureAlgorithm.pszObjId, src->SignatureAlgorithm.pszObjId );
    ptr = copy_blob( ptr, &dst->SignatureAlgorithm.Parameters, &src->SignatureAlgorithm.Parameters );
    ptr = copy_blob( ptr, &dst->Issuer, &src->Issuer );
    ptr = copy_blob( ptr, &dst->Subject, &src->Subject );
    ptr = copy_string( ptr, &dst->SubjectPublicKeyInfo.Algorithm.pszObjId, src->SubjectPublicKeyInfo.Algorithm.pszObjId );
    ptr = copy_blob( ptr, &dst->SubjectPublicKeyInfo.Algorithm.Parameters, &src->SubjectPublicKeyInfo.Algorithm.Parameters );
    ptr = copy_bit_blob( ptr, &dst->SubjectPublicKeyInfo.PublicKey, &src->SubjectPublicKeyInfo.PublicKey );
    ptr = copy_bit_blob( ptr, &dst->IssuerUniqueId, &src->IssuerUniqueId );
    ptr = copy_bit_blob( ptr, &dst->SubjectUniqueId, &src->SubjectUniqueId );

    dst->rgExtension = (CERT_EXTENSION *)ptr;
    ptr += src->cExtension * sizeof(CERT_EXTENSION);
    for (i = 0; i < src->cExtension; ++i)
    {
        const CERT_EXTENSION *src_ext = &src->rgExtension[i];
        CERT_EXTENSION *dst_ext = &dst->rgExtension[i];
        ptr = copy_extension( ptr, dst_ext, src_ext );
    }

    return dst;
}

static context_t *Cert_clone( context_t *context, WINECRYPT_CERTSTORE *store )
{
    const cert_t *cloned = (const cert_t *)context;
    cert_t *cert;

    if (!(cert = (cert_t *)Context_CreateDataContext( sizeof(CERT_CONTEXT), &cert_vtbl, store ))) return NULL;
    Context_CopyProperties( &cert->ctx, &cloned->ctx );

    cert->ctx.dwCertEncodingType = cloned->ctx.dwCertEncodingType;
    cert->ctx.pbCertEncoded = CryptMemAlloc( cloned->ctx.cbCertEncoded );
    memcpy( cert->ctx.pbCertEncoded, cloned->ctx.pbCertEncoded, cloned->ctx.cbCertEncoded );
    cert->ctx.cbCertEncoded = cloned->ctx.cbCertEncoded;

    if (!(cert->ctx.pCertInfo = cert_info_clone( cloned->ctx.pCertInfo, cloned->base.info_size )))
    {
        CertFreeCertificateContext( &cert->ctx );
        return NULL;
    }
    cert->base.info_size = cloned->base.info_size;

    cert->ctx.hCertStore = store;
    return &cert->base;
}

static const context_vtbl_t cert_vtbl =
{
    Cert_free,
    Cert_link,
    Cert_clone,
};

const CERT_CONTEXT *WINAPI CertCreateCertificateContext( DWORD encoding, const BYTE *buf, DWORD size )
{
    cert_t *cert = NULL;
    BYTE *data = NULL;
    BOOL ret;
    CERT_INFO *info;
    DWORD info_size = 0;

    TRACE( "(%08lx, %p, %ld)\n", encoding, buf, size );

    if ((encoding & CERT_ENCODING_TYPE_MASK) != X509_ASN_ENCODING)
    {
        SetLastError( E_INVALIDARG );
        return NULL;
    }

    ret = CryptDecodeObjectEx( encoding, X509_CERT_TO_BE_SIGNED, buf, size,
                               CRYPT_DECODE_ALLOC_FLAG, NULL, &info, &info_size );
    if (!ret) return NULL;

    if (!(cert = (cert_t *)Context_CreateDataContext( sizeof(CERT_CONTEXT), &cert_vtbl, &empty_store ))) return NULL;
    if (!(data = CryptMemAlloc( size )))
    {
        Context_Release( &cert->base );
        return NULL;
    }

    memcpy( data, buf, size );
    cert->ctx.dwCertEncodingType = encoding;
    cert->ctx.pbCertEncoded = data;
    cert->ctx.cbCertEncoded = size;
    cert->ctx.pCertInfo = info;
    cert->ctx.hCertStore = &empty_store;
    cert->base.info_size = info_size;

    return &cert->ctx;
}

const CERT_CONTEXT *WINAPI CertDuplicateCertificateContext( const CERT_CONTEXT *cert )
{
    TRACE( "cert %p\n", cert );
    if (cert) Context_AddRef( &cert_from_ptr( cert )->base );
    return cert;
}

BOOL WINAPI CertFreeCertificateContext( const CERT_CONTEXT *cert )
{
    TRACE( "cert %p\n", cert );
    if (cert) Context_Release( &cert_from_ptr( cert )->base );
    return TRUE;
}

DWORD WINAPI CertEnumCertificateContextProperties( const CERT_CONTEXT *cert, DWORD id )
{
    TRACE( "cert %p, id %#lx\n", cert, id );
    return properties_enum_ids( cert_from_ptr( cert )->base.properties, id );
}

static BOOL cert_get_hash_property( cert_t *cert, DWORD prop_id, ALG_ID alg_id, const BYTE *key_buf,
                                    DWORD key_len, void *data_buf, DWORD *data_len )
{
    BOOL ret = CryptHashCertificate( 0, alg_id, 0, key_buf, key_len, data_buf, data_len );

    if (ret && data_buf)
    {
        CRYPT_DATA_BLOB blob = {.cbData = *data_len, .pbData = data_buf};
        ret = CertSetCertificateContextProperty( &cert->ctx, prop_id, 0, &blob );
    }

    return ret;
}

static BOOL copy_property_data( void *dst_buf, DWORD *dst_len, const void *src_buf, DWORD src_len )
{
    BOOL ret = TRUE;

    if (!dst_buf)
        *dst_len = src_len;
    else if (*dst_len < src_len)
    {
        SetLastError( ERROR_MORE_DATA );
        *dst_len = src_len;
        ret = FALSE;
    }
    else
    {
        memcpy( dst_buf, src_buf, src_len );
        *dst_len = src_len;
    }

    return ret;
}

void CRYPT_ConvertKeyContext( const struct store_CERT_KEY_CONTEXT *src, CERT_KEY_CONTEXT *dst )
{
    dst->cbSize = sizeof(*dst);
    dst->hCryptProv = src->hCryptProv;
    dst->dwKeySpec = src->dwKeySpec;
}

/*
 * Fix offsets in a continuous block of memory of CRYPT_KEY_PROV_INFO with
 * its associated data.
 */
static void fixup_crypt_key_prov_info( CRYPT_KEY_PROV_INFO *info )
{
    BYTE *data;
    DWORD i;

    data = (BYTE *)(info + 1) + sizeof(CRYPT_KEY_PROV_PARAM) * info->cProvParam;

    if (info->pwszContainerName)
    {
        info->pwszContainerName = (LPWSTR)data;
        data += (lstrlenW( info->pwszContainerName ) + 1) * sizeof(WCHAR);
    }

    if (info->pwszProvName)
    {
        info->pwszProvName = (LPWSTR)data;
        data += (lstrlenW( info->pwszProvName ) + 1) * sizeof(WCHAR);
    }

    info->rgProvParam = info->cProvParam ? (CRYPT_KEY_PROV_PARAM *)(info + 1) : NULL;

    for (i = 0; i < info->cProvParam; i++)
    {
        info->rgProvParam[i].pbData = info->rgProvParam[i].cbData ? data : NULL;
        data += info->rgProvParam[i].cbData;
    }
}

static BOOL cert_get_property( cert_t *cert, DWORD id, void *buf, DWORD *len )
{
    BOOL ret;
    CRYPT_DATA_BLOB blob;

    if ((ret = properties_lookup( cert->base.properties, id, &blob )))
    {
        CERT_KEY_CONTEXT key;

        if (id == CERT_KEY_CONTEXT_PROP_ID)
        {
            CRYPT_ConvertKeyContext( (const struct store_CERT_KEY_CONTEXT *)blob.pbData, &key );
            blob.pbData = (BYTE *)&key;
            blob.cbData = key.cbSize;
        }

        ret = copy_property_data( buf, len, blob.pbData, blob.cbData );
    }
    else
    {
        /* Implicit properties */
        switch (id)
        {
        case CERT_SHA1_HASH_PROP_ID:
            ret = cert_get_hash_property( cert, id, CALG_SHA1, cert->ctx.pbCertEncoded,
                                           cert->ctx.cbCertEncoded, buf, len );
            break;
        case CERT_MD5_HASH_PROP_ID:
            ret = cert_get_hash_property( cert, id, CALG_MD5, cert->ctx.pbCertEncoded,
                                           cert->ctx.cbCertEncoded, buf, len );
            break;
        case CERT_SUBJECT_NAME_MD5_HASH_PROP_ID:
            ret = cert_get_hash_property( cert, id, CALG_MD5, cert->ctx.pCertInfo->Subject.pbData,
                                           cert->ctx.pCertInfo->Subject.cbData, buf, len );
            break;
        case CERT_SUBJECT_PUBLIC_KEY_MD5_HASH_PROP_ID:
            ret = cert_get_hash_property( cert, id, CALG_MD5, cert->ctx.pCertInfo->SubjectPublicKeyInfo.PublicKey.pbData,
                                           cert->ctx.pCertInfo->SubjectPublicKeyInfo.PublicKey.cbData, buf, len );
            break;
        case CERT_ISSUER_SERIAL_NUMBER_MD5_HASH_PROP_ID:
            ret = cert_get_hash_property( cert, id, CALG_MD5, cert->ctx.pCertInfo->SerialNumber.pbData,
                                           cert->ctx.pCertInfo->SerialNumber.cbData, buf, len );
            break;
        case CERT_SIGNATURE_HASH_PROP_ID:
            ret = CryptHashToBeSigned( 0, cert->ctx.dwCertEncodingType, cert->ctx.pbCertEncoded,
                                       cert->ctx.cbCertEncoded, buf, len );
            if (ret && buf)
            {
                CRYPT_DATA_BLOB blob = {.cbData = *len, .pbData = buf};
                ret = CertSetCertificateContextProperty( &cert->ctx, id, 0, &blob );
            }
            break;
        case CERT_KEY_IDENTIFIER_PROP_ID:
        {
            CERT_EXTENSION *ext = CertFindExtension( szOID_SUBJECT_KEY_IDENTIFIER,
                                                     cert->ctx.pCertInfo->cExtension,
                                                     cert->ctx.pCertInfo->rgExtension );
            if (ext)
            {
                CRYPT_DATA_BLOB value;
                DWORD size = sizeof(value);

                ret = CryptDecodeObjectEx( X509_ASN_ENCODING, szOID_SUBJECT_KEY_IDENTIFIER,
                                           ext->Value.pbData, ext->Value.cbData,
                                           CRYPT_DECODE_NOCOPY_FLAG, NULL, &value, &size );
                if (ret)
                {
                    ret = copy_property_data( buf, len, value.pbData, value.cbData );
                    CertSetCertificateContextProperty( &cert->ctx, id, 0, &value );
                }
            }
            else SetLastError( ERROR_INVALID_DATA );
            break;
        }
        default:
            SetLastError( CRYPT_E_NOT_FOUND );
            break;
        }
    }

    return ret;
}

BOOL WINAPI CertGetCertificateContextProperty( const CERT_CONTEXT *pCertContext, DWORD id, void *buf, DWORD *len )
{
    cert_t *cert = cert_from_ptr( pCertContext );
    BOOL ret;

    TRACE( "cert %p, id %#lx, buf %p, len %p\n", cert, id, buf, len );

    switch (id)
    {
    case 0:
    case CERT_CERT_PROP_ID:
    case CERT_CRL_PROP_ID:
    case CERT_CTL_PROP_ID:
        SetLastError( E_INVALIDARG );
        ret = FALSE;
        break;
    case CERT_ACCESS_STATE_PROP_ID:
        ret = CertGetStoreProperty( cert->ctx.hCertStore, id, buf, len );
        break;
    case CERT_KEY_PROV_HANDLE_PROP_ID:
    {
        CERT_KEY_CONTEXT key;
        DWORD size = sizeof(key);

        ret = cert_get_property( cert, CERT_KEY_CONTEXT_PROP_ID, &key, &size );
        if (ret) ret = copy_property_data( buf, len, &key.hCryptProv, sizeof(key.hCryptProv) );
        break;
    }
    case CERT_KEY_PROV_INFO_PROP_ID:
        ret = cert_get_property( cert, id, buf, len );
        if (ret && buf) fixup_crypt_key_prov_info( buf );
        break;
    default:
        ret = cert_get_property( cert, id, buf, len );
        break;
    }

    TRACE( "returning %d\n", ret );
    return ret;
}

/*
 * Copy to a continuous block of memory of CRYPT_KEY_PROV_INFO with
 * its associated data.
 */
static void copy_crypt_key_prov_info( const CRYPT_KEY_PROV_INFO *from, CRYPT_KEY_PROV_INFO *to )
{
    BYTE *data;
    DWORD i;

    data = (BYTE *)(to + 1) + sizeof(CRYPT_KEY_PROV_PARAM) * from->cProvParam;

    if (from->pwszContainerName)
    {
        to->pwszContainerName = (LPWSTR)data;
        lstrcpyW( (LPWSTR)data, from->pwszContainerName );
        data += (lstrlenW( from->pwszContainerName ) + 1) * sizeof(WCHAR);
    }
    else to->pwszContainerName = NULL;

    if (from->pwszProvName)
    {
        to->pwszProvName = (LPWSTR)data;
        lstrcpyW( (LPWSTR)data, from->pwszProvName );
        data += (lstrlenW( from->pwszProvName ) + 1) * sizeof(WCHAR);
    }
    else to->pwszProvName = NULL;

    to->dwProvType = from->dwProvType;
    to->dwFlags = from->dwFlags;
    to->cProvParam = from->cProvParam;
    to->rgProvParam = from->cProvParam ? (CRYPT_KEY_PROV_PARAM *)(to + 1) : NULL;
    to->dwKeySpec = from->dwKeySpec;

    for (i = 0; i < from->cProvParam; i++)
    {
        to->rgProvParam[i].dwParam = from->rgProvParam[i].dwParam;
        to->rgProvParam[i].dwFlags = from->rgProvParam[i].dwFlags;
        to->rgProvParam[i].cbData = from->rgProvParam[i].cbData;
        to->rgProvParam[i].pbData = from->rgProvParam[i].cbData ? data : NULL;
        memcpy( data, from->rgProvParam[i].pbData, from->rgProvParam[i].cbData );
        data += from->rgProvParam[i].cbData;
    }
}

/*
 * Create a continuous block of memory for CRYPT_KEY_PROV_INFO with
 * its associated data, and add it to the certificate properties.
 */
static BOOL cert_set_crypt_key_prov_info_property( struct properties *properties, const CRYPT_KEY_PROV_INFO *info )
{
    CRYPT_KEY_PROV_INFO *prop;
    DWORD size = sizeof(CRYPT_KEY_PROV_INFO), i;
    BOOL ret;

    if (info->pwszContainerName) size += (lstrlenW( info->pwszContainerName ) + 1) * sizeof(WCHAR);
    if (info->pwszProvName) size += (lstrlenW( info->pwszProvName ) + 1) * sizeof(WCHAR);

    for (i = 0; i < info->cProvParam; i++)
        size += sizeof(CRYPT_KEY_PROV_PARAM) + info->rgProvParam[i].cbData;

    if (!(prop = CryptMemAlloc( size )))
    {
        SetLastError( ERROR_OUTOFMEMORY );
        return FALSE;
    }

    copy_crypt_key_prov_info( info, prop );

    ret = properties_insert( properties, CERT_KEY_PROV_INFO_PROP_ID, (const BYTE *)prop, size );
    CryptMemFree( prop );

    return ret;
}

static BOOL cert_set_crypt_key_context_property( struct properties *properties, const CERT_KEY_CONTEXT *keyContext )
{
    struct store_CERT_KEY_CONTEXT ctx;

    ctx.cbSize = sizeof(ctx);
    ctx.hCryptProv = keyContext->hCryptProv;
    ctx.dwKeySpec = keyContext->dwKeySpec;

    return properties_insert( properties, CERT_KEY_CONTEXT_PROP_ID, (const BYTE *)&ctx, ctx.cbSize );
}

static BOOL cert_set_property( cert_t *cert, DWORD id, DWORD flags, const void *data )
{
    BOOL ret;

    if (id >= CERT_FIRST_USER_PROP_ID && id <= CERT_LAST_USER_PROP_ID)
    {
        if (data)
        {
            const CRYPT_DATA_BLOB *blob = data;
            ret = properties_insert( cert->base.properties, id, blob->pbData, blob->cbData );
        }
        else
        {
            properties_remove( cert->base.properties, id );
            ret = TRUE;
        }
    }
    else
    {
        switch (id)
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
        case CERT_EXTENDED_ERROR_INFO_PROP_ID:
        case CERT_SUBJECT_PUBLIC_KEY_MD5_HASH_PROP_ID:
        case CERT_ENROLLMENT_PROP_ID:
        case CERT_CROSS_CERT_DIST_POINTS_PROP_ID:
        case CERT_OCSP_RESPONSE_PROP_ID:
        case CERT_RENEWAL_PROP_ID:
            if (data)
            {
                const CRYPT_DATA_BLOB *blob = data;
                ret = properties_insert( cert->base.properties, id, blob->pbData, blob->cbData );
            }
            else
            {
                properties_remove( cert->base.properties, id );
                ret = TRUE;
            }
            break;
        case CERT_DATE_STAMP_PROP_ID:
            if (data)
                ret = properties_insert( cert->base.properties, id, data, sizeof(FILETIME) );
            else
            {
                properties_remove( cert->base.properties, id );
                ret = TRUE;
            }
            break;
        case CERT_KEY_CONTEXT_PROP_ID:
            if (data)
            {
                const CERT_KEY_CONTEXT *keyContext = data;

                if (keyContext->cbSize != sizeof(CERT_KEY_CONTEXT))
                {
                    SetLastError( E_INVALIDARG );
                    ret = FALSE;
                }
                else
                {
                    ret = cert_set_crypt_key_context_property( cert->base.properties, data );
                }
            }
            else
            {
                properties_remove( cert->base.properties, id );
                ret = TRUE;
            }
            break;
        case CERT_KEY_PROV_INFO_PROP_ID:
            if (data)
                ret = cert_set_crypt_key_prov_info_property( cert->base.properties, data );
            else
            {
                properties_remove( cert->base.properties, id );
                ret = TRUE;
            }
            break;
        case CERT_KEY_PROV_HANDLE_PROP_ID:
        {
            CERT_KEY_CONTEXT key;
            DWORD size = sizeof(key);

            ret = CertGetCertificateContextProperty( &cert->ctx, CERT_KEY_CONTEXT_PROP_ID, &key, &size );
            if (ret && !(flags & CERT_STORE_NO_CRYPT_RELEASE_FLAG)) CryptReleaseContext( key.hCryptProv, 0 );

            key.cbSize = sizeof(key);
            if (data)
                key.hCryptProv = *(const HCRYPTPROV *)data;
            else
            {
                key.hCryptProv = 0;
                key.dwKeySpec = AT_SIGNATURE;
            }

            ret = cert_set_property( cert, CERT_KEY_CONTEXT_PROP_ID, 0, &key );
            break;
        }
        default:
            FIXME( "%ld: stub\n", id );
            ret = FALSE;
            break;
        }
    }

    return ret;
}

BOOL WINAPI CertSetCertificateContextProperty( const CERT_CONTEXT *cert, DWORD id, DWORD flags, const void *data )
{
    BOOL ret;

    TRACE( "cert %p, id %#lx, data %p\n", cert, id, data );

    /* Handle special cases for "read-only"/invalid prop IDs.  Windows just
     * crashes on most of these, I'll be safer.
     */
    switch (id)
    {
    case 0:
    case CERT_ACCESS_STATE_PROP_ID:
    case CERT_CERT_PROP_ID:
    case CERT_CRL_PROP_ID:
    case CERT_CTL_PROP_ID:
        SetLastError( E_INVALIDARG );
        return FALSE;
    }

    ret = cert_set_property( cert_from_ptr( cert ), id, flags, data );
    TRACE( "returning %d\n", ret );
    return ret;
}
