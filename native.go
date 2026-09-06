package dxgo

/*
#cgo windows LDFLAGS: -ld3d12 -ldxgi -ld3dcompiler_47 -luser32 -lgdi32 -luuid
#include <stdlib.h>
#include "native.h"
*/
import "C"

import "unsafe"

func nativeInit(title string, width, height int) bool {
	cTitle := C.CString(title)
	defer C.free(unsafe.Pointer(cTitle))

	return C.dxgo_native_init(
		cTitle,
		C.int(width),
		C.int(height),
	) != 0
}

func nativeOpen() bool {
	return C.dxgo_native_open() != 0
}

func nativeUpdate() {
	C.dxgo_native_update()
}

func nativeClose() {
	C.dxgo_native_close()
}

func nativeClear(r, g, b, a float32) {
	C.dxgo_native_clear(
		C.float(r),
		C.float(g),
		C.float(b),
		C.float(a),
	)
}

func nativeColor(c Color) C.DXGOColor {
	return C.DXGOColor{
		r: C.uchar(c.R),
		g: C.uchar(c.G),
		b: C.uchar(c.B),
		a: C.uchar(c.A),
	}
}

func nativeDrawPoint(x, y float32, color Color) {
	C.dxgo_native_draw_point(
		C.float(x),
		C.float(y),
		nativeColor(color),
	)
}

func nativeDrawLine(x1, y1, x2, y2 float32, color Color) {
	C.dxgo_native_draw_line(
		C.float(x1),
		C.float(y1),
		C.float(x2),
		C.float(y2),
		nativeColor(color),
	)
}

func nativeDrawRect(x, y, width, height float32, color Color) {
	C.dxgo_native_draw_rect(
		C.float(x),
		C.float(y),
		C.float(width),
		C.float(height),
		nativeColor(color),
	)
}

func nativeFillRect(x, y, width, height float32, color Color) {
	C.dxgo_native_fill_rect(
		C.float(x),
		C.float(y),
		C.float(width),
		C.float(height),
		nativeColor(color),
	)
}

func nativeDrawCircle(x, y, radius float32, color Color) {
	C.dxgo_native_draw_circle(
		C.float(x),
		C.float(y),
		C.float(radius),
		nativeColor(color),
	)
}

func nativeFillCircle(x, y, radius float32, color Color) {
	C.dxgo_native_fill_circle(
		C.float(x),
		C.float(y),
		C.float(radius),
		nativeColor(color),
	)
}

func nativeDrawEllipse(x, y, rx, ry float32, color Color) {
	C.dxgo_native_draw_ellipse(
		C.float(x),
		C.float(y),
		C.float(rx),
		C.float(ry),
		nativeColor(color),
	)
}

func nativeFillEllipse(x, y, rx, ry float32, color Color) {
	C.dxgo_native_fill_ellipse(
		C.float(x),
		C.float(y),
		C.float(rx),
		C.float(ry),
		nativeColor(color),
	)
}

func nativeDrawTriangle(
	x1, y1,
	x2, y2,
	x3, y3 float32,
	color Color,
) {
	C.dxgo_native_draw_triangle(
		C.float(x1),
		C.float(y1),
		C.float(x2),
		C.float(y2),
		C.float(x3),
		C.float(y3),
		nativeColor(color),
	)
}

func nativeFillTriangle(
	x1, y1,
	x2, y2,
	x3, y3 float32,
	color Color,
) {
	C.dxgo_native_fill_triangle(
		C.float(x1),
		C.float(y1),
		C.float(x2),
		C.float(y2),
		C.float(x3),
		C.float(y3),
		nativeColor(color),
	)
}

func nativeDrawPolygon(points []float32, color Color) {
	if len(points) < 4 || len(points)%2 != 0 {
		return
	}

	C.dxgo_native_draw_polygon(
		(*C.float)(unsafe.Pointer(&points[0])),
		C.int(len(points) / 2),
		nativeColor(color),
	)
}

func nativeFillPolygon(points []float32, color Color) {
	if len(points) < 6 || len(points)%2 != 0 {
		return
	}

	C.dxgo_native_fill_polygon(
		(*C.float)(unsafe.Pointer(&points[0])),
		C.int(len(points) / 2),
		nativeColor(color),
	)
}

func nativeDrawText(text string, x, y float32, size int, color Color) {
	cText := C.CString(text)
	defer C.free(unsafe.Pointer(cText))

	C.dxgo_native_draw_text(
		cText,
		C.float(x),
		C.float(y),
		C.int(size),
		nativeColor(color),
	)
}

func nativeKeyDown(key int) bool {
	return C.dxgo_native_key_down(C.int(key)) != 0
}

func nativeKeyPressed(key int) bool {
	return C.dxgo_native_key_pressed(C.int(key)) != 0
}

func nativeKeyReleased(key int) bool {
	return C.dxgo_native_key_released(C.int(key)) != 0
}