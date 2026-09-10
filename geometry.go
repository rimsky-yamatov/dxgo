package dxgo

import "math"

// Vector2 is a 2D vector, commonly used for positions, movement, and
// velocity in game code.
type Vector2 struct {
	X float32
	Y float32
}

func Vec2(x, y float32) Vector2 {
	return Vector2{X: x, Y: y}
}

func (v Vector2) Add(other Vector2) Vector2 {
	return Vector2{X: v.X + other.X, Y: v.Y + other.Y}
}

func (v Vector2) Sub(other Vector2) Vector2 {
	return Vector2{X: v.X - other.X, Y: v.Y - other.Y}
}

func (v Vector2) Scale(factor float32) Vector2 {
	return Vector2{X: v.X * factor, Y: v.Y * factor}
}

func (v Vector2) Dot(other Vector2) float32 {
	return v.X*other.X + v.Y*other.Y
}

func (v Vector2) Length() float32 {
	return float32(math.Sqrt(float64(v.X*v.X + v.Y*v.Y)))
}

func (v Vector2) LengthSquared() float32 {
	return v.X*v.X + v.Y*v.Y
}

// Normalize returns a unit-length vector pointing in the same direction
// as v. It returns the zero vector if v has zero length.
func (v Vector2) Normalize() Vector2 {
	length := v.Length()
	if length == 0 {
		return Vector2{}
	}

	return Vector2{X: v.X / length, Y: v.Y / length}
}

func (v Vector2) Distance(other Vector2) float32 {
	return v.Sub(other).Length()
}

func (v Vector2) DistanceSquared(other Vector2) float32 {
	return v.Sub(other).LengthSquared()
}

// Lerp linearly interpolates between v and other. t is typically in
// [0, 1], where 0 returns v and 1 returns other.
func (v Vector2) Lerp(other Vector2, t float32) Vector2 {
	return Vector2{
		X: v.X + (other.X-v.X)*t,
		Y: v.Y + (other.Y-v.Y)*t,
	}
}

// Angle returns the vector's direction in radians, as used by Arc, Pie,
// and the trigonometric shape helpers (0 points along +X, increasing
// toward +Y).
func (v Vector2) Angle() float32 {
	return float32(math.Atan2(float64(v.Y), float64(v.X)))
}

// Rect is an axis-aligned rectangle, useful for hitboxes and simple
// collision detection.
type Rect struct {
	X      float32
	Y      float32
	Width  float32
	Height float32
}

func (r Rect) Contains(x, y float32) bool {
	return x >= r.X && x <= r.X+r.Width &&
		y >= r.Y && y <= r.Y+r.Height
}

func (r Rect) ContainsVec(point Vector2) bool {
	return r.Contains(point.X, point.Y)
}

// Intersects reports whether r and other overlap (an axis-aligned
// bounding box check).
func (r Rect) Intersects(other Rect) bool {
	return r.X < other.X+other.Width &&
		r.X+r.Width > other.X &&
		r.Y < other.Y+other.Height &&
		r.Y+r.Height > other.Y
}

func (r Rect) Center() Vector2 {
	return Vector2{X: r.X + r.Width/2, Y: r.Y + r.Height/2}
}

// Circle is used for hitboxes and simple collision detection against
// other circles or rectangles.
type Circle struct {
	X      float32
	Y      float32
	Radius float32
}

func (c Circle) Contains(x, y float32) bool {
	dx := x - c.X
	dy := y - c.Y
	return dx*dx+dy*dy <= c.Radius*c.Radius
}

func (c Circle) ContainsVec(point Vector2) bool {
	return c.Contains(point.X, point.Y)
}

func (c Circle) IntersectsCircle(other Circle) bool {
	dx := other.X - c.X
	dy := other.Y - c.Y
	radiusSum := c.Radius + other.Radius
	return dx*dx+dy*dy <= radiusSum*radiusSum
}

// IntersectsRect reports whether the circle overlaps the axis-aligned
// rectangle r.
func (c Circle) IntersectsRect(r Rect) bool {
	closestX := c.X
	if closestX < r.X {
		closestX = r.X
	} else if closestX > r.X+r.Width {
		closestX = r.X + r.Width
	}

	closestY := c.Y
	if closestY < r.Y {
		closestY = r.Y
	} else if closestY > r.Y+r.Height {
		closestY = r.Y + r.Height
	}

	dx := c.X - closestX
	dy := c.Y - closestY

	return dx*dx+dy*dy <= c.Radius*c.Radius
}
