#define GLM_ENABLE_EXPERIMENTAL
#include "mesh.h"

#include <d3d12.h>
#include <dxgiformat.h>
#include <wrl/client.h>

#include "glm/gtx/dual_quaternion.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/matrix.hpp"

#include "clock.h"
#include "commandqueue.h"

#include <stdint.h>
#include <tiny_obj_loader.h>
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>

using namespace Microsoft::WRL;

// Clamp a value between a min and max range.
template <typename T>
constexpr const T& clamp(const T& val, const T& min, const T& max)
{
    return val < min ? min : val > max ? max :
                                         val;
}

namespace std
{
template <>
struct hash<MeshApp::Vertex>
{
    size_t operator()(MeshApp::Vertex const& vertex) const
    {
        return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texcoord) << 1);
    }
};
} // namespace std

MeshApp::MeshApp() :
    m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)),
    m_FoV(glm::radians(45.0f)),
    m_ContentLoaded(false)
{
    m_LightDir = glm::normalize(glm::vec3(1.0, 1.0, -1.0));
}

bool MeshApp::Initialize()
{
    return true;
}

bool MeshApp::LoadContent()
{
    auto device = Application::Get().GetDevice();

    if (!LoadMesh())
        return false;
    // create the vertex and index buffer.

    if (!UploadVertices())
        return false;

    CreateRenderTargets();
    CreatePSOs();
    CreateUniforms();

    // Resize/Create the depth buffer.
    std::shared_ptr<Window> window = Application::Get().GetActiveWindow();
    m_ContentLoaded                = true;

    // rely on content loaded
    Resize(window->GetClientWidth(), window->GetClientHeight());

    return m_ContentLoaded;
}

bool MeshApp::LoadMesh()
{
    tinyobj::attrib_t attrib;

    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn;
    std::string                      err;

    HighResolutionClock clock;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "models/bmw.obj", "models", true))
        return false;
    clock.Tick();
    std::cout << "total load time:" << clock.GetTotalSeconds() << " seconds" << std::endl;

    if (!warn.empty())
        std::cout << "WARN: " << warn << std::endl;
    if (!err.empty())
        std::cout << "ERR: " << err << std::endl;

    int last_mid = -2;
    // Loop over shapes, we should only have one mesh here.
    // std::unordered_map<Vertex, uint32_t> unique_vertices;

    // load vertices
    for (size_t s = 0; s < shapes.size(); s++)
    {
        // Loop over faces(polygon), obj does not pack the vertex attributes
        // together, so we actually need to manually generate indices

        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            // the material processing is even worse, tinyobj loader uses
            //  per-face material_id
            //  shapes[s].mesh.
            size_t fv  = size_t(shapes[s].mesh.num_face_vertices[f]);
            int    mid = shapes[s].mesh.material_ids[f];
            // creating new submesh
            if (mid != last_mid)
            {
                last_mid = mid;

                SubMesh new_subMesh      = {};
                new_subMesh.material_id  = (uint32_t)mid;
                new_subMesh.index_offset = (uint32_t)m_Indices.size();
                new_subMesh.index_count  = 0;
                m_SubMeshes.push_back(new_subMesh);
            }

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++)
            {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                // Check if `normal_index` is zero or positive. negative = no normal data
                tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                Vertex vertex {};
                vertex.position = glm::vec3(vx, vy, vz);
                vertex.normal   = glm::normalize(glm::vec3(nx, ny, nz));
                vertex.texcoord = glm::vec3(-1.0f, -1.0, -1.0);

                // Check if `texcoord_index` is zero or positive. negative = no texcoord
                if (idx.texcoord_index >= 0)
                {
                    tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                    vertex.texcoord    = glm::vec3(glm::vec2(tx, ty), 0.0);
                }

                // // deduplication of vertices, didn't work out
                // if (unique_vertices.count(vertex) == 0)
                // {
                //     unique_vertices[vertex] = static_cast<uint32_t>(m_Vertices.size());
                // }
                m_Vertices.push_back(vertex);

                m_Indices.push_back((uint32_t)m_Indices.size());
                m_SubMeshes.back().index_count += 1;
            }
            index_offset += fv;
        }
    }

    // load materials, obj is phong model, There are extensions to have PBR but...
    for (size_t m = 0; m < materials.size(); m++)
    {
        Material new_material;

        new_material.diffuse  = glm::vec4(glm::make_vec3(materials[m].diffuse),
                                         materials[m].dissolve);
        new_material.specular = glm::vec4(glm::make_vec3(materials[m].specular),
                                          materials[m].shininess);
        m_Materials.push_back(new_material);
    }
}

void MeshApp::UpdateBufferResource(
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

bool MeshApp::UploadVertices()
{
    auto device       = Application::Get().GetDevice();
    auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
    auto commandList  = commandQueue->GetCommandList();

    // Upload vertex buffer data.
    ComPtr<ID3D12Resource> intermediateVertexBuffer;

    UpdateBufferResource(commandList, &m_VertexBuffer, &intermediateVertexBuffer, m_Vertices.size(), sizeof(Vertex), m_Vertices.data());
    m_VertexBuffer->SetName(L"Vertex Buffer");
    m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
    m_VertexBufferView.SizeInBytes    = sizeof(Vertex) * m_Vertices.size();
    m_VertexBufferView.StrideInBytes  = sizeof(Vertex);

    // Upload index buffer data.
    ComPtr<ID3D12Resource> intermediateIndexBuffer;
    UpdateBufferResource(commandList,
                         &m_IndexBuffer,
                         &intermediateIndexBuffer,
                         m_Indices.size(),
                         sizeof(uint32_t),
                         m_Indices.data());
    m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
    m_IndexBufferView.Format         = DXGI_FORMAT_R32_UINT;
    m_IndexBufferView.SizeInBytes    = sizeof(uint32_t) * m_Indices.size();
    m_IndexBuffer->SetName(L"Index Buffer");

    // upload to GPU
    auto fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);
    return true;
}

bool MeshApp::CreateRenderTargets()
{
    auto device = Application::Get().GetDevice();
    // Create the descriptor heap for the depth-stencil view.
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors             = 1;
    dsvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DSVHeap)));
    return true;
}

void MeshApp::CreateMeshRootSignature()
{
    // right now we just reuse the cube PSO
    auto device = Application::Get().GetDevice();

    // Create a root signature.
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion                    = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // Allow input layout and deny unnecessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = VertexPixelRootSignatureFlags();

    //  A 32-bit constant root parameter for vertices
    // And a material/lighting root parameters for pixels
    std::array<CD3DX12_ROOT_PARAMETER1, 2> rootParameters;
    // rootParameters[0].InitAsConstants(sizeof(Uniform) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[1].InitAsConstants(sizeof(Material) / sizeof(uint32_t), 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    // rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(rootParameters.size(), rootParameters.data(), 0, nullptr, rootSignatureFlags);

    // Serialize the root signature.
    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
                                                        featureData.HighestVersion,
                                                        &rootSignatureBlob,
                                                        &errorBlob));
    // Create the root signature.
    ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_MeshPipeline.root_signature)));
}

void MeshApp::CreateMeshPSO()
{
    // right now we just reuse the cube PSO
    auto device = Application::Get().GetDevice();
    // Create the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Load the vertex shader.
    ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"MeshVertex.cso", &vertexShaderBlob));

    // Load the pixel shader.
    ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"MeshPixel.cso", &pixelShaderBlob));

    PipelineStateStream pipelineStateStream;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets      = 1;
    rtvFormats.RTFormats[0]          = DXGI_FORMAT_R8G8B8A8_UNORM;

    pipelineStateStream.pRootSignature        = m_MeshPipeline.root_signature.Get();
    pipelineStateStream.InputLayout           = { inputLayout, _countof(inputLayout) };
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS                    = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.PS                    = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    pipelineStateStream.DSVFormat             = DXGI_FORMAT_D32_FLOAT;
    pipelineStateStream.RTVFormats            = rtvFormats;

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
        sizeof(PipelineStateStream), &pipelineStateStream
    };

    ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc,
                                              IID_PPV_ARGS(&m_MeshPipeline.pso)));
}

void MeshApp::CreatePSOs()
{
    // right now we just reuse the cube PSO
    auto device = Application::Get().GetDevice();

    CreateMeshRootSignature();
    CreateMeshPSO();
}

void MeshApp::CreateUniforms()
{
    auto device = Application::Get().GetDevice();

    size_t uniform_size = (sizeof(Uniform) + 255) & ~255;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uniform_size, D3D12_RESOURCE_FLAG_NONE),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_UniformBuffer)));
    m_UniformBuffer->SetName(L"Uniform Buffer");

    // // I don't need a view for root descriptor
    // D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc {};
    // cbvDesc.BufferLocation = m_MaterialBuffer->GetGPUVirtualAddress();
    // cbvDesc.SizeInBytes    = uniform_size; // need to be 256 byte aligned.

    // D3D12_CPU_DESCRIPTOR_HANDLE
}

void MeshApp::UnloadContent()
{
    m_ContentLoaded = false;
}

void MeshApp::CleanUp()
{
    // remove the windows. But it is done by application already.
}

void MeshApp::ResizeDepthBuffer(int width, int height)
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

void MeshApp::Update(double delta, double total)
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
    const glm::vec3 axis  = glm::vec3(0.0, 1.0, 0.0);
    m_ModelMatrix         = glm::translate(glm::vec3(0.0f, -2.0f, 2.0f)) * glm::rotate(glm::mat4(1.0), glm::radians(angle), axis) * glm::scale(glm::vec3(0.01));
    // m_ModelMatrix = glm::rotate(glm::mat4(1.0), glm::radians(angle), axis);
    // Update the view matrix.
    const glm::vec3 eye    = glm::vec3(0, 0, -10);
    const glm::vec3 center = glm::vec3(0, 0, 0);
    const glm::vec3 up     = glm::vec3(0, 1, 0);
    m_ViewMatrix           = glm::lookAt(eye, center, up);

    // Update the projection matrix.
    auto  window       = Application::Get().GetActiveWindow();
    float aspectRatio  = window->GetClientWidth() / static_cast<float>(window->GetClientHeight());
    m_ProjectionMatrix = glm::perspective((float)m_FoV, aspectRatio, 1.0f, 100.0f);
}

void MeshApp::Render(double delta, double total)
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

    RenderMesh(commandList, delta, total);

    // Present
    {
        TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        m_FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);

        currentBackBufferIndex = window->Present();

        commandQueue->WaitForFenceValue(m_FenceValues[currentBackBufferIndex]);
    }
}

void MeshApp::RenderMesh(ComPtr<ID3D12GraphicsCommandList2> commandList, double delta, double total)
{
    std::shared_ptr<Window> window = Application::Get().GetActiveWindow();
    auto                    rtv    = window->GetCurrentRenderTargetView();
    auto                    dsv    = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();

    commandList->SetPipelineState(m_MeshPipeline.pso.Get());
    commandList->SetGraphicsRootSignature(m_MeshPipeline.root_signature.Get());

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    commandList->IASetIndexBuffer(&m_IndexBufferView);

    commandList->RSSetViewports(1, &m_Viewport);
    commandList->RSSetScissorRects(1, &m_ScissorRect);

    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    // Update the MVP matrix
    Uniform uniform = {
        m_ProjectionMatrix * m_ViewMatrix * m_ModelMatrix,
        glm::mat4(glm::transpose(glm::inverse(glm::mat3(m_ModelMatrix)))),
    };
    D3D12_RANGE readRange { 0, sizeof(uniform) };
    Uniform*    mapped = nullptr;
    ThrowIfFailed(m_UniformBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mapped)));
    memcpy(mapped, &uniform, sizeof(uniform));
    m_UniformBuffer->Unmap(0, &readRange);

    commandList->SetGraphicsRootConstantBufferView(0, m_UniformBuffer->GetGPUVirtualAddress());

    for (auto submesh : m_SubMeshes)
    {
        Material material = submesh.material_id >= 0 ? m_Materials[submesh.material_id] :
                                                       Material::default_material();
        material.lightDir = glm::vec4(m_LightDir, 0.0);

        commandList->SetGraphicsRoot32BitConstants(1, sizeof(Material) / sizeof(uint32_t), &material, 0);
        commandList->DrawIndexedInstanced(submesh.index_count, 1, submesh.index_offset, 0, 0);
    }
}

void MeshApp::Resize(int w, int h)
{
    m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
    ResizeDepthBuffer(w, h);
}

void MeshApp::TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
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
void MeshApp::ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
                       D3D12_CPU_DESCRIPTOR_HANDLE                        rtv,
                       FLOAT*                                             clearColor)
{
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void MeshApp::ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
                         D3D12_CPU_DESCRIPTOR_HANDLE                        dsv,
                         FLOAT                                              depth)
{
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}
