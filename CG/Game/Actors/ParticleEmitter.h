#pragma once
#include "Matrix4x4.h"
#include "Renderer.h"
#include <random>

// Engine 側のパーティクルを利用
#include "Particle.h" // ← Engine::ParticleSystem が宣言されている想定

namespace Game {

/// エミッタの基本パラメータ
struct EmitterParams {
	// 放出
	float emitRate = 0.0f; // 1秒あたり放出数（0で自動放出なし）
	int maxOnce = 16;      // 1フレームあたりの上限（保険）

	// 初期値レンジ
	Engine::Vector3 initVelMin{-1.0f, +2.0f, -1.0f};
	Engine::Vector3 initVelMax{+1.0f, +4.0f, +1.0f};
	Engine::Vector3 initScaleMin{0.08f, 0.08f, 0.08f};
	Engine::Vector3 initScaleMax{0.16f, 0.16f, 0.16f};
	Engine::Vector4 initColor{1.0f, 0.6f, 0.2f, 1.0f};
	float lifeMin = 0.8f;
	float lifeMax = 1.6f;

	// 描画
	bool useAdditive = true; // 加算ブレンドで光らせる

	Engine::Vector3 spawnPosJitter{0.0f, 0.0f, 0.0f};
};

/// シーンに置いて使えるパーティクル・アクター
class ParticleEmitter {
public:
	void Initialize(Engine::Renderer& renderer, Engine::WindowDX& dx, size_t maxParticles = 1000);

	void Initialize(Engine::Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd, size_t maxParticles = 1000);

	// 位置・有効化
	void SetPosition(const Engine::Vector3& p) { position_ = p; }
	const Engine::Vector3& Position() const { return position_; }
	void SetActive(bool a) { active_ = a; }
	bool IsActive() const { return active_; }

	// 発生パラメータ
	EmitterParams& Params() { return params_; }
	const EmitterParams& Params() const { return params_; }

	// まとめて放出（任意位置）
	void BurstAt(const Engine::Vector3& pos, int count) {
		Engine::Vector3 prev = position_;
		position_ = pos;
		Burst(count);
		position_ = prev;
	}

	// 単発放出ショートカット
	void EmitOnce() { Burst(1); }

	// 自動/手動切替
	void EnableAutoEmit(bool enable) { active_ = enable; }
	bool IsAutoEmit() const { return active_; }

	// 外部から任意のタイミングでまとめて発生
	void Burst(int count);

	// 毎フレーム
	void Update(float dt);
	void Draw(ID3D12GraphicsCommandList* cmd, const Engine::Camera& cam);

private:
	Engine::Renderer* renderer_ = nullptr;
	Engine::ParticleSystem sys_; // Engine 既存のパーティクルシステム
	Engine::Vector3 position_{0, 0, 0};
	bool active_ = true;

	// 自動放出用
	float emitCarry_ = 0.0f;

	// 乱数
	std::mt19937 rng_{std::random_device{}()};
	float RandF(float a, float b) {
		std::uniform_real_distribution<float> dist(a, b);
		return dist(rng_);
	}
	Engine::Vector3 RandV3(const Engine::Vector3& mn, const Engine::Vector3& mx) { return {RandF(mn.x, mx.x), RandF(mn.y, mx.y), RandF(mn.z, mx.z)}; }

	EmitterParams params_{};
};

} // namespace Game
