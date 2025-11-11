#include "Particle.h"
#include "Model.h"
#include "Renderer.h"
#include <algorithm>

namespace Engine {

void ParticleSystem::Initialize(Renderer& renderer, WindowDX& dx, size_t maxCount) {
	renderer_ = &renderer;
	particles_.resize(maxCount);

	// 板ポリモデルをロード（例: Resources/plane.obj）
	auto* dev = dx.Dev();
	auto* cmd = dx.List();
	modelHandle_ = renderer.LoadModel(dev, cmd, "Resources/plane.obj");
}

void ParticleSystem::Initialize(Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd, size_t maxCount) {
	renderer_ = &renderer;
	particles_.resize(maxCount);
	// WindowDX を使わずにモデルをロード
	modelHandle_ = renderer.LoadModel(device, cmd, "Resources/plane.obj");
}

void ParticleSystem::Emit(const Vector3& pos, const Vector3& vel, const Vector3& scale, const Vector4& color, float life) {
	for (auto& p : particles_) {
		if (!p.active) {
			p.active = true;
			p.pos = pos;
			p.vel = vel;
			p.scale = scale;
			p.color = color;
			p.life = life;
			p.age = 0.0f;
			break;
		}
	}
}

void ParticleSystem::Update(float dt) {
	for (auto& p : particles_) {
		if (!p.active)
			continue;
		p.age += dt;
		if (p.age >= p.life) {
			p.active = false;
			continue;
		}
		p.pos += p.vel * dt;
		p.color.w = 1.0f - (p.age / p.life); // フェードアウト
	}
}

void ParticleSystem::Draw(ID3D12GraphicsCommandList* cmd, const Camera& cam) {
	if (!renderer_)
		return;

	size_t slot = 0;
	const size_t maxSlots = renderer_->MaxModelCBSlots();

	// ★カメラ位置の取得（Camera に Position() がある想定）
	const auto cp = cam.Position(); // XMFLOAT3 を受ける
	const Vector3 camPos{cp.x, cp.y, cp.z};

	for (auto& p : particles_) {
		if (!p.active)
			continue;
		if (slot >= maxSlots)
			break;

		// カメラ方向（パーティクル→カメラ）
		Vector3 d = camPos - p.pos;
		const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
		if (len > 1e-6f) {
			d.x /= len;
			d.y /= len;
			d.z /= len;
		} else {
			d = {0, 0, 1}; // 退避
		}

		// ヨー（Y軸回り）とピッチ（X軸回り）を算出
		// 前方(+Z) を d に合わせる想定
		const float yaw = std::atan2(d.x, d.z);
		const float pitch = std::atan2(-d.y, std::sqrt(d.x * d.x + d.z * d.z));
		const float roll = 0.0f;

		Transform tf;
		tf.translate = p.pos;
		tf.scale = p.scale;
		tf.rotate = {pitch, yaw, roll}; // ← これで常にカメラ正面向き

		renderer_->UpdateModelCBWithColorAt(modelHandle_, slot, cam, tf, p.color);
		renderer_->DrawModelAt(modelHandle_, cmd, slot);
		++slot;
	}
}

} // namespace Engine
