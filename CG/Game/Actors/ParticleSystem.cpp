#include "ParticleSystem.h"
#include <DirectXMath.h>
#include <cstdlib>

namespace Engine {

void ParticleSystem::Initialize(Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const std::string& sparkPath, const std::string& ringPath, UINT baseSlot) {
	sparkModelHandle_ = renderer.LoadModel(device, cmd, sparkPath);
	ringModelHandle_ = renderer.LoadModel(device, cmd, ringPath);
	baseSlot_ = baseSlot;
}

//--------------------------------
// 火花生成
//--------------------------------
void ParticleSystem::Spawn(const Vector3& position, const Vector3& normal, int count, const Vector4& color, float spreadFactor) {
	for (int i = 0; i < count; ++i) {
		Particle p{};
		p.pos = position;
		p.type = 0;
		p.color = color;

		// ==========================
		// 散布方向を決定
		// ==========================
		float theta = ((float)rand() / RAND_MAX) * DirectX::XM_2PI;
		float phi = ((float)rand() / RAND_MAX) * (DirectX::XM_PI * spreadFactor * spreadAngleFactor_); // ★ 拡散角調整

		Vector3 dirRand{std::cos(theta) * std::sin(phi), std::cos(phi), std::sin(theta) * std::sin(phi)};

		// ★ 基準ベクトル
		Vector3 baseDir = normal;
		if (scatterMode_ == ScatterMode::Directional) {
			baseDir = mainDir_;
		}

		Vector3 up = {0, 1, 0};
		Vector3 axis = Cross(up, Normalize(baseDir));
		float dotVal = Dot(up, Normalize(baseDir));
		float angle = std::acos(std::clamp(dotVal, -1.0f, 1.0f));

		Matrix4x4 rotMat = MakeRotateAxisAngle(Normalize(axis), angle);
		dirRand = TransformNormal(dirRand, rotMat);

		// ==========================
		// 速度・寿命設定
		// ==========================
		float speed = 2.0f + ((float)rand() / RAND_MAX) * 2.5f;
		p.vel = Normalize(dirRand) * speed;
		p.life = 0.2f + ((float)rand() / RAND_MAX) * 0.2f;
		p.size = 0.1f + ((float)rand() / RAND_MAX) * 0.05f;

		particles_.push_back(p);
	}

	// 上限処理
	if (particles_.size() > 2000) {
		particles_.erase(particles_.begin(), particles_.begin() + (particles_.size() - 2000));
	}
}

void ParticleSystem::EnemySpawn(const Vector3& position, const Vector3& normal, int count, const Vector4& color, float spreadFactor) {

	for (int i = 0; i < count; ++i) {
		Particle p{};
		p.pos = position;
		p.type = 0;
		p.color = color;

		// ==========================
		// 散布方向を決定
		// ==========================
		float theta = ((float)rand() / RAND_MAX) * DirectX::XM_2PI;
		float phi = ((float)rand() / RAND_MAX) * (DirectX::XM_PI * spreadFactor * spreadAngleFactor_); // ★ 拡散角調整

		Vector3 dirRand{std::cos(theta) * std::sin(phi), std::cos(phi), std::sin(theta) * std::sin(phi)};

		// ★ 基準ベクトル
		Vector3 baseDir = normal;
		if (scatterMode_ == ScatterMode::Directional) {
			baseDir = mainDir_;
		}

		Vector3 up = {0, 1, 0};
		Vector3 axis = Cross(up, Normalize(baseDir));
		float dotVal = Dot(up, Normalize(baseDir));
		float angle = std::acos(std::clamp(dotVal, -1.0f, 1.0f));

		Matrix4x4 rotMat = MakeRotateAxisAngle(Normalize(axis), angle);
		dirRand = TransformNormal(dirRand, rotMat);

		// ==========================
		// 速度・寿命設定
		// ==========================
		float speed = 2.0f + ((float)rand() / RAND_MAX) * 10.0f;
		p.vel = Normalize(dirRand) * speed;
		p.life = 0.2f + ((float)rand() / RAND_MAX) * 0.2f;
		p.size = 0.1f + ((float)rand() / RAND_MAX) * 0.05f;

		particles_.push_back(p);
	}

	// 上限処理
	if (particles_.size() > 2000) {
		particles_.erase(particles_.begin(), particles_.begin() + (particles_.size() - 2000));
	}
}

//--------------------------------
// リング生成
//--------------------------------
using namespace DirectX;

void ParticleSystem::SpawnRing(const Vector3& position, const Vector3& normal, const Vector4& color) {
	Particle p{};
	p.pos = position + normal * 0.05f;
	p.vel = {0, 0, 0};
	p.life = 0.3f;
	p.size = 0.7f;
	p.color = color;
	p.type = 1;

	XMVECTOR up = XMVectorSet(0, 1, 0, 0);
	XMVECTOR n = XMVector3Normalize(XMLoadFloat3((XMFLOAT3*)&normal));

	XMVECTOR axis = XMVector3Cross(up, n);
	float axisLen = XMVectorGetX(XMVector3Length(axis));
	float angle = 0.0f;
	if (axisLen > 1e-5f) {
		axis = XMVector3Normalize(axis);
		float dot = XMVectorGetX(XMVector3Dot(up, n));
		dot = std::clamp(dot, -1.0f, 1.0f);
		angle = std::acos(dot);
	}

	XMMATRIX rotM = XMMatrixRotationAxis(axis, angle);
	XMFLOAT3X3 m;
	XMStoreFloat3x3(&m, rotM);

	Vector3 euler{};
	euler.x = std::atan2(m._32, m._33);
	euler.y = std::atan2(-m._31, std::sqrt(m._32 * m._32 + m._33 * m._33));
	euler.z = std::atan2(m._21, m._11);
	p.rot = euler;

	particles_.push_back(p);
}

//--------------------------------
// 更新
//--------------------------------
void ParticleSystem::Update(float deltaTime) {
	if (particles_.size() > 2000) {
		particles_.erase(particles_.begin(), particles_.begin() + (particles_.size() - 2000));
	}

	for (auto& p : particles_) {
		if (p.type == 0) {
			// --- 火花 ---
			p.pos += p.vel * deltaTime * 10.0f;

			// 残り寿命の割合 (1.0 → 0.0)
			float lifeRatio = p.life / 0.5f; // 寿命が約0.5秒の前提（Spawnで設定してる値に合わせる）

			// 残り寿命が少ないほど小さく
			if (lifeRatio < 0.4f) {
				p.size *= (1.0f - deltaTime * 5.0f); // 徐々に縮小
				if (p.size < 0.02f)
					p.size = 0.02f;
			}

			// フェードアウト
			p.color.w = std::clamp(lifeRatio * 2.0f, 0.0f, 1.0f);

			// 寿命減少
			p.life -= deltaTime * 2.0f;
		} else if (p.type == 1) {
			// --- リング（既存の自然消滅処理）---
			float totalLife = 0.3f;
			float t = (totalLife - p.life) / totalLife; // 0→1

			float scaleCurve = std::sin(t * DirectX::XM_PI);
			p.size = scaleCurve * 1.0f;
			p.color.w = std::clamp((1.0f - t) * 2.0f, 0.0f, 1.0f);

			p.life -= deltaTime;
		}
	}

	// 死亡パーティクル削除
	particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.life <= 0.0f; }), particles_.end());
}

//--------------------------------
// 描画
//--------------------------------
void ParticleSystem::Draw(Renderer& renderer, const Camera& camera, ID3D12GraphicsCommandList* cmd) {
	if (sparkModelHandle_ < 0 || ringModelHandle_ < 0)
		return;

	const UINT kSparkSpan = 2048;
	const UINT kRingSpan = 2048;
	const size_t maxSlots = renderer.MaxModelCBSlots(); // ★ 総スロット
	if (maxSlots == 0)
		return;

	for (size_t i = 0; i < particles_.size(); ++i) {
		const auto& p = particles_[i];
		Transform tf{};
		tf.translate = p.pos;
		tf.rotate = p.rot;
		Vector4 color = p.color;

		if (p.type == 0) {
			tf.scale = {p.size, p.size, p.size};
			size_t raw = size_t(baseSlot_) + (i % kSparkSpan);
			UINT slot = UINT(raw % maxSlots); // ★ 丸める
			renderer.UpdateModelCBWithColorAt(sparkModelHandle_, slot, camera, tf, color);
			renderer.DrawModelAt(sparkModelHandle_, cmd, slot);
		} else {
			tf.scale = {p.size, 0.01f, p.size};
			size_t raw = size_t(baseSlot_) + 2048 + (i % kRingSpan);
			UINT slot = UINT(raw % maxSlots); // ★ 丸める
			renderer.UpdateModelCBWithColorAt(ringModelHandle_, slot, camera, tf, color);
			renderer.DrawModelAt(ringModelHandle_, cmd, slot);
		}
	}
}

} // namespace Engine
