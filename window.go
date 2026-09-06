package dxgo

func Open() bool {
	return nativeOpen()
}

func Update() {
	nativeUpdate()
}

func Close() {
	nativeClose()
}