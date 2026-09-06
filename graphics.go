package dxgo

/*
#include "internal/native/native.h"
*/
import "C"

func Clear() {
	C.dxgo_native_clear(
		0,
		0,
		0,
		1,
	)
}