//// ================================
//// Laser.h
//// ================================
//#pragma once
//#include "AABB.h"
//#include "Camera.h"
//#include "Collision.h"
//#include "Matrix4x4.h" // Vector3, Transform
//#include "ParticleSystem.h"
//#include "Renderer.h"
//#include <DirectXMath.h>
//#include <vector>
//
//namespace Engine {
//
///// 単一レーザー（反射対応）
//class Laser {
//public:
//
//	enum class LaserState { Idle, Growing, FullLength, Retracting, Inactive };
//
//
//	Laser();
//	~Laser() = default;
//
//	    // 所要時間を指定して縮退開始（durationSec <= 0 なら既定値を使用）
//	void StartRetract(float durationSec = -1.0f);
//	bool IsRetracting() const { return retracting_; }
//
//	// 発射
//	void Shoot(const Vector3& startPos, const Vector3& dir);
//
//	// 更新（通常壁 + プリズム壁対応）
//	//  - walls       : 通常の反射対象AABB
//	//  - prismWalls  : プリズム面AABB
//	//  - prismAngles : 対応するプリズム面の出射角(度)
//	//  - deltaTime   : 経過時間
//	void Update(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, float deltaTime);
//
//	// 予測線（未発射でも経路を可視化）
//	void BuildPrediction(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, float /*step*/, int maxBounces, std::vector<Transform>& out) const;
//
//	// 描画（Renderer が各 Transform をスロット描画できる前提）
//	void Draw(ID3D12GraphicsCommandList* cmd, Renderer& renderer, int modelHandleNormal, int modelHandleEnhanced, const Camera& camera);
//
//	// 状態
//	bool IsActive() const { return active_; }
//
//	// 描画用：線分の Transform 群（各セグメントの中点・長さ・向きが入っている）
//	const std::vector<Transform>& GetSegments() const { return segXforms_; }
//
//	// 判定用：線分の端点リスト（これを LaserManager が使って当たり判定する）
//	struct Segment {
//		Vector3 a, b;
//	};
//	const std::vector<Segment>& GetSegmentsEndpoints() const { return segEnds_; }
//
//	void SetEnhanced(bool e) {
//		if (bounceCount_ <= 2) {
//			enhanced_ = e;
//			lifeTime_ = 0.0f;     // 強化開始時にリセット
//			enhancedLife_ = 0.0f; // 経過時間リセット
//		}
//	}
//
//	bool IsEnhanced() const { return enhanced_; }
//
//	// 今伸びているか？
//	float GetCurrentLength() const { return currentLength_; }
//	float GetMaxLength() const { return maxLength_; }
//
//	// 伸び中判定（反射も含む）
//	bool IsGrowing() const { return active_ && !enhanced_ && currentLength_ < maxLength_; }
//
//	bool IsMaxLength() const { return isMaxLength; }
//
//	bool IsStopped() const { return stopped_; }
//
//	bool WasUpdatedThisFrame() const { return updatedThisFrame_; }
//	void ClearUpdateFlag() { updatedThisFrame_ = false; } // 毎フレーム最初に呼ぶ
//
//	void OnKillEnemy();
//
//	ParticleSystem spark_;
//
//	void ResetEnemyKill()  {  killsThisShot_ = 0; }
//
//	//スロット確保
//	   int GetUniqueID() const { return uniqueID_; }
//	   void SetUniqueID(int id) { uniqueID_ = id; }
//
//	   void SetEnhancedLifeLimit(float sec) { enhancedLifeLimit_ = sec; }
//	   float GetEnhancedLifeLimit() const { return enhancedLifeLimit_; }
//
//
//private:
//	// ---- 描画用 ----
//	std::vector<Transform> segments_; // 経路すべて
//
//	// セグメント表現
//	std::vector<Transform> segXforms_; // 描画用
//	std::vector<Segment> segEnds_;     // 判定用（端点）
//
//	// 反射ヘルパ
//	static Vector3 Reflect(const Vector3& dir, const Vector3& normal);
//
//	   static inline int s_nextID_ = 0;
//	int uniqueID_ = 0;
//
//	//レーザーの動き
//	LaserState state_ = LaserState::Idle;
//
//	void UpdateRetract(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, float dt);
//
//	void UpdateGrow(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, float dt);
//
//	void BuildLaserSegments(const std::vector<AABB>& walls, const std::vector<AABB>& prismWalls, const std::vector<float>& prismAngles, bool retractMode = false);
//
//private:
//	// 内部ステート
//	Vector3 startPos_{0, 0, 0};
//	Vector3 dir_{0, 0, 1}; // 正規化
//	bool active_ = false;
//
//	float currentLength_ = 0.0f; // 現在の進行距離
//	float maxLength_ = 1000.0f;  // 最大距離
//	float speed_ = 60.0f;        // 伸びる速度
//
//	float num = 1.031f;
//
//	bool enhanced_ = false; // 強化してるかどうか
//	float lifeTime_ = 0.0f; // 寿命
//	float enhancedLife_ = 0.0f; // 強化レーザー寿命用
//	float enhancedLifeLimit_ = 3.0f; // ← ここを追加
//	float enhancedRetractDuration_ = 0.8f; // 強化レーザーが消えるまでの秒数（例：0.8秒）
//
//
//	bool isMaxLength = false;
//
//	bool stopped_ = false;          // 伸びきって止まった状態かどうか
//	bool updatedThisFrame_ = false; // ← 追加：今フレーム Update が呼ばれたか
//
//	int killsThisShot_ = 0; // このレーザーで倒した敵数
//
//	int bounceCount_ = 0; // ← 反射回数を記録
//
//	int particleModelHandle_ = -1;
//
//	int effectSpawnedCount_ = 0; // 各反射ごとに1回だけエフェクトを出すためのカウンタ
//
//	// --- 反射瞬間検出 ---
//	Vector3 lastReflectPos_{};
//	bool justReflected_ = false;
//	std::vector<Vector3> reflectHistory_; // 各フレームで生成済みの反射点
//
//	   bool retracting_ = false;
//	float retractSpeed_ = 100.0f;          // 実際には開始時に再計算
//	float retractDurationDefault_ = 0.25f; // 既定の縮退所要時間(秒)
//	float retractDurationOnStop_ = 0.35f;  // 伸び切りで自動縮退する時の所要時間
//};
//
//} // namespace Engine
