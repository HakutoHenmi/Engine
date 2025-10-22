// ===============================
// WindowDX.cpp  — Win32 + DX12 土台
// ===============================

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h> // ← 必ず最初期

// ── ImGui をグローバルスコープで取り込む（順序固定）
#include "imgui.h" // ★ 先にこれ（IMGUI_IMPL_API を定義）
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h" // ★ WndProcHandler の宣言

// まれに CW_USE_DEFAULT が未定義な環境があるため保険
#ifndef CW_USE_DEFAULT
#define CW_USE_DEFAULT ((int)0x80000000)
#endif

#ifndef IMGUI_IMPL_WIN32_H
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#endif
// ─────────────────────────────────────────────────────

#include "WindowDX.h"
#include <cassert>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace {

// 深度テクスチャ（D32）
static ID3D12Resource* CreateDepth(ID3D12Device* dev, UINT w, UINT h) {
	CD3DX12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, w, h, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	D3D12_CLEAR_VALUE cv{};
	cv.Format = DXGI_FORMAT_D32_FLOAT;
	cv.DepthStencil.Depth = 1.0f;
	CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
	ID3D12Resource* res{};
	HRESULT hr = dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&res));
	//assert(SUCCEEDED(hr));

	   if (FAILED(hr)) {
		char buf[256];
		sprintf_s(buf, "CreateDepth failed! hr=0x%08X (w=%u, h=%u)\n", hr, w, h);
		OutputDebugStringA(buf);
		return nullptr;
	}

	return res;
}

} // namespace

namespace Engine {

// ------------------------ WndProc ------------------------
LRESULT CALLBACK WindowDX::WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
	// ImGui へ先渡し（宣言は上で必ず可視化済み）
	if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) {
		return 0;
	}
	if (m == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(h, m, w, l);
}

// ------------------------ Initialize ------------------------
bool WindowDX::Initialize(HINSTANCE hInst, int cmdShow, HWND& outHwnd) {

	    hInst_ = hInst;

	// 1) Window
	const wchar_t* kClass = L"DX12AppClass";
	WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
	wc.lpfnWndProc = WindowDX::WndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = kClass;
	RegisterClassExW(&wc);

	RECT rc{0, 0, (LONG)kW, (LONG)kH};
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	hwnd_ = CreateWindowW(kClass, L"DirectX12 Sample", WS_OVERLAPPEDWINDOW, CW_USE_DEFAULT, CW_USE_DEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
	if (!hwnd_)
		return false;
	ShowWindow(hwnd_, cmdShow);
	outHwnd = hwnd_;

	// 2) Device/Queue/Allocator/List
#ifdef _DEBUG
	ComPtr<ID3D12Debug> dbg;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
		dbg->EnableDebugLayer();
#endif
	ComPtr<IDXGIFactory7> fac;
	UINT flags = 0;
#ifdef _DEBUG
	flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&fac));
	assert(SUCCEEDED(hr));

	ComPtr<IDXGIAdapter4> adp;
	for (UINT i = 0; fac->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adp)) != DXGI_ERROR_NOT_FOUND; ++i) {
		if (SUCCEEDED(D3D12CreateDevice(adp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev_))))
			break;
		adp.Reset();
	}
	if (!dev_) {
		hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev_));
		assert(SUCCEEDED(hr));
	}

	D3D12_COMMAND_QUEUE_DESC qd{};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = dev_->CreateCommandQueue(&qd, IID_PPV_ARGS(&que_));
	assert(SUCCEEDED(hr));
	hr = dev_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc_));
	assert(SUCCEEDED(hr));
	hr = dev_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc_.Get(), nullptr, IID_PPV_ARGS(&list_));
	assert(SUCCEEDED(hr));
	list_->Close();

	// 3) SwapChain
	DXGI_SWAP_CHAIN_DESC1 sd{};
	sd.Width = kW;
	sd.Height = kH;
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = kBB;
	sd.SampleDesc.Count = 1;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ComPtr<IDXGISwapChain1> sc1;
	hr = fac->CreateSwapChainForHwnd(que_.Get(), hwnd_, &sd, nullptr, nullptr, &sc1);
	assert(SUCCEEDED(hr));
	hr = sc1.As(&swap_);
	assert(SUCCEEDED(hr));
	fi_ = swap_->GetCurrentBackBufferIndex();

	// 4) Heaps
	rtvInc_ = dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dsvInc_ = dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	srvInc_ = dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// RTV
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		hd.NumDescriptors = kBB;
		hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = dev_->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvH_));
		assert(SUCCEEDED(hr));
		for (UINT i = 0; i < kBB; ++i) {
			hr = swap_->GetBuffer(i, IID_PPV_ARGS(&back_[i]));
			assert(SUCCEEDED(hr));
			auto h = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvH_->GetCPUDescriptorHandleForHeapStart(), i, rtvInc_);
			dev_->CreateRenderTargetView(back_[i].Get(), nullptr, h);
		}
	}
	// DSV
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		hd.NumDescriptors = 1;
		hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = dev_->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&dsvH_));
		assert(SUCCEEDED(hr));
		depth_ = CreateDepth(dev_.Get(), kW, kH);
		dev_->CreateDepthStencilView(depth_.Get(), nullptr, dsvH_->GetCPUDescriptorHandleForHeapStart());
	}
	// SRV（Model / Sprite0 / Sprite1 / ImGui の想定で4枠）
	{
		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		hd.NumDescriptors = 4;
		hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		hr = dev_->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&srvH_));
		assert(SUCCEEDED(hr));
	}

	// 5) Fence / Viewport / Scissor
	hr = dev_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr));
	fev_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	vp_ = {0.0f, 0.0f, (float)kW, (float)kH, 0.0f, 1.0f};
	sc_ = {0, 0, (LONG)kW, (LONG)kH};
	return true;
}

// ------------------------ Shutdown ------------------------
void WindowDX::Shutdown() {
	if (fev_) {
		CloseHandle(fev_);
		fev_ = nullptr;
	}
	depth_.Reset();
	for (auto& b : back_)
		b.Reset();
	srvH_.Reset();
	dsvH_.Reset();
	rtvH_.Reset();
	list_.Reset();
	alloc_.Reset();
	que_.Reset();
	swap_.Reset();
	fence_.Reset();
	dev_.Reset();
}

// ------------------------ BeginFrame ------------------------
void WindowDX::BeginFrame() {
	fi_ = swap_->GetCurrentBackBufferIndex();
	alloc_->Reset();
	list_->Reset(alloc_.Get(), nullptr);

	auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(back_[fi_].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	list_->ResourceBarrier(1, &toRT);

	auto rtv = RTV_CPU(fi_), dsv = DSV_CPU();
	list_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	const float clear[4] = {0.25f, 0.45f, 0.60f, 1.0f};
	list_->ClearRenderTargetView(rtv, clear, 0, nullptr);
	list_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	list_->RSSetViewports(1, &vp_);
	list_->RSSetScissorRects(1, &sc_);

	ID3D12DescriptorHeap* heaps[] = {srvH_.Get()};
	list_->SetDescriptorHeaps(1, heaps);
}

// ------------------------ EndFrame ------------------------
void WindowDX::EndFrame() {
	auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(back_[fi_].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	list_->ResourceBarrier(1, &toPresent);

	list_->Close();
	ID3D12CommandList* lists[] = {list_.Get()};
	que_->ExecuteCommandLists(1, lists);
	swap_->Present(1, 0);
	WaitGPU();
}

// ------------------------ WaitGPU ------------------------
void WindowDX::WaitGPU() {
	que_->Signal(fence_.Get(), ++fv_);
	if (fence_->GetCompletedValue() < fv_) {
		fence_->SetEventOnCompletion(fv_, fev_);
		WaitForSingleObject(fev_, INFINITE);
	}
}

// ------------------------ Handle utils ------------------------
D3D12_CPU_DESCRIPTOR_HANDLE WindowDX::SRV_CPU(int offset) const {
	auto h = srvH_->GetCPUDescriptorHandleForHeapStart();
	h.ptr += static_cast<SIZE_T>(offset) * srvInc_;
	return h;
}
D3D12_GPU_DESCRIPTOR_HANDLE WindowDX::SRV_GPU(int offset) const {
	auto h = srvH_->GetGPUDescriptorHandleForHeapStart();
	h.ptr += static_cast<UINT64>(offset) * srvInc_;
	return h;
}
D3D12_CPU_DESCRIPTOR_HANDLE WindowDX::RTV_CPU(int idx) const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvH_->GetCPUDescriptorHandleForHeapStart(), idx, rtvInc_); }
D3D12_CPU_DESCRIPTOR_HANDLE WindowDX::DSV_CPU() const { return dsvH_->GetCPUDescriptorHandleForHeapStart(); }

} // namespace Engine
