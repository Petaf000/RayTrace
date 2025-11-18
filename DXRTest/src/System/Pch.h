#pragma once

#define WIN32_LEAN_AND_MEAN


#include <memory>

#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <chrono>

#include <string>

#include <typeinfo>

#include <taskflow.hpp>

#include <Windows.h>

#include <d3d12.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <dxcapi.h>
#include <wrl/client.h>
using namespace Microsoft::WRL;

#include <DirectXMath.h>
using namespace DirectX;

#include <Xinput.h>


//#include "MatrixVector.h"



#include <Imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>



#include "Utils/UtilTypes.h"
#include "Utils/singleton_template.h"


// エラーチェック用ヘルパー関数
inline void ThrowIfFailedImpl(HRESULT hr, const char* file, int line) {
    if (FAILED(hr)) {
        LPWSTR errorMsg = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            hr,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&errorMsg,
            0, nullptr);

        std::wstring errorDesc = errorMsg ? errorMsg : L"Unknown error";
        if (errorMsg) LocalFree(errorMsg);

        // wstringからstringへ変換(UTF-8)
        int descSize = WideCharToMultiByte(CP_UTF8, 0, errorDesc.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8Desc(descSize - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, errorDesc.c_str(), -1, utf8Desc.data(), descSize, nullptr, nullptr);

        std::string finalError = std::format(
            "{}:{} - HRESULT failed (0x{:08X}): {}",
            file,
            line,
            static_cast<unsigned>(hr),
            utf8Desc
        );

        throw std::runtime_error(finalError);
    }
}

#define ThrowIfFailed(hr) ::ThrowIfFailedImpl((HRESULT)(hr), __FILE__, __LINE__)