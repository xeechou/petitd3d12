#pragma once

#include "application.h"
#include "window.h"
#include <d3d12.h>
#include <stdint.h>
#include <wrl/client.h>

#if !defined(GLM_FORCE_LEFT_HANDED)
#    define GLM_FORCE_LEFT_HANDED
#endif

#if !defined(GLM_FORCE_DEPTH_ZERO_TO_ONE)
#    define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/matrix.hpp>

class MeshApp : public Application
{
public:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 texcoord;

        bool operator==(const Vertex& other) const
        {
            return position == other.position && normal == other.normal && texcoord == other.texcoord;
        }
    };
    struct SubMesh
    {
        uint32_t index_offset;
        uint32_t index_count;
        uint32_t material_id;
    };

    struct Material
    {
        glm::vec3 diffuse   = glm::vec3(1.0);
        glm::vec3 specular  = glm::vec3(0.0);
        float     shininess = 0.0;
        // glm::
    };

    struct Uniform
    {
        glm::mat4 MVP;
        glm::mat4 normal;
    };

public:
    MeshApp();

protected: // overrides
    virtual bool Initialize() override;
    virtual bool LoadContent() override;
    virtual void UnloadContent() override;
    virtual void CleanUp() override;
    virtual void Update(double delta, double total) override;
    virtual void Render(double delta, double total) override;
    virtual void Resize(int w, int h) override;

protected:
    bool LoadMesh();
    bool UploadVertices();
    bool CreateRenderTargets();
    void CreatePSOs();

    void CreateMeshPSO();
    void CreateMeshRootSignature();

    void UpdateBufferResource(
        WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        ID3D12Resource**                        pDestinationResource,
        ID3D12Resource**                        pIntermediateResource,
        size_t                                  numElements,
        size_t                                  elementSize,
        const void*                             bufferData,
        D3D12_RESOURCE_FLAGS                    flags = D3D12_RESOURCE_FLAG_NONE);
    // Transitioning resource
    void TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> cmdlist,
                            Microsoft::WRL::ComPtr<ID3D12Resource>             resource,
                            D3D12_RESOURCE_STATES                              before,
                            D3D12_RESOURCE_STATES                              after);

    // Clear a render target view.
    void ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
                  D3D12_CPU_DESCRIPTOR_HANDLE                        rtv,
                  FLOAT*                                             clearColor);

    // Clear the depth of a depth-stencil view.
    void ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
                    D3D12_CPU_DESCRIPTOR_HANDLE                        dsv,
                    FLOAT                                              depth = 1.0f);

    void ResizeDepthBuffer(int width, int height);

    void RenderMesh(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> cmd_list,
                    double                                             delta,
                    double                                             total);

private: // parameters
    bool           m_ContentLoaded = false;
    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT     m_ScissorRect;
    float          m_FoV;
    // push constants
    glm::mat4 m_ModelMatrix;
    glm::mat4 m_ViewMatrix;
    glm::mat4 m_ProjectionMatrix;

private: // CPU Data.
    std::vector<Vertex>   m_Vertices;
    std::vector<uint32_t> m_Indices;
    std::vector<SubMesh>  m_SubMeshes;
    std::vector<Material> m_Materials;

private: // GPU Data
    uint64_t m_FenceValues[Window::BufferCount] = {};
    // Vertex buffer for the mesh
    Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW               m_VertexBufferView;
    // Index buffer for the mesh
    Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW                m_IndexBufferView;

    /// render targets
    Microsoft::WRL::ComPtr<ID3D12Resource> m_DepthBuffer;
    // Descriptor heap for depth buffer.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap;

    // Pipelines
    struct
    {
        // Root signature
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
        // Pipeline state object.
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    } m_MeshPipeline;
};
