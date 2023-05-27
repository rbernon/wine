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

typedef BOOL (*CrlCompareFunc)(PCCRL_CONTEXT pCrlContext, DWORD dwType,
 DWORD dwFlags, const void *pvPara);

static BOOL compare_crl_any(PCCRL_CONTEXT pCrlContext, DWORD dwType,
 DWORD dwFlags, const void *pvPara)
{
    return TRUE;
}

static BOOL compare_crl_issued_by(PCCRL_CONTEXT pCrlContext, DWORD dwType,
 DWORD dwFlags, const void *pvPara)
{
    BOOL ret;

    if (pvPara)
    {
        PCCERT_CONTEXT issuer = pvPara;

        ret = CertCompareCertificateName(issuer->dwCertEncodingType,
         &issuer->pCertInfo->Subject, &pCrlContext->pCrlInfo->Issuer);
        if (ret && (dwFlags & CRL_FIND_ISSUED_BY_SIGNATURE_FLAG))
            ret = CryptVerifyCertificateSignatureEx(0,
             issuer->dwCertEncodingType,
             CRYPT_VERIFY_CERT_SIGN_SUBJECT_CRL, (void *)pCrlContext,
             CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT, (void *)issuer, 0, NULL);
        if (ret && (dwFlags & CRL_FIND_ISSUED_BY_AKI_FLAG))
        {
            PCERT_EXTENSION ext = CertFindExtension(
             szOID_AUTHORITY_KEY_IDENTIFIER2, pCrlContext->pCrlInfo->cExtension,
             pCrlContext->pCrlInfo->rgExtension);

            if (ext)
            {
                CERT_AUTHORITY_KEY_ID2_INFO *info;
                DWORD size;

                if ((ret = CryptDecodeObjectEx(X509_ASN_ENCODING,
                 X509_AUTHORITY_KEY_ID2, ext->Value.pbData, ext->Value.cbData,
                 CRYPT_DECODE_ALLOC_FLAG, NULL, &info, &size)))
                {
                    if (info->AuthorityCertIssuer.cAltEntry &&
                     info->AuthorityCertSerialNumber.cbData)
                    {
                        PCERT_ALT_NAME_ENTRY directoryName = NULL;
                        DWORD i;

                        for (i = 0; !directoryName &&
                         i < info->AuthorityCertIssuer.cAltEntry; i++)
                            if (info->AuthorityCertIssuer.rgAltEntry[i].
                             dwAltNameChoice == CERT_ALT_NAME_DIRECTORY_NAME)
                                directoryName =
                                 &info->AuthorityCertIssuer.rgAltEntry[i];
                        if (directoryName)
                        {
                            ret = CertCompareCertificateName(
                             issuer->dwCertEncodingType,
                             &issuer->pCertInfo->Subject,
                             &directoryName->DirectoryName);
                            if (ret)
                                ret = CertCompareIntegerBlob(
                                 &issuer->pCertInfo->SerialNumber,
                                 &info->AuthorityCertSerialNumber);
                        }
                        else
                        {
                            FIXME("no supported name type in authority key id2\n");
                            ret = FALSE;
                        }
                    }
                    else if (info->KeyId.cbData)
                    {
                        DWORD size;

                        ret = CertGetCertificateContextProperty(issuer,
                         CERT_KEY_IDENTIFIER_PROP_ID, NULL, &size);
                        if (ret && size == info->KeyId.cbData)
                        {
                            LPBYTE buf = CryptMemAlloc(size);

                            if (buf)
                            {
                                CertGetCertificateContextProperty(issuer,
                                 CERT_KEY_IDENTIFIER_PROP_ID, buf, &size);
                                ret = !memcmp(buf, info->KeyId.pbData, size);
                                CryptMemFree(buf);
                            }
                            else
                                ret = FALSE;
                        }
                        else
                            ret = FALSE;
                    }
                    else
                    {
                        FIXME("unsupported value for AKI extension\n");
                        ret = FALSE;
                    }
                    LocalFree(info);
                }
            }
            /* else: a CRL without an AKI matches any cert */
        }
    }
    else
        ret = TRUE;
    return ret;
}

static BOOL compare_crl_existing(PCCRL_CONTEXT pCrlContext, DWORD dwType,
 DWORD dwFlags, const void *pvPara)
{
    BOOL ret;

    if (pvPara)
    {
        PCCRL_CONTEXT crl = pvPara;

        ret = CertCompareCertificateName(pCrlContext->dwCertEncodingType,
         &pCrlContext->pCrlInfo->Issuer, &crl->pCrlInfo->Issuer);
    }
    else
        ret = TRUE;
    return ret;
}

static BOOL compare_crl_issued_for(PCCRL_CONTEXT pCrlContext, DWORD dwType,
 DWORD dwFlags, const void *pvPara)
{
    const CRL_FIND_ISSUED_FOR_PARA *para = pvPara;
    BOOL ret;

    ret = CertCompareCertificateName(para->pIssuerCert->dwCertEncodingType,
     &para->pIssuerCert->pCertInfo->Issuer, &pCrlContext->pCrlInfo->Issuer);
    return ret;
}

PCCRL_CONTEXT WINAPI CertFindCRLInStore(HCERTSTORE hCertStore,
 DWORD dwCertEncodingType, DWORD dwFindFlags, DWORD dwFindType,
 const void *pvFindPara, PCCRL_CONTEXT pPrevCrlContext)
{
    PCCRL_CONTEXT ret;
    CrlCompareFunc compare;

    TRACE("(%p, %ld, %ld, %ld, %p, %p)\n", hCertStore, dwCertEncodingType,
	 dwFindFlags, dwFindType, pvFindPara, pPrevCrlContext);

    switch (dwFindType)
    {
    case CRL_FIND_ANY:
        compare = compare_crl_any;
        break;
    case CRL_FIND_ISSUED_BY:
        compare = compare_crl_issued_by;
        break;
    case CRL_FIND_EXISTING:
        compare = compare_crl_existing;
        break;
    case CRL_FIND_ISSUED_FOR:
        compare = compare_crl_issued_for;
        break;
    default:
        FIXME("find type %08lx unimplemented\n", dwFindType);
        compare = NULL;
    }

    if (compare)
    {
        BOOL matches = FALSE;

        ret = pPrevCrlContext;
        do {
            ret = CertEnumCRLsInStore(hCertStore, ret);
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

PCCRL_CONTEXT WINAPI CertGetCRLFromStore(HCERTSTORE hCertStore,
 PCCERT_CONTEXT pIssuerContext, PCCRL_CONTEXT pPrevCrlContext, DWORD *pdwFlags)
{
    static const DWORD supportedFlags = CERT_STORE_SIGNATURE_FLAG |
     CERT_STORE_TIME_VALIDITY_FLAG | CERT_STORE_BASE_CRL_FLAG |
     CERT_STORE_DELTA_CRL_FLAG;
    PCCRL_CONTEXT ret;

    TRACE("(%p, %p, %p, %08lx)\n", hCertStore, pIssuerContext, pPrevCrlContext,
     *pdwFlags);

    if (*pdwFlags & ~supportedFlags)
    {
        SetLastError(E_INVALIDARG);
        return NULL;
    }
    if (pIssuerContext)
        ret = CertFindCRLInStore(hCertStore, pIssuerContext->dwCertEncodingType,
         0, CRL_FIND_ISSUED_BY, pIssuerContext, pPrevCrlContext);
    else
        ret = CertFindCRLInStore(hCertStore, 0, 0, CRL_FIND_ANY, NULL,
         pPrevCrlContext);
    if (ret)
    {
        if (*pdwFlags & CERT_STORE_TIME_VALIDITY_FLAG)
        {
            if (0 == CertVerifyCRLTimeValidity(NULL, ret->pCrlInfo))
                *pdwFlags &= ~CERT_STORE_TIME_VALIDITY_FLAG;
        }
        if (*pdwFlags & CERT_STORE_SIGNATURE_FLAG)
        {
            if (CryptVerifyCertificateSignatureEx(0, ret->dwCertEncodingType,
             CRYPT_VERIFY_CERT_SIGN_SUBJECT_CRL, (void *)ret,
             CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT, (void *)pIssuerContext, 0,
             NULL))
                *pdwFlags &= ~CERT_STORE_SIGNATURE_FLAG;
        }
    }
    return ret;
}

static BOOL compare_dist_point_name( const CRL_DIST_POINT_NAME *name1, const CRL_DIST_POINT_NAME *name2 )
{
    DWORD i;

    if (name1->dwDistPointNameChoice != name2->dwDistPointNameChoice) return FALSE;
    if (name1->dwDistPointNameChoice != CRL_DIST_POINT_FULL_NAME) return TRUE;

    if (name1->u.FullName.cAltEntry != name2->u.FullName.cAltEntry) return FALSE;
    for (i = 0; i < name1->u.FullName.cAltEntry; i++)
    {
        const CERT_ALT_NAME_ENTRY *entry1 = &name1->u.FullName.rgAltEntry[i];
        const CERT_ALT_NAME_ENTRY *entry2 = &name2->u.FullName.rgAltEntry[i];

        if (entry1->dwAltNameChoice != entry2->dwAltNameChoice) return FALSE;
        switch (entry1->dwAltNameChoice)
        {
        case CERT_ALT_NAME_URL:
            if (wcsicmp( entry1->u.pwszURL, entry2->u.pwszURL )) return FALSE;
            break;
        case CERT_ALT_NAME_DIRECTORY_NAME:
            if (entry1->u.DirectoryName.cbData != entry2->u.DirectoryName.cbData) return FALSE;
            if (memcmp( entry1->u.DirectoryName.pbData, entry2->u.DirectoryName.pbData,
                        entry1->u.DirectoryName.cbData )) return FALSE;
            break;
        default:
            FIXME( "unimplemented for type %ld\n", entry1->dwAltNameChoice );
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL match_dist_point_with_issuing_dist_point( const CRL_DIST_POINT *dist, const CRL_ISSUING_DIST_POINT *idp )
{
    const CRYPT_BIT_BLOB *dist_flags = &dist->ReasonFlags, *idp_flags = &idp->OnlySomeReasonFlags;

    /* While RFC 5280, section 4.2.1.13 recommends against segmenting
     * CRL distribution points by reasons, it doesn't preclude doing so.
     * "This profile RECOMMENDS against segmenting CRLs by reason code."
     * If the issuing distribution point for this CRL is only valid for
     * some reasons, only match if the reasons covered also match the
     * reasons in the CRL distribution point.
     */
    if (idp_flags->cbData && idp_flags->cbData != dist_flags->cbData) return FALSE;
    if (memcmp( idp_flags->pbData, dist_flags->pbData, dist_flags->cbData )) return FALSE;
    return compare_dist_point_name( &idp->DistPointName, &dist->DistPointName );
}

BOOL WINAPI CertIsValidCRLForCertificate(PCCERT_CONTEXT pCert,
 PCCRL_CONTEXT pCrl, DWORD dwFlags, void *pvReserved)
{
    PCERT_EXTENSION ext;
    BOOL ret;

    TRACE("(%p, %p, %08lx, %p)\n", pCert, pCrl, dwFlags, pvReserved);

    if (!pCert)
        return TRUE;

    if ((ext = CertFindExtension(szOID_ISSUING_DIST_POINT,
     pCrl->pCrlInfo->cExtension, pCrl->pCrlInfo->rgExtension)))
    {
        CRL_ISSUING_DIST_POINT *idp;
        DWORD size;

        if ((ret = CryptDecodeObjectEx(pCrl->dwCertEncodingType,
         X509_ISSUING_DIST_POINT, ext->Value.pbData, ext->Value.cbData,
         CRYPT_DECODE_ALLOC_FLAG, NULL, &idp, &size)))
        {
            if ((ext = CertFindExtension(szOID_CRL_DIST_POINTS,
             pCert->pCertInfo->cExtension, pCert->pCertInfo->rgExtension)))
            {
                CRL_DIST_POINTS_INFO *distPoints;

                if ((ret = CryptDecodeObjectEx(pCert->dwCertEncodingType,
                 X509_CRL_DIST_POINTS, ext->Value.pbData, ext->Value.cbData,
                 CRYPT_DECODE_ALLOC_FLAG, NULL, &distPoints, &size)))
                {
                    DWORD i;

                    ret = FALSE;
                    for (i = 0; !ret && i < distPoints->cDistPoint; i++)
                        ret = match_dist_point_with_issuing_dist_point(
                         &distPoints->rgDistPoint[i], idp);
                    if (!ret)
                        SetLastError(CRYPT_E_NO_MATCH);
                    LocalFree(distPoints);
                }
            }
            else
            {
                /* no CRL dist points extension in cert, can't match the CRL
                 * (which has an issuing dist point extension)
                 */
                ret = FALSE;
                SetLastError(CRYPT_E_NO_MATCH);
            }
            LocalFree(idp);
        }
    }
    else
        ret = TRUE;
    return ret;
}

static CRL_ENTRY *find_certificate_in_crl( CERT_INFO *cert, const CRL_INFO *crl )
{
    DWORD i;

    for (i = 0; i < crl->cCRLEntry; i++)
        if (CertCompareIntegerBlob( &crl->rgCRLEntry[i].SerialNumber, &cert->SerialNumber ))
            return &crl->rgCRLEntry[i];

    return NULL;
}

BOOL WINAPI CertFindCertificateInCRL( const CERT_CONTEXT *cert, const CRL_CONTEXT *crl,
                                      DWORD flags, void *reserved, CRL_ENTRY **entry )
{
    TRACE( "cert %p, crl %p, flags %#lx, reserved %p, entry %p\n", cert, crl, flags, reserved, entry );
    *entry = find_certificate_in_crl( cert->pCertInfo, crl->pCrlInfo );
    return TRUE;
}

BOOL WINAPI CertVerifyCRLRevocation( DWORD encoding, CERT_INFO *cert, DWORD count, CRL_INFO **crls )
{
    TRACE( "encoding %#lx, cert %p, count %lu, crls %p\n", encoding, cert, count, crls );
    while (count--) if (find_certificate_in_crl( cert, crls[count] )) return FALSE;
    return TRUE;
}

LONG WINAPI CertVerifyCRLTimeValidity( FILETIME *time, CRL_INFO *crl )
{
    FILETIME tmp;
    LONG ret;

    TRACE( "time %p, crl %p\n", time, crl );

    if (!time) GetSystemTimeAsFileTime( (time = &tmp) );
    if ((ret = CompareFileTime( time, &crl->ThisUpdate )) >= 0)
    {
        ret = CompareFileTime( time, &crl->NextUpdate );
        if (ret < 0) ret = 0;
    }

    return ret;
}
