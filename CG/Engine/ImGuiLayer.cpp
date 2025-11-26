#include "ImGuiLayer.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

namespace Engine {

bool ImGuiLayer::Initialize(HWND hwnd, WindowDX& dx, float jpFontSize, const char* jpFontPath) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// === ImGui 専用の SRV ヒープを作る（フォント用） ===
	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1; // フォントテクスチャ用に1個だけ
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;
	if (FAILED(dx.Dev()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&imguiHeap_)))) {
		return false;
	}

	// フォント（日本語＋既定）
	ImGuiIO& io = ImGui::GetIO();
	// 既定フォントはImGui内部のデフォルトを使いつつ…
	fontDefault_ = io.FontDefault;
	// 日本語フォントを追加
	fontJP_ = io.Fonts->AddFontFromFileTTF(jpFontPath, jpFontSize, nullptr, io.Fonts->GetGlyphRangesJapanese());
	if (fontJP_) {
		io.FontDefault = fontJP_; // 既定を日本語フォントに
	}

	auto cpu = imguiHeap_->GetCPUDescriptorHandleForHeapStart();
	auto gpu = imguiHeap_->GetGPUDescriptorHandleForHeapStart();

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(
	    dx.Dev(),
	    2, // フレーム数（バックバッファ数）
	    DXGI_FORMAT_R8G8B8A8_UNORM,
	    imguiHeap_, // ★ ImGui専用ヒープ
	    cpu, gpu);

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

	if (imguiHeap_) { // ★ ヒープ解放
		imguiHeap_->Release();
		imguiHeap_ = nullptr;
	}
}

} // namespace Engine
