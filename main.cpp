// main.cpp — エントリのみ
#include "Engine/App.h"
#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int cmdShow) {
	Engine::App app;
	if (!app.Initialize(hInst, cmdShow))
		return -1;
	app.Run();
	app.Shutdown();
	return 0;
}
