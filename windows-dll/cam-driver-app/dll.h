#pragma once

EXTERN_C BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);
STDAPI AMovieSetupRegisterServer(CLSID   clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType = L"InprocServer32");
STDAPI AMovieSetupUnregisterServer(CLSID clsServer);

// Unique identifier of this virtual camera: {8E14549A-DB61-4309-AFA1-3578E927E933}
DEFINE_GUID(CLSID_VCAM,
	0x8e14549a, 0xdb61, 0x4309, 0xaf, 0xa1, 0x35, 0x78, 0xe9, 0x27, 0xe9, 0x33);
