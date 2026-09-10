package dxgo

import "math"

// Common named colors, ready to use without calling RGB() yourself.
var (
	White       = RGB(255, 255, 255)
	Black       = RGB(0, 0, 0)
	Red         = RGB(255, 0, 0)
	Green       = RGB(0, 255, 0)
	Blue        = RGB(0, 0, 255)
	Yellow      = RGB(255, 255, 0)
	Cyan        = RGB(0, 255, 255)
	Magenta     = RGB(255, 0, 255)
	Gray        = RGB(128, 128, 128)
	DarkGray    = RGB(64, 64, 64)
	LightGray   = RGB(192, 192, 192)
	Orange      = RGB(255, 165, 0)
	Purple      = RGB(128, 0, 128)
	Pink        = RGB(255, 192, 203)
	Brown       = RGB(139, 69, 19)
	Transparent = RGBA(0, 0, 0, 0)
)

// Lerp linearly interpolates between c and other, channel by channel.
// t is typically in [0, 1], where 0 returns c and 1 returns other; it is
// clamped to that range.
func (c Color) Lerp(other Color, t float32) Color {
	if t < 0 {
		t = 0
	}

	if t > 1 {
		t = 1
	}

	return Color{
		R: lerpChannel(c.R, other.R, t),
		G: lerpChannel(c.G, other.G, t),
		B: lerpChannel(c.B, other.B, t),
		A: lerpChannel(c.A, other.A, t),
	}
}

func lerpChannel(from, to uint8, t float32) uint8 {
	return uint8(float32(from) + (float32(to)-float32(from))*t)
}

// WithAlpha returns a copy of c with the alpha channel replaced by a.
func (c Color) WithAlpha(a uint8) Color {
	return Color{R: c.R, G: c.G, B: c.B, A: a}
}

// HSV builds an opaque Color from hue (degrees, any range - wrapped to
// [0, 360)), saturation, and value (both in [0, 1]). Handy for rainbow
// cycles and procedural color variation.
func HSV(hue, saturation, value float32) Color {
	hue = float32(math.Mod(float64(hue), 360))
	if hue < 0 {
		hue += 360
	}

	if saturation < 0 {
		saturation = 0
	} else if saturation > 1 {
		saturation = 1
	}

	if value < 0 {
		value = 0
	} else if value > 1 {
		value = 1
	}

	chroma := value * saturation
	huePrime := hue / 60
	x := chroma * (1 - float32(math.Abs(math.Mod(float64(huePrime), 2)-1)))
	m := value - chroma

	var r, g, b float32

	switch {
	case huePrime < 1:
		r, g, b = chroma, x, 0
	case huePrime < 2:
		r, g, b = x, chroma, 0
	case huePrime < 3:
		r, g, b = 0, chroma, x
	case huePrime < 4:
		r, g, b = 0, x, chroma
	case huePrime < 5:
		r, g, b = x, 0, chroma
	default:
		r, g, b = chroma, 0, x
	}

	return RGB(
		uint8((r+m)*255),
		uint8((g+m)*255),
		uint8((b+m)*255),
	)
}
