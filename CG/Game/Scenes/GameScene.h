#pragma once
#include "Boss.h"
#include "Camera.h"
#include "IScene.h"
#include "Input.h"
#include "ParticleEmitter.h"
#include "Player.h"
#include "Renderer.h"
#include "Stage.h"
#include "WindowDX.h"
#include <DirectXMath.h> // ← 追加（XMFLOAT3 用）
#include <algorithm>     // ← 追加（std::clamp 用）

namespace Engine {

class GameScene : public IScene {
public:
	void Initialize(WindowDX* dx) override;
	void Update() override;
	void Draw() override;

	Game::ParticleEmitter sparks_; // 火花エミッタ

	bool IsEnd() const override { return end_; }
	std::string Next() const override { return next_; }

private:
	WindowDX* dx_ = nullptr;
	Renderer renderer_;
	Input input_;

	Camera camPlay_;
	Camera camDebug_;
	Camera* activeCam_ = nullptr;
	bool useDebugCam_ = false;
	bool prevF1_ = false;

	Stage stage_;
	Player player_;

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
};

} // namespace Engine
