#include "App.h"
#include <Windows.h>
#include <chrono>
#include <dinput.h>

using namespace std::chrono;

namespace Engine {

bool App::Initialize(HINSTANCE hInst, int cmdShow) {
	// 1) Window + DX
	sceneManager_.SetDX(&dx_);
	if (!dx_.Initialize(hInst, cmdShow, hwnd_))
		return false;

	// 2) Input / Camera / Audio
	input_.Initialize(hInst, hwnd_);
	camera_.Initialize();
	if (!audio_.Initialize())
		return false; // ここではWAVは読まない（ゲーム側で）

	// 3) Renderer & ImGui
	if (!renderer_.Initialize(dx_))
		return false;
	if (!imgui_.Initialize(hwnd_, dx_))
		return false;

	// 4) --- ここが重要：ゲーム側の登録フックを呼び出す ---
	if (registrar_) {
		registrar_(sceneManager_, dx_);
	}
	// 5) 初期シーンへ
	if (!initialSceneKey_.empty()) {
		sceneManager_.Change(initialSceneKey_);
	}

	prev_ = steady_clock::now();
	return true;
}

void App::BeginFrame_() {
	dx_.BeginFrame();
	auto* cmd = dx_.List();
	renderer_.BeginFrame(cmd);
	imgui_.NewFrame(); // ゲーム側で ImGui を描くならここで開始
}

void App::EndFrame_() {
	// ゲーム側の ImGui 描画結果を反映
	imgui_.Render(dx_);
	auto* cmd = dx_.List();
	renderer_.EndFrame(cmd);
	dx_.EndFrame();
}

void App::Run() {
	MSG msg = {};
	bool running = true;

	auto prev = steady_clock::now();

	while (running) {
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				running = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (!running)
			break;

		// dt 計算（必要ならシーン側で取得）
		auto now = steady_clock::now();
		float dt = duration<float>(now - prev).count();
		(void)dt;
		prev = now;

		// 入力アップデート（必要なら Scene 側で参照）
		input_.Update();

		// === 1フレーム ===
		BeginFrame_();

		// --- シーン更新・描画（ゲーム管理は SceneManager に一任） ---
		sceneManager_.Update();
		sceneManager_.Draw();

		EndFrame_();
	}
}

void App::Shutdown() {
	// === GPU 完了待ち ===
	if (dx_.Queue()) {
		if (!fence_) {
			dx_.Dev()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
			fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		}
		fenceValue_++;
		dx_.Queue()->Signal(fence_.Get(), fenceValue_);
		if (fence_->GetCompletedValue() < fenceValue_) {
			fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
			WaitForSingleObject(fenceEvent_, INFINITE);
		}
	}

	// 解放順（GPU→描画→音→入力→Window）
	imgui_.Shutdown();
	renderer_.Shutdown();
	audio_.Shutdown();
	input_.Shutdown();
	dx_.Shutdown();

	fence_.Reset();
	if (fenceEvent_) {
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
		const DXGI_DEBUG_RLO_FLAGS flags = static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL);
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, flags);
	}
#endif
}

} // namespace Engine
