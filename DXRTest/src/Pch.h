#pragma once

#define WIN32_LEAN_AND_MEAN


#include <memory>

#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <mutex>
#include <algorithm>

#include <string>

#include <typeinfo>

#include <taskflow.hpp>

#include <Windows.h>

#include <d3d12.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
using namespace Microsoft::WRL;

#include <DirectXMath.h>
using namespace DirectX;

#include <Xinput.h>


//#include "MatrixVector.h"



#include <Imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>



#include "UtilTypes.h"
#include "singleton_template.h"


// エラーチェック用ヘルパー関数
inline void ThrowIfFailed(HRESULT hr) {
    if ( FAILED(hr) ) {
        LPVOID errorMsg;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            hr,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&errorMsg,
            0, NULL);
        std::string errorStr = "HRESULT failed: " + std::string((char*)errorMsg);
        LocalFree(errorMsg);
        throw std::runtime_error(errorStr);
    }
}