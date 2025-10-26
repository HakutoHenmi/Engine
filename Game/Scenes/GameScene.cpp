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
	// 4. Stage 初期化/
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

	// === 壁との当たり判定（半径あり：Collide & Slide） ===
	const auto& walls = stage_.GetWallsDynamic();

	// プレイヤーの当たり半径（見た目に合わせて調整）
	const float playerRadius = 0.45f;

	auto IsHitExpanded = [&](const AABB& box, const Vector3& p) -> bool {
		// 壁AABBを半径ぶん膨らませる（めり込み防止）
		const Vector3 minE{box.min.x - playerRadius, box.min.y - playerRadius, box.min.z - playerRadius};
		const Vector3 maxE{box.max.x + playerRadius, box.max.y + playerRadius, box.max.z + playerRadius};
		return (p.x >= minE.x && p.x <= maxE.x) && (p.y >= minE.y && p.y <= maxE.y) && (p.z >= minE.z && p.z <= maxE.z);
	};

	Vector3 cur = prevPos;
	Vector3 next = newPos;

	// ---- X軸だけ先に動かして衝突したらXを打ち消す
	Vector3 tryX = {next.x, cur.y, cur.z};
	bool hitX = false;
	for (const auto& w : walls) {
		if (IsHitExpanded(w, tryX)) {
			hitX = true;
			break;
		}
	}
	if (!hitX)
		cur.x = tryX.x;

	// ---- Z軸も同様（スライド）
	Vector3 tryZ = {cur.x, cur.y, next.z};
	bool hitZ = false;
	for (const auto& w : walls) {
		if (IsHitExpanded(w, tryZ)) {
			hitZ = true;
			break;
		}
	}
	if (!hitZ)
		cur.z = tryZ.z;

	// ---- Y軸（ジャンプ/落下）
	Vector3 tryY = {cur.x, next.y, cur.z};
	bool hitY = false;
	for (const auto& w : walls) {
		if (IsHitExpanded(w, tryY)) {
			hitY = true;
			break;
		}
	}
	if (!hitY) {
		cur.y = tryY.y;
	} else {
		// Yが衝突したら落下速度を止める（2段ジャンプ安定）
		player_.SetVelocityY(0.0f); // ← プレイヤー側に用意済み前提
	}

	// 位置反映
	player_.SetPos(cur);

	// === ステージ更新 ===
	stage_.SetPlayerEffect(player_.GetPos(), 6.0f);
	stage_.Update();

	// === TPS遅延カメラ（相対マウス入力 / 画面端でも止まらない） ===
	if (!useDebugCam_) {
		using namespace DirectX;

		// --- ズーム（ホイール） ---
		float wheel = input_.GetMouseWheelDelta();
		if (wheel != 0.0f) {
			camDist_ -= wheel * 1.0f; // 上で近づく
			camDist_ = std::clamp(camDist_, 3.0f, 20.0f);
		}

		// --- マウス相対移動（DirectInputのΔ。画面端に依存しない） ---
		const float dx = input_.GetMouseDeltaX();
		const float dy = input_.GetMouseDeltaY();

		// --- 角度更新（左右逆・上下逆の指定） ---
		const float mouseSensitivity = 0.002f;
		camYaw_ -= dx * mouseSensitivity;   // 左右
		camPitch_ += dy * mouseSensitivity; // 上下
		camPitch_ = std::clamp(camPitch_, -1.2f, 1.2f);

		// --- 追従対象座標 ---
		const Vector3 playerPos = player_.GetPos();

		// 目標カメラ位置（オービット）
		const float cx = playerPos.x - std::cos(camYaw_) * std::cos(camPitch_) * camDist_;
		const float cz = playerPos.z - std::sin(camYaw_) * std::cos(camPitch_) * camDist_;
		const float cy = playerPos.y + std::sin(camPitch_) * camDist_ + camHeight_;
		XMFLOAT3 targetCamPos{cx, cy, cz};

		// ================================
		// カメラ衝突：球スイープでクリップ防止
		// ================================
		const float camRadius = 0.30f; // カメラ球半径
		const float skin = 0.02f;      // 押し戻し

		Engine::Vector3 eye{playerPos.x, playerPos.y + 1.0f, playerPos.z};
		Engine::Vector3 desired{targetCamPos.x, targetCamPos.y, targetCamPos.z};

		const auto& camWalls = stage_.GetWallsDynamic();

		float bestT = 1.0f;
		Engine::Vector3 bestN{0, 0, 0};
		bool hit = false;

		// 壁AABBをカメラ半径分“膨らませたAABB”に対して線分判定
		for (const auto& wall : camWalls) {
			AABB a = wall;
			a.min.x -= camRadius;
			a.min.y -= camRadius;
			a.min.z -= camRadius;
			a.max.x += camRadius;
			a.max.y += camRadius;
			a.max.z += camRadius;

			float t;
			Engine::Vector3 n;
			if (Collision::IntersectSegmentAABB(eye, desired, a, t, n)) {
				if (t < bestT) {
					bestT = t;
					bestN = n;
					hit = true;
				}
			}
		}

		// 衝突していれば手前で止める＋法線方向にskin押し戻し
		Engine::Vector3 allowed = desired;
		if (hit) {
			Engine::Vector3 hitPos{eye.x + (desired.x - eye.x) * bestT, eye.y + (desired.y - eye.y) * bestT, eye.z + (desired.z - eye.z) * bestT};
			allowed = {hitPos.x - bestN.x * skin, hitPos.y - bestN.y * skin, hitPos.z - bestN.z * skin};
		}

		// --- 遅延追従（衝突解決後の位置へ） ---
		XMFLOAT3 noClipTarget{allowed.x, allowed.y, allowed.z};
		const float followSpeed = 0.15f;
		camPos_.x += (noClipTarget.x - camPos_.x) * followSpeed;
		camPos_.y += (noClipTarget.y - camPos_.y) * followSpeed;
		camPos_.z += (noClipTarget.z - camPos_.z) * followSpeed;

		// 反映と注視
		camPlay_.SetPosition(camPos_);
		XMFLOAT3 lookAt{playerPos.x, playerPos.y + 1.0f, playerPos.z};
		camPlay_.LookAt(lookAt, XMFLOAT3{0, 1, 0});
	}

	// === シーン遷移（Gキーで切替） ===
	if (input_.Trigger(DIK_G)) {
		end_ = true;
		next_ = "Result";
	}
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
