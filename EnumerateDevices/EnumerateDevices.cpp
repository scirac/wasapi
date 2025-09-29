#include <iostream>
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <locale>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

int main()
{
    std::locale::global(std::locale(""));
    std::wcout.imbue(std::locale());
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        std::cerr << "Unable to initialize COM in thread: " << hr << std::endl;
        return hr;
    }

    IMMDeviceEnumerator* pEnumerator = NULL;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create device enumerator: " << hr << std::endl;
        CoUninitialize();
        return hr;
    }

    IMMDeviceCollection* pCollection = NULL;
    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr))
    {
        std::cerr << "Failed to enumerate audio endpoints: " << hr << std::endl;
        pEnumerator->Release();
        CoUninitialize();
        return hr;
    }

    UINT count = 0;
    hr = pCollection->GetCount(&count);
    if (FAILED(hr))
    {
        std::cerr << "Failed to get device count: " << hr << std::endl;
        pCollection->Release();
        pEnumerator->Release();
        CoUninitialize();
        return hr;
    }

    std::cout << "Number of active audio input devices: " << count << std::endl;

    for (UINT i = 0; i < count; ++i)
    {
        IMMDevice* pDevice = NULL;
        hr = pCollection->Item(i, &pDevice);
        if (FAILED(hr))
        {
            std::cerr << "Failed to get device at index " << i << ": " << hr << std::endl;
            continue;
        }

        LPWSTR pwstrID = NULL;
        hr = pDevice->GetId(&pwstrID);
        if (SUCCEEDED(hr))
        {
            std::wcout << L"Device ID: " << pwstrID << std::endl;
            CoTaskMemFree(pwstrID);
        }

        IPropertyStore* pProps = NULL;
        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (SUCCEEDED(hr))
        {
            PROPVARIANT pvName;
            PropVariantInit(&pvName);
            hr = pProps->GetValue(PKEY_Device_FriendlyName, &pvName);
            if (SUCCEEDED(hr))
            {
                std::wcout << L"Device Name: " << pvName.pwszVal << std::endl;
                PropVariantClear(&pvName);
            }
            pProps->Release();
        }

        IAudioClient* pAudioClient = NULL;
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
        if (SUCCEEDED(hr))
        {
            WAVEFORMATEX* pwfx = NULL;
            hr = pAudioClient->GetMixFormat(&pwfx);
            if (SUCCEEDED(hr))
            {
                std::cout << "Format: " << std::endl;
                std::cout << "  wFormatTag: " << pwfx->wFormatTag << std::endl;
                std::cout << "  nChannels: " << pwfx->nChannels << std::endl;
                std::cout << "  nSamplesPerSec: " << pwfx->nSamplesPerSec << std::endl;
                std::cout << "  nAvgBytesPerSec: " << pwfx->nAvgBytesPerSec << std::endl;
                std::cout << "  nBlockAlign: " << pwfx->nBlockAlign << std::endl;
                std::cout << "  wBitsPerSample: " << pwfx->wBitsPerSample << std::endl;
                std::cout << "  cbSize: " << pwfx->cbSize << std::endl;
                CoTaskMemFree(pwfx);
            }
            pAudioClient->Release();
        }

        pDevice->Release();
    }

    pCollection->Release();
    pEnumerator->Release();
    CoUninitialize();

    return 0;
}
