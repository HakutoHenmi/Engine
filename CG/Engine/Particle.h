#pragma once
#include "Matrix4x4.h"
#include "Renderer.h"

namespace Engine {

struct Particle {
	Vector3 pos;
	Vector3 vel;
	Vector3 scale;
	Vector4 color;
	float life = 1.0f;
	float age = 0.0f;
	bool active = false;
};

class ParticleSystem {
public:
	void Initialize(Renderer& renderer, WindowDX& dx, size_t maxCount = 1000);
	void Initialize(Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd, size_t maxCount = 1000);
	void Update(float dt);
	void Draw(ID3D12GraphicsCommandList* cmd, const Camera& cam);

	void Emit(const Vector3& pos, const Vector3& vel, const Vector3& scale, const Vector4& color, float life);

private:
	Renderer* renderer_ = nullptr;
	int modelHandle_ = -1;
	std::vector<Particle> particles_;
};
} // namespace Engine
