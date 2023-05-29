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
