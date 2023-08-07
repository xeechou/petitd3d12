#include "window.h"
#include "SDL.h"
#include "SDL_syswm.h"
#include "SDL_video.h"
#include "application.h"
#include "commandqueue.h"
#include "d3dx12.h"
#include "helpers.h"
#include <algorithm>
#include <utility>

using namespace Microsoft::WRL;

Window::Window(const std::string& windowName, int clientWidth, int clientHeight, bool vSync) :
    m_WindowName(windowName), m_VSync(vSync), m_FrameCounter(0)
{
    Application& app = Application::Get();

    m_window = SDL_CreateWindow(windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, clientWidth, clientHeight, SDL_WINDOW_RESIZABLE);

    m_IsTearingSupported = app.IsTearingSupported();

    m_dxgiSwapChain          = CreateSwapChain();
    m_d3d12RTVDescriptorHeap = app.CreateDescriptorHeap(BufferCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_RTVDescriptorSize      = app.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    UpdateRenderTargetViews();
}

Window::~Window()
{
    // Window should be destroyed with Application::DestroyWindow before
    // the window goes out of scope.
    SDL_DestroyWindow(m_window);

    // assert(!m_hWnd && "Use Application::DestroyWindow before destruction.");
}

// I don'
HWND Window::GetWindowHandle() const
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(m_window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;
    return hwnd;
}

uint32_t Window::GetWindowId() const { return SDL_GetWindowID(m_window); }

const std::string& Window::GetWindowName() const { return m_WindowName; }

void Window::Show() { SDL_ShowWindow(m_window); }

/**
 * Hide the window.
 */
void Window::Hide() { SDL_HideWindow(m_window); }

static inline std::pair<int, int> getWindowSize(SDL_Window* win)
{
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    return std::make_pair(w, h);
}

int Window::GetClientWidth() const { return getWindowSize(m_window).first; }

int Window::GetClientHeight() const { return getWindowSize(m_window).second; }

bool Window::IsVSync() const { return m_VSync; }

void Window::SetVSync(bool vSync) { m_VSync = vSync; }

void Window::ToggleVSync() { SetVSync(!m_VSync); }

void Window::OnWindowEvent(const SDL_WindowEvent* event)
{
    if (event->event == SDL_WINDOWEVENT_RESIZED)
    {
        OnResize(event->data1, event->data2);
    }
}

void Window::OnResize(int w, int h)
{
    auto window_size = getWindowSize(m_window);
    // Update the client size.
    if (window_size.first != w || window_size.second != h)
    {
        window_size.first  = std::max(1, w);
        window_size.second = std::max(1, h);

        Application::Get().Flush();

        for (int i = 0; i < BufferCount; ++i)
        {
            m_d3d12BackBuffers[i].Reset();
        }

        auto                 size          = getWindowSize(m_window);
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        ThrowIfFailed(m_dxgiSwapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(m_dxgiSwapChain->ResizeBuffers(
            BufferCount, size.first, size.second, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

        m_CurrentBackBufferIndex = m_dxgiSwapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews();
    }

    Application::Get().Resize(w, h);
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
    Application& app = Application::Get();

    ComPtr<IDXGISwapChain4> dxgiSwapChain4;
    ComPtr<IDXGIFactory4>   dxgiFactory4;
    UINT                    createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(
        CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width                 = GetClientWidth();
    swapChainDesc.Height                = GetClientHeight();
    swapChainDesc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo                = FALSE;
    swapChainDesc.SampleDesc            = { 1, 0 };
    swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount           = BufferCount;
    swapChainDesc.Scaling               = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode             = DXGI_ALPHA_MODE_UNSPECIFIED;
    // It is recommended to always allow tearing if tearing support is
    // available.
    swapChainDesc.Flags               = m_IsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    ID3D12CommandQueue* pCommandQueue = app.GetCommandQueue()->GetD3D12CommandQueue().Get();

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
        pCommandQueue, GetWindowHandle(), &swapChainDesc, nullptr, nullptr, &swapChain1));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually.
    ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(GetWindowHandle(),
                                                      DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

    m_CurrentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();

    return dxgiSwapChain4;
}

// Update the render target views for the swapchain back buffers.
void Window::UpdateRenderTargetViews()
{
    auto device = Application::Get().GetDevice();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        m_d3d12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < BufferCount; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(m_dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

        m_d3d12BackBuffers[i] = backBuffer;

        rtvHandle.Offset(m_RTVDescriptorSize);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentRenderTargetView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_d3d12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        m_CurrentBackBufferIndex,
        m_RTVDescriptorSize);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Window::GetCurrentBackBuffer() const
{
    return m_d3d12BackBuffers[m_CurrentBackBufferIndex];
}

UINT Window::GetCurrentBackBufferIndex() const
{
    return m_CurrentBackBufferIndex;
}

UINT Window::Present()
{
    UINT syncInterval = m_VSync ? 1 : 0;
    UINT presentFlags = m_IsTearingSupported && !m_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(m_dxgiSwapChain->Present(syncInterval, presentFlags));
    m_CurrentBackBufferIndex = m_dxgiSwapChain->GetCurrentBackBufferIndex();

    return m_CurrentBackBufferIndex;
}
