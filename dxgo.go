package dxgo

/*
#cgo windows LDFLAGS: -ld3d12 -ldxgi -luser32 -lgdi32
#include "internal/native/native.h"
*/
import "C"

func Window(title string, width, height int) {
	cTitle := C.CString(title)
	defer C.free(unsafe.Pointer(cTitle))

	C.dxgo_native_init(
		cTitle,
		C.int(width),
		C.int(height),
	)
}