
//#define USETHREADS
//#define FPSLIMIT 4000

#define HEIGHT GetSystemMetrics(SM_CYSCREEN)
#define WIDTH  GetSystemMetrics(SM_CXSCREEN)

#include <d3d9.h>
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>
#include <thread>

#pragma comment(lib, "winmm.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

D3DCOLOR black = D3DCOLOR(0x00000000);
D3DCOLOR waiting = D3DCOLOR(0x00404040);
D3DCOLOR currentColor = black;

auto soundPlayTime = std::chrono::high_resolution_clock::now();
auto soundStartTime = soundPlayTime;
auto clickTime = soundPlayTime;
auto redStartTime = soundPlayTime;

int state = 0; // 0 - stats screen, 1 - waiting (silence), 2 - sound playing
int errors = 0;
int click = 0;
std::vector<int64_t> clickTimes;
int64_t clickAmount = 0;

bool stop = false;
bool soundPlaying = false;

void GenerateTone(BYTE* buffer, int sampleRate, double frequency, double duration, double volume) {
    int numSamples = (int)(sampleRate * duration);
    short* samples = (short*)buffer;

    for (int i = 0; i < numSamples; i++) {
        double t = (double)i / sampleRate;
        double value = sin(2.0 * M_PI * frequency * t) * volume * 32767.0;
        samples[i * 2] = (short)value;     // Left channel
        samples[i * 2 + 1] = (short)value; // Right channel
    }
}

void PlayTone(double frequency, double duration, double volume) {
    const int SAMPLE_RATE = 44100;
    const int BITS_PER_SAMPLE = 16;
    const int CHANNELS = 2;

    int numSamples = (int)(SAMPLE_RATE * duration);
    int bufferSize = numSamples * CHANNELS * (BITS_PER_SAMPLE / 8);

    BYTE* buffer = new BYTE[bufferSize];
    GenerateTone(buffer, SAMPLE_RATE, frequency, duration, volume);

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = CHANNELS;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = BITS_PER_SAMPLE;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    HWAVEOUT hWaveOut = NULL;
    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
        WAVEHDR waveHdr = {};
        waveHdr.lpData = (LPSTR)buffer;
        waveHdr.dwBufferLength = bufferSize;

        waveOutPrepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &waveHdr, sizeof(WAVEHDR));

        while (!(waveHdr.dwFlags & WHDR_DONE)) Sleep(10);

        waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);
    }
    delete[] buffer;
}

UINT sizeofRAWINPUTHEADER = sizeof(RAWINPUTHEADER);
RAWINPUT* raw_buf = (PRAWINPUT)malloc(800);
UINT cb_size = 0;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INPUT: {
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &cb_size, sizeofRAWINPUTHEADER);
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, raw_buf, &cb_size, sizeofRAWINPUTHEADER);

        if (raw_buf->header.dwType == RIM_TYPEMOUSE &&
            (raw_buf->data.mouse.usButtonFlags == RI_MOUSE_LEFT_BUTTON_DOWN ||
                raw_buf->data.mouse.usButtonFlags == RI_MOUSE_RIGHT_BUTTON_DOWN)) {

            if (state == 0) {
                state = 1;
                soundPlaying = false;
                currentColor = waiting;
  
                int rnd = rand() % 4000 + 2000;
                auto now = std::chrono::high_resolution_clock::now();
                soundPlayTime = now + std::chrono::milliseconds(rnd);
                redStartTime = now;
            }
            else if (state == 1) { 
                currentColor = black;
                auto now = std::chrono::high_resolution_clock::now();
                auto timeDiff = now - redStartTime;
                if (std::chrono::duration_cast<std::chrono::microseconds>(timeDiff).count() >= 500000) {
                    state = 0;
                    errors++;
                    std::cout << "Too early!\n";
                }
            }
            else if (state == 2) { 
                state = 0;
                currentColor = black;
                clickTime = std::chrono::high_resolution_clock::now();
                auto timeDiff = clickTime - soundStartTime;
                int64_t reactionTime = std::chrono::duration_cast<std::chrono::microseconds>(timeDiff).count();
                clickTimes.push_back(reactionTime);
                std::cout << "#" << clickAmount + 1 << ": " << reactionTime / 1000.0 << "ms\n";
                    clickAmount++;
                soundPlaying = false;
            }
        }

        if (raw_buf->header.dwType == RIM_TYPEKEYBOARD) {
            if (raw_buf->data.keyboard.Message == WM_KEYDOWN || raw_buf->data.keyboard.Message == WM_SYSKEYDOWN) {
                if (raw_buf->data.keyboard.VKey == 0x1B) {  // ESC key
                    if (clickTimes.size() > 0) {
                        sort(clickTimes.begin(), clickTimes.end());

                        size_t size = clickTimes.size();
                        int64_t sum = 0;
                        for (int64_t ct : clickTimes) {
                            sum += ct;
                        }
                        int64_t average = sum / size;

                        double standard_deviation = 0.0;
                        for (int64_t ct : clickTimes) {
                            standard_deviation += pow(ct - average, 2);
                        }
                        double stdev = sqrt(standard_deviation / (size - 1));

                        std::cout << "\n=== STATISTICS ===\n";
                        std::cout << "Max: " << clickTimes.back() / 1000.0 << "ms\n";
                        std::cout << "Avg: " << average / 1000.0 << "ms\n";
                        std::cout << "Min: " << clickTimes.front() / 1000.0 << "ms\n";
                        std::cout << "STDEV: " << stdev / 1000.0 << "\n";
                        std::cout << "Total Clicks: " << clickAmount + errors << "\n";
                        std::cout << "Successful Clicks: " << clickAmount << "\n";
                        std::cout << "Early Clicks: " << errors << "\n";
                    }
                    stop = true;
                    DestroyWindow(hWnd);
                    break;
                }
            }
        }

        if (GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT)
            DefWindowProc(hWnd, msg, wParam, lParam);
        break;
    }
    case WM_CLOSE:
    case WM_DESTROY:
    case WM_QUIT:
        stop = TRUE;
        PostQuitMessage(0);
        return DefWindowProc(hWnd, msg, wParam, lParam);
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int main()
{
    srand(static_cast<unsigned>(time(NULL)));
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "=== Audio Reaction Time Test ===\n";
    std::cout << "Press ESC to see statistics and exit.\n\n";

    int ret = WinMain(GetModuleHandle(NULL), NULL, NULL, SW_SHOWNORMAL);
    return ret;
}

inline void renderFunc(LPDIRECT3DDEVICE9 d3ddev) {
    if (state == 1 && !soundPlaying && std::chrono::high_resolution_clock::now() >= soundPlayTime) {
        state = 2;
        soundPlaying = true;
        soundStartTime = std::chrono::high_resolution_clock::now();

        PlayTone(800.0, 0.15, 0.3);
    }

    d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, currentColor, 0.0f, 0);
    d3ddev->Present(NULL, NULL, NULL, NULL);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    srand(static_cast<unsigned int>(time(NULL)));

    HWND hWnd;
    WNDCLASSEX wc;
    MSG msg;

    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"AudioReactionTest";

    RegisterClassEx(&wc);

    hWnd = CreateWindowEx(0, L"AudioReactionTest", L"Audio Reaction Time Test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,  // НЕ WS_POPUP!
        100, 100, 800, 600,
        NULL, NULL, hInstance, NULL);
    ShowWindow(hWnd, nCmdShow);

    LPDIRECT3D9 d3d;
    LPDIRECT3DDEVICE9 d3ddev;
    D3DPRESENT_PARAMETERS d3dpp;

    d3d = Direct3DCreate9(D3D_SDK_VERSION);

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hWnd;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferWidth = (UINT)GetSystemMetrics(SM_CXSCREEN);
    d3dpp.BackBufferHeight = (UINT)GetSystemMetrics(SM_CYSCREEN);
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &d3ddev);

    D3DVIEWPORT9 pViewport = { 0, 0, (DWORD)WIDTH, (DWORD)HEIGHT, 0.0, 1.0 };
    d3ddev->SetViewport(&pViewport);

    RAWINPUTDEVICE Keyboard;
    Keyboard.usUsagePage = 0x01;
    Keyboard.usUsage = 0x06;
    Keyboard.dwFlags = RIDEV_NOLEGACY;
    Keyboard.hwndTarget = hWnd;
    RegisterRawInputDevices(&Keyboard, 1, sizeof(RAWINPUTDEVICE));

    RAWINPUTDEVICE Mouse;
    Mouse.usUsagePage = 0x01;
    Mouse.usUsage = 0x02;
    Mouse.dwFlags = RIDEV_NOLEGACY;
    Mouse.hwndTarget = hWnd;
    RegisterRawInputDevices(&Mouse, 1, sizeof(RAWINPUTDEVICE));

    ShowCursor(FALSE);
    SetCursor(NULL);
    d3ddev->ShowCursor(FALSE);

    HANDLE process = GetCurrentProcess();
    SetPriorityClass(process, REALTIME_PRIORITY_CLASS);

    while (!stop) {
        while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
            DispatchMessage(&msg);
        renderFunc(d3ddev);
    }

    d3ddev->Release();
    d3d->Release();
    free(raw_buf);
    DestroyWindow(hWnd);
    PostQuitMessage(0);
    ShowCursor(true);

    std::cout << "\nPress Enter to Continue\n";
    getchar();

    return 0;
}
