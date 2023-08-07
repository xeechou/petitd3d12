#include "application.h"
#include "SDL_events.h"
#include "commandqueue.h"
#include "helpers.h"
#include "window.h"
#include "clock.h"
#include <SDL.h>
#include <assert.h>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>

// #include <CommandQueue.h>
// #include <Game.h>
// #include <Window.h>
using namespace Microsoft::WRL;

constexpr wchar_t WINDOW_CLASS_NAME[] = L"DX12RenderWindowClass";

using WindowPtr     = std::shared_ptr<Window>;
using WindowMap     = std::map<uint32_t, WindowPtr>;
using WindowNameMap = std::map<std::string, uint32_t>;

static WindowMap     gs_Windows;
static WindowNameMap gs_WindowByName;

// A wrapper struct to allow shared pointers for the window class.
struct MakeWindow : public Window
{
    MakeWindow(const std::string& windowName, int clientWidth, int clientHeight, bool vSync) :
        Window(windowName, clientWidth, clientHeight, vSync)
    {
    }
};

Application::Application() :
    m_TearingSupported(false)
{
    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window
    // to achieve 100% scaling while still allowing non-client window content to
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#if defined(_DEBUG)
    // Always enable the debug layer before doing anything DX12 related
    // so all possible errors generated while creating DX12 objects
    // are caught by the debug layer.
    WRL::ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif

    m_dxgiAdapter = GetAdapter(false);
    if (m_dxgiAdapter)
    {
        m_d3d12Device = CreateDevice(m_dxgiAdapter);
    }
    if (m_d3d12Device)
    {
        m_DirectCommandQueue = std::make_shared<CommandQueue>(
            m_d3d12Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        m_ComputeCommandQueue = std::make_shared<CommandQueue>(
            m_d3d12Device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
        m_CopyCommandQueue = std::make_shared<CommandQueue>(
            m_d3d12Device, D3D12_COMMAND_LIST_TYPE_COPY);

        m_TearingSupported = CheckTearingSupport();
    }
}

Application& Application::Get()
{
    assert(gs_pSingelton);
    return *gs_pSingelton;
}

void Application::Destroy()
{
    if (gs_pSingelton)
    {
        assert(gs_Windows.empty() && gs_WindowByName.empty() && "All windows should be destroyed before destroying the "
                                                                "application instance.");

        delete gs_pSingelton;
        gs_pSingelton = nullptr;
    }
}

Application::~Application() { Flush(); }

Microsoft::WRL::ComPtr<IDXGIAdapter4> Application::GetAdapter(bool bUseWarp)
{
    ComPtr<IDXGIFactory4> dxgiFactory;
    UINT                  createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(
        CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    if (bUseWarp)
    {
        ThrowIfFailed(
            dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
    }
    else
    {
        SIZE_T maxDedicatedVideoMemory = 0;
        for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND;
             ++i)
        {
            DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
            dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

            // Check to see if the adapter can create a D3D12 device without
            // actually creating it. The adapter with the largest dedicated
            // video memory is favored.
            if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 && SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) && dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
            {
                maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
            }
        }
    }

    return dxgiAdapter4;
}

Microsoft::WRL::ComPtr<ID3D12Device2>
    Application::CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter)
{
    ComPtr<ID3D12Device2> d3d12Device2;
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));
    //    NAME_D3D12_OBJECT(d3d12Device2);

    // Enable debug messages in debug mode.
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // Suppress whole categories of messages
        // D3D12_MESSAGE_CATEGORY Categories[] = {};

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] = { D3D12_MESSAGE_SEVERITY_INFO };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, // I'm
                                                                          // really
                                                                          // not
                                                                          // sure
                                                                          // how
                                                                          // to
                                                                          // avoid
                                                                          // this
                                                                          // message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                       // This warning occurs when
                                                                          // using capture frame while
                                                                          // graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                     // This warning occurs
                                                                          // when using capture
                                                                          // frame while graphics
                                                                          // debugging.
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        // NewFilter.DenyList.NumCategories = _countof(Categories);
        // NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs        = _countof(DenyIds);
        NewFilter.DenyList.pIDList       = DenyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif

    return d3d12Device2;
}

bool Application::CheckTearingSupport()
{
    BOOL allowTearing = FALSE;

    // Rather than create the DXGI 1.5 factory interface directly, we create the
    // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the
    // graphics debugging tools which will not support the 1.5 factory interface
    // until a future update.
    ComPtr<IDXGIFactory4> factory4;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
    {
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory4.As(&factory5)))
        {
            factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                          &allowTearing,
                                          sizeof(allowTearing));
        }
    }

    return allowTearing == TRUE;
}

bool Application::IsTearingSupported() const { return m_TearingSupported; }

std::shared_ptr<Window>
    Application::CreateRenderWindow(const std::string& windowName,
                                    int                clientWidth,
                                    int                clientHeight,
                                    bool               vSync)
{
    // First check if a window with the given name already exists.
    WindowNameMap::iterator windowIter = gs_WindowByName.find(windowName);
    if (windowIter != gs_WindowByName.end())
    {
        return gs_Windows[windowIter->second];
    }

    WindowPtr pWindow = std::make_shared<MakeWindow>(windowName, clientWidth, clientHeight, vSync);

    gs_Windows.insert(WindowMap::value_type(pWindow->GetWindowId(), pWindow));
    gs_WindowByName.insert(WindowNameMap::value_type(windowName,
                                                     pWindow->GetWindowId()));

    return pWindow;
}

void Application::DestroyWindow(const std::string& windowName)
{
    int                     window_id = -1;
    WindowNameMap::iterator iter      = gs_WindowByName.find(windowName);
    if (iter != gs_WindowByName.end())
    {
        window_id = iter->second;
        gs_WindowByName.erase(iter);
    }

    if (window_id >= 0)
    {
        gs_Windows.erase(window_id);
    }
}

std::shared_ptr<Window>
    Application::GetWindowByName(const std::string& windowName)
{
    std::shared_ptr<Window> window;
    WindowNameMap::iterator iter = gs_WindowByName.find(windowName);
    if (iter != gs_WindowByName.end())
    {
        window = gs_Windows[iter->second];
    }

    return window;
}

std::shared_ptr<Window>
    Application::GetActiveWindow()
{
    if (!gs_Windows.empty())
        return gs_Windows.begin()->second;
    else
        return nullptr;
}

int Application::Run()
{
    if (!Initialize()) return 1;
    if (!LoadContent()) return 2;

    while (m_running = true)
    {
        m_UpdateClock.Tick();
        SDL_Event event;
        if (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                break;
            else if (event.type == SDL_WINDOWEVENT)
            {
                auto window = gs_Windows.find(event.window.windowID);
                if (window != gs_Windows.end())
                {
                    window->second->OnWindowEvent(&event.window);
                }
            }
            else if (event.type == SDL_KEYUP)
            {
                onKeyUp(&event.key);
            }
            else if (event.type == SDL_KEYDOWN)
            {
                onKeyDown(&event.key);
            }
            else
            {
                continue;
            }
        }
        Update(m_UpdateClock.GetDeltaSeconds(), m_UpdateClock.GetTotalSeconds());
        Render(m_UpdateClock.GetDeltaSeconds(), m_UpdateClock.GetTotalSeconds());
    }
    // Flush any commands in the commands queues before quiting.
    Flush();

    UnloadContent();
    CleanUp();

    // destroy all the windows
    gs_Windows.clear();
    gs_WindowByName.clear();

    return 0;
}

void Application::Quit(int exitCode) { m_running = false; }

Microsoft::WRL::ComPtr<ID3D12Device2> Application::GetDevice() const
{
    return m_d3d12Device;
}

std::shared_ptr<CommandQueue>
    Application::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) const
{
    std::shared_ptr<CommandQueue> commandQueue;
    switch (type)
    {
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
            commandQueue = m_DirectCommandQueue;
            break;
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            commandQueue = m_ComputeCommandQueue;
            break;
        case D3D12_COMMAND_LIST_TYPE_COPY:
            commandQueue = m_CopyCommandQueue;
            break;
        default:
            assert(false && "Invalid command queue type.");
    }

    return commandQueue;
}

void Application::Flush()
{
    m_DirectCommandQueue->Flush();
    m_ComputeCommandQueue->Flush();
    m_CopyCommandQueue->Flush();
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
    Application::CreateDescriptorHeap(UINT                       numDescriptors,
                                      D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type                       = type;
    desc.NumDescriptors             = numDescriptors;
    desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask                   = 0;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    ThrowIfFailed(m_d3d12Device->CreateDescriptorHeap(
        &desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

UINT Application::GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
    return m_d3d12Device->GetDescriptorHandleIncrementSize(type);
}

// Remove a window from our window lists.
static void RemoveWindow(uint32_t id)
{
    WindowMap::iterator windowIter = gs_Windows.find(id);
    if (windowIter != gs_Windows.end())
    {
        WindowPtr pWindow = windowIter->second;
        gs_WindowByName.erase(pWindow->GetWindowName());
        gs_Windows.erase(windowIter);
    }
}
