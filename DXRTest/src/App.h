#pragma once

class App
{
public:
	static void Run();
	static void Cleanup();
    static void ToggleFullscreen() {
        m_isFullscreen = !m_isFullscreen;

        if ( m_isFullscreen ) {
            SetWindowLong(m_hWnd, GWL_STYLE, WS_POPUP);
            ShowWindow(m_hWnd, SW_MAXIMIZE);
        }
        else {
            SetWindowLong(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

            SetWindowPos(m_hWnd, NULL, 0, 0, 1280, 720, ( SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED ));
            ShowWindow(m_hWnd, SW_NORMAL);
        }
    };



	static HWND GetWindowHandle() { return m_hWnd; }
    static bool IsRunning() { return m_isRunning; }
	static SQUARE GetWindowSize() { return m_windowSize; }
	


    static void SetWindowName(const std::string& name) {  
       m_windowName = name;  
       if (m_isRunning)  
           SetWindowText(m_hWnd, m_windowName.c_str()); // Use SetWindowTextW for wide strings  
    }

	static void SetWindowSize(LONG width, LONG height) {
		m_windowSize = SQUARE(width, height);
		SetWindowPos(m_hWnd, NULL, 0, 0, m_windowSize.Width, m_windowSize.Height, ( SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED ));
	}

	static void SetFrameRate(float fps) {
		m_targetFrameRate = fps;
	}

    // とりあえずのエラーフィルターを付けます
	static void SetDefaultUnhandledExceptionFilter() {
		SetUnhandledExceptionFilter(TopLevelExceptionFilter);
	}

    static PWSTR GetCommand() { return (m_pCmdLine == nullptr) ? const_cast<PWSTR>(L"") : m_pCmdLine; }
	
protected:
    static void Init();
    
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LONG WINAPI TopLevelExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo);

private:
    inline static bool m_isRunning = false;
    inline static bool m_isFullscreen = false;
    inline static SQUARE m_windowSize = SQUARE(1280, 720);
	inline static std::string m_windowName = "DefaultWin32App";

    inline static Time m_time = {};

	// Window Handles
    inline static HWND m_hWnd = nullptr;
    inline static HINSTANCE m_hInstance = NULL;
	inline static PWSTR m_pCmdLine = nullptr;

	inline static float m_targetFrameRate = 60.0f;
};