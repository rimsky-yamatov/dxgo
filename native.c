#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "native.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#define FRAME_COUNT 2

static HWND g_hwnd = NULL;
static int g_width = 800;
static int g_height = 600;

static ID3D12Device* g_device = NULL;
static ID3D12CommandQueue* g_command_queue = NULL;
static IDXGISwapChain3* g_swap_chain = NULL;
static ID3D12DescriptorHeap* g_rtv_heap = NULL;

static ID3D12Resource* g_render_targets[FRAME_COUNT];

static ID3D12CommandAllocator* g_command_allocator = NULL;
static ID3D12GraphicsCommandList* g_command_list = NULL;

static ID3D12Fence* g_fence = NULL;
static HANDLE g_fence_event = NULL;
static UINT64 g_fence_value = 0;

static UINT g_rtv_descriptor_size = 0;
static UINT g_frame_index = 0;

static WNDCLASSA g_window_class;

static LRESULT CALLBACK dxgo_window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam
) {
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static int create_window(const char* title, int width, int height) {
    HINSTANCE instance = GetModuleHandleA(NULL);

    memset(&g_window_class, 0, sizeof(g_window_class));

    g_window_class.lpfnWndProc = dxgo_window_proc;
    g_window_class.hInstance = instance;
    g_window_class.lpszClassName = "DXGOWindowClass";
    g_window_class.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassA(&g_window_class)) {
        return 0;
    }

    RECT rect;

    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;

    AdjustWindowRect(
        &rect,
        WS_OVERLAPPEDWINDOW,
        FALSE
    );

    g_hwnd = CreateWindowExA(
        0,
        "DXGOWindowClass",
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        instance,
        NULL
    );

    if (!g_hwnd) {
        return 0;
    }

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    return 1;
}

static int create_device(void) {
    IDXGIFactory4* factory = NULL;

    HRESULT hr = CreateDXGIFactory1(
        &IID_IDXGIFactory4,
        (void**)&factory
    );

    if (FAILED(hr)) {
        return 0;
    }

    hr = D3D12CreateDevice(
        NULL,
        D3D_FEATURE_LEVEL_11_0,
        &IID_ID3D12Device,
        (void**)&g_device
    );

    if (FAILED(hr)) {
        factory->lpVtbl->Release(factory);
        return 0;
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc;

    memset(&queue_desc, 0, sizeof(queue_desc));

    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    hr = g_device->lpVtbl->CreateCommandQueue(
        g_device,
        &queue_desc,
        &IID_ID3D12CommandQueue,
        (void**)&g_command_queue
    );

    if (FAILED(hr)) {
        factory->lpVtbl->Release(factory);
        return 0;
    }

    DXGI_SWAP_CHAIN_DESC1 swap_desc;

    memset(&swap_desc, 0, sizeof(swap_desc));

    swap_desc.Width = g_width;
    swap_desc.Height = g_height;
    swap_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.SampleDesc.Count = 1;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount = FRAME_COUNT;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain1* swap_chain1 = NULL;

    hr = factory->lpVtbl->CreateSwapChainForHwnd(
        factory,
        (IUnknown*)g_command_queue,
        g_hwnd,
        &swap_desc,
        NULL,
        NULL,
        &swap_chain1
    );

    if (FAILED(hr)) {
        factory->lpVtbl->Release(factory);
        return 0;
    }

    hr = swap_chain1->lpVtbl->QueryInterface(
        swap_chain1,
        &IID_IDXGISwapChain3,
        (void**)&g_swap_chain
    );

    swap_chain1->lpVtbl->Release(swap_chain1);
    factory->lpVtbl->Release(factory);

    if (FAILED(hr)) {
        return 0;
    }

    g_frame_index =
        g_swap_chain->lpVtbl->GetCurrentBackBufferIndex(
            g_swap_chain
        );

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc;

    memset(&heap_desc, 0, sizeof(heap_desc));

    heap_desc.NumDescriptors = FRAME_COUNT;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    hr = g_device->lpVtbl->CreateDescriptorHeap(
        g_device,
        &heap_desc,
        &IID_ID3D12DescriptorHeap,
        (void**)&g_rtv_heap
    );

    if (FAILED(hr)) {
        return 0;
    }

    g_rtv_descriptor_size =
        g_device->lpVtbl->GetDescriptorHandleIncrementSize(
            g_device,
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV
        );

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        g_rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(
            g_rtv_heap
        );

    for (UINT i = 0; i < FRAME_COUNT; i++) {
        hr = g_swap_chain->lpVtbl->GetBuffer(
            g_swap_chain,
            i,
            &IID_ID3D12Resource,
            (void**)&g_render_targets[i]
        );

        if (FAILED(hr)) {
            return 0;
        }

        g_device->lpVtbl->CreateRenderTargetView(
            g_device,
            g_render_targets[i],
            NULL,
            handle
        );

        handle.ptr += g_rtv_descriptor_size;
    }

    hr = g_device->lpVtbl->CreateCommandAllocator(
        g_device,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        &IID_ID3D12CommandAllocator,
        (void**)&g_command_allocator
    );

    if (FAILED(hr)) {
        return 0;
    }

    hr = g_device->lpVtbl->CreateCommandList(
        g_device,
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_command_allocator,
        NULL,
        &IID_ID3D12GraphicsCommandList,
        (void**)&g_command_list
    );

    if (FAILED(hr)) {
        return 0;
    }

    g_command_list->lpVtbl->Close(g_command_list);

    hr = g_device->lpVtbl->CreateFence(
        g_device,
        0,
        D3D12_FENCE_FLAG_NONE,
        &IID_ID3D12Fence,
        (void**)&g_fence
    );

    if (FAILED(hr)) {
        return 0;
    }

    g_fence_event = CreateEventA(
        NULL,
        FALSE,
        FALSE,
        NULL
    );

    if (!g_fence_event) {
        return 0;
    }

    return 1;
}

static void wait_gpu(void) {
    g_fence_value++;

    g_command_queue->lpVtbl->Signal(
        g_command_queue,
        g_fence,
        g_fence_value
    );

    if (
        g_fence->lpVtbl->GetCompletedValue(g_fence)
        < g_fence_value
    ) {
        g_fence->lpVtbl->SetEventOnCompletion(
            g_fence,
            g_fence_value,
            g_fence_event
        );

        WaitForSingleObject(
            g_fence_event,
            INFINITE
        );
    }
}

int dxgo_native_init(
    const char* title,
    int width,
    int height
) {
    g_width = width;
    g_height = height;

    if (!create_window(title, width, height)) {
        return 0;
    }

    if (!create_device()) {
        return 0;
    }

    return 1;
}

int dxgo_native_open(void) {
    MSG message;

    while (PeekMessageA(
        &message,
        NULL,
        0,
        0,
        PM_REMOVE
    )) {
        if (message.message == WM_QUIT) {
            return 0;
        }

        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    return IsWindow(g_hwnd) != 0;
}

void dxgo_native_clear(
    float r,
    float g,
    float b,
    float a
) {
    g_command_allocator->lpVtbl->Reset(
        g_command_allocator
    );

    g_command_list->lpVtbl->Reset(
        g_command_list,
        g_command_allocator,
        NULL
    );

    D3D12_RESOURCE_BARRIER barrier;

    memset(&barrier, 0, sizeof(barrier));

    barrier.Type =
        D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

    barrier.Transition.pResource =
        g_render_targets[g_frame_index];

    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_PRESENT;

    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_RENDER_TARGET;

    g_command_list->lpVtbl->ResourceBarrier(
        g_command_list,
        1,
        &barrier
    );

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        g_rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(
            g_rtv_heap
        );

    rtv.ptr +=
        g_frame_index * g_rtv_descriptor_size;

    FLOAT clear_color[4] = {
        r,
        g,
        b,
        a
    };

    g_command_list->lpVtbl->ClearRenderTargetView(
        g_command_list,
        rtv,
        clear_color,
        0,
        NULL
    );

    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_RENDER_TARGET;

    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_PRESENT;

    g_command_list->lpVtbl->ResourceBarrier(
        g_command_list,
        1,
        &barrier
    );

    g_command_list->lpVtbl->Close(
        g_command_list
    );
}

void dxgo_native_update(void) {
    ID3D12CommandList* lists[] = {
        (ID3D12CommandList*)g_command_list
    };

    g_command_queue->lpVtbl->ExecuteCommandLists(
        g_command_queue,
        1,
        lists
    );

    g_swap_chain->lpVtbl->Present(
        g_swap_chain,
        1,
        0
    );

    wait_gpu();

    g_frame_index =
        g_swap_chain->lpVtbl->GetCurrentBackBufferIndex(
            g_swap_chain
        );
}

void dxgo_native_close(void) {
    if (g_command_queue && g_fence) {
        wait_gpu();
    }

    if (g_fence_event) {
        CloseHandle(g_fence_event);
        g_fence_event = NULL;
    }

    if (g_fence) {
        g_fence->lpVtbl->Release(g_fence);
        g_fence = NULL;
    }

    if (g_command_list) {
        g_command_list->lpVtbl->Release(g_command_list);
        g_command_list = NULL;
    }

    if (g_command_allocator) {
        g_command_allocator->lpVtbl->Release(
            g_command_allocator
        );
        g_command_allocator = NULL;
    }

    for (int i = 0; i < FRAME_COUNT; i++) {
        if (g_render_targets[i]) {
            g_render_targets[i]->lpVtbl->Release(
                g_render_targets[i]
            );
            g_render_targets[i] = NULL;
        }
    }

    if (g_rtv_heap) {
        g_rtv_heap->lpVtbl->Release(g_rtv_heap);
        g_rtv_heap = NULL;
    }

    if (g_swap_chain) {
        g_swap_chain->lpVtbl->Release(g_swap_chain);
        g_swap_chain = NULL;
    }

    if (g_command_queue) {
        g_command_queue->lpVtbl->Release(
            g_command_queue
        );
        g_command_queue = NULL;
    }

    if (g_device) {
        g_device->lpVtbl->Release(g_device);
        g_device = NULL;
    }

    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
}