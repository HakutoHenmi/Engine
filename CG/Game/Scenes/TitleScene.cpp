// タイトル（ベース）

#include "TitleScene.h"
#include <Windows.h>
#include <Xinput.h>
#pragma comment(lib, "xinput9_1_0.lib")

namespace Engine {

void TitleScene::Initialize(WindowDX* dx) {
	dx_ = dx;
	end_ = false;
	next_.clear();
	waitRelease_ = true;
	prevPressed_ = false;
}

void TitleScene::Update() {
	// Space または Pad(A) を検出
	const bool key = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

	XINPUT_STATE pad{};
	bool padA = false;
	if (XInputGetState(0, &pad) == ERROR_SUCCESS) {
		padA = (pad.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
	}

	const bool nowPressed = key || padA;

	// 入りっぱなしを無視：一度離すまで待機
	if (waitRelease_) {
		if (!nowPressed)
			waitRelease_ = false;
		prevPressed_ = nowPressed;
		return;
	}

	// 立ち上がりで Game へ
	if (nowPressed && !prevPressed_) {
		end_ = true;
		next_ = "Game";
	}
	prevPressed_ = nowPressed;
}

void TitleScene::Draw() { OutputDebugStringA("== TitleScene (base) ==  Press SPACE or A to start\n"); }

} // namespace Engine
