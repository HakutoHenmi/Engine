#include "Input.h"
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#include <cassert>
namespace Engine {

void Input::Initialize(HINSTANCE hInst, HWND hwnd) {
	HRESULT hr = DirectInput8Create(hInst, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)di_.GetAddressOf(), nullptr);
	assert(SUCCEEDED(hr) && "DirectInput8Create failed");

	hr = di_->CreateDevice(GUID_SysKeyboard, kb_.GetAddressOf(), nullptr);
	assert(SUCCEEDED(hr) && "CreateDevice failed");

	hr = kb_->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr) && "SetDataFormat failed");

	hr = kb_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr) && "SetCooperativeLevel failed");

	kb_->Acquire();

	ZeroMemory(state_, 256);
	ZeroMemory(prev_, 256);
}

void Input::Update() {
	memcpy(prev_, state_, 256);
	if (FAILED(kb_->GetDeviceState(256, state_))) {
		kb_->Acquire();
		kb_->GetDeviceState(256, state_);
	}
}

void Input::Shutdown() {
	if (kb_)
		kb_->Unacquire();
	kb_.Reset();
	di_.Reset();
}

} // namespace Engine
