#include "cube.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "helpers.h"
#include "window.h"

#include "commandqueue.h"
#include <memory>
#include <SDL_events.h>

using namespace Microsoft::WRL;

// Clamp a value between a min and max range.
template <typename T>
constexpr const T& clamp(const T& val, const T& min, const T& max)
{
    return val < min ? min : val > max ? max :
                                         val;
}

// Vertex data for a colored cube.
struct VertexPosColor
{
    glm::vec3 Position;
    glm::vec3 Color;
};

static VertexPosColor g_Vertices[8] = {
    { glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(0.0f, 0.0f, 0.0f) }, // 0
    { glm::vec3(-1.0f, 1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f) },  // 1
    { glm::vec3(1.0f, 1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f) },   // 2
    { glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f) },  // 3
    { glm::vec3(-1.0f, -1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f) },  // 4
    { glm::vec3(-1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f) },   // 5
    { glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f) },    // 6
    { glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 1.0f) }    // 7
};

static WORD g_Indicies[36] = {
    0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 4, 5, 1, 4, 1, 0, 3, 2, 6, 3, 6, 7, 1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7
};

CubeApp::CubeApp() :
    m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)),
    m_FoV(glm::radians(45.0f)),
    m_ContentLoaded(false)
{
}

void CubeApp::UpdateBufferResource(
    ComPtr<ID3D12GraphicsCommandList2> commandList,
    ID3D12Resource**                   pDestinationResource,
    ID3D12Resource**                   pIntermediateResource,
    size_t                             numElements,
    size_t                             elementSize,
    const void*                        bufferData,
    D3D12_RESOURCE_FLAGS               flags)
{
    auto device = Application::Get().GetDevice();

    size_t bufferSize = numElements * elementSize;

    // Create a committed resource for the GPU resource in a default heap.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(pDestinationResource)));

    // Create an committed resource for the upload.
    if (bufferData)
    {
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(pIntermediateResource)));

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData                  = bufferData;
        subresourceData.RowPitch               = bufferSize;
        subresourceData.SlicePitch             = subresourceData.RowPitch;

        UpdateSubresources(commandList.Get(),
                           *pDestinationResource,
                           *pIntermediateResource,
                           0,
                           0,
                           1,
                           &subresourceData);
    }
}

bool CubeApp::Initialize() { return true; }

void CubeApp::LoadVertices()
{
    auto device       = Application::Get().GetDevice();
    auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
    auto commandList  = commandQueue->GetCommandList();

    // Upload vertex buffer data.
    ComPtr<ID3D12Resource> intermediateVertexBuffer;
    UpdateBufferResource(commandList,
                         &m_VertexBuffer,
                         &intermediateVertexBuffer,
                         _countof(g_Vertices),
                         sizeof(VertexPosColor),
                         g_Vertices);

    // Create the vertex buffer view.
    m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
    m_VertexBufferView.SizeInBytes    = sizeof(g_Vertices);
    m_VertexBufferView.StrideInBytes  = sizeof(VertexPosColor);
    m_VertexBuffer->SetName(L"Vertex Buffer");

    // Upload index buffer data.
    ComPtr<ID3D12Resource> intermediateIndexBuffer;
    UpdateBufferResource(commandList,
                         &m_IndexBuffer,
                         &intermediateIndexBuffer,
                         _countof(g_Indicies),
                         sizeof(WORD),
                         g_Indicies);

    // Create index buffer view.
    m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
    m_IndexBufferView.Format         = DXGI_FORMAT_R16_UINT;
    m_IndexBufferView.SizeInBytes    = sizeof(g_Indicies);
    m_IndexBuffer->SetName(L"Index Buffer");

    auto fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);
}

bool CubeApp::LoadContent()
{
    auto device = Application::Get().GetDevice();
    // auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
    // auto commandList  = commandQueue->GetCommandList();

    LoadVertices();

    // Create the descriptor heap for the depth-stencil view.
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors             = 1;
    dsvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DSVHeap)));

    // Load the vertex shader.
    ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", &vertexShaderBlob));

    // Load the pixel shader.
    ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", &pixelShaderBlob));

    // Create the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // Create a root signature.
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion                    = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // Allow input layout and deny unnecessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    // A single 32-bit constant root parameter that is used by the vertex shader.
    CD3DX12_ROOT_PARAMETER1 rootParameters[1];
    rootParameters[0].InitAsConstants(sizeof(glm::mat4) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    // Serialize the root signature.
    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
                                                        featureData.HighestVersion,
                                                        &rootSignatureBlob,
                                                        &errorBlob));
    // Create the root signature.
    ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
    } pipelineStateStream;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets      = 1;
    rtvFormats.RTFormats[0]          = DXGI_FORMAT_R8G8B8A8_UNORM;

    pipelineStateStream.pRootSignature        = m_RootSignature.Get();
    pipelineStateStream.InputLayout           = { inputLayout, _countof(inputLayout) };
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS                    = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.PS                    = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    pipelineStateStream.DSVFormat             = DXGI_FORMAT_D32_FLOAT;
    pipelineStateStream.RTVFormats            = rtvFormats;

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
        sizeof(PipelineStateStream), &pipelineStateStream
    };
    ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));

    // auto fenceValue = commandQueue->ExecuteCommandList(commandList);
    // commandQueue->WaitForFenceValue(fenceValue);

    m_ContentLoaded = true;

    // Resize/Create the depth buffer.
    std::shared_ptr<Window> window = Application::Get().GetActiveWindow();

    Resize(window->GetClientWidth(), window->GetClientHeight());
    // ResizeDepthBuffer(window->GetClientWidth(), window->GetClientHeight());

    return true;
}

void CubeApp::ResizeDepthBuffer(int width, int height)
{
    if (m_ContentLoaded)
    {
        // Flush any GPU commands that might be referencing the depth buffer.
        Application::Get().Flush();

        width  = std::max(1, width);
        height = std::max(1, height);

        auto device = Application::Get().GetDevice();

        // Resize screen dependent resources.
        // Create a depth buffer.
        D3D12_CLEAR_VALUE optimizedClearValue = {};
        optimizedClearValue.Format            = DXGI_FORMAT_D32_FLOAT;
        optimizedClearValue.DepthStencil      = { 1.0f, 0 };

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &optimizedClearValue,
            IID_PPV_ARGS(&m_DepthBuffer)));

        // Update the depth-stencil view.
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
        dsv.Format                        = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv.Texture2D.MipSlice            = 0;
        dsv.Flags                         = D3D12_DSV_FLAG_NONE;

        device->CreateDepthStencilView(m_DepthBuffer.Get(), &dsv, m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
    }
}

// Transition a resource
void CubeApp::TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
                                 Microsoft::WRL::ComPtr<ID3D12Resource>             resource,
                                 D3D12_RESOURCE_STATES                              beforeState,
                                 D3D12_RESOURCE_STATES                              afterState)
{
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource.Get(),
        beforeState,
        afterState);

    commandList->ResourceBarrier(1, &barrier);
}

// Clear a render target.
void CubeApp::ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
                       D3D12_CPU_DESCRIPTOR_HANDLE                        rtv,
                       FLOAT*                                             clearColor)
{
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void CubeApp::ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
                         D3D12_CPU_DESCRIPTOR_HANDLE                        dsv,
                         FLOAT                                              depth)
{
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void CubeApp::UnloadContent()
{
    m_ContentLoaded = false;
}

void CubeApp::CleanUp()
{
    // remove the windows. But it is done by application already.
}

void CubeApp::onKeyDown(const SDL_KeyboardEvent* key)
{
    switch (key->keysym.sym)
    {
        case SDLK_q:
            Application::Get().Quit(0);
            break;
            // TODO: toggle fullscreen and vsync.
        default:
            break;
    }
}

void CubeApp::onKeyUp(const SDL_KeyboardEvent* key) { }

void CubeApp::Update(double delta, double total)
{
    static uint64_t frameCount = 0;
    static double   totalTime  = 0.0;

    // super::OnUpdate(e);

    totalTime += delta;
    frameCount++;

    if (totalTime > 1.0)
    {
        double fps = frameCount / totalTime;

        char buffer[512];
        sprintf_s(buffer, "FPS: %f\n", fps);
        OutputDebugStringA(buffer);

        frameCount = 0;
        totalTime  = 0.0;
    }

    // Update the model matrix.
    float           angle = static_cast<float>(total * 90.0);
    const glm::vec3 axis  = glm::vec3(0.0, 1.0, 1.0);
    m_ModelMatrix         = glm::rotate(glm::mat4(1.0), glm::radians(angle), axis);

    // Update the view matrix.
    const glm::vec3 eye    = glm::vec3(0, 0, -10);
    const glm::vec3 center = glm::vec3(0, 0, 0);
    const glm::vec3 up     = glm::vec3(0, 1, 0);
    m_ViewMatrix           = glm::lookAt(eye, center, up);

    // Update the projection matrix.
    auto  window       = Application::Get().GetActiveWindow();
    float aspectRatio  = window->GetClientWidth() / static_cast<float>(window->GetClientHeight());
    m_ProjectionMatrix = glm::perspective((float)m_FoV, aspectRatio, 0.1f, 100.0f);
}

void CubeApp::Render(double delta, double total)
{
    std::shared_ptr<Window> window = Application::Get().GetActiveWindow();

    auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto commandList  = commandQueue->GetCommandList();

    UINT currentBackBufferIndex = window->GetCurrentBackBufferIndex();
    auto backBuffer             = window->GetCurrentBackBuffer();
    auto rtv                    = window->GetCurrentRenderTargetView();
    auto dsv                    = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();

    // Clear the render targets.
    {
        TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

        ClearRTV(commandList, rtv, clearColor);
        ClearDepth(commandList, dsv);
    }

    commandList->SetPipelineState(m_PipelineState.Get());
    commandList->SetGraphicsRootSignature(m_RootSignature.Get());

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    commandList->IASetIndexBuffer(&m_IndexBufferView);

    commandList->RSSetViewports(1, &m_Viewport);
    commandList->RSSetScissorRects(1, &m_ScissorRect);

    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    // Update the MVP matrix
    glm::mat4 mvpMatrix = m_ProjectionMatrix * m_ViewMatrix * m_ModelMatrix;
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(glm::mat4) / 4, &mvpMatrix, 0);

    commandList->DrawIndexedInstanced(_countof(g_Indicies), 1, 0, 0, 0);

    // Present
    {
        TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        m_FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);

        currentBackBufferIndex = window->Present();

        commandQueue->WaitForFenceValue(m_FenceValues[currentBackBufferIndex]);
    }
}

void CubeApp::Resize(int w, int h)
{
    m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
    ResizeDepthBuffer(w, h);
}
