// TerrainManager.h
#pragma once
#include "Renderer.h"
#include <DirectXMath.h>

namespace Engine {

class TerrainManager {
public:
	struct Desc {
		UINT chunkX = 64;
		UINT chunkZ = 64;
		UINT chunkCountX = 1;
		UINT chunkCountZ = 1;
		float cell = 1.0f;
		float chunkSizeX = 64.0f;
		float chunkSizeZ = 64.0f;
		DirectX::XMFLOAT2 worldCenter{0.0f, 0.0f};
	};

public:
	void Initialize(Renderer* renderer, const Desc& desc);
	void Update(const DirectX::XMFLOAT3& playerPos);
	float HeightAt(float x, float z) const;
	Vector3 NormalAt(float x, float z) const;

	const Desc& GetDesc() const { return desc_; }

private:
	Renderer* renderer_ = nullptr;
	Desc desc_{};

	// いま中心にしているチャンク座標（チャンク単位のインデックス）
	DirectX::XMINT2 centerChunk_{0, 0};
};

} // namespace Engine
