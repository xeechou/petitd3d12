#pragma once

#if !defined(GLM_FORCE_LEFT_HANDED)
#    define GLM_FORCE_LEFT_HANDED
#endif

#if !defined(GLM_FORCE_DEPTH_ZERO_TO_ONE)
#    define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/matrix.hpp>

#include "application.h"
#include "window.h"
#include "helpers.h"

#include <string>

class CubeApp : public Application
{
public:
    CubeApp();

protected:
    virtual bool Initialize() override;
    virtual bool LoadContent() override;
    virtual void UnloadContent() override;
    virtual void CleanUp() override;

    virtual void onKeyDown(const SDL_KeyboardEvent* key) override;
    virtual void onKeyUp(const SDL_KeyboardEvent* key) override;
    virtual void Update(double delta, double total) override;
    virtual void Render(double delta, double total) override;
    virtual void Resize(int width, int height) override;

private:
    void UpdateBufferResource(
        WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        ID3D12Resource**                        pDestinationResource,
        ID3D12Resource**                        pIntermediateResource,
        size_t                                  numElements,
        size_t                                  elementSize,
        const void*                             bufferData,
        D3D12_RESOURCE_FLAGS                    flags = D3D12_RESOURCE_FLAG_NONE);
    void LoadVertices();
    void ResizeDepthBuffer(int width, int height);

    void TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
                            Microsoft::WRL::ComPtr<ID3D12Resource>             resource,
                            D3D12_RESOURCE_STATES                              beforeState,
                            D3D12_RESOURCE_STATES                              afterState);

    // Clear a render target view.
    void ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
                  D3D12_CPU_DESCRIPTOR_HANDLE                        rtv,
                  FLOAT*                                             clearColor);

    // Clear the depth of a depth-stencil view.
    void ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
                    D3D12_CPU_DESCRIPTOR_HANDLE                        dsv,
                    FLOAT                                              depth = 1.0f);

private:
    uint64_t m_FenceValues[Window::BufferCount] = {};
    // Vertex buffer for the cube.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW               m_VertexBufferView;
    // Index buffer for the cube.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW                m_IndexBufferView;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_DepthBuffer;
    // Descriptor heap for depth buffer.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap;

    // Root signature
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;

    // Pipeline state object.
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT     m_ScissorRect;
    float          m_FoV;

    glm::mat4 m_ModelMatrix;
    glm::mat4 m_ViewMatrix;
    glm::mat4 m_ProjectionMatrix;

    bool m_ContentLoaded;
};
