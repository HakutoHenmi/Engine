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
	player_.Initialize(renderer_, dx_->Dev(), dx_->List());

	// --- 剣モデルを読み込み・プレイヤーにセット ---
	int sword = renderer_.LoadModel(dx_->Dev(), dx_->List(), "Resources/weapons/sword.obj");
	player_.SetSwordModel(sword);

	// パーティクル
	sparks_.Initialize(renderer_, *dx_, 6000); // 2000枚まで
	sparks_.SetPosition({0, 1.0f, 0});
	auto& p = sparks_.Params();
	p.maxOnce = 256;
	p.useAdditive = true;
	p.initColor = {1.0f, 0.85f, 0.35f, 1.0f};
	p.initScaleMin = {0.35f, 0.35f, 0.35f};
	p.initScaleMax = {0.55f, 0.55f, 0.55f};
	p.lifeMin = 0.30f;
	p.lifeMax = 0.55f;
	p.initVelMin = {-1.6f, 2.8f, -1.6f};
	p.initVelMax = {+1.6f, 5.0f, +1.6f};
	p.spawnPosJitter = {0.40f, 0.60f, 0.40f};

	// ===============================
	// 4. Stage 初期化
	// ===============================
	stage_.Initialize(renderer_, *dx_, *activeCam_, "Resources/Maps/Stage1_Map.csv", "Resources/Maps/Stage1_Angle.csv", 1.0f, 1.0f, 1.0f, GridAnchor::Center, ModelOrigin::Center, 0.02f, 0.02f);

	// --- ボス生成 ---
	boss_ = std::make_unique<Game::Boss>();
	boss_->Initialize(renderer_, *dx_);

	// ★ボスのルート位置（ステージ中央など）
	Engine::Vector3 bossRoot{0.0f, 0.0f, 0.0f};

	// ★この XZ でのボクセル地形の高さ(Y)を取得
	float groundY = renderer_.TerrainHeightAt(bossRoot.x, bossRoot.z);

	// 必要なら +0.1f とかで少し浮かせてもOK
	bossRoot.y = groundY; // または groundY + 0.1f;

	// ルート位置（支点）を設定
	boss_->SetPosition(bossRoot);

	// 地形を凹ませるコールバック
	boss_->SetTerrainHitCallback([this](const Game::TerrainHitInfo& info) { ApplyBossHitToTerrain_(info); });

	// ★XZ 位置からボクセル地形の高さを返すコールバック
	boss_->SetTerrainHeightCallback([this](const Engine::Vector3& pos) { return renderer_.TerrainHeightAt(pos.x, pos.z); });

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

	// --- ボス更新（プレイヤー位置を渡す）---
	Engine::Vector3 playerPos = player_.GetPos();
	float dt = 1.0f / 60.0f; // 今は固定フレームと同じ
	if (boss_) {
		boss_->Update(dt, playerPos);
	}

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

	// === 右クリックで板ポリパーティクル大量発生 ===
	const bool rbNow = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	if (rbNow) {
		// 出す位置：プレイヤーの少し前＆少し上
		Engine::Vector3 pos = player_.GetPos();
		// プレイヤー前方へ 0.8m・上へ 0.6m（大きく散らすので中心だけ上げる）
		// 前方はカメラ向きでもOK：ここではカメラ前方を使う
		const float cy = std::cos(camYaw_), sy = std::sin(camYaw_);
		pos.x += cy * 0.8f;
		pos.z += sy * 0.8f;
		pos.y += 0.6f;
		sparks_.SetPosition(pos);
		// 毎フレ複数回バーストして“ドバッ”と出す
		sparks_.Burst(256);
		sparks_.Burst(256);
		if (!prevRB_) {
			sparks_.Burst(512);
		}
	}
	prevRB_ = rbNow;

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
		// const Vector3 playerPos = player_.GetPos();

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

	sparks_.Update(1.0f / 60.0f);
}

void GameScene::ApplyBossHitToTerrain_(const Game::TerrainHitInfo& info) {
	char buf[256];
	std::snprintf(buf, sizeof(buf), "[BossHit] pos=(%.2f, %.2f, %.2f), r=%.2f, depth=%.2f\n", info.position.x, info.position.y, info.position.z, info.radius, info.depth);
	OutputDebugStringA(buf);

	// ★ボスの着弾位置を GPU ボクセル地形に凹みとして登録
	DirectX::XMFLOAT3 pos{info.position.x, info.position.y, info.position.z};
	renderer_.AddTerrainDent(pos, info.radius, info.depth);
}

void GameScene::Draw() {
	ID3D12GraphicsCommandList* cmd = dx_->List();

	// フレーム開始
	renderer_.BeginFrame(cmd);

	// ここで Compute
	renderer_.DispatchVoxel(cmd, /*gridX=*/256, /*gridZ=*/256);
	renderer_.DrawVoxel(cmd, *activeCam_);

	// 既存の描画
	if (gridVisible_) {
		renderer_.DrawGrid(*activeCam_, cmd);
	}
	stage_.Draw(renderer_, cmd);
	player_.Draw(renderer_, *activeCam_, cmd);
	sparks_.Draw(cmd, *activeCam_);

	if (boss_) {
		boss_->Draw(renderer_, dx_->List(), *activeCam_);
	}

	renderer_.EndFrame(cmd);
}

} // namespace Engine
