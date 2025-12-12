#pragma once
#include "Boss.h"
#include "Camera.h"
#include "Enemy.h"
#include "IScene.h"
#include "Input.h"
#include "Model.h"
#include "ParticleEmitter.h"
#include "Player.h"
#include "Renderer.h"
#include "Sprite2D.h"
#include "SpriteRenderer.h"
#include "TextureManager.h"
#include "Water/WaterSurface.h"
#include "WindowDX.h"
#include <DirectXMath.h> // ← 追加（XMFLOAT3 用）
#include <algorithm>     // ← 追加（std::clamp 用）
#include <chrono>

namespace Engine {

class GameScene : public IScene {
public:
	void Initialize(WindowDX* dx) override;
	void Update() override;
	void Draw() override;

	Game::ParticleEmitter sparks_; // 火花エミッタ

	int outerGroundHandle_ = -1;
	Engine::Transform outerGroundTf_;

	// Fキー：プレイヤーから常時パーティクルを出す ON/OFF
	bool particleAutoEmit_ = false;

	// Gキー：風の ON/OFF
	bool particleWindOn_ = false;

	// 風OFF時の“元の速度パラメータ”を保存しておく
	Engine::Vector3 sparkVelBaseMin_{};
	Engine::Vector3 sparkVelBaseMax_{};

	bool IsEnd() const override { return end_; }
	std::string Next() const override { return next_; }

private:
	Engine::SpriteRenderer* sprite_ = nullptr;
	WindowDX* dx_ = nullptr;
	Renderer renderer_;
	Input input_;

	Camera camPlay_;
	Camera camDebug_;
	Camera* activeCam_ = nullptr;
	bool useDebugCam_ = false;
	bool prevF1_ = false;

	Player player_;

	// UIで使いたいメンバを追加
	Anchor::Sprite2D testSprite_;

	std::unique_ptr<Engine::WaterSurface> water_;

	// ヒットストップ
	float hitStopTimer_ = 0.0f; // 残りヒットストップ時間（秒）
	float hitStopScale_ = 0.1f; // ヒットストップ中のスケール（0で完全停止）
	float debugBaseDt_ = 0.0f;  // 生のdt（1/60）
	float debugGameDt_ = 0.0f;  // ヒットストップ適用後のdt

	// ==== 敵スポーン制御 ====
	float enemySpawnTimer_ = 0.0f;    // 経過時間
	float enemySpawnInterval_ = 3.0f; // 何秒ごとにスポーンするか
	int enemyMaxCount_ = 15;          // 同時に存在できる最大数
	float enemySpawnRadius_ = 18.0f;  // プレイヤー中心のスポーン半径

	void DrawImGui_();

	std::unique_ptr<Game::Boss> boss_;
	void ApplyBossHitToTerrain_(const Game::TerrainHitInfo& info); // 地形を凹ませる用

	bool gridVisible_ = true;

	bool end_ = false;
	std::string next_;
	bool waitRelease_ = true;
	bool prevPressed_ = false;

	// === TPSカメラ制御用 ===
	float camYaw_ = 0.0f;
	float camPitch_ = 0.3f;
	float camDist_ = 8.0f;
	float camHeight_ = 2.0f;
	DirectX::XMFLOAT3 camPos_{0, 0, 0}; // 現在のカメラ実座標（lerp用）

	// マウスΔを自前で取る（Input に無いので）
	bool firstMouse_ = true;
	LONG prevMouseX_ = 0;
	LONG prevMouseY_ = 0;

	bool prevRB_ = false; // 右クリックの前フレーム状態
	bool cursorFree_ = false;

	// == FPS 計測用 ==
	float fps_ = 0.0f;
	int fpsFrameCount_ = 0;
	std::chrono::steady_clock::time_point fpsLastTime_;
};

} // namespace Engine
