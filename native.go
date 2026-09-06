package dxgo

/*
#cgo windows LDFLAGS: -ld3d12 -ldxgi -luser32 -lgdi32
#include <stdlib.h>
#include "native.h"
*/
import "C"

import "unsafe"

func nativeInit(title string, width, height int) bool {
	cTitle := C.CString(title)
	defer C.free(unsafe.Pointer(cTitle))
	return C.dxgo_native_init(cTitle, C.int(width), C.int(height)) != 0
}

func nativeOpen() bool {
	return C.dxgo_native_open() != 0
}

func nativeClear(r, g, b, a float32) {
	C.dxgo_native_clear(C.float(r), C.float(g), C.float(b), C.float(a))
}

func nativeUpdate() {
	C.dxgo_native_update()
}

func nativeClose() {
	C.dxgo_native_close()
}