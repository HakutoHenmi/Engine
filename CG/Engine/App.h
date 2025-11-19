#pragma once
#include "Audio.h"
#include "Camera.h"
#include "ImGuiLayer.h"
#include "Input.h"
#include "Matrix4x4.h"
#include "Renderer.h"
#include "SceneManager.h"
#include "SpriteRenderer.h"
#include "TextureManager.h"
#include "WindowDX.h"
#include <Windows.h>
#include <chrono>
#include <dxgidebug.h>
#include <functional>
#include <string>
#include <wrl.h>
#pragma comment(lib, "dxguid.lib")

namespace Engine {

class App {
public:
	bool Initialize(HINSTANCE hInst, int cmdShow);
	void Run();
	void Shutdown();

	// --- ゲーム側から渡す拡張ポイント ---
	using SceneRegistrar = std::function<void(SceneManager& sm, WindowDX& dx)>;
	void SetSceneRegistrar(SceneRegistrar f) { registrar_ = std::move(f); }

	void SetInitialSceneKey(const std::string& key) { initialSceneKey_ = key; }

private:
	void BeginFrame_();
	void EndFrame_();

private:
	SceneManager sceneManager_;
	std::unique_ptr<SpriteRenderer> spriteRenderer_;
	HWND hwnd_ = nullptr;
	WindowDX dx_;
	Input input_;
	Camera camera_;
	Audio audio_;
	Renderer renderer_;
	ImGuiLayer imgui_;

	std::chrono::steady_clock::time_point prev_{};

	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	UINT64 fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;

	// --- 差し込み用 ---
	SceneRegistrar registrar_{};
	std::string initialSceneKey_{};
};

} // namespace Engine
