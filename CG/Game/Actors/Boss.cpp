#include "Boss.h"

#include <cmath>

#include "Camera.h"
#include "Renderer.h"
#include "WindowDX.h"

namespace Game {

using Engine::Vector3;
using Engine::Vector4;

static constexpr float PI = 3.1415926535f;

// 簡単なユーティリティ
static Vector3 MakeVec3(float x, float y, float z) { return Vector3{x, y, z}; }

// XZ だけの正規化
static void NormalizeXZ(Vector3& v) {
	float len = std::sqrt(v.x * v.x + v.z * v.z);
	if (len > 0.0001f) {
		v.x /= len;
		v.z /= len;
	} else {
		v.x = 0.0f;
		v.z = 1.0f;
	}
}

//-------------------------------------------
// 初期化
//-------------------------------------------
void Boss::Initialize(Engine::Renderer& renderer, Engine::WindowDX& dx) {
	// Resources/cube/cube.obj を棒として使う
	modelHandle_ = renderer.LoadModel(dx.Dev(), dx.List(), "Resources/Boss/bou.obj");

	stickLength_ = 25.0f; // 好きな長さ

	tf_.scale = MakeVec3(1.0f, stickLength_, 1.0f); // Y方向に伸ばす
	tf_.rotate = MakeVec3(0.0f, 0.0f, 0.0f);
	tf_.translate = MakeVec3(0.0f, 0.0f, 0.0f); // 実際の位置は UpdateTransformFromPose_ で決める

	rootPos_ = MakeVec3(0.0f, 0.0f, 0.0f); // GameScene 側で SetPosition してもOK

	currentAngle_ = 0.0f;
	maxAngle_ = PI * 0.5f + PI * 0.1f; // 90°ちょい

	fallAxis_ = MakeVec3(1.0f, 0.0f, 0.0f); // 初期値。攻撃ごとにプレイヤー方向に更新する

	state_ = State::Idle;
	stateTimer_ = 0.0f;

	UpdateTransformFromPose_();
}

//-------------------------------------------
// Update
//-------------------------------------------
void Boss::Update(float dt, const Vector3& playerPos) {
	stateTimer_ += dt;

	switch (state_) {
	case State::Idle:
		UpdateIdle_(dt, playerPos);
		break;
	case State::Windup:
		UpdateWindup_(dt);
		break;
	case State::Falling:
		UpdateFalling_(dt);
		break;
	case State::Recover:
		UpdateRecover_(dt);
		break;
	default:
		break;
	}

	// 角度が変わったら Transform を更新
	UpdateTransformFromPose_();
}

//-------------------------------------------
// Draw
//-------------------------------------------
void Boss::Draw(Engine::Renderer& renderer, ID3D12GraphicsCommandList* cmd, const Engine::Camera& cam) {
	if (modelHandle_ < 0)
		return;

	renderer.UpdateModelCBWithColor(modelHandle_, cam, tf_, Vector4{1, 1, 1, 1});
	renderer.DrawModel(modelHandle_, cmd);
}

//-------------------------------------------
// Idle → 一定時間ごとに攻撃開始
//-------------------------------------------
void Boss::UpdateIdle_(float /*dt*/, const Vector3& playerPos) {
	if (stateTimer_ >= idleTime_) {
		StartAttack_(playerPos);
	}
}

//-------------------------------------------
// 攻撃開始：プレイヤー方向を計算
//-------------------------------------------
void Boss::StartAttack_(const Vector3& playerPos) {
	state_ = State::Windup;
	stateTimer_ = 0.0f;
	terrainHitNotified_ = false;

	// プレイヤー方向（XZ）
	Vector3 toPlayer{playerPos.x - rootPos_.x, 0.0f, playerPos.z - rootPos_.z};
	NormalizeXZ(toPlayer);

	// ★ここを反転させる：プレイヤー側に倒れるようにする
	fallAxis_.x = -toPlayer.x;
	fallAxis_.y = 0.0f;
	fallAxis_.z = -toPlayer.z;

	currentAngle_ = 0.0f; // 真上からスタート
}

//-------------------------------------------
// 溜め：少し後ろに反る
//-------------------------------------------
void Boss::UpdateWindup_(float /*dt*/) {
	float t = (windupTime_ > 0.0f) ? (stateTimer_ / windupTime_) : 1.0f;
	if (t > 1.0f)
		t = 1.0f;

	// ちょっとだけ反る（マイナス方向）
	float backAngle = -PI * 0.1f;
	currentAngle_ = backAngle * t;

	if (stateTimer_ >= windupTime_) {
		state_ = State::Falling;
		stateTimer_ = 0.0f;
		currentAngle_ = 0.0f; // 真上から倒し始める
	}
}

//-------------------------------------------
// 倒れ：地面に当たるまで倒す
//-------------------------------------------
void Boss::UpdateFalling_(float /*dt*/) {
	float t = (fallingTime_ > 0.0f) ? (stateTimer_ / fallingTime_) : 1.0f;
	if (t > 1.0f)
		t = 1.0f;

	// 0 → maxAngle_ に向かって増加
	currentAngle_ = maxAngle_ * t;

	// 棒の先端と地形の衝突判定
	bool hitGround = false;
	if (terrainHeightCallback_) {
		Vector3 tip = GetTipPosition_();
		float groundY = terrainHeightCallback_(tip);
		const float eps = 0.02f; // 少しだけめり込みを許す
		if (tip.y <= groundY + eps) {
			hitGround = true;
		}
	}

	if (hitGround) {
		if (!terrainHitNotified_) {
			NotifyTerrainHit_();
			terrainHitNotified_ = true;
		}
		state_ = State::Recover;
		stateTimer_ = 0.0f;
		return;
	}

	// 安全策：時間切れになったら強制的に戻る
	if (stateTimer_ >= fallingTime_) {
		if (!terrainHitNotified_) {
			NotifyTerrainHit_();
			terrainHitNotified_ = true;
		}
		state_ = State::Recover;
		stateTimer_ = 0.0f;
	}
}

//-------------------------------------------
// 復帰：角度を 0 に戻す
//-------------------------------------------
void Boss::UpdateRecover_(float /*dt*/) {
	float t = (recoverTime_ > 0.0f) ? (stateTimer_ / recoverTime_) : 1.0f;
	if (t > 1.0f)
		t = 1.0f;

	float start = currentAngle_;
	float end = 0.0f;

	currentAngle_ = start + (end - start) * t;

	if (stateTimer_ >= recoverTime_) {
		state_ = State::Idle;
		stateTimer_ = 0.0f;
		currentAngle_ = 0.0f;
	}
}

//-------------------------------------------
// Transform を更新（倒れるとき前進／戻るとき後退でごまかし）
//-------------------------------------------
void Boss::UpdateTransformFromPose_() {
	// 角度から倒れ方向ベクトルを作成
	float s = std::sin(currentAngle_);
	float c = std::cos(currentAngle_);

	// 倒れ方向（Y成分込み）
	Vector3 dir;
	dir.x = fallAxis_.x * s;
	dir.y = c;
	dir.z = fallAxis_.z * s;

	// ---- ここで「前進／後退」の量を決める ----
	// currentAngle_ が 0 → 0, maxAngle_ → slideMax になるように補間
	const float slideMax = 2.0f; // ☆お好みで距離を調整（前にどれくらい出すか）
	float t = 0.0f;
	if (maxAngle_ > 0.0001f) {
		t = currentAngle_ / maxAngle_;
		if (t < 0.0f)
			t = 0.0f;
		if (t > 1.0f)
			t = 1.0f;
	}

	float slide = slideMax * t;

	// 倒れ方向の水平成分(fallAxis_)に沿ってスライド
	Vector3 base = rootPos_;
	base.x += fallAxis_.x * slide;
	base.z += fallAxis_.z * slide;
	// base.y は rootPos_.y のまま

	// モデルは「中心が原点で、Y方向に1.0長さ」を前提
	float half = stickLength_ * 0.5f;

	// 中心 = スライド後の根元 + dir * (長さの半分)
	tf_.translate.x = base.x + dir.x * half;
	tf_.translate.y = base.y + dir.y * half;
	tf_.translate.z = base.z + dir.z * half;

	// 回転：倒れる向き（yaw）＋ ピッチ
	float yaw = std::atan2(fallAxis_.x, fallAxis_.z); // XZ 平面方向
	float pitch = -currentAngle_;                     // 手前にバタン

	tf_.rotate.x = pitch;
	tf_.rotate.y = yaw;
	tf_.rotate.z = 0.0f;

	// スケール（長さ）はそのまま
	tf_.scale.y = stickLength_;
}

//-------------------------------------------
// 棒の先端ワールド座標（前進/後退込み）
//-------------------------------------------
Vector3 Boss::GetTipPosition_() const {
	float s = std::sin(currentAngle_);
	float c = std::cos(currentAngle_);

	Vector3 dir;
	dir.x = fallAxis_.x * s;
	dir.y = c;
	dir.z = fallAxis_.z * s;

	// UpdateTransformFromPose_ と同じスライド量を計算
	const float slideMax = 2.0f;
	float t = 0.0f;
	if (maxAngle_ > 0.0001f) {
		t = currentAngle_ / maxAngle_;
		if (t < 0.0f)
			t = 0.0f;
		if (t > 1.0f)
			t = 1.0f;
	}
	float slide = slideMax * t;

	Vector3 base = rootPos_;
	base.x += fallAxis_.x * slide;
	base.z += fallAxis_.z * slide;

	// 先端 = スライド後の根元 + dir * stickLength_
	Vector3 tip;
	tip.x = base.x + dir.x * stickLength_;
	tip.y = base.y + dir.y * stickLength_;
	tip.z = base.z + dir.z * stickLength_;
	return tip;
}

//-------------------------------------------
// 地形への「ここを凹ませて」通知
//-------------------------------------------
void Boss::NotifyTerrainHit_() {
	if (!terrainHitCallback_)
		return;

	TerrainHitInfo info{};
	info.position = GetTipPosition_(); // 実際に当たった先端位置
	info.radius = 4.0f;                // 凹みの広さ（お好みで調整）
	info.depth = 1.5f;                 // 凹みの深さ

	terrainHitCallback_(info);
}

} // namespace Game
