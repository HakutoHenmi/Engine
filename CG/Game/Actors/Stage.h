#pragma once
#include "AABB.h"
#include "Camera.h"
#include "Matrix4x4.h"
#include "Renderer.h"
#include "Transform.h"
#include <string>
#include <vector>

namespace Engine {

// グリッドの起点（CSVの(0,0)をどこに置くか）
enum class GridAnchor {
	Center, // ステージ全体の中心を(0,0)に
	TopLeft // 左上(0,0)から+X右、+Z下へ
};

// モデルの原点（OBJの原点がどこか）
enum class ModelOrigin {
	Center,   // モデル中心が(0,0,0)
	MinCorner // 最小コーナーが(0,0,0)
};

class Stage {
public:
	void Initialize(
	    Renderer& renderer, WindowDX& dx, const Camera& camera, const std::string& mapCsvPath, const std::string& angleCsvPath, float tileW, float tileD, float tileH,
	    GridAnchor anchor = GridAnchor::Center, ModelOrigin modelOrigin = ModelOrigin::Center, float gapX = 0.02f, float gapZ = 0.02f);

	void Update();
	void Draw(Renderer& renderer, ID3D12GraphicsCommandList* cmd);

	void SetCamera(const Camera* cam) { camera_ = cam; }

	// 衝突などで利用
	const std::vector<AABB>& GetWalls() const { return wallAABBs_; }
	const std::vector<AABB>& GetPrismWalls() const { return prismAABBs_; }
	const std::vector<float>& GetPrismAngles() const { return prismAngles_; }
	std::vector<AABB> GetWallsDynamic() const;

	// 昇降ブロックのトグル
	void TriggerLiftBlocks();

	// グリッド描画合わせ用
	int Cols() const { return maxCols_; } // 列数
	int Rows() const { return rows_; }    // 行数
	float PitchX() const { return tileWidth_ + gapX_; }
	float PitchZ() const { return tileDepth_ + gapZ_; }

	// 毎フレーム、プレイヤー位置と半径を渡す
	void SetPlayerEffect(
	    const Vector3& pos, float radius,
	    float riseLerp = 0.25f,  // 近づいた時の立ち上がり
	    float fallLerp = 0.06f); // 離れた時の減衰

private:
	Vector3 playerPos_{0, 0, 0};
	float glowRadius_ = 6.0f;
	float glowRise_ = 0.25f; // 0..1 1フレームの追従率
	float glowFall_ = 0.06f; // 0..1 1フレームの追従率

	struct Tile {
		Transform transform;
		int modelHandle = -1;
		bool isWall = false;
		bool isPrism = false;
		bool isLift = false;
		bool isGround = false;

		float prismAngle = 0.0f;

		AABB aabb{};
		float baseY = 0.0f;
		bool isUp = false;
		bool activeCollision = true;

		// ★ 追加：床タイルの発光強度（0..1）
		float glow = 0.0f;
	};

	static std::vector<std::vector<std::string>> LoadCsvRobust(const std::string& path);
	inline void gridToWorld(int gx, int gz, float& outX, float& outZ) const;

private:
	std::vector<Tile> tiles_;
	std::vector<AABB> wallAABBs_;
	std::vector<AABB> prismAABBs_;
	std::vector<float> prismAngles_;
	const Camera* camera_ = nullptr;

	float tileWidth_ = 1.0f;
	float tileDepth_ = 1.0f;
	float tileHeight_ = 1.0f;
	float gapX_ = 0.0f, gapZ_ = 0.0f;
	float pitchX_ = 1.0f, pitchZ_ = 1.0f;
	int rows_ = 0, maxCols_ = 0;

	GridAnchor anchor_ = GridAnchor::Center;
	ModelOrigin modelOrigin_ = ModelOrigin::Center;

	int wallModelHandle_ = -1;
	int prismModelHandle_ = -1;
	int sensorModelHandle_ = -1;
};

} // namespace Engine
