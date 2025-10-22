#pragma once
#include "Matrix4x4.h"
#include "Renderer.h"
#include <random>
#include <vector>

namespace Engine {

struct Particle {
	Vector3 pos;
	Vector3 vel;
	float life;
	float size;
	Vector4 color;
	int type = 0; // 0=火花, 1=リング（新演出）
	Vector3 rot;  // ★ 追加：回転角（リング用）
};


class ParticleSystem {
public:

	 enum class ScatterMode {
		Random,     // 球状
		Normal,     // 法線方向中心
		Reflect,    // 反射方向中心
		Directional // ★ 新規：指定方向中心
	};

	ParticleSystem() = default;
	~ParticleSystem() = default;

	// 初期化
	void Initialize(Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const std::string& sparkPath, const std::string& ringPath,  UINT baseSlot = 0); // ★ 追加
	// パーティクル生成
	void Spawn(const Vector3& position, const Vector3& normal, int count, const Vector4& color, float spreadFactor);

	void EnemySpawn(const Vector3& position, const Vector3& normal, int count, const Vector4& color, float spreadFactor);

	// 更新・描画
	void Update(float deltaTime);
	void Draw(Renderer& renderer, const Camera& camera, ID3D12GraphicsCommandList* cmd);

	void Clear() { particles_.clear(); }
	bool Empty() const { return particles_.empty(); }

	// リングを生成
	void SpawnRing(const Vector3& position, const Vector3& normal, const Vector4& color = {0.2f, 1.0f, 1.0f, 1});

	  void SetBaseSlot(UINT base) { baseSlot_ = base; } // ★ 任意変更用

	  bool IsInitialized() const { return sparkModelHandle_ >= 0 && ringModelHandle_ >= 0; }

	  
    void SetScatterMode(ScatterMode mode) { scatterMode_ = mode; }
	  void SetMainDirection(const Vector3& dir) { mainDir_ = Normalize(dir); } // ★ 新規
	  ScatterMode GetScatterMode() const { return scatterMode_; }

	  //--------------------------------
	  // ★ 拡散角設定関数
	  //--------------------------------
	  void SetSpreadAngle(float factor) { spreadAngleFactor_ = factor; }
	  float GetSpreadAngle() const { return spreadAngleFactor_; }

private:
	std::vector<Particle> particles_;

		int sparkModelHandle_ = -1; // 火花モデル
	int ringModelHandle_ = -1;  // リングモデル
	    UINT baseSlot_ = 0;         // ★ このインスタンスが使うスロット帯の開始番号

		   ScatterMode scatterMode_ = ScatterMode::Random;
	    Vector3 mainDir_ = {0, 1, 0}; // ★ Directionalモード用の基準方向

			float spreadAngleFactor_ = 1.0f; // ★ デフォルト拡散倍率

};

} // namespace Engine
