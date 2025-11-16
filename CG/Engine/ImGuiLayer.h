#pragma once
// ===============================
//  ImGuiLayer : ImGui初期化/描画
// ===============================
#include "WindowDX.h"
#include <Windows.h>

// DirectX12 の前方宣言（ポインタだけ使うのでこれでOK）
struct ID3D12DescriptorHeap;

namespace Engine {

class ImGuiLayer {
public:
	bool Initialize(HWND hwnd, WindowDX& dx);
	void NewFrame();
	void Render(WindowDX& dx);
	void Shutdown();

private:
	ID3D12DescriptorHeap* imguiHeap_ = nullptr; // ★ ImGui専用SRVヒープ
};

} // namespace Engine
