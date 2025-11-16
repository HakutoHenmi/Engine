#pragma once
#include <d3d12.h>
#include <functional>

#include "Transform.h" // Vector3 / Transform

namespace Engine {
class Renderer;
class WindowDX;
class Camera;
struct Vector3;
struct Vector4;
} // namespace Engine

namespace Game {

// 地形を凹ませるための情報
struct TerrainHitInfo {
	Engine::Vector3 position; // ヒット中心位置
	float radius = 3.0f;      // どのくらい広く凹ませるか
	float depth = 1.0f;       // どのくらい深く凹ませるか（+で下方向）
};

class Boss {
public:
	// ボスの状態
	enum class State {
		Waiting, // 次の攻撃までの待機
		Aim,     // プレイヤー方向を狙っている
		Charge,  // 溜め（後ろに反る）
		Slam,    // 叩きつけ
		Stuck,   // 地面に刺さって少し止まる
		Recover  // 元の位置に戻る
	};

	Boss() = default;

	// renderer / dx を受け取ってモデル読み込み
	void Initialize(Engine::Renderer& renderer, Engine::WindowDX& dx);

	// dt: 経過時間, playerPos: プレイヤー位置
	void Update(float dt, const Engine::Vector3& playerPos);

	// 描画
	void Draw(Engine::Renderer& renderer, ID3D12GraphicsCommandList* cmd, const Engine::Camera& cam);

	// ボスの根元（回転の支点）位置
	Engine::Vector3 GetPosition() const { return rootPos_; }

	// ★位置を変えたら必ず Transform も更新する
	void SetPosition(const Engine::Vector3& pos) {
		rootPos_ = pos;
		UpdateTransformFromPose_();
	}

	// 地形を凹ませる処理を登録
	void SetTerrainHitCallback(std::function<void(const TerrainHitInfo&)> cb) { terrainHitCallback_ = std::move(cb); }

	// XZ の位置から、その地点でのボクセル地形の高さ(Y)を返すコールバック
	void SetTerrainHeightCallback(std::function<float(const Engine::Vector3&)> cb) { terrainHeightCallback_ = std::move(cb); }

private:
	// 状態更新
	void UpdateWaiting_(float dt, const Engine::Vector3& playerPos);
	void UpdateAim_(float dt, const Engine::Vector3& playerPos);
	void UpdateCharge_(float dt);
	void UpdateSlam_(float dt);
	void UpdateStuck_(float dt);
	void UpdateRecover_(float dt);

	// 状態遷移ヘルパー
	void BeginAim_(const Engine::Vector3& playerPos);
	void BeginCharge_();
	void BeginSlam_();
	void BeginStuck_();
	void BeginRecover_();

	// Transform 更新
	void UpdateTransformFromPose_();

	// 現在の傾きから「棒の先端」のワールド座標を計算
	Engine::Vector3 GetTipPosition_() const;

	// 地形への「ここを凹ませて」通知
	void NotifyTerrainHit_();

private:
	int modelHandle_ = -1; // 棒モデル（bou.obj）
	Engine::Transform tf_{};

	// 棒の全長
	float stickLength_ = 25.0f;

	// 根元（回転の支点）のワールド座標
	Engine::Vector3 rootPos_{0.0f, 0.0f, 0.0f};

	// 状態管理
	State state_ = State::Waiting;
	float stateTimer_ = 0.0f;

	// 各フェーズの時間
	float waitTime_ = 1.5f;    // 攻撃間隔
	float aimTime_ = 1.0f;     // 狙う時間（テレグラフ）
	float chargeTime_ = 0.5f;  // 溜め
	float slamTime_ = 0.25f;   // 叩きつけ（速く）
	float stuckTime_ = 0.3f;   // 刺さったまま止まる
	float recoverTime_ = 0.6f; // 戻り

	// 傾き角度（0 = 真上、正の方向に fallAxis_ 側へ倒れていく）
	float currentAngle_ = 0.0f; // ラジアン
	float maxAngle_ = 1.6f;     // 最大でここまで倒す（約 90°ちょい）

	// 倒れる向き（XZ 平面の単位ベクトル）
	Engine::Vector3 fallAxis_{1.0f, 0.0f, 0.0f};

	// Aim での補間用
	Engine::Vector3 aimStartAxis_{1.0f, 0.0f, 0.0f};
	Engine::Vector3 aimTargetAxis_{1.0f, 0.0f, 0.0f};

	// Recover で使う開始角度 / Stuck での固定角
	float recoverStartAngle_ = 0.0f;
	float stuckAngle_ = 0.0f;

	// 地形ヒット通知フラグ
	bool terrainHitNotified_ = false;

	// コールバック
	std::function<void(const TerrainHitInfo&)> terrainHitCallback_;
	std::function<float(const Engine::Vector3&)> terrainHeightCallback_;
};

} // namespace Game
