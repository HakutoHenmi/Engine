// タイトル（ベース）

#include "TitleScene.h"
#include <Windows.h>
#include <Xinput.h>
#pragma comment(lib, "xinput9_1_0.lib")

#include "imgui.h"

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

void TitleScene::Draw() {
	// デバッグログはお好みで残してOK
	OutputDebugStringA("== TitleScene (base) ==  Press SPACE or A to start\n");

	// ★ ImGui でメッセージを表示
	// 画面中央付近に、背景なしのテキストだけ出すイメージ
	ImGui::SetNextWindowBgAlpha(0.0f); // 背景透明

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
	                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("TitleMessage", nullptr, flags);

	// 位置調整（画面中央っぽい位置に移動）
	// ※ウィンドウ生成直後に一度だけ位置を変えたいなら ImGuiCond_FirstUseEver でもOK
	ImGui::SetWindowPos(ImVec2(500, 300), ImGuiCond_Always); // 適当に調整してください

	// 日本語＋英語メッセージ
	ImGui::Text("スペースキーを押してください");
	ImGui::Text("Press SPACE key to start");

	ImGui::End();
}

} // namespace Engine
