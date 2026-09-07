package dxgo

func (w *GameWindow) Text(text string, x, y float32, size int, color Color) {
	if text == "" || size <= 0 {
		return
	}

	nativeDrawText(text, x, y, size, color)
}