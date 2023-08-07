#pragma once

// include the basic windows header files and the Direct3D header files
#include <initguid.h> //for IID macros
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <windowsx.h>
#include <winuser.h>
#include <wrl.h>
#include <wrl/client.h>

#include <vector>

#include "d3dx12.h"

#ifdef max
#    undef max
#endif

#ifdef min
#    undef min
#endif

namespace WRL = Microsoft::WRL;
// WRL is a smart pointer that calls Release() before destructs.
// When you need to use & of a raw pointer, use .GetAddressOf() instead, it's
// not as convinient though

static inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}
