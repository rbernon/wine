#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "imm.h"
#include "ddk/imm.h"
#include "objbase.h"
#include "rpcproxy.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ime);

# if 0
# @ stdcall ImeGetImeMenuItems(long long long ptr ptr long)
#endif

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    FIXME("clsid %s, riid %s, out %p stub!\n", debugstr_guid(clsid), debugstr_guid(riid), out);
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources();
}

HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources();
}

BOOL WINAPI ImeConfigure(HKL hKL,HWND hWnd, DWORD dwMode, LPVOID lpData)
{
    FIXME("stub!\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

DWORD WINAPI ImeConversionList(HIMC hIMC, LPCWSTR lpSource,
                LPCANDIDATELIST lpCandList, DWORD dwBufLen, UINT uFlag)

{
    FIXME("stub!\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

BOOL WINAPI ImeDestroy(UINT uForce)
{
    FIXME("stub!\n");
    return TRUE;
}

UINT WINAPI ImeEnumRegisterWord(REGISTERWORDENUMPROCW lpfnEnumProc,
                                LPCWSTR lpszReading, DWORD dwStyle,
                                LPCWSTR lpszRegister, LPVOID lpData)
{
    FIXME("stub!\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

LRESULT WINAPI ImeEscape(HIMC hIMC, UINT uSubFunc, LPVOID lpData)
{
    FIXME("stub!\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

UINT WINAPI ImeGetRegisterWordStyle(UINT nItem, LPSTYLEBUFW lpStyleBuf)
{
    FIXME("stub!\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

BOOL WINAPI ImeInquire(LPIMEINFO lpIMEInfo, LPWSTR lpszUIClass,
                       LPCWSTR lpszOption)
{
    FIXME("stub!\n");
    return TRUE;
}

BOOL WINAPI ImeProcessKey(HIMC hIMC, UINT vKey, LPARAM lKeyData, const LPBYTE lpbKeyState)
{
    FIXME("stub!\n");
    return FALSE;
}

BOOL WINAPI ImeRegisterWord(LPCWSTR lpszReading, DWORD dwStyle,
                            LPCWSTR lpszRegister)
{
    FIXME("stub!\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL WINAPI ImeSelect(HIMC hIMC, BOOL fSelect)
{
    FIXME("stub!\n");
    return FALSE;
}

BOOL WINAPI ImeSetActiveContext(HIMC hIMC,BOOL fFlag)
{
    FIXME("stub!\n");
    return TRUE;
}

BOOL WINAPI ImeSetCompositionString(HIMC hIMC, DWORD dwIndex, LPCVOID lpComp,
                                    DWORD dwCompLen, LPCVOID lpRead,
                                    DWORD dwReadLen)
{
    FIXME("stub!\n");
    return FALSE;
}

UINT WINAPI ImeToAsciiEx (UINT uVKey, UINT uScanCode, const LPBYTE lpbKeyState,
                          LPDWORD lpdwTransKey, UINT fuState, HIMC hIMC)
{
    FIXME("stub!\n");
    return 0;
}

BOOL WINAPI ImeUnregisterWord(LPCWSTR lpszReading, DWORD dwStyle,
                              LPCWSTR lpszUnregister)
{
    FIXME("stub!\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL WINAPI NotifyIME(HIMC hIMC, DWORD dwAction, DWORD dwIndex, DWORD dwValue)
{
    FIXME("stub!\n");
    return FALSE;
}
