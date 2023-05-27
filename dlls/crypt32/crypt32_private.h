/*
 * Copyright 2005 Juan Lang
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

#ifndef __CRYPT32_PRIVATE_H__
#define __CRYPT32_PRIVATE_H__

#include <stddef.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wincrypt.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unixlib.h"

struct store_prov_vtbl
{
    PFN_CERT_STORE_PROV_CLOSE              close;
    PFN_CERT_STORE_PROV_CONTROL            control;

    PFN_CERT_STORE_PROV_READ_CERT          read_cert;
    PFN_CERT_STORE_PROV_WRITE_CERT         write_cert;
    PFN_CERT_STORE_PROV_DELETE_CERT        delete_cert;
    PFN_CERT_STORE_PROV_FIND_CERT          find_cert;
    PFN_CERT_STORE_PROV_FREE_FIND_CERT     free_find_cert;
    PFN_CERT_STORE_PROV_SET_CERT_PROPERTY  set_cert_property;
    PFN_CERT_STORE_PROV_GET_CERT_PROPERTY  get_cert_property;

    PFN_CERT_STORE_PROV_READ_CRL           read_crl;
    PFN_CERT_STORE_PROV_WRITE_CRL          write_crl;
    PFN_CERT_STORE_PROV_DELETE_CRL         delete_crl;
    PFN_CERT_STORE_PROV_FIND_CRL           find_crl;
    PFN_CERT_STORE_PROV_FREE_FIND_CRL      free_find_crl;
    PFN_CERT_STORE_PROV_SET_CRL_PROPERTY   set_crl_property;
    PFN_CERT_STORE_PROV_GET_CRL_PROPERTY   get_crl_property;

    PFN_CERT_STORE_PROV_READ_CTL           read_ctl;
    PFN_CERT_STORE_PROV_WRITE_CTL          write_ctl;
    PFN_CERT_STORE_PROV_DELETE_CTL         delete_ctl;
    PFN_CERT_STORE_PROV_FIND_CTL           find_ctl;
    PFN_CERT_STORE_PROV_FREE_FIND_CTL      free_find_ctl;
    PFN_CERT_STORE_PROV_SET_CTL_PROPERTY   set_ctl_property;
    PFN_CERT_STORE_PROV_GET_CTL_PROPERTY   get_ctl_property;
};

BOOL CNG_ImportPubKey(CERT_PUBLIC_KEY_INFO *pubKeyInfo, BCRYPT_KEY_HANDLE *key);
BOOL cng_prepare_signature(const char *alg_oid, BYTE *encoded_sig, DWORD encoded_sig_len,
    BYTE **sig_value, DWORD *sig_len);

/* a few asn.1 tags we need */
#define ASN_BOOL            (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x01)
#define ASN_BITSTRING       (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x03)
#define ASN_ENUMERATED      (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x0a)
#define ASN_UTF8STRING      (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x0c)
#define ASN_SETOF           (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x11)
#define ASN_NUMERICSTRING   (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x12)
#define ASN_PRINTABLESTRING (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x13)
#define ASN_T61STRING       (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x14)
#define ASN_VIDEOTEXSTRING  (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x15)
#define ASN_IA5STRING       (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x16)
#define ASN_UTCTIME         (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x17)
#define ASN_GENERALTIME     (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x18)
#define ASN_GRAPHICSTRING   (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x19)
#define ASN_VISIBLESTRING   (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x1a)
#define ASN_GENERALSTRING   (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x1b)
#define ASN_UNIVERSALSTRING (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x1c)
#define ASN_BMPSTRING       (ASN_UNIVERSAL | ASN_PRIMITIVE | 0x1e)

/* Copies `len` bytes from `src` to `dst`,
 * reversing the order of the bytes
 */
void CRYPT_CopyReversed(BYTE *dst, const BYTE *src, size_t len);

BOOL CRYPT_EncodeLen(DWORD len, BYTE *pbEncoded, DWORD *pcbEncoded);

typedef BOOL (WINAPI *CryptEncodeObjectExFunc)(DWORD, LPCSTR, const void *,
 DWORD, PCRYPT_ENCODE_PARA, BYTE *, DWORD *);

struct AsnEncodeSequenceItem
{
    const void             *pvStructInfo;
    CryptEncodeObjectExFunc encodeFunc;
    DWORD                   size; /* used during encoding, not for your use */
};

BOOL WINAPI CRYPT_AsnEncodeSequence(DWORD dwCertEncodingType,
 struct AsnEncodeSequenceItem items[], DWORD cItem, DWORD dwFlags,
 PCRYPT_ENCODE_PARA pEncodePara, BYTE *pbEncoded, DWORD *pcbEncoded);

struct AsnConstructedItem
{
    BYTE                    tag;
    const void             *pvStructInfo;
    CryptEncodeObjectExFunc encodeFunc;
};

BOOL WINAPI CRYPT_AsnEncodeConstructed(DWORD dwCertEncodingType,
 LPCSTR lpszStructType, const void *pvStructInfo, DWORD dwFlags,
 PCRYPT_ENCODE_PARA pEncodePara, BYTE *pbEncoded, DWORD *pcbEncoded);
BOOL WINAPI CRYPT_AsnEncodeOid(DWORD dwCertEncodingType,
 LPCSTR lpszStructType, const void *pvStructInfo, DWORD dwFlags,
 PCRYPT_ENCODE_PARA pEncodePara, BYTE *pbEncoded, DWORD *pcbEncoded);
BOOL WINAPI CRYPT_AsnEncodeOctets(DWORD dwCertEncodingType,
 LPCSTR lpszStructType, const void *pvStructInfo, DWORD dwFlags,
 PCRYPT_ENCODE_PARA pEncodePara, BYTE *pbEncoded, DWORD *pcbEncoded);

typedef struct _CRYPT_DIGESTED_DATA
{
    DWORD                      version;
    CRYPT_ALGORITHM_IDENTIFIER DigestAlgorithm;
    CRYPT_CONTENT_INFO         ContentInfo;
    CRYPT_HASH_BLOB            hash;
} CRYPT_DIGESTED_DATA;

BOOL CRYPT_AsnEncodePKCSDigestedData(const CRYPT_DIGESTED_DATA *digestedData,
 void *pvData, DWORD *pcbData);

typedef struct _CRYPT_ENCRYPTED_CONTENT_INFO
{
    LPSTR                      contentType;
    CRYPT_ALGORITHM_IDENTIFIER contentEncryptionAlgorithm;
    CRYPT_DATA_BLOB            encryptedContent;
} CRYPT_ENCRYPTED_CONTENT_INFO;

typedef struct _CRYPT_ENVELOPED_DATA
{
    DWORD                          version;
    DWORD                          cRecipientInfo;
    PCMSG_KEY_TRANS_RECIPIENT_INFO rgRecipientInfo;
    CRYPT_ENCRYPTED_CONTENT_INFO   encryptedContentInfo;
} CRYPT_ENVELOPED_DATA;

BOOL CRYPT_AsnEncodePKCSEnvelopedData(const CRYPT_ENVELOPED_DATA *envelopedData,
 void *pvData, DWORD *pcbData);

BOOL CRYPT_AsnDecodePKCSEnvelopedData(const BYTE *pbEncoded, DWORD cbEncoded,
 DWORD dwFlags, PCRYPT_DECODE_PARA pDecodePara,
 CRYPT_ENVELOPED_DATA *envelopedData, DWORD *pcbEnvelopedData);

typedef struct _CRYPT_SIGNED_INFO
{
    DWORD                 version;
    DWORD                 cCertEncoded;
    PCERT_BLOB            rgCertEncoded;
    DWORD                 cCrlEncoded;
    PCRL_BLOB             rgCrlEncoded;
    CRYPT_CONTENT_INFO    content;
    DWORD                 cSignerInfo;
    PCMSG_CMS_SIGNER_INFO rgSignerInfo;
    PDWORD                signerKeySpec;
} CRYPT_SIGNED_INFO;

BOOL CRYPT_AsnEncodeCMSSignedInfo(CRYPT_SIGNED_INFO *, void *pvData,
 DWORD *pcbData);

BOOL CRYPT_AsnDecodeCMSSignedInfo(const BYTE *pbEncoded, DWORD cbEncoded,
 DWORD dwFlags, PCRYPT_DECODE_PARA pDecodePara,
 CRYPT_SIGNED_INFO *signedInfo, DWORD *pcbSignedInfo);

/* Helper function to check *pcbEncoded, set it to the required size, and
 * optionally to allocate memory.  Assumes pbEncoded is not NULL.
 * If CRYPT_ENCODE_ALLOC_FLAG is set in dwFlags, *pbEncoded will be set to a
 * pointer to the newly allocated memory.
 */
BOOL CRYPT_EncodeEnsureSpace(DWORD dwFlags, const CRYPT_ENCODE_PARA *pEncodePara,
 BYTE *pbEncoded, DWORD *pcbEncoded, DWORD bytesNeeded);

BOOL CRYPT_AsnDecodePKCSDigestedData(const BYTE *pbEncoded, DWORD cbEncoded,
 DWORD dwFlags, PCRYPT_DECODE_PARA pDecodePara,
 CRYPT_DIGESTED_DATA *digestedData, DWORD *pcbDigestedData);

BOOL WINAPI CRYPT_AsnEncodePubKeyInfoNoNull(DWORD dwCertEncodingType,
 LPCSTR lpszStructType, const void *pvStructInfo, DWORD dwFlags,
 PCRYPT_ENCODE_PARA pEncodePara, BYTE *pbEncoded, DWORD *pcbEncoded);

/* The following aren't defined in wincrypt.h, as they're "reserved" */
#define CERT_CERT_PROP_ID 32
#define CERT_CRL_PROP_ID  33
#define CERT_CTL_PROP_ID  34

/* Returns a handle to the default crypto provider; loads it if necessary.
 * Returns NULL on failure.
 */
HCRYPTPROV WINAPI I_CryptGetDefaultCryptProv(ALG_ID);

extern HINSTANCE hInstance;

void crypt_oid_init(void);
void crypt_oid_free(void);
void crypt_sip_free(void);
void root_store_free(void);
void default_chain_engine_free(void);

/* (Internal) certificate store types and functions */
struct WINE_CRYPTCERTSTORE;

struct properties;

typedef struct _context_t context_t;
struct context_vtbl;

struct _context_t {
    const struct context_vtbl *vtbl;
    LONG ref;
    struct WINE_CRYPTCERTSTORE *store;
    struct _context_t *linked;
    struct properties *properties;
    union {
        struct list entry;
        void *ptr;
    } u;
    DWORD info_size;
    union
    {
        CERT_CONTEXT cert;
        CRL_CONTEXT crl;
        CTL_CONTEXT ctl;
        BYTE ptr[1];
    };
};

static inline context_t *context_from_ptr(const void *ptr)
{
    return CONTAINING_RECORD( ptr, context_t, ptr );
}

static inline void *context_ptr(context_t *context)
{
    return &context->ptr;
}

static inline context_t *context_from_cert( const CERT_CONTEXT *cert )
{
    return CONTAINING_RECORD( cert, context_t, cert );
}

static inline context_t *context_from_crl( const CRL_CONTEXT *crl )
{
    return CONTAINING_RECORD( crl, context_t, crl );
}

static inline context_t *context_from_ctl( const CTL_CONTEXT *ctl )
{
    return CONTAINING_RECORD( ctl, context_t, ctl );
}

/* Some typedefs that make it easier to abstract which type of context we're
 * working with.
 */
typedef const void *(WINAPI *CreateContextFunc)(DWORD dwCertEncodingType,
 const BYTE *pbCertEncoded, DWORD cbCertEncoded);
typedef BOOL (WINAPI *AddContextToStoreFunc)(HCERTSTORE hCertStore,
 const void *context, DWORD dwAddDisposition, const void **ppStoreContext);
typedef BOOL (WINAPI *AddEncodedContextToStoreFunc)(HCERTSTORE hCertStore,
 DWORD dwCertEncodingType, const BYTE *pbEncoded, DWORD cbEncoded,
 DWORD dwAddDisposition, const void **ppContext);
typedef const void *(WINAPI *EnumContextsInStoreFunc)(HCERTSTORE hCertStore,
 const void *pPrevContext);
typedef DWORD (WINAPI *EnumPropertiesFunc)(const void *context, DWORD dwPropId);
typedef BOOL (WINAPI *GetContextPropertyFunc)(const void *context,
 DWORD dwPropID, void *pvData, DWORD *pcbData);
typedef BOOL (WINAPI *SetContextPropertyFunc)(const void *context,
 DWORD dwPropID, DWORD dwFlags, const void *pvData);
typedef BOOL (WINAPI *SerializeElementFunc)(const void *context, DWORD dwFlags,
 BYTE *pbElement, DWORD *pcbElement);
typedef BOOL (WINAPI *DeleteContextFunc)(const void *contex);

/* An abstract context (certificate, CRL, or CTL) interface */
typedef struct _WINE_CONTEXT_INTERFACE
{
    CreateContextFunc            create;
    AddContextToStoreFunc        addContextToStore;
    AddEncodedContextToStoreFunc addEncodedToStore;
    EnumContextsInStoreFunc      enumContextsInStore;
    EnumPropertiesFunc           enumProps;
    GetContextPropertyFunc       getProp;
    SetContextPropertyFunc       setProp;
    SerializeElementFunc         serialize;
    DeleteContextFunc            deleteFromStore;
} WINE_CONTEXT_INTERFACE;

extern const WINE_CONTEXT_INTERFACE *pCertInterface;
extern const WINE_CONTEXT_INTERFACE *pCRLInterface;
extern const WINE_CONTEXT_INTERFACE *pCTLInterface;

typedef struct WINE_CRYPTCERTSTORE * (*StoreOpenFunc)(HCRYPTPROV hCryptProv,
 DWORD dwFlags, const void *pvPara);

typedef struct _CONTEXT_FUNCS
{
  /* Called to add a context to a store.  If toReplace is not NULL,
   * context replaces toReplace in the store, and access checks should not be
   * performed.  Otherwise context is a new context, and it should only be
   * added if the store allows it.  If ppStoreContext is not NULL, the added
   * context should be returned in *ppStoreContext.
   */
    BOOL (*addContext)(struct WINE_CRYPTCERTSTORE*,context_t*,context_t*,context_t**,BOOL);
    context_t *(*enumContext)(struct WINE_CRYPTCERTSTORE *store, context_t *prev);
    BOOL (*delete)(struct WINE_CRYPTCERTSTORE*,context_t*);
} CONTEXT_FUNCS;

typedef enum _CertStoreType {
    StoreTypeMem,
    StoreTypeCollection,
    StoreTypeProvider,
    StoreTypeEmpty
} CertStoreType;

#define WINE_CRYPTCERTSTORE_MAGIC 0x74726563

/* A cert store is polymorphic through the use of function pointers.  A type
 * is still needed to distinguish collection stores from other types.
 * On the function pointers:
 * - closeStore is called when the store's ref count becomes 0
 * - control is optional, but should be implemented by any store that supports
 *   persistence
 */

typedef struct {
    void (*addref)(struct WINE_CRYPTCERTSTORE*);
    DWORD (*release)(struct WINE_CRYPTCERTSTORE*,DWORD);
    void (*releaseContext)(struct WINE_CRYPTCERTSTORE*,context_t*);
    BOOL (*control)(struct WINE_CRYPTCERTSTORE*,DWORD,DWORD,void const*);
    CONTEXT_FUNCS certs;
    CONTEXT_FUNCS crls;
    CONTEXT_FUNCS ctls;
} store_vtbl_t;

typedef struct WINE_CRYPTCERTSTORE
{
    DWORD                       dwMagic;
    LONG                        ref;
    DWORD                       dwOpenFlags;
    CertStoreType               type;
    const store_vtbl_t         *vtbl;
    struct properties          *properties;
} WINECRYPT_CERTSTORE;

void CRYPT_InitStore(WINECRYPT_CERTSTORE *store, DWORD dwFlags,
 CertStoreType type, const store_vtbl_t*);
void CRYPT_FreeStore(WINECRYPT_CERTSTORE *store);
BOOL WINAPI I_CertUpdateStore(HCERTSTORE store1, HCERTSTORE store2, DWORD unk0,
 DWORD unk1);

WINECRYPT_CERTSTORE *CRYPT_CollectionOpenStore(HCRYPTPROV hCryptProv,
 DWORD dwFlags, const void *pvPara);
WINECRYPT_CERTSTORE *CRYPT_ProvCreateStore(DWORD dwFlags,
 WINECRYPT_CERTSTORE *memStore, const CERT_STORE_PROV_INFO *pProvInfo);
WINECRYPT_CERTSTORE *CRYPT_ProvOpenStore(LPCSTR lpszStoreProvider,
 DWORD dwEncodingType, HCRYPTPROV hCryptProv, DWORD dwFlags,
 const void *pvPara);
WINECRYPT_CERTSTORE *CRYPT_RegOpenStore(HCRYPTPROV hCryptProv, DWORD dwFlags,
 const void *pvPara);
WINECRYPT_CERTSTORE *CRYPT_FileOpenStore(HCRYPTPROV hCryptProv, DWORD dwFlags,
 const void *pvPara);
WINECRYPT_CERTSTORE *CRYPT_FileNameOpenStoreA(HCRYPTPROV hCryptProv,
 DWORD dwFlags, const void *pvPara);
WINECRYPT_CERTSTORE *CRYPT_FileNameOpenStoreW(HCRYPTPROV hCryptProv,
 DWORD dwFlags, const void *pvPara);

void CRYPT_ImportSystemRootCertsToReg(void);
BOOL CRYPT_SerializeContextsToReg(HKEY key, DWORD flags, const WINE_CONTEXT_INTERFACE *contextInterface,
    HCERTSTORE memStore);
void CRYPT_RegReadSerializedFromReg(HKEY key, DWORD contextType,
    HCERTSTORE store, DWORD disposition);

DWORD CRYPT_IsCertificateSelfSigned(const CERT_CONTEXT *cert);

/* Allocates and initializes a certificate chain engine, but without creating
 * the root store.  Instead, it uses root, and assumes the caller has done any
 * checking necessary.
 */
HCERTCHAINENGINE CRYPT_CreateChainEngine(HCERTSTORE, DWORD, const CERT_CHAIN_ENGINE_CONFIG*);

/* Helper function for store reading functions and
 * CertAddSerializedElementToStore.  Returns a context of the appropriate type
 * if it can, or NULL otherwise.  Doesn't validate any of the properties in
 * the serialized context (for example, bad hashes are retained.)
 * *pdwContentType is set to the type of the returned context.
 */
const void *CRYPT_ReadSerializedElement(const BYTE *pbElement,
 DWORD cbElement, DWORD dwContextTypeFlags, DWORD *pdwContentType);

/* Reads contexts serialized in the file into the memory store.  Returns FALSE
 * if the file is not of the expected format.
 */
BOOL CRYPT_ReadSerializedStoreFromFile(HANDLE file, HCERTSTORE store);

/* Reads contexts serialized in the blob into the memory store.  Returns FALSE
 * if the file is not of the expected format.
 */
BOOL CRYPT_ReadSerializedStoreFromBlob(const CRYPT_DATA_BLOB *blob,
 HCERTSTORE store);

struct store_CERT_KEY_CONTEXT
{
    DWORD   cbSize;
    DWORD64 hCryptProv;
    DWORD   dwKeySpec;
};
void CRYPT_ConvertKeyContext(const struct store_CERT_KEY_CONTEXT *src, CERT_KEY_CONTEXT *dst);

/**
 *  String functions
 */

DWORD cert_name_to_str_with_indent(DWORD dwCertEncodingType, DWORD indent,
 const CERT_NAME_BLOB *pName, DWORD dwStrType, LPWSTR psz, DWORD csz);

/**
 *  Context functions
 */

extern context_t *context_create_copy( context_t *source, struct WINE_CRYPTCERTSTORE *store );
extern context_t *context_create_link( context_t *source, struct WINE_CRYPTCERTSTORE *store );

extern void cert_context_copy_properties( const CERT_CONTEXT *dst, const CERT_CONTEXT *src );
extern void crl_context_copy_properties( const CRL_CONTEXT *dst, const CRL_CONTEXT *src );
extern void ctl_context_copy_properties( const CTL_CONTEXT *dst, const CTL_CONTEXT *src );

void Context_AddRef(context_t*);
void Context_Release(context_t *context);
void Context_Free(context_t*);

/* properties.c */

struct properties *properties_create(void) DECLSPEC_HIDDEN;
void properties_destroy( struct properties *list ) DECLSPEC_HIDDEN;
BOOL properties_lookup( struct properties *list, DWORD id, CRYPT_DATA_BLOB *blob ) DECLSPEC_HIDDEN;
BOOL properties_insert( struct properties *list, DWORD id, const BYTE *data, size_t size ) DECLSPEC_HIDDEN;
void properties_remove( struct properties *list, DWORD id ) DECLSPEC_HIDDEN;
DWORD properties_enum_ids( struct properties *list, DWORD id ) DECLSPEC_HIDDEN;
void properties_copy( struct properties *dst, struct properties *src ) DECLSPEC_HIDDEN;

extern WINECRYPT_CERTSTORE empty_store;
void init_empty_store(void);

/**
 *  Utilities.
 */

/* Align up to a DWORD_PTR boundary
 */
#define ALIGN_DWORD_PTR(x) (((x) + sizeof(DWORD_PTR) - 1) & ~(sizeof(DWORD_PTR) - 1))
#define POINTER_ALIGN_DWORD_PTR(p) ((LPVOID)ALIGN_DWORD_PTR((DWORD_PTR)(p)))

/* Check if the OID is a small int
 */
#define IS_INTOID(x)    (((ULONG_PTR)(x) >> 16) == 0)

/* Unix interface */

typedef UINT64 cert_store_data_t;

struct open_cert_store_params
{
    CRYPT_DATA_BLOB *pfx;
    const WCHAR *password;
    cert_store_data_t *data_ret;
};

struct import_store_key_params
{
    cert_store_data_t data;
    void *buf;
    DWORD *buf_size;
};

struct import_store_cert_params
{
    cert_store_data_t data;
    unsigned int index;
    void *buf;
    DWORD *buf_size;
};

struct close_cert_store_params
{
    cert_store_data_t data;
};

struct enum_root_certs_params
{
    void  *buffer;
    DWORD  size;
    DWORD *needed;
};

enum unix_funcs
{
    unix_process_attach,
    unix_process_detach,
    unix_open_cert_store,
    unix_import_store_key,
    unix_import_store_cert,
    unix_close_cert_store,
    unix_enum_root_certs,
    unix_funcs_count,
};

#define CRYPT32_CALL( func, params ) WINE_UNIX_CALL( unix_ ## func, params )

#endif
