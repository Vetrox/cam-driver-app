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

// https://github.com/CatxFish/obs-virtual-cam/blob/master/src/virtual-source/virtual-cam.cpp
//////////////////////////////////////////////////////////////////////////
//  CVCam is the source filter which masquerades as a capture device
//////////////////////////////////////////////////////////////////////////
CUnknown* WINAPI CVCam::CreateInstance(LPUNKNOWN lpunk, HRESULT* phr) {
    ASSERT(phr);
    CUnknown* punk = new CVCam(lpunk, phr);
    return punk;
}

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT* phr) : CSource(CAMERA_NAME, lpunk, CLSID_VCAM) {
    ASSERT(phr);
    CAutoLock cAutoLock(&m_cStateLock);
    // Create the one and only output pin
    m_paStreams = (CSourceStream**) new CVCamStream * [1];
    m_paStreams[0] = new CVCamStream(phr, this, L"Video");
}

HRESULT CVCam::QueryInterface(REFIID riid, void** ppv) {
    //Forward request for IAMStreamConfig & IKsPropertySet to the pin
    if (riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet))
        return m_paStreams[0]->QueryInterface(riid, ppv);
    else
        return CSource::QueryInterface(riid, ppv);
}

//////////////////////////////////////////////////////////////////////////
// CVCamStream is the one and only output pin of CVCam which handles 
// all the stuff.
//////////////////////////////////////////////////////////////////////////
CVCamStream::CVCamStream(HRESULT* phr, CVCam* pParent, LPCWSTR pPinName) : CSourceStream(CAMERA_NAME, phr, pParent, pPinName), m_pParent(pParent) {

    // Set the default media type as 320x240x24@15
    //TODO: Change to other resolution
    GetMediaType(&m_mt);
}


CVCamStream::~CVCamStream() {
}

HRESULT CVCamStream::QueryInterface(REFIID riid, void** ppv) {
    // Standard OLE stuff
    if (riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if (riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CVCamStream::AddRef() {
    return GetOwner()->AddRef(); 
}

STDMETHODIMP_(ULONG) CVCamStream::Release() {
    return GetOwner()->Release(); 
}

HRESULT CVCamStream::OnThreadStartPlay()
{
    cda::logln("CVCamStream::OnThreadStartPlay");
    // STRAM STARTS HERE. CORRESPONDS TO USER BUTTON PRESS
    cda::setup();
    return S_OK;
}

HRESULT CVCamStream::OnThreadDestroy() // GETS NEVER EXECUTED SOMEHOW...
{
    cda::logln("CVCamStream::OnThreadDestroy");
    if (cda::cleanup() != S_OK) {
        abort();
    }
    return S_OK;
}

HRESULT CVCamStream::FillBuffer(IMediaSample* pms) {
    BYTE* pData;
    const long lDataLen = pms->GetSize();
    pms->GetPointer(&pData);
    for (size_t i = 0; i < lDataLen; i++) {
        pData[i] = (BYTE)(rand());
    }
    return NOERROR;
}

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CVCamStream::GetMediaType(CMediaType* pmt) {
    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*) pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression    = COMPRESSSION;
    pvi->bmiHeader.biBitCount       = NUMBITS;
    pvi->bmiHeader.biSize           = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth          = CAM_WIDTH;
    pvi->bmiHeader.biHeight         = CAM_HEIGHT;
    pvi->bmiHeader.biPlanes         = 1;
    pvi->bmiHeader.biSizeImage      = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant   = 0;

    pvi->AvgTimePerFrame = FPS_NANO;

    SetRectEmpty(&(pvi->rcSource));
    SetRectEmpty(&(pvi->rcTarget));

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);
    pmt->SetSubtype(&MSUBTYPE);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);

    return S_OK;
}

HRESULT CVCamStream::SetMediaType(const CMediaType* pmt) {
    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*) (pmt->Format());
    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CVCamStream::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties) {
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
    pProperties->cBuffers = 1;
    if (pvi != nullptr) {
        pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;
    }
    else {
        cda::logln("null");
    }
    

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties, &Actual);

    if (FAILED(hr)) return hr;
    if (Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return NOERROR;
} // DecideBufferSize

//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE* pmt) {
    if (pmt == nullptr) {
        return E_FAIL;
    }

    if (m_pParent->GetState() != State_Stopped)
        return E_FAIL;

    /*if (CheckMediaType((CMediaType*)pmt) != S_OK) {
        return E_FAIL;
    }*/

    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)(pmt->pbFormat);
    m_mt = *pmt;
    //m_mt.SetFormat(m_mt.Format(), sizeof(VIDEOINFOHEADER));

    IPin* pin;
    ConnectedTo(&pin);
    if (pin)
    {
        IFilterGraph* pGraph = m_pParent->GetGraph();
        pGraph->Reconnect(this);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetFormat(AM_MEDIA_TYPE** ppmt) {
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetNumberOfCapabilities(int* piCount, int* piSize) {
    *piCount = 1;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE** pmt, BYTE* pSCC) {
    if (iIndex != 0) {
        return E_INVALIDARG;
    }
    GetFormat(pmt);
    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)(*pmt)->pbFormat;
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth = CAM_WIDTH;
    pvi->bmiHeader.biHeight = CAM_HEIGHT;
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biBitCount = NUMBITS;
    pvi->bmiHeader.biCompression = COMPRESSSION;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;
    pvi->AvgTimePerFrame = FPS_NANO;

    SetRectEmpty(&(pvi->rcSource));
    SetRectEmpty(&(pvi->rcTarget));

    (*pmt)->majortype = MEDIATYPE_Video;
    (*pmt)->subtype = MSUBTYPE;
    (*pmt)->formattype = FORMAT_VideoInfo;
    (*pmt)->bFixedSizeSamples = FALSE;
    (*pmt)->bTemporalCompression = FALSE;
    (*pmt)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*pmt)->cbFormat = sizeof(VIDEOINFOHEADER);

    VIDEO_STREAM_CONFIG_CAPS* pvscc = (VIDEO_STREAM_CONFIG_CAPS*)pSCC;
    ZeroMemory(pvscc, sizeof(VIDEO_STREAM_CONFIG_CAPS));

    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;    // Digital sensor.
    pvscc->MinFrameInterval = FPS_NANO;         // fps
    pvscc->MaxFrameInterval = FPS_NANO;         // fps

    return S_OK;
}

// Get: Return the pin category (our only property). 
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
    if (pPropData == NULL)          return S_OK; // Caller just wants to know the size. 
    if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.

    *(GUID*)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CVCamStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD* pTypeSupport) {
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET;
    return S_OK;
}