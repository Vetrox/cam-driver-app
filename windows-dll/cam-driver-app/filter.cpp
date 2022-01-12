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

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);
EXTERN_C const GUID CLSID_VCAM;

static auto last_time = std::chrono::steady_clock::now();
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
    const auto cur_time = std::chrono::steady_clock::now();
    const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - last_time).count();
    last_time = cur_time;

    BYTE* pData;
    const long lDataLen = pms->GetSize();
    pms->GetPointer(&pData);

    //cda::send_beacon();

    // TODO: TEMPORARY AREA
   /* BYTE* temp = new BYTE[1280 * 720 * 3];
    HRESULT h = cda::recv_img(temp, 1280 * 720 * 3);
    if (h != S_OK) {
        ZeroMemory(pData, lDataLen);
    }
    for (size_t i = 0; i < 1280 * 720; i++) {
        pData[i * 4]        = 0;
        pData[i * 4 + 1]    = temp[i * 3];
        pData[i * 4 + 2]    = temp[i * 3 + 1];
        pData[i * 4 + 3]    = temp[i * 3 + 2];
    }
    delete[] temp;*/
    //
    /*if (h != S_OK) {
        ZeroMemory(pData, lDataLen);
    }*/


    for (size_t i = 0; i < lDataLen; i++) {
        pData[i] = (BYTE)(rand());
    }
    
    return NOERROR;
}


// Ignore quality management messages sent from the downstream filter
STDMETHODIMP CVCamStream::Notify(IBaseFilter* pSender, Quality q) {
    return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////
// This is called when the output format has been negotiated
//////////////////////////////////////////////////////////////////////////
HRESULT CVCamStream::SetMediaType(const CMediaType* pmt) {
    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->Format());
    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CVCamStream::GetMediaType(CMediaType* pmt) {
    // TODO: redo
   /* cda::log("GetMediaType called with iPosition ");
    cda::logln(std::to_string(iPosition));
    if (iPosition < 0) return E_INVALIDARG;
    if (iPosition != 0) return VFW_S_NO_MORE_ITEMS;*/

    /*if (iPosition == 0)
    {
        *pmt = m_mt;
        return S_OK;
    }*/

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression = COMPRESSSION; // original: BI_RGB --- BI_BITFIELDS
    pvi->bmiHeader.biBitCount = NUMBITS;
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth = CAM_WIDTH;         // seems to be controlling the real resolution.
    pvi->bmiHeader.biHeight = CAM_HEIGHT;       // seems to be controlling the real resolution.
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = 333333;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    // Work out the GUID for the subtype from the header info.
    //const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
    pmt->SetSubtype(&MSUBTYPE);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);

    


    return NOERROR;

} // GetMediaType

// This method is called to see if a given output format is supported
//HRESULT CVCamStream::CheckMediaType(const CMediaType* pMediaType) {
//    return S_OK;
//    if (*pMediaType == nullptr) {
//        return E_FAIL;
//    }
//// https://github.com/CatxFish/obs-virtual-cam/blob/master/src/virtual-source/virtual-cam.cpp
//    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)(pMediaType->Format());
//
//    const GUID* type = pMediaType->Type();
//    const GUID* info = pMediaType->FormatType();
//    const GUID* subtype = pMediaType->Subtype();
//
//    if (*type != MEDIATYPE_Video)
//        return E_INVALIDARG;
//
//    if (*info != FORMAT_VideoInfo)
//        return E_INVALIDARG;
//
//    if (*subtype != MSUBTYPE)
//        return E_INVALIDARG;
//
//    if (pvi->bmiHeader.biWidth != CAM_WIDTH || pvi->bmiHeader.biHeight != CAM_HEIGHT)
//        return E_INVALIDARG;
//
//    return S_OK;
//} // CheckMediaType

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

// Called when graph is run
HRESULT CVCamStream::OnThreadCreate() {
    return S_OK;
} // OnThreadCreate


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
    if (iIndex < 0 || iIndex > 1) {
        return E_INVALIDARG;
    }
    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(VIDEOINFOHEADER, pvi, (*pmt)->pbFormat);

    cda::log("GetStreamCaps with iIndex ");
    cda::logln(std::to_string(iIndex));

    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth = CAM_WIDTH;
    pvi->bmiHeader.biHeight = CAM_HEIGHT;
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biBitCount = NUMBITS;
    pvi->bmiHeader.biCompression = COMPRESSSION;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;
    pvi->AvgTimePerFrame = 333333;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    (*pmt)->majortype = MEDIATYPE_Video;
    (*pmt)->subtype = MSUBTYPE; //TODO: change this to MEDIASUBTYPE_RGB32 and make it XRGB
// supported formats https://github.com/obsproject/obs-studio/blob/master/plugins/win-dshow/win-dshow.cpp#L414
    (*pmt)->formattype = FORMAT_VideoInfo;
    (*pmt)->bFixedSizeSamples = FALSE;
    (*pmt)->bTemporalCompression = FALSE;
    (*pmt)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*pmt)->cbFormat = sizeof(VIDEOINFOHEADER);

    DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);
    ZeroMemory(pvscc, sizeof(VIDEO_STREAM_CONFIG_CAPS));

    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;    // Digital sensor.
    pvscc->MinFrameInterval = 1000 * 10000 / FPS_MIN;         // fps
    pvscc->MaxFrameInterval = 1000 * 10000 / FPS_MAX;         // fps

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////


HRESULT CVCamStream::Set(REFGUID guidPropSet, DWORD dwID, void* pInstanceData,
    DWORD cbInstanceData, void* pPropData, DWORD cbPropData) {// Set: Cannot set any properties.
    return E_NOTIMPL;
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