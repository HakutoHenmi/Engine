// TerrainManager.cpp
#include "TerrainManager.h"
#include <algorithm> // std::clamp
#include <cmath>     // std::round

namespace Engine {

using DirectX::XMFLOAT2;
using DirectX::XMFLOAT3;

void TerrainManager::Initialize(Renderer* renderer, const Desc& desc) {
	renderer_ = renderer;
	desc_ = desc;

	centerChunk_ = DirectX::XMINT2{0, 0};

	if (renderer_) {
		// ★ ここで 3×3 チャンクを指定（Desc に合わせる）
		renderer_->SetVoxelChunkCount(static_cast<int>(desc_.chunkCountX), static_cast<int>(desc_.chunkCountZ));

		DirectX::XMFLOAT2 origin = desc_.worldCenter;
		renderer_->SetVoxelWorldOrigin(origin);
	}
}

void TerrainManager::Update(const XMFLOAT3& playerPos) {
	if (!renderer_) {
		return;
	}

	// ===== プレイヤーの位置を「ワールド中心からのオフセット」で見る =====
	float localX = playerPos.x - desc_.worldCenter.x;
	float localZ = playerPos.z - desc_.worldCenter.y;

	// 1チャンクのサイズで割って「何チャンク分離れているか」を算出
	float fx = localX / desc_.chunkSizeX;
	float fz = localZ / desc_.chunkSizeZ;

	// 一番近いチャンクインデックスに丸める（… -1, 0, +1, ...）
	int ix = static_cast<int>(std::round(fx));
	int iz = static_cast<int>(std::round(fz));

	// 用意しているチャンク数の範囲にクランプ
	int halfX = static_cast<int>(desc_.chunkCountX) / 2; // 例：3 → ±1
	int halfZ = static_cast<int>(desc_.chunkCountZ) / 2;

	ix = std::clamp(ix, -halfX, halfX);
	iz = std::clamp(iz, -halfZ, halfZ);

	// 変化がなければ何もしない（再生成しない）
	if (ix == centerChunk_.x && iz == centerChunk_.y) {
		return;
	}

	// 中心チャンクを更新
	centerChunk_.x = ix;
	centerChunk_.y = iz;

	// ===== このチャンクの「中心ワールド座標」を計算して Renderer に渡す =====
	XMFLOAT2 origin;
	origin.x = desc_.worldCenter.x + static_cast<float>(ix) * desc_.chunkSizeX;
	origin.y = desc_.worldCenter.y + static_cast<float>(iz) * desc_.chunkSizeZ;

	renderer_->SetVoxelWorldOrigin(origin);
}

// HeightAt / NormalAt は前回のままでOK
float TerrainManager::HeightAt(float x, float z) const {
	if (!renderer_) {
		return 0.0f;
	}
	return renderer_->TerrainHeightAt(x, z);
}

Vector3 TerrainManager::NormalAt(float x, float z) const {
	if (!renderer_) {
		return Vector3{0.0f, 1.0f, 0.0f};
	}
	return renderer_->TerrainNormalAt(x, z);
}

} // namespace Engine
