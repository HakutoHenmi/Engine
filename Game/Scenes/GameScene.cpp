#include "GameScene.h"
#include "Collision.h"
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

	camPos_ = DirectX::XMFLOAT3{0.0f, 10.0f, -20.0f};
	camPlay_.SetPosition(camPos_);
	firstMouse_ = true;
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
	Vector3 prevPos = player_.GetPos(); // 移動前
	player_.Update(*activeCam_, input_);
	Vector3 newPos = player_.GetPos(); // 移動後

	// === 壁との当たり判定 ===
	const auto& walls = stage_.GetWallsDynamic(); // ← 正しい関数名

	for (const auto& wall : walls) {
		float t;
		Vector3 n;
		// Segment vs AABB 判定
		if (Collision::IntersectSegmentAABB(prevPos, newPos, wall, t, n)) {
			// 衝突点
			Vector3 hit = {prevPos.x + (newPos.x - prevPos.x) * t, prevPos.y + (newPos.y - prevPos.y) * t, prevPos.z + (newPos.z - prevPos.z) * t};
			// 少し押し戻し
			const float pushBack = 0.05f;
			newPos = {hit.x + n.x * pushBack, hit.y + n.y * pushBack, hit.z + n.z * pushBack};
			player_.SetPos(newPos);
			break;
		}
	}

	// === ステージ更新 ===
	stage_.SetPlayerEffect(player_.GetPos(), 6.0f);
	stage_.Update();

	// === TPS遅延カメラ ===
	if (!useDebugCam_) {
		using namespace DirectX;

		// === ズーム ===
		float wheel = input_.GetMouseWheelDelta();
		if (wheel != 0.0f) {
			camDist_ -= wheel * 1.0f; // 上に回すとズームイン
			camDist_ = std::clamp(camDist_, 3.0f, 20.0f);
		}

		// 1) マウスΔを自前で取得
		POINT p;
		GetCursorPos(&p);
		ScreenToClient(dx_->GetHwnd(), &p);
		float dx = 0.0f, dy = 0.0f;
		if (firstMouse_) {
			prevMouseX_ = p.x;
			prevMouseY_ = p.y;
			firstMouse_ = false;
		} else {
			dx = float(p.x - prevMouseX_);
			dy = float(p.y - prevMouseY_);
			prevMouseX_ = p.x;
			prevMouseY_ = p.y;
		}

		// 2) 角度更新（マウス操作）
		const float mouseSensitivity = 0.002f;
		// 符号を反転して操作方向を逆にする
		camYaw_ -= dx * mouseSensitivity;   // 左右を逆に
		camPitch_ += dy * mouseSensitivity; // 上下を逆に

		// ピッチ制限（上下）
		camPitch_ = std::clamp(camPitch_, -1.2f, 1.2f);

		// 3) プレイヤー基準の目標カメラ座標を算出（オービット）
		const auto playerPos = player_.GetPos(); // Engine::Vector3
		// Vector3 → XMFLOAT3 に都度展開
		const float cx = playerPos.x - std::cos(camYaw_) * std::cos(camPitch_) * camDist_;
		const float cz = playerPos.z - std::sin(camYaw_) * std::cos(camPitch_) * camDist_;
		const float cy = playerPos.y + std::sin(camPitch_) * camDist_ + camHeight_;
		XMFLOAT3 targetCamPos{cx, cy, cz};

		// 4) 遅延追従（lerp は自前の XMFLOAT3 で行う）
		const float followSpeed = 0.15f;
		camPos_.x += (targetCamPos.x - camPos_.x) * followSpeed;
		camPos_.y += (targetCamPos.y - camPos_.y) * followSpeed;
		camPos_.z += (targetCamPos.z - camPos_.z) * followSpeed;

		// 5) カメラへ適用（XMFLOAT3 を渡す）
		camPlay_.SetPosition(camPos_);

		// 6) 注視点（XMFLOAT3 で渡す / または float*6 のオーバーロードを使う）
		XMFLOAT3 lookAt{playerPos.x, playerPos.y + 1.0f, playerPos.z};
		camPlay_.LookAt(lookAt, XMFLOAT3{0, 1, 0});
		// もしくは：camPlay_.LookAt(lookAt.x, lookAt.y, lookAt.z, 0, 1, 0);
	}

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
	if (input_.Trigger(DIK_G)) {
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
