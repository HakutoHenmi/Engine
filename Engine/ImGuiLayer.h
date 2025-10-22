#pragma once
// ===============================
//  ImGuiLayer : ImGui初期化/描画
// ===============================
#include "WindowDX.h"
#include <Windows.h>

namespace Engine {

class ImGuiLayer {
public:
	bool Initialize(HWND hwnd, WindowDX& dx);
	void NewFrame();
	void Render(WindowDX& dx);
	void Shutdown();
};

} // namespace Engine
