#pragma once

#include "Camera.h"
#include "Input.h"
#include "Matrix4x4.h"

#include "Renderer.h"
#include "Transform.h"
#include <DirectXMath.h>

namespace Engine {

class Player {
public:
	// 初期化
	void Initialize(Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd);
	// 更新
	void Update(const Camera& cam, const Input& input);
	// 描画
	//	void Draw(Renderer& renderer, ID3D12GraphicsCommandList* cmd);

	void Draw(Renderer& renderer, const Camera& cam, ID3D12GraphicsCommandList* cmd);

	void Damage(int dmg) { hp_ = (std::max)(0, hp_ - dmg); }
	int GetHP() const { return hp_; }

	// プレイヤー位置取得
	Vector3 GetPos() const { return transform_.translate; }
	// 進行方向取得（現状は固定）
	Vector3 GetForwardDir() const;

	void SetPos(const Vector3& p) { transform_.translate = p; }

	const Transform& GetTransform() const { return transform_; }

private:
	Transform transform_;         // 位置・回転・スケール
	const Camera* cam_ = nullptr; // カメラ参照用
	int modelHandle_ = -1;        // モデルハンドル
	float speed_ = 0.1f;          // 移動速度
	int hp_ = 3;

	Vector3 lastMoveDir_{0, 0, 1}; // 最後に移動した方向（初期はZ+）

	float rotateSpeed = 0.05f;

	bool onGround_ = true;      // 地面についているか
	bool canDoubleJump_ = true; // 2段目のジャンプ可否
	float velocityY_ = 0.0f;    // 上下速度
	float gravity_ = - 5.0f;     // 重力加速度
	float jumpPower_ = 100.0f;   // ジャンプ初速度

	// --- レーザー準備／発射管理 ---
	bool isCharging_ = false;         // 長押し中（発射準備）
	bool firedThisFrame_ = false;     // 今フレーム離して発射した
	float chargeTime_ = 0.0f;         // 今回の準備経過秒
	float lastChargeDuration_ = 0.0f; // 直近の準備時間（離した時に確定）
};

} // namespace Engine
