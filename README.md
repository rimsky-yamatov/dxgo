# dxgo

A lightweight, immediate-mode 2D game/graphics library for Go on Windows,
backed by **Direct3D 12**. A single `cgo` layer talks directly to D3D12,
DXGI and GDI — no external engine, no asset pipeline, just a window and a
draw call away from pixels on screen.

```go
package main

import "github.com/rimsky-yamatov/dxgo"

func main() {
	w := dxgo.Window("Hello dxgo", 800, 600)
	if w == nil {
		panic("failed to create window")
	}
	defer w.Close()

	x := float32(50)

	for w.Open() {
		w.Clear()

		x += 120 * w.DeltaTime() // move 120px/sec, independent of frame rate
		if x > 800 {
			x = 0
		}

		w.FillCircle(x, 300, 20, dxgo.Orange)
		w.Text("Hello, dxgo!", 10, 10, 20, dxgo.White)

		w.Update()
	}
}
```

## Requirements

- **Windows 10/11**, with a GPU/driver supporting Direct3D 12 (feature
  level 11.0+).
- **Go** with `CGO_ENABLED=1`.
- A **GCC-compatible C toolchain** on `PATH` (e.g. [MinGW-w64](https://www.mingw-w64.org/),
  [w64devkit](https://github.com/skeeto/w64devkit), or MSYS2's
  `mingw-w64-x86_64-gcc`). The package's `#cgo LDFLAGS` link against
  `d3d12`, `dxgi`, `d3dcompiler_47`, `user32`, `gdi32`, and `uuid`, all of
  which ship with MinGW-w64.

## Install

```
go get github.com/rimsky-yamatov/dxgo
```

## Design

Every draw call is immediate-mode: call it between `Clear()` and
`Update()` and it appears on screen that frame. There's no retained scene
graph or texture/sprite system (yet) — `dxgo` batches your draw calls
into one vertex buffer per frame and submits three draw calls (points,
lines, triangles) to the GPU on `Update()`.

The window is created at a fixed size and is not resizable.

## Quick reference

### Window

| Function | Description |
|---|---|
| `Window(title string, width, height int) *GameWindow` | Creates and opens the native window. Returns `nil` on failure. |
| `(w) Open() bool` | Pumps the OS message queue; returns `false` once the window is closed. Drives the `for w.Open() { ... }` loop. |
| `(w) Clear()` | Clears the frame to black and begins a new frame. |
| `(w) Update()` | Submits the frame's draw calls and presents it. Also refreshes input state, `DeltaTime()` and `FPS()`. |
| `(w) Close()` | Releases all native resources. |
| `(w) Width() int` / `(w) Height() int` | The window's fixed client-area size. |
| `(w) Title() string` / `(w) SetTitle(string)` | Read or change the title bar text. |
| `(w) ShowCursor(visible bool)` | Show or hide the mouse cursor over the window. |
| `(w) IsFocused() bool` | Whether the window currently has input focus. |
| `(w) SetVSync(enabled bool)` | Toggle vertical sync (on by default). |
| `(w) DeltaTime() float32` | Seconds elapsed during the previous `Update()`, capped at 0.25s. Use for frame-rate-independent movement. |
| `(w) FPS() float32` | Frames per second, smoothed over roughly the last half second. |

### Drawing primitives

```go
w.Point(x, y, color)
w.Line(x1, y1, x2, y2, color)
w.DrawRect(x, y, width, height, color)   w.FillRect(...)
w.Circle(x, y, radius, color)            w.FillCircle(...)
w.Ellipse(x, y, rx, ry, color)           w.FillEllipse(...)
w.Triangle(x1, y1, x2, y2, x3, y3, color) w.FillTriangle(...)
w.Polygon(points []float32, color)       w.FillPolygon(...)
```

### Shapes

New composite shapes, built on top of the primitives above:

```go
w.RoundedRect(x, y, width, height, radius, color)     w.FillRoundedRect(...)
w.Arc(x, y, radius, startAngle, endAngle, color)       // open arc, radians
w.Pie(x, y, radius, startAngle, endAngle, color)       w.FillPie(...)
w.Star(x, y, outerRadius, innerRadius, points, color)  w.FillStar(...)
w.RegularPolygon(x, y, radius, sides, color)           w.FillRegularPolygon(...)
w.Arrow(x1, y1, x2, y2, headSize, color)               // a line with an arrowhead
w.Cross(x, y, size, color)                             // a "+" marker
w.Grid(x, y, width, height, cellSize, color)           // debug/level grid
```

Angles are in radians, where `0` points along `+X` and increases toward
`+Y` (matching `Vector2.Angle()` below).

### Text

```go
w.Text(text string, x, y float32, size int, color)  // supports "\n" for multiple lines
w.MeasureText(text string, size int) (width, height int)
```

Text is rendered fully on the GPU: a bitmap font atlas (printable ASCII)
is built once from a system font and uploaded as a D3D12 texture, then
each `Text()` call draws textured quads through their own pipeline.
This avoids the reliability problems of drawing GDI text directly onto
a window that owns a D3D12 flip-model swap chain.

### Color

```go
dxgo.RGB(r, g, b uint8) Color
dxgo.RGBA(r, g, b, a uint8) Color
dxgo.HSV(hue, saturation, value float32) Color   // hue in degrees

color.Lerp(other Color, t float32) Color
color.WithAlpha(a uint8) Color
```

Named colors are ready to use: `White`, `Black`, `Red`, `Green`, `Blue`,
`Yellow`, `Cyan`, `Magenta`, `Gray`, `DarkGray`, `LightGray`, `Orange`,
`Purple`, `Pink`, `Brown`, `Transparent`.

### Keyboard

```go
w.IsKeyDown(key Key) bool
w.IsKeyPressed(key Key) bool   // true only on the frame it went down
w.IsKeyReleased(key Key) bool  // true only on the frame it went up
```

Covers letters, digits, arrows, `F1`–`F12`, the numpad, modifier keys
(left/right shift, control, alt), navigation keys (`Home`, `End`,
`PageUp/Down`, `Insert`, `Delete`), lock keys, and standard US-layout
punctuation (`;`, `=`, `,`, `-`, `.`, `/`, `` ` ``, `[`, `\`, `]`, `'`).
See `keyboard.go` for the full `Key` constant list.

### Mouse

```go
w.MouseX() float32              w.MouseY() float32
w.MouseWheel() float32          // notches scrolled since the last Update()
w.IsMouseDown(dxgo.MouseLeft | dxgo.MouseRight | dxgo.MouseMiddle) bool
w.IsMousePressed(button) bool
w.IsMouseReleased(button) bool
```

### Math & collision (`Vector2`, `Rect`, `Circle`)

```go
v := dxgo.Vec2(x, y)
v.Add(o) / v.Sub(o) / v.Scale(f) / v.Dot(o)
v.Length() / v.LengthSquared() / v.Normalize()
v.Distance(o) / v.DistanceSquared(o) / v.Lerp(o, t) / v.Angle()

r := dxgo.Rect{X, Y, Width, Height}
r.Contains(x, y) / r.ContainsVec(v) / r.Intersects(other) / r.Center()

c := dxgo.Circle{X, Y, Radius}
c.Contains(x, y) / c.ContainsVec(v)
c.IntersectsCircle(other) / c.IntersectsRect(rect)
```

These have no dependency on the window and are plain, allocation-free
math — handy for movement, hitboxes, and simple physics.

## Known limitations

- The window is fixed-size (no resizing or fullscreen toggle).
- No general image/sprite/texture loading yet — non-text drawing is
  flat-color vector shapes.
- No audio.
- The font atlas covers printable ASCII only (no accented characters
  or non-Latin scripts).

## License

Add your preferred license here.
