// GameScene.cpp — 完全版
#include "GameScene.h"

#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>

#include <Xinput.h>
#pragma comment(lib, "xinput9_1_0.lib")

using namespace DirectX;

namespace Engine {

static inline float ClampF(float v, float lo, float hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }

void GameScene::Initialize(WindowDX* dx) {
	assert(dx);
	dx_ = dx;

	end_ = false;
	next_.clear();

	// 入力
	input_.Initialize(dx_->GetHInstance(), dx_->GetHwnd());

	// レンダラ
	renderer_.Initialize(*dx_);

	// カメラ
	camPlay_.Initialize();
	camDebug_.Initialize();
	const float aspect = 1280.0f / 720.0f;
	camPlay_.SetProjection(XMConvertToRadians(60.0f), aspect, 0.1f, 500.0f);
	camDebug_.SetProjection(XMConvertToRadians(60.0f), aspect, 0.1f, 500.0f);

	// プレイカメラ
	camPlay_.SetPosition({0.0f, 20.0f, -25.0f});
	camPlay_.LookAt({0.0f, 0.0f, 0.0f}, {0, 1, 0});

	// デバッグカメラ
	dbgPos_ = {0.0f, 12.0f, -30.0f};
	dbgYaw_ = 0.0f;
	dbgPitch_ = 0.20f;
	{
		XMFLOAT3 look{dbgPos_.x + std::sin(dbgYaw_) * std::cos(dbgPitch_), dbgPos_.y + std::sin(dbgPitch_), dbgPos_.z + std::cos(dbgYaw_) * std::cos(dbgPitch_)};
		camDebug_.SetPosition(dbgPos_);
		camDebug_.LookAt(look, {0, 1, 0});
	}
	activeCam_ = ActiveCam::Play;
	prevF1_ = false;
	rmbPrevDown_ = false;

	// 1) モデル読み込み（通常OBJとして）
	skydomeHandle_ = renderer_.LoadModel(dx_->Dev(), dx_->List(), "Resources/skydome/skydome.obj");

	// 2) 変換（まずは確実に見えるサイズと高さ）
	skydomeTf_.scale = {1.0f, 1.0f, 1.0f}; // 大きめ
	skydomeTf_.rotate = {0.0f, 0.0f, 0.0f};
	skydomeTf_.translate = {0.0f, 10.0f, 0.0f}; // ステージの上空あたり

	// プレイヤ
	player_.Initialize(renderer_, dx_->Dev(), dx_->List());

	// モデル読み込み
	cubeModelHandle_ = renderer_.LoadModel(dx_->Dev(), dx_->List(), "Resources/cube/cube.obj");
	lazerModelHandle_ = renderer_.LoadModel(dx_->Dev(), dx_->List(), "Resources/cube/cube.obj");
	enhanceModelHandle_ = renderer_.LoadModel(dx_->Dev(), dx_->List(), "Resources/cube/cube.obj");
	// particleModelHandle_ = renderer_.LoadModel(dx_->Dev(), dx_->List(), "Resources/cube/cube.obj");

	laserManager_.InitializeParticle(
	    renderer_, dx_->Dev(), dx_->List(),
	    "Resources/cube/cube.obj",  // 火花用
	    "Resources/cube/cube.obj"); // リング用

	laserManager_.InitializeParticle(renderer_, dx_->Dev(), dx_->List(), "Resources/cube/cube.obj", "Resources/cube/cube.obj");

	// 敵のパーティクルはスロットをずらす
	enemyParticle_.Initialize(renderer_, dx_->Dev(), dx_->List(), "Resources/cube/cube.obj", "Resources/cube/cube.obj", 4096);

	// ステージ（CSV の中心が原点になるよう Stage 側で配置）
	LoadStage(1);

	// 敵
	enemies_.resize(2); // 2体
	for (size_t i = 0; i < enemies_.size(); ++i) {
		enemies_[i].Initialize(renderer_, dx_->Dev(), dx_->List());
	}
	// 配置を個別設定
	enemies_[0].GetTransform().translate = {-6.0f, 0.5f, 0.0f};
	enemies_[1].GetTransform().translate = {6.0f, 0.5f, 0.0f};

	// グリッド線
	const float pitchX = stage_.PitchX();
	const float pitchZ = stage_.PitchZ();
	const int cols = stage_.Cols();
	const int rows = stage_.Rows();

	// ステージの幾何学的な左下境界（= 端の線）を位相にする
	const float minX = -0.5f * cols * pitchX;
	const float minZ = -0.5f * rows * pitchZ;

	// 余裕を持って広めに描画（half は大きめでOK）
	renderer_.InitGrid(*dx_, 50, pitchX, 0.0f, /*phaseX=*/minX, /*phaseZ=*/minZ);
}

void GameScene::Update() {
	input_.Update();

	// フレーム経過時間 dt[sec] を算出（高分解能）
	using clock = std::chrono::steady_clock;
	static auto prev = clock::now();
	const auto now = clock::now();
	float dt = std::chrono::duration<float>(now - prev).count();
	prev = now;

	// === ★ ヒットストップ処理 ===
	if (hitStopTimer_ > 0.0f) {
		hitStopTimer_ -= dt;

		if (hitStopTimer_ <= 0.0f && !hitStopEnded_) {
			hitStopEnded_ = true;

			// 進行方向を取得
			Vector3 laserDir = laserManager_.GetLastLaserDir();

			// Directional モードに切り替え
			enemyParticle_.SetScatterMode(ParticleSystem::ScatterMode::Directional);
			enemyParticle_.SetMainDirection(laserDir);

			enemyParticle_.SetSpreadAngle(0.3f);

			// パーティクル生成（レーザー進行方向へ散らす）
			enemyParticle_.EnemySpawn(
			    lastKillPos_,
			    laserDir,                 // normal 代わりにレーザー方向
			    30,                       // 個数
			    {0.2f, 1.0f, 1.0f, 1.0f}, // 色（シアン系）
			    0.5f                      // spread（広がり）
			);
		}
		return; // ← ここで一時停止
	}

	CurrentCamera().Tick(dt);

	// Spaceでリザルトへ
	{
		SHORT s = GetAsyncKeyState(VK_SPACE);
		bool nowSpace = (s & 0x8000) != 0;
		if (waitRelease_) {
			if (!nowSpace)
				waitRelease_ = false;
			prevSpace_ = nowSpace;
		} else {
			if (nowSpace && !prevSpace_) {
				end_ = true;
				next_ = "Result";
			}
			prevSpace_ = nowSpace;
		}
	}

	// F1でカメラ切替
	bool nowF1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
	if (nowF1 && !prevF1_) {
		activeCam_ = (activeCam_ == ActiveCam::Play) ? ActiveCam::Debug : ActiveCam::Play;
		stage_.SetCamera(&CurrentCamera());
		rmbPrevDown_ = false;
	}
	prevF1_ = nowF1;

	// float dt = GetDeltaSec();

	// カメラ更新
	if (activeCam_ == ActiveCam::Play)
		UpdatePlayCamera(dt);
	else
		UpdateDebugCamera(dt);

	// プレイヤ（Play時のみ）
	if (activeCam_ == ActiveCam::Play) {
		player_.Update(CurrentCamera(), input_);

		// Qでレーザー発射
		if (input_.Trigger(DIK_P)) {
			stage_.TriggerLiftBlocks();
			Vector3 start = player_.GetTransform().translate;
			Vector3 dir = player_.GetForwardDir();

			invisivleTime_ = 60; // 予測線を少しの間隠す

			start.x += dir.x * 0.5f;
			start.z += dir.z * 0.5f;
			start.y += 0.0f; // 少し上から撃つ
			laserManager_.Shoot(start, dir);
		}
	}

	// --- コントローラ：Aボタン（立ち上がり）---
	//  敵が生きている間はビーム発射。全滅後はリザルトへ遷移。
	{
		static bool prevA = false; // 立ち上がり検出
		XINPUT_STATE state{};
		bool nowA = false;
		if (XInputGetState(0, &state) == ERROR_SUCCESS) { // 0番パッド
			nowA = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
			stage_.TriggerLiftBlocks();
		}

		if (nowA && !prevA) {
			if (activeCam_ == ActiveCam::Play) {
				bool anyAlive = false;
				for (auto& e : enemies_) {
					if (e.IsAlive()) {
						anyAlive = true;
						break;
					}
				}

				if (anyAlive) {
					// 敵がまだいる → ビーム発射
					Vector3 start = player_.GetTransform().translate;
					Vector3 dir = player_.GetForwardDir();

					invisivleTime_ = 60;
					start.x += dir.x * 0.5f;
					start.z += dir.z * 0.5f;
					start.y += 0.0f;
					laserManager_.Shoot(start, dir);
				} else {
					// 全滅 → リザルトへ
					end_ = true;
					next_ = "Result";
				}
			}
		}

		prevA = nowA;
	}

	// レーザー更新（壁AABB使用）
	laserManager_.Update(stage_.GetWallsDynamic(), stage_.GetPrismWalls(), stage_.GetPrismAngles(), dt);

	// ★ パーティクル更新（重要！）
	enemyParticle_.Update(dt);

	// 敵のレーザー接触フラグ更新（描画色切り替え用）
	static int killCount = 0;
	for (auto& e : enemies_) {
		e.CheckLaserHit(laserManager_);
		e.Update(dt, player_, stage_.GetWalls(), CurrentCamera(), laserManager_);
		if (e.IsAlive() && laserManager_.CheckHitEnemy(e)) {
			killCount++;
			OutputDebugStringA(("Enemy destroyed! total=" + std::to_string(killCount) + "\n").c_str());

			// ★ ヒットストップ発動
			hitStopTimer_ = hitStopDuration_;
			hitStopEnded_ = false;

			// ★ 敵の位置を記録（パーティクルを出す位置）
			lastKillPos_ = e.GetTransform().translate;
		}
	}

	// すべての敵判定が終わったら次のフレームに備えてリセット
	// laserManager_.SetEnhancedMode(laserManager_.IsEnhancedMode()); // 強化状態は保持
	// laserManager_.ResetKillCount();                                // ← 新関数を追加してリセット

	//// 2体以上倒したら強化レーザー解放
	// if (killCount >= 2 && !laserManager_.HasEnhancedLaser()) {
	//	Vector3 start = player_.GetTransform().translate;
	//	Vector3 dir = player_.GetForwardDir();
	//	laserManager_.ShootEnhanced(start, dir, 5.0f); // 5秒持続
	//	killCount = 0;                                 // 1回だけ発動
	// }

	const Engine::Vector3 playerPos = player_.GetPos(); // 足元のワールド座標
	stage_.SetPlayerEffect(
	    playerPos, /*半径*/ 6.0f,
	    /*立ち上がり*/ 0.25f, /*減衰*/ 0.06f);

	// ステージ
	stage_.Update();

	// プレイヤー死亡時のリザルト遷移（Space遷移と同じ形式）
	if (player_.GetHP() <= 0) {
		if (!waitRelease_) {
			end_ = true;
			next_ = "Result";
		}
	}

	if (invisivleTime_ > 0) {
		invisivleTime_--;
	}
}

void GameScene::Draw() {
	if (!dx_ || !dx_->List())
		return;
	auto* cmd = dx_->List();

	renderer_.BeginFrame(cmd);

	// スカイドーム（まずは白乗算：テクスチャそのまま）
	renderer_.UpdateModelCBWithColor(skydomeHandle_, CurrentCamera(), skydomeTf_, {1, 1, 1, 1});
	renderer_.DrawModel(skydomeHandle_, cmd);

	// グリッド
	renderer_.DrawGrid(CurrentCamera(), cmd);

	// ステージ
	stage_.Draw(renderer_, cmd);

	//// =======================
	//// ★ ネオン枠（発光ライン）描画
	//// =======================
	// Transform tf{};
	// tf.scale = {1.0f, 1.0f, 1.0f};
	// tf.translate = {0.0f, 0.5f, 0.0f}; // 任意の位置に

	//// 通常モデルを描画（必要なら）
	// renderer_.UpdateModelCBWithColor(cubeModelHandle_, CurrentCamera(), tf, {1, 1, 1, 1});
	// renderer_.DrawModel(cubeModelHandle_, cmd);

	//// ネオン枠を重ね描き（発光色を指定）
	// renderer_.UpdateModelCBWithColor(cubeModelHandle_, CurrentCamera(), tf, {0.2f, 0.8f, 1.0f, 1.0f}); // シアン系
	// renderer_.DrawModelNeonFrame(cubeModelHandle_, cmd);

	// プレイヤ
	player_.Draw(renderer_, CurrentCamera(), cmd);

	// 敵
	for (auto& e : enemies_) {
		e.Draw(renderer_, CurrentCamera(), cmd);
		e.DrawBullets(renderer_, CurrentCamera(), cmd);
	};

	// 予測線（最大10m / 2反射）— レーザー発射直後は少し隠す
	if (invisivleTime_ <= 0) {
		Vector3 start = player_.GetTransform().translate;
		Vector3 dir = player_.GetForwardDir();
		start.x += dir.x * 0.5f;
		start.z += dir.z * 0.5f;
		start.y += 0.0f;
		laserManager_.DrawPredictionFrom(renderer_, cubeModelHandle_, CurrentCamera(), cmd, stage_.GetWallsDynamic(), stage_.GetPrismWalls(), stage_.GetPrismAngles(), start, dir, 10.0f, 2);

		// ★ 修正ポイント：DrawPredictionFrom の正しい呼び出し
		//    (renderer, model, camera, cmd, walls, start, dir, travel, bounces)
		// laserManager_.DrawPredictionFrom(renderer_, cubeModelHandle_, CurrentCamera(), cmd, stage_.GetWalls(), start, dir, 10.0f, 2);
	}

	// 実レーザー
	laserManager_.Draw(renderer_, lazerModelHandle_, enhanceModelHandle_, CurrentCamera(), cmd);

	enemyParticle_.Draw(renderer_, CurrentCamera(), cmd);

	renderer_.EndFrame(cmd);
}

Camera& GameScene::CurrentCamera() { return (activeCam_ == ActiveCam::Play) ? camPlay_ : camDebug_; }

void GameScene::UpdatePlayCamera(float) {
	camPlay_.SetPosition({0.0f, 20.0f, -25.0f});
	camPlay_.LookAt({0.0f, 0.0f, 0.0f}, {0, 1, 0});
}

void GameScene::UpdateDebugCamera(float dt) {
	bool rDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	POINT cur{};
	GetCursorPos(&cur);
	ScreenToClient(dx_->GetHwnd(), &cur);

	if (rDown) {
		if (rmbPrevDown_) {
			float dx = float(cur.x - prevMouseX_);
			float dy = float(cur.y - prevMouseY_);
			const float sens = 0.0030f;
			dbgYaw_ -= dx * sens;
			dbgPitch_ += dy * sens;
			dbgPitch_ = ClampF(dbgPitch_, -1.25f, 1.25f);
		}
		prevMouseX_ = cur.x;
		prevMouseY_ = cur.y;
		rmbPrevDown_ = true;
	} else {
		rmbPrevDown_ = false;
	}

	auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
	XMVECTOR fwd = XMVector3Normalize(XMVectorSet(std::sin(dbgYaw_) * std::cos(dbgPitch_), std::sin(dbgPitch_), std::cos(dbgYaw_) * std::cos(dbgPitch_), 0));
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(fwd, XMVectorSet(0, 1, 0, 0)));

	XMVECTOR v = XMVectorZero();
	if (down('W'))
		v = XMVectorAdd(v, fwd);
	if (down('S'))
		v = XMVectorSubtract(v, fwd);
	if (down('A'))
		v = XMVectorSubtract(v, right);
	if (down('D'))
		v = XMVectorAdd(v, right);
	if (down('E'))
		v = XMVectorAdd(v, XMVectorSet(0, 1, 0, 0));
	if (down('Q'))
		v = XMVectorSubtract(v, XMVectorSet(0, 1, 0, 0));
	if (!XMVector3Equal(v, XMVectorZero()))
		v = XMVector3Normalize(v);

	const float speed = 8.0f * dt;
	XMVECTOR pos = XMLoadFloat3(&dbgPos_);
	pos = XMVectorAdd(pos, XMVectorScale(v, speed));
	XMStoreFloat3(&dbgPos_, pos);

	camDebug_.SetPosition(dbgPos_);
	XMFLOAT3 look{dbgPos_.x + std::sin(dbgYaw_) * std::cos(dbgPitch_), dbgPos_.y + std::sin(dbgPitch_), dbgPos_.z + std::cos(dbgYaw_) * std::cos(dbgPitch_)};
	camDebug_.LookAt(look, {0, 1, 0});
}

float GameScene::GetDeltaSec() {
	static ULONGLONG prev = GetTickCount64();
	ULONGLONG now = GetTickCount64();
	float dt = float(now - prev) * 0.001f;
	prev = now;
	if (dt > 0.1f)
		dt = 0.1f;
	return dt;
}

//========================================
// ステージ読み込み（推奨版：中心基準対応）
//========================================
void GameScene::LoadStage(int stageIndex) {
	// --- CSVパス構築 ---
	std::string basePath = "Resources/Maps/";
	std::string mapPath = basePath + "Stage" + std::to_string(stageIndex) + "_Map.csv";
	std::string anglePath = basePath + "Stage" + std::to_string(stageIndex) + "_Angle.csv";

	// --- ステージ初期化 ---
	//   ModelOrigin::Center にすることで、Blender Cubeのような「中心原点モデル」と整合
	//   GridAnchor::Center により、CSV全体の中心がワールド原点に一致
	//   tileWidth/tileDepth=1.0f が1マス、gap=0.02f がZ-fighting防止用
	stage_.Initialize(
	    renderer_, *dx_, CurrentCamera(), mapPath, anglePath,
	    1.0f, // tileWidth
	    1.0f, // tileDepth
	    1.0f, // tileHeight
	    Engine::GridAnchor::Center, Engine::ModelOrigin::Center,
	    0.02f, // gapX
	    0.02f  // gapZ
	);

	// --- カメラ再登録 ---
	//   (F1でカメラ切替時にStageが参照するCameraを更新するため)
	stage_.SetCamera(&CurrentCamera());

	// --- デバッグ出力 ---
	char buf[256];
	std::snprintf(buf, sizeof(buf), "[GameScene] Stage%d loaded: %s\n", stageIndex, mapPath.c_str());
	OutputDebugStringA(buf);
}

} // namespace Engine
