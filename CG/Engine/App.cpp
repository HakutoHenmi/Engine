#include "App.h"
#include "SpriteRenderer.h"
#include "TextureManager.h"
#include <Windows.h>
#include <chrono>
#include <dinput.h>
#include <mmsystem.h>
#include <thread>
#pragma comment(lib, "winmm.lib")

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

	// ===========================
	//  TextureManager 初期化
	// ===========================
	Engine::TextureManager::Instance().Initialize(&dx_, &renderer_);

	// ===========================
	//  SpriteRenderer 初期化
	// ===========================
	spriteRenderer_ = std::make_unique<Engine::SpriteRenderer>();
	spriteRenderer_->Initialize(&dx_);

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

	// ★ スリープ精度を 1ms に上げる
	timeBeginPeriod(1);

	using clock = std::chrono::steady_clock;
	using micro = std::chrono::microseconds;

	const micro kMinTime(16667);  // 1/60 秒 ≒ 16.667ms
	const micro kMinCheck(15000); // 15ms を超えたらビジーウェイトに切り替える

	auto reference = clock::now();

	while (running) {
		// --- メッセージ処理 ---
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

		// ==== 60FPS 固定処理 ====
		auto now = clock::now();
		auto elapsed = std::chrono::duration_cast<micro>(now - reference);

		if (elapsed < kMinTime) {
			// まずは 1ms スリープで 1/60秒手前まで近づく
			while (elapsed < kMinCheck) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				now = clock::now();
				elapsed = std::chrono::duration_cast<micro>(now - reference);
			}

			// 残りわずかはビジーウェイトで微調整
			while (elapsed < kMinTime) {
				now = clock::now();
				elapsed = std::chrono::duration_cast<micro>(now - reference);
			}
		}

		// 実際にかかったフレーム時間から dt を計算（必要なら使う）
		float dt = std::chrono::duration<float>(now - reference).count();
		(void)dt;
		reference = now;
		// =========================

		// 入力更新
		input_.Update();

		// 1フレーム分の処理
		BeginFrame_();
		sceneManager_.Update();
		sceneManager_.Draw();
		EndFrame_();
	}

	// ★ 元に戻す
	timeEndPeriod(1);
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
	dx_.WaitIdle();
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
