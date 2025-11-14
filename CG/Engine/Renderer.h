#pragma once
// =======================================
//  Renderer : OBJ / Sprite / Sphere / Grid / Voxel / Laser
//  ※シェーダは埋め込み文字列で同梱（cpp）
// =======================================
#include "Camera.h"
#include "Matrix4x4.h"
#include "Model.h"
#include "Transform.h"
#include "WindowDX.h"

#include <DirectXMath.h>
#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>

namespace Engine {

class Renderer {
public:
	// ---- 定数（CBは256Bアライン）----
	static constexpr size_t kCBSlots = 8192;
	static constexpr size_t kCBStride = 256;
	static constexpr UINT kSRVHeapSize = 16384;

	// ---- UI から操作する構造体 ----
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

	// ---- ブレンドモード ----
	enum class BlendMode { Opaque, Alpha, Add, Subtract, Multiply };

public:
	// 初期化/終了
	bool Initialize(WindowDX& dx);
	void Shutdown();

	// 共通CB更新 & デモ描画（OBJ, Sphere, Sprite）
	void UpdateCB(const Camera& cam, const SpriteTf& spr, const SphereTf& sph, const UVParam& uv, const Vector3& lightDir);
	void Record(WindowDX& dx, bool useBallTex);

	// ブレンドモード
	void SetBlendMode(BlendMode m) { blendMode_ = m; }
	BlendMode GetBlendMode() const { return blendMode_; }

	// モデル（外部Modelクラス）
	int LoadModel(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const std::string& filepath);
	void UpdateModelCBWithColor(int handle, const Camera& cam, const Transform& tf, const Vector4& mulColor);
	void DrawModel(int handle, ID3D12GraphicsCommandList* cmd);

	// フレーム境界（必要なときだけ使用）
	void BeginFrame(ID3D12GraphicsCommandList* cmd);
	void EndFrame(ID3D12GraphicsCommandList* cmd);

	// 便利アクセス
	ID3D12Device* GetDevice(WindowDX& dx) { return dx.Dev(); }
	ID3D12GraphicsCommandList* GetCommandList(WindowDX& dx) { return dx.List(); }

	// モデル：多スロットCB
	void UpdateModelCBWithColorAt(int handle, size_t slot, const Camera& cam, const Transform& tf, const Vector4& mulColor);
	void DrawModelAt(int handle, ID3D12GraphicsCommandList* cmd, size_t slot);

	// Grid（Unity風シーングリッド）
	bool InitGrid(WindowDX& dx, int half = 30, float cell = 1.0f, float y = 0.0f, float phaseX = 0.0f, float phaseZ = 0.0f);
	void DrawGrid(const Camera& cam, ID3D12GraphicsCommandList* cmd, Vector4 mainColor = {0.15f, 0.15f, 0.15f, 1}, Vector4 axisColor = {0.30f, 0.30f, 0.30f, 1}, int majorStep = 5);

	// ネオン枠（モデル用）PSO
	bool InitNeonFramePSO(ID3D12Device* device, ID3D12RootSignature* rs);
	void DrawModelNeonFrame(int handle, ID3D12GraphicsCommandList* cmd);
	void DrawModelNeonFrameAt(int handle, ID3D12GraphicsCommandList* cmd, size_t slot);

	// レーザー専用PSO
	bool InitLaserPSO(ID3D12Device* device, ID3D12RootSignature* rs);

	// 情報
	size_t MaxModelCBSlots() const; // = kCBSlots
	size_t CBStride() const;        // = kCBStride

	// ==== Voxel（Compute生成 → Draw）====
	bool InitVoxelCS(ID3D12Device* dev);
	bool InitVoxelDrawPSO(ID3D12Device* dev);
	bool CreateVoxelBuffers(ID3D12Device* dev, UINT maxVertices);
	void DispatchVoxel(ID3D12GraphicsCommandList* cmd, UINT gridX, UINT gridZ);
	UINT ReadbackVoxelVertexCount();
	void DrawVoxel(ID3D12GraphicsCommandList* cmd, const Camera& cam);

	// ==== モデルまとめ ====
	struct Vertex {
		DirectX::XMFLOAT4 pos;
		DirectX::XMFLOAT2 uv;
		DirectX::XMFLOAT3 normal;
	};
	struct CBCommon {
		DirectX::XMFLOAT4X4 mvp;
		DirectX::XMFLOAT4 col;
	};
	struct CBLight {
		DirectX::XMFLOAT4 dir;
	};

	struct ModelEntry {
		std::unique_ptr<Model> model;
		Microsoft::WRL::ComPtr<ID3D12Resource> cb;
		D3D12_GPU_DESCRIPTOR_HANDLE srvGpu{};
		Transform transform;
		uint8_t* cbMapped = nullptr;
	};

private:
	// ---- 内部ユーティリティ（cppで実装）----
	static std::vector<Vertex> LoadObj(const std::string& dir, const std::string& name);
	static Microsoft::WRL::ComPtr<ID3DBlob> Compile(const char* src, const char* entry, const char* target);

	bool InitModel(WindowDX& dx);
	bool InitSprite(WindowDX& dx);
	bool InitSphere(WindowDX& dx);

public: // （外からも参照することが多いので public に）
	// OBJ（デモ）
	struct Modeler {
		std::vector<Vertex> v;
		Microsoft::WRL::ComPtr<ID3D12Resource> vb, cb, tex, up;
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rs;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		int srvIndex = 0;
	} mdl_;

	// Sprite（各種ブレンド）
	struct Sprite {
		Microsoft::WRL::ComPtr<ID3D12Resource> vb, ib, cb, cbUv;
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		D3D12_INDEX_BUFFER_VIEW ibv{};
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rs;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> psoAlpha, psoAdd, psoSub, psoMul;
		Microsoft::WRL::ComPtr<ID3D12Resource> tex0, up0, tex1, up1;
		int srvIndex0 = 1, srvIndex1 = 2;
	} spr_;

	// Sphere（簡易ライティング）
	struct Sphere {
		Microsoft::WRL::ComPtr<ID3D12Resource> vb, ib, cb, cbLight;
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		D3D12_INDEX_BUFFER_VIEW ibv{};
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rs;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
	} sph_;

	// Grid
	struct VTXL {
		Vector3 pos;
	};
	struct Grid {
		Microsoft::WRL::ComPtr<ID3D12Resource> vb;
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		UINT vcount = 0;
		Microsoft::WRL::ComPtr<ID3D12Resource> cb;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rs;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		float y = 0.0f;
		int half = 30;
		float cell = 1.0f;
		float phaseX = 0.0f, phaseZ = 0.0f;
		UINT zCount = 0, xCount = 0;
	} grid_;

	// 複数モデル
	std::vector<ModelEntry> models_;

	// 共通（SRVヒープ / ルート / PSO）
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
	UINT descriptorSize_ = 0;
	int nextSrvIndex_ = 3;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rs_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> psoNeonFrame_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> psoLaser_;

	// 実行時設定
	WindowDX* dx_ = nullptr;
	BlendMode blendMode_ = BlendMode::Opaque;

	// デモ用状態
	SpriteTf sprTf_{};
	SphereTf sphTf_{
	    {0, 0, 5}
    };
	UVParam uv_{};
	Vector3 lightDir_{0, 1, -1};

	// ==== Voxel（Compute）====
	struct VoxelGPU {
		// 頂点出力（UAV & VB 兼用）
		Microsoft::WRL::ComPtr<ID3D12Resource> vbUav;
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		UINT maxVertices = 0;

		// 頂点数カウンタ
		Microsoft::WRL::ComPtr<ID3D12Resource> counterUav; // RAW R32
		int counterSrvIndex = -1;

		Microsoft::WRL::ComPtr<ID3D12Resource> counterReadback;
		void* counterCpuPtr = nullptr;

		// SRV/UAVテーブル用の先頭インデックス
		int vbUavIndex = -1;
		int dummySrvIndex = -1;

		// CS
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rsCS;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> psoCS;

		// Draw
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rsVoxelDraw;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> psoVoxelDraw;

		// CB
		Microsoft::WRL::ComPtr<ID3D12Resource> cbCS;
		Microsoft::WRL::ComPtr<ID3D12Resource> cbDraw;

		// （将来のトライプラナ用）テクスチャ
		Microsoft::WRL::ComPtr<ID3D12Resource> tex[3], texUp[3];
		UINT texBaseIndex = UINT_MAX; // t0.t2

		// ---- 凹み情報（ボス攻撃）----
		struct Dent {
			DirectX::XMFLOAT2 centerXZ; // 凹み中心 (x,z)
			float radius;               // 半径
			float depth;                // 深さ（正の値、下方向にへこませる）
		};

		static constexpr UINT kMaxDents = 32;

		Dent dents[kMaxDents]{}; // 登録済み凹み
		UINT dentCount = 0;      // 有効な凹み数

		// ---- CS に渡す定数バッファ ----
		struct CBCS {
			DirectX::XMUINT2 grid; // (nx, nz)
			float cell;
			float amp;

			float freq;
			UINT maxVerts;
			UINT dentCount;
			float pad; // 16B アライン用

			Dent dents[kMaxDents];
		} params{};
	} voxel_;

	// 現在のボクセル地形パラメータから、任意XZにおける高さ/法線を返す
	float TerrainHeightAt(float x, float z) const;
	Vector3 TerrainNormalAt(float x, float z) const;

	// ボス攻撃などで「この位置をへこませたい」という情報を登録
	void AddTerrainDent(const DirectX::XMFLOAT3& position, float radius, float depth);

	// 4B の 0 を置いたアップロード（UAVカウンタ初期化に使う）
	Microsoft::WRL::ComPtr<ID3D12Resource> zeroUpload_;
};

} // namespace Engine
