package dxgo

func DrawText(text string, x, y float32, size int, color Color) {
	if text == "" || size <= 0 {
		return
	}

	nativeDrawText(text, x, y, size, color)
}