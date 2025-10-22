#pragma once
#include <DirectXMath.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>
#include <DirectXTex.h> 

namespace Engine {

struct VertexData {
	DirectX::XMFLOAT4 position;
	DirectX::XMFLOAT2 texcoord;
	DirectX::XMFLOAT3 normal;
};

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

class Model {
public:
	// objPath: 例 "resources/cube.obj"
	bool Load(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const std::string& objPath);

	// SRV を指定のヒープ index に作成して、GPU ハンドルを保持
	void CreateSrv(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap, UINT descriptorSize, UINT heapIndex);

	// rootSrvParamIndex は既定で 2（あなたの RootParameter[2] と一致）
	void Draw(ID3D12GraphicsCommandList* cmd, UINT rootSrvParamIndex = 1);

	   UINT GetVertexCount() const { return static_cast<UINT>(data_.vertices.size()); }

	// 必要なら外から使えるやつ
	const D3D12_VERTEX_BUFFER_VIEW& GetVBV() const { return vbv_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpu() const { return srvGpu_; }

	// ------------ 低レベルユーティリティ ------------
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& meta);

	static Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* cmd);

	// ------------ OBJ/MTL ローダ ------------
	static MaterialData LoadMtl(const std::string& dir, const std::string& mtlFile);
	static ModelData LoadObj(const std::string& dir, const std::string& objFile);


private:
	

	// ------------ メンバ ------------
	ModelData data_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
	D3D12_VERTEX_BUFFER_VIEW vbv_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> tex_;
	Microsoft::WRL::ComPtr<ID3D12Resource> upload_; // 中間バッファ保持
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_{};
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpu_{};
	bool hasTexture_ = false;
};
} // namespace MyNamespace