#pragma once
// =======================================
//  Renderer : OBJ / Sprite / Sphere / Grid描画
//  ※シェーダは埋め込み文字列で同梱
//  ※ブレンドモード切替対応（Sprite）
// =======================================
#include "Camera.h"
#include "Matrix4x4.h"
#include "Model.h"
#include "Transform.h"
#include "WindowDX.h"
#include <DirectXMath.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

namespace Engine {

class Renderer {
public:
	// ★ 統一（cpp側の独自定数は廃止）
	static constexpr size_t kCBSlots = 8192; // リング総スロット数（以前4096⇔8192が混在）
	static constexpr size_t kCBStride = 256; // 256Bアライン

	// ==== ユーザーが触る構造体（ImGui連携想定） ====
	struct SpriteTf {
		Vector3 t{0, 0, 0}, s{1, 1, 1};
		Vector4 c{1, 1, 1, 1};
	};
	struct SphereTf {
		Vector3 t{0, 0, 0}, r{0, 0, 0}, s{1, 1, 1};
		Vector4 c{1, 1, 1, 1};
	};
	struct UVParam {
		Vector3 trans{0, 0, 0}, scale{1, 1, 1};
		float rot = 0.0f; // degree
	};

	// ==== ブレンドモード列挙 ====
	enum class BlendMode {
		Opaque,
		Alpha,
		Add,
		Subtract,
		Multiply,
	};

public:
	bool Initialize(WindowDX& dx);
	void Shutdown();

	void UpdateCB(const Camera& cam, const SpriteTf& spr, const SphereTf& sph, const UVParam& uv, const Vector3& lightDir);

	// useBallTex == true のとき球に sample.png を貼る（デモ用）
	void Record(WindowDX& dx, bool useBallTex);

	// ==== ブレンドモード操作 ====
	void SetBlendMode(BlendMode m) { blendMode_ = m; }
	BlendMode GetBlendMode() const { return blendMode_; }

	// ==== モデル管理 ====
	int LoadModel(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const std::string& filepath);
	// 追記
	void UpdateModelCBWithColor(int handle, const Camera& cam, const Transform& tf, const Vector4& mulColor);

	void DrawModel(int handle, ID3D12GraphicsCommandList* cmd);

	// ==== ★フレーム境界ラッパー ====
	void BeginFrame(ID3D12GraphicsCommandList* cmd);
	void EndFrame(ID3D12GraphicsCommandList* cmd);

	// ====  内部リソースアクセス ====
	ID3D12Device* GetDevice(WindowDX& dx) { return dx.Dev(); }
	ID3D12GraphicsCommandList* GetCommandList(WindowDX& dx) { return dx.List(); }

	void UpdateModelCBWithColorAt(int handle, size_t slot, const Engine::Camera& cam, const Engine::Transform& tf, const Engine::Vector4& mulColor);

	void DrawModelAt(int handle, ID3D12GraphicsCommandList* cmd, size_t slot);

	// ==== ★グリッド（Unityのシーンビュー風） ====
	// half: 片側の線本数 / cell: マス目サイズ / y: 設置高さ
	bool InitGrid(WindowDX& dx, int half = 30, float cell = 1.0f, float y = 0.0f, float phaseX = 0.0f, float phaseZ = 0.0f);

	void DrawGrid(const Camera& cam, ID3D12GraphicsCommandList* cmd, Vector4 mainColor = {0.15f, 0.15f, 0.15f, 1}, Vector4 axisColor = {0.30f, 0.30f, 0.30f, 1}, int majorStep = 5);

	// 共通
	struct Vertex {
		DirectX::XMFLOAT4 pos;    // (x,y,z,w) ※w=1
		DirectX::XMFLOAT2 uv;     // (u,v)
		DirectX::XMFLOAT3 normal; // 使わなくても確保
	};

	struct CBCommon {
		DirectX::XMFLOAT4X4 mvp; // ← XMMATRIX ではなく XMFLOAT4X4 に
		DirectX::XMFLOAT4 col;   // ← 色は col に一本化（mulColor は廃止）
	};
	static_assert(sizeof(CBCommon) <= kCBStride, "CBCommon too large for CB stride");

	struct CBSpriteUV {
		DirectX::XMFLOAT4X4 uvMat;
	};

	struct CBLight {
		DirectX::XMFLOAT4 dir;
	};

	// OBJ
	struct Modeler {
		std::vector<Vertex> v;
		Microsoft::WRL::ComPtr<ID3D12Resource> vb, cb, tex, up;
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rs;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		int srvIndex = 0;
	} mdl_;

	// Sprite
	struct Sprite {
		Microsoft::WRL::ComPtr<ID3D12Resource> vb, ib, cb, cbUv;
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		D3D12_INDEX_BUFFER_VIEW ibv{};
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rs;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> psoAlpha;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> psoAdd;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> psoSub;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> psoMul;
		Microsoft::WRL::ComPtr<ID3D12Resource> tex0, up0, tex1, up1;
		int srvIndex0 = 1;
		int srvIndex1 = 2;
	} spr_;

	// Sphere
	struct Sphere {
		Microsoft::WRL::ComPtr<ID3D12Resource> vb, ib, cb, cbLight;
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		D3D12_INDEX_BUFFER_VIEW ibv{};
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rs;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
	} sph_;

	// ==== ★Grid（ライン描画用） ====
	struct VTXL {
		Vector3 pos;
	};
	struct Grid {
		Microsoft::WRL::ComPtr<ID3D12Resource> vb; // 全ライン頂点
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		UINT vcount = 0;
		Microsoft::WRL::ComPtr<ID3D12Resource> cb; // mvp+color
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rs;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		float y = 0.0f;
		int half = 30;
		float cell = 1.0f;
		float phaseX = 0.0f;
		float phaseZ = 0.0f;
		UINT zCount = 0; // 生成した縦線の本数
		UINT xCount = 0; // 生成した横線の本数
	} grid_;

	// 複数モデル管理
	struct ModelEntry {
		std::unique_ptr<Model> model;
		Microsoft::WRL::ComPtr<ID3D12Resource> cb; // 定数バッファ
		D3D12_GPU_DESCRIPTOR_HANDLE srvGpu{};
		Transform transform;
		uint8_t* cbMapped = nullptr;
	};
	std::vector<ModelEntry> models_;

	// 共通リソース
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
	UINT descriptorSize_ = 0;
	int nextSrvIndex_ = 3;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rs_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

	// 内部ユーティリティ
	bool InitModel(WindowDX& dx);
	bool InitSprite(WindowDX& dx);
	bool InitSphere(WindowDX& dx);
	static std::vector<Vertex> LoadObj(const std::string& dir, const std::string& name);
	static Microsoft::WRL::ComPtr<ID3DBlob> Compile(const char* src, const char* entry, const char* target);

	// 一時
	SpriteTf sprTf_{};
	SphereTf sphTf_{
	    {0, 0, 5}
    };
	UVParam uv_{};
	Vector3 lightDir_{0, 1, -1};

	// 描画対象DX
	WindowDX* dx_ = nullptr;

	// 現在のブレンドモード
	BlendMode blendMode_ = BlendMode::Opaque;

	// ---- モデル情報へのアクセス ----
	ModelEntry& GetModel(int handle) { return models_[handle]; }
	const ModelEntry& GetModel(int handle) const { return models_[handle]; }

	// ==== レーザー専用PSO ====
	Microsoft::WRL::ComPtr<ID3D12PipelineState> psoLaser_;
	bool InitLaserPSO(ID3D12Device* device, ID3D12RootSignature* rs);

	size_t MaxModelCBSlots() const; // 例: 1024
	size_t CBStride() const;        // 例: 256（CBは256Bアライン）

	// ネオン枠（モデル用）を作るPSO
	bool InitNeonFramePSO(ID3D12Device* device, ID3D12RootSignature* rs);
	void DrawModelNeonFrame(int handle, ID3D12GraphicsCommandList* cmd);

	void DrawModelNeonFrameAt(int handle, ID3D12GraphicsCommandList* cmd, size_t slot);

private:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> psoNeonFrame_;
};

} // namespace Engine
