// GameScene.h — 完全版
#pragma once
// =====================================================
// GameScene : プレイカメラ / デバッグカメラ / ステージ / 敵 / レーザー統合
// =====================================================
#include <DirectXMath.h>
#include <string>

#include "Camera.h"
#include "IScene.h"
#include "Input.h"
#include "Renderer.h"
#include "WindowDX.h"

#include "Enemy.h"
#include "LaserManager.h"
#include "ParticleSystem.h"
#include "Player.h"
#include "Stage.h"

namespace Engine {

class GameScene : public IScene {
public:
	void Initialize(WindowDX* dx) override;
	void Update() override;
	void Draw() override;

	bool IsEnd() const override { return end_; }
	std::string Next() const override { return next_; }

	void LoadStage(int stageIndex); // ステージ読み込み

private:
	// ========= カメラ =========
	enum class ActiveCam { Play, Debug };
	Camera& CurrentCamera();
	void UpdatePlayCamera(float dt);
	void UpdateDebugCamera(float dt);

	// ========= 時間 =========
	float GetDeltaSec();

private:
	// ========= シーン制御 =========
	bool end_ = false;
	std::string next_;

	// ========= 基盤 =========
	WindowDX* dx_ = nullptr;
	Renderer renderer_;
	Input input_;

	// ========= ワールド =========
	Stage stage_;
	Player player_;
	std::vector<Enemy> enemies_;
	LaserManager laserManager_;

	int skydomeHandle_ = -1;
	Transform skydomeTf_;

	// 予測線などで使う共通モデル
	int cubeModelHandle_ = -1;
	int lazerModelHandle_ = -1;
	int enhanceModelHandle_ = -1;
	int particleModelHandle_ = -1;
	// ========= カメラ =========
	ActiveCam activeCam_ = ActiveCam::Play;

	Camera camPlay_;
	Camera camDebug_;

	// パーティクル
	ParticleSystem particle;

	// Debugカメラ状態
	DirectX::XMFLOAT3 dbgPos_{0.0f, 12.0f, -30.0f};
	float dbgYaw_ = 0.0f;
	float dbgPitch_ = 0.20f;
	bool rmbPrevDown_ = false;
	long prevMouseX_ = 0, prevMouseY_ = 0;

	// キーの立ち上がり用
	bool prevF1_ = false;
	bool prevSpace_ = false;
	bool waitRelease_ = true;

	// レーザー予測を一時的に隠すためのカウンタ
	int invisivleTime_ = 0;

	float hitStopTimer_ = 0.0f;
	const float hitStopDuration_ = 0.1f; // 0.1秒停止

	// ★ ヒットストップ後のエフェクト用
	bool hitStopEnded_ = false;
	Vector3 lastKillPos_{};

	// 敵用のパーティクル
	ParticleSystem enemyParticle_;
};

} // namespace Engine
