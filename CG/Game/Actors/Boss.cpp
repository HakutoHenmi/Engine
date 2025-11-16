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
	// Resources/Boss/bou.obj を棒として使う
	modelHandle_ = renderer.LoadModel(dx.Dev(), dx.List(), "Resources/Boss/bou.obj");

	stickLength_ = 25.0f; // 好きな長さ

	tf_.scale = MakeVec3(1.0f, stickLength_, 1.0f); // Y方向に伸ばす
	tf_.rotate = MakeVec3(0.0f, 0.0f, 0.0f);
	tf_.translate = MakeVec3(0.0f, 0.0f, 0.0f); // 実際の位置は UpdateTransformFromPose_ で決める

	rootPos_ = MakeVec3(0.0f, 0.0f, 0.0f); // GameScene 側で SetPosition してもOK

	currentAngle_ = 0.0f;
	maxAngle_ = PI * 0.5f + PI * 0.1f; // 90°ちょい

	fallAxis_ = MakeVec3(1.0f, 0.0f, 0.0f);
	aimStartAxis_ = fallAxis_;
	aimTargetAxis_ = fallAxis_;

	state_ = State::Waiting;
	stateTimer_ = 0.0f;

	terrainHitNotified_ = false;

	UpdateTransformFromPose_();
}

//-------------------------------------------
// Update
//-------------------------------------------
void Boss::Update(float dt, const Vector3& playerPos) {
	stateTimer_ += dt;

	switch (state_) {
	case State::Waiting:
		UpdateWaiting_(dt, playerPos);
		break;
	case State::Aim:
		UpdateAim_(dt, playerPos);
		break;
	case State::Charge:
		UpdateCharge_(dt);
		break;
	case State::Slam:
		UpdateSlam_(dt);
		break;
	case State::Stuck:
		UpdateStuck_(dt);
		break;
	case State::Recover:
		UpdateRecover_(dt);
		break;
	default:
		break;
	}

	// 角度や向きが変わったら Transform を更新
	UpdateTransformFromPose_();
}

//-------------------------------------------
// Draw
//-------------------------------------------
void Boss::Draw(Engine::Renderer& renderer, ID3D12GraphicsCommandList* cmd, const Engine::Camera& cam) {
	if (modelHandle_ < 0)
		return;

	// 状態によって色を変えて「攻撃っぽさ」を出す
	Vector4 col{1, 1, 1, 1};
	switch (state_) {
	case State::Waiting:
		col = Vector4{0.8f, 0.8f, 0.8f, 1.0f};
		break;
	case State::Aim:
		col = Vector4{1.0f, 1.0f, 0.3f, 1.0f}; // 黄色（狙い中）
		break;
	case State::Charge:
		col = Vector4{1.0f, 0.6f, 0.2f, 1.0f}; // オレンジ（溜め）
		break;
	case State::Slam:
		col = Vector4{1.0f, 0.25f, 0.2f, 1.0f}; // 赤（攻撃中）
		break;
	case State::Stuck:
		col = Vector4{0.9f, 0.1f, 0.1f, 1.0f}; // 濃い赤（刺さり）
		break;
	case State::Recover:
		col = Vector4{0.7f, 0.7f, 1.0f, 1.0f}; // 青寄り（戻り）
		break;
	}

	renderer.UpdateModelCBWithColor(modelHandle_, cam, tf_, col);
	renderer.DrawModel(modelHandle_, cmd);
}

//-------------------------------------------
// Waiting → 一定時間ごとに Aim へ
//-------------------------------------------
void Boss::UpdateWaiting_(float /*dt*/, const Vector3& playerPos) {
	// ちょっとだけプルプルさせて「生きてる」感じに
	currentAngle_ = 0.02f * std::sin(stateTimer_ * 2.0f);

	if (stateTimer_ >= waitTime_) {
		BeginAim_(playerPos);
	}
}

//-------------------------------------------
// Aim 開始：プレイヤー方向を狙い始める
//-------------------------------------------
void Boss::BeginAim_(const Vector3& playerPos) {
	state_ = State::Aim;
	stateTimer_ = 0.0f;

	aimStartAxis_ = fallAxis_;

	// プレイヤー方向（XZ）
	Vector3 toPlayer{playerPos.x - rootPos_.x, 0.0f, playerPos.z - rootPos_.z};
	NormalizeXZ(toPlayer);

	// ★符号を反転させない
	aimTargetAxis_.x = toPlayer.x;
	aimTargetAxis_.y = 0.0f;
	aimTargetAxis_.z = toPlayer.z;

	terrainHitNotified_ = false;
}

//-------------------------------------------
// Aim：方向補間＆ガクガクするテレグラフ
//-------------------------------------------
void Boss::UpdateAim_(float /*dt*/, const Vector3& /*playerPos*/) {
	float t = (aimTime_ > 0.0f) ? (stateTimer_ / aimTime_) : 1.0f;
	if (t > 1.0f)
		t = 1.0f;

	// fallAxis_ をプレイヤー方向へ補間
	Vector3 axis{};
	axis.x = aimStartAxis_.x + (aimTargetAxis_.x - aimStartAxis_.x) * t;
	axis.y = 0.0f;
	axis.z = aimStartAxis_.z + (aimTargetAxis_.z - aimStartAxis_.z) * t;
	fallAxis_ = axis;
	NormalizeXZ(fallAxis_);

	// 狙い中は小さく揺れる
	currentAngle_ = 0.05f * std::sin(stateTimer_ * 10.0f);

	if (stateTimer_ >= aimTime_) {
		BeginCharge_();
	}
}

//-------------------------------------------
// Charge 開始：溜めに入る
//-------------------------------------------
void Boss::BeginCharge_() {
	state_ = State::Charge;
	stateTimer_ = 0.0f;
}

//-------------------------------------------
// Charge：後ろに反る
//-------------------------------------------
void Boss::UpdateCharge_(float /*dt*/) {
	float t = (chargeTime_ > 0.0f) ? (stateTimer_ / chargeTime_) : 1.0f;
	if (t > 1.0f)
		t = 1.0f;

	// イージング（スムーズステップ）
	float tt = t * t * (3.0f - 2.0f * t);

	float backAngle = -PI * 0.35f; // ちょっと後ろに反る
	currentAngle_ = backAngle * tt;

	if (stateTimer_ >= chargeTime_) {
		BeginSlam_();
	}
}

//-------------------------------------------
// Slam 開始：叩きつけフェーズへ
//-------------------------------------------
void Boss::BeginSlam_() {
	state_ = State::Slam;
	stateTimer_ = 0.0f;
	terrainHitNotified_ = false;
}

//-------------------------------------------
// Slam：素早く叩きつける（地面ヒットで凹ませ通知）
//-------------------------------------------
void Boss::UpdateSlam_(float /*dt*/) {
	float t = (slamTime_ > 0.0f) ? (stateTimer_ / slamTime_) : 1.0f;
	if (t > 1.0f)
		t = 1.0f;

	// 速く加速して叩きつけるイメージ
	float tt = t * t * (3.0f - 2.0f * t);

	float backAngle = -PI * 0.35f;
	currentAngle_ = backAngle + (maxAngle_ - backAngle) * tt;

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
		BeginStuck_();
		return;
	}

	// 時間切れでも一応ヒット扱いにしておく
	if (stateTimer_ >= slamTime_) {
		if (!terrainHitNotified_) {
			NotifyTerrainHit_();
			terrainHitNotified_ = true;
		}
		BeginStuck_();
	}
}

//-------------------------------------------
// Stuck 開始：刺さった姿勢を保存
//-------------------------------------------
void Boss::BeginStuck_() {
	state_ = State::Stuck;
	stateTimer_ = 0.0f;
	stuckAngle_ = currentAngle_;
}

//-------------------------------------------
// Stuck：刺さったまま少し揺れる
//-------------------------------------------
void Boss::UpdateStuck_(float /*dt*/) {
	// 刺さった角度＋小さな揺れ
	float shake = 0.05f * std::sin(stateTimer_ * 30.0f);
	currentAngle_ = stuckAngle_ + shake;

	if (stateTimer_ >= stuckTime_) {
		BeginRecover_();
	}
}

//-------------------------------------------
// Recover 開始：元に戻る
//-------------------------------------------
void Boss::BeginRecover_() {
	state_ = State::Recover;
	stateTimer_ = 0.0f;
	recoverStartAngle_ = currentAngle_;
}

//-------------------------------------------
// Recover：角度を 0 に戻す
//-------------------------------------------
void Boss::UpdateRecover_(float /*dt*/) {
	float t = (recoverTime_ > 0.0f) ? (stateTimer_ / recoverTime_) : 1.0f;
	if (t > 1.0f)
		t = 1.0f;

	float tt = t * t * (3.0f - 2.0f * t);

	currentAngle_ = recoverStartAngle_ * (1.0f - tt);

	if (stateTimer_ >= recoverTime_) {
		state_ = State::Waiting;
		stateTimer_ = 0.0f;
		currentAngle_ = 0.0f;

		// 向きは最後に狙った方向のままにする
		aimStartAxis_ = fallAxis_;
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
