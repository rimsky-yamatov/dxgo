package dxgo

type Key int

const (
	KeyBackspace Key = 0x08
	KeyTab       Key = 0x09
	KeyEnter     Key = 0x0D
	KeyShift     Key = 0x10
	KeyControl   Key = 0x11
	KeyAlt       Key = 0x12
	KeyEscape    Key = 0x1B
	KeySpace     Key = 0x20

	KeyLeft  Key = 0x25
	KeyUp    Key = 0x26
	KeyRight Key = 0x27
	KeyDown  Key = 0x28

	Key0 Key = 0x30
	Key1 Key = 0x31
	Key2 Key = 0x32
	Key3 Key = 0x33
	Key4 Key = 0x34
	Key5 Key = 0x35
	Key6 Key = 0x36
	Key7 Key = 0x37
	Key8 Key = 0x38
	Key9 Key = 0x39

	KeyA Key = 0x41
	KeyB Key = 0x42
	KeyC Key = 0x43
	KeyD Key = 0x44
	KeyE Key = 0x45
	KeyF Key = 0x46
	KeyG Key = 0x47
	KeyH Key = 0x48
	KeyI Key = 0x49
	KeyJ Key = 0x4A
	KeyK Key = 0x4B
	KeyL Key = 0x4C
	KeyM Key = 0x4D
	KeyN Key = 0x4E
	KeyO Key = 0x4F
	KeyP Key = 0x50
	KeyQ Key = 0x51
	KeyR Key = 0x52
	KeyS Key = 0x53
	KeyT Key = 0x54
	KeyU Key = 0x55
	KeyV Key = 0x56
	KeyW Key = 0x57
	KeyX Key = 0x58
	KeyY Key = 0x59
	KeyZ Key = 0x5A
)

func KeyDown(key Key) bool {
	return nativeKeyDown(int(key))
}

func KeyPressed(key Key) bool {
	return nativeKeyPressed(int(key))
}

func KeyReleased(key Key) bool {
	return nativeKeyReleased(int(key))
}