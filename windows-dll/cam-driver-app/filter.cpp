// #pragma warning(disable:4244)
// #pragma warning(disable:4711)

#include <thread>
#include <streams.h>
#include <stdio.h>
#include <olectl.h>
#include <dvdmedia.h>

#include "config.h"
#include "filter.h"
#include "networkmgr.h"
#include "logger.h"

EXTERN_C const GUID CLSID_VCAM;

//////////////////////////////////////////////////////////////////////////
//  CVCam is the source filter which masquerades as a capture device
//////////////////////////////////////////////////////////////////////////
CUnknown* WINAPI CVCam::CreateInstance(LPUNKNOWN lpunk, HRESULT* phr)
{
    CDA_LOG("CreateInstance.\n");
    ASSERT(phr);
    CUnknown* punk = new CVCam(lpunk, phr);
    return punk;
}

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT* phr) :
    CSource(CAMERA_NAME, lpunk, CLSID_VCAM)
{
    CDA_LOG("CVCam Constructor.\n");
    ASSERT(phr);
    CAutoLock cAutoLock(&m_cStateLock);
    // Create the one and only output pin
    m_paStreams = (CSourceStream**) new CVCamStream * [1];
    m_paStreams[0] = new CVCamStream(phr, this, CAMERA_NAME);
}

HRESULT CVCam::QueryInterface(REFIID riid, void** ppv)
{
    CDA_LOG("QueryInterface.\n");
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
CVCamStream::CVCamStream(HRESULT* phr, CVCam* pParent, LPCWSTR pPinName) :
    CSourceStream(CAMERA_NAME, phr, pParent, pPinName), m_pParent(pParent)
{
    CDA_LOG("CVCamStream Constructor.\n");
    // Set the default media type as 320x240x24@15
    //TODO: Change to other resolution
    GetMediaType(4, &m_mt);
}

CVCamStream::~CVCamStream()
{
    // STRAM ENDS HERE. CORRESPONDS TO USER BUTTON PRESS
    CDA_LOG("CVCamStream Destuctor.\n");
}

HRESULT CVCamStream::QueryInterface(REFIID riid, void** ppv)
{
    CDA_LOG("CVCamStream::QueryInterface.\n");
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

// EXTERN
bool running = false;
RGBImg* buffered_image = NULL;
// INTERN
std::thread receiver, beacon;
RGBImg* last_image = NULL;

HRESULT CVCamStream::FillBuffer(IMediaSample* pms)
{

    CDA_LOG("CVCamStream::FillBuffer.\n");
    if (!running) {
        running = true;
        receiver = std::thread(setup); // START Receiver
        beacon = std::thread(setup_beacon);
    }
    REFERENCE_TIME rtNow;
    REFERENCE_TIME avgFrameTime = ((VIDEOINFOHEADER*)m_mt.pbFormat)->AvgTimePerFrame;

    rtNow = m_rtLastTime;
    m_rtLastTime += avgFrameTime;
    pms->SetTime(&rtNow, &m_rtLastTime);
    pms->SetSyncPoint(TRUE);

    BYTE* pData;
    const long lDataLen = pms->GetSize();
    pms->GetPointer(&pData);

    if (buffered_image != NULL && (buffered_image->width * buffered_image->height * 3 == lDataLen)) {
        /// DISPLAY DATA ///
        memcpy(pData, buffered_image->buf, lDataLen);
        if (last_image != NULL) {
            free(last_image->buf);
            free(last_image);
        }
        last_image = buffered_image;
        buffered_image = NULL;
    }
    else if (last_image != NULL) {
        memcpy(pData, last_image->buf, lDataLen);
    }
    else {
        ZeroMemory(pData, lDataLen);
    }
    return NOERROR;
}


// Ignore quality management messages sent from the downstream filter
STDMETHODIMP CVCamStream::Notify(IBaseFilter* pSender, Quality q)
{
    CDA_LOG("CVCamStream::Notify\n");
    return E_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////
// This is called when the output format has been negotiated
//////////////////////////////////////////////////////////////////////////
HRESULT CVCamStream::SetMediaType(const CMediaType* pmt)
{
    CDA_LOG("CVCamStream::SetMediaType\n");
    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CVCamStream::GetMediaType(int iPosition, CMediaType* pmt)
{
    CDA_LOG("CVCamStream::GetMediaType\n");
    if (iPosition < 0) return E_INVALIDARG;
    if (iPosition > 8) return VFW_S_NO_MORE_ITEMS;

    if (iPosition == 0)
    {
        *pmt = m_mt;
        return S_OK;
    }

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression = BI_BITFIELDS; // original: BI_RGB --- BI_BITFIELDS
    pvi->bmiHeader.biBitCount = 24;
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth = RATIO_WIDTH * iPosition;
    pvi->bmiHeader.biHeight = RATIO_HEIGHT * iPosition;
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = 1000000;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    // Work out the GUID for the subtype from the header info.
    const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
    pmt->SetSubtype(&SubTypeGUID);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);

    return NOERROR;

} // GetMediaType

// This method is called to see if a given output format is supported
HRESULT CVCamStream::CheckMediaType(const CMediaType* pMediaType)
{
    CDA_LOG("CVCamStream::CheckMediaType\n");
    if (*pMediaType != m_mt)
        return E_INVALIDARG;
    return S_OK;
} // CheckMediaType

// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CVCamStream::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties)
{
    CDA_LOG("CVCamStream::DecideBufferSize\n");
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties, &Actual);

    if (FAILED(hr)) return hr;
    if (Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return NOERROR;
} // DecideBufferSize

// Called when graph is run
HRESULT CVCamStream::OnThreadCreate()
{ // STREAM STARTS HERE
    CDA_LOG("CVCamStream::OnThreadCreate\n");
    m_rtLastTime = 0;
    return NOERROR;
} // OnThreadCreate


//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE* pmt)
{
    CDA_LOG("CVCamStream::SetFormat\n");
    m_mt = *pmt;
    IPin* pin;
    ConnectedTo(&pin);
    if (pin)
    {
        IFilterGraph* pGraph = m_pParent->GetGraph();
        pGraph->Reconnect(this);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetFormat(AM_MEDIA_TYPE** ppmt)
{
    CDA_LOG("CVCamStream::GetFormat\n");
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetNumberOfCapabilities(int* piCount, int* piSize)
{
    CDA_LOG("CVCamStream::GetNumberOfCapabilities\n");
    *piCount = 8;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE** pmt, BYTE* pSCC)
{
    CDA_LOG("CVCamStream::GetStreamCaps\n");
    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(VIDEOINFOHEADER, pvi, (*pmt)->pbFormat);

    if (iIndex == 0) iIndex = 4;

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount = 24;
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth = RATIO_WIDTH * iIndex;
    pvi->bmiHeader.biHeight = RATIO_HEIGHT * iIndex;
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    (*pmt)->majortype = MEDIATYPE_Video;
    (*pmt)->subtype = MEDIASUBTYPE_RGB24;
    (*pmt)->formattype = FORMAT_VideoInfo;
    (*pmt)->bTemporalCompression = FALSE;
    (*pmt)->bFixedSizeSamples = FALSE;
    (*pmt)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*pmt)->cbFormat = sizeof(VIDEOINFOHEADER);

    DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);

    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;
    pvscc->InputSize.cx = 640;
    pvscc->InputSize.cy = 480;
    pvscc->MinCroppingSize.cx = 80;
    pvscc->MinCroppingSize.cy = 60;
    pvscc->MaxCroppingSize.cx = 640;
    pvscc->MaxCroppingSize.cy = 480;
    pvscc->CropGranularityX = 80;
    pvscc->CropGranularityY = 60;
    pvscc->CropAlignX = 0;
    pvscc->CropAlignY = 0;

    pvscc->MinOutputSize.cx = 80;
    pvscc->MinOutputSize.cy = 60;
    pvscc->MaxOutputSize.cx = 640;
    pvscc->MaxOutputSize.cy = 480;
    pvscc->OutputGranularityX = 0;
    pvscc->OutputGranularityY = 0;
    pvscc->StretchTapsX = 0;
    pvscc->StretchTapsY = 0;
    pvscc->ShrinkTapsX = 0;
    pvscc->ShrinkTapsY = 0;
    pvscc->MinFrameInterval = 200000;   // 50 fps
    pvscc->MaxFrameInterval = 50000000; // 0.2 fps
    pvscc->MinBitsPerSecond = (80 * 60 * 3 * 8) / 5;
    pvscc->MaxBitsPerSecond = 640 * 480 * 3 * 8 * 50;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////


HRESULT CVCamStream::Set(REFGUID guidPropSet, DWORD dwID, void* pInstanceData,
    DWORD cbInstanceData, void* pPropData, DWORD cbPropData)
{// Set: Cannot set any properties.
    CDA_LOG("CVCamStream::Set\n");
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
)
{
    CDA_LOG("CVCamStream::Get\n");
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
HRESULT CVCamStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD* pTypeSupport)
{
    CDA_LOG("CVCamStream::QuerySupported\n");
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET;
    return S_OK;
}
