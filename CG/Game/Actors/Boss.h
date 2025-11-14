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
	enum class State {
		Idle,    // 立っている
		Windup,  // 溜め
		Falling, // 倒れている
		Recover  // 元に戻る
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
	void StartAttack_(const Engine::Vector3& playerPos);
	void UpdateIdle_(float dt, const Engine::Vector3& playerPos);
	void UpdateWindup_(float dt);
	void UpdateFalling_(float dt);
	void UpdateRecover_(float dt);

	// 現在の傾き(currentAngle_)と根元(rootPos_)にもとづいて
	// Transform の平行移動/回転を更新（棒の根元が常に rootPos_ に来るようにする）
	void UpdateTransformFromPose_();

	// 現在の傾きから「棒の先端」のワールド座標を計算
	Engine::Vector3 GetTipPosition_() const;

	void NotifyTerrainHit_();

private:
	int modelHandle_ = -1; // cube.obj
	Engine::Transform tf_{};

	// 棒の全長
	float stickLength_ = 25.0f;

	// 根元（回転の支点）のワールド座標
	Engine::Vector3 rootPos_{0.0f, 0.0f, 0.0f};

	State state_ = State::Idle;
	float stateTimer_ = 0.0f;

	float idleTime_ = 2.0f;
	float windupTime_ = 0.6f;
	float fallingTime_ = 1.0f;
	float recoverTime_ = 0.7f;

	// 傾き角度（0 = 真上、正の方向に fallAxis_ 側へ倒れていく）
	float currentAngle_ = 0.0f; // ラジアン
	float maxAngle_ = 1.6f;     // 最大でここまで倒す（約 90°ちょい）

	// 倒れる向き（XZ平面の単位ベクトル）
	Engine::Vector3 fallAxis_{1.0f, 0.0f, 0.0f};

	bool terrainHitNotified_ = false;
	std::function<void(const TerrainHitInfo&)> terrainHitCallback_;
	std::function<float(const Engine::Vector3&)> terrainHeightCallback_;
};

} // namespace Game
