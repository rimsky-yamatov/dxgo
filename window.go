package dxgo

import "time"

// maxDeltaTime caps the reported frame delta so a stall (e.g. the user
// dragging the window) doesn't produce a huge time step on the next frame.
const maxDeltaTime = 0.25

type GameWindow struct {
	width  int
	height int
	title  string

	lastFrame     time.Time
	deltaTime     float32
	fps           float32
	fpsAccumTime  float32
	fpsFrameCount int
}

func Window(title string, width, height int) *GameWindow {
	if !nativeInit(title, width, height) {
		return nil
	}

	return &GameWindow{
		width:     width,
		height:    height,
		title:     title,
		lastFrame: time.Now(),
	}
}

func (w *GameWindow) Open() bool {
	return nativeOpen()
}

func (w *GameWindow) Update() {
	nativeUpdate()

	now := time.Now()
	delta := float32(now.Sub(w.lastFrame).Seconds())
	w.lastFrame = now

	if delta < 0 {
		delta = 0
	}

	if delta > maxDeltaTime {
		delta = maxDeltaTime
	}

	w.deltaTime = delta

	w.fpsAccumTime += delta
	w.fpsFrameCount++

	if w.fpsAccumTime >= 0.5 {
		w.fps = float32(w.fpsFrameCount) / w.fpsAccumTime
		w.fpsAccumTime = 0
		w.fpsFrameCount = 0
	}
}

func (w *GameWindow) Clear() {
	nativeClear(0, 0, 0, 1)
}

func (w *GameWindow) Close() {
	nativeClose()
}

// Width returns the window's client area width, in pixels, as given to
// Window(). The window is not resizable, so this value never changes.
func (w *GameWindow) Width() int {
	return w.width
}

// Height returns the window's client area height, in pixels, as given to
// Window(). The window is not resizable, so this value never changes.
func (w *GameWindow) Height() int {
	return w.height
}

// Title returns the window's current title, as last set via Window() or
// SetTitle().
func (w *GameWindow) Title() string {
	return w.title
}

// SetTitle changes the text shown in the window's title bar.
func (w *GameWindow) SetTitle(title string) {
	w.title = title
	nativeSetTitle(title)
}

// ShowCursor shows or hides the mouse cursor while it is over the window.
func (w *GameWindow) ShowCursor(visible bool) {
	nativeShowCursor(visible)
}

// IsFocused reports whether the window currently has input focus.
func (w *GameWindow) IsFocused() bool {
	return nativeIsFocused()
}

// SetVSync enables or disables waiting for the display's vertical sync
// before presenting each frame. VSync is enabled by default.
func (w *GameWindow) SetVSync(enabled bool) {
	nativeSetVSync(enabled)
}

// DeltaTime returns the time, in seconds, that the previous call to
// Update() took to complete. Use it to make movement and animation
// framerate-independent, e.g. `x += speed * w.DeltaTime()`.
func (w *GameWindow) DeltaTime() float32 {
	return w.deltaTime
}

// FPS returns the current frames-per-second, averaged over roughly the
// last half second for a stable reading.
func (w *GameWindow) FPS() float32 {
	return w.fps
}
