package dxgo

/*
#include "native.h"
*/
import "C"

func Open() bool {
	return C.dxgo_native_open() != 0
}

func Close() {
	C.dxgo_native_close()
}

func Update() {
	C.dxgo_native_update()
}