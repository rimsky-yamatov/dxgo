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
#define DXGO_PI 3.14159265358979323846f

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

static ID3D12Resource* g_render_targets[FRAME_COUNT] = {
    NULL,
    NULL
};

static ID3D12CommandAllocator* g_command_allocator = NULL;
static ID3D12GraphicsCommandList* g_command_list = NULL;

static ID3D12Fence* g_fence = NULL;
static HANDLE g_fence_event = NULL;
static UINT64 g_fence_value = 0;

static UINT g_rtv_descriptor_size = 0;
static UINT g_frame_index = 0;

static ID3D12RootSignature* g_root_signature = NULL;

static ID3D12PipelineState* g_point_pipeline_state = NULL;
static ID3D12PipelineState* g_line_pipeline_state = NULL;
static ID3D12PipelineState* g_triangle_pipeline_state = NULL;

static ID3D12Resource* g_vertex_buffer = NULL;
static DXGOVertex* g_vertex_data = NULL;

static D3D12_VERTEX_BUFFER_VIEW g_vertex_buffer_view;

static UINT g_vertex_count = 0;
static int g_frame_started = 0;
static int g_initialized = 0;

static unsigned char g_key_current[256];
static unsigned char g_key_previous[256];

static int g_vsync = 1;
static int g_cursor_visible = 1;

static int g_mouse_wheel_accum = 0;
static float g_mouse_wheel_frame = 0.0f;

#define FONT_GLYPH_FIRST 32
#define FONT_GLYPH_LAST 126
#define FONT_GLYPH_COUNT (FONT_GLYPH_LAST - FONT_GLYPH_FIRST + 1)
#define FONT_ATLAS_COLS 10
#define FONT_ATLAS_ROWS 10
#define FONT_CELL_W 26
#define FONT_CELL_H 34
#define FONT_ATLAS_W (FONT_ATLAS_COLS * FONT_CELL_W)
#define FONT_ATLAS_H (FONT_ATLAS_ROWS * FONT_CELL_H)
#define FONT_REFERENCE_SIZE 28
#define MAX_TEXT_VERTICES 6144

typedef struct {
    float x;
    float y;
    float u;
    float v;
    float r;
    float g;
    float b;
    float a;
} DXGOTextVertex;

static int g_font_atlas_ready = 0;
static int g_font_line_height = 0;
static float g_glyph_advance[FONT_GLYPH_COUNT];

static ID3D12RootSignature* g_text_root_signature = NULL;
static ID3D12PipelineState* g_text_pipeline_state = NULL;
static ID3D12DescriptorHeap* g_text_srv_heap = NULL;
static ID3D12Resource* g_font_texture = NULL;
static ID3D12Resource* g_text_vertex_buffer = NULL;
static DXGOTextVertex* g_text_vertex_data = NULL;
static D3D12_VERTEX_BUFFER_VIEW g_text_vertex_buffer_view;

static void update_keyboard(void);

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

        case WM_MOUSEWHEEL:
            g_mouse_wheel_accum +=
                GET_WHEEL_DELTA_WPARAM(wparam);
            return 0;

        default:
            break;
    }

    return DefWindowProcA(
        hwnd,
        message,
        wparam,
        lparam
    );
}

static int create_window(
    const char* title
)
{
    HINSTANCE instance =
        GetModuleHandleA(NULL);

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

    if (!AdjustWindowRect(
        &rect,
        WS_OVERLAPPEDWINDOW,
        FALSE
    )) {
        return 0;
    }

    g_hwnd = CreateWindowA(
        "DXGOWindowClass",
        title ? title : "dxgo",
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
    HRESULT hr;

    IDXGIFactory4* factory = NULL;

    hr = CreateDXGIFactory2(
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

    hr =
        g_device->lpVtbl->CreateCommandQueue(
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
    HRESULT hr;

    IDXGIFactory4* factory = NULL;

    hr = CreateDXGIFactory2(
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

    desc.Width = (UINT)g_width;
    desc.Height = (UINT)g_height;
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

    hr =
        factory->lpVtbl->CreateSwapChainForHwnd(
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

    hr =
        swap_chain1->lpVtbl->QueryInterface(
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
    HRESULT hr;

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

    hr =
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
        hr =
            g_swap_chain->lpVtbl->GetBuffer(
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
    HRESULT hr;

    hr =
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
    HRESULT hr;

    hr =
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

    HRESULT hr =
        D3DCompile(
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

    memset(
        g_vertex_data,
        0,
        sizeof(DXGOVertex) *
        MAX_VERTICES
    );

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
    if (!g_command_queue ||
        !g_fence ||
        !g_fence_event) {
        return;
    }

    g_fence_value++;

    if (FAILED(
        g_command_queue->lpVtbl->Signal(
            g_command_queue,
            g_fence,
            g_fence_value
        )
    )) {
        return;
    }

    if (
        g_fence->lpVtbl->GetCompletedValue(
            g_fence
        ) < g_fence_value
    ) {
        if (SUCCEEDED(
            g_fence->lpVtbl->SetEventOnCompletion(
                g_fence,
                g_fence_value,
                g_fence_event
            )
        )) {
            WaitForSingleObject(
                g_fence_event,
                INFINITE
            );
        }
    }
}

static D3D12_CPU_DESCRIPTOR_HANDLE current_rtv(void)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;

    memset(
        &handle,
        0,
        sizeof(handle)
    );

    if (!g_rtv_heap) {
        return handle;
    }

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
    if (!g_initialized) {
        return;
    }

    if (g_frame_started) {
        return;
    }

    if (!g_command_allocator ||
        !g_command_list ||
        !g_vertex_buffer ||
        !g_vertex_data) {
        return;
    }

    g_vertex_count = 0;

    if (FAILED(
        g_command_allocator->lpVtbl->Reset(
            g_command_allocator
        )
    )) {
        return;
    }

    if (FAILED(
        g_command_list->lpVtbl->Reset(
            g_command_list,
            g_command_allocator,
            g_triangle_pipeline_state
        )
    )) {
        return;
    }

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
    if (!g_frame_started) {
        return 0;
    }

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
    if (!g_frame_started) {
        return;
    }

    if (count == 0) {
        return;
    }

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

    if (!pipeline) {
        return;
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

static float color_r(
    DXGOColor color
)
{
    return (float)color.r / 255.0f;
}

static float color_g(
    DXGOColor color
)
{
    return (float)color.g / 255.0f;
}

static float color_b(
    DXGOColor color
)
{
    return (float)color.b / 255.0f;
}

static float color_a(
    DXGOColor color
)
{
    return (float)color.a / 255.0f;
}

int dxgo_native_init(
    const char* title,
    int width,
    int height
)
{
    if (g_initialized) {
        return 1;
    }

    if (width <= 0 ||
        height <= 0) {
        return 0;
    }

    g_width = width;
    g_height = height;

    memset(
        g_key_current,
        0,
        sizeof(g_key_current)
    );

    memset(
        g_key_previous,
        0,
        sizeof(g_key_previous)
    );

    g_vsync = 1;
    g_mouse_wheel_accum = 0;
    g_mouse_wheel_frame = 0.0f;

    if (!create_window(title)) {
        return 0;
    }

    if (!create_device()) {
        dxgo_native_close();
        return 0;
    }

    if (!create_swap_chain()) {
        dxgo_native_close();
        return 0;
    }

    if (!create_rtv_heap()) {
        dxgo_native_close();
        return 0;
    }

    if (!create_command_objects()) {
        dxgo_native_close();
        return 0;
    }

    if (!create_fence()) {
        dxgo_native_close();
        return 0;
    }

    if (!create_root_signature()) {
        dxgo_native_close();
        return 0;
    }

    if (!create_pipeline_states()) {
        dxgo_native_close();
        return 0;
    }

    if (!create_vertex_buffer()) {
        dxgo_native_close();
        return 0;
    }

    g_vertex_count = 0;
    g_frame_started = 0;
    g_fence_value = 0;

    g_initialized = 1;

    return 1;
}

int dxgo_native_open(void)
{
    MSG message;

    if (!g_initialized ||
        !g_hwnd ||
        !IsWindow(g_hwnd)) {
        return 0;
    }

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

    if (!g_hwnd ||
        !IsWindow(g_hwnd)) {
        return 0;
    }

    update_keyboard();

    g_mouse_wheel_frame =
        (float)g_mouse_wheel_accum /
        (float)WHEEL_DELTA;

    g_mouse_wheel_accum = 0;

    return 1;
}

void dxgo_native_clear(
    float r,
    float g,
    float b,
    float a
)
{
    if (!g_initialized) {
        return;
    }

    begin_frame();

    if (!g_frame_started) {
        return;
    }

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

    UINT start =
        g_vertex_count;

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

    UINT start =
        g_vertex_count;

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

    UINT start =
        g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    if (!add_vertex(
        x,
        y,
        r,
        g,
        b,
        a
    )) return;

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

    if (!add_vertex(
        x,
        y,
        r,
        g,
        b,
        a
    )) return;

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

    UINT start =
        g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    if (!add_vertex(
        x,
        y,
        r,
        g,
        b,
        a
    )) return;

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

    if (!add_vertex(
        x,
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

    UINT start =
        g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 0;
         i < CIRCLE_SEGMENTS;
         i++) {

        float angle1 =
            (float)i /
            CIRCLE_SEGMENTS *
            2.0f *
            DXGO_PI;

        float angle2 =
            (float)(i + 1) /
            CIRCLE_SEGMENTS *
            2.0f *
            DXGO_PI;

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

    UINT start =
        g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 0;
         i < CIRCLE_SEGMENTS;
         i++) {

        float angle1 =
            (float)i /
            CIRCLE_SEGMENTS *
            2.0f *
            DXGO_PI;

        float angle2 =
            (float)(i + 1) /
            CIRCLE_SEGMENTS *
            2.0f *
            DXGO_PI;

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

    UINT start =
        g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 0;
         i < CIRCLE_SEGMENTS;
         i++) {

        float angle1 =
            (float)i /
            CIRCLE_SEGMENTS *
            2.0f *
            DXGO_PI;

        float angle2 =
            (float)(i + 1) /
            CIRCLE_SEGMENTS *
            2.0f *
            DXGO_PI;

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

    UINT start =
        g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 0;
         i < CIRCLE_SEGMENTS;
         i++) {

        float angle1 =
            (float)i /
            CIRCLE_SEGMENTS *
            2.0f *
            DXGO_PI;

        float angle2 =
            (float)(i + 1) /
            CIRCLE_SEGMENTS *
            2.0f *
            DXGO_PI;

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

    UINT start =
        g_vertex_count;

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

    UINT start =
        g_vertex_count;

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
    if (!points ||
        count < 2) {
        return;
    }

    begin_frame();

    UINT start =
        g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 0;
         i < count;
         i++) {

        int next =
            (i + 1) % count;

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
        (UINT)count * 2
    );
}

void dxgo_native_fill_polygon(
    const float* points,
    int count,
    DXGOColor color
)
{
    if (!points ||
        count < 3) {
        return;
    }

    begin_frame();

    UINT start =
        g_vertex_count;

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    for (int i = 1;
         i < count - 1;
         i++) {

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
        (UINT)(count - 2) * 3
    );
}

void dxgo_native_update(void)
{
    if (!g_initialized) {
        return;
    }

    if (!g_frame_started) {
        begin_frame();
    }

    if (!g_frame_started) {
        return;
    }

    if (g_vertex_count > 0) {
        D3D12_RANGE read_range;

        read_range.Begin = 0;
        read_range.End = 0;

        void* mapped = NULL;

        HRESULT hr =
            g_vertex_buffer->lpVtbl->Map(
                g_vertex_buffer,
                0,
                &read_range,
                &mapped
            );

        if (SUCCEEDED(hr) &&
            mapped) {

            memcpy(
                mapped,
                g_vertex_data,
                sizeof(DXGOVertex) *
                g_vertex_count
            );

            g_vertex_buffer->lpVtbl->Unmap(
                g_vertex_buffer,
                0,
                NULL
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

    if (FAILED(
        g_command_list->lpVtbl->Close(
            g_command_list
        )
    )) {
        g_frame_started = 0;
        g_vertex_count = 0;
        return;
    }

    ID3D12CommandList* lists[1];

    lists[0] =
        (ID3D12CommandList*)g_command_list;

    g_command_queue->lpVtbl->ExecuteCommandLists(
        g_command_queue,
        1,
        lists
    );

    HRESULT present_result =
        g_swap_chain->lpVtbl->Present(
            g_swap_chain,
            g_vsync ? 1 : 0,
            0
        );

    wait_gpu();

    if (SUCCEEDED(present_result)) {
        g_frame_index =
            g_swap_chain->lpVtbl->GetCurrentBackBufferIndex(
                g_swap_chain
            );
    }

    g_vertex_count = 0;
    g_frame_started = 0;

    if (g_hwnd &&
        IsWindow(g_hwnd)) {
        UpdateWindow(g_hwnd);
    }
}

void dxgo_native_close(void)
{
    if (!g_hwnd &&
        !g_device &&
        !g_swap_chain) {
        g_initialized = 0;
        return;
    }

    if (!g_cursor_visible) {
        ShowCursor(TRUE);
        g_cursor_visible = 1;
    }

    if (g_frame_started &&
        g_command_list) {

        dxgo_native_update();
    }

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

    memset(
        &g_vertex_buffer_view,
        0,
        sizeof(g_vertex_buffer_view)
    );

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

    if (g_text_vertex_data) {
        free(g_text_vertex_data);
        g_text_vertex_data = NULL;
    }

    if (g_text_vertex_buffer) {
        g_text_vertex_buffer->lpVtbl->Release(g_text_vertex_buffer);
        g_text_vertex_buffer = NULL;
    }

    memset(
        &g_text_vertex_buffer_view,
        0,
        sizeof(g_text_vertex_buffer_view)
    );

    if (g_font_texture) {
        g_font_texture->lpVtbl->Release(g_font_texture);
        g_font_texture = NULL;
    }

    if (g_text_srv_heap) {
        g_text_srv_heap->lpVtbl->Release(g_text_srv_heap);
        g_text_srv_heap = NULL;
    }

    if (g_text_pipeline_state) {
        g_text_pipeline_state->lpVtbl->Release(g_text_pipeline_state);
        g_text_pipeline_state = NULL;
    }

    if (g_text_root_signature) {
        g_text_root_signature->lpVtbl->Release(g_text_root_signature);
        g_text_root_signature = NULL;
    }

    g_font_atlas_ready = 0;
    g_font_line_height = 0;

    memset(
        g_glyph_advance,
        0,
        sizeof(g_glyph_advance)
    );

    if (g_fence_event) {
        CloseHandle(
            g_fence_event
        );
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

    for (int i = 0;
         i < FRAME_COUNT;
         i++) {

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
        DestroyWindow(
            g_hwnd
        );
        g_hwnd = NULL;
    }

    g_vertex_count = 0;
    g_frame_started = 0;
    g_fence_value = 0;
    g_frame_index = 0;
    g_rtv_descriptor_size = 0;
    g_initialized = 0;

    g_vsync = 1;
    g_mouse_wheel_accum = 0;
    g_mouse_wheel_frame = 0.0f;

    memset(
        g_key_current,
        0,
        sizeof(g_key_current)
    );

    memset(
        g_key_previous,
        0,
        sizeof(g_key_previous)
    );
}

/* ---------------------------------------------------------------------
 * Text rendering.
 *
 * Text is rendered as GPU-drawn, texture-sampled quads rather than a
 * direct GDI overlay: the swap chain uses the D3D12-required flip
 * presentation model, and GDI drawing straight onto the window's DC is
 * not reliably composited on top of a flip-model swap chain. Instead, a
 * bitmap font atlas (printable ASCII 32-126) is rendered once with GDI
 * into an off-screen DIB, uploaded to a D3D12 texture, and every Text()
 * call afterwards draws textured quads through their own small pipeline
 * that is separate from (and restores) the shared colored-vertex
 * pipeline used by the other primitives. See the globals and
 * FONT_* defines near the top of this file.
 * ------------------------------------------------------------------- */

static int create_text_root_signature(void)
{
    D3D12_DESCRIPTOR_RANGE range;
    memset(&range, 0, sizeof(range));
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER parameters[2];
    memset(parameters, 0, sizeof(parameters));

    parameters[0].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants.ShaderRegister = 0;
    parameters[0].Constants.RegisterSpace = 0;
    parameters[0].Constants.Num32BitValues = 2;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    parameters[1].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    parameters[1].DescriptorTable.pDescriptorRanges = &range;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler;
    memset(&sampler, 0, sizeof(sampler));
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.NumParameters = 2;
    desc.pParameters = parameters;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
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
            signature->lpVtbl->GetBufferPointer(signature),
            signature->lpVtbl->GetBufferSize(signature),
            &IID_ID3D12RootSignature,
            (void**)&g_text_root_signature
        );

    signature->lpVtbl->Release(signature);

    return SUCCEEDED(hr);
}

static int create_text_pipeline_state(void)
{
    const char* shader_source =
        "struct VSInput {"
        "float2 position : POSITION;"
        "float2 uv : TEXCOORD;"
        "float4 color : COLOR;"
        "};"
        "struct VSOutput {"
        "float4 position : SV_POSITION;"
        "float2 uv : TEXCOORD;"
        "float4 color : COLOR;"
        "};"
        "cbuffer Constants : register(b0) {"
        "float screenWidth;"
        "float screenHeight;"
        "};"
        "Texture2D atlasTexture : register(t0);"
        "SamplerState atlasSampler : register(s0);"
        "VSOutput VSMain(VSInput input) {"
        "VSOutput output;"
        "float x = input.position.x / screenWidth * 2.0 - 1.0;"
        "float y = 1.0 - input.position.y / screenHeight * 2.0;"
        "output.position = float4(x, y, 0.0, 1.0);"
        "output.uv = input.uv;"
        "output.color = input.color;"
        "return output;"
        "}"
        "float4 PSMain(VSOutput input) : SV_TARGET {"
        "float4 tex = atlasTexture.Sample(atlasSampler, input.uv);"
        "return float4(input.color.rgb * tex.rgb, input.color.a * tex.a);"
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
        vertex_shader->lpVtbl->Release(vertex_shader);
        return 0;
    }

    D3D12_INPUT_ELEMENT_DESC input_layout[3];
    memset(input_layout, 0, sizeof(input_layout));

    input_layout[0].SemanticName = "POSITION";
    input_layout[0].Format = DXGI_FORMAT_R32G32_FLOAT;
    input_layout[0].AlignedByteOffset = 0;
    input_layout[0].InputSlotClass =
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    input_layout[1].SemanticName = "TEXCOORD";
    input_layout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    input_layout[1].AlignedByteOffset = 8;
    input_layout[1].InputSlotClass =
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    input_layout[2].SemanticName = "COLOR";
    input_layout[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    input_layout[2].AlignedByteOffset = 16;
    input_layout[2].InputSlotClass =
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    D3D12_RASTERIZER_DESC rasterizer;
    memset(&rasterizer, 0, sizeof(rasterizer));
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.DepthClipEnable = TRUE;

    D3D12_BLEND_DESC blend;
    memset(&blend, 0, sizeof(blend));
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC depth;
    memset(&depth, 0, sizeof(depth));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.pRootSignature = g_text_root_signature;
    desc.VS.pShaderBytecode =
        vertex_shader->lpVtbl->GetBufferPointer(vertex_shader);
    desc.VS.BytecodeLength =
        vertex_shader->lpVtbl->GetBufferSize(vertex_shader);
    desc.PS.pShaderBytecode =
        pixel_shader->lpVtbl->GetBufferPointer(pixel_shader);
    desc.PS.BytecodeLength =
        pixel_shader->lpVtbl->GetBufferSize(pixel_shader);
    desc.BlendState = blend;
    desc.SampleMask = 0xffffffff;
    desc.RasterizerState = rasterizer;
    desc.DepthStencilState = depth;
    desc.InputLayout.pInputElementDescs = input_layout;
    desc.InputLayout.NumElements = 3;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    HRESULT hr =
        g_device->lpVtbl->CreateGraphicsPipelineState(
            g_device,
            &desc,
            &IID_ID3D12PipelineState,
            (void**)&g_text_pipeline_state
        );

    pixel_shader->lpVtbl->Release(pixel_shader);
    vertex_shader->lpVtbl->Release(vertex_shader);

    return SUCCEEDED(hr);
}

static int create_text_vertex_buffer(void)
{
    D3D12_HEAP_PROPERTIES heap_properties;
    memset(&heap_properties, 0, sizeof(heap_properties));
    heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeof(DXGOTextVertex) * MAX_TEXT_VERTICES;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr =
        g_device->lpVtbl->CreateCommittedResource(
            g_device,
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            &IID_ID3D12Resource,
            (void**)&g_text_vertex_buffer
        );

    if (FAILED(hr)) {
        return 0;
    }

    g_text_vertex_data =
        (DXGOTextVertex*)malloc(
            sizeof(DXGOTextVertex) * MAX_TEXT_VERTICES
        );

    if (!g_text_vertex_data) {
        return 0;
    }

    g_text_vertex_buffer_view.BufferLocation =
        g_text_vertex_buffer->lpVtbl->GetGPUVirtualAddress(
            g_text_vertex_buffer
        );

    g_text_vertex_buffer_view.StrideInBytes =
        sizeof(DXGOTextVertex);

    g_text_vertex_buffer_view.SizeInBytes =
        sizeof(DXGOTextVertex) * MAX_TEXT_VERTICES;

    return 1;
}

static int upload_font_texture(const unsigned char* rgba)
{
    D3D12_HEAP_PROPERTIES default_heap;
    memset(&default_heap, 0, sizeof(default_heap));
    default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC tex_desc;
    memset(&tex_desc, 0, sizeof(tex_desc));
    tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex_desc.Width = FONT_ATLAS_W;
    tex_desc.Height = FONT_ATLAS_H;
    tex_desc.DepthOrArraySize = 1;
    tex_desc.MipLevels = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    tex_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr =
        g_device->lpVtbl->CreateCommittedResource(
            g_device,
            &default_heap,
            D3D12_HEAP_FLAG_NONE,
            &tex_desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            NULL,
            &IID_ID3D12Resource,
            (void**)&g_font_texture
        );

    if (FAILED(hr)) {
        return 0;
    }

    UINT row_pitch = ((UINT)FONT_ATLAS_W * 4 + 255) & ~(UINT)255;
    UINT upload_size = row_pitch * (UINT)FONT_ATLAS_H;

    D3D12_HEAP_PROPERTIES upload_heap;
    memset(&upload_heap, 0, sizeof(upload_heap));
    upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC buf_desc;
    memset(&buf_desc, 0, sizeof(buf_desc));
    buf_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buf_desc.Width = upload_size;
    buf_desc.Height = 1;
    buf_desc.DepthOrArraySize = 1;
    buf_desc.MipLevels = 1;
    buf_desc.Format = DXGI_FORMAT_UNKNOWN;
    buf_desc.SampleDesc.Count = 1;
    buf_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    buf_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource* staging = NULL;

    hr =
        g_device->lpVtbl->CreateCommittedResource(
            g_device,
            &upload_heap,
            D3D12_HEAP_FLAG_NONE,
            &buf_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            &IID_ID3D12Resource,
            (void**)&staging
        );

    if (FAILED(hr)) {
        g_font_texture->lpVtbl->Release(g_font_texture);
        g_font_texture = NULL;
        return 0;
    }

    D3D12_RANGE no_read;
    no_read.Begin = 0;
    no_read.End = 0;

    void* mapped = NULL;

    hr = staging->lpVtbl->Map(staging, 0, &no_read, &mapped);

    if (FAILED(hr) || !mapped) {
        staging->lpVtbl->Release(staging);
        g_font_texture->lpVtbl->Release(g_font_texture);
        g_font_texture = NULL;
        return 0;
    }

    for (int y = 0; y < FONT_ATLAS_H; y++) {
        memcpy(
            (unsigned char*)mapped + (size_t)y * row_pitch,
            rgba + (size_t)y * FONT_ATLAS_W * 4,
            (size_t)FONT_ATLAS_W * 4
        );
    }

    staging->lpVtbl->Unmap(staging, 0, NULL);

    ID3D12CommandAllocator* upload_allocator = NULL;

    hr =
        g_device->lpVtbl->CreateCommandAllocator(
            g_device,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            &IID_ID3D12CommandAllocator,
            (void**)&upload_allocator
        );

    if (FAILED(hr)) {
        staging->lpVtbl->Release(staging);
        g_font_texture->lpVtbl->Release(g_font_texture);
        g_font_texture = NULL;
        return 0;
    }

    ID3D12GraphicsCommandList* upload_list = NULL;

    hr =
        g_device->lpVtbl->CreateCommandList(
            g_device,
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            upload_allocator,
            NULL,
            &IID_ID3D12GraphicsCommandList,
            (void**)&upload_list
        );

    if (FAILED(hr)) {
        upload_allocator->lpVtbl->Release(upload_allocator);
        staging->lpVtbl->Release(staging);
        g_font_texture->lpVtbl->Release(g_font_texture);
        g_font_texture = NULL;
        return 0;
    }

    D3D12_TEXTURE_COPY_LOCATION dst_loc;
    memset(&dst_loc, 0, sizeof(dst_loc));
    dst_loc.pResource = g_font_texture;
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_loc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src_loc;
    memset(&src_loc, 0, sizeof(src_loc));
    src_loc.pResource = staging;
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_loc.PlacedFootprint.Offset = 0;
    src_loc.PlacedFootprint.Footprint.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;
    src_loc.PlacedFootprint.Footprint.Width = FONT_ATLAS_W;
    src_loc.PlacedFootprint.Footprint.Height = FONT_ATLAS_H;
    src_loc.PlacedFootprint.Footprint.Depth = 1;
    src_loc.PlacedFootprint.Footprint.RowPitch = row_pitch;

    upload_list->lpVtbl->CopyTextureRegion(
        upload_list,
        &dst_loc,
        0,
        0,
        0,
        &src_loc,
        NULL
    );

    D3D12_RESOURCE_BARRIER barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = g_font_texture;
    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    upload_list->lpVtbl->ResourceBarrier(upload_list, 1, &barrier);

    upload_list->lpVtbl->Close(upload_list);

    ID3D12CommandList* lists[1];
    lists[0] = (ID3D12CommandList*)upload_list;

    g_command_queue->lpVtbl->ExecuteCommandLists(
        g_command_queue,
        1,
        lists
    );

    wait_gpu();

    upload_list->lpVtbl->Release(upload_list);
    upload_allocator->lpVtbl->Release(upload_allocator);
    staging->lpVtbl->Release(staging);

    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc;
    memset(&srv_heap_desc, 0, sizeof(srv_heap_desc));
    srv_heap_desc.NumDescriptors = 1;
    srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    hr =
        g_device->lpVtbl->CreateDescriptorHeap(
            g_device,
            &srv_heap_desc,
            &IID_ID3D12DescriptorHeap,
            (void**)&g_text_srv_heap
        );

    if (FAILED(hr)) {
        g_font_texture->lpVtbl->Release(g_font_texture);
        g_font_texture = NULL;
        return 0;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
    memset(&srv_desc, 0, sizeof(srv_desc));
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE srv_handle;
    g_text_srv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(
        g_text_srv_heap,
        &srv_handle
    );

    g_device->lpVtbl->CreateShaderResourceView(
        g_device,
        g_font_texture,
        &srv_desc,
        srv_handle
    );

    return 1;
}

static int build_font_atlas(void)
{
    HDC mem_dc = CreateCompatibleDC(NULL);

    if (!mem_dc) {
        return 0;
    }

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = FONT_ATLAS_W;
    bmi.bmiHeader.biHeight = -FONT_ATLAS_H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dib_pixels = NULL;

    HBITMAP dib =
        CreateDIBSection(
            mem_dc,
            &bmi,
            DIB_RGB_COLORS,
            &dib_pixels,
            NULL,
            0
        );

    if (!dib || !dib_pixels) {
        DeleteDC(mem_dc);
        return 0;
    }

    HBITMAP old_bitmap =
        (HBITMAP)SelectObject(mem_dc, dib);

    RECT full_rect;
    full_rect.left = 0;
    full_rect.top = 0;
    full_rect.right = FONT_ATLAS_W;
    full_rect.bottom = FONT_ATLAS_H;

    FillRect(
        mem_dc,
        &full_rect,
        (HBRUSH)GetStockObject(BLACK_BRUSH)
    );

    HFONT font =
        CreateFontA(
            -FONT_REFERENCE_SIZE,
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
            ANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "Segoe UI"
        );

    if (!font) {
        SelectObject(mem_dc, old_bitmap);
        DeleteObject(dib);
        DeleteDC(mem_dc);
        return 0;
    }

    HFONT old_font =
        (HFONT)SelectObject(mem_dc, font);

    SetBkMode(mem_dc, TRANSPARENT);
    SetTextColor(mem_dc, RGB(255, 255, 255));

    TEXTMETRICA metrics;
    GetTextMetricsA(mem_dc, &metrics);
    g_font_line_height = (int)metrics.tmHeight;

    for (int i = 0; i < FONT_GLYPH_COUNT; i++) {
        char ch = (char)(FONT_GLYPH_FIRST + i);
        int col = i % FONT_ATLAS_COLS;
        int row = i / FONT_ATLAS_COLS;

        TextOutA(
            mem_dc,
            col * FONT_CELL_W,
            row * FONT_CELL_H,
            &ch,
            1
        );

        INT width = 0;

        GetCharWidth32A(
            mem_dc,
            (UINT)(unsigned char)ch,
            (UINT)(unsigned char)ch,
            &width
        );

        g_glyph_advance[i] = (float)width;
    }

    SelectObject(mem_dc, old_font);
    DeleteObject(font);

    unsigned char* rgba =
        (unsigned char*)malloc(
            (size_t)FONT_ATLAS_W * (size_t)FONT_ATLAS_H * 4
        );

    if (!rgba) {
        SelectObject(mem_dc, old_bitmap);
        DeleteObject(dib);
        DeleteDC(mem_dc);
        return 0;
    }

    unsigned char* src = (unsigned char*)dib_pixels;

    for (int p = 0; p < FONT_ATLAS_W * FONT_ATLAS_H; p++) {
        unsigned char coverage = src[p * 4 + 2];

        rgba[p * 4 + 0] = 255;
        rgba[p * 4 + 1] = 255;
        rgba[p * 4 + 2] = 255;
        rgba[p * 4 + 3] = coverage;
    }

    SelectObject(mem_dc, old_bitmap);
    DeleteObject(dib);
    DeleteDC(mem_dc);

    int ok = upload_font_texture(rgba);

    free(rgba);

    return ok;
}

static int ensure_font_atlas(void)
{
    if (g_font_atlas_ready) {
        return 1;
    }

    if (!g_device || !g_command_queue) {
        return 0;
    }

    if (!create_text_root_signature()) {
        return 0;
    }

    if (!create_text_pipeline_state()) {
        return 0;
    }

    if (!create_text_vertex_buffer()) {
        return 0;
    }

    if (!build_font_atlas()) {
        return 0;
    }

    g_font_atlas_ready = 1;

    return 1;
}

static void measure_text_internal(
    const char* text,
    int size,
    int* out_width,
    int* out_height
)
{
    float scale = (float)size / (float)FONT_REFERENCE_SIZE;
    float width = 0;
    float line_width = 0;
    int lines = 1;

    for (const char* p = text; *p; p++) {
        if (*p == '\n') {
            if (line_width > width) {
                width = line_width;
            }
            line_width = 0;
            lines++;
            continue;
        }

        int index = (unsigned char)*p - FONT_GLYPH_FIRST;

        if (index < 0 || index >= FONT_GLYPH_COUNT) {
            continue;
        }

        line_width += g_glyph_advance[index] * scale;
    }

    if (line_width > width) {
        width = line_width;
    }

    if (out_width) {
        *out_width = (int)(width + 0.5f);
    }

    if (out_height) {
        *out_height =
            (int)((float)g_font_line_height * scale * (float)lines + 0.5f);
    }
}

void dxgo_native_draw_text(
    const char* text,
    float x,
    float y,
    int size,
    DXGOColor color
)
{
    if (!text || size <= 0) {
        return;
    }

    begin_frame();

    if (!g_frame_started) {
        return;
    }

    if (!ensure_font_atlas()) {
        return;
    }

    float r = color_r(color);
    float g = color_g(color);
    float b = color_b(color);
    float a = color_a(color);

    float scale = (float)size / (float)FONT_REFERENCE_SIZE;
    float cursor_x = x;
    float cursor_y = y;
    float line_height = (float)g_font_line_height * scale;

    UINT vertex_count = 0;

    for (const char* p = text; *p; p++) {
        if (*p == '\n') {
            cursor_x = x;
            cursor_y += line_height;
            continue;
        }

        int index = (unsigned char)*p - FONT_GLYPH_FIRST;

        if (index < 0 || index >= FONT_GLYPH_COUNT) {
            continue;
        }

        if (vertex_count + 6 > MAX_TEXT_VERTICES) {
            break;
        }

        int col = index % FONT_ATLAS_COLS;
        int row = index / FONT_ATLAS_COLS;

        float u0 = (float)(col * FONT_CELL_W) / (float)FONT_ATLAS_W;
        float v0 = (float)(row * FONT_CELL_H) / (float)FONT_ATLAS_H;
        float u1 =
            (float)(col * FONT_CELL_W + FONT_CELL_W) / (float)FONT_ATLAS_W;
        float v1 =
            (float)(row * FONT_CELL_H + FONT_CELL_H) / (float)FONT_ATLAS_H;

        float qx0 = cursor_x;
        float qy0 = cursor_y;
        float qx1 = cursor_x + (float)FONT_CELL_W * scale;
        float qy1 = cursor_y + (float)FONT_CELL_H * scale;

        DXGOTextVertex* v = &g_text_vertex_data[vertex_count];

        v[0].x = qx0; v[0].y = qy0; v[0].u = u0; v[0].v = v0;
        v[1].x = qx1; v[1].y = qy0; v[1].u = u1; v[1].v = v0;
        v[2].x = qx1; v[2].y = qy1; v[2].u = u1; v[2].v = v1;
        v[3].x = qx0; v[3].y = qy0; v[3].u = u0; v[3].v = v0;
        v[4].x = qx1; v[4].y = qy1; v[4].u = u1; v[4].v = v1;
        v[5].x = qx0; v[5].y = qy1; v[5].u = u0; v[5].v = v1;

        for (int i = 0; i < 6; i++) {
            v[i].r = r;
            v[i].g = g;
            v[i].b = b;
            v[i].a = a;
        }

        vertex_count += 6;

        cursor_x += g_glyph_advance[index] * scale;
    }

    if (vertex_count == 0) {
        return;
    }

    D3D12_RANGE read_range;
    read_range.Begin = 0;
    read_range.End = 0;

    void* mapped = NULL;

    if (FAILED(g_text_vertex_buffer->lpVtbl->Map(
        g_text_vertex_buffer,
        0,
        &read_range,
        &mapped
    )) || !mapped) {
        return;
    }

    memcpy(
        mapped,
        g_text_vertex_data,
        sizeof(DXGOTextVertex) * vertex_count
    );

    g_text_vertex_buffer->lpVtbl->Unmap(g_text_vertex_buffer, 0, NULL);

    D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu_handle;
    g_text_srv_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(
        g_text_srv_heap,
        &srv_gpu_handle
    );

    ID3D12DescriptorHeap* heaps[1];
    heaps[0] = g_text_srv_heap;

    g_command_list->lpVtbl->SetDescriptorHeaps(g_command_list, 1, heaps);

    g_command_list->lpVtbl->SetPipelineState(
        g_command_list,
        g_text_pipeline_state
    );

    g_command_list->lpVtbl->SetGraphicsRootSignature(
        g_command_list,
        g_text_root_signature
    );

    float constants[2];
    constants[0] = (float)g_width;
    constants[1] = (float)g_height;

    g_command_list->lpVtbl->SetGraphicsRoot32BitConstants(
        g_command_list,
        0,
        2,
        constants,
        0
    );

    g_command_list->lpVtbl->SetGraphicsRootDescriptorTable(
        g_command_list,
        1,
        srv_gpu_handle
    );

    g_command_list->lpVtbl->IASetVertexBuffers(
        g_command_list,
        0,
        1,
        &g_text_vertex_buffer_view
    );

    g_command_list->lpVtbl->IASetPrimitiveTopology(
        g_command_list,
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    g_command_list->lpVtbl->DrawInstanced(
        g_command_list,
        vertex_count,
        1,
        0,
        0
    );

    /* Restore the shared colored-vertex pipeline's bindings so any
     * shape draw calls issued after this Text() call within the same
     * frame keep working (they rely on the bindings begin_frame() set
     * up and do not re-bind the root signature/vertex buffer
     * themselves). */
    g_command_list->lpVtbl->SetGraphicsRootSignature(
        g_command_list,
        g_root_signature
    );

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
}

static void update_keyboard(void)
{
    memcpy(
        g_key_previous,
        g_key_current,
        sizeof(g_key_current)
    );

    for (int i = 0;
         i < 256;
         i++) {

        SHORT state =
            GetAsyncKeyState(i);

        g_key_current[i] =
            (state & 0x8000) != 0;
    }
}

int dxgo_native_key_down(
    int key
)
{
    if (key < 0 ||
        key >= 256) {
        return 0;
    }

    return g_key_current[key] != 0;
}

int dxgo_native_key_pressed(
    int key
)
{
    if (key < 0 ||
        key >= 256) {
        return 0;
    }

    return
        g_key_current[key] != 0 &&
        g_key_previous[key] == 0;
}

int dxgo_native_key_released(
    int key
)
{
    if (key < 0 ||
        key >= 256) {
        return 0;
    }

    return
        g_key_current[key] == 0 &&
        g_key_previous[key] != 0;
}

void dxgo_native_set_title(
    const char* title
)
{
    if (!g_hwnd || !title) {
        return;
    }

    SetWindowTextA(
        g_hwnd,
        title
    );
}

void dxgo_native_show_cursor(
    int show
)
{
    int visible =
        show != 0;

    if (visible == g_cursor_visible) {
        return;
    }

    ShowCursor(visible);

    g_cursor_visible = visible;
}

int dxgo_native_is_focused(void)
{
    if (!g_hwnd) {
        return 0;
    }

    return
        GetForegroundWindow() == g_hwnd;
}

void dxgo_native_set_vsync(
    int enabled
)
{
    g_vsync =
        enabled != 0;
}

float dxgo_native_mouse_x(void)
{
    if (!g_hwnd) {
        return 0.0f;
    }

    POINT point;

    if (!GetCursorPos(&point)) {
        return 0.0f;
    }

    if (!ScreenToClient(
        g_hwnd,
        &point
    )) {
        return 0.0f;
    }

    return (float)point.x;
}

float dxgo_native_mouse_y(void)
{
    if (!g_hwnd) {
        return 0.0f;
    }

    POINT point;

    if (!GetCursorPos(&point)) {
        return 0.0f;
    }

    if (!ScreenToClient(
        g_hwnd,
        &point
    )) {
        return 0.0f;
    }

    return (float)point.y;
}

float dxgo_native_mouse_wheel(void)
{
    return g_mouse_wheel_frame;
}

void dxgo_native_measure_text(
    const char* text,
    int size,
    int* out_width,
    int* out_height
)
{
    if (out_width) {
        *out_width = 0;
    }

    if (out_height) {
        *out_height = 0;
    }

    if (!text || size <= 0) {
        return;
    }

    if (!ensure_font_atlas()) {
        return;
    }

    measure_text_internal(text, size, out_width, out_height);
}