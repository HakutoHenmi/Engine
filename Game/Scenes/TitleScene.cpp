// タイトルシーン（実装）

#include "TitleScene.h"
#include <Windows.h>
#include <Xinput.h>
#include <windows.h> // OutputDebugStringA
#pragma comment(lib, "xinput9_1_0.lib")

namespace Engine {

// 初期化：フラグをリセット
void TitleScene::Initialize(WindowDX* dx) {
	dx_ = dx;
	end_ = false;
	next_.clear();
	prevSpace_ = false;
	waitRelease_ = true;
}

// 更新：Spaceキー or AボタンでGameへ遷移
void TitleScene::Update() {
	// --- Spaceキー ---
	SHORT s = GetAsyncKeyState(VK_SPACE);
	bool nowSpace = (s & 0x8000) != 0;

	// --- コントローラAボタン ---
	XINPUT_STATE pad{};
	bool nowA = false;
	if (XInputGetState(0, &pad) == ERROR_SUCCESS) {
		nowA = (pad.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
	}

	bool nowPressed = nowSpace || nowA;

	// 入りっぱなし防止：離すまで待つ
	if (waitRelease_) {
		if (!nowPressed)
			waitRelease_ = false;
		prevSpace_ = nowPressed;
		return;
	}

	// 立ち上がり（Space or A）
	if (nowPressed && !prevSpace_) {
		end_ = true;
		next_ = "Game";
	}
	prevSpace_ = nowPressed;
}

// 仮の描画
void TitleScene::Draw() { OutputDebugStringA("=== Title Scene ===\n"); }

// 遷移IF
bool TitleScene::IsEnd() const { return end_; }
std::string TitleScene::Next() const { return next_; }

} // namespace Engine
