#include "GameScene.h"
#include <Windows.h>
#include <Xinput.h>
#pragma comment(lib, "xinput9_1_0.lib")

using namespace DirectX;

namespace Engine {

void GameScene::Initialize(WindowDX* dx) {
	dx_ = dx;
	end_ = false;
	next_.clear();
	waitRelease_ = true;
	prevPressed_ = false;
	gridVisible_ = true;

	// ===============================
	// 1. Renderer / Input
	// ===============================
	renderer_.Initialize(*dx_);
	input_.Initialize(dx_->GetHInstance(), dx_->GetHwnd());

	// ===============================
	// 2. Camera
	// ===============================
	const float aspect = 1280.0f / 720.0f;
	camPlay_.Initialize();
	camDebug_.Initialize();
	camPlay_.SetProjection(XMConvertToRadians(60.0f), aspect, 0.1f, 500.0f);
	camDebug_.SetProjection(XMConvertToRadians(60.0f), aspect, 0.1f, 500.0f);
	camPlay_.SetPosition({0.0f, 10.0f, -20.0f});
	camPlay_.LookAt({0.0f, 0.0f, 0.0f}, {0, 1, 0});
	camDebug_.SetPosition({0.0f, 20.0f, -30.0f});
	camDebug_.LookAt({0.0f, 0.0f, 0.0f}, {0, 1, 0});
	activeCam_ = &camPlay_;

	// ===============================
	// 3. モデル / プレイヤー
	// ===============================
	// ※ Model/Texture の読み込みは Renderer の内部SRVを変更するため
	//   Stage を作る前に完了させる必要がある
	player_.Initialize(renderer_, dx_->Dev(), dx_->List());

	// ===============================
	// 4. Stage 初期化
	// ===============================
	stage_.Initialize(renderer_, *dx_, *activeCam_, "Resources/Maps/Stage1_Map.csv", "Resources/Maps/Stage1_Angle.csv", 1.0f, 1.0f, 1.0f, GridAnchor::Center, ModelOrigin::Center, 0.02f, 0.02f);

	// ===============================
	// 5. Grid 初期化
	// ===============================
	{
		const float pitchX = stage_.PitchX();
		const float pitchZ = stage_.PitchZ();
		const int cols = stage_.Cols();
		const int rows = stage_.Rows();
		const float minX = -0.5f * cols * pitchX;
		const float minZ = -0.5f * rows * pitchZ;
		renderer_.InitGrid(*dx_, (std::max)(cols, rows), pitchX, 0.0f, minX, minZ);
	}
}

void GameScene::Update() {
	input_.Update();

	// === F1：カメラ切替 ===
	bool nowF1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
	if (nowF1 && !prevF1_) {
		useDebugCam_ = !useDebugCam_;
		activeCam_ = useDebugCam_ ? &camDebug_ : &camPlay_;
		stage_.SetCamera(activeCam_);
	}
	prevF1_ = nowF1;

	// === F2：グリッド切替 ===
	if (input_.Trigger(DIK_F2)) {
		gridVisible_ = !gridVisible_;
	}

	// === プレイヤー更新 ===
	player_.Update(*activeCam_, input_);

	// === ステージ更新 ===
	stage_.SetPlayerEffect(player_.GetPos(), 6.0f);
	stage_.Update();

	// === シーン遷移 ===
	bool key = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
	XINPUT_STATE pad{};
	bool padA = false;
	if (XInputGetState(0, &pad) == ERROR_SUCCESS) {
		padA = (pad.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
	}
	bool nowPressed = key || padA;

	if (waitRelease_) {
		if (!nowPressed)
			waitRelease_ = false;
		prevPressed_ = nowPressed;
		return;
	}
	if (nowPressed && !prevPressed_) {
		end_ = true;
		next_ = "Result";
	}
	prevPressed_ = nowPressed;
}

void GameScene::Draw() {
	// ===== コマンドリスト取得（WindowDXがBegin済み想定） =====
	ID3D12GraphicsCommandList* cmd = dx_->List();

	// Renderer BeginFrame（状態セットのみ）
	renderer_.BeginFrame(cmd);

	// グリッド
	if (gridVisible_) {
		renderer_.DrawGrid(*activeCam_, cmd);
	}

	// ステージ
	stage_.Draw(renderer_, cmd);

	// プレイヤー
	player_.Draw(renderer_, *activeCam_, cmd);

	// Renderer EndFrame（Closeしない）
	renderer_.EndFrame(cmd);
}

} // namespace Engine
