#include <thread>
#include <streams.h>
#include <stdio.h>
#include <olectl.h>
#include <dvdmedia.h>
#include <chrono>
#include <ratio>
#include <ctime>
#include <string>

#include "config.h"
#include "filter.h"
#include "networkmgr.h"
#include "logger.h"

EXTERN_C const GUID CLSID_VCAM;

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT* phr) : CSource(CAMERA_NAME, lpunk, CLSID_VCAM) {
    CAutoLock cAutoLock(&m_cStateLock);
    m_paStreams = (CSourceStream**) new CVCamStream * [1];
    m_paStreams[0] = new CVCamStream(phr, this, L"Video");
}

HRESULT CVCam::QueryInterface(REFIID riid, void** ppv) {
    if (riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet))
        return m_paStreams[0]->QueryInterface(riid, ppv);
    else
        return CSource::QueryInterface(riid, ppv);
}

HRESULT CVCamStream::QueryInterface(REFIID riid, void** ppv) {
    if (riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if (riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);
    AddRef();
    return S_OK;
}

HRESULT CVCamStream::OnThreadStartPlay() {
    return cda::setup();
}

HRESULT CVCamStream::OnThreadDestroy() {
    return cda::cleanup();
}

HRESULT CVCamStream::FillBuffer(IMediaSample* pms) {
    BYTE* pData;
    const long lDataLen = pms->GetSize();
    pms->GetPointer(&pData);
    cda::send_beacon();
    cda::recv_img(pData, lDataLen);
    return S_OK;
}

// preferred media type. gets stored in m_mt in constructor
HRESULT CVCamStream::GetMediaType(CMediaType* pmt) {
    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*) pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biSize           = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth          = CAM_WIDTH;
    pvi->bmiHeader.biHeight         = CAM_HEIGHT;
    pvi->bmiHeader.biPlanes         = 1;
    pvi->bmiHeader.biBitCount       = NUMBITS;
    pvi->bmiHeader.biCompression    = COMPRESSSION;
    pvi->bmiHeader.biSizeImage      = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biXPelsPerMeter  = CAM_WIDTH * 2; // 0.5 meter screen.
    pvi->bmiHeader.biYPelsPerMeter  = CAM_HEIGHT * 2;
    pvi->bmiHeader.biClrUsed        = 0;
    pvi->bmiHeader.biClrImportant   = 0;
    pvi->dwBitRate          = pvi->bmiHeader.biSizeImage * (1000'0000 / FPS_NANO);
    pvi->AvgTimePerFrame    = FPS_NANO;

    SetRectEmpty(&(pvi->rcSource));
    SetRectEmpty(&(pvi->rcTarget));

    pmt->SetType(&MEDIATYPE_Video);
    const auto SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
    pmt->SetSubtype(&SubTypeGUID);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    return S_OK;
}

inline HRESULT CVCamStream::SetMediaType(const CMediaType* pmt) {
    cda::logln("SetMediaType. The format has been negotiated:");
    if (pmt == nullptr) {
        cda::logln("nullptr (may be used to reset our pin)");
    }
    else {
        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)(pmt->pbFormat);
        cda::log("Width: ");
        cda::logln(std::to_string(vih->bmiHeader.biWidth));
        cda::log("Height: ");
        cda::logln(std::to_string(vih->bmiHeader.biHeight));
        cda::log("Bitcount: ");
        cda::logln(std::to_string(vih->bmiHeader.biBitCount));
    }
    return CSourceStream::SetMediaType(pmt);
}

HRESULT CVCamStream::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties) {
    CAutoLock cAutoLock(m_pFilter->pStateLock());

    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    HRESULT hr = pAlloc->SetProperties(pProperties, &Actual);
    if (FAILED(hr)) return hr;
    if (Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return S_OK;
}

// just say we set it, even though we don't care
HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE* pmt) {
    cda::logln("::SetFormat. Trying to set the format to:");
    if (pmt == nullptr) {
        cda::logln("nullptr (may be used to reset our pin)");
    }
    else {
        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*) (pmt->pbFormat);
        cda::log("Width: ");
        cda::logln(std::to_string(vih->bmiHeader.biWidth));
        cda::log("Height: ");
        cda::logln(std::to_string(vih->bmiHeader.biHeight));
        cda::log("Bitcount: ");
        cda::logln(std::to_string(vih->bmiHeader.biBitCount));
    }
    
    return S_OK;
}

// current or prefferred format (we only have one)
HRESULT STDMETHODCALLTYPE CVCamStream::GetFormat(AM_MEDIA_TYPE** ppmt) {
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetNumberOfCapabilities(int* piCount, int* piSize) {
    *piCount = 1;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE** pmt, BYTE* deprecated) {
    if (iIndex < 0) return E_INVALIDARG;
    if (iIndex > 0) return S_FALSE;
    GetFormat(pmt);
    return S_OK;
}


//// IKsPropertySet ///////////////////////////

inline STDMETHODIMP_(HRESULT __stdcall) CVCamStream::Set(REFGUID, DWORD, void*, DWORD, void*, DWORD) {
    cda::logln("IKsPropertySet::Set was called.");
    return E_NOTIMPL;
}

HRESULT CVCamStream::Get(
    REFGUID guidPropSet,   // Which property set.
    DWORD dwPropID,        // Which property in that set.
    void* pInstanceData,   // Instance data (ignore).
    DWORD cbInstanceData,  // Size of the instance data (ignore).
    void* pPropData,       // Buffer to receive the property data.
    DWORD cbPropData,      // Size of the buffer.
    DWORD* pcbReturned     // Return the size of the property.
) {
    if (guidPropSet != AMPROPSETID_Pin)             return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)        return E_PROP_ID_UNSUPPORTED;
    if (pPropData == NULL && pcbReturned == NULL)   return E_POINTER;

    if (pcbReturned) *pcbReturned = sizeof(GUID);
    if (pPropData == NULL)          return S_OK;        // Caller just wants to know the size. 
    if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.

    *(GUID*)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

HRESULT CVCamStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD* pTypeSupport) {
    if (guidPropSet != AMPROPSETID_Pin)         return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)    return E_PROP_ID_UNSUPPORTED;
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET;
    return S_OK;
}