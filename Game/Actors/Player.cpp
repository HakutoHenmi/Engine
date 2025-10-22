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

	Vector3 move{0, 0, 0};
	bool hasStickInput = false;

	// --- コントローラー入力 ---
	float targetYaw = transform_.rotate.y;
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
				targetYaw = std::atan2f(lx, ly); // スティック方向を目標角度に設定
				move = {std::sin(targetYaw), 0.0f, std::cos(targetYaw)};
			}
		}
	}

	// --- キーボード操作（スティックが無入力時のみ有効） ---
	if (!hasStickInput) {
		if (input.Down(DIK_W))
			move.z += 1.0f;
		if (input.Down(DIK_S))
			move.z -= 1.0f;
		if (input.Down(DIK_A))
			move.x -= 1.0f;
		if (input.Down(DIK_D))
			move.x += 1.0f;

		if (move.x != 0.0f || move.z != 0.0f) {
			targetYaw = std::atan2f(move.x, move.z);
		}
	}

	// --- スムージング回転 ---
	const float rotateSmooth_ = 0.15f; // ←この値を下げるとよりゆっくり
	transform_.rotate.y = LerpAngle(transform_.rotate.y, targetYaw, rotateSmooth_);

	// --- 移動処理 ---
	if (move.x != 0.0f || move.z != 0.0f) {
		float len = std::sqrtf(move.x * move.x + move.z * move.z);
		move.x /= len;
		move.z /= len;
		transform_.translate.x += move.x * speed_;
		transform_.translate.z += move.z * speed_;
		lastMoveDir_ = {move.x, 0.0f, move.z};
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
