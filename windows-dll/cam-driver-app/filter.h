#pragma once

class CVCam : public CSource {
public:
    //// IUnknown /////////////////////////////////
    static STDMETHODIMP_(CUnknown*) CreateInstance(LPUNKNOWN lpunk, HRESULT* phr) { return new CVCam(lpunk, phr); }
    STDMETHODIMP                    QueryInterface(REFIID riid, void** ppv);

    IFilterGraph* GetGraph() { return m_pGraph; }
    FILTER_STATE GetState() { return m_State; }
private:
    CVCam(LPUNKNOWN lpunk, HRESULT* phr);
};

class CVCamStream : public CSourceStream, public IAMStreamConfig, public IKsPropertySet {
public:
    //// IUnknown /////////////////////////////////
    STDMETHODIMP            QueryInterface(REFIID riid, void** ppv);
    STDMETHODIMP_(ULONG)    AddRef() { return GetOwner()->AddRef(); }
    STDMETHODIMP_(ULONG)    Release() { return GetOwner()->Release(); }
    //// IQualityControl //////////////////////////
    //STDMETHODIMP Notify(IBaseFilter* pSender, Quality q) { return E_NOTIMPL; }
    //// IAMStreamConfig //////////////////////////
    STDMETHODIMP SetFormat(AM_MEDIA_TYPE* pmt);
    STDMETHODIMP GetFormat(AM_MEDIA_TYPE** ppmt);
    STDMETHODIMP GetNumberOfCapabilities(int* piCount, int* piSize);
    STDMETHODIMP GetStreamCaps(int iIndex, AM_MEDIA_TYPE** pmt, BYTE* pSCC);
    //// IKsPropertySet ///////////////////////////
    STDMETHODIMP Set(REFGUID, DWORD, void*, DWORD, void*, DWORD) { return E_NOTIMPL; }
    STDMETHODIMP Get(REFGUID, DWORD, void*, DWORD, void*, DWORD, DWORD*);
    STDMETHODIMP QuerySupported(REFGUID, DWORD, DWORD*);
    //// CSourceStream ////////////////////////////
    CVCamStream(HRESULT* phr, CVCam* pParent, LPCWSTR pPinName) : 
        CSourceStream(CAMERA_NAME, phr, pParent, pPinName), m_pParent(pParent) { 
        GetMediaType(&m_mt); 
    }
    HRESULT OnThreadStartPlay();
    HRESULT OnThreadDestroy();
    HRESULT OnThreadCreate() { return S_OK; }
    HRESULT FillBuffer(IMediaSample* pms);
    HRESULT DecideBufferSize(IMemAllocator* pIMemAlloc, ALLOCATOR_PROPERTIES* pProperties);
    HRESULT GetMediaType(CMediaType* pmt);
    HRESULT SetMediaType(const CMediaType* pmt) { return CSourceStream::SetMediaType(pmt); }
private:
    CVCam*  m_pParent;
};