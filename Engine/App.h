#pragma once
#include "Audio.h"
#include "Camera.h"
#include "ImGuiLayer.h"
#include "Input.h"
#include "Matrix4x4.h"
#include "Renderer.h"
#include "SceneManager.h"
#include "WindowDX.h"
#include <Windows.h>
#include <chrono>
#include <functional> // ★ 追加
#include <string>     // ★ 追加

#include <dxgidebug.h>
#include <wrl.h>
#pragma comment(lib, "dxguid.lib")

namespace Engine {

class App {
public:
	// 既存
	bool Initialize(HINSTANCE hInst, int cmdShow);
	void Run();
	void Shutdown();

	// ★ 追加：シーン登録フックを Game 側から差し込む
	//   - DX初期化"後"に呼ぶので WindowDX 参照を安全に渡せます
	using SceneRegistrar = std::function<void(SceneManager& sm, WindowDX& dx)>;
	void SetSceneRegistrar(SceneRegistrar f) { registrar_ = std::move(f); }

	// ★ 追加：起動時の初期シーンキー（未指定なら Change は呼ばない）
	void SetInitialSceneKey(const std::string& key) { initialSceneKey_ = key; }

private:
	void Update(float dt);
	void Draw();

private:
	SceneManager sceneManager_;
	HWND hwnd_ = nullptr;
	WindowDX dx_;
	Input input_;
	Camera camera_;
	Audio audio_;
	Renderer renderer_;
	ImGuiLayer imgui_;

	Renderer::SpriteTf sprTf_{};
	Renderer::SphereTf sphTf_{
	    {0, 0, 5}
    };
	Renderer::UVParam uv_{};
	Vector3 lightDir_{0, 1, -1};
	bool useBallTex_ = true;

	std::chrono::steady_clock::time_point prev_;

	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	UINT64 fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;

	// ★ 追加
	SceneRegistrar registrar_{};
	std::string initialSceneKey_{};
};

} // namespace Engine
