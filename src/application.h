/**
 * The application class is used to create windows for our application.
 */
#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include "helpers.h"

#include "clock.h"

class Window;
class Game;
class CommandQueue;
union SDL_Event;
struct SDL_KeyboardEvent;

class Application
{
public:
    /**
     * Create the application singleton with the application instance handle.
     */
    template <typename T>
    static void Create()
    {
        static_assert(std::is_base_of_v<Application, T>,
                      "Not a valid Application type");
        gs_pSingelton = new T();
    }

    /**
     * Destroy the application instance and all windows created by this
     * application instance.
     */
    static void Destroy();
    /**
     * Get the application singleton.
     */
    static Application& Get();

    /**
     * Check to see if VSync-off is supported.
     */
    bool IsTearingSupported() const;

    /**
     * Create a new DirectX11 render window instance.
     * @param windowName The name of the window. This name will appear in the
     * title bar of the window. This name should be unique.
     * @param clientWidth The width (in pixels) of the window's client area.
     * @param clientHeight The height (in pixels) of the window's client area.
     * @param vSync Should the rendering be synchronized with the vertical
     * refresh rate of the screen.
     * @param windowed If true, the window will be created in windowed mode. If
     * false, the window will be created full-screen.
     * @returns The created window instance. If an error occurred while creating
     * the window an invalid window instance is returned. If a window with the
     * given name already exists, that window will be returned.
     */
    std::shared_ptr<Window> CreateRenderWindow(const std::string& windowName,
                                               int                clientWidth,
                                               int                clientHeight,
                                               bool               vSync = true);

    /**
     * Destroy a window given the window name.
     */
    void DestroyWindow(const std::string& windowName);
    /**
     * Find a window by the window name.
     */
    std::shared_ptr<Window> GetWindowByName(const std::string& windowName);

    std::shared_ptr<Window> GetActiveWindow();

    /**
     * Run the application loop and message pump.
     * @return The error code if an error occurred.
     */
    int Run();

    /**
     * Request to quit the application and close all windows.
     * @param exitCode The error code to return to the invoking process.
     */
    void Quit(int exitCode = 0);

    /**
     * Get the Direct3D 12 device
     */
    Microsoft::WRL::ComPtr<ID3D12Device2> GetDevice() const;
    /**
     * Get a command queue. Valid types are:
     * - D3D12_COMMAND_LIST_TYPE_DIRECT : Can be used for draw, dispatch, or
     * copy commands.
     * - D3D12_COMMAND_LIST_TYPE_COMPUTE: Can be used for dispatch or copy
     * commands.
     * - D3D12_COMMAND_LIST_TYPE_COPY   : Can be used for copy commands.
     */
    std::shared_ptr<CommandQueue> GetCommandQueue(
        D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

    // Flush all command queues.
    void Flush();

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
        CreateDescriptorHeap(UINT                       numDescriptors,
                             D3D12_DESCRIPTOR_HEAP_TYPE type);
    UINT
        GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

protected:
    // Create an application instance.
    Application();
    // Destroy the application instance and all windows associated with this
    // application.
    virtual ~Application();

    Microsoft::WRL::ComPtr<IDXGIAdapter4> GetAdapter(bool bUseWarp);
    Microsoft::WRL::ComPtr<ID3D12Device2>
         CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);
    bool CheckTearingSupport();

protected:
    virtual bool Initialize()    = 0;
    virtual bool LoadContent()   = 0;
    virtual void UnloadContent() = 0;
    virtual void CleanUp()       = 0;

protected:
    friend class Window;

    // event handlers
    virtual void onKeyDown(const SDL_KeyboardEvent* event) { }
    virtual void onKeyUp(const SDL_KeyboardEvent* event) { }
    virtual void Update(double delta, double total) { }
    virtual void Render(double delta, double total) { }
    virtual void Resize(int width, int height) { }

private:
    Application(const Application& copy)
        = delete;
    Application& operator=(const Application& other) = delete;

    static inline Application* gs_pSingelton = nullptr;

    Microsoft::WRL::ComPtr<IDXGIAdapter4> m_dxgiAdapter;
    Microsoft::WRL::ComPtr<ID3D12Device2> m_d3d12Device;

    std::shared_ptr<CommandQueue> m_DirectCommandQueue;
    std::shared_ptr<CommandQueue> m_ComputeCommandQueue;
    std::shared_ptr<CommandQueue> m_CopyCommandQueue;

    HighResolutionClock m_UpdateClock;

    bool m_TearingSupported;
    bool m_running = true;
};
