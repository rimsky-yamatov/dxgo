#ifndef DXGO_NATIVE_H
#define DXGO_NATIVE_H

#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} DXGOColor;

int dxgo_native_init(const char* title, int width, int height);
int dxgo_native_open(void);
void dxgo_native_update(void);
void dxgo_native_close(void);

void dxgo_native_clear(float r, float g, float b, float a);

void dxgo_native_draw_point(
    float x,
    float y,
    DXGOColor color
);

void dxgo_native_draw_line(
    float x1,
    float y1,
    float x2,
    float y2,
    DXGOColor color
);

void dxgo_native_draw_rect(
    float x,
    float y,
    float width,
    float height,
    DXGOColor color
);

void dxgo_native_fill_rect(
    float x,
    float y,
    float width,
    float height,
    DXGOColor color
);

void dxgo_native_draw_circle(
    float x,
    float y,
    float radius,
    DXGOColor color
);

void dxgo_native_fill_circle(
    float x,
    float y,
    float radius,
    DXGOColor color
);

void dxgo_native_draw_ellipse(
    float x,
    float y,
    float rx,
    float ry,
    DXGOColor color
);

void dxgo_native_fill_ellipse(
    float x,
    float y,
    float rx,
    float ry,
    DXGOColor color
);

void dxgo_native_draw_triangle(
    float x1,
    float y1,
    float x2,
    float y2,
    float x3,
    float y3,
    DXGOColor color
);

void dxgo_native_fill_triangle(
    float x1,
    float y1,
    float x2,
    float y2,
    float x3,
    float y3,
    DXGOColor color
);

void dxgo_native_draw_polygon(
    const float* points,
    int count,
    DXGOColor color
);

void dxgo_native_fill_polygon(
    const float* points,
    int count,
    DXGOColor color
);

void dxgo_native_draw_text(
    const char* text,
    float x,
    float y,
    int size,
    DXGOColor color
);

int dxgo_native_key_down(int key);
int dxgo_native_key_pressed(int key);
int dxgo_native_key_released(int key);

void dxgo_native_set_title(const char* title);
void dxgo_native_show_cursor(int show);
int dxgo_native_is_focused(void);
void dxgo_native_set_vsync(int enabled);

float dxgo_native_mouse_x(void);
float dxgo_native_mouse_y(void);
float dxgo_native_mouse_wheel(void);

void dxgo_native_measure_text(
    const char* text,
    int size,
    int* out_width,
    int* out_height
);

#endif