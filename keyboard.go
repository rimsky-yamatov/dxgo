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

	KeyLeftWindows  Key = 0x5B
	KeyRightWindows Key = 0x5C
	KeyApplications Key = 0x5D

	KeyNumpad0 Key = 0x60
	KeyNumpad1 Key = 0x61
	KeyNumpad2 Key = 0x62
	KeyNumpad3 Key = 0x63
	KeyNumpad4 Key = 0x64
	KeyNumpad5 Key = 0x65
	KeyNumpad6 Key = 0x66
	KeyNumpad7 Key = 0x67
	KeyNumpad8 Key = 0x68
	KeyNumpad9 Key = 0x69

	KeyNumpadMultiply Key = 0x6A
	KeyNumpadAdd      Key = 0x6B
	KeyNumpadSubtract Key = 0x6D
	KeyNumpadDecimal  Key = 0x6E
	KeyNumpadDivide   Key = 0x6F

	KeyF1  Key = 0x70
	KeyF2  Key = 0x71
	KeyF3  Key = 0x72
	KeyF4  Key = 0x73
	KeyF5  Key = 0x74
	KeyF6  Key = 0x75
	KeyF7  Key = 0x76
	KeyF8  Key = 0x77
	KeyF9  Key = 0x78
	KeyF10 Key = 0x79
	KeyF11 Key = 0x7A
	KeyF12 Key = 0x7B

	KeyNumLock    Key = 0x90
	KeyScrollLock Key = 0x91

	KeyLeftShift    Key = 0xA0
	KeyRightShift   Key = 0xA1
	KeyLeftControl  Key = 0xA2
	KeyRightControl Key = 0xA3
	KeyLeftAlt      Key = 0xA4
	KeyRightAlt     Key = 0xA5

	KeyPrintScreen Key = 0x2C
	KeyPause       Key = 0x13
	KeyCapsLock    Key = 0x14
	KeyPageUp      Key = 0x21
	KeyPageDown    Key = 0x22
	KeyEnd         Key = 0x23
	KeyHome        Key = 0x24
	KeyInsert      Key = 0x2D
	KeyDelete      Key = 0x2E

	KeySemicolon    Key = 0xBA // ;
	KeyEquals       Key = 0xBB // =
	KeyComma        Key = 0xBC // ,
	KeyMinus        Key = 0xBD // -
	KeyPeriod       Key = 0xBE // .
	KeySlash        Key = 0xBF // /
	KeyBacktick     Key = 0xC0 // `
	KeyLeftBracket  Key = 0xDB // [
	KeyBackslash    Key = 0xDC // \
	KeyRightBracket Key = 0xDD // ]
	KeyQuote        Key = 0xDE // '
)

func (w *GameWindow) IsKeyDown(key Key) bool {
	return nativeKeyDown(int(key))
}

func (w *GameWindow) IsKeyPressed(key Key) bool {
	return nativeKeyPressed(int(key))
}

func (w *GameWindow) IsKeyReleased(key Key) bool {
	return nativeKeyReleased(int(key))
}
