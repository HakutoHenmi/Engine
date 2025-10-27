#include "App.h"
#include "GameScene.h"
#include "Input.h"
#include "Renderer.h"
#include "ResultScene.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "imgui.h"
#include <DirectXMath.h>
#include <Windows.h>
#include <chrono>
#include <dinput.h>
using namespace DirectX;

namespace sc = std::chrono;

namespace Engine {

bool App::Initialize(HINSTANCE hInst, int cmdShow) {
	sceneManager_.SetDX(&dx_);

	// 1) Window + DX
	if (!dx_.Initialize(hInst, cmdShow, hwnd_))
		return false;

	// 2) Input / Camera / Audioo
	input_.Initialize(hInst, hwnd_);
	camera_.Initialize();
	if (!audio_.Initialize())
		return false;
	if (!audio_.LoadWav(L"Resources/fanfare.wav"))
		return false;

	// 3) Renderer & ImGui
	if (!renderer_.Initialize(dx_))
		return false;
	if (!imgui_.Initialize(hwnd_, dx_))
		return false;

	// 初期値
	sprTf_ = {};
	sphTf_ = Renderer::SphereTf{
	    {0, 0, 5}
    };
	lightDir_ = {0, 1, -1};
	useBallTex_ = true;

	sceneManager_.Register("Title", [] { return std::make_unique<Engine::TitleScene>(); });
	sceneManager_.Register("Game", [this] {
		auto s = std::make_unique<Engine::GameScene>();
		s->Initialize(&dx_); // ★ dx_ を渡す
		return s;
	});
	sceneManager_.Register("Result", [] { return std::make_unique<Engine::ResultScene>(); });
	sceneManager_.Change("Title"); // 最初はタイトル

	// ★ 修正：chrono を完全修飾
	prev_ = sc::steady_clock::now();
	return true;
}
void App::Run() {
	MSG msg = {};
	bool running = true;

	auto prev = sc::steady_clock::now();

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

		auto now = sc::steady_clock::now();
		float dt = sc::duration<float>(now - prev).count();
		prev = now;
		(void)dt;

		input_.Update();

		// === フレーム開始 ===
		dx_.BeginFrame();
		auto* cmd = dx_.List();

		renderer_.BeginFrame(cmd);

		// --- シーン更新・描画 ---
		sceneManager_.Update();
		sceneManager_.Draw();

		renderer_.EndFrame(cmd);
		dx_.EndFrame(); // ★ 必ず呼ぶ
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

	// === 各コンポーネントの解放 ===
	imgui_.Shutdown();
	renderer_.Shutdown(); // ← GPUリソース(Pipeline, Heap, Buffer, Texture)をResetしていること
	audio_.Shutdown();
	input_.Shutdown();

	// WindowDX の GPU 関連（スワップチェーン, RTV, DSV, コマンドキュー）解放
	dx_.Shutdown();

	// === Fence/Event の解放 ===
	fence_.Reset();
	if (fenceEvent_) {
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}

#ifdef _DEBUG
	// === Live Object レポート ===
	Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
		const DXGI_DEBUG_RLO_FLAGS flags = static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL);
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, flags);
	}
#endif
}

void App::Update(float /*dt*/) {
	input_.Update();
	camera_.Update(input_);

	if (input_.Trigger(DIK_SPACE)) {
		audio_.Play();
	}

	// ==== ImGui UI ====
	imgui_.NewFrame();
	ImGui::Begin("Control");
	ImGui::Checkbox("BallTex", &useBallTex_);
	if (ImGui::CollapsingHeader("Light")) {
		ImGui::DragFloat3("Dir", &lightDir_.x, 0.01f, -1, 1);
	}
	if (ImGui::CollapsingHeader("OBJ")) {
		ImGui::Text("plane.obj のみ表示");
	}
	if (ImGui::CollapsingHeader("Sprite")) {
		ImGui::DragFloat3("Pos", &sprTf_.t.x, 1.0f, -640, 1280);
		ImGui::DragFloat3("Scale", &sprTf_.s.x, 0.01f, 0.01f, 5.0f);
		ImGui::ColorEdit4("Col", &sprTf_.c.x);
		if (ImGui::TreeNode("UV")) {
			ImGui::DragFloat3("Trans", &uv_.trans.x, 0.01f, -10, 10);
			ImGui::DragFloat3("Scale", &uv_.scale.x, 0.01f, 0.01f, 10);
			ImGui::SliderAngle("Rot", &uv_.rot, -180, 180);
			ImGui::TreePop();
		}
		static const char* modes[] = {"Opaque", "Alpha", "Add", "Subtract", "Multiply"};
		int current = static_cast<int>(renderer_.GetBlendMode());
		if (ImGui::Combo("BlendMode", &current, modes, IM_ARRAYSIZE(modes))) {
			renderer_.SetBlendMode(static_cast<Renderer::BlendMode>(current));
		}
	}
	if (ImGui::CollapsingHeader("Sphere")) {
		ImGui::DragFloat3("Pos", &sphTf_.t.x, 0.01f);
		ImGui::DragFloat3("Rot", &sphTf_.r.x, 0.5f);
		ImGui::DragFloat3("Scale", &sphTf_.s.x, 0.01f, 0.1f, 5);
		ImGui::ColorEdit4("Col", &sphTf_.c.x);
	}
	auto cp = camera_.Position();
	ImGui::Text("Cam(%.2f, %.2f, %.2f)", cp.x, cp.y, cp.z);
	ImGui::End();
}

void App::Draw() {
	// 1) CB更新
	renderer_.UpdateCB(camera_, sprTf_, sphTf_, uv_, lightDir_);

	// 2) 描画コマンド記録
	dx_.BeginFrame();
	renderer_.Record(dx_, useBallTex_);

	// 3) ImGui描画
	imgui_.Render(dx_);

	// 4) 仕上げ
	dx_.EndFrame();
}

} // namespace Engine
