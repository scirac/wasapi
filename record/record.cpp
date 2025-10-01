#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <avrt.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <locale>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "avrt.lib")

#define BUFFER_LENGTH_MS  10 //BUFFER长度10ms,这里系统中定义的frame含义是一个采样点，避免混淆，用buffer代替通常的frame
#define REFTIMES_PER_SEC  BUFFER_LENGTH_MS*10000 //BUFFER长度10ms，以100ns为单位

void WriteWavFile(const std::wstring& filename, const std::vector<BYTE>& audioData, WAVEFORMATEX* pwfx) {
    std::ofstream out(filename, std::ios::binary);
    int dataSize = static_cast<int>(audioData.size());
	int chunkSize = 38 + dataSize + pwfx->cbSize; // 用38而不是36是因为cbSize占用2字节，但是实测36也没有问题

    out.write("RIFF", 4);
    out.write((const char*)&chunkSize, 4);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    int fmtChunkSize = sizeof(WAVEFORMATEX) + pwfx->cbSize;
    out.write((const char*)&fmtChunkSize, 4);
    out.write((const char*)pwfx, sizeof(WAVEFORMATEX));

    // 写入扩展数据
    if (pwfx->cbSize > 0) {
        BYTE* pExtraData = (BYTE*)pwfx + sizeof(WAVEFORMATEX);
        out.write((const char*)pExtraData, pwfx->cbSize);
    }

    out.write("data", 4);
    out.write((const char*)&dataSize, 4);
    out.write((const char*)audioData.data(), dataSize);

    out.close();
}

int main(int argc,char* argv[]) {

	int recordSeconds = 10; //default record 10 seconds
	int mode = 1; //default mode
    // 命令行参数解析
    for (int i = 1; i < argc - 1; ++i) {
        if ((strcmp(argv[i], "-L") == 0)|| (strcmp(argv[i], "-l") == 0)) {
            int val = atoi(argv[i + 1]);
            if (val > 0) recordSeconds = val;
        }
        if ((strcmp(argv[i], "-M") == 0) || (strcmp(argv[i], "-m") == 0)) {
            int val = atoi(argv[i + 1]);
            if (val >= 0) mode = val;
        }
    }

    HRESULT hr = CoInitialize(nullptr);

    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    IMMDevice* pDevice = nullptr;
    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice);

    IPropertyStore* pProps = nullptr;
    hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
    if (SUCCEEDED(hr)) {
        PROPVARIANT varName;
        PropVariantInit(&varName);

        // 获取设备友好名称
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        if (SUCCEEDED(hr)) {
            std::locale::global(std::locale(""));
            std::wcout.imbue(std::locale());
            std::wcout << L"Record Device: " << varName.pwszVal << std::endl;
        }
        PropVariantClear(&varName);
        pProps->Release();
    }

    IAudioClient2* pAudioClient = nullptr;
    hr = pDevice->Activate(__uuidof(IAudioClient2), CLSCTX_ALL, nullptr, (void**)&pAudioClient);

    WAVEFORMATEX* pwfx = nullptr;
    hr = pAudioClient->GetMixFormat(&pwfx);
	// 设置音频类别
	AudioClientProperties properties = { };
	properties.cbSize = sizeof(AudioClientProperties);
    switch (mode)
    {
	case 0://RAW mode
        properties.eCategory = AudioCategory_Other;
        properties.bIsOffload = FALSE;
        properties.Options = AUDCLNT_STREAMOPTIONS_RAW;
		break;
	case 1:
		properties.eCategory = AudioCategory_Other;//default mode
		break;
	case 2:
		properties.eCategory = AudioCategory_Communications;
		break;
	case 3:
		properties.eCategory = AudioCategory_Speech;
		break;
    default:
        properties.eCategory = AudioCategory_Other;//default mode
        break;
    }
	pAudioClient->SetClientProperties(&properties);

    std::cout << "Default device info:" << std::endl;
	std::cout << "Format Tag:" << pwfx->wFormatTag << std::endl;
    std::cout << "Channels: " << pwfx->nChannels << std::endl;
    std::cout << "Samples Rate: " << pwfx->nSamplesPerSec << std::endl;
    std::cout << "Avg Bytes per Sec: " << pwfx->nAvgBytesPerSec << std::endl;
    std::cout << "Block Align: " << pwfx->nBlockAlign << std::endl;
    std::cout << "Bits per Sample: " << pwfx->wBitsPerSample << std::endl;
	std::cout << "Size: " << pwfx->cbSize << std::endl;
	std::cout << "Recording Mode: " << mode << std::endl;
	std::cout << "Recording Length: " << recordSeconds << " seconds" << std::endl;

    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        hnsRequestedDuration,
        0,
        pwfx,
        nullptr
     );

    IAudioCaptureClient* pCaptureClient = nullptr;
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);

    hr = pAudioClient->Start();

    std::vector<BYTE> audioData;
    UINT32 packetLength = 0;

	
	long TotalFrames = recordSeconds * pwfx->nSamplesPerSec;
    long CurrentFrames = 0;
    std::cout << "start recording..." << std::endl;

    while (CurrentFrames < TotalFrames) {
        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        while (packetLength != 0) {
            BYTE* pData;
            UINT32 numFramesAvailable;
            DWORD flags;

            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);

            size_t bytesToCopy = numFramesAvailable * pwfx->nBlockAlign;
            audioData.insert(audioData.end(), pData, pData + bytesToCopy);

            hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
			CurrentFrames += numFramesAvailable;
            hr = pCaptureClient->GetNextPacketSize(&packetLength);
        }
        Sleep(BUFFER_LENGTH_MS);
    }

    hr = pAudioClient->Stop();

    std::wcout << L"Saving to record.wav..." << std::endl;
    WriteWavFile(L"record.wav", audioData, pwfx);

    std::wcout << L"Saved successfully!" << std::endl;

    CoTaskMemFree(pwfx);
    pCaptureClient->Release();
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    return 0;
}
