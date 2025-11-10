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

	// 握り位置（プレイヤー原点からのローカルオフセット）
	swordLocal_.translate = {+0.35f, 0.85f, +0.15f}; // 右手・少し前
	swordLocal_.rotate = {0.0f, 0.0f, 0.0f};
	swordLocal_.scale = {1.0f, 1.0f, 1.0f};
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
		// dir = f * inY + r * inX
		XMVECTOR f = XMLoadFloat3(&cb.f);
		XMVECTOR r = XMLoadFloat3(&cb.r);
		XMVECTOR dir = XMVectorAdd(XMVectorScale(f, inY), XMVectorScale(r, inX));

		// 入力なしなら**座標を一切動かさない**（カメラだけ回しても動かない）
		XMFLOAT3 d;
		XMStoreFloat3(&d, dir);
		float len2 = d.x * d.x + d.z * d.z;
		if (len2 > 0.0001f) {
			// 正規化
			XMVECTOR n = XMVector3Normalize(dir);
			XMStoreFloat3(&move, n);
		} else {
			move = {0, 0, 0};
		}
	}

	// 固定フレーム想定（あなたのコードに合わせる）
	constexpr float dt = 0.016f;

	// ==== 右クリック（緊急回避）入力 ====
	bool rightNow = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	bool rightTrig = rightNow && !prevRightBtn_;
	prevRightBtn_ = rightNow;

	// クールタイム消化
	if (dodgeCooldownTimer_ > 0.0f) {
		dodgeCooldownTimer_ -= dt;
		if (dodgeCooldownTimer_ < 0.0f)
			dodgeCooldownTimer_ = 0.0f;
	}

	// 発動条件：押下エッジ & 非発動中 & CT終了
	if (rightTrig && !dodgeActive_ && dodgeCooldownTimer_ <= 0.0f) {
		// 入力方向が無いときは現在の向きに回避
		DirectX::XMFLOAT3 dir = move;
		if (dir.x == 0.0f && dir.z == 0.0f) {
			// yaw から前方向
			float s = std::sin(transform_.rotate.y);
			float c = std::cos(transform_.rotate.y);
			dir = {s, 0.0f, c};
		}
		// 正規化（安全）
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

	// ==== 進行方向へ回頭（回避中は回避方向へ向ける）====
	{
		float targetYaw = transform_.rotate.y;
		if (dodgeActive_) {
			targetYaw = std::atan2(dodgeDir_.x, dodgeDir_.z);
		} else if (move.x != 0.0f || move.z != 0.0f) {
			targetYaw = std::atan2(move.x, move.z);
		}
		const float rotateSmooth = 1.0f - std::exp(-12.0f * dt);
		transform_.rotate.y = LerpAngle(transform_.rotate.y, targetYaw, rotateSmooth);
	}

	// ==== 平行移動 ====
	// 回避中は通常移動を無効化し、回避移動のみ行う
	if (dodgeActive_) {
		transform_.translate.x += dodgeDir_.x * dodgeSpeed_ * dt;
		transform_.translate.z += dodgeDir_.z * dodgeSpeed_ * dt;

		dodgeTimer_ -= dt;
		if (dodgeTimer_ <= 0.0f) {
			dodgeActive_ = false;
		}
	} else {
		if (move.x != 0.0f || move.z != 0.0f) {
			transform_.translate.x += move.x * speed_;
			transform_.translate.z += move.z * speed_;
			lastMoveDir_ = {move.x, 0.0f, move.z};
		}
	}

	// ==== 近接（左クリックで横薙ぎ）====

	bool lNow = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	bool lTrig = lNow && !prevLeftBtn_;
	prevLeftBtn_ = lNow;

	// クールタイム
	if (meleeCooldownTimer_ > 0.0f) {
		meleeCooldownTimer_ -= dt;
		if (meleeCooldownTimer_ < 0.0f)
			meleeCooldownTimer_ = 0.0f;
	}

	// 発動（回避中は無効にしたいなら !dodgeActive_ も条件に）
	if (lTrig && !meleeSwing_ && meleeCooldownTimer_ <= 0.0f) {
		meleeSwing_ = true;
		meleeTime_ = 0.0f;
		meleeCooldownTimer_ = meleeCooldown_;
	}

	// 進行方向へ回頭は既存ロジックに任せる（ここでは角度だけ使う）
	float yaw = transform_.rotate.y;

	// 剣の基準姿勢（プレイヤーに追従）
	auto MakeSwordWorld = [&](float yawRad, float addYawDeg) -> Transform {
		Transform t = swordLocal_;
		// yaw でプレイヤーに追従 + 追加のスイング角
		t.rotate.y += yawRad + DirectX::XMConvertToRadians(addYawDeg);
		// ワールドへ：原始的な“プレイヤー原点 + 回転したローカル”でOK
		// ここでは簡略に「平面回転のみ」(XZ回り)
		float s = std::sin(yawRad);
		float c = std::cos(yawRad);
		Vector3 off = swordLocal_.translate;
		Vector3 rotOff{off.x * c - off.z * s, off.y, off.x * s + off.z * c};
		t.translate.x = transform_.translate.x + rotOff.x;
		t.translate.y = transform_.translate.y + rotOff.y;
		t.translate.z = transform_.translate.z + rotOff.z;
		return t;
	};

	meleeHitActive_ = false; // デフォルトは無効

	if (meleeSwing_) {
		meleeTime_ += dt;
		float t = meleeTime_ / meleeDuration_;
		if (t >= 1.0f) {
			t = 1.0f;
			meleeSwing_ = false;
		}

		// ちょい加速 → 減速のイージング
		float e = t * t * (3.0f - 2.0f * t);

		// 角度補間（-70° → +70°）
		float addDeg = meleeAngStartDeg_ + (meleeAngEndDeg_ - meleeAngStartDeg_) * e;

		// 当たりが出るフレーム（中央～後半あたり）
		if (t > 0.25f && t < 0.70f) {
			// 刃の軌跡をカプセルで近似：柄(根本)と先端の2点
			// 剣の長さ・厚みの仮定
			const float bladeLen = 1.2f; // m
			const float gripToTip = bladeLen;

			Transform tmp = MakeSwordWorld(yaw, addDeg);

			// 柄（剣の付け根）= swordTf_ 位置付近
			Vector3 h = tmp.translate;

			// 先端 = 剣ローカルの +Z 方向に bladeLen（見た目を合わせて調整）
			float sy = tmp.rotate.y;
			float ss = std::sin(sy), sc = std::cos(sy);
			Vector3 tip{h.x + sc * gripToTip, h.y, h.z + ss * gripToTip};

			meleeCapsuleP0_ = h;
			meleeCapsuleP1_ = tip;
			meleeHitActive_ = true;
		}

		// 表示用の最終姿勢
		swordTf_ = MakeSwordWorld(yaw, addDeg);

	} else {
		// 待機時の剣（腰の横に下げる/少し前に構える などお好みで）
		swordTf_ = MakeSwordWorld(yaw, -20.0f);
	}

	// === ジャンプ（元のまま）===
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

	// === 重力 ===
	velocityY_ += gravity_ * 0.98f;
	velocityY_ = (std::max)(velocityY_, -20.0f);
	transform_.translate.y += velocityY_ * 0.016f;

	// === 地面判定 ===
	if (transform_.translate.y <= 0.5f) {
		transform_.translate.y = 0.5f;
		velocityY_ = 0.5f;
		onGround_ = true;
	}
}

Vector3 Player::GetForwardDir() const {
	float c = std::cos(transform_.rotate.y);
	float s = std::sin(transform_.rotate.y);
	return {s, 0.0f, c};
}

void Player::Draw(Engine::Renderer& renderer, const Camera& cam, ID3D12GraphicsCommandList* cmd) {
	if (modelHandle_ >= 0) {
		renderer.UpdateModelCBWithColor(modelHandle_, cam, transform_, {1, 1, 1, 1});
		renderer.DrawModel(modelHandle_, cmd);
	}
}

} // namespace Engine
