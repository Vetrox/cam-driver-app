#include <streams.h>            // include dir: baseclasses, lib: strmbase.lib
#include <initguid.h>           // CLSID / GUID

#include "config.h"             // project configuration
#include "filter.h"
#include "dll.h"

const AMOVIESETUP_MEDIATYPE AMSMediaTypesVCam =
{
    &MEDIATYPE_Video,
    &MSUBTYPE
};

const AMOVIESETUP_PIN AMSPinVCam =
{
    NULL,                  // (obsolete) Pin string name 
    FALSE,                 // Is it rendered
    TRUE,                  // Is it an output
    FALSE,                 // Can we have zero instances of this pin
    FALSE,                 // Can we have more than one instance of this type of pin
    &CLSID_NULL,           // (obsolete) Connects to filter 
    NULL,                  // (obsolete) Connects to pin 
    1,                     // Number of media types supported by this pin. TODO: consider changing to 0
    &AMSMediaTypesVCam     // Pin Media types (1 Video)
};

const AMOVIESETUP_FILTER AMSFilterVCam =
{
    &CLSID_VCAM,            // Filter CLSID
    CAMERA_NAME,            // Filter name
    MERIT_DO_NOT_USE,       // Filter merit (no filter graph)
    1,                      // Number pins
    &AMSPinVCam             // Pin details
};

CFactoryTemplate g_Templates[] =
{
    {
        CAMERA_NAME,            // Name of the filter.
        &CLSID_VCAM,            // Pointer to the CLSID of the object.
        CVCam::CreateInstance,  // Pointer to a function that creates an instance of the object.
        NULL,                   // Pointer to a function that gets called from the DLL entry point.
        &AMSFilterVCam
    },

};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

/**
* This function creates registry entries for every component
* in the g_Templates array.
*/
STDAPI RegisterFilters(BOOL bRegister)
{
    //l::log("RegisterFilters.\n");
    HRESULT hr = NOERROR;
    WCHAR achFileName[MAX_PATH];
    char achTemp[MAX_PATH];
    ASSERT(g_hInst != 0);

    if (0 == GetModuleFileNameA(g_hInst, achTemp, sizeof(achTemp)))
        return AmHresultFromWin32(GetLastError());

    MultiByteToWideChar(CP_ACP, 0L, achTemp, lstrlenA(achTemp) + 1,
        achFileName, NUMELMS(achFileName));

    hr = CoInitialize(0);
    if (bRegister)
    {
        hr = AMovieSetupRegisterServer(CLSID_VCAM, CAMERA_NAME, achFileName, L"Both", L"InprocServer32");
    }

    if (SUCCEEDED(hr))
    {
        IFilterMapper2* fm = 0;
        hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper2, (void**)&fm);
        if (SUCCEEDED(hr))
        {
            if (bRegister)
            {
                IMoniker* pMoniker = 0;
                REGFILTER2 rf2;
                rf2.dwVersion = 1;
                rf2.dwMerit = MERIT_DO_NOT_USE;
                rf2.cPins = 1;
                rf2.rgPins = &AMSPinVCam;
                hr = fm->RegisterFilter(
                    CLSID_VCAM,                         // Filter CLSID.
                    CAMERA_NAME,                        // Filter name.
                    &pMoniker,                          // Device monkier
                    &CLSID_VideoInputDeviceCategory,    // Filter category.
                    NULL,                               // Instance data.
                    &rf2                                // Pointer to filter information.
                );
            }
            else
            {
                hr = fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, CLSID_VCAM);
            }
        }

        // release interface
        if (fm) fm->Release();
    }

    if (SUCCEEDED(hr) && !bRegister)
        hr = AMovieSetupUnregisterServer(CLSID_VCAM);

    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

// AUTOCALL
STDAPI DllRegisterServer() {
    return RegisterFilters(TRUE);
}

// AUTOCALL
STDAPI DllUnregisterServer() {
    return RegisterFilters(FALSE);
}

// AUTOCALL
BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved) {
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}