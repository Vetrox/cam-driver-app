#pragma once

class CVCam : public CSource {
public:
    //// IUnknown /////////////////////////////////
    static STDMETHODIMP_(CUnknown*) CreateInstance(LPUNKNOWN lpunk, HRESULT* phr);
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
    STDMETHODIMP_(ULONG)    AddRef();
    STDMETHODIMP_(ULONG)    Release();
    //// IQualityControl //////////////////////////
    STDMETHODIMP Notify(IBaseFilter* pSender, Quality q);
    //// IAMStreamConfig //////////////////////////
    STDMETHODIMP SetFormat(AM_MEDIA_TYPE* pmt);
    STDMETHODIMP GetFormat(AM_MEDIA_TYPE** ppmt);
    STDMETHODIMP GetNumberOfCapabilities(int* piCount, int* piSize);
    STDMETHODIMP GetStreamCaps(int iIndex, AM_MEDIA_TYPE** pmt, BYTE* pSCC);
    //// IKsPropertySet ///////////////////////////
    STDMETHODIMP Set(REFGUID, DWORD, void*, DWORD, void*, DWORD);
    STDMETHODIMP Get(REFGUID, DWORD, void*, DWORD, void*, DWORD, DWORD*);
    STDMETHODIMP QuerySupported(REFGUID, DWORD, DWORD*);
    //// CSourceStream ////////////////////////////
    CVCamStream(HRESULT* phr, CVCam* pParent, LPCWSTR pPinName);
    HRESULT OnThreadStartPlay();
    HRESULT OnThreadDestroy();
    HRESULT FillBuffer(IMediaSample* pms);
    HRESULT DecideBufferSize(IMemAllocator* pIMemAlloc, ALLOCATOR_PROPERTIES* pProperties);
    //HRESULT CheckMediaType(const CMediaType* pMediaType);
    HRESULT GetMediaType(CMediaType* pmt);
    HRESULT SetMediaType(const CMediaType* pmt);
    HRESULT OnThreadCreate(void);
    ~CVCamStream();
private:
    CVCam*              m_pParent;
    HBITMAP             m_hLogoBmp;
    CCritSec            m_cSharedState;
    IReferenceClock*    m_pClock;
};