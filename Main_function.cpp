#include<iostream>
#include <Mmdeviceapi.h>
#include <Audiopolicy.h>
#include <Audioclient.h>
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"


class Noise_Gen {
public:
    drflac* flacEngine;
    WAVEFORMATEXTENSIBLE format;
    void SetFormat(WAVEFORMATEX* wfex) {
        if(wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            format = *reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wfex);
        } else {
            format.Format = *wfex;
            format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            INIT_WAVEFORMATEX_GUID(&format.SubFormat, wfex->wFormatTag);
            format.Samples.wValidBitsPerSample = format.Format.wBitsPerSample;
            format.dwChannelMask = 0;
        }
    }

    
    void FillBuffer(UINT32 bufferFrameCount, BYTE* pData, DWORD* flags) {
        const UINT16 formatTag = EXTRACT_WAVEFORMATEX_ID(&format.SubFormat);
        if(formatTag == WAVE_FORMAT_IEEE_FLOAT) {
           
            float* fData = (float*)pData;
            UINT64 frames_fetched = drflac_read_pcm_frames_f32(flacEngine, bufferFrameCount, fData);
            std::cout << "Frames fetched: "<< frames_fetched<< std::endl;
            if (frames_fetched < bufferFrameCount) {
                *flags = AUDCLNT_BUFFERFLAGS_SILENT;
            }
        }
        else if (formatTag == WAVE_FORMAT_PCM) {
            INT32* fData = (INT32*)pData;
            UINT64 frames_fetched = drflac_read_pcm_frames_s32(flacEngine, bufferFrameCount, fData);
            std::cout << "Frames fetched: " << frames_fetched << std::endl;
            if (frames_fetched < bufferFrameCount) {
                *flags = AUDCLNT_BUFFERFLAGS_SILENT;
            }
        }
        else {
            *flags = AUDCLNT_BUFFERFLAGS_SILENT;
        }
    }
};

#define REFTIMES_PER_SEC  10000000ll
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

HRESULT PlayAudioStream(Noise_Gen* pMySource) {
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    REFERENCE_TIME hnsActualDuration;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IPropertyStore* pPropertyStore = NULL;
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    IAudioRenderClient* pRenderClient = NULL;
    WAVEFORMATEX* pwfx = NULL;
    UINT32 bufferFrameCount;
    BYTE* pData;
    DWORD flags = 0;
    

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    EXIT_ON_ERROR(hr);
    hr = pEnumerator->GetDefaultAudioEndpoint(
        eRender, eConsole, &pDevice);
    EXIT_ON_ERROR(hr);

    hr = pDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
    EXIT_ON_ERROR(hr);
 
    hr = pDevice->Activate(__uuidof(pAudioClient), CLSCTX_ALL,
        NULL, (void**) &pAudioClient);
    EXIT_ON_ERROR(hr);
    hr = pAudioClient->GetMixFormat(&pwfx);
    EXIT_ON_ERROR(hr);

    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
        0, hnsRequestedDuration,
        0, pwfx, NULL);
    EXIT_ON_ERROR(hr);
    
    pMySource->SetFormat(pwfx);
    
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR(hr);
    hr = pAudioClient->GetService(IID_PPV_ARGS(&pRenderClient));
    EXIT_ON_ERROR(hr);
   
    hr = pRenderClient->GetBuffer(bufferFrameCount, &pData);
    EXIT_ON_ERROR(hr);

  
    pMySource->FillBuffer(bufferFrameCount, pData, &flags);    

    hr = pRenderClient->ReleaseBuffer(bufferFrameCount, flags);
    EXIT_ON_ERROR(hr);
   
    hnsActualDuration = REFTIMES_PER_SEC * bufferFrameCount / pwfx->nSamplesPerSec;
    hr = pAudioClient->Start(); 
    EXIT_ON_ERROR(hr);
   
    DWORD sleepTime;
    while(flags != AUDCLNT_BUFFERFLAGS_SILENT) {
      
        sleepTime = (DWORD) (hnsActualDuration / REFTIMES_PER_MILLISEC / 2);
        if(sleepTime != 0)
            Sleep(sleepTime);
       
        UINT32 numFramesPadding;
        hr = pAudioClient->GetCurrentPadding(&numFramesPadding);
        EXIT_ON_ERROR(hr);

        UINT32 numFramesAvailable = bufferFrameCount - numFramesPadding;
       
        hr = pRenderClient->GetBuffer(numFramesAvailable, &pData);
        EXIT_ON_ERROR(hr);

       
        pMySource->FillBuffer(numFramesAvailable, pData, &flags);

        hr = pRenderClient->ReleaseBuffer(numFramesAvailable, flags);
        EXIT_ON_ERROR(hr);
    }
   
    sleepTime = (DWORD) (hnsActualDuration / REFTIMES_PER_MILLISEC / 2);
    if(sleepTime != 0)
        Sleep(sleepTime);
    hr = pAudioClient->Stop();  
    EXIT_ON_ERROR(hr);

Exit:
    CoTaskMemFree(pwfx);
    SAFE_RELEASE(pRenderClient);
    SAFE_RELEASE(pAudioClient);
    SAFE_RELEASE(pDevice);
    SAFE_RELEASE(pDevice);
    SAFE_RELEASE(pEnumerator);
    return hr;
}

int main() {
    HRESULT hr = CoInitialize(nullptr);
    if(FAILED(hr)) { return hr; }
    Noise_Gen ng;
    ng.flacEngine = drflac_open_file("example.flac", NULL);
    PlayAudioStream(&ng);
    std::cout << "" << std::endl;
    std::cout << " Project Made By:\n Shubham Kumar Rai\n Vaibhav Singh\n Abhigyan Mittal\n Rahul Sharma" << std::endl;
    CoUninitialize();
}