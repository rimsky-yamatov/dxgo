package dxgo

func Window(title string, width, height int) {
	if !nativeInit(title, width, height) {
		panic("dxgo: failed to initialize")
	}
}