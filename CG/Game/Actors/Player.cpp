#include "Player.h"
#include "Input.h"

#include <Windows.h>
#include <Xinput.h>
#include <cmath>
#include <dinput.h>

#pragma comment(lib, "xinput9_1_0.lib")

namespace Engine {

using namespace DirectX;

void Player::Initialize(Engine::Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd) {
	modelHandle_ = renderer.LoadModel(device, cmd, "Resources/cube/cube.obj");
	OutputDebugStringA(("Player modelHandle=" + std::to_string(modelHandle_) + "\n").c_str());

	transform_.translate = {0, 0.5f, 0};
	transform_.rotate = {0, 0, 0};
	transform_.scale = {0.5, 0.5, 0.5};

	dodgeActive_ = false;
	dodgeTimer_ = 0.0f;
	dodgeCooldownTimer_ = 0.0f;
	prevRightBtn_ = false;
	dodgeDir_ = {0, 0, 0};

	meleeSwing_ = false;
	meleeTime_ = 0.0f;
	meleeCooldownTimer_ = 0.0f;
	prevLeftBtn_ = false;

	// 見た目スケール（モデルサイズ）※お好みで
	swordLocal_.scale = {0.30f, 0.80f, 0.30f};

	// モデルが +Y に伸びる → +Z 基準へ補正
	swordLocal_.rotate.x = -DirectX::XM_PIDIV2;
	swordLocal_.rotate.y = DirectX::XM_PI; // 前後が逆なら 0 / 180° を切り替え
	swordLocal_.rotate.z = 0.0f;

	// ★ 位置オフセットは「前方距離」と「高さ」だけを使う
	swordForwardDist_ = 0.62f; // 前（+）へ 0.62m
	swordUpOffset_ = 0.90f;    // 上へ 0.90m

	// ===== パーティクル（攻撃用）初期化 =====
	// 振り始め：淡い風切り
	swingEmitter_.Initialize(renderer, device, cmd, 256);
	{
		auto& ep = swingEmitter_.Params();
		ep.emitRate = 0.0f;
		ep.maxOnce = 64;
		ep.useAdditive = true;
		ep.initColor = {0.75f, 0.95f, 1.0f, 1.0f};
		ep.initScaleMin = {0.14f, 0.04f, 0.14f};
		ep.initScaleMax = {0.22f, 0.06f, 0.22f};
		ep.initVelMin = {-0.25f, 0.10f, -0.25f};
		ep.initVelMax = {+0.25f, 0.30f, +0.25f};
		ep.lifeMin = 0.10f;
		ep.lifeMax = 0.16f;
	}
	// 軌跡：剣先に追従して細かく
	trailEmitter_.Initialize(renderer, device, cmd, 6000);
	{
		auto& ep = trailEmitter_.Params();
		ep.emitRate = 0.0f; // 攻撃中だけ手動で出す
		ep.maxOnce = 128;
		ep.useAdditive = true;
		ep.initColor = {1.0f, 0.85f, 0.35f, 1.0f};
		ep.initScaleMin = {0.05f, 0.05f, 0.05f};
		ep.initScaleMax = {0.10f, 0.10f, 0.10f};
		ep.initVelMin = {-0.30f, 0.00f, -0.30f};
		ep.initVelMax = {+0.30f, 0.20f, +0.30f};
		ep.lifeMin = 0.10f;
		ep.lifeMax = 0.18f;
	}
	// 命中：明るい火花
	hitEmitter_.Initialize(renderer, device, cmd, 512);
	{
		auto& ep = hitEmitter_.Params();
		ep.emitRate = 0.0f;
		ep.useAdditive = true;
		ep.initColor = {1.0f, 0.6f, 0.2f, 1.0f};
		ep.initScaleMin = {0.05f, 0.05f, 0.05f};
		ep.initScaleMax = {0.10f, 0.10f, 0.10f};
		ep.initVelMin = {-2.0f, 0.6f, -2.0f};
		ep.initVelMax = {+2.0f, 1.6f, +2.0f};
		ep.lifeMin = 0.10f;
		ep.lifeMax = 0.25f;
	}
}

// XZ 成分だけにして正規化
static inline DirectX::XMFLOAT3 FlattenXZ(const DirectX::XMFLOAT3& v) {
	using namespace DirectX;
	XMFLOAT3 tmp{v.x, 0.0f, v.z}; // ← 一時をローカルに退避
	XMVECTOR vec = XMLoadFloat3(&tmp);
	vec = XMVector3Normalize(vec);
	XMFLOAT3 out;
	XMStoreFloat3(&out, vec);
	return out;
}

// View 行列からカメラの「前/右」をワールド空間で取得（LookAtでもOK）
struct CamBasisXZ {
	DirectX::XMFLOAT3 f; // forward
	DirectX::XMFLOAT3 r; // right
};
static inline CamBasisXZ MakeCamBasisXZ_FromView(const DirectX::XMMATRIX& view) {
	using namespace DirectX;
	// view の逆行列 = カメラのワールド変換。r[0]=right, r[1]=up, r[2]=forward
	XMMATRIX inv = XMMatrixInverse(nullptr, view);
	XMFLOAT3 f{inv.r[2].m128_f32[0], inv.r[2].m128_f32[1], inv.r[2].m128_f32[2]};
	XMFLOAT3 r{inv.r[0].m128_f32[0], inv.r[0].m128_f32[1], inv.r[0].m128_f32[2]};
	CamBasisXZ b;
	b.f = FlattenXZ(f);
	b.r = FlattenXZ(r);
	return b;
}

// [-1,1] の値にデッドゾーン適用
static inline float ApplyDeadzoneSigned(float v, float dz /*0..1*/) {
	float av = std::fabs(v);
	if (av < dz)
		return 0.0f;
	float t = (av - dz) / (1.0f - dz);
	return (v < 0.0f) ? -t : t;
}

// ラップ付き補間 (0～2πの角度補間)
static inline float LerpAngle(float from, float to, float t) {
	float diff = to - from;
	while (diff > XM_PI)
		diff -= XM_2PI;
	while (diff < -XM_PI)
		diff += XM_2PI;
	return from + diff * t;
}

Transform Player::MakeSwordWorld_(float yawRad, float addYawDeg, float tiltZDeg, float pitchXDeg) const {
	Transform t = swordLocal_; // ここに X=-90° 等の基礎回転と scale が入っている

	// 見た目の向き（Yaw にスイング角を足す）
	t.rotate.y += DirectX::XMConvertToRadians(addYawDeg);
	t.rotate.y += yawRad;

	// ★ 斜め感：刃の傾き（Z回転）と、切り下ろしの俯仰（X回転）を加える
	t.rotate.z += DirectX::XMConvertToRadians(tiltZDeg);  // 右下：負、左下：正
	t.rotate.x += DirectX::XMConvertToRadians(pitchXDeg); // 下げたいときは負方向

	// 位置は“常に前側”のみ
	float s = std::sin(yawRad), c = std::cos(yawRad);
	t.translate.x = transform_.translate.x + s * swordForwardDist_;
	t.translate.y = transform_.translate.y + swordUpOffset_;
	t.translate.z = transform_.translate.z + c * swordForwardDist_;
	return t;
}

void Player::Update(const Camera& cam, const Input& input) {
	cam_ = &cam;

	// === カメラ基底（前/右）を XZ に投影 ===
	CamBasisXZ cb = MakeCamBasisXZ_FromView(cam.View());

	// === 入力収集（-1..1）===
	float inX = 0.0f; // 右(+)/左(-)
	float inY = 0.0f; // 前(+)/後(-)
	bool hasStickInput = false;

	// --- コントローラ ---
	{
		XINPUT_STATE state{};
		if (XInputGetState(0, &state) == ERROR_SUCCESS) {
			const float inv = 1.0f / 32767.0f;
			float lx = state.Gamepad.sThumbLX * inv;
			float ly = state.Gamepad.sThumbLY * inv;
			constexpr float DEADZONE = 7849.0f / 32767.0f; // ≒0.24
			lx = ApplyDeadzoneSigned(lx, DEADZONE);
			ly = ApplyDeadzoneSigned(ly, DEADZONE);
			if (lx != 0.0f || ly != 0.0f) {
				hasStickInput = true;
				inX = lx;
				inY = ly; // ↑が前進
			}
		}
	}

	// --- キーボード（スティック無入力時のみ）---
	if (!hasStickInput) {
		if (input.Down(DIK_D))
			inX += 1.0f;
		if (input.Down(DIK_A))
			inX -= 1.0f;
		if (input.Down(DIK_W))
			inY += 1.0f;
		if (input.Down(DIK_S))
			inY -= 1.0f;
	}

	// === カメラ相対の移動方向を作成 ===
	DirectX::XMFLOAT3 move{0, 0, 0};
	{
		using namespace DirectX;
		XMVECTOR f = XMLoadFloat3(&cb.f);
		XMVECTOR r = XMLoadFloat3(&cb.r);
		XMVECTOR dir = XMVectorAdd(XMVectorScale(f, inY), XMVectorScale(r, inX));

		XMFLOAT3 d;
		XMStoreFloat3(&d, dir);
		float len2 = d.x * d.x + d.z * d.z;
		if (len2 > 0.0001f) {
			XMVECTOR n = XMVector3Normalize(dir);
			XMStoreFloat3(&move, n);
		} else {
			move = {0, 0, 0};
		}
	}

	constexpr float dt = 0.016f; // 固定フレーム想定

	// ==== 右クリック（緊急回避）====
	bool rightNow = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	bool rightTrig = rightNow && !prevRightBtn_;
	prevRightBtn_ = rightNow;

	if (dodgeCooldownTimer_ > 0.0f) {
		dodgeCooldownTimer_ -= dt;
		if (dodgeCooldownTimer_ < 0.0f)
			dodgeCooldownTimer_ = 0.0f;
	}

	if (rightTrig && !dodgeActive_ && dodgeCooldownTimer_ <= 0.0f) {
		DirectX::XMFLOAT3 dir = move;
		if (dir.x == 0.0f && dir.z == 0.0f) {
			float s = std::sin(transform_.rotate.y);
			float c = std::cos(transform_.rotate.y);
			dir = {s, 0.0f, c};
		}
		float len = std::sqrt(dir.x * dir.x + dir.z * dir.z);
		if (len > 1e-6f) {
			dir.x /= len;
			dir.z /= len;
		} else {
			dir = {0.0f, 0.0f, 1.0f};
		}

		dodgeDir_ = {dir.x, 0.0f, dir.z};
		dodgeActive_ = true;
		dodgeTimer_ = dodgeDuration_;
		dodgeCooldownTimer_ = dodgeCooldown_;
	}

	// ==== 進行方向へ回頭 ====
	{
		float targetYaw = transform_.rotate.y;
		if (dodgeActive_)
			targetYaw = std::atan2(dodgeDir_.x, dodgeDir_.z);
		else if (move.x != 0.0f || move.z != 0.0f)
			targetYaw = std::atan2(move.x, move.z);

		const float rotateSmooth = 1.0f - std::exp(-12.0f * dt);
		transform_.rotate.y = LerpAngle(transform_.rotate.y, targetYaw, rotateSmooth);
	}

	// ==== 平行移動 ====
	if (dodgeActive_) {
		transform_.translate.x += dodgeDir_.x * dodgeSpeed_ * dt;
		transform_.translate.z += dodgeDir_.z * dodgeSpeed_ * dt;

		dodgeTimer_ -= dt;
		if (dodgeTimer_ <= 0.0f)
			dodgeActive_ = false;
	} else {
		if (move.x != 0.0f || move.z != 0.0f) {
			transform_.translate.x += move.x * speed_;
			transform_.translate.z += move.z * speed_;
			lastMoveDir_ = {move.x, 0.0f, move.z};
		}
	}

	// ==== 近接（左クリックコンボ）====
	// 入力
	bool lNow = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	bool lTrig = lNow && !prevLeftBtn_;
	prevLeftBtn_ = lNow;

	// クールタイム
	if (meleeCooldownTimer_ > 0.0f) {
		meleeCooldownTimer_ -= dt;
		if (meleeCooldownTimer_ < 0.0f)
			meleeCooldownTimer_ = 0.0f;
	}

	// コンボ受付
	if (comboTimer_ > 0.0f) {
		comboTimer_ -= dt;
		if (comboTimer_ <= 0.0f) {
			comboTimer_ = 0.0f;
			comboStep_ = 0;
		}
	}

	// ===== 攻撃開始（開始角を“前回の終了角”から繋ぐ）=====
	if (lTrig && !meleeSwing_) {
		meleeSwing_ = true;
		++comboStep_;
		if (comboStep_ > 3)
			comboStep_ = 1; // 1→2→3→1
		comboTimer_ = comboInterval_;
		meleeTime_ = 0.0f;

		// 開始角：前回の終了角（初回だけはアイドル）
		float baseStartYaw = hasPrevSwing_ ? prevEndYaw_ : swordIdleYaw_;
		float baseStartTiltZ = hasPrevSwing_ ? prevEndTiltZ_ : swordIdleTiltZ_;
		float baseStartPitchX = hasPrevSwing_ ? prevEndPitchX_ : swordIdlePitchX_;

		// 終了角：段ごとの目標（バッテン→横薙ぎ）
		float yawEnd = 0, tiltEnd = 0, pitchEnd = 0;
		switch (comboStep_) {
		case 1: // ＼：右上→左下（終端は左下寄り）
			yawEnd = -65.0f;
			tiltEnd = -35.0f;  // 右傾き
			pitchEnd = -25.0f; // 下へ
			break;
		case 2: // ／：左上→右下（終端は右下寄り）
			yawEnd = +65.0f;
			tiltEnd = +35.0f;  // 左傾き
			pitchEnd = -25.0f; // 下へ
			break;
		case 3:               // 横薙ぎ
			yawEnd = +150.0f; // 大きく右へ抜ける
			tiltEnd = 0.0f;
			pitchEnd = 0.0f;
			break;
		}

		// ★ 今回スイングの開始/終了を確定（開始=前回終端／終了=段ごとの目標）
		swingYawStart_ = baseStartYaw;
		swingYawEnd_ = yawEnd;
		swingTiltStart_ = baseStartTiltZ;
		swingTiltEnd_ = tiltEnd;
		swingPitchStart_ = baseStartPitchX;
		swingPitchEnd_ = pitchEnd;

		// ---- 風切りパーティクルを一発バースト ----
		{
			// プレイヤー前方やや上から出す
			Vector3 start = transform_.translate + GetForwardDir() * 0.6f + Vector3{0, 0.2f, 0};
			swingEmitter_.SetPosition(start);
			swingEmitter_.Burst(48);
		}
	}

	float yaw = transform_.rotate.y;
	meleeHitActive_ = false;

	if (meleeSwing_) {
		// 1段の再生（少しゆっくり：meleeDuration_ は 0.48f 推奨）
		meleeTime_ += dt;
		float t = meleeTime_ / meleeDuration_;
		if (t >= 1.0f) {
			t = 1.0f;
			meleeSwing_ = false;
		}

		// イージング：両端で速度0に近づける（smoothstep）
		float e = t * t * (3.0f - 2.0f * t);

		// 連続補間（開始→終了）
		float addDeg = swingYawStart_ + (swingYawEnd_ - swingYawStart_) * e;
		float tiltZ = swingTiltStart_ + (swingTiltEnd_ - swingTiltStart_) * e;
		float pitchX = swingPitchStart_ + (swingPitchEnd_ - swingPitchStart_) * e;

		// 当たり判定（交差の中心付近を広めに）
		float tMin = 0.20f, tMax = 0.75f;
		if (t > tMin && t < tMax) {
			const float bladeLen = 1.45f;
			Transform tmp = MakeSwordWorld_(yaw, addDeg, tiltZ, pitchX);
			Vector3 h = tmp.translate;
			float sy = tmp.rotate.y;
			float ss = std::sin(sy), sc = std::cos(sy);
			Vector3 tip{h.x + sc * bladeLen, h.y, h.z + ss * bladeLen};
			meleeCapsuleP0_ = h;
			meleeCapsuleP1_ = tip;
			meleeHitActive_ = true;
		}

		// 表示
		swordTf_ = MakeSwordWorld_(yaw, addDeg, tiltZ, pitchX);

		// ---- 軌跡パーティクル（剣先に追従）----
		{
			// 刃の先端近似（X/Z は yaw を使って前に bladeLen だけ伸ばす）
			const float bladeLen = 1.45f;
			const float sy = std::sin(swordTf_.rotate.y);
			const float sc = std::cos(swordTf_.rotate.y);
			Vector3 tip{swordTf_.translate.x + sc * bladeLen, +swordTf_.translate.y, +swordTf_.translate.z + sy * bladeLen};
			trailEmitter_.SetPosition(tip);
			// 振りのコア区間を少し濃く
			const bool inCore = (t >= 0.05f && t <= 0.25f);
			const int spawn = inCore ? 28 : 12;
			trailEmitter_.Burst(spawn);
		}

		// ===== スイング終了時：今回の“終了角”を保存（次回の開始に使う）=====
		if (!meleeSwing_) {
			prevEndYaw_ = swingYawEnd_;
			prevEndTiltZ_ = swingTiltEnd_;
			prevEndPitchX_ = swingPitchEnd_;
			hasPrevSwing_ = true;
		}

	} else {
		// ===== 待機時：前回終端から“なめらか”にアイドルへ戻す（スナップ禁止）=====
		// exp平滑化で戻す（1フレで戻らず、数フレ掛けて戻る）
		const float relaxRate = 6.0f; // 大きいほど早く戻る
		float a = 1.0f - std::exp(-relaxRate * dt);

		// 目標：アイドル角
		prevEndYaw_ = prevEndYaw_ + (swordIdleYaw_ - prevEndYaw_) * a;
		prevEndTiltZ_ = prevEndTiltZ_ + (swordIdleTiltZ_ - prevEndTiltZ_) * a;
		prevEndPitchX_ = prevEndPitchX_ + (swordIdlePitchX_ - prevEndPitchX_) * a;

		// その時点の角で見た目更新（＝センターへスナップしない）
		swordTf_ = MakeSwordWorld_(yaw, prevEndYaw_, prevEndTiltZ_, prevEndPitchX_);

		if (comboTimer_ <= 0.0f)
			comboStep_ = 0; // 受付切れで段数は戻す
	}

	// ---- パーティクルの更新 ----
	swingEmitter_.Update(dt);
	trailEmitter_.Update(dt);
	hitEmitter_.Update(dt);

	// ==== ジャンプ ====
	if (input.Trigger(DIK_SPACE)) {
		if (onGround_) {
			velocityY_ = jumpPower_;
			onGround_ = false;
			canDoubleJump_ = true;
		} else if (canDoubleJump_) {
			velocityY_ = jumpPower_ * 0.9f;
			canDoubleJump_ = false;
		}
	}

	// ==== 重力 ====
	velocityY_ += gravity_ * 0.98f;
	velocityY_ = (std::max)(velocityY_, -20.0f);
	transform_.translate.y += velocityY_ * 0.016f;

	// ==== 地面判定 ====
	if (transform_.translate.y <= 0.5f) {
		transform_.translate.y = 0.5f;
		velocityY_ = 0.5f;
		onGround_ = true;
	}
}

void Player::TriggerHitEffect(const Vector3& hitPos, const Vector3& hitNormal) {
	// 命中点から少し前に出してバースト
	Vector3 p = hitPos + hitNormal * 0.02f;
	hitEmitter_.SetPosition(p);
	hitEmitter_.Burst(24);
}

Vector3 Player::GetForwardDir() const {
	float c = std::cos(transform_.rotate.y);
	float s = std::sin(transform_.rotate.y);
	return {s, 0.0f, c};
}

void Player::Draw(Engine::Renderer& renderer, const Camera& cam, ID3D12GraphicsCommandList* cmd) {
	// 本体
	if (modelHandle_ >= 0) {
		renderer.UpdateModelCBWithColor(modelHandle_, cam, transform_, Engine::Vector4{1, 1, 1, 1});
		renderer.DrawModel(modelHandle_, cmd);
	}

	// ★常に描画（ハンドルが有効なら）
	if (swordHandle_ >= 0) {
		renderer.UpdateModelCBWithColor(swordHandle_, cam, swordTf_, Engine::Vector4{1, 1, 1, 1});
		renderer.DrawModel(swordHandle_, cmd);
	}

	// ---- パーティクル描画 ----
	swingEmitter_.Draw(cmd, cam);
	trailEmitter_.Draw(cmd, cam);
	hitEmitter_.Draw(cmd, cam);
}

} // namespace Engine
