package dxgo

func (w *GameWindow) Text(text string, x, y float32, size int, color Color) {
	if text == "" || size <= 0 {
		return
	}

	nativeDrawText(text, x, y, size, color)
}

// MeasureText returns the width and height, in pixels, that text would
// occupy if drawn with Text() at the given size. Useful for centering or
// right-aligning text before drawing it.
func (w *GameWindow) MeasureText(text string, size int) (width, height int) {
	if text == "" || size <= 0 {
		return 0, 0
	}

	return nativeMeasureText(text, size)
}
