
#include <Windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <initguid.h>
#include <propkey.h>
#include <functiondiscoverykeys_devpkey.h>
#include <functional>

std::function<void()> escapeKeyCallback;
HHOOK hHook = NULL;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*) lParam;
        if (wParam == WM_KEYDOWN) {
            if (p->vkCode == VK_NUMPAD0) {
                if (escapeKeyCallback) {
                    escapeKeyCallback();
                }
            }
        }
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

void InitializeHook() {
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (hHook == NULL) {
        printf("Failed to set keyboard hook\n");
    }
}

template<typename T>
void SetEscapeKeyCallback(T* object, void(T::* callback)()) {
    escapeKeyCallback = std::bind(callback, object);
}

class AudioKilla {
public:
    void KillDevice();

    int targetDeviceID = NULL;
    HRESULT device_hr;
    HRESULT device_name;
    LPWSTR pwszID = NULL;
    PROPVARIANT varName;
    BOOL bMute;
    UINT count;

    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDeviceCollection* pCollection = NULL;

    IAudioEndpointVolume* pEndpointVolume = NULL;
    IPropertyStore* pProps = NULL;
    IMMDevice* pDevice = NULL;

    AudioKilla() {
        if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {}
        device_hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    }
    
    void setTargetDevice(int ID) {
        targetDeviceID = ID;
    }

    void getDevices() {
        pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
        pCollection->GetCount(&count);

        for (UINT i = 0; i < count; i++) {
            pDevice = NULL;
            pCollection->Item(i, &pDevice);
            pDevice->GetId(&pwszID);

            pDevice->OpenPropertyStore(STGM_READ, &pProps);
            PropVariantInit(&varName);
            pProps->GetValue(PKEY_Device_FriendlyName, &varName);

            wprintf(L"ID %d - %s\n", i, varName.pwszVal);

            PropVariantClear(&varName);
            pDevice->Release();
        }
    }
};

void AudioKilla::KillDevice() {
    pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    pCollection->GetCount(&count);

    for (UINT i = 0; i < count; i++) {
        pDevice = NULL;
        pCollection->Item(i, &pDevice);
        pDevice->GetId(&pwszID);
        pEnumerator->GetDevice(pwszID, &pDevice);
        pDevice->Activate(__uuidof(IAudioEndpointVolume),
            CLSCTX_ALL, NULL, (void**)&pEndpointVolume);
        pEndpointVolume->GetMute(&bMute);

        pDevice->OpenPropertyStore(STGM_READ, &pProps);
        PropVariantInit(&varName);
        pProps->GetValue(PKEY_Device_FriendlyName, &varName);

        if (i == targetDeviceID) {
            if (!bMute) {
                pEndpointVolume->SetMute(TRUE, NULL);
                wprintf(L"Muted device %s (ID %d)\n", varName.pwszVal, i);
                break;
            }
            else {
                pEndpointVolume->SetMute(FALSE, NULL);
                wprintf(L"Unmuted device %s (ID %d)\n", varName.pwszVal, i);
                break;
            }
        }

        PropVariantClear(&varName);
        pDevice->Release();
    }
}

void menu() {
menu:
    system("cls");
    printf("1. Set target device\n");
    printf("2. Listen\n");
}

int main() {
    int choice;
    int target;

    AudioKilla auk;
    MSG msg;

    InitializeHook();
    SetEscapeKeyCallback(&auk, &AudioKilla::KillDevice);

    do {
        menu();
        printf("\n> ");
        scanf_s("%i", &choice);
        switch (choice) {
        case 1:
            system("cls");
            printf("Currently running devices:\n");
            auk.getDevices();
            printf("\nTarget ID: ");
            scanf_s("%i", &target);
            auk.setTargetDevice(target);
            break;
        case 2:
            while (GetMessage(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            UnhookWindowsHookEx(hHook);
            break;
        case 0:
            system("pause");
        default: printf("WARN. Incorrect option.\n");
        }
    } while (choice != 0);
    return 0;
}