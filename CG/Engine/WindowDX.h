#pragma once
// =========================================
//  WindowDX : Win32 + DX12土台
//  ・デバイス/スワップチェーン/ヒープ/バックバッファ
//  ・CommandListのBegin/End/Present
// =========================================
#include <Windows.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <wrl.h>

namespace Engine {

class WindowDX {
public:
	bool Initialize(HINSTANCE hInst, int cmdShow, HWND& outHwnd);
	void Shutdown();

	void BeginFrame();
	void EndFrame();

	// 取得系
	ID3D12Device* Dev() const { return dev_.Get(); }
	ID3D12GraphicsCommandList* List() const { return list_.Get(); }
	ID3D12CommandQueue* Queue() const { return que_.Get(); }
	ID3D12DescriptorHeap* SRV() const { return srvH_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE SRV_CPU(int offset) const;
	D3D12_GPU_DESCRIPTOR_HANDLE SRV_GPU(int offset) const;
	D3D12_CPU_DESCRIPTOR_HANDLE RTV_CPU(int idx) const;
	D3D12_CPU_DESCRIPTOR_HANDLE DSV_CPU() const;

	UINT SrvInc() const { return srvInc_; }
	UINT RtvInc() const { return rtvInc_; }
	UINT DsvInc() const { return dsvInc_; }
	UINT FrameIndex() const { return fi_; }

	HINSTANCE GetHInstance() const { return hInst_; }
	HWND GetHwnd() const { return hwnd_; }

	// ImGuiフォントSRVの確保位置
	int FontSrvIndex() const { return 3; } // [0]=Model, [1]=Sprite0, [2]=Sprite1, [3]=ImGui

	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const {
		D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvH_->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += fi_ * rtvInc_; // 現在のバックバッファの RTV 位置
		return handle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return dsvH_->GetCPUDescriptorHandleForHeapStart(); }

	void WaitIdle();

private:
	static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
	void WaitGPU();

private:
	// 定数はオリジナルを踏襲
	static constexpr UINT kW = 1280, kH = 720, kBB = 2;

	// DX主要オブジェクト
	Microsoft::WRL::ComPtr<ID3D12Device> dev_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> que_;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swap_;

	Microsoft::WRL::ComPtr<ID3D12Resource> back_[kBB], depth_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvH_, dsvH_, srvH_;
	UINT rtvInc_{}, dsvInc_{}, srvInc_{};
	UINT fi_{};

	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	UINT64 fv_ = 0;
	HANDLE fev_ = nullptr;

	D3D12_VIEWPORT vp_{};
	D3D12_RECT sc_{};
	HINSTANCE hInst_ = nullptr;
	HWND hwnd_ = nullptr;
};

} // namespace Engine
