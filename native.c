#define WIN32_LEAN_AND_MEAN
#define INITGUID

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "native.h"

#define FRAME_COUNT 2
#define MAX_VERTICES 65536
#define CIRCLE_SEGMENTS 64

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

static ID3D12RootSignature* g_root_signature = NULL;
static ID3D12PipelineState* g_pipeline_state = NULL;
static ID3D12Resource* g_vertex_buffer = NULL;
static D3D12_VERTEX_BUFFER_VIEW g_vertex_buffer_view;

typedef struct {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
} DXGOVertex;

static DXGOVertex g_vertices[MAX_VERTICES];
static UINT g_vertex_count = 0;

static WNDCLASSA g_window_class;

static const char* g_vertex_shader =
"struct VSInput {"
"float2 pos : POSITION;"
"float4 color : COLOR;"
"};"
"struct VSOutput {"
"float4 pos : SV_POSITION;"
"float4 color : COLOR;"
"};"
"cbuffer Constants : register(b0) {"
"float width;"
"float height;"
"};"
"VSOutput main(VSInput input) {"
"VSOutput output;"
"float x = input.pos.x / width * 2.0 - 1.0;"
"float y = 1.0 - input.pos.y / height * 2.0;"
"output.pos = float4(x, y, 0.0, 1.0);"
"output.color = input.color;"
"return output;"
"}";

static const char* g_pixel_shader =
"struct PSInput {"
"float4 pos : SV_POSITION;"
"float4 color : COLOR;"
"};"
"float4 main(PSInput input) : SV_TARGET {"
"return input.color;"
"}";

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

static int create_window(
    const char* title,
    int width,
    int height
) {
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

static int create_shaders(void) {
    ID3DBlob* vs_blob = NULL;
    ID3DBlob* ps_blob = NULL;
    ID3DBlob* error_blob = NULL;

    HRESULT hr = D3DCompile(
        g_vertex_shader,
        strlen(g_vertex_shader),
        NULL,
        NULL,
        NULL,
        "main",
        "vs_5_0",
        0,
        0,
        &vs_blob,
        &error_blob
    );

    if (FAILED(hr)) {
        if (error_blob) {
            error_blob->lpVtbl->Release(error_blob);
        }
        return 0;
    }

    hr = D3DCompile(
        g_pixel_shader,
        strlen(g_pixel_shader),
        NULL,
        NULL,
        NULL,
        "main",
        "ps_5_0",
        0,
        0,
        &ps_blob,
        &error_blob
    );

    if (FAILED(hr)) {
        vs_blob->lpVtbl->Release(vs_blob);

        if (error_blob) {
            error_blob->lpVtbl->Release(error_blob);
        }

        return 0;
    }

    D3D12_ROOT_PARAMETER root_parameter;

    memset(&root_parameter, 0, sizeof(root_parameter));

    root_parameter.ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;

    root_parameter.Constants.ShaderRegister = 0;
    root_parameter.Constants.RegisterSpace = 0;
    root_parameter.Constants.Num32BitValues = 2;
    root_parameter.ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC root_desc;

    memset(&root_desc, 0, sizeof(root_desc));

    root_desc.NumParameters = 1;
    root_desc.pParameters = &root_parameter;
    root_desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob* signature_blob = NULL;

    hr = D3D12SerializeRootSignature(
        &root_desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature_blob,
        &error_blob
    );

    if (FAILED(hr)) {
        vs_blob->lpVtbl->Release(vs_blob);
        ps_blob->lpVtbl->Release(ps_blob);

        if (error_blob) {
            error_blob->lpVtbl->Release(error_blob);
        }

        return 0;
    }

    hr = g_device->lpVtbl->CreateRootSignature(
        g_device,
        0,
        signature_blob->lpVtbl->GetBufferPointer(signature_blob),
        signature_blob->lpVtbl->GetBufferSize(signature_blob),
        &IID_ID3D12RootSignature,
        (void**)&g_root_signature
    );

    signature_blob->lpVtbl->Release(signature_blob);

    if (FAILED(hr)) {
        vs_blob->lpVtbl->Release(vs_blob);
        ps_blob->lpVtbl->Release(ps_blob);
        return 0;
    }

    D3D12_INPUT_ELEMENT_DESC input_layout[2];

    memset(input_layout, 0, sizeof(input_layout));

    input_layout[0].SemanticName = "POSITION";
    input_layout[0].SemanticIndex = 0;
    input_layout[0].Format = DXGI_FORMAT_R32G32_FLOAT;
    input_layout[0].InputSlot = 0;
    input_layout[0].AlignedByteOffset = 0;
    input_layout[0].InputSlotClass =
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    input_layout[1].SemanticName = "COLOR";
    input_layout[1].SemanticIndex = 0;
    input_layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    input_layout[1].InputSlot = 0;
    input_layout[1].AlignedByteOffset = 8;
    input_layout[1].InputSlotClass =
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;

    memset(&pso_desc, 0, sizeof(pso_desc));

    pso_desc.pRootSignature = g_root_signature;

    pso_desc.VS.pShaderBytecode =
        vs_blob->lpVtbl->GetBufferPointer(vs_blob);

    pso_desc.VS.BytecodeLength =
        vs_blob->lpVtbl->GetBufferSize(vs_blob);

    pso_desc.PS.pShaderBytecode =
        ps_blob->lpVtbl->GetBufferPointer(ps_blob);

    pso_desc.PS.BytecodeLength =
        ps_blob->lpVtbl->GetBufferSize(ps_blob);

    pso_desc.InputLayout.pInputElementDescs = input_layout;
    pso_desc.InputLayout.NumElements = 2;

    pso_desc.RasterizerState.FillMode =
        D3D12_FILL_MODE_SOLID;

    pso_desc.RasterizerState.CullMode =
        D3D12_CULL_MODE_NONE;

    pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    pso_desc.SampleMask = UINT_MAX;

    pso_desc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;

    pso_desc.NumRenderTargets = 1;

    pso_desc.RTVFormats[0] =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    pso_desc.SampleDesc.Count = 1;

    hr = g_device->lpVtbl->CreateGraphicsPipelineState(
        g_device,
        &pso_desc,
        &IID_ID3D12PipelineState,
        (void**)&g_pipeline_state
    );

    vs_blob->lpVtbl->Release(vs_blob);
    ps_blob->lpVtbl->Release(ps_blob);

    if (FAILED(hr)) {
        return 0;
    }

    return 1;
}

static int create_vertex_buffer(void) {
    D3D12_HEAP_PROPERTIES heap_properties;

    memset(&heap_properties, 0, sizeof(heap_properties));

    heap_properties.Type =
        D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resource_desc;

    memset(&resource_desc, 0, sizeof(resource_desc));

    resource_desc.Dimension =
        D3D12_RESOURCE_DIMENSION_BUFFER;

    resource_desc.Width =
        sizeof(DXGOVertex) * MAX_VERTICES;

    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.Layout =
        D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr =
        g_device->lpVtbl->CreateCommittedResource(
            g_device,
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            &IID_ID3D12Resource,
            (void**)&g_vertex_buffer
        );

    if (FAILED(hr)) {
        return 0;
    }

    g_vertex_buffer_view.BufferLocation =
        g_vertex_buffer->lpVtbl->GetGPUVirtualAddress(
            g_vertex_buffer
        );

    g_vertex_buffer_view.SizeInBytes =
        sizeof(DXGOVertex) * MAX_VERTICES;

    g_vertex_buffer_view.StrideInBytes =
        sizeof(DXGOVertex);

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

    queue_desc.Type =
        D3D12_COMMAND_LIST_TYPE_DIRECT;

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
    swap_desc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    swap_desc.SampleDesc.Count = 1;
    swap_desc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT;

    swap_desc.BufferCount = FRAME_COUNT;

    swap_desc.SwapEffect =
        DXGI_SWAP_EFFECT_FLIP_DISCARD;

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
    heap_desc.Type =
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

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

    D3D12_CPU_DESCRIPTOR_HANDLE handle;

    g_rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(
        g_rtv_heap,
        &handle
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

    if (!create_shaders()) {
        return 0;
    }

    if (!create_vertex_buffer()) {
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

static void begin_frame(void) {
    g_vertex_count = 0;

    g_command_allocator->lpVtbl->Reset(
        g_command_allocator
    );

    g_command_list->lpVtbl->Reset(
        g_command_list,
        g_command_allocator,
        g_pipeline_state
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
}

static void push_vertex(
    float x,
    float y,
    DXGOColor color
) {
    if (g_vertex_count >= MAX_VERTICES) {
        return;
    }

    g_vertices[g_vertex_count].x = x;
    g_vertices[g_vertex_count].y = y;
    g_vertices[g_vertex_count].r = color.r / 255.0f;
    g_vertices[g_vertex_count].g = color.g / 255.0f;
    g_vertices[g_vertex_count].b = color.b / 255.0f;
    g_vertices[g_vertex_count].a = color.a / 255.0f;

    g_vertex_count++;
}

static void draw_primitive(
    D3D12_PRIMITIVE_TOPOLOGY topology,
    UINT start,
    UINT count
) {
    if (count == 0) {
        return;
    }

    g_command_list->lpVtbl->IASetPrimitiveTopology(
        g_command_list,
        topology
    );

    g_command_list->lpVtbl->IASetVertexBuffers(
        g_command_list,
        0,
        1,
        &g_vertex_buffer_view
    );

    g_command_list->lpVtbl->DrawInstanced(
        g_command_list,
        count,
        1,
        start,
        0
    );
}

void dxgo_native_clear(
    float r,
    float g,
    float b,
    float a
) {
    begin_frame();

    D3D12_CPU_DESCRIPTOR_HANDLE rtv;

    g_rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(
        g_rtv_heap,
        &rtv
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

    g_command_list->lpVtbl->SetGraphicsRoot32BitConstants(
        g_command_list,
        0,
        2,
        (float[]){
            (float)g_width,
            (float)g_height
        },
        0
    );
}

void dxgo_native_draw_point(
    float x,
    float y,
    DXGOColor color
) {
    UINT start = g_vertex_count;

    push_vertex(x, y, color);

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
        start,
        1
    );
}

void dxgo_native_draw_line(
    float x1,
    float y1,
    float x2,
    float y2,
    DXGOColor color
) {
    UINT start = g_vertex_count;

    push_vertex(x1, y1, color);
    push_vertex(x2, y2, color);

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        start,
        2
    );
}

void dxgo_native_draw_rect(
    float x,
    float y,
    float width,
    float height,
    DXGOColor color
) {
    dxgo_native_draw_line(
        x,
        y,
        x + width,
        y,
        color
    );

    dxgo_native_draw_line(
        x + width,
        y,
        x + width,
        y + height,
        color
    );

    dxgo_native_draw_line(
        x + width,
        y + height,
        x,
        y + height,
        color
    );

    dxgo_native_draw_line(
        x,
        y + height,
        x,
        y,
        color
    );
}

void dxgo_native_fill_rect(
    float x,
    float y,
    float width,
    float height,
    DXGOColor color
) {
    UINT start = g_vertex_count;

    push_vertex(x, y, color);
    push_vertex(x + width, y, color);
    push_vertex(x + width, y + height, color);

    push_vertex(x, y, color);
    push_vertex(x + width, y + height, color);
    push_vertex(x, y + height, color);

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        start,
        6
    );
}

void dxgo_native_draw_circle(
    float x,
    float y,
    float radius,
    DXGOColor color
) {
    UINT start = g_vertex_count;

    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float a1 =
            2.0f * 3.14159265358979323846f *
            (float)i / CIRCLE_SEGMENTS;

        float a2 =
            2.0f * 3.14159265358979323846f *
            (float)(i + 1) / CIRCLE_SEGMENTS;

        push_vertex(
            x + cosf(a1) * radius,
            y + sinf(a1) * radius,
            color
        );

        push_vertex(
            x + cosf(a2) * radius,
            y + sinf(a2) * radius,
            color
        );
    }

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        start,
        CIRCLE_SEGMENTS * 2
    );
}

void dxgo_native_fill_circle(
    float x,
    float y,
    float radius,
    DXGOColor color
) {
    UINT start = g_vertex_count;

    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float a1 =
            2.0f * 3.14159265358979323846f *
            (float)i / CIRCLE_SEGMENTS;

        float a2 =
            2.0f * 3.14159265358979323846f *
            (float)(i + 1) / CIRCLE_SEGMENTS;

        push_vertex(x, y, color);

        push_vertex(
            x + cosf(a1) * radius,
            y + sinf(a1) * radius,
            color
        );

        push_vertex(
            x + cosf(a2) * radius,
            y + sinf(a2) * radius,
            color
        );
    }

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        start,
        CIRCLE_SEGMENTS * 3
    );
}

void dxgo_native_draw_ellipse(
    float x,
    float y,
    float rx,
    float ry,
    DXGOColor color
) {
    UINT start = g_vertex_count;

    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float a1 =
            2.0f * 3.14159265358979323846f *
            (float)i / CIRCLE_SEGMENTS;

        float a2 =
            2.0f * 3.14159265358979323846f *
            (float)(i + 1) / CIRCLE_SEGMENTS;

        push_vertex(
            x + cosf(a1) * rx,
            y + sinf(a1) * ry,
            color
        );

        push_vertex(
            x + cosf(a2) * rx,
            y + sinf(a2) * ry,
            color
        );
    }

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        start,
        CIRCLE_SEGMENTS * 2
    );
}

void dxgo_native_fill_ellipse(
    float x,
    float y,
    float rx,
    float ry,
    DXGOColor color
) {
    UINT start = g_vertex_count;

    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float a1 =
            2.0f * 3.14159265358979323846f *
            (float)i / CIRCLE_SEGMENTS;

        float a2 =
            2.0f * 3.14159265358979323846f *
            (float)(i + 1) / CIRCLE_SEGMENTS;

        push_vertex(x, y, color);

        push_vertex(
            x + cosf(a1) * rx,
            y + sinf(a1) * ry,
            color
        );

        push_vertex(
            x + cosf(a2) * rx,
            y + sinf(a2) * ry,
            color
        );
    }

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        start,
        CIRCLE_SEGMENTS * 3
    );
}

void dxgo_native_draw_triangle(
    float x1,
    float y1,
    float x2,
    float y2,
    float x3,
    float y3,
    DXGOColor color
) {
    dxgo_native_draw_line(x1, y1, x2, y2, color);
    dxgo_native_draw_line(x2, y2, x3, y3, color);
    dxgo_native_draw_line(x3, y3, x1, y1, color);
}

void dxgo_native_fill_triangle(
    float x1,
    float y1,
    float x2,
    float y2,
    float x3,
    float y3,
    DXGOColor color
) {
    UINT start = g_vertex_count;

    push_vertex(x1, y1, color);
    push_vertex(x2, y2, color);
    push_vertex(x3, y3, color);

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        start,
        3
    );
}

void dxgo_native_draw_polygon(
    const float* points,
    int count,
    DXGOColor color
) {
    if (!points || count < 2) {
        return;
    }

    for (int i = 0; i < count; i++) {
        int next = (i + 1) % count;

        dxgo_native_draw_line(
            points[i * 2],
            points[i * 2 + 1],
            points[next * 2],
            points[next * 2 + 1],
            color
        );
    }
}

void dxgo_native_fill_polygon(
    const float* points,
    int count,
    DXGOColor color
) {
    if (!points || count < 3) {
        return;
    }

    UINT start = g_vertex_count;

    for (int i = 1; i < count - 1; i++) {
        push_vertex(
            points[0],
            points[1],
            color
        );

        push_vertex(
            points[i * 2],
            points[i * 2 + 1],
            color
        );

        push_vertex(
            points[(i + 1) * 2],
            points[(i + 1) * 2 + 1],
            color
        );
    }

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        start,
        (count - 2) * 3
    );
}

void dxgo_native_update(void) {
    D3D12_RESOURCE_BARRIER barrier;

    memset(&barrier, 0, sizeof(barrier));

    barrier.Type =
        D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

    barrier.Transition.pResource =
        g_render_targets[g_frame_index];

    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

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

    if (g_vertex_buffer) {
        void* mapped = NULL;

        D3D12_RANGE range;

        range.Begin = 0;
        range.End = 0;

        if (SUCCEEDED(
            g_vertex_buffer->lpVtbl->Map(
                g_vertex_buffer,
                0,
                &range,
                &mapped
            )
        )) {
            memcpy(
                mapped,
                g_vertices,
                sizeof(DXGOVertex) * g_vertex_count
            );

            g_vertex_buffer->lpVtbl->Unmap(
                g_vertex_buffer,
                0,
                NULL
            );
        }
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

void dxgo_native_close(void) {
    if (g_command_queue && g_fence) {
        wait_gpu();
    }

    if (g_fence_event) {
        CloseHandle(g_fence_event);
        g_fence_event = NULL;
    }

    if (g_vertex_buffer) {
        g_vertex_buffer->lpVtbl->Release(g_vertex_buffer);
        g_vertex_buffer = NULL;
    }

    if (g_pipeline_state) {
        g_pipeline_state->lpVtbl->Release(g_pipeline_state);
        g_pipeline_state = NULL;
    }

    if (g_root_signature) {
        g_root_signature->lpVtbl->Release(g_root_signature);
        g_root_signature = NULL;
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