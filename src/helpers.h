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

struct PipelineStateStream
{
    CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
    CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
    CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
    CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
    CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DSVFormat;
    CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
};

static inline void
    ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

static inline D3D12_ROOT_SIGNATURE_FLAGS VertexPixelRootSignatureFlags()
{
    return D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
}
