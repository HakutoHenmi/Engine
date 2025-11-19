#pragma once
#include <DirectXTex.h>
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

#include "Renderer.h"
#include "WindowDX.h"

namespace Engine {

// テクスチャ1枚ぶんの情報
struct TextureHandle {
	int index = -1; // 内部ID（配列インデックス）
};

class TextureManager {
public:
	// シングルトン
	static TextureManager& Instance();

	// 初期化：DX土台と Renderer を渡す（SRVヒープを共用）
	void Initialize(WindowDX* dx, Renderer* renderer);

	// 終了
	void Shutdown();

	// テクスチャ読み込み（同じパスは使いまわし）
	//   relPath: exeと同じフォルダからの相対パス(L"Resources/sample.png" など)
	TextureHandle Load(const std::wstring& relPath);

	// GPUハンドル取得（スプライト描画用）
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPU(const TextureHandle& h) const;

	// CPUハンドルも必要なら
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPU(const TextureHandle& h) const;

	Renderer* GetRenderer() const { return renderer_; }

	bool IsValid(const TextureHandle& h) const { return (h.index >= 0 && h.index < static_cast<int>(textures_.size())); }

private:
	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	struct TexData {
		Microsoft::WRL::ComPtr<ID3D12Resource> texture;
		Microsoft::WRL::ComPtr<ID3D12Resource> upload; // アップロード用
		int srvIndex = -1;
		D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
	};

	WindowDX* dx_ = nullptr;
	Renderer* renderer_ = nullptr;

	std::unordered_map<std::wstring, int> pathToIndex_;
	std::vector<TexData> textures_;
};

} // namespace Engine
