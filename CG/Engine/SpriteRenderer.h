#pragma once
#include <DirectXMath.h>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

#include "TextureManager.h"
#include "WindowDX.h"

namespace Engine {

class SpriteRenderer {
public:
	struct Sprite {
		TextureHandle tex;                   // 使用するテクスチャ
		DirectX::XMFLOAT2 pos{0.0f, 0.0f};   // 左上座標（画面ピクセル）
		DirectX::XMFLOAT2 size{100, 100};    // サイズ（画面ピクセル）
		DirectX::XMFLOAT4 color{1, 1, 1, 1}; // 乗算カラー

		// 切り取り範囲（UV）
		// (u0, v0) = 左上, (u1, v1) = 右下, 0～1
		DirectX::XMFLOAT4 srcUV{0, 0, 1, 1};
	};

public:
	SpriteRenderer() = default;
	~SpriteRenderer() = default;

	static SpriteRenderer* Instance();

	bool Initialize(WindowDX* dx);
	void Shutdown();

	// 1枚だけ即時描画
	void DrawSprite(const Sprite& sprite);

	// バッチ描画したい場合
	void Begin();
	void Draw(const Sprite& sprite); // Begin〜Endの間で複数回呼ぶ
	void End();

private:
	struct Vertex {
		DirectX::XMFLOAT4 pos; // ローカル(0,0)-(1,1)
		DirectX::XMFLOAT2 uv;
	};

	struct CBCb {
		DirectX::XMFLOAT4X4 mvp; // 変換
		DirectX::XMFLOAT4 color; // 乗算カラー
		DirectX::XMFLOAT4 srcUV; // (u0,v0,u1,v1)
	};

	bool CreateResources_();
	void DrawInternal_(const Sprite& sprite);

private:
	WindowDX* dx_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
	Microsoft::WRL::ComPtr<ID3D12Resource> ib_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cb_;

	D3D12_VERTEX_BUFFER_VIEW vbv_{};
	D3D12_INDEX_BUFFER_VIEW ibv_{};

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rs_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

	bool batching_ = false;

	static SpriteRenderer* sInstance_;
};

} // namespace Engine
