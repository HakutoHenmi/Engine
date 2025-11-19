#include "TextureManager.h"
#include <cassert>
#include <d3dx12.h>
#include <filesystem>

#pragma comment(lib, "DirectXTex.lib")

namespace Engine {

using Microsoft::WRL::ComPtr;

static std::wstring AssetFullPath(const std::wstring& rel) {
	wchar_t buf[MAX_PATH]{};
	::GetModuleFileNameW(nullptr, buf, MAX_PATH);
	std::filesystem::path exeDir = std::filesystem::path(buf).parent_path();
	return (exeDir / rel).wstring();
}

TextureManager& TextureManager::Instance() {
	static TextureManager inst;
	return inst;
}

void TextureManager::Initialize(WindowDX* dx, Renderer* renderer) {
	dx_ = dx;
	renderer_ = renderer;
	pathToIndex_.clear();
	textures_.clear();
}

void TextureManager::Shutdown() {
	textures_.clear();
	pathToIndex_.clear();
	dx_ = nullptr;
	renderer_ = nullptr;
}

TextureHandle TextureManager::Load(const std::wstring& relPath) {
	TextureHandle handle;

	if (!dx_ || !renderer_) {
		// 未初期化
		return handle;
	}

	// 既に読み込み済みならそれを返す
	auto it = pathToIndex_.find(relPath);
	if (it != pathToIndex_.end()) {
		handle.index = it->second;
		return handle;
	}

	// 実ファイルパス
	std::wstring full = AssetFullPath(relPath);

	// 画像読み込み
	DirectX::ScratchImage img;
	HRESULT hr = LoadFromWICFile(full.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, img);
	if (FAILED(hr)) {
		// 読めなかったら無効ハンドルのまま返す
		return handle;
	}

	const auto& meta = img.GetMetadata();

	// テクスチャリソース作成
	ComPtr<ID3D12Device> dev = dx_->Dev();
	CD3DX12_HEAP_PROPERTIES hpD(D3D12_HEAP_TYPE_DEFAULT);
	auto rdTex = CD3DX12_RESOURCE_DESC::Tex2D(meta.format, meta.width, (UINT)meta.height);
	ComPtr<ID3D12Resource> tex;
	hr = dev->CreateCommittedResource(&hpD, D3D12_HEAP_FLAG_NONE, &rdTex, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex));
	if (FAILED(hr)) {
		return handle;
	}

	// アップロード用バッファ
	UINT64 upSize = GetRequiredIntermediateSize(tex.Get(), 0, 1);
	CD3DX12_HEAP_PROPERTIES hpU(D3D12_HEAP_TYPE_UPLOAD);
	auto rdUp = CD3DX12_RESOURCE_DESC::Buffer(upSize);
	ComPtr<ID3D12Resource> up;
	hr = dev->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdUp, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&up));
	if (FAILED(hr)) {
		return handle;
	}

	// サブリソース設定
	D3D12_SUBRESOURCE_DATA sd{};
	sd.pData = img.GetImages()->pixels;
	sd.RowPitch = (LONG_PTR)img.GetImages()->rowPitch;
	sd.SlicePitch = (LONG_PTR)img.GetImages()->slicePitch;

	// ============================================
	// フレームの開閉は App 側で行うので、
	// ここでは二重に BeginFrame/EndFrame しない
	// （まだフレームが始まっていない場合だけ一時的に開く）
	// ============================================
	bool openedTempFrame = false;
	auto* cmd = dx_->List();
	if (!cmd) {
		dx_->BeginFrame();
		cmd = dx_->List();
		openedTempFrame = true;
	}

	// テクスチャを GPU にアップロード
	UpdateSubresources(cmd, tex.Get(), up.Get(), 0, 0, 1, &sd);
	auto bar = CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	cmd->ResourceBarrier(1, &bar);

	// ここで自分で開いたフレームだけ閉じる
	if (openedTempFrame) {
		dx_->EndFrame();
	}

	// SRVスロットをRendererから確保
	const int srvIndex = renderer_->AllocateSRV();
	D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
	sv.Format = meta.format;
	sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sv.Texture2D.MipLevels = 1;

	dev->CreateShaderResourceView(tex.Get(), &sv, renderer_->GetSRVCPU(srvIndex));

	// 登録
	TexData td;
	td.texture = tex;
	td.upload = up;
	td.srvIndex = srvIndex;
	td.gpu = renderer_->GetSRVGPU(srvIndex);

	int newIndex = static_cast<int>(textures_.size());
	textures_.push_back(td);
	pathToIndex_[relPath] = newIndex;

	handle.index = newIndex;
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGPU(const TextureHandle& h) const {
	if (!IsValid(h)) {
		D3D12_GPU_DESCRIPTOR_HANDLE dummy{};
		return dummy;
	}
	return textures_[h.index].gpu;
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetCPU(const TextureHandle& h) const {
	D3D12_CPU_DESCRIPTOR_HANDLE dummy{};
	if (!IsValid(h)) {
		return dummy;
	}
	return renderer_->GetSRVCPU(textures_[h.index].srvIndex);
}

} // namespace Engine
