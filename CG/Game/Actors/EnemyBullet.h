#pragma once
#include <d3d12.h>
#include <vector>

#include "AABB.h"
#include "Camera.h"
#include "LaserManager.h" // ★追加：レーザーとの衝突判定で使用
#include "Renderer.h"
#include "Transform.h"

namespace Engine {

class EnemyBullet {
public:
	// 弾生成
	void Spawn(const Vector3& pos, const Vector3& dir, float speed, float lifeSec = 3.0f);

	// 更新（壁・レーザーとの判定を含む）
	void Update(const std::vector<AABB>& walls, LaserManager& lasers, Camera& cam, float dt);

	// 描画
	void Draw(Renderer& renderer, const Camera& cam, ID3D12GraphicsCommandList* cmd, UINT slot);

	// 当たり判定（球 vs AABB）
	static bool SphereAABB(const Vector3& c, float r, const AABB& b);

	// モデルハンドルを設定
	void SetModelHandle(int h) { modelHandle_ = h; }

	void SetHeightBias(float biasY) { heightBiasY_ = biasY; } // 追加：下げ量を外側から設定できる

	// 状態アクセサ
	bool IsAlive() const { return alive_; }
	void Kill() { alive_ = false; }
	const Transform& GetTransform() const { return transform_; }
	float Radius() const { return radius_; }

private:
	Transform transform_{};
	Vector3 vel_{0, 0, 0};
	float speed_ = 0.0f;
	float life_ = 0.0f;
	bool alive_ = false;

	float heightBiasY_ = -0.30f; // 追加：デフォルトで 30cm 下げる（好みで調整）

	float radius_ = 0.25f; // 衝突半径
	int modelHandle_ = -1; // モデルハンドル
};

} // namespace Engine
