#include "App.h"

#include <shellapi.h>
#include <strsafe.h>

#include "GameManager.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void App::Init() {

    // wWinMain�̈����擾
    {
        if ( ( m_hInstance = GetModuleHandle(nullptr) ) == NULL )
            throw std::runtime_error("�C���X�^���X������Ɏ擾�ł��܂���ł���");

        // �R�}���h���C�������̎擾
        // NOTE: �Ƃ肠�����̎����ł�
        // TODO: �R�}���h���C�����K�v�Ȏ��ɍēx����
        int argc;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        while ( argc > 1 ) {
            m_pCmdLine = argv[argc - 1];
            argc--;
        }
    }
    



	// �E�B���h�E�N���X�̓o�^
    {
        WNDCLASSEX wcex;
        {
            wcex.cbSize = sizeof(WNDCLASSEX);
            wcex.style = 0;
            wcex.lpfnWndProc = WindowProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = m_hInstance;
            wcex.hIcon = nullptr;
            wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wcex.hbrBackground = nullptr;
            wcex.lpszMenuName = nullptr;
            wcex.lpszClassName = m_windowName.c_str();
            wcex.hIconSm = nullptr;

            RegisterClassEx(&wcex);


            RECT rc = { 0, 0, (LONG)m_windowSize.Width, (LONG)m_windowSize.Height };
            AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);



            m_hWnd = CreateWindowEx(0,
                m_windowName.c_str(),
                m_windowName.c_str(),
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT,
                1280, 720,
                nullptr, nullptr, m_hInstance, nullptr);

            if ( FAILED(m_hWnd) )
                throw std::runtime_error("�E�B���h�E�̍쐬�Ɏ��s���܂���");
        }

        if ( FAILED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED)) )
            throw std::runtime_error("COM�������Ɏ��s���܂���");
    }


    // TODO: ������

	Singleton<GameManager>::getInstance().Init();



    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);

    m_isRunning = true;

    return;
}




void App::Run()
{
    Init();



	// ���C�����[�v
    MSG msg{};
    float delta;
    while ( true ) {
        if ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ) {
            if ( msg.message == WM_QUIT ) {
                break;
            }
            else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else {

			// �σt���[�����[�g(�ő�600FPS)
            if ( ( delta = m_time.DeltaTime) >= 1.0f / m_targetFrameRate) {


                std::string fps = "FPS" + std::to_string(1.0f / delta);
                App::SetWindowName(fps);
                GetClientRect(m_hWnd, &m_windowSize.Rect);
                
                // TODO: �t���[������
                try {
					Singleton<GameManager>::getInstance().Update();
                }
                catch ( const std::exception& e ) {
                    MessageBoxA(nullptr, e.what(), "�G���[", MB_OK);
					Cleanup();
                    break;
                }

				m_time.LastTime = m_time.Qpc.QuadPart;
            }
        }
    }
}





void App::Cleanup() {

	// TODO: �N���[���A�b�v����

	Singleton<GameManager>::getInstance().UnInit();



    UnregisterClass(m_windowName.c_str(), m_hInstance);

    CoUninitialize();

    HWND m_hWnd = nullptr;
    HINSTANCE m_hInstance = NULL;
    PWSTR m_pCmdLine = nullptr;
    m_isRunning = false;
}






#pragma region Windows


LRESULT App::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if ( ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam) )
        return true;

    switch ( message ) {

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_KEYDOWN:
            switch ( wParam ) {
                case VK_ESCAPE:
                    DestroyWindow(hWnd);
                    break;
                case VK_F11:
                    ToggleFullscreen();
                    break;
            }
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);

    }

    return 0;
}



// Windows��O�t�B���^
// NOTE: �Ƃ肠�����G���[�t�B���^�[�������Ă܂��@�J�X�^���ł�肽���Ƃ��͕ʓrSetUnhandledExceptionFilter���Ă�������
LONG WINAPI App::TopLevelExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo) {
    TCHAR szText[1024] = { TEXT('\0') };

    StringCchPrintf(szText,
        sizeof(szText) / sizeof(szText[0]),
        TEXT("ExceptionAddress=%X, ExceptionCode=%X, ExceptionFlags=%X, ExceptionInformation=%X, NumberParameters=%X"),
        ExceptionInfo->ExceptionRecord->ExceptionAddress,
        ExceptionInfo->ExceptionRecord->ExceptionCode,
        ExceptionInfo->ExceptionRecord->ExceptionFlags,
        ExceptionInfo->ExceptionRecord->ExceptionInformation,
        ExceptionInfo->ExceptionRecord->NumberParameters);

    MessageBox(NULL, szText, TEXT("Win32�G���[�t�B���^�["), MB_OK);

    return 0;
}


#pragma endregion