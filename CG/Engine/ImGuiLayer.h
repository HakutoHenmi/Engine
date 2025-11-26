#pragma once
// ===============================
//  ImGuiLayer : ImGui初期化/描画
// ===============================
#include "WindowDX.h"
#include <Windows.h>
#include "imgui.h"

// DirectX12 の前方宣言（ポインタだけ使うのでこれでOK）
struct ID3D12DescriptorHeap;

namespace Engine {

class ImGuiLayer {
public:
	bool Initialize(HWND hwnd, WindowDX& dx, float jpFontSize = 15.0f, const char* jpFontPath = "Resources/fonts/Huninn/Huninn-Regular.ttf");
	void NewFrame();
	void Render(WindowDX& dx);
	void Shutdown();

	ImFont* GetDefaultFont() const { return fontDefault_; }
	ImFont* GetJPFont() const { return fontJP_; }
private:
	ID3D12DescriptorHeap* imguiHeap_ = nullptr; // ★ ImGui専用SRVヒープ
	ImFont* fontDefault_ = nullptr;
	ImFont* fontJP_ = nullptr;
};

} // namespace Engine
