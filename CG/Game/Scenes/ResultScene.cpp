// リザルト（ベース）

#include "ResultScene.h"
#include <Windows.h>
#include <Xinput.h>
#pragma comment(lib, "xinput9_1_0.lib")

namespace Engine {

void ResultScene::Initialize(WindowDX* dx) {
	dx_ = dx;
	end_ = false;
	next_.clear();
	waitRelease_ = true;
	prevPressed_ = false;
}

void ResultScene::Update() {
	// Space または Pad(A) の立ち上がりで Title へ戻る
	const bool key = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

	XINPUT_STATE pad{};
	bool padA = false;
	if (XInputGetState(0, &pad) == ERROR_SUCCESS) {
		padA = (pad.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
	}

	const bool nowPressed = key || padA;

	if (waitRelease_) {
		if (!nowPressed)
			waitRelease_ = false;
		prevPressed_ = nowPressed;
		return;
	}

	if (nowPressed && !prevPressed_) {
		end_ = true;
		next_ = "Title";
	}
	prevPressed_ = nowPressed;
}

void ResultScene::Draw() { OutputDebugStringA("== ResultScene (base) ==  Press SPACE or A to go Title\n"); }

} // namespace Engine
