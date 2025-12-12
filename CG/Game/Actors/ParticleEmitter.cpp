#include "ParticleEmitter.h"

namespace Game {

void ParticleEmitter::Initialize(Engine::Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd, size_t maxParticles) {
	renderer_ = &renderer;
	sys_.Initialize(renderer, device, cmd, maxParticles); // ← 新しく足したPSのオーバーロードを呼ぶ
}

void ParticleEmitter::Initialize(Engine::Renderer& renderer, Engine::WindowDX& dx, size_t maxParticles) {
	renderer_ = &renderer;
	sys_.Initialize(renderer, dx, maxParticles);
}

void ParticleEmitter::Burst(int count) {
	if (!renderer_ || count <= 0)
		return;

	count = (std::min)(count, params_.maxOnce * 4); // 安全上限

	for (int i = 0; i < count; ++i) {
		Engine::Vector3 v = RandV3(params_.initVelMin, params_.initVelMax);
		Engine::Vector3 scl = RandV3(params_.initScaleMin, params_.initScaleMax);
		float life = RandF(params_.lifeMin, params_.lifeMax);

		Engine::Vector3 p = position_;
		if (params_.spawnPosJitter.x != 0.0f || params_.spawnPosJitter.y != 0.0f || params_.spawnPosJitter.z != 0.0f) {
			p.x += RandF(-params_.spawnPosJitter.x, +params_.spawnPosJitter.x);
			p.y += RandF(-params_.spawnPosJitter.y, +params_.spawnPosJitter.y);
			p.z += RandF(-params_.spawnPosJitter.z, +params_.spawnPosJitter.z);
		}

		// ★風が有効なら、速度に風の成分を加算して吹き飛ばす
		if (windEnabled_) {
			v.x += windDir_.x * windStrength_;
			v.y += windDir_.y * windStrength_;
			v.z += windDir_.z * windStrength_;
		}

		sys_.Emit(p, v, scl, params_.initColor, life);
	}
}

void ParticleEmitter::SetWind(const Engine::Vector3& dir, float strength) {
	windDir_ = dir;
	windStrength_ = strength;
}

void ParticleEmitter::Update(float dt) {
	if (!renderer_)
		return;

	if (active_ && params_.emitRate > 0.0f) {
		// レートに基づく Poisson 的発生（積算 → 整数分放出）
		emitCarry_ += params_.emitRate * dt;
		int toEmit = (std::min)((int)emitCarry_, params_.maxOnce);
		if (toEmit > 0) {
			emitCarry_ -= (float)toEmit;
			Burst(toEmit);
		}
	}

	sys_.Update(dt);
}

void ParticleEmitter::Draw(ID3D12GraphicsCommandList* cmd, const Engine::Camera& cam) {
	if (!renderer_)
		return;

	// ブレンド切替（加算/不透明）
	const auto prev = renderer_->GetBlendMode();
	if (params_.useAdditive) {
		renderer_->SetBlendMode(Engine::Renderer::BlendMode::Add);
	} else {
		renderer_->SetBlendMode(Engine::Renderer::BlendMode::Alpha);
	}

	sys_.Draw(cmd, cam);

	// 元に戻す
	renderer_->SetBlendMode(prev);
}

} // namespace Game
