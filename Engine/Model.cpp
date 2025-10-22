#include "Model.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>

#include "d3dx12.h" // 例: externals/DirectXTex/d3dx12.h
#include <DirectXTex.h>

using Microsoft::WRL::ComPtr;

namespace Engine {

// 文字列 -> wstring
static std::wstring ToWide(const std::string& s) {
	if (s.empty())
		return {};
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
	std::wstring w(n - 1, 0);
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
	return w;
}

// フルパスを dir / file に分割（filesystem 不使用）
static void SplitPath(const std::string& full, std::string& dir, std::string& file) {
	size_t p = full.find_last_of("/\\");
	if (p == std::string::npos) {
		dir = ".";
		file = full;
	} else {
		dir = full.substr(0, p);
		file = full.substr(p + 1);
	}
}

// --------------------- GPU ユーティリティ ---------------------
ComPtr<ID3D12Resource> Model::CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = sizeInBytes;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> res;
	[[maybe_unused]] HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&res));
	assert(SUCCEEDED(hr));
	return res;
}

ComPtr<ID3D12Resource> Model::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& meta) {
	D3D12_RESOURCE_DESC rd{};
	rd.Width = UINT(meta.width);
	rd.Height = UINT(meta.height);
	rd.MipLevels = UINT16(meta.mipLevels);
	rd.DepthOrArraySize = UINT16(meta.arraySize);
	rd.Format = meta.format;
	rd.SampleDesc.Count = 1;
	rd.Dimension = D3D12_RESOURCE_DIMENSION(UINT(meta.dimension));
	rd.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES hp{};
	hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	ComPtr<ID3D12Resource> tex;
	[[maybe_unused]] HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex));
	assert(SUCCEEDED(hr));
	return tex;
}

ComPtr<ID3D12Resource> Model::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* cmd) {
	std::vector<D3D12_SUBRESOURCE_DATA> subs;
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subs);

	uint64_t interSize = GetRequiredIntermediateSize(texture, 0, UINT(subs.size()));
	ComPtr<ID3D12Resource> inter = CreateBufferResource(device, interSize);

	UpdateSubresources(cmd, texture, inter.Get(), 0, 0, UINT(subs.size()), subs.data());

	D3D12_RESOURCE_BARRIER br{};
	br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	br.Transition.pResource = texture;
	br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	br.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	br.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	cmd->ResourceBarrier(1, &br);

	return inter; // 呼び出し側でスコープ維持
}

// --------------------- OBJ / MTL ローダ ---------------------
MaterialData Model::LoadMtl(const std::string& dir, const std::string& mtlFile) {
	MaterialData out{};
	std::ifstream file(dir + "/" + mtlFile);
	if (!file.is_open())
		return out;

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream s(line);
		std::string id;
		s >> id;
		if (id == "map_Kd") {
			std::string tex;
			s >> tex;
			out.textureFilePath = dir + "/" + tex;
		}
	}
	return out;
}

ModelData Model::LoadObj(const std::string& dir, const std::string& objFile) {
	ModelData md{};
	std::ifstream file(dir + "/" + objFile);
	assert(file.is_open());

	std::vector<DirectX::XMFLOAT4> positions;
	std::vector<DirectX::XMFLOAT2> uvs;
	std::vector<DirectX::XMFLOAT3> norms;

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream s(line);
		std::string id;
		s >> id;

		if (id == "v") {
			DirectX::XMFLOAT4 p{};
			s >> p.x >> p.y >> p.z;
			p.w = 1.0f;
			p.x *= -1.0f; // 左手系へ反転
			positions.push_back(p);
		} else if (id == "vt") {
			DirectX::XMFLOAT2 uv{};
			s >> uv.x >> uv.y;
			//	uv.y = 1.0f - uv.y;
			uvs.push_back(uv);
		} else if (id == "vn") {
			DirectX::XMFLOAT3 n{};
			s >> n.x >> n.y >> n.z;
			n.x *= -1.0f;
			norms.push_back(n);
		} else if (id == "f") {
			std::vector<VertexData> poly;
			std::string vdef;
			while (s >> vdef) {
				std::istringstream v(vdef);
				uint32_t idx[3]{};
				for (int i = 0; i < 3; ++i) {
					std::string t;
					std::getline(v, t, '/');
					idx[i] = std::stoi(t);
				}
				VertexData vd{};
				vd.position = positions[idx[0] - 1];
				vd.texcoord = uvs[idx[1] - 1];
				vd.normal = norms[idx[2] - 1];
				poly.push_back(vd);
			}
			// 三角形ファン → 左手 CCW
			for (size_t i = 1; i + 1 < poly.size(); ++i) {
				md.vertices.push_back(poly[i + 1]);
				md.vertices.push_back(poly[i]);
				md.vertices.push_back(poly[0]);
			}
		} else if (id == "mtllib") {
			std::string mtl;
			s >> mtl;
			md.material = LoadMtl(dir, mtl);
		}
	}
	return md;
}

// --------------------- Model 本体 ---------------------
bool Model::Load(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const std::string& objPath) {
	std::string dir, file;
	SplitPath(objPath, dir, file);

	// CPU側読み込み
	data_ = LoadObj(dir, file);

	// 頂点バッファ
	vb_ = CreateBufferResource(device, sizeof(VertexData) * data_.vertices.size());
	vbv_.BufferLocation = vb_->GetGPUVirtualAddress();
	vbv_.SizeInBytes = UINT(sizeof(VertexData) * data_.vertices.size());
	vbv_.StrideInBytes = sizeof(VertexData);

	// 転送
	{
		VertexData* map = nullptr;
		vb_->Map(0, nullptr, reinterpret_cast<void**>(&map));
		std::memcpy(map, data_.vertices.data(), sizeof(VertexData) * data_.vertices.size());
		vb_->Unmap(0, nullptr);
	}

	// テクスチャ
	hasTexture_ = false;
	if (!data_.material.textureFilePath.empty()) {
		DirectX::ScratchImage mip;
		auto w = ToWide(data_.material.textureFilePath);
		[[maybe_unused]] HRESULT hr = DirectX::LoadFromWICFile(w.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, mip);
		assert(SUCCEEDED(hr));

		tex_ = CreateTextureResource(device, mip.GetMetadata());
		upload_ = UploadTextureData(tex_.Get(), mip, device, cmd);

		srvDesc_.Format = mip.GetMetadata().format;
		srvDesc_.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc_.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc_.Texture2D.MipLevels = UINT(mip.GetMetadata().mipLevels);

		hasTexture_ = true;
	}
	return true;
}

void Model::CreateSrv(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap, UINT descriptorSize, UINT heapIndex) {
	if (!hasTexture_)
		return;

	// CPU/GPU ハンドル計算
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
	cpu.ptr += SIZE_T(descriptorSize) * heapIndex;

	srvGpu_ = srvHeap->GetGPUDescriptorHandleForHeapStart();
	srvGpu_.ptr += UINT64(descriptorSize) * heapIndex;

	// SRV 発行
	device->CreateShaderResourceView(tex_.Get(), &srvDesc_, cpu);
}

void Model::Draw(ID3D12GraphicsCommandList* cmd, UINT rootSrvParamIndex) {
	cmd->IASetVertexBuffers(0, 1, &vbv_);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	if (hasTexture_) {
		cmd->SetGraphicsRootDescriptorTable(rootSrvParamIndex, srvGpu_);
	}
	cmd->DrawInstanced((UINT)data_.vertices.size(), 1, 0, 0);
}
} // namespace Engine