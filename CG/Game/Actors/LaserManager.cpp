//// ================================
//// LaserManager.cpp
//// ================================
//#include "LaserManager.h"
//#include "Camera.h"
//#include "Collision.h"
//#include "Enemy.h"
//#include "Renderer.h"
//#include <Windows.h> // OutputDebugStringA
//#include <algorithm>
//
//namespace Engine {
//
//// 初期化
//void LaserManager::InitializeParticle(Renderer& r, ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, const std::string& sparkPath, const std::string& ringPath) {
//	particlePrefab_.Initialize(r, dev, cmd, sparkPath, ringPath, 0); // baseSlot=0
//}
//
//
////-------------------------------------------
//// 発射
////-------------------------------------------
//void LaserManager::Shoot(const Vector3& startPos, const Vector3& dir) {
//	enemiesKilledInOneShot_ = 0;
//
//	// 既に伸びているレーザーがある場合はリセットしない
//	bool hasActiveLaser = false;
//	for (auto& l : lasers_) {
//		if (l.IsActive() && !l.IsEnhanced()) {
//			hasActiveLaser = true;
//			break;
//		}
//	}
//
//	if (!hasActiveLaser) {
//		enemiesKilledInOneShot_ = 0; // ← 新規発射時だけリセット
//	}
//
//	// --- 既存レーザーを縮退開始 ---
//	for (auto& l : lasers_) {
//		if (l.IsActive() && !l.IsEnhanced()) {
//			l.StartRetract(0.18f); // ← 置き換え時はキビキビ消す
//		}
//	}
//
//	// --- 新しいレーザーを追加 ---
//	lasers_.emplace_back();
//	Laser& laser = lasers_.back();
//
//	// ★ 新しいレーザーにユニークIDを割り当て
//	int activeIndex = static_cast<int>(lasers_.size()) - 1;
//	laser.SetUniqueID(activeIndex);
//
//	// 初期化
//	laser.Shoot(startPos, dir);
//	laser.SetEnhanced(false);
//	laser.ResetEnemyKill();
//	lastLaserDir_ = Normalize(dir); // ← 進行方向を記録
//	// ======================================================
//	// 🔧 ここから追加（パーティクル初期化）
//	// ======================================================
//
//	// LaserManager 側に「particlePrefab_」などを用意しておくことが前提。
//	// → InitializeParticle() で一度だけ prefab_.Initialize() しておく。
//
//	if (particlePrefab_.IsInitialized()) {                   // 安全チェック
//		laser.spark_ = particlePrefab_;                      // コピー
//		const UINT kLaserSlotSpan = 64;                      // 各レーザーが使うCBVスロット範囲
//		UINT baseSlot = (activeIndex % 64) * kLaserSlotSpan; // 0〜4096未満に収める
//		laser.spark_.SetBaseSlot(baseSlot);
//	} else {
//		OutputDebugStringA("[LaserMgr] WARNING: particlePrefab_ not initialized!\n");
//	}
//
//	// ======================================================
//
//	OutputDebugStringA("[LaserMgr] Shoot: new laser created.\n");
//}
//
////-------------------------------------------
//// 更新
////-------------------------------------------
//void LaserManager::Update(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, float deltaTime) {
//	
//   // === まず全レーザーのUpdateフラグをリセット ===
//	for (auto& l : lasers_) {
//		l.ClearUpdateFlag();
//	}
//
//	// ★ 1回だけ Update を呼ぶ。active か retracting のどちらかなら更新する
//	for (auto& l : lasers_) {
//		if (l.IsActive() || l.IsRetracting()) {
//			l.Update(walls, prismWalls, prismAngles, deltaTime);
//		}
//	}
//
//	// ★ 削除条件は「リトラクト完了後」
//	lasers_.erase(std::remove_if(lasers_.begin(), lasers_.end(), [](const Laser& l) { return (!l.IsActive() && !l.IsRetracting()); }), lasers_.end());
//
//	// --- すべてのレーザーが止まった（伸び切った）ならリセット ---
//	bool allStopped = true;
//	int activeCount = 0;
//	for (auto& l : lasers_) {
//		if (l.IsActive() || l.IsRetracting())
//			activeCount++;
//
//		if (!l.IsStopped()) {
//			allStopped = false;
//		}
//	}
//
//	char buf[128];
//	sprintf_s(buf, "[LaserMgr] lasers=%d allStopped=%d\n", activeCount, allStopped ? 1 : 0);
//	OutputDebugStringA(buf);
//	if (allStopped) {
//		// --- キルカウントリセット ---
//		ResetKillCount();
//
//		// --- 強化解除 ---
//		for (auto& l : lasers_) {
//			if (l.IsEnhanced()) {
//				l.SetEnhanced(false);
//				OutputDebugStringA("[LaserMgr] Enhanced mode cleared\n");
//			}
//		}
//
//		OutputDebugStringA("[LaserMgr] All lasers stopped -> KillCount reset + enhanced cleared\n");
//	}
//}
//
//
////-------------------------------------------
//// 描画
////-------------------------------------------
//void LaserManager::Draw(Renderer& renderer, int modelHandleNormal, int modelHandleEnhanced, const Camera& cam, ID3D12GraphicsCommandList* cmd) {
//	for (auto& l : lasers_) {
//		if (l.IsActive() || l.IsRetracting()) {
//			l.Draw(cmd, renderer, modelHandleNormal, modelHandleEnhanced, cam);
//		}
//	}
//
//}
//
//
////-------------------------------------------
//// 予測線（未発射でも出せる）
//void LaserManager::DrawPredictionFrom(
//    Renderer& renderer, int modelHandle, const Camera& cam, ID3D12GraphicsCommandList* cmd, const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles,
//    const Vector3& startPos, const Vector3& dir, float travel, int maxBounces) {
//
//	Laser temp;
//	temp.Shoot(startPos, dir);
//
//	std::vector<Transform> segs;
//	temp.BuildPrediction(walls, prismWalls, prismAngles, travel, maxBounces, segs);
//
//	// 赤で描画
//	for (size_t i = 0; i < segs.size(); ++i) {
//		renderer.UpdateModelCBWithColorAt(modelHandle, (UINT)i, cam, segs[i], {1, 0, 0, 1});
//		renderer.DrawModelAt(modelHandle, cmd, (UINT)i);
//	}
//
//	OutputDebugStringA(("BuildPrediction seg count = " + std::to_string(segs.size()) + "\n").c_str());
//}
//
//
////-------------------------------------------
//// 「既に張られている線分」との球ヒット（副作用なし）
////-------------------------------------------
//bool LaserManager::HitSphere(const Vector3& center, float radius) const {
//	if (lasers_.empty())
//		return false;
//
//	float t;
//	Vector3 n;
//	for (const auto& l : lasers_) {
//		if (!l.IsActive())
//			continue;
//
//		const auto& segs = l.GetSegmentsEndpoints(); // 端点で正確に
//		for (const auto& s : segs) {
//			if (Collision::IntersectSegmentSphere(s.a, s.b, center, radius, t, n)) {
//				return true;
//			}
//		}
//	}
//	return false;
//}
//
////-------------------------------------------
//// 「既に張られている線分」との球ヒット（命中時にカメラを揺らす）
////-------------------------------------------
//bool LaserManager::HitSphere(const Vector3& center, float radius, Camera& cam) const {
//	if (HitSphere(center, radius)) {
//		cam.StartShake(0.25f, 0.06f, DirectX::XMConvertToRadians(0.6f));
//		return true;
//	}
//	return false;
//}
//
////-------------------------------------------
//// 敵AABBとの直撃（従来機能）
////-------------------------------------------
//bool LaserManager::CheckHitEnemy(Enemy& enemy) {
//	if (!enemy.IsAlive())
//		return false;
//
//	const AABB box = enemy.GetAABB();
//	float t;
//	Vector3 n;
//	bool hit = false;
//
//	for (auto& l : lasers_) {
//		if (!l.IsActive())
//			continue;
//
//		const auto& segs = l.GetSegmentsEndpoints();
//		for (const auto& s : segs) {
//			if (Collision::IntersectSegmentAABB(s.a, s.b, box, t, n)) {
//
//				enemy.Damage(1);
//				hit = true;
//
//				// ← Laser自身に通知
//				l.OnKillEnemy();
//
//				char buf[128];
//				sprintf_s(buf, "[LaserMgr] Enemy hit by laser (%d kills)\n", enemy.IsAlive() ? 0 : 1);
//				OutputDebugStringA(buf);
//
//			}
//		}
//	}
//
//	return hit;
//}
//
//} // namespace Engine
