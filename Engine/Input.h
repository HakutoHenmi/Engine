#pragma once
// ===============================
//  Input : DirectInput(Keyboard)
// ===============================
#include <Windows.h>
#include <dinput.h>
#include <wrl.h>

namespace Engine {

class Input {
public:
	void Initialize(HINSTANCE hInst, HWND hwnd);
	void Update();
	void Shutdown();

	// 押下/立ち上がり
	bool Down(BYTE k) const { return (state_[k] & 0x80) != 0; }
	bool Trigger(BYTE k) const { return Down(k) && !(prev_[k] & 0x80); }

private:
	Microsoft::WRL::ComPtr<IDirectInput8> di_;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> kb_;
	BYTE state_[256]{};
	BYTE prev_[256]{};
};

} // namespace Engine
