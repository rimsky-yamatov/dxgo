package dxgo

type Color struct {
	R uint8
	G uint8
	B uint8
	A uint8
}

func RGB(r, g, b uint8) Color {
	return Color{R: r, G: g, B: b, A: 255}
}

func RGBA(r, g, b, a uint8) Color {
	return Color{R: r, G: g, B: b, A: a}
}

func Clear() {
	nativeClear(0, 0, 0, 1)
}

func DrawPoint(x, y float32, color Color) {
	nativeDrawPoint(x, y, color)
}

func DrawLine(x1, y1, x2, y2 float32, color Color) {
	nativeDrawLine(x1, y1, x2, y2, color)
}

func DrawRect(x, y, width, height float32, color Color) {
	nativeDrawRect(x, y, width, height, color)
}

func FillRect(x, y, width, height float32, color Color) {
	nativeFillRect(x, y, width, height, color)
}

func DrawCircle(x, y, radius float32, color Color) {
	nativeDrawCircle(x, y, radius, color)
}

func FillCircle(x, y, radius float32, color Color) {
	nativeFillCircle(x, y, radius, color)
}

func DrawEllipse(x, y, rx, ry float32, color Color) {
	nativeDrawEllipse(x, y, rx, ry, color)
}

func FillEllipse(x, y, rx, ry float32, color Color) {
	nativeFillEllipse(x, y, rx, ry, color)
}

func DrawTriangle(x1, y1, x2, y2, x3, y3 float32, color Color) {
	nativeDrawTriangle(x1, y1, x2, y2, x3, y3, color)
}

func FillTriangle(x1, y1, x2, y2, x3, y3 float32, color Color) {
	nativeFillTriangle(x1, y1, x2, y2, x3, y3, color)
}

func DrawPolygon(points []float32, color Color) {
	if len(points) < 4 || len(points)%2 != 0 {
		return
	}
	nativeDrawPolygon(points, color)
}

func FillPolygon(points []float32, color Color) {
	if len(points) < 6 || len(points)%2 != 0 {
		return
	}
	nativeFillPolygon(points, color)
}