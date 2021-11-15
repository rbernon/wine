/*
 * ntoskrnl.exe testing framework
 *
 * Copyright 2015 Sebastian Lackner
 * Copyright 2015 Michael Müller
 * Copyright 2015 Christian Costa
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

#ifndef __WINE_DRIVER_HID_H
#define __WINE_DRIVER_HID_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winioctl.h"
#include "winternl.h"
#include "wincrypt.h"
#include "winreg.h"
#include "winsvc.h"
#include "winuser.h"
#include "winnls.h"

#include "mscat.h"
#include "mssip.h"
#include "ntsecapi.h"
#include "setupapi.h"
#include "cfgmgr32.h"
#include "newdev.h"
#include "objbase.h"

#include "ddk/wdm.h"

#include "wine/mssign.h"

#define IOCTL_WINETEST_HID_SET_EXPECT    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x800, METHOD_IN_DIRECT, FILE_ANY_ACCESS)
#define IOCTL_WINETEST_HID_WAIT_EXPECT   CTL_CODE(FILE_DEVICE_KEYBOARD, 0x801, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_WINETEST_HID_SEND_INPUT    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x802, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

struct hid_expect
{
    DWORD code;
    DWORD ret_length;
    DWORD ret_status;
    BYTE todo;   /* missing on wine */
    BYTE broken; /* missing on some win versions */
    BYTE wine_only;
    BYTE report_id;
    BYTE report_len;
    BYTE report_buf[128];
    char context[64];
};

/* kernel/user shared data */
struct winetest_shared_data
{
    int running_under_wine;
    int winetest_report_success;
    int winetest_debug;
    int failures;
    int todo_failures;
};

#ifdef __WINE_WINE_TEST_H

static struct winetest_shared_data *test_data;
static HANDLE test_data_mapping;
static HANDLE okfile;

static HRESULT (WINAPI *pSignerSign)( SIGNER_SUBJECT_INFO *subject, SIGNER_CERT *cert,
                                      SIGNER_SIGNATURE_INFO *signature, SIGNER_PROVIDER_INFO *provider,
                                      const WCHAR *timestamp, CRYPT_ATTRIBUTES *attr, void *sip_data );

static inline BOOL driver_hid_init(void)
{
    BOOL is_wow64;

    subtest( "driver_hid.h" );

    pSignerSign = (void *)GetProcAddress( LoadLibraryW( L"mssign32" ), "SignerSign" );

    if (IsWow64Process( GetCurrentProcess(), &is_wow64 ) && is_wow64)
    {
        skip( "Running in WoW64.\n" );
        return FALSE;
    }

    test_data_mapping = CreateFileMappingW( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(*test_data),
                                            L"Global\\winetest_dinput_section" );
    if (!test_data_mapping && GetLastError() == ERROR_ACCESS_DENIED)
    {
        win_skip( "Failed to create test data mapping.\n" );
        return FALSE;
    }
    ok( !!test_data_mapping, "got error %u\n", GetLastError() );
    test_data = MapViewOfFile( test_data_mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 1024 );
    test_data->running_under_wine = !strcmp( winetest_platform, "wine" );
    test_data->winetest_report_success = winetest_report_success;
    test_data->winetest_debug = winetest_debug;

    okfile = CreateFileW( L"C:\\windows\\winetest_dinput_okfile", GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL );
    ok( okfile != INVALID_HANDLE_VALUE, "failed to create file, error %u\n", GetLastError() );

    subtest( "driver_hid" );
    return TRUE;
}

static inline void driver_hid_cleanup(void)
{
    UnmapViewOfFile( test_data );
    CloseHandle( test_data_mapping );
    CloseHandle( okfile );
    DeleteFileW( L"C:\\windows\\winetest_dinput_okfile" );
}

static inline const CERT_CONTEXT *testsign_sign( const WCHAR *filename )
{
    BYTE encoded_name[100], encoded_key_id[200], public_key_info_buffer[1000];
    BYTE hash_buffer[16], cert_buffer[1000], provider_nameA[100], serial[16];
    CERT_PUBLIC_KEY_INFO *public_key_info = (CERT_PUBLIC_KEY_INFO *)public_key_info_buffer;
    SIGNER_SIGNATURE_INFO signature = {sizeof(SIGNER_SIGNATURE_INFO)};
    SIGNER_CERT_STORE_INFO store = {sizeof(SIGNER_CERT_STORE_INFO)};
    SIGNER_ATTR_AUTHCODE authcode = {sizeof(SIGNER_ATTR_AUTHCODE)};
    SIGNER_SUBJECT_INFO subject = {sizeof(SIGNER_SUBJECT_INFO)};
    SIGNER_FILE_INFO file = {sizeof(SIGNER_FILE_INFO)};
    SIGNER_CERT signer = {sizeof(SIGNER_CERT)};
    CRYPT_KEY_PROV_INFO provider_info = {0};
    CRYPT_ALGORITHM_IDENTIFIER algid = {0};
    CERT_AUTHORITY_KEY_ID_INFO key_info;
    HCERTSTORE root_store, pub_store;
    CERT_INFO cert_info = {0};
    WCHAR provider_nameW[100];
    const CERT_CONTEXT *cert;
    CERT_EXTENSION extension;
    DWORD size, index = 0;
    HCRYPTPROV provider;
    HCRYPTKEY key;
    HRESULT hr;
    BOOL ret;

    ret = CryptAcquireContextW( &provider, L"wine_testsign", NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET );
    if (!ret && GetLastError() == NTE_EXISTS)
    {
        ret = CryptAcquireContextW( &provider, L"wine_testsign", NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET );
        ok( ret, "Failed to delete container, error %#x\n", GetLastError() );
        ret = CryptAcquireContextW( &provider, L"wine_testsign", NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET );
    }
    ok( ret, "Failed to create container, error %#x\n", GetLastError() );

    ret = CryptGenKey( provider, AT_SIGNATURE, CRYPT_EXPORTABLE, &key );
    ok( ret, "Failed to create key, error %#x\n", GetLastError() );
    ret = CryptDestroyKey( key );
    ok( ret, "Failed to destroy key, error %#x\n", GetLastError() );
    ret = CryptGetUserKey( provider, AT_SIGNATURE, &key );
    ok( ret, "Failed to get user key, error %#x\n", GetLastError() );
    ret = CryptDestroyKey( key );
    ok( ret, "Failed to destroy key, error %#x\n", GetLastError() );

    size = sizeof(encoded_name);
    ret = CertStrToNameW( X509_ASN_ENCODING, L"CN=winetest_cert", CERT_X500_NAME_STR, NULL,
                          encoded_name, &size, NULL );
    ok( ret, "Failed to convert name, error %#x\n", GetLastError() );
    key_info.CertIssuer.cbData = size;
    key_info.CertIssuer.pbData = encoded_name;

    size = sizeof(public_key_info_buffer);
    ret = CryptExportPublicKeyInfo( provider, AT_SIGNATURE, X509_ASN_ENCODING, public_key_info, &size );
    ok( ret, "Failed to export public key, error %#x\n", GetLastError() );
    cert_info.SubjectPublicKeyInfo = *public_key_info;

    size = sizeof(hash_buffer);
    ret = CryptHashPublicKeyInfo( provider, CALG_MD5, 0, X509_ASN_ENCODING, public_key_info, hash_buffer, &size );
    ok( ret, "Failed to hash public key, error %#x\n", GetLastError() );

    key_info.KeyId.cbData = size;
    key_info.KeyId.pbData = hash_buffer;

    RtlGenRandom( serial, sizeof(serial) );
    key_info.CertSerialNumber.cbData = sizeof(serial);
    key_info.CertSerialNumber.pbData = serial;

    size = sizeof(encoded_key_id);
    ret = CryptEncodeObject( X509_ASN_ENCODING, X509_AUTHORITY_KEY_ID, &key_info, encoded_key_id, &size );
    ok( ret, "Failed to convert name, error %#x\n", GetLastError() );

    extension.pszObjId = (char *)szOID_AUTHORITY_KEY_IDENTIFIER;
    extension.fCritical = TRUE;
    extension.Value.cbData = size;
    extension.Value.pbData = encoded_key_id;

    cert_info.dwVersion = CERT_V3;
    cert_info.SerialNumber = key_info.CertSerialNumber;
    cert_info.SignatureAlgorithm.pszObjId = (char *)szOID_RSA_SHA1RSA;
    cert_info.Issuer = key_info.CertIssuer;
    GetSystemTimeAsFileTime( &cert_info.NotBefore );
    GetSystemTimeAsFileTime( &cert_info.NotAfter );
    cert_info.NotAfter.dwHighDateTime += 1;
    cert_info.Subject = key_info.CertIssuer;
    cert_info.cExtension = 1;
    cert_info.rgExtension = &extension;
    algid.pszObjId = (char *)szOID_RSA_SHA1RSA;
    size = sizeof(cert_buffer);
    ret = CryptSignAndEncodeCertificate( provider, AT_SIGNATURE, X509_ASN_ENCODING, X509_CERT_TO_BE_SIGNED,
                                         &cert_info, &algid, NULL, cert_buffer, &size );
    ok( ret, "Failed to create certificate, error %#x\n", GetLastError() );

    cert = CertCreateCertificateContext( X509_ASN_ENCODING, cert_buffer, size );
    ok( !!cert, "Failed to create context, error %#x\n", GetLastError() );

    size = sizeof(provider_nameA);
    ret = CryptGetProvParam( provider, PP_NAME, provider_nameA, &size, 0 );
    ok( ret, "Failed to get prov param, error %#x\n", GetLastError() );
    MultiByteToWideChar( CP_ACP, 0, (char *)provider_nameA, -1, provider_nameW, ARRAY_SIZE(provider_nameW) );

    provider_info.pwszContainerName = (WCHAR *)L"wine_testsign";
    provider_info.pwszProvName = provider_nameW;
    provider_info.dwProvType = PROV_RSA_FULL;
    provider_info.dwKeySpec = AT_SIGNATURE;
    ret = CertSetCertificateContextProperty( cert, CERT_KEY_PROV_INFO_PROP_ID, 0, &provider_info );
    ok( ret, "Failed to set provider info, error %#x\n", GetLastError() );

    ret = CryptReleaseContext( provider, 0 );
    ok( ret, "failed to release context, error %u\n", GetLastError() );

    root_store = CertOpenStore( CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"root" );
    if (!root_store && GetLastError() == ERROR_ACCESS_DENIED)
    {
        win_skip( "Failed to open root store.\n" );
        ret = CertFreeCertificateContext( cert );
        ok( ret, "Failed to free certificate, error %u\n", GetLastError() );
        return NULL;
    }
    ok( !!root_store, "Failed to open store, error %u\n", GetLastError() );
    ret = CertAddCertificateContextToStore( root_store, cert, CERT_STORE_ADD_ALWAYS, NULL );
    if (!ret && GetLastError() == ERROR_ACCESS_DENIED)
    {
        win_skip( "Failed to add self-signed certificate to store.\n" );
        ret = CertFreeCertificateContext( cert );
        ok( ret, "Failed to free certificate, error %u\n", GetLastError() );
        ret = CertCloseStore( root_store, CERT_CLOSE_STORE_CHECK_FLAG );
        ok( ret, "Failed to close store, error %u\n", GetLastError() );
        return NULL;
    }
    ok( ret, "Failed to add certificate, error %u\n", GetLastError() );
    ret = CertCloseStore( root_store, CERT_CLOSE_STORE_CHECK_FLAG );
    ok( ret, "Failed to close store, error %u\n", GetLastError() );

    pub_store = CertOpenStore( CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0,
                               CERT_SYSTEM_STORE_LOCAL_MACHINE, L"trustedpublisher" );
    ok( !!pub_store, "Failed to open store, error %u\n", GetLastError() );
    ret = CertAddCertificateContextToStore( pub_store, cert, CERT_STORE_ADD_ALWAYS, NULL );
    ok( ret, "Failed to add certificate, error %u\n", GetLastError() );
    ret = CertCloseStore( pub_store, CERT_CLOSE_STORE_CHECK_FLAG );
    ok( ret, "Failed to close store, error %u\n", GetLastError() );

    subject.dwSubjectChoice = 1;
    subject.pdwIndex = &index;
    subject.pSignerFileInfo = &file;
    file.pwszFileName = (WCHAR *)filename;
    signer.dwCertChoice = 2;
    signer.pCertStoreInfo = &store;
    store.pSigningCert = cert;
    store.dwCertPolicy = 0;
    signature.algidHash = CALG_SHA_256;
    signature.dwAttrChoice = SIGNER_AUTHCODE_ATTR;
    signature.pAttrAuthcode = &authcode;
    authcode.pwszName = L"";
    authcode.pwszInfo = L"";
    hr = pSignerSign( &subject, &signer, &signature, NULL, NULL, NULL, NULL );
    todo_wine
    ok( hr == S_OK || broken( hr == NTE_BAD_ALGID ) /* < 7 */, "Failed to sign, hr %#x\n", hr );

    return cert;
}

static inline void testsign_cleanup( const CERT_CONTEXT *cert )
{
    HCERTSTORE root_store, pub_store;
    const CERT_CONTEXT *store_cert;
    HCRYPTPROV provider;
    BOOL ret;

    root_store = CertOpenStore( CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"root" );
    ok( !!root_store, "Failed to open store, error %u\n", GetLastError() );
    store_cert = CertFindCertificateInStore( root_store, X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, cert, NULL );
    ok( !!store_cert, "Failed to find root certificate, error %u\n", GetLastError() );
    ret = CertDeleteCertificateFromStore( store_cert );
    ok( ret, "Failed to remove certificate, error %u\n", GetLastError() );
    ret = CertCloseStore( root_store, CERT_CLOSE_STORE_CHECK_FLAG );
    ok( ret, "Failed to close store, error %u\n", GetLastError() );

    pub_store = CertOpenStore( CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0,
                               CERT_SYSTEM_STORE_LOCAL_MACHINE, L"trustedpublisher" );
    ok( !!pub_store, "Failed to open store, error %u\n", GetLastError() );
    store_cert = CertFindCertificateInStore( pub_store, X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, cert, NULL );
    ok( !!store_cert, "Failed to find publisher certificate, error %u\n", GetLastError() );
    ret = CertDeleteCertificateFromStore( store_cert );
    ok( ret, "Failed to remove certificate, error %u\n", GetLastError() );
    ret = CertCloseStore( pub_store, CERT_CLOSE_STORE_CHECK_FLAG );
    ok( ret, "Failed to close store, error %u\n", GetLastError() );

    ret = CertFreeCertificateContext( cert );
    ok( ret, "Failed to free certificate, error %u\n", GetLastError() );

    ret = CryptAcquireContextW( &provider, L"wine_testsign", NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET );
    ok( ret, "Failed to delete container, error %#x\n", GetLastError() );
}

static inline void load_resource( const WCHAR *name, WCHAR *filename )
{
    static WCHAR path[MAX_PATH];
    DWORD written;
    HANDLE file;
    HRSRC res;
    void *ptr;

    GetTempPathW( ARRAY_SIZE(path), path );
    GetTempFileNameW( path, name, 0, filename );

    file = CreateFileW( filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( file != INVALID_HANDLE_VALUE, "failed to create %s, error %u\n", debugstr_w(filename), GetLastError() );

    res = FindResourceW( NULL, name, L"TESTDLL" );
    ok( res != 0, "couldn't find resource\n" );
    ptr = LockResource( LoadResource( GetModuleHandleW( NULL ), res ) );
    WriteFile( file, ptr, SizeofResource( GetModuleHandleW( NULL ), res ), &written, NULL );
    ok( written == SizeofResource( GetModuleHandleW( NULL ), res ), "couldn't write resource\n" );
    CloseHandle( file );
}

#ifdef __i386__
#define EXT "x86"
#elif defined(__x86_64__)
#define EXT "amd64"
#elif defined(__arm__)
#define EXT "arm"
#elif defined(__aarch64__)
#define EXT "arm64"
#else
#define EXT
#endif

static const char inf_text[] =
    "[Version]\n"
    "Signature=$Chicago$\n"
    "ClassGuid={4d36e97d-e325-11ce-bfc1-08002be10318}\n"
    "CatalogFile=winetest.cat\n"
    "DriverVer=09/21/2006,6.0.5736.1\n"

    "[Manufacturer]\n"
    "Wine=mfg_section,NT" EXT "\n"

    "[mfg_section.NT" EXT "]\n"
    "Wine test root driver=device_section,test_hardware_id\n"

    "[device_section.NT" EXT "]\n"
    "CopyFiles=file_section\n"

    "[device_section.NT" EXT ".Services]\n"
    "AddService=winetest,0x2,svc_section\n"

    "[file_section]\n"
    "winetest.sys\n"

    "[SourceDisksFiles]\n"
    "winetest.sys=1\n"

    "[SourceDisksNames]\n"
    "1=,winetest.sys\n"

    "[DestinationDirs]\n"
    "DefaultDestDir=12\n"

    "[svc_section]\n"
    "ServiceBinary=%12%\\winetest.sys\n"
    "ServiceType=1\n"
    "StartType=3\n"
    "ErrorControl=1\n"
    "LoadOrderGroup=Extended Base\n"
    "DisplayName=\"winetest bus driver\"\n"
    "; they don't sleep anymore, on the beach\n";

static inline void add_file_to_catalog( HANDLE catalog, const WCHAR *file )
{
    SIP_SUBJECTINFO subject_info = {sizeof(SIP_SUBJECTINFO)};
    SIP_INDIRECT_DATA *indirect_data;
    const WCHAR *filepart = file;
    CRYPTCATMEMBER *member;
    WCHAR hash_buffer[100];
    GUID subject_guid;
    unsigned int i;
    DWORD size;
    BOOL ret;

    ret = CryptSIPRetrieveSubjectGuidForCatalogFile( file, NULL, &subject_guid );
    todo_wine
    ok( ret, "Failed to get subject guid, error %u\n", GetLastError() );

    size = 0;
    subject_info.pgSubjectType = &subject_guid;
    subject_info.pwsFileName = file;
    subject_info.DigestAlgorithm.pszObjId = (char *)szOID_OIWSEC_sha1;
    subject_info.dwFlags = SPC_INC_PE_RESOURCES_FLAG | SPC_INC_PE_IMPORT_ADDR_TABLE_FLAG |
                           SPC_EXC_PE_PAGE_HASHES_FLAG | 0x10000;
    ret = CryptSIPCreateIndirectData( &subject_info, &size, NULL );
    todo_wine
    ok( ret, "Failed to get indirect data size, error %u\n", GetLastError() );

    indirect_data = malloc( size );
    ret = CryptSIPCreateIndirectData( &subject_info, &size, indirect_data );
    todo_wine
    ok( ret, "Failed to get indirect data, error %u\n", GetLastError() );
    if (ret)
    {
        memset( hash_buffer, 0, sizeof(hash_buffer) );
        for (i = 0; i < indirect_data->Digest.cbData; ++i)
            swprintf( &hash_buffer[i * 2], 2, L"%02X", indirect_data->Digest.pbData[i] );

        member = CryptCATPutMemberInfo( catalog, (WCHAR *)file, hash_buffer, &subject_guid,
                                        0, size, (BYTE *)indirect_data );
        ok( !!member, "Failed to write member, error %u\n", GetLastError() );

        if (wcsrchr( file, '\\' )) filepart = wcsrchr( file, '\\' ) + 1;

        ret = !!CryptCATPutAttrInfo( catalog, member, (WCHAR *)L"File",
                                     CRYPTCAT_ATTR_NAMEASCII | CRYPTCAT_ATTR_DATAASCII | CRYPTCAT_ATTR_AUTHENTICATED,
                                     (wcslen( filepart ) + 1) * 2, (BYTE *)filepart );
        ok( ret, "Failed to write attr, error %u\n", GetLastError() );

        ret = !!CryptCATPutAttrInfo( catalog, member, (WCHAR *)L"OSAttr",
                                     CRYPTCAT_ATTR_NAMEASCII | CRYPTCAT_ATTR_DATAASCII | CRYPTCAT_ATTR_AUTHENTICATED,
                                     sizeof(L"2:6.0"), (BYTE *)L"2:6.0" );
        ok( ret, "Failed to write attr, error %u\n", GetLastError() );
    }
}

static inline void unload_driver( SC_HANDLE service )
{
    SERVICE_STATUS status;

    ControlService( service, SERVICE_CONTROL_STOP, &status );
    while (status.dwCurrentState == SERVICE_STOP_PENDING)
    {
        BOOL ret;
        Sleep( 100 );
        ret = QueryServiceStatus( service, &status );
        ok( ret, "QueryServiceStatus failed: %u\n", GetLastError() );
    }
    ok( status.dwCurrentState == SERVICE_STOPPED, "expected SERVICE_STOPPED, got %d\n", status.dwCurrentState );

    DeleteService( service );
    CloseServiceHandle( service );
}

static inline void pnp_driver_stop(void)
{
    SP_DEVINFO_DATA device = {sizeof(SP_DEVINFO_DATA)};
    WCHAR path[MAX_PATH], dest[MAX_PATH], *filepart;
    SC_HANDLE manager, service;
    char buffer[512];
    HDEVINFO set;
    HANDLE file;
    DWORD size;
    BOOL ret;

    set = SetupDiCreateDeviceInfoList( NULL, NULL );
    ok( set != INVALID_HANDLE_VALUE, "failed to create device list, error %u\n", GetLastError() );

    ret = SetupDiOpenDeviceInfoW( set, L"root\\winetest\\0", NULL, 0, &device );
    if (!ret && GetLastError() == ERROR_NO_SUCH_DEVINST)
    {
        ret = SetupDiDestroyDeviceInfoList( set );
        ok( ret, "failed to destroy set, error %u\n", GetLastError() );
        return;
    }
    ok( ret, "failed to open device, error %u\n", GetLastError() );

    ret = SetupDiCallClassInstaller( DIF_REMOVE, set, &device );
    ok( ret, "failed to remove device, error %u\n", GetLastError() );

    file = CreateFileW( L"\\\\?\\root#winetest#0#{deadbeef-29ef-4538-a5fd-b69573a362c0}", 0, 0,
                        NULL, OPEN_EXISTING, 0, NULL );
    ok( file == INVALID_HANDLE_VALUE, "expected failure\n" );
    ok( GetLastError() == ERROR_FILE_NOT_FOUND, "got error %u\n", GetLastError() );

    ret = SetupDiDestroyDeviceInfoList( set );
    ok( ret, "failed to destroy set, error %u\n", GetLastError() );

    /* Windows stops the service but does not delete it. */
    manager = OpenSCManagerW( NULL, NULL, SC_MANAGER_CONNECT );
    ok( !!manager, "failed to open service manager, error %u\n", GetLastError() );

    service = OpenServiceW( manager, L"winetest", SERVICE_STOP | DELETE );
    if (service) unload_driver( service );
    else ok( GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST, "got error %u\n", GetLastError() );

    CloseServiceHandle( manager );

    SetFilePointer( okfile, 0, NULL, FILE_BEGIN );
    do
    {
        ReadFile( okfile, buffer, sizeof(buffer), &size, NULL );
        printf( "%.*s", size, buffer );
    } while (size == sizeof(buffer));
    SetFilePointer( okfile, 0, NULL, FILE_BEGIN );
    SetEndOfFile( okfile );

    winetest_add_failures( InterlockedExchange( &test_data->failures, 0 ) );
    winetest_add_failures( InterlockedExchange( &test_data->todo_failures, 0 ) );

    GetFullPathNameW( L"winetest.inf", ARRAY_SIZE(path), path, NULL );
    ret = SetupCopyOEMInfW( path, NULL, 0, 0, dest, ARRAY_SIZE(dest), NULL, &filepart );
    ok( ret, "Failed to copy INF, error %u\n", GetLastError() );
    ret = SetupUninstallOEMInfW( filepart, 0, NULL );
    ok( ret, "Failed to uninstall INF, error %u\n", GetLastError() );

    ret = DeleteFileW( L"winetest.cat" );
    ok( ret, "Failed to delete file, error %u\n", GetLastError() );
    ret = DeleteFileW( L"winetest.inf" );
    ok( ret, "Failed to delete file, error %u\n", GetLastError() );
    ret = DeleteFileW( L"winetest.sys" );
    ok( ret, "Failed to delete file, error %u\n", GetLastError() );
    /* Windows 10 apparently deletes the image in SetupUninstallOEMInf(). */
    ret = DeleteFileW( L"C:/windows/system32/drivers/winetest.sys" );
    ok( ret || GetLastError() == ERROR_FILE_NOT_FOUND, "Failed to delete file, error %u\n", GetLastError() );
}

static inline BOOL pnp_driver_start( const WCHAR *resource )
{
    static const WCHAR hardware_id[] = L"test_hardware_id\0";
    SP_DEVINFO_DATA device = {sizeof(SP_DEVINFO_DATA)};
    WCHAR path[MAX_PATH], filename[MAX_PATH];
    SC_HANDLE manager, service;
    const CERT_CONTEXT *cert;
    int old_mute_threshold;
    BOOL ret, need_reboot;
    HANDLE catalog;
    HDEVINFO set;
    FILE *f;

    old_mute_threshold = winetest_mute_threshold;
    winetest_mute_threshold = 1;

    load_resource( resource, filename );
    ret = MoveFileExW( filename, L"winetest.sys", MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING );
    ok( ret, "failed to move file, error %u\n", GetLastError() );

    f = fopen( "winetest.inf", "w" );
    ok( !!f, "failed to open winetest.inf: %s\n", strerror( errno ) );
    fputs( inf_text, f );
    fclose( f );

    /* Create the catalog file. */

    catalog = CryptCATOpen( (WCHAR *)L"winetest.cat", CRYPTCAT_OPEN_CREATENEW, 0, CRYPTCAT_VERSION_1, 0 );
    ok( catalog != INVALID_HANDLE_VALUE, "Failed to create catalog, error %u\n", GetLastError() );

    add_file_to_catalog( catalog, L"winetest.sys" );
    add_file_to_catalog( catalog, L"winetest.inf" );

    ret = CryptCATPersistStore( catalog );
    todo_wine
    ok( ret, "Failed to write catalog, error %u\n", GetLastError() );

    ret = CryptCATClose( catalog );
    ok( ret, "Failed to close catalog, error %u\n", GetLastError() );

    if (!(cert = testsign_sign( L"winetest.cat" )))
    {
        ret = DeleteFileW( L"winetest.cat" );
        ok( ret, "Failed to delete file, error %u\n", GetLastError() );
        ret = DeleteFileW( L"winetest.inf" );
        ok( ret, "Failed to delete file, error %u\n", GetLastError() );
        ret = DeleteFileW( L"winetest.sys" );
        ok( ret, "Failed to delete file, error %u\n", GetLastError() );
        winetest_mute_threshold = old_mute_threshold;
        return FALSE;
    }

    /* Install the driver. */

    set = SetupDiCreateDeviceInfoList( NULL, NULL );
    ok( set != INVALID_HANDLE_VALUE, "failed to create device list, error %u\n", GetLastError() );

    ret = SetupDiCreateDeviceInfoW( set, L"root\\winetest\\0", &GUID_NULL, NULL, NULL, 0, &device );
    ok( ret, "failed to create device, error %#x\n", GetLastError() );

    ret = SetupDiSetDeviceRegistryPropertyW( set, &device, SPDRP_HARDWAREID, (const BYTE *)hardware_id,
                                             sizeof(hardware_id) );
    ok( ret, "failed to create set hardware ID, error %u\n", GetLastError() );

    ret = SetupDiCallClassInstaller( DIF_REGISTERDEVICE, set, &device );
    ok( ret, "failed to register device, error %u\n", GetLastError() );

    ret = SetupDiDestroyDeviceInfoList( set );
    ok( ret, "failed to destroy set, error %u\n", GetLastError() );

    GetFullPathNameW( L"winetest.inf", ARRAY_SIZE(path), path, NULL );

    ret = UpdateDriverForPlugAndPlayDevicesW( NULL, hardware_id, path, INSTALLFLAG_FORCE, &need_reboot );
    ok( ret, "failed to install device, error %u\n", GetLastError() );
    ok( !need_reboot, "expected no reboot necessary\n" );

    testsign_cleanup( cert );

    /* Check that the service is created and started. */
    manager = OpenSCManagerW( NULL, NULL, SC_MANAGER_CONNECT );
    ok( !!manager, "failed to open service manager, error %u\n", GetLastError() );

    service = OpenServiceW( manager, L"winetest", SERVICE_START );
    ok( !!service, "failed to open service, error %u\n", GetLastError() );

    ret = StartServiceW( service, 0, NULL );
    ok( !ret, "service didn't start automatically\n" );
    if (!ret && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING)
    {
        /* If Secure Boot is enabled or the machine is 64-bit, it will reject an unsigned driver. */
        ok( GetLastError() == ERROR_DRIVER_BLOCKED || GetLastError() == ERROR_INVALID_IMAGE_HASH,
            "unexpected error %u\n", GetLastError() );
        win_skip( "Failed to start service; probably your machine doesn't accept unsigned drivers.\n" );
    }

    CloseServiceHandle( service );
    CloseServiceHandle( manager );

    winetest_mute_threshold = old_mute_threshold;
    return ret || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
}

#else

#if !defined( __WINE_USE_MSVCRT ) || defined( __MINGW32__ )
#define __WINE_PRINTF_ATTR( fmt, args ) __attribute__((format( printf, fmt, args )))
#else
#define __WINE_PRINTF_ATTR( fmt, args )
#endif

static HANDLE okfile;
static LONG successes;
static LONG failures;
static LONG skipped;
static LONG todo_successes;
static LONG todo_failures;
static LONG muted_traces;
static LONG muted_skipped;
static LONG muted_todo_successes;

static int running_under_wine;
static int winetest_debug;
static int winetest_report_success;

/* silence todos and skips above this threshold */
static int winetest_mute_threshold = 42;

/* counts how many times a given line printed a message */
static LONG line_counters[16384];

/* The following data must be kept track of on a per-thread basis */
struct tls_data
{
    HANDLE thread;
    const char *current_file; /* file of current check */
    int current_line;         /* line of current check */
    unsigned int todo_level;  /* current todo nesting level */
    int todo_do_loop;
    char *str_pos;              /* position in debug buffer */
    char strings[2000];         /* buffer for debug strings */
    char context[8][128];       /* data to print before messages */
    unsigned int context_count; /* number of context prefixes */
};

static KSPIN_LOCK tls_data_lock;
static struct tls_data tls_data_pool[128];
static DWORD tls_data_count;

static inline struct tls_data *get_tls_data(void)
{
    static struct tls_data tls_overflow;
    struct tls_data *data;
    HANDLE thread = PsGetCurrentThreadId();
    KIRQL irql;

    KeAcquireSpinLock( &tls_data_lock, &irql );
    for (data = tls_data_pool; data != tls_data_pool + tls_data_count; ++data)
        if (data->thread == thread) break;
    if (data == tls_data_pool + ARRAY_SIZE(tls_data_pool)) data = &tls_overflow;
    else if (data == tls_data_pool + tls_data_count)
    {
        data->thread = thread;
        data->str_pos = data->strings;
        tls_data_count++;
    }
    KeReleaseSpinLock( &tls_data_lock, irql );

    return data;
}

static inline void winetest_set_location( const char *file, int line )
{
    struct tls_data *data = get_tls_data();
    data->current_file = strrchr( file, '/' );
    if (data->current_file == NULL) data->current_file = strrchr( file, '\\' );
    if (data->current_file == NULL) data->current_file = file;
    else data->current_file++;
    data->current_line = line;
}

static inline void kvprintf( const char *format, va_list ap )
{
    struct tls_data *data = get_tls_data();
    IO_STATUS_BLOCK io;
    int len = vsnprintf( data->str_pos, sizeof(data->strings) - (data->str_pos - data->strings), format, ap );
    data->str_pos += len;

    if (len && data->str_pos[-1] == '\n')
    {
        ZwWriteFile( okfile, NULL, NULL, NULL, &io, data->strings,
                     strlen( data->strings ), NULL, NULL );
        data->str_pos = data->strings;
    }
}

static inline void WINAPIV kprintf( const char *format, ... ) __WINE_PRINTF_ATTR( 1, 2 );
static inline void WINAPIV kprintf( const char *format, ... )
{
    va_list valist;

    va_start( valist, format );
    kvprintf( format, valist );
    va_end( valist );
}

static inline void WINAPIV winetest_printf( const char *msg, ... ) __WINE_PRINTF_ATTR( 1, 2 );
static inline void WINAPIV winetest_printf( const char *msg, ... )
{
    struct tls_data *data = get_tls_data();
    va_list valist;

    kprintf( "%s:%d: ", data->current_file, data->current_line );
    va_start( valist, msg );
    kvprintf( msg, valist );
    va_end( valist );
}

static inline NTSTATUS winetest_init(void)
{
    const struct winetest_shared_data *data;
    SIZE_T size = sizeof(*data);
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING string;
    IO_STATUS_BLOCK io;
    void *addr = NULL;
    HANDLE section;
    NTSTATUS ret;

    KeInitializeSpinLock( &tls_data_lock );

    RtlInitUnicodeString( &string, L"\\BaseNamedObjects\\winetest_dinput_section" );
    /* OBJ_KERNEL_HANDLE is necessary for the file to be accessible from system threads */
    InitializeObjectAttributes( &attr, &string, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, 0, NULL );
    if ((ret = ZwOpenSection( &section, SECTION_MAP_READ, &attr ))) return ret;

    if ((ret = ZwMapViewOfSection( section, NtCurrentProcess(), &addr, 0, 0, NULL, &size, ViewUnmap, 0, PAGE_READONLY )))
    {
        ZwClose( section );
        return ret;
    }
    data = addr;
    running_under_wine = data->running_under_wine;
    winetest_debug = data->winetest_debug;
    winetest_report_success = data->winetest_report_success;

    ZwUnmapViewOfSection( NtCurrentProcess(), addr );
    ZwClose( section );

    RtlInitUnicodeString( &string, L"\\??\\C:\\windows\\winetest_dinput_okfile" );
    return ZwOpenFile( &okfile, FILE_APPEND_DATA | SYNCHRONIZE, &attr, &io,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SYNCHRONOUS_IO_NONALERT );
}

#define winetest_cleanup() winetest_cleanup_( __FILE__ )
static inline void winetest_cleanup_( const char *file )
{
    char test_name[MAX_PATH], *tmp;
    struct winetest_shared_data *data;
    SIZE_T size = sizeof(*data);
    const char *source_file;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING string;
    void *addr = NULL;
    HANDLE section;

    source_file = strrchr( file, '/' );
    if (!source_file) source_file = strrchr( file, '\\' );
    if (!source_file) source_file = file;
    else source_file++;

    strcpy( test_name, source_file );
    if ((tmp = strrchr( test_name, '.' ))) *tmp = 0;

    if (winetest_debug)
    {
        kprintf( "%04x:%s: %d tests executed (%d marked as todo, %d %s), %d skipped.\n",
                 (DWORD)(DWORD_PTR)PsGetCurrentProcessId(), test_name,
                 successes + failures + todo_successes + todo_failures, todo_successes, failures + todo_failures,
                 (failures + todo_failures != 1) ? "failures" : "failure", skipped );
    }

    RtlInitUnicodeString( &string, L"\\BaseNamedObjects\\winetest_dinput_section" );
    /* OBJ_KERNEL_HANDLE is necessary for the file to be accessible from system threads */
    InitializeObjectAttributes( &attr, &string, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, 0, NULL );

    if (!ZwOpenSection( &section, SECTION_MAP_READ | SECTION_MAP_WRITE, &attr ))
    {
        if (!ZwMapViewOfSection( section, NtCurrentProcess(), &addr, 0, 0, NULL, &size, ViewUnmap, 0, PAGE_READWRITE ))
        {
            data = addr;

            InterlockedExchangeAdd( &data->failures, failures );
            InterlockedExchangeAdd( &data->todo_failures, todo_failures );

            ZwUnmapViewOfSection( NtCurrentProcess(), addr );
        }
        ZwClose( section );
    }

    ZwClose( okfile );
}

static inline void winetest_print_context( const char *msgtype )
{
    struct tls_data *data = get_tls_data();
    unsigned int i;

    winetest_printf( "%s", msgtype );
    for (i = 0; i < data->context_count; ++i) kprintf( "%s: ", data->context[i] );
}

static inline LONG winetest_add_line(void)
{
    struct tls_data *data;
    int index, count;

    if (winetest_debug > 1) return 0;

    data = get_tls_data();
    index = data->current_line % ARRAY_SIZE(line_counters);
    count = InterlockedIncrement( line_counters + index ) - 1;
    if (count == winetest_mute_threshold)
        winetest_printf( "Line has been silenced after %d occurrences\n", winetest_mute_threshold );

    return count;
}

static inline int winetest_vok( int condition, const char *msg, va_list args )
{
    struct tls_data *data = get_tls_data();

    if (data->todo_level)
    {
        if (condition)
        {
            winetest_print_context( "Test succeeded inside todo block: " );
            kvprintf( msg, args );
            InterlockedIncrement( &todo_failures );
            return 0;
        }
        else
        {
            if (!winetest_debug || winetest_add_line() < winetest_mute_threshold)
            {
                if (winetest_debug > 0)
                {
                    winetest_print_context( "Test marked todo: " );
                    kvprintf( msg, args );
                }
                InterlockedIncrement( &todo_successes );
            }
            else InterlockedIncrement( &muted_todo_successes );
            return 1;
        }
    }
    else
    {
        if (!condition)
        {
            winetest_print_context( "Test failed: " );
            kvprintf( msg, args );
            InterlockedIncrement( &failures );
            return 0;
        }
        else
        {
            if (winetest_report_success) winetest_printf( "Test succeeded\n" );
            InterlockedIncrement( &successes );
            return 1;
        }
    }
}

static inline void WINAPIV winetest_ok( int condition, const char *msg, ... ) __WINE_PRINTF_ATTR( 2, 3 );
static inline void WINAPIV winetest_ok( int condition, const char *msg, ... )
{
    va_list args;
    va_start( args, msg );
    winetest_vok( condition, msg, args );
    va_end( args );
}

static inline void winetest_vskip( const char *msg, va_list args )
{
    if (winetest_add_line() < winetest_mute_threshold)
    {
        winetest_print_context( "Driver tests skipped: " );
        kvprintf( msg, args );
        InterlockedIncrement( &skipped );
    }
    else InterlockedIncrement( &muted_skipped );
}

static inline void WINAPIV winetest_skip( const char *msg, ... ) __WINE_PRINTF_ATTR( 1, 2 );
static inline void WINAPIV winetest_skip( const char *msg, ... )
{
    va_list args;
    va_start( args, msg );
    winetest_vskip( msg, args );
    va_end( args );
}

static inline void WINAPIV winetest_win_skip( const char *msg, ... ) __WINE_PRINTF_ATTR( 1, 2 );
static inline void WINAPIV winetest_win_skip( const char *msg, ... )
{
    va_list args;
    va_start( args, msg );
    if (!running_under_wine) winetest_vskip( msg, args );
    else winetest_vok( 0, msg, args );
    va_end( args );
}

static inline void WINAPIV winetest_trace( const char *msg, ... ) __WINE_PRINTF_ATTR( 1, 2 );
static inline void WINAPIV winetest_trace( const char *msg, ... )
{
    va_list args;

    if (!winetest_debug) return;
    if (winetest_add_line() < winetest_mute_threshold)
    {
        winetest_print_context( "" );
        va_start( args, msg );
        kvprintf( msg, args );
        va_end( args );
    }
    else InterlockedIncrement( &muted_traces );
}

static inline void winetest_start_todo( int is_todo )
{
    struct tls_data *data = get_tls_data();
    data->todo_level = (data->todo_level << 1) | (is_todo != 0);
    data->todo_do_loop = 1;
}

static inline int winetest_loop_todo(void)
{
    struct tls_data *data = get_tls_data();
    int do_loop = data->todo_do_loop;
    data->todo_do_loop = 0;
    return do_loop;
}

static inline void winetest_end_todo(void)
{
    struct tls_data *data = get_tls_data();
    data->todo_level >>= 1;
}

static inline void WINAPIV winetest_push_context( const char *fmt, ... ) __WINE_PRINTF_ATTR( 1, 2 );
static inline void WINAPIV winetest_push_context( const char *fmt, ... )
{
    struct tls_data *data = get_tls_data();
    va_list valist;

    if (data->context_count < ARRAY_SIZE(data->context))
    {
        va_start( valist, fmt );
        vsnprintf( data->context[data->context_count], sizeof(data->context[data->context_count]), fmt, valist );
        va_end( valist );
        data->context[data->context_count][sizeof(data->context[data->context_count]) - 1] = 0;
    }
    ++data->context_count;
}

static inline void winetest_pop_context(void)
{
    struct tls_data *data = get_tls_data();

    if (data->context_count) --data->context_count;
}

static inline int broken( int condition )
{
    return !running_under_wine && condition;
}

#ifdef WINETEST_NO_LINE_NUMBERS
# define subtest_(file, line)  (winetest_set_location(file, 0), 0) ? (void)0 : winetest_subtest
# define ignore_exceptions_(file, line)  (winetest_set_location(file, 0), 0) ? (void)0 : winetest_ignore_exceptions
# define ok_(file, line)       (winetest_set_location(file, 0), 0) ? (void)0 : winetest_ok
# define skip_(file, line)     (winetest_set_location(file, 0), 0) ? (void)0 : winetest_skip
# define win_skip_(file, line) (winetest_set_location(file, 0), 0) ? (void)0 : winetest_win_skip
# define trace_(file, line)    (winetest_set_location(file, 0), 0) ? (void)0 : winetest_trace
# define wait_child_process_(file, line) (winetest_set_location(file, 0), 0) ? (void)0 : winetest_wait_child_process
#else
# define subtest_(file, line)  (winetest_set_location(file, line), 0) ? (void)0 : winetest_subtest
# define ignore_exceptions_(file, line)  (winetest_set_location(file, line), 0) ? (void)0 : winetest_ignore_exceptions
# define ok_(file, line)       (winetest_set_location(file, line), 0) ? (void)0 : winetest_ok
# define skip_(file, line)     (winetest_set_location(file, line), 0) ? (void)0 : winetest_skip
# define win_skip_(file, line) (winetest_set_location(file, line), 0) ? (void)0 : winetest_win_skip
# define trace_(file, line)    (winetest_set_location(file, line), 0) ? (void)0 : winetest_trace
# define wait_child_process_(file, line) (winetest_set_location(file, line), 0) ? (void)0 : winetest_wait_child_process
#endif

#define ok       ok_(__FILE__, __LINE__)
#define skip     skip_(__FILE__, __LINE__)
#define trace    trace_(__FILE__, __LINE__)
#define win_skip win_skip_(__FILE__, __LINE__)

#define todo_if(is_todo) for (winetest_start_todo(is_todo); \
                              winetest_loop_todo(); \
                              winetest_end_todo())
#define todo_wine               todo_if(running_under_wine)
#define todo_wine_if(is_todo)   todo_if((is_todo) && running_under_wine)

#endif /* __WINE_WINE_TEST_H */

#endif /* __WINE_DRIVER_HID_H */
