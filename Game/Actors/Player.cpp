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

	// === 進行方向へ回頭（スムージング）===
	float targetYaw = transform_.rotate.y;
	if (move.x != 0.0f || move.z != 0.0f) {
		targetYaw = std::atan2(move.x, move.z); // z前・x右
	}
	const float rotateSmooth = 1.0f - std::exp(-12.0f * 0.016f); // ≒ turnSpeed=12
	transform_.rotate.y = LerpAngle(transform_.rotate.y, targetYaw, rotateSmooth);

	// === 平行移動 ===
	if (move.x != 0.0f || move.z != 0.0f) {
		transform_.translate.x += move.x * speed_;
		transform_.translate.z += move.z * speed_;
		lastMoveDir_ = {move.x, 0.0f, move.z};
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

	// === 重力（元のまま）===
	velocityY_ += gravity_ * 0.98f;
	velocityY_ = (std::max)(velocityY_, -20.0f);
	transform_.translate.y += velocityY_ * 0.016f;

	// === 地面判定（元のまま）===
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
