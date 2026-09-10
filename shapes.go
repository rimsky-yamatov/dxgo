package dxgo

import "math"

// roundedRectCornerSegments controls how many line segments approximate
// each 90-degree corner of a rounded rectangle.
const roundedRectCornerSegments = 8

// fullCircleSegments mirrors the native circle tessellation quality, and
// is used to scale segment counts for partial arcs.
const fullCircleSegments = 64

// RoundedRect draws the outline of a rectangle with rounded corners.
func (w *GameWindow) RoundedRect(x, y, width, height, radius float32, color Color) {
	w.Polygon(roundedRectPoints(x, y, width, height, radius), color)
}

// FillRoundedRect draws a filled rectangle with rounded corners.
func (w *GameWindow) FillRoundedRect(x, y, width, height, radius float32, color Color) {
	w.FillPolygon(roundedRectPoints(x, y, width, height, radius), color)
}

func roundedRectPoints(x, y, width, height, radius float32) []float32 {
	radius = clampRadius(radius, width, height)

	if radius <= 0 {
		return []float32{
			x, y,
			x + width, y,
			x + width, y + height,
			x, y + height,
		}
	}

	var points []float32

	points = append(points, arcPoints(x+width-radius, y+radius, radius, -math.Pi/2, 0, roundedRectCornerSegments)...)
	points = append(points, arcPoints(x+width-radius, y+height-radius, radius, 0, math.Pi/2, roundedRectCornerSegments)...)
	points = append(points, arcPoints(x+radius, y+height-radius, radius, math.Pi/2, math.Pi, roundedRectCornerSegments)...)
	points = append(points, arcPoints(x+radius, y+radius, radius, math.Pi, 3*math.Pi/2, roundedRectCornerSegments)...)

	return points
}

func clampRadius(radius, width, height float32) float32 {
	if radius < 0 {
		return 0
	}

	maxRadius := width / 2
	if height/2 < maxRadius {
		maxRadius = height / 2
	}

	if radius > maxRadius {
		return maxRadius
	}

	return radius
}

// Arc draws an open arc of the given radius from startAngle to endAngle
// (in radians; 0 points along +X, increasing toward +Y).
func (w *GameWindow) Arc(x, y, radius, startAngle, endAngle float32, color Color) {
	points := arcPoints(x, y, radius, startAngle, endAngle, arcSegmentCount(startAngle, endAngle))

	for i := 0; i+3 < len(points); i += 2 {
		w.Line(points[i], points[i+1], points[i+2], points[i+3], color)
	}
}

// Pie draws the outline of a pie slice (a wedge from the center out to
// the arc and back) from startAngle to endAngle, in radians.
func (w *GameWindow) Pie(x, y, radius, startAngle, endAngle float32, color Color) {
	w.Polygon(piePoints(x, y, radius, startAngle, endAngle), color)
}

// FillPie draws a filled pie slice from startAngle to endAngle, in
// radians.
func (w *GameWindow) FillPie(x, y, radius, startAngle, endAngle float32, color Color) {
	w.FillPolygon(piePoints(x, y, radius, startAngle, endAngle), color)
}

func piePoints(x, y, radius, startAngle, endAngle float32) []float32 {
	arc := arcPoints(x, y, radius, startAngle, endAngle, arcSegmentCount(startAngle, endAngle))

	points := make([]float32, 0, len(arc)+2)
	points = append(points, x, y)
	points = append(points, arc...)

	return points
}

// arcPoints tessellates the arc of the given radius, centered at
// (cx, cy), from startAngle to endAngle (radians) into segments+1
// vertices, returned as a flat [x0, y0, x1, y1, ...] slice.
func arcPoints(cx, cy, radius, startAngle, endAngle float32, segments int) []float32 {
	if segments < 1 {
		segments = 1
	}

	points := make([]float32, 0, (segments+1)*2)

	for i := 0; i <= segments; i++ {
		t := startAngle + (endAngle-startAngle)*float32(i)/float32(segments)

		points = append(
			points,
			cx+float32(math.Cos(float64(t)))*radius,
			cy+float32(math.Sin(float64(t)))*radius,
		)
	}

	return points
}

// arcSegmentCount picks a segment count proportional to the arc's angular
// span, so small arcs stay cheap and large ones stay smooth.
func arcSegmentCount(startAngle, endAngle float32) int {
	span := math.Abs(float64(endAngle - startAngle))

	segments := int(span / (2 * math.Pi) * fullCircleSegments)
	if segments < 4 {
		segments = 4
	}

	return segments
}

// Star draws the outline of a star with the given number of points,
// alternating between outerRadius and innerRadius.
func (w *GameWindow) Star(x, y, outerRadius, innerRadius float32, points int, color Color) {
	w.Polygon(starPoints(x, y, outerRadius, innerRadius, points), color)
}

// FillStar draws a filled star with the given number of points,
// alternating between outerRadius and innerRadius.
func (w *GameWindow) FillStar(x, y, outerRadius, innerRadius float32, points int, color Color) {
	w.FillPolygon(starPoints(x, y, outerRadius, innerRadius, points), color)
}

func starPoints(x, y, outerRadius, innerRadius float32, points int) []float32 {
	if points < 2 {
		points = 2
	}

	count := points * 2
	result := make([]float32, 0, count*2)

	for i := 0; i < count; i++ {
		angle := float32(i)*math.Pi/float32(points) - math.Pi/2

		radius := outerRadius
		if i%2 == 1 {
			radius = innerRadius
		}

		result = append(
			result,
			x+float32(math.Cos(float64(angle)))*radius,
			y+float32(math.Sin(float64(angle)))*radius,
		)
	}

	return result
}

// RegularPolygon draws the outline of a regular polygon (equal sides and
// angles), such as a pentagon or hexagon, with sides edges.
func (w *GameWindow) RegularPolygon(x, y, radius float32, sides int, color Color) {
	w.Polygon(regularPolygonPoints(x, y, radius, sides), color)
}

// FillRegularPolygon draws a filled regular polygon with sides edges.
func (w *GameWindow) FillRegularPolygon(x, y, radius float32, sides int, color Color) {
	w.FillPolygon(regularPolygonPoints(x, y, radius, sides), color)
}

func regularPolygonPoints(x, y, radius float32, sides int) []float32 {
	if sides < 3 {
		sides = 3
	}

	result := make([]float32, 0, sides*2)

	for i := 0; i < sides; i++ {
		angle := float32(i)*2*math.Pi/float32(sides) - math.Pi/2

		result = append(
			result,
			x+float32(math.Cos(float64(angle)))*radius,
			y+float32(math.Sin(float64(angle)))*radius,
		)
	}

	return result
}

// Arrow draws a line from (x1, y1) to (x2, y2) with a triangular
// arrowhead at the end, sized by headSize. Useful for debugging
// velocities, forces, or directions.
func (w *GameWindow) Arrow(x1, y1, x2, y2, headSize float32, color Color) {
	w.Line(x1, y1, x2, y2, color)

	angle := math.Atan2(float64(y2-y1), float64(x2-x1))

	leftAngle := angle + math.Pi - math.Pi/6
	rightAngle := angle + math.Pi + math.Pi/6

	leftX := x2 + float32(math.Cos(leftAngle))*headSize
	leftY := y2 + float32(math.Sin(leftAngle))*headSize

	rightX := x2 + float32(math.Cos(rightAngle))*headSize
	rightY := y2 + float32(math.Sin(rightAngle))*headSize

	w.FillTriangle(x2, y2, leftX, leftY, rightX, rightY, color)
}

// Cross draws a plus-shaped marker centered at (x, y), spanning size
// pixels in each direction. Handy for marking a point on screen.
func (w *GameWindow) Cross(x, y, size float32, color Color) {
	w.Line(x-size, y, x+size, y, color)
	w.Line(x, y-size, x, y+size, color)
}

// Grid draws evenly spaced vertical and horizontal lines across the
// given area, useful for visualizing a tile grid or level layout while
// developing.
func (w *GameWindow) Grid(x, y, width, height, cellSize float32, color Color) {
	if cellSize <= 0 || width <= 0 || height <= 0 {
		return
	}

	cols := int(width/cellSize) + 1
	rows := int(height/cellSize) + 1

	for i := 0; i <= cols; i++ {
		gx := x + float32(i)*cellSize
		if gx > x+width {
			break
		}

		w.Line(gx, y, gx, y+height, color)
	}

	for i := 0; i <= rows; i++ {
		gy := y + float32(i)*cellSize
		if gy > y+height {
			break
		}

		w.Line(x, gy, x+width, gy, color)
	}
}
