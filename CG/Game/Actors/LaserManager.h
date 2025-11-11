//#pragma once
//#include "AABB.h"
//#include "Laser.h"
//#include "Transform.h"
//#include <cstdint>
//#include <vector>
//#include "ParticleSystem.h"
//
//namespace Engine {
//
//class Renderer;
//class Camera;
//class Enemy;
//
//class LaserManager {
//public:
//
//	   void InitializeParticle(Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const std::string& sparkPath, const std::string& ringPath);
//
//	// 発射
//	void Shoot(const Vector3& startPos, const Vector3& dir);
//
//	// 張られているレーザー群の更新
//	void Update(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, float deltaTime);
//
//	// 描画
//	void Draw(Renderer& renderer, int modelHandleNormal, int modelHandleEnhanced, const Camera& cam, ID3D12GraphicsCommandList* cmd);
//
//	// 予測線の描画（未発射でも）
//	void DrawPredictionFrom(
//	    Renderer& renderer, int modelHandle, const Camera& cam, ID3D12GraphicsCommandList* cmd, const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls,
//	    const std::vector<float>& prismAngles, const Vector3& startPos, const Vector3& dir, float travel, int maxBounces);
//
//	// ――― 衝突判定 ―――
//	// 既に張られている線と球の当たり（副作用なし）
//	bool HitSphere(const Vector3& center, float radius) const;
//
//	// 既に張られている線と球の当たり（命中時にカメラシェイク）
//	bool HitSphere(const Vector3& center, float radius, Camera& cam) const;
//
//	// 既存：敵のAABBと当たり（ダメージ）
//	bool CheckHitEnemy(Enemy& enemy);
//
//	// 必要に応じて消すユーティリティ
//	void Clear() { lasers_.clear(); }
//
//	// 外から読みたい場合
//	const std::vector<Laser>& GetLasers() const { return lasers_; }
//
//	 void SetEnhancedMode(bool flag) { enhancedMode_ = flag; }//強化させるよう
//	bool IsEnhancedMode() const { return enhancedMode_; }//強化中
//	
//	void ResetKillCount() { enemiesKilledInOneShot_ = 0; }
//
//	void SetParticleModelHandle(int handle);
//
//	ParticleSystem& GetParticleSystem() { return particles_; }
//	ParticleSystem& GetWallParticleSystem() { return particleWall_; }
//
//	 Vector3 GetLastLaserDir() const { return lastLaserDir_; }
//
//private:
//	std::vector<Laser> lasers_;
//	bool enhancedMode_ = false; // ← 強化中かどうか
//	int enemiesKilledInOneShot_ = 0;//一回でどれくらい倒したか
//
//	int particleModelHandle_ = -1;
//
//	 ParticleSystem sparkTemplate_; // 火花＆リング用のテンプレート
//	int particleModelHandleSpark_ = -1;
//	int particleModelHandleRing_ = -1;
//
//	  ParticleSystem particles_;
//	ParticleSystem particleWall_; // ← 壁用
//	  ParticleSystem particlePrefab_; // テンプレ
//
//	      Vector3 lastLaserDir_{0, 0, 1};
//
//};
//
//} // namespace Engine
