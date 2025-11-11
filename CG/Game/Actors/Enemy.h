//#pragma once
//// =============================================================
//// Enemy : 追尾 → 予備動作 → 射撃 → クールダウン
//// 仕様：
////  - 敵本体がプレイヤーレーザーに触れるとダメージ
////  - 敵弾がプレイヤーに当たるとダメージ
////  - 敵弾がプレイヤーレーザーに触れたら弾は消滅（プレイヤーは無傷）
//// =============================================================
//#include <d3d12.h>
//#include <vector>
//
//#include "AABB.h"
//#include "Camera.h"
//#include "EnemyBullet.h"
//#include "LaserManager.h"
//#include "Player.h"
//#include "Renderer.h"
//#include "Transform.h"
//
//namespace Engine {
//
//class Enemy {
//public:
//	void Initialize(Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd);
//
//	// Player はダメージを与えるため参照（非 const）
//	// Camera はシェイク等で利用するため参照
//	// LaserManager は弾更新・当たり判定で利用するため参照
//	void Update(float dt, Player& player, const std::vector<AABB>& walls, Camera& cam, LaserManager& lasers);
//
//	void Draw(Renderer& renderer, const Camera& cam, ID3D12GraphicsCommandList* cmd);
//	void DrawBullets(Renderer& renderer, const Camera& cam, ID3D12GraphicsCommandList* cmd);
//
//	void Damage(int dmg);
//	AABB GetAABB() const;
//	bool IsAlive() const { return isAlive_; }
//
//	const Transform& GetTransform() const { return transform_; }
//	Transform& GetTransform() { return transform_; }
//
//	// レーザー接触フラグ（描画色切り替え用）
//	void SetHit(bool h) { hit_ = h; }
//	bool IsHit() const { return hit_; }
//
//	// レーザーとの接触検出（簡易）：レーザー線分群と敵中心球
//	void CheckLaserHit(const LaserManager& laserManager);
//
//private:
//	enum class State { Patrol, PreAttack, Shoot, Cooldown };
//
//	void FaceTo(const Vector3& dir, float lerp = 0.2f);
//	void SpawnBulletToward(const Vector3& target);
//
//private:
//	// モデル
//	int modelHandle_ = -1;       // 本体
//	int markerHandle_ = -1;      // 予備動作マーカー
//	int bulletModelHandle_ = -1; // 弾モデル
//
//	// 変換
//	Transform transform_{};
//
//	// ステータス
//	bool isAlive_ = true;
//	float hp_ = 3.0f;
//	float radius_ = 0.8f; // 衝突判定半径（本体）
//
//	// AI
//	State state_ = State::Patrol;
//	float stateTimer_ = 0.0f;
//	Vector3 moveDir_{1, 0, 0};
//	float moveSpeed_ = 2.2f;
//	float preTime_ = 0.6f;
//	float cdTime_ = 0.7f;
//
//	// 弾
//	std::vector<EnemyBullet> bullets_;
//	float bulletSpeed_ = 9.0f;
//	float bulletLife_ = 3.0f;
//
//	// レーザー接触の可視化
//	bool hit_ = false;
//};
//
//} // namespace Engine
