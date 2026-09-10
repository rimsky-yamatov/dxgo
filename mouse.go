package dxgo

// MouseButton identifies a mouse button. Values match the underlying
// Windows virtual-key codes, so they share the same polling logic as
// keyboard keys.
type MouseButton int

const (
	MouseLeft   MouseButton = 0x01
	MouseRight  MouseButton = 0x02
	MouseMiddle MouseButton = 0x04
)

// MouseX returns the mouse cursor's X position in window client
// coordinates (0 is the left edge).
func (w *GameWindow) MouseX() float32 {
	return nativeMouseX()
}

// MouseY returns the mouse cursor's Y position in window client
// coordinates (0 is the top edge).
func (w *GameWindow) MouseY() float32 {
	return nativeMouseY()
}

// MouseWheel returns how far the mouse wheel moved since the last
// Update(), in notches (positive is away from the user / scroll up).
func (w *GameWindow) MouseWheel() float32 {
	return nativeMouseWheel()
}

// IsMouseDown reports whether the given mouse button is currently held.
func (w *GameWindow) IsMouseDown(button MouseButton) bool {
	return nativeKeyDown(int(button))
}

// IsMousePressed reports whether the given mouse button was pressed down
// during this frame (i.e. it was up last frame and is down now).
func (w *GameWindow) IsMousePressed(button MouseButton) bool {
	return nativeKeyPressed(int(button))
}

// IsMouseReleased reports whether the given mouse button was released
// during this frame (i.e. it was down last frame and is up now).
func (w *GameWindow) IsMouseReleased(button MouseButton) bool {
	return nativeKeyReleased(int(button))
}
