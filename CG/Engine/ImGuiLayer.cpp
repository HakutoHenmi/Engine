#include "ImGuiLayer.h"
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

namespace Engine {

bool ImGuiLayer::Initialize(HWND hwnd, WindowDX& dx) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	// ImGuiのフォントSRVはSRVヒープの最後（index=3）を使用
	auto cpu = dx.SRV_CPU(dx.FontSrvIndex());
	auto gpu = dx.SRV_GPU(dx.FontSrvIndex());
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(dx.Dev(), 2, DXGI_FORMAT_R8G8B8A8_UNORM, dx.SRV(), cpu, gpu);
	return true;
}

void ImGuiLayer::NewFrame() {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiLayer::Render(WindowDX& dx) {
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx.List());
}

void ImGuiLayer::Shutdown() {
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

} // namespace Engine
