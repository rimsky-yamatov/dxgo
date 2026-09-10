package dxgo

type Color struct {
	R uint8
	G uint8
	B uint8
	A uint8
}

func RGB(r, g, b uint8) Color {
	return Color{
		R: r,
		G: g,
		B: b,
		A: 255,
	}
}

func RGBA(r, g, b, a uint8) Color {
	return Color{
		R: r,
		G: g,
		B: b,
		A: a,
	}
}

func (w *GameWindow) Point(x, y float32, color Color) {
	nativeDrawPoint(x, y, color)
}

func (w *GameWindow) Line(x1, y1, x2, y2 float32, color Color) {
	nativeDrawLine(x1, y1, x2, y2, color)
}

func (w *GameWindow) DrawRect(x, y, width, height float32, color Color) {
	nativeDrawRect(x, y, width, height, color)
}

func (w *GameWindow) FillRect(x, y, width, height float32, color Color) {
	nativeFillRect(x, y, width, height, color)
}

func (w *GameWindow) Circle(x, y, radius float32, color Color) {
	nativeDrawCircle(x, y, radius, color)
}

func (w *GameWindow) FillCircle(x, y, radius float32, color Color) {
	nativeFillCircle(x, y, radius, color)
}

func (w *GameWindow) Ellipse(x, y, rx, ry float32, color Color) {
	nativeDrawEllipse(x, y, rx, ry, color)
}

func (w *GameWindow) FillEllipse(x, y, rx, ry float32, color Color) {
	nativeFillEllipse(x, y, rx, ry, color)
}

func (w *GameWindow) Triangle(x1, y1, x2, y2, x3, y3 float32, color Color) {
	nativeDrawTriangle(x1, y1, x2, y2, x3, y3, color)
}

func (w *GameWindow) FillTriangle(x1, y1, x2, y2, x3, y3 float32, color Color) {
	nativeFillTriangle(x1, y1, x2, y2, x3, y3, color)
}

func (w *GameWindow) Polygon(points []float32, color Color) {
	if len(points) < 4 || len(points)%2 != 0 {
		return
	}

	nativeDrawPolygon(points, color)
}

func (w *GameWindow) FillPolygon(points []float32, color Color) {
	if len(points) < 6 || len(points)%2 != 0 {
		return
	}

	nativeFillPolygon(points, color)
}
