#include "EnemyBullet.h"
#include <algorithm>
#include <cmath>

namespace {
inline Engine::Vector3 add(const Engine::Vector3& a, const Engine::Vector3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Engine::Vector3 mul(const Engine::Vector3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float len2(const Engine::Vector3& a) { return a.x * a.x + a.y * a.y + a.z * a.z; }
inline float len(const Engine::Vector3& a) { return std::sqrtf(len2(a)); }
inline Engine::Vector3 norm(const Engine::Vector3& a) {
	float l = len(a);
	return (l > 1e-6f) ? Engine::Vector3{a.x / l, a.y / l, a.z / l} : Engine::Vector3{0, 0, 0};
}
} // namespace

namespace Engine {

bool EnemyBullet::SphereAABB(const Vector3& c, float r, const AABB& b) {
	float cx = (std::max)(b.min.x, (std::min)(c.x, b.max.x));
	float cy = (std::max)(b.min.y, (std::min)(c.y, b.max.y));
	float cz = (std::max)(b.min.z, (std::min)(c.z, b.max.z));
	float dx = c.x - cx, dy = c.y - cy, dz = c.z - cz;
	return (dx * dx + dy * dy + dz * dz) <= r * r;
}

void EnemyBullet::Spawn(const Vector3& pos, const Vector3& dir, float speed, float lifeSec) {
	transform_.translate = pos;
	transform_.translate.y += heightBiasY_;
	transform_.scale = {0.2f, 0.2f, 0.2f};
	transform_.rotate = {0, 0, 0};

	vel_ = norm(dir);
	speed_ = speed;
	life_ = lifeSec;
	alive_ = true;
}

void EnemyBullet::Update(const std::vector<AABB>& walls, LaserManager& lasers, Camera& cam, float dt) {
	if (!alive_)
		return;

	// 移動
	transform_.translate = add(transform_.translate, mul(vel_, speed_ * dt));
	life_ -= dt;
	if (life_ <= 0.0f) {
		alive_ = false;
		return;
	}

	// --- 壁に当たったら消す ---
	for (const auto& w : walls) {
		if (SphereAABB(transform_.translate, radius_, w)) {
			alive_ = false;
			return;
		}
	}

	// --- 既に存在するレーザーに当たったら消す＆画面シェイク ---
	if (lasers.HitSphere(transform_.translate, radius_, cam)) {
		alive_ = false;
		return;
	}
}

void EnemyBullet::Draw(Renderer& renderer, const Camera& cam, ID3D12GraphicsCommandList* cmd, UINT slot) {
	if (!alive_ || modelHandle_ < 0)
		return;

	renderer.UpdateModelCBWithColorAt(modelHandle_, slot, cam, transform_, {1, 0.25f, 0.25f, 1});
	renderer.DrawModelAt(modelHandle_, cmd, slot);
}

} // namespace Engine
