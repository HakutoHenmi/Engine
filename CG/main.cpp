// main.cpp — エンジンは土台だけ。ゲーム管理（シーン登録/初期シーン）はここで行う。
#include "Engine/App.h"
#include "GameScene.h"
#include "ResultScene.h"
#include "TitleScene.h"
#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int cmdShow) {
	Engine::App app;

	// --- ゲーム固有：シーン登録を差し込む ---
	app.SetSceneRegistrar([](Engine::SceneManager& sm, Engine::WindowDX& dx) {
		sm.Register("Title", [] { return std::make_unique<Engine::TitleScene>(); });
		sm.Register("Game", [&dx] {
			auto s = std::make_unique<Engine::GameScene>();
			s->Initialize(&dx); // ★ DX を安全に受け渡し
			return s;
		});
		sm.Register("Result", [] { return std::make_unique<Engine::ResultScene>(); });
	});

	// --- 起動時のシーン ---
	app.SetInitialSceneKey("Title");

	if (!app.Initialize(hInst, cmdShow))
		return -1;
	app.Run();
	app.Shutdown();
	return 0;
}
