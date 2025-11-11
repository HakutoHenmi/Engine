#pragma once

#include "Camera.h"
#include "Input.h"
#include "Matrix4x4.h"

#include "Actors/ParticleEmitter.h"
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

	void SetVelocityY(float v) { velocityY_ = v; }

	float GetVelocityY() const { return velocityY_; }

	void SetPos(const Vector3& p) { transform_.translate = p; }

	const Transform& GetTransform() const { return transform_; }

	void SetSwordModel(int handle) { swordHandle_ = handle; }

	// 命中演出（外部のヒット判定から呼ぶ）
	void TriggerHitEffect(const Vector3& hitPos, const Vector3& hitNormal);

	// 当たりカプセルを返す
	bool GetMeleeHitCapsule(Vector3& p0, Vector3& p1, float& r) const {
		if (!meleeHitActive_)
			return false;
		p0 = meleeCapsuleP0_;
		p1 = meleeCapsuleP1_;
		r = meleeCapsuleR_;
		return true;
	}

	// ==== パーティクル（攻撃用）====
	Game::ParticleEmitter swingEmitter_; // 振り始めの風切り（瞬間）
	Game::ParticleEmitter trailEmitter_; // スイング軌跡（追従・小粒）
	Game::ParticleEmitter hitEmitter_;   // 命中スパーク（瞬間）

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
	float gravity_ = -5.0f;     // 重力加速度
	float jumpPower_ = 100.0f;  // ジャンプ初速度

	// --- レーザー準備／発射管理 ---
	bool isCharging_ = false;         // 長押し中（発射準備）
	bool firedThisFrame_ = false;     // 今フレーム離して発射した
	float chargeTime_ = 0.0f;         // 今回の準備経過秒
	float lastChargeDuration_ = 0.0f; // 直近の準備時間（離した時に確定）

	// --- 緊急回避（ステップ） ---
	bool dodgeActive_ = false;
	float dodgeTimer_ = 0.0f;     // 発動中の残り時間
	float dodgeDuration_ = 0.18f; // 回避の継続時間（秒）
	float dodgeSpeed_ = 30.0f;    // 回避の速度（m/s）
	float dodgeCooldown_ = 0.55f; // 次に出せるまでの待ち時間（秒）
	float dodgeCooldownTimer_ = 0.0f;

	Vector3 dodgeDir_{0, 0, 0}; // 進む方向（XZ 正規化）
	bool prevRightBtn_ = false; // 右クリックのエッジ検出

	// --- 剣モデル ---
	int swordHandle_ = -1;
	Transform swordTf_;    // ワールド（毎フレーム計算）
	Transform swordLocal_; // プレイヤー基準のローカル（握り位置）

	// --- 近接攻撃（横薙ぎ）---
	bool meleeSwing_ = false;
	float meleeTime_ = 0.0f;
	float meleeDuration_ = 0.48f; // 1振りの時間
	float meleeCooldown_ = 0.35f; // 連発防止
	float meleeCooldownTimer_ = 0.0f;
	float meleeAngStartDeg_ = -70.0f; // 左後ろ → 右前
	float meleeAngEndDeg_ = 70.0f;

	// --- 剣の位置オフセット（前方距離・高さ）---
	float swordForwardDist_ = 0.62f; // プレイヤー“前”方向へ距離
	float swordUpOffset_ = 0.90f;    // プレイヤー“上”方向へ距離

	bool prevLeftBtn_ = false; // マウス左のエッジ検出

	// --- 当たり判定（カプセル）---
	bool meleeHitActive_ = false;
	Vector3 meleeCapsuleP0_{0, 0, 0};
	Vector3 meleeCapsuleP1_{0, 0, 0};
	float meleeCapsuleR_ = 0.55f; // 半径（お好みで）

	// --- 近接攻撃コンボ制御 ---
	int comboStep_ = 0;                // 現在のコンボ段階（1〜3）
	float comboTimer_ = 0.0f;          // コンボ継続受付時間タイマー
	const float comboInterval_ = 1.0f; // この秒数以内に押せば次へ繋がる

	// === 近接モーションの連結用（前回→今回） ===
	float swordIdleYaw_ = -10.0f;  // 待機の見た目（Yaw）
	float swordIdleTiltZ_ = 0.0f;  // 待機の傾き
	float swordIdlePitchX_ = 0.0f; // 待機の俯仰

	// 現在のスイングの開始/終了角（Yaw/TiltZ/PitchX）
	float swingYawStart_ = 0.0f, swingYawEnd_ = 0.0f;
	float swingTiltStart_ = 0.0f, swingTiltEnd_ = 0.0f;
	float swingPitchStart_ = 0.0f, swingPitchEnd_ = 0.0f;

	// 直近スイングの終了角（= 次回開始の基準）
	float prevEndYaw_ = -10.0f;
	float prevEndTiltZ_ = 0.0f;
	float prevEndPitchX_ = 0.0f;

	bool hasPrevSwing_ = false; // 最初だけアイドルから始める

	// 剣の最終ワールド姿勢を作る（位置は“常に前側”、向きは yaw+addYawDeg）
	Transform MakeSwordWorld_(float yawRad, float addYawDeg, float tiltZDeg = 0.0f, float pitchXDeg = 0.0f) const;

};

} // namespace Engine
