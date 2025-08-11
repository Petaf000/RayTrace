#include "Pch.h"
#include "App.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    
	App::SetDefaultUnhandledExceptionFilter();

	App::SetWindowName("Test");

	try {
        App::Run();
	}
	catch ( const std::runtime_error& e ) {
		MessageBoxA(nullptr, e.what(), "エラー", MB_OK);
		return -1;
	}

	App::Cleanup();

    return 0;
}