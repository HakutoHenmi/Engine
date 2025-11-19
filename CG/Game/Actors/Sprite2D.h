#pragma once
#include <DirectXMath.h>
#include <string>

#include "../Engine/SpriteRenderer.h"
#include "../Engine/TextureManager.h"

namespace Anchor {

class Sprite2D {
public:
	void Initialize(Engine::SpriteRenderer* renderer, const std::wstring& textureRelPath) {
		renderer_ = renderer;
		tex_ = Engine::TextureManager::Instance().Load(textureRelPath);
	}

	void SetPosition(float x, float y) { pos_ = {x, y}; }
	void SetSize(float w, float h) { size_ = {w, h}; }

	// 色 (RGBA)
	void SetColor(float r, float g, float b, float a = 1.0f) { color_ = {r, g, b, a}; }

	// 切り取り範囲（UV）
	//   u0,v0: 左上, u1,v1: 右下  (0～1)
	void SetSrcUV(float u0, float v0, float u1, float v1) { srcUV_ = {u0, v0, u1, v1}; }

	// 実際に描画
	void Draw() {
		if (!renderer_)
			return;
		Engine::SpriteRenderer::Sprite s;
		s.tex = tex_;
		s.pos = pos_;
		s.size = size_;
		s.color = color_;
		s.srcUV = srcUV_;
		renderer_->DrawSprite(s);
	}

private:
	Engine::SpriteRenderer* renderer_ = nullptr;
	Engine::TextureHandle tex_{};

	DirectX::XMFLOAT2 pos_{0, 0};
	DirectX::XMFLOAT2 size_{100, 100};
	DirectX::XMFLOAT4 color_{1, 1, 1, 1};
	DirectX::XMFLOAT4 srcUV_{0, 0, 1, 1};
};

} // namespace Anchor
