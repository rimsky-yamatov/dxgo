package dxgo

type GameWindow struct{}

func Window(title string, width, height int) *GameWindow {
	if !nativeInit(title, width, height) {
		return nil
	}
	return &GameWindow{}
}

func (w *GameWindow) Open() bool {
	return nativeOpen()
}

func (w *GameWindow) Update() {
	nativeUpdate()
}

func (w *GameWindow) Clear() {
	nativeClear(0, 0, 0, 1)
}

func (w *GameWindow) Close() {
	nativeClose()
}