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

typedef struct {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
} DXGOVertex;

static HWND g_hwnd = NULL;
static int g_width = 800;
static int g_height = 600;

static WNDCLASSA g_window_class;

static ID3D12Device* g_device = NULL;
static ID3D12CommandQueue* g_command_queue = NULL;
static IDXGISwapChain3* g_swap_chain = NULL;
static ID3D12DescriptorHeap* g_rtv_heap = NULL;
static ID3D12Resource* g_render_targets[FRAME_COUNT] = {0};

static ID3D12CommandAllocator* g_command_allocator = NULL;
static ID3D12GraphicsCommandList* g_command_list = NULL;

static ID3D12Fence* g_fence = NULL;
static HANDLE g_fence_event = NULL;
static UINT64 g_fence_value = 0;

static UINT g_rtv_descriptor_size = 0;
static UINT g_frame_index = 0;

static unsigned char g_key_current[256];
static unsigned char g_key_previous[256];

static void update_keyboard(void);

static ID3D12RootSignature* g_root_signature = NULL;
static ID3D12PipelineState* g_point_pipeline_state = NULL;
static ID3D12PipelineState* g_line_pipeline_state = NULL;
static ID3D12PipelineState* g_triangle_pipeline_state = NULL;

static ID3D12Resource* g_vertex_buffer = NULL;
static DXGOVertex* g_vertex_data = NULL;
static D3D12_VERTEX_BUFFER_VIEW g_vertex_buffer_view;

static UINT g_vertex_count = 0;
static int g_frame_started = 0;

static LRESULT CALLBACK dxgo_wnd_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam
)
{
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            if (lparam != 0) {
                int width = (int)LOWORD(lparam);
                int height = (int)HIWORD(lparam);

                if (width > 0) {
                    g_width = width;
                }

                if (height > 0) {
                    g_height = height;
                }
            }
            return 0;
    }

    return DefWindowProcA(
        hwnd,
        message,
        wparam,
        lparam
    );
}

static int create_window(const char* title)
{
    HINSTANCE instance = GetModuleHandleA(NULL);

    memset(
        &g_window_class,
        0,
        sizeof(g_window_class)
    );

    g_window_class.style =
        CS_HREDRAW | CS_VREDRAW;

    g_window_class.lpfnWndProc =
        dxgo_wnd_proc;

    g_window_class.hInstance =
        instance;

    g_window_class.hCursor =
        LoadCursorA(NULL, IDC_ARROW);

    g_window_class.lpszClassName =
        "DXGOWindowClass";

    if (!RegisterClassA(&g_window_class)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return 0;
        }
    }

    RECT rect;

    rect.left = 0;
    rect.top = 0;
    rect.right = g_width;
    rect.bottom = g_height;

    AdjustWindowRect(
        &rect,
        WS_OVERLAPPEDWINDOW,
        FALSE
    );

    g_hwnd = CreateWindowA(
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

    ShowWindow(
        g_hwnd,
        SW_SHOW
    );

    UpdateWindow(g_hwnd);

    return 1;
}

static int create_device(void)
{
    IDXGIFactory4* factory = NULL;

    HRESULT hr = CreateDXGIFactory2(
        0,
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

    memset(
        &queue_desc,
        0,
        sizeof(queue_desc)
    );

    queue_desc.Type =
        D3D12_COMMAND_LIST_TYPE_DIRECT;

    queue_desc.Flags =
        D3D12_COMMAND_QUEUE_FLAG_NONE;

    hr = g_device->lpVtbl->CreateCommandQueue(
        g_device,
        &queue_desc,
        &IID_ID3D12CommandQueue,
        (void**)&g_command_queue
    );

    factory->lpVtbl->Release(factory);

    if (FAILED(hr)) {
        return 0;
    }

    return 1;
}

static int create_swap_chain(void)
{
    IDXGIFactory4* factory = NULL;

    HRESULT hr = CreateDXGIFactory2(
        0,
        &IID_IDXGIFactory4,
        (void**)&factory
    );

    if (FAILED(hr)) {
        return 0;
    }

    DXGI_SWAP_CHAIN_DESC1 desc;

    memset(
        &desc,
        0,
        sizeof(desc)
    );

    desc.Width = g_width;
    desc.Height = g_height;
    desc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = FRAME_COUNT;
    desc.Scaling =
        DXGI_SCALING_STRETCH;
    desc.SwapEffect =
        DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode =
        DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = 0;

    IDXGISwapChain1* swap_chain1 = NULL;

    hr = factory->lpVtbl->CreateSwapChainForHwnd(
        factory,
        (IUnknown*)g_command_queue,
        g_hwnd,
        &desc,
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

    swap_chain1->lpVtbl->Release(
        swap_chain1
    );

    factory->lpVtbl->Release(
        factory
    );

    if (FAILED(hr)) {
        return 0;
    }

    g_frame_index =
        g_swap_chain->lpVtbl->GetCurrentBackBufferIndex(
            g_swap_chain
        );

    return 1;
}

static int create_rtv_heap(void)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc;

    memset(
        &desc,
        0,
        sizeof(desc)
    );

    desc.NumDescriptors =
        FRAME_COUNT;

    desc.Type =
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    desc.Flags =
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr =
        g_device->lpVtbl->CreateDescriptorHeap(
            g_device,
            &desc,
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

        D3D12_CPU_DESCRIPTOR_HANDLE rtv =
            handle;

        rtv.ptr +=
            (SIZE_T)i *
            g_rtv_descriptor_size;

        g_device->lpVtbl->CreateRenderTargetView(
            g_device,
            g_render_targets[i],
            NULL,
            rtv
        );
    }

    return 1;
}

static int create_command_objects(void)
{
    HRESULT hr =
        g_device->lpVtbl->CreateCommandAllocator(
            g_device,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            &IID_ID3D12CommandAllocator,
            (void**)&g_command_allocator
        );

    if (FAILED(hr)) {
        return 0;
    }

    hr =
        g_device->lpVtbl->CreateCommandList(
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

    hr =
        g_command_list->lpVtbl->Close(
            g_command_list
        );

    if (FAILED(hr)) {
        return 0;
    }

    return 1;
}

static int create_fence(void)
{
    HRESULT hr =
        g_device->lpVtbl->CreateFence(
            g_device,
            0,
            D3D12_FENCE_FLAG_NONE,
            &IID_ID3D12Fence,
            (void**)&g_fence
        );

    if (FAILED(hr)) {
        return 0;
    }

    g_fence_event =
        CreateEventA(
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

static int compile_shader(
    const char* source,
    const char* entry,
    const char* target,
    ID3DBlob** blob
)
{
    ID3DBlob* error_blob = NULL;

    HRESULT hr = D3DCompile(
        source,
        strlen(source),
        NULL,
        NULL,
        NULL,
        entry,
        target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        blob,
        &error_blob
    );

    if (error_blob) {
        error_blob->lpVtbl->Release(
            error_blob
        );
    }

    return SUCCEEDED(hr);
}

static int create_root_signature(void)
{
    D3D12_ROOT_PARAMETER parameter;

    memset(
        &parameter,
        0,
        sizeof(parameter)
    );

    parameter.ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;

    parameter.Constants.ShaderRegister = 0;
    parameter.Constants.RegisterSpace = 0;
    parameter.Constants.Num32BitValues = 2;

    parameter.ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC desc;

    memset(
        &desc,
        0,
        sizeof(desc)
    );

    desc.NumParameters = 1;
    desc.pParameters = &parameter;
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = NULL;

    desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob* signature = NULL;
    ID3DBlob* error = NULL;

    HRESULT hr =
        D3D12SerializeRootSignature(
            &desc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signature,
            &error
        );

    if (error) {
        error->lpVtbl->Release(error);
    }

    if (FAILED(hr)) {
        return 0;
    }

    hr =
        g_device->lpVtbl->CreateRootSignature(
            g_device,
            0,
            signature->lpVtbl->GetBufferPointer(
                signature
            ),
            signature->lpVtbl->GetBufferSize(
                signature
            ),
            &IID_ID3D12RootSignature,
            (void**)&g_root_signature
        );

    signature->lpVtbl->Release(
        signature
    );

    if (FAILED(hr)) {
        return 0;
    }

    return 1;
}

static int create_pipeline_states(void)
{
    const char* shader_source =
        "struct VSInput {"
        "float2 position : POSITION;"
        "float4 color : COLOR;"
        "};"

        "struct VSOutput {"
        "float4 position : SV_POSITION;"
        "float4 color : COLOR;"
        "};"

        "cbuffer Constants : register(b0) {"
        "float width;"
        "float height;"
        "};"

        "VSOutput VSMain(VSInput input)"
        "{"
        "VSOutput output;"
        "float x = input.position.x / width * 2.0 - 1.0;"
        "float y = 1.0 - input.position.y / height * 2.0;"
        "output.position = float4(x, y, 0.0, 1.0);"
        "output.color = input.color;"
        "return output;"
        "}"

        "float4 PSMain(VSOutput input) : SV_TARGET"
        "{"
        "return input.color;"
        "}";

    ID3DBlob* vertex_shader = NULL;
    ID3DBlob* pixel_shader = NULL;

    if (!compile_shader(
        shader_source,
        "VSMain",
        "vs_5_0",
        &vertex_shader
    )) {
        return 0;
    }

    if (!compile_shader(
        shader_source,
        "PSMain",
        "ps_5_0",
        &pixel_shader
    )) {
        vertex_shader->lpVtbl->Release(
            vertex_shader
        );
        return 0;
    }

    D3D12_INPUT_ELEMENT_DESC input_layout[2];

    memset(
        input_layout,
        0,
        sizeof(input_layout)
    );

    input_layout[0].SemanticName =
        "POSITION";

    input_layout[0].SemanticIndex = 0;

    input_layout[0].Format =
        DXGI_FORMAT_R32G32_FLOAT;

    input_layout[0].InputSlot = 0;

    input_layout[0].AlignedByteOffset = 0;

    input_layout[0].InputSlotClass =
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    input_layout[1].SemanticName =
        "COLOR";

    input_layout[1].SemanticIndex = 0;

    input_layout[1].Format =
        DXGI_FORMAT_R32G32B32A32_FLOAT;

    input_layout[1].InputSlot = 0;

    input_layout[1].AlignedByteOffset = 8;

    input_layout[1].InputSlotClass =
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    D3D12_RASTERIZER_DESC rasterizer;

    memset(
        &rasterizer,
        0,
        sizeof(rasterizer)
    );

    rasterizer.FillMode =
        D3D12_FILL_MODE_SOLID;

    rasterizer.CullMode =
        D3D12_CULL_MODE_NONE;

    rasterizer.FrontCounterClockwise =
        FALSE;

    rasterizer.DepthBias =
        D3D12_DEFAULT_DEPTH_BIAS;

    rasterizer.DepthBiasClamp =
        D3D12_DEFAULT_DEPTH_BIAS_CLAMP;

    rasterizer.SlopeScaledDepthBias =
        D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;

    rasterizer.DepthClipEnable =
        TRUE;

    rasterizer.MultisampleEnable =
        FALSE;

    rasterizer.AntialiasedLineEnable =
        FALSE;

    rasterizer.ForcedSampleCount = 0;

    rasterizer.ConservativeRaster =
        D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blend;

    memset(
        &blend,
        0,
        sizeof(blend)
    );

    blend.AlphaToCoverageEnable =
        FALSE;

    blend.IndependentBlendEnable =
        FALSE;

    blend.RenderTarget[0].BlendEnable =
        TRUE;

    blend.RenderTarget[0].LogicOpEnable =
        FALSE;

    blend.RenderTarget[0].SrcBlend =
        D3D12_BLEND_SRC_ALPHA;

    blend.RenderTarget[0].DestBlend =
        D3D12_BLEND_INV_SRC_ALPHA;

    blend.RenderTarget[0].BlendOp =
        D3D12_BLEND_OP_ADD;

    blend.RenderTarget[0].SrcBlendAlpha =
        D3D12_BLEND_ONE;

    blend.RenderTarget[0].DestBlendAlpha =
        D3D12_BLEND_INV_SRC_ALPHA;

    blend.RenderTarget[0].BlendOpAlpha =
        D3D12_BLEND_OP_ADD;

    blend.RenderTarget[0].LogicOp =
        D3D12_LOGIC_OP_NOOP;

    blend.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC depth;

    memset(
        &depth,
        0,
        sizeof(depth)
    );

    depth.DepthEnable = FALSE;
    depth.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC base;

    memset(
        &base,
        0,
        sizeof(base)
    );

    base.pRootSignature =
        g_root_signature;

    base.VS.pShaderBytecode =
        vertex_shader->lpVtbl->GetBufferPointer(
            vertex_shader
        );

    base.VS.BytecodeLength =
        vertex_shader->lpVtbl->GetBufferSize(
            vertex_shader
        );

    base.PS.pShaderBytecode =
        pixel_shader->lpVtbl->GetBufferPointer(
            pixel_shader
        );

    base.PS.BytecodeLength =
        pixel_shader->lpVtbl->GetBufferSize(
            pixel_shader
        );

    base.BlendState = blend;
    base.SampleMask = UINT_MAX;
    base.RasterizerState = rasterizer;
    base.DepthStencilState = depth;

    base.InputLayout.pInputElementDescs =
        input_layout;

    base.InputLayout.NumElements = 2;

    base.NumRenderTargets = 1;

    base.RTVFormats[0] =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    base.DSVFormat =
        DXGI_FORMAT_UNKNOWN;

    base.SampleDesc.Count = 1;
    base.SampleDesc.Quality = 0;

    base.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

    HRESULT hr =
        g_device->lpVtbl->CreateGraphicsPipelineState(
            g_device,
            &base,
            &IID_ID3D12PipelineState,
            (void**)&g_point_pipeline_state
        );

    if (FAILED(hr)) {
        pixel_shader->lpVtbl->Release(
            pixel_shader
        );

        vertex_shader->lpVtbl->Release(
            vertex_shader
        );

        return 0;
    }

    base.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

    hr =
        g_device->lpVtbl->CreateGraphicsPipelineState(
            g_device,
            &base,
            &IID_ID3D12PipelineState,
            (void**)&g_line_pipeline_state
        );

    if (FAILED(hr)) {
        pixel_shader->lpVtbl->Release(
            pixel_shader
        );

        vertex_shader->lpVtbl->Release(
            vertex_shader
        );

        return 0;
    }

    base.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    hr =
        g_device->lpVtbl->CreateGraphicsPipelineState(
            g_device,
            &base,
            &IID_ID3D12PipelineState,
            (void**)&g_triangle_pipeline_state
        );

    pixel_shader->lpVtbl->Release(
        pixel_shader
    );

    vertex_shader->lpVtbl->Release(
        vertex_shader
    );

    if (FAILED(hr)) {
        return 0;
    }

    return 1;
}

static int create_vertex_buffer(void)
{
    D3D12_HEAP_PROPERTIES heap;

    memset(
        &heap,
        0,
        sizeof(heap)
    );

    heap.Type =
        D3D12_HEAP_TYPE_UPLOAD;

    heap.CPUPageProperty =
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN;

    heap.MemoryPoolPreference =
        D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC desc;

    memset(
        &desc,
        0,
        sizeof(desc)
    );

    desc.Dimension =
        D3D12_RESOURCE_DIMENSION_BUFFER;

    desc.Alignment = 0;

    desc.Width =
        sizeof(DXGOVertex) *
        MAX_VERTICES;

    desc.Height = 1;

    desc.DepthOrArraySize = 1;

    desc.MipLevels = 1;

    desc.Format =
        DXGI_FORMAT_UNKNOWN;

    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    desc.Layout =
        D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    desc.Flags =
        D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr =
        g_device->lpVtbl->CreateCommittedResource(
            g_device,
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            &IID_ID3D12Resource,
            (void**)&g_vertex_buffer
        );

    if (FAILED(hr)) {
        return 0;
    }

    g_vertex_data =
        (DXGOVertex*)malloc(
            sizeof(DXGOVertex) *
            MAX_VERTICES
        );

    if (!g_vertex_data) {
        return 0;
    }

    g_vertex_buffer_view.BufferLocation =
        g_vertex_buffer->lpVtbl->GetGPUVirtualAddress(
            g_vertex_buffer
        );

    g_vertex_buffer_view.StrideInBytes =
        sizeof(DXGOVertex);

    g_vertex_buffer_view.SizeInBytes =
        sizeof(DXGOVertex) *
        MAX_VERTICES;

    return 1;
}

static void wait_gpu(void)
{
    g_fence_value++;

    g_command_queue->lpVtbl->Signal(
        g_command_queue,
        g_fence,
        g_fence_value
    );

    if (
        g_fence->lpVtbl->GetCompletedValue(
            g_fence
        ) < g_fence_value
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

static D3D12_CPU_DESCRIPTOR_HANDLE current_rtv(void)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;

    g_rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(
        g_rtv_heap,
        &handle
    );

    handle.ptr +=
        (SIZE_T)g_frame_index *
        g_rtv_descriptor_size;

    return handle;
}

static void begin_frame(void)
{
    if (g_frame_started) {
        return;
    }

    g_vertex_count = 0;

    g_command_allocator->lpVtbl->Reset(
        g_command_allocator
    );

    g_command_list->lpVtbl->Reset(
        g_command_list,
        g_command_allocator,
        g_triangle_pipeline_state
    );

    D3D12_VIEWPORT viewport;

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = (float)g_width;
    viewport.Height = (float)g_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    g_command_list->lpVtbl->RSSetViewports(
        g_command_list,
        1,
        &viewport
    );

    D3D12_RECT scissor;

    scissor.left = 0;
    scissor.top = 0;
    scissor.right = g_width;
    scissor.bottom = g_height;

    g_command_list->lpVtbl->RSSetScissorRects(
        g_command_list,
        1,
        &scissor
    );

    g_command_list->lpVtbl->SetGraphicsRootSignature(
        g_command_list,
        g_root_signature
    );

    float constants[2];

    constants[0] =
        (float)g_width;

    constants[1] =
        (float)g_height;

    g_command_list->lpVtbl->SetGraphicsRoot32BitConstants(
        g_command_list,
        0,
        2,
        constants,
        0
    );

    g_command_list->lpVtbl->IASetVertexBuffers(
        g_command_list,
        0,
        1,
        &g_vertex_buffer_view
    );

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        current_rtv();

    D3D12_RESOURCE_BARRIER barrier;

    memset(
        &barrier,
        0,
        sizeof(barrier)
    );

    barrier.Type =
        D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

    barrier.Flags =
        D3D12_RESOURCE_BARRIER_FLAG_NONE;

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

    g_command_list->lpVtbl->OMSetRenderTargets(
        g_command_list,
        1,
        &rtv,
        FALSE,
        NULL
    );

    g_frame_started = 1;
}

static int add_vertex(
    float x,
    float y,
    float r,
    float g,
    float b,
    float a
)
{
    if (g_vertex_count >= MAX_VERTICES) {
        return 0;
    }

    DXGOVertex* vertex =
        &g_vertex_data[g_vertex_count];

    vertex->x = x;
    vertex->y = y;
    vertex->r = r;
    vertex->g = g;
    vertex->b = b;
    vertex->a = a;

    g_vertex_count++;

    return 1;
}

static void draw_primitive(
    D3D12_PRIMITIVE_TOPOLOGY topology,
    UINT start,
    UINT count
)
{
    ID3D12PipelineState* pipeline =
        g_triangle_pipeline_state;

    if (
        topology ==
        D3D_PRIMITIVE_TOPOLOGY_POINTLIST
    ) {
        pipeline =
            g_point_pipeline_state;
    } else if (
        topology ==
        D3D_PRIMITIVE_TOPOLOGY_LINELIST
    ) {
        pipeline =
            g_line_pipeline_state;
    }

    g_command_list->lpVtbl->SetPipelineState(
        g_command_list,
        pipeline
    );

    g_command_list->lpVtbl->IASetPrimitiveTopology(
        g_command_list,
        topology
    );

    g_command_list->lpVtbl->DrawInstanced(
        g_command_list,
        count,
        1,
        start,
        0
    );
}

static float color_r(DXGOColor color)
{
    return (float)color.r / 255.0f;
}

static float color_g(DXGOColor color)
{
    return (float)color.g / 255.0f;
}

static float color_b(DXGOColor color)
{
    return (float)color.b / 255.0f;
}

static float color_a(DXGOColor color)
{
    return (float)color.a / 255.0f;
}

int dxgo_native_init(
    const char* title,
    int width,
    int height
)
{
    g_width = width;
    g_height = height;

    if (!create_window(title)) {
        return 0;
    }

    if (!create_device()) {
        return 0;
    }

    if (!create_swap_chain()) {
        return 0;
    }

    if (!create_rtv_heap()) {
        return 0;
    }

    if (!create_command_objects()) {
        return 0;
    }

    if (!create_fence()) {
        return 0;
    }

    if (!create_root_signature()) {
        return 0;
    }

    if (!create_pipeline_states()) {
        return 0;
    }

    if (!create_vertex_buffer()) {
        return 0;
    }

    return 1;
}

int dxgo_native_open(void)
{
    MSG message;

    while (
        PeekMessageA(
            &message,
            NULL,
            0,
            0,
            PM_REMOVE
        )
    ) {
        if (message.message == WM_QUIT) {
            return 0;
        }

        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    update_keyboard();

    return g_hwnd != NULL &&
           IsWindow(g_hwnd);
}

void dxgo_native_clear(
    float r,
    float g,
    float b,
    float a
)
{
    begin_frame();

    float color[4];

    color[0] = r;
    color[1] = g;
    color[2] = b;
    color[3] = a;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        current_rtv();

    g_command_list->lpVtbl->ClearRenderTargetView(
        g_command_list,
        rtv,
        color,
        0,
        NULL
    );
}

void dxgo_native_draw_point(
    float x,
    float y,
    DXGOColor color
)
{
    begin_frame();

    UINT start = g_vertex_count;

    if (!add_vertex(
        x,
        y,
        color_r(color),
        color_g(color),
        color_b(color),
        color_a(color)
    )) {
        return;
    }

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
)
{
    begin_frame();

    UINT start = g_vertex_count;

    if (!add_vertex(
        x1,
        y1,
        color_r(color),
        color_g(color),
        color_b(color),
        color_a(color)
    )) {
        return;
    }

    if (!add_vertex(
        x2,
        y2,
        color_r(color),
        color_g(color),
        color_b(color),
        color_a(color)
    )) {
        return;
    }

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
)
{
    begin_frame();

    UINT start = g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    if (!add_vertex(x, y, r, g, b, a)) return;
    if (!add_vertex(x + width, y, r, g, b, a)) return;

    if (!add_vertex(x + width, y, r, g, b, a)) return;
    if (!add_vertex(
        x + width,
        y + height,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x + width,
        y + height,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x,
        y + height,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x,
        y + height,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(x, y, r, g, b, a)) return;

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        start,
        8
    );
}

void dxgo_native_fill_rect(
    float x,
    float y,
    float width,
    float height,
    DXGOColor color
)
{
    begin_frame();

    UINT start = g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    if (!add_vertex(x, y, r, g, b, a)) return;

    if (!add_vertex(
        x + width,
        y,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x + width,
        y + height,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(x, y, r, g, b, a)) return;

    if (!add_vertex(
        x + width,
        y + height,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x,
        y + height,
        r,
        g,
        b,
        a
    )) return;

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
)
{
    begin_frame();

    UINT start = g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float angle1 =
            (float)i /
            CIRCLE_SEGMENTS *
            2.0f *
            (float)M_PI;

        float angle2 =
            (float)(i + 1) /
            CIRCLE_SEGMENTS *
            2.0f *
            (float)M_PI;

        if (!add_vertex(
            x + cosf(angle1) * radius,
            y + sinf(angle1) * radius,
            r,
            g,
            b,
            a
        )) {
            return;
        }

        if (!add_vertex(
            x + cosf(angle2) * radius,
            y + sinf(angle2) * radius,
            r,
            g,
            b,
            a
        )) {
            return;
        }
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
)
{
    begin_frame();

    UINT start = g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float angle1 =
            (float)i /
            CIRCLE_SEGMENTS *
            2.0f *
            (float)M_PI;

        float angle2 =
            (float)(i + 1) /
            CIRCLE_SEGMENTS *
            2.0f *
            (float)M_PI;

        if (!add_vertex(
            x,
            y,
            r,
            g,
            b,
            a
        )) {
            return;
        }

        if (!add_vertex(
            x + cosf(angle1) * radius,
            y + sinf(angle1) * radius,
            r,
            g,
            b,
            a
        )) {
            return;
        }

        if (!add_vertex(
            x + cosf(angle2) * radius,
            y + sinf(angle2) * radius,
            r,
            g,
            b,
            a
        )) {
            return;
        }
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
)
{
    begin_frame();

    UINT start = g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float angle1 =
            (float)i /
            CIRCLE_SEGMENTS *
            2.0f *
            (float)M_PI;

        float angle2 =
            (float)(i + 1) /
            CIRCLE_SEGMENTS *
            2.0f *
            (float)M_PI;

        if (!add_vertex(
            x + cosf(angle1) * rx,
            y + sinf(angle1) * ry,
            r,
            g,
            b,
            a
        )) {
            return;
        }

        if (!add_vertex(
            x + cosf(angle2) * rx,
            y + sinf(angle2) * ry,
            r,
            g,
            b,
            a
        )) {
            return;
        }
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
)
{
    begin_frame();

    UINT start = g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float angle1 =
            (float)i /
            CIRCLE_SEGMENTS *
            2.0f *
            (float)M_PI;

        float angle2 =
            (float)(i + 1) /
            CIRCLE_SEGMENTS *
            2.0f *
            (float)M_PI;

        if (!add_vertex(
            x,
            y,
            r,
            g,
            b,
            a
        )) {
            return;
        }

        if (!add_vertex(
            x + cosf(angle1) * rx,
            y + sinf(angle1) * ry,
            r,
            g,
            b,
            a
        )) {
            return;
        }

        if (!add_vertex(
            x + cosf(angle2) * rx,
            y + sinf(angle2) * ry,
            r,
            g,
            b,
            a
        )) {
            return;
        }
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
)
{
    begin_frame();

    UINT start = g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    if (!add_vertex(
        x1,
        y1,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x2,
        y2,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x2,
        y2,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x3,
        y3,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x3,
        y3,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x1,
        y1,
        r,
        g,
        b,
        a
    )) return;

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        start,
        6
    );
}

void dxgo_native_fill_triangle(
    float x1,
    float y1,
    float x2,
    float y2,
    float x3,
    float y3,
    DXGOColor color
)
{
    begin_frame();

    UINT start = g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    if (!add_vertex(
        x1,
        y1,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x2,
        y2,
        r,
        g,
        b,
        a
    )) return;

    if (!add_vertex(
        x3,
        y3,
        r,
        g,
        b,
        a
    )) return;

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
)
{
    if (!points || count < 2) {
        return;
    }

    begin_frame();

    UINT start = g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 0; i < count; i++) {
        int next = (i + 1) % count;

        if (!add_vertex(
            points[i * 2],
            points[i * 2 + 1],
            r,
            g,
            b,
            a
        )) {
            return;
        }

        if (!add_vertex(
            points[next * 2],
            points[next * 2 + 1],
            r,
            g,
            b,
            a
        )) {
            return;
        }
    }

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        start,
        count * 2
    );
}

void dxgo_native_fill_polygon(
    const float* points,
    int count,
    DXGOColor color
)
{
    if (!points || count < 3) {
        return;
    }

    begin_frame();

    UINT start = g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 1; i < count - 1; i++) {
        if (!add_vertex(
            points[0],
            points[1],
            r,
            g,
            b,
            a
        )) {
            return;
        }

        if (!add_vertex(
            points[i * 2],
            points[i * 2 + 1],
            r,
            g,
            b,
            a
        )) {
            return;
        }

        if (!add_vertex(
            points[(i + 1) * 2],
            points[(i + 1) * 2 + 1],
            r,
            g,
            b,
            a
        )) {
            return;
        }
    }

    draw_primitive(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        start,
        (count - 2) * 3
    );
}

void dxgo_native_update(void)
{
    if (!g_frame_started) {
        begin_frame();
    }

    if (g_vertex_count > 0) {
        D3D12_RANGE range;

        range.Begin = 0;
        range.End =
            sizeof(DXGOVertex) *
            g_vertex_count;

        void* mapped = NULL;

        HRESULT hr =
            g_vertex_buffer->lpVtbl->Map(
                g_vertex_buffer,
                0,
                &range,
                &mapped
            );

        if (SUCCEEDED(hr)) {
            memcpy(
                mapped,
                g_vertex_data,
                sizeof(DXGOVertex) *
                g_vertex_count
            );

            g_vertex_buffer->lpVtbl->Unmap(
                g_vertex_buffer,
                0,
                &range
            );
        }
    }

    D3D12_RESOURCE_BARRIER barrier;

    memset(
        &barrier,
        0,
        sizeof(barrier)
    );

    barrier.Type =
        D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

    barrier.Flags =
        D3D12_RESOURCE_BARRIER_FLAG_NONE;

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

    ID3D12CommandList* lists[1];

    lists[0] =
        (ID3D12CommandList*)g_command_list;

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

    g_vertex_count = 0;
    g_frame_started = 0;
}

void dxgo_native_close(void)
{
    if (g_command_queue &&
        g_fence &&
        g_fence_event) {
        wait_gpu();
    }

    if (g_vertex_data) {
        free(g_vertex_data);
        g_vertex_data = NULL;
    }

    if (g_vertex_buffer) {
        g_vertex_buffer->lpVtbl->Release(
            g_vertex_buffer
        );
        g_vertex_buffer = NULL;
    }

    if (g_point_pipeline_state) {
        g_point_pipeline_state->lpVtbl->Release(
            g_point_pipeline_state
        );
        g_point_pipeline_state = NULL;
    }

    if (g_line_pipeline_state) {
        g_line_pipeline_state->lpVtbl->Release(
            g_line_pipeline_state
        );
        g_line_pipeline_state = NULL;
    }

    if (g_triangle_pipeline_state) {
        g_triangle_pipeline_state->lpVtbl->Release(
            g_triangle_pipeline_state
        );
        g_triangle_pipeline_state = NULL;
    }

    if (g_root_signature) {
        g_root_signature->lpVtbl->Release(
            g_root_signature
        );
        g_root_signature = NULL;
    }

    if (g_fence_event) {
        CloseHandle(g_fence_event);
        g_fence_event = NULL;
    }

    if (g_fence) {
        g_fence->lpVtbl->Release(
            g_fence
        );
        g_fence = NULL;
    }

    if (g_command_list) {
        g_command_list->lpVtbl->Release(
            g_command_list
        );
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
        g_rtv_heap->lpVtbl->Release(
            g_rtv_heap
        );
        g_rtv_heap = NULL;
    }

    if (g_swap_chain) {
        g_swap_chain->lpVtbl->Release(
            g_swap_chain
        );
        g_swap_chain = NULL;
    }

    if (g_command_queue) {
        g_command_queue->lpVtbl->Release(
            g_command_queue
        );
        g_command_queue = NULL;
    }

    if (g_device) {
        g_device->lpVtbl->Release(
            g_device
        );
        g_device = NULL;
    }

    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }

    g_vertex_count = 0;
    g_frame_started = 0;
}

void dxgo_native_draw_text(
    const char* text,
    float x,
    float y,
    int size,
    DXGOColor color
)
{
    if (!text || !g_hwnd || size <= 0) {
        return;
    }

    HDC dc = GetDC(g_hwnd);

    if (!dc) {
        return;
    }

    HFONT font = CreateFontA(
        -size,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI"
    );

    if (!font) {
        ReleaseDC(g_hwnd, dc);
        return;
    }

    HFONT old_font =
        (HFONT)SelectObject(dc, font);

    SetBkMode(
        dc,
        TRANSPARENT
    );

    SetTextColor(
        dc,
        RGB(
            color.r,
            color.g,
            color.b
        )
    );

    RECT rect;

    rect.left = (LONG)x;
    rect.top = (LONG)y;
    rect.right = rect.left + 4096;
    rect.bottom = rect.top + size * 2;

    DrawTextA(
        dc,
        text,
        -1,
        &rect,
        DT_LEFT | DT_TOP | DT_NOCLIP
    );

    SelectObject(
        dc,
        old_font
    );

    DeleteObject(font);

    ReleaseDC(
        g_hwnd,
        dc
    );
}

static void update_keyboard(void)
{
    memcpy(
        g_key_previous,
        g_key_current,
        sizeof(g_key_current)
    );

    for (int i = 0; i < 256; i++) {
        SHORT state =
            GetAsyncKeyState(i);

        g_key_current[i] =
            (state & 0x8000) != 0;
    }
}

int dxgo_native_key_down(int key)
{
    if (key < 0 || key >= 256) {
        return 0;
    }

    return g_key_current[key] != 0;
}

int dxgo_native_key_pressed(int key)
{
    if (key < 0 || key >= 256) {
        return 0;
    }

    return g_key_current[key] != 0 &&
           g_key_previous[key] == 0;
}

int dxgo_native_key_released(int key)
{
    if (key < 0 || key >= 256) {
        return 0;
    }

    return g_key_current[key] == 0 &&
           g_key_previous[key] != 0;
}