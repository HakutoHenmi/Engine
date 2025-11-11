//// ================================
//// Laser.cpp
//// ================================
//#include "Laser.h"
//#include "LaserManager.h"
//#include <Windows.h>
//#include <algorithm>
//#include <cmath>
//#include <string>
//
//namespace Engine {
//
////--------------------------------------------
//// 反射ベクトル
////--------------------------------------------
//Vector3 Laser::Reflect(const Vector3& d, const Vector3& n) {
//	float dot = d.x * n.x + d.y * n.y + d.z * n.z;
//	Vector3 r{d.x - 2.f * dot * n.x, d.y - 2.f * dot * n.y, d.z - 2.f * dot * n.z};
//	// 正規化
//	float len = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
//	if (len > 1e-6f) {
//		r.x /= len;
//		r.y /= len;
//		r.z /= len;
//	}
//	return r;
//}
//
////--------------------------------------------
//// コンストラクタ
////--------------------------------------------
//Laser::Laser() {
//	startPos_ = {0, 0, 0};
//	dir_ = {0, 0, 1};
//	currentLength_ = 0.f;
//	maxLength_ = 5000.f;
//	speed_ = 80.f;
//	active_ = false;
//	segXforms_.clear();
//	segEnds_.clear();
//}
//
////--------------------------------------------
//// 発射
////--------------------------------------------
//void Laser::Shoot(const Vector3& startPos, const Vector3& dir) {
//	uniqueID_ = s_nextID_++ % 64; // 64レーザーでローテーション
//	bounceCount_ = 0;
//	reflectHistory_.clear();
//	effectSpawnedCount_ = 0; // ★ ここでリセット
//	stopped_ = false;        // ← ここでリセット！
//	bounceCount_ = 0;
//	killsThisShot_ = 0;
//	startPos_ = startPos;
//	float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
//	dir_ = (len > 0.f) ? Vector3{dir.x / len, dir.y / len, dir.z / len} : Vector3{1, 0, 0};
//	currentLength_ = 0.f;
//	active_ = true;
//	segXforms_.clear();
//	segEnds_.clear();
//
//	lifeTime_ = 0.0f;
//	enhancedLife_ = 0.0f;
//
//	// 最大距離か
//	isMaxLength = false;
//
//	active_ = true;
//	retracting_ = false;
//	state_ = LaserState::Growing;
//}
//
//void Laser::StartRetract(float durationSec) {
//	retracting_ = true;
//	state_ = LaserState::Retracting;
//
//	// --- 通常レーザーと強化レーザーで別スピード ---
//	float dur;
//	if (enhanced_) {
//		// 強化レーザー専用の縮退時間
//		dur = enhancedRetractDuration_;
//	} else {
//		// 通常レーザー
//		dur = (durationSec > 0.0f) ? durationSec : retractDurationDefault_;
//	}
//
//	dur = std::max<float>(0.016f, dur); // 1フレーム未満は不可
//
//	retractSpeed_ = std::max<float>(1.0f, GetCurrentLength() / dur);
//}
//
////--------------------------------------------
//// 予測線（Transform 群を生成）
////--------------------------------------------
//void Laser::BuildPrediction(
//    const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, float /*step*/, int maxbounceCount_s, std::vector<Transform>& out) const {
//	out.clear();
//
//	Vector3 pos = startPos_;
//	Vector3 dir = dir_;
//	const float smallNum = 0.001f;
//	const float previewLength = 1000.0f;
//	float traveled = 0.0f;
//
//	int bounce = 0;
//	int lastPrismIndex = -1; // ★ 直前に通過したプリズムを記録
//
//	while (true) {
//		if (bounce > maxbounceCount_s)
//			break;
//
//		float nearestT = 1e9f;
//		Vector3 hitNormal{};
//		bool hit = false;
//		bool hitPrism = false;
//		float prismAngleDeg = 0.0f;
//		int hitPrismIndex = -1;
//
//		// --- 通常壁 ---
//		for (const auto& wall : walls) {
//			float t;
//			Vector3 n;
//			if (Collision::IntersectRayAABBExpanded(pos, dir, wall, 0.5f, t, n)) {
//				if (t < nearestT && t > 1e-4f) {
//					nearestT = t;
//					hitNormal = n;
//					hit = true;
//				}
//			}
//		}
//
//		// --- プリズム壁 ---
//		for (size_t i = 0; i < prismWalls.size(); ++i) {
//			if ((int)i == lastPrismIndex)
//				continue; // ★ 直前のプリズムをスキップ
//
//			float t;
//			Vector3 n;
//			if (Collision::IntersectRayAABBExpanded(pos, dir, prismWalls[i], 0.5f, t, n)) {
//				if (t < nearestT && t > 1e-4f) {
//					nearestT = t;
//					hitNormal = n;
//					hit = true;
//					hitPrism = true;
//					prismAngleDeg = prismAngles[i];
//					hitPrismIndex = static_cast<int>(i);
//				}
//			}
//		}
//
//		// --- 衝突点 ---
//		Vector3 endPos = hit ? pos + dir * nearestT : pos + dir * 100.0f;
//		Vector3 seg = endPos - pos;
//		float len = std::sqrt(seg.x * seg.x + seg.z * seg.z);
//
//		// --- 線分登録 ---
//		if (len > 1e-4f) {
//			Transform tf{};
//			tf.translate = {(pos.x + endPos.x) * 0.5f, (pos.y + endPos.y) * 0.5f, (pos.z + endPos.z) * 0.5f};
//			tf.scale = {0.2f, 0.2f, len * 0.5f};
//			tf.rotate = {0.0f, std::atan2(seg.x, seg.z), 0.0f};
//			out.push_back(tf);
//		}
//
//		traveled += len;
//		if (!hit || traveled >= previewLength)
//			break;
//
//		// --- プリズム処理 ---
//		if (hitPrism) {
//			float rad = XMConvertToRadians(prismAngleDeg);
//			dir = {std::sin(rad), 0.0f, std::cos(rad)};
//			dir = Normalize(dir);
//
//			const AABB& prism = prismWalls[hitPrismIndex];
//			Vector3 prismCenter = {(prism.min.x + prism.max.x) * 0.5f, (prism.min.y + prism.max.y) * 0.5f, (prism.min.z + prism.max.z) * 0.5f};
//
//			// --- 表面→中心補間（吸い込み演出） ---
//			Vector3 midSeg = prismCenter - endPos;
//			float lenMid = std::sqrt(midSeg.x * midSeg.x + midSeg.z * midSeg.z);
//			if (lenMid > 1e-4f) {
//				Transform tf{};
//				tf.translate = {(endPos.x + prismCenter.x) * 0.5f, (endPos.y + prismCenter.y) * 0.5f, (endPos.z + prismCenter.z) * 0.5f};
//				tf.scale = {0.2f, 0.2f, lenMid * 0.5f};
//				tf.rotate = {0.0f, std::atan2(midSeg.x, midSeg.z), 0.0f};
//				out.push_back(tf);
//			}
//
//			// --- 中心から再出射 ---
//			pos = prismCenter + dir * 0.2f; // ★ 少し離す
//			lastPrismIndex = hitPrismIndex; // ★ 無限再ヒット防止
//			continue;
//		}
//
//		// --- 壁反射 ---
//		dir = Reflect(dir, hitNormal);
//		++bounce;
//		pos = endPos + dir * smallNum;
//
//		lastPrismIndex = -1; // ★ 通常壁に当たったらリセット
//	}
//}
//
////--------------------------------------------
//// レーザー発射初期化
////--------------------------------------------
//// void Laser::Shoot(const Vector3& startPos, const Vector3& dir) {
////	startPos_ = startPos;
////	float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
////	dir_ = (len > 0.f) ? Vector3{dir.x / len, dir.y / len, dir.z / len} : Vector3{1, 0, 0};
////
////	active_ = true;
////	segments_.clear();
////}
//
////--------------------------------------------
//// 更新（反射2回 + 伸びアニメ + プリズム）
////--------------------------------------------
//void Laser::Update(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, float deltaTime) {
//
//	// --- 強化レーザー寿命チェック ---
//	if (enhanced_) {
//		enhancedLife_ += deltaTime;
//		if (enhancedLife_ > enhancedLifeLimit_) {
//			StartRetract(0.5f); // 時間が来たら縮退開始
//		}
//	}
//
//	updatedThisFrame_ = true;
//
//	switch (state_) {
//	case LaserState::Growing:
//		UpdateGrow(walls, prismWalls, prismAngles, deltaTime);
//		break;
//	case LaserState::Retracting:
//		UpdateRetract(walls, prismWalls, prismAngles, deltaTime);
//		break;
//	default:
//		break;
//	}
//
//	// ★★★ ここを追加 ★★★
//	spark_.Update(deltaTime);
//}
//
////--------------------------------------------
//// 共通：レーザーの線分を構築（反射＆プリズム対応）
////--------------------------------------------
//void Laser::BuildLaserSegments(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, bool retractMode /*= false*/) {
//	segXforms_.clear();
//	segEnds_.clear();
//
//	Vector3 pos = startPos_;
//	Vector3 dir = dir_;
//	const int maxReflections = 2;
//	float traveled = 0.0f;
//
//	int bounce = 0;
//	int lastPrismIndex = -1;
//
//	while (true) {
//		if (bounce > maxReflections)
//			break;
//
//		float nearestT = 1e9f;
//		Vector3 hitNormal{};
//		bool hit = false;
//		bool hitPrism = false;
//		float prismAngleDeg = 0.0f;
//		int hitPrismIndex = -1;
//
//		// --- 通常壁 ---
//		for (const auto& wall : walls) {
//			float t;
//			Vector3 n;
//			if (Collision::IntersectRayAABBExpanded(pos, dir, wall, 0.5f, t, n)) {
//				if (t < nearestT && t > 1e-4f) {
//					nearestT = t;
//					hitNormal = n;
//					hit = true;
//				}
//			}
//		}
//
//		// --- プリズム壁 ---
//		for (size_t i = 0; i < prismWalls.size(); ++i) {
//			if ((int)i == lastPrismIndex)
//				continue;
//			float t;
//			Vector3 n;
//			if (Collision::IntersectRayAABBExpanded(pos, dir, prismWalls[i], 0.5f, t, n)) {
//				if (t < nearestT && t > 1e-4f) {
//					nearestT = t;
//					hitNormal = n;
//					hit = true;
//					hitPrism = true;
//					prismAngleDeg = prismAngles[i];
//					hitPrismIndex = static_cast<int>(i);
//				}
//			}
//		}
//
//		float segLen = hit ? nearestT : 9999.0f;
//
//		// 「今のセグメントが見える範囲」を超えるなら、そこで止める
//		float visibleLimit = currentLength_ - traveled;
//		if (segLen > visibleLimit) {
//			segLen = visibleLimit;
//			hit = false; // 壁に届かない扱い（反射はしない）
//		}
//
//		if (segLen <= 0.0f)
//			break;
//
//		Vector3 endPos = pos + dir * segLen;
//
//		// --- 線分登録 ---
//		Vector3 seg = endPos - pos;
//		float lenXZ = std::sqrt(seg.x * seg.x + seg.z * seg.z);
//		if (lenXZ > 1e-4f) {
//			Transform tf{};
//			tf.translate = {(pos.x + endPos.x) * 0.5f, (pos.y + endPos.y) * 0.5f, (pos.z + endPos.z) * 0.5f};
//			tf.scale = {0.2f, 0.2f, lenXZ * 0.5f};
//			tf.rotate = {0.0f, std::atan2(-seg.x, -seg.z), 0.0f};
//			segXforms_.push_back(tf);
//		}
//
//		segEnds_.push_back({pos, endPos});
//		traveled += lenXZ;
//		if (!hit || traveled >= currentLength_)
//			break;
//
//		// --- プリズム処理 ---
//		if (hitPrism) {
//			float rad = XMConvertToRadians(prismAngleDeg);
//			dir = {std::sin(rad), 0.0f, std::cos(rad)};
//			dir = Normalize(dir);
//
//			const AABB& prism = prismWalls[hitPrismIndex];
//			Vector3 prismCenter = {(prism.min.x + prism.max.x) * 0.5f, (prism.min.y + prism.max.y) * 0.5f, (prism.min.z + prism.max.z) * 0.5f};
//
//			// 吸い込み線
//			Vector3 midSeg = prismCenter - endPos;
//			float lenMid = std::sqrt(midSeg.x * midSeg.x + midSeg.z * midSeg.z);
//			if (lenMid > 1e-4f) {
//				Transform tf{};
//				tf.translate = {(endPos.x + prismCenter.x) * 0.5f, (endPos.y + prismCenter.y) * 0.5f, (endPos.z + prismCenter.z) * 0.5f};
//				tf.scale = {0.2f, 0.2f, lenMid * 0.5f};
//				tf.rotate = {0.0f, std::atan2(midSeg.x, midSeg.z), 0.0f};
//				segXforms_.push_back(tf);
//			}
//
//			pos = prismCenter + dir * 0.2f;
//			lastPrismIndex = hitPrismIndex;
//			continue;
//		}
//
//		// --- 通常壁反射 ---
//		dir = Reflect(dir, hitNormal);
//		++bounce;
//		pos = endPos + dir * 0.01f;
//		lastPrismIndex = -1;
//
//		// ★ 一度だけエフェクトを出す（反射位置ごとに一回だけ）
//		bool alreadySpawned = false;
//		for (const auto& prev : reflectHistory_) {
//			float distSq = (prev - endPos).x * (prev - endPos).x + (prev - endPos).y * (prev - endPos).y + (prev - endPos).z * (prev - endPos).z;
//			if (distSq < 0.01f) { // ほぼ同じ位置なら既に出した
//				alreadySpawned = true;
//				break;
//			}
//		}
//
//		if (!alreadySpawned && !hitPrism) {
//			// 登録
//			reflectHistory_.push_back(endPos);
//
//			// エフェクト生成（リング＋火花）
//			Vector4 sparkColor{1.0f, 0.8f, 0.3f, 1.0f};
//			Vector4 ringColor{0.2f, 1.0f, 1.0f, 1.0f};
//
//			//散らす
//			spark_.SetSpreadAngle(2.0f);
//			spark_.Spawn(endPos, hitNormal, 32, {1, 0.8f, 0.3f, 1}, 0.1f); 
//			spark_.SpawnRing(endPos, hitNormal, ringColor);
//		}
//
//	}
//
//	// === 縮退モード：1本目 → 2本目 → 3本目の順に消える ===
//	if (retractMode) {
//		float visible = currentLength_;
//
//		if (visible < 0.5f) {
//			segXforms_.clear();
//			segEnds_.clear();
//			return;
//		}
//
//		std::vector<Transform> newSegs;
//		std::vector<Segment> newEnds;
//		newSegs.reserve(segXforms_.size());
//		newEnds.reserve(segEnds_.size());
//
//		float totalLen = 0.0f;
//		for (auto& s : segEnds_) {
//			Vector3 d = s.b - s.a;
//			totalLen += std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
//		}
//
//		float visibleRemaining = std::clamp(visible, 0.0f, totalLen);
//
//		// ★ 発射順のまま処理する（反転しない）
//		for (size_t i = 0; i < segEnds_.size(); ++i) {
//			const auto& s = segEnds_[i];
//			Vector3 d = s.b - s.a;
//			float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
//
//			if (visibleRemaining >= len) {
//				newEnds.push_back(s);
//				newSegs.push_back(segXforms_[i]);
//				visibleRemaining -= len;
//			} else if (visibleRemaining > 0.0f) {
//				float ratio = visibleRemaining / len;
//				Vector3 newA = s.a;
//				Vector3 newB = s.a + d * ratio;
//
//				Segment partial{newA, newB};
//				newEnds.push_back(partial);
//
//				Transform tf{};
//				tf.translate = {(newA.x + newB.x) * 0.5f, (newA.y + newB.y) * 0.5f, (newA.z + newB.z) * 0.5f};
//				tf.scale = {0.2f, 0.2f, (visibleRemaining) * 0.5f};
//				tf.rotate = {0.0f, std::atan2(d.x, d.z), 0.0f};
//				newSegs.push_back(tf);
//				break;
//			} else {
//				break;
//			}
//		}
//
//		segEnds_ = std::move(newEnds);
//		segXforms_ = std::move(newSegs);
//	}
//}
//
//void Laser::UpdateGrow(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, float dt) {
//	currentLength_ += speed_ * dt;
//	if (currentLength_ >= maxLength_) {
//		currentLength_ = maxLength_;
//		stopped_ = true;
//		state_ = LaserState::FullLength;
//
//		// --- 強化レーザーでも消えるように ---
//		if (!enhanced_) {
//			StartRetract(retractDurationOnStop_);
//		} else {
//			// 強化レーザーは設定された寿命時間で消滅
//			lifeTime_ += dt;
//			if (lifeTime_ > enhancedLifeLimit_) {
//				StartRetract(0.4f); // 時間が来たら縮退開始
//				lifeTime_ = 0.0f;   // タイマーリセット
//			}
//		}
//
//		return;
//	}
//
//	BuildLaserSegments(walls, prismWalls, prismAngles);
//}
//
//void Laser::UpdateRetract(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, float dt) {
//	currentLength_ -= retractSpeed_ * dt;
//
//	// --- ほぼゼロになったら完全に消去 ---
//	if (currentLength_ <= 0.5f) {
//		currentLength_ = 0.0f;
//		active_ = false;
//		retracting_ = false; // ★ これを追加！
//		state_ = LaserState::Inactive;
//		segXforms_.clear(); // ★ 残り線を明示的に消す
//		segEnds_.clear();
//		return;
//	}
//
//	BuildLaserSegments(walls, prismWalls, prismAngles, /*retractMode=*/true);
//}
//
//void Laser::OnKillEnemy() {
//	char buf[128];
//	sprintf_s(buf, "[Laser] OnKillEnemy: kills=%d, bounce=%d\n", killsThisShot_, bounceCount_);
//	OutputDebugStringA(buf);
//
//	killsThisShot_++;
//
//	// 1発のレーザーで2体以上倒したらのみ強化
//	if (!enhanced_ && killsThisShot_ >= 2) {
//		SetEnhanced(true);
//		SetEnhancedLifeLimit(3.0f);
//		OutputDebugStringA("[Laser] Enhanced Mode Activated (self)\n");
//	}
//}
//
////--------------------------------------------
//// 描画
////--------------------------------------------
//void Laser::Draw(ID3D12GraphicsCommandList* cmd, Renderer& renderer, int modelHandleNormal, int modelHandleEnhanced, const Camera& camera) {
//	if (!active_ && !retracting_)
//		return;
//
//	if (segXforms_.empty())
//		return;
//
//	int handle = enhanced_ ? modelHandleEnhanced : modelHandleNormal;
//
//	const int kSlotsPerLaser = 32;
//	int slotBase = uniqueID_ * kSlotsPerLaser;
//
//	// ★ 描画を逆順にする（末尾→先頭）
//	for (int i = static_cast<int>(segXforms_.size()) - 1; i >= 0; --i) {
//		Vector4 color = enhanced_ ? Vector4{0.2f, 1.0f, 1.0f, 1.0f} : Vector4{1.0f, 1.0f, 0.0f, 1.0f};
//		renderer.UpdateModelCBWithColorAt(handle, slotBase + (UINT)i, camera, segXforms_[i], color);
//		renderer.DrawModelAt(handle, cmd, slotBase + (UINT)i);
//	}
//
//	spark_.Draw(renderer, camera, cmd);
//}
//
//} // namespace Engine
