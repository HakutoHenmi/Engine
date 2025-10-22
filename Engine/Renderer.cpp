#include "Renderer.h"
#include <DirectXTex.h>
#include <d3dcompiler.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ----- HRESULT チェッカー（ThrowIfFailed 代替）-----
#ifndef HR_CHECK
#define HR_CHECK(x)                                                                                                                                                                                    \
	do {                                                                                                                                                                                               \
		HRESULT __hr__ = (x);                                                                                                                                                                          \
		if (FAILED(__hr__)) {                                                                                                                                                                          \
			char buf[256];                                                                                                                                                                             \
			sprintf_s(buf, "HR failed 0x%08X at %s(%d)\n", (unsigned)__hr__, __FILE__, __LINE__);                                                                                                      \
			OutputDebugStringA(buf);                                                                                                                                                                   \
			assert(false && "D3D12 call failed");                                                                                                                                                      \
			std::abort();                                                                                                                                                                              \
		}                                                                                                                                                                                              \
	} while (0)
#endif

namespace Engine {

static constexpr UINT kCBStride = 256; // CBVは256Bアライン
static constexpr UINT kCBSlots = 8192; // ←★ ここを増やす！（4096～8192が安全）

// ---- HLSL（元ソース踏襲） ----
// ------------------- OBJ / Model -------------------
static const char* gVSObj = R"(
cbuffer C0:register(b0){ float4x4 mvp; float4 col; }
struct VSIn  { float4 pos:POSITION; float2 uv:TEXCOORD; };
struct VSOut { float4 sp:SV_Position; float2 uv:TEXCOORD; };
VSOut main(VSIn i){
    VSOut o;
    o.sp = mul(i.pos, mvp);
    o.uv = i.uv;
    return o;
})";

static const char* gPSObj = R"(
cbuffer C0:register(b0){ float4x4 mvp; float4 col; }
Texture2D T:register(t0);
SamplerState S:register(s0);
float4 main(float4 sp:SV_Position, float2 uv:TEXCOORD):SV_Target {
    return T.Sample(S, uv) * col;
})";

// ------------------- Sprite -------------------
static const char* gVSSprite = R"(
cbuffer C0:register(b0){ float4x4 mvp; float4 col; }
struct VSIn  { float4 pos:POSITION; float2 uv:TEXCOORD; };
struct VSOut { float4 sp:SV_Position; float2 uv:TEXCOORD; };
VSOut main(VSIn i){
    VSOut o;
    o.sp = mul(i.pos, mvp);
    o.uv = i.uv;
    return o;
})";

static const char* gPSSprite = R"(
cbuffer C0:register(b0){ float4x4 mvp; float4 col; }
cbuffer C1:register(b1){ float4x4 uvMat; }
Texture2D T:register(t0);
SamplerState S:register(s0);
float4 main(float4 sp:SV_Position, float2 uv:TEXCOORD):SV_Target {
    float3 tuv = mul((float3x3)uvMat, float3(uv, 1));
    return T.Sample(S, tuv.xy) * col;
})";

// ------------------- Sphere / Lighting -------------------
static const char* gVSLit = R"(
cbuffer C0:register(b0){ float4x4 mvp; float4 col; }
struct VSIn  { float4 pos:POSITION; float2 uv:TEXCOORD; };
struct VSOut { float4 sp:SV_Position; float2 uv:TEXCOORD; float3 n:NORMAL; };
VSOut main(VSIn i){
    VSOut o;
    o.sp = mul(i.pos, mvp);
    o.uv = i.uv;
    // 擬似法線：位置正規化で代用
    o.n = normalize(i.pos.xyz);
    return o;
})";

static const char* gPSLit = R"(
cbuffer C0:register(b0){ float4x4 mvp; float4 col; }
cbuffer C1:register(b1){ float4 dir; }
Texture2D T:register(t0);
SamplerState S:register(s0);
float4 main(float4 sp:SV_Position, float2 uv:TEXCOORD, float3 n:NORMAL):SV_Target {
    float ndl = dot(normalize(n), normalize(-dir.xyz));
    float hLambert = pow(0.5 * ndl + 0.5, 2.0);
    return T.Sample(S, uv) * col * hLambert;
})";

// ==== ★Grid用シェーダ（ライン描画・単色） ====
static const char* gVSLine = R"(
cbuffer C0:register(b0){float4x4 mvp;float4 col;}
struct I{float3 p:POSITION;};
struct O{float4 p:SV_Position;};
O main(I i){O o;o.p=mul(float4(i.p,1),mvp);return o;}
)";

static const char* gPSLine = R"(
cbuffer C0:register(b0){float4x4 mvp;float4 col;}
float4 main():SV_Target{ return col; }
)";

// ---- モデル用：ネオン枠（UVベース, FRAC対応版）----
static const char* gPSNeonFrame = R"(
cbuffer C0:register(b0){ float4x4 mvp; float4 col; }

float4 main(float4 sp:SV_Position, float2 uv:TEXCOORD) : SV_Target
{
    // タイルUVでも 0..1 範囲に丸める
    float2 uv01 = frac(uv);
    float2 d2   = min(uv01, 1.0 - uv01);
    float  edge = min(d2.x, d2.y);

    const float wLine = 0.020;  // 線の太さ（必要なら調整）
    const float wGlow = 0.080;  // グロー幅

    float aLine = 1.0 - smoothstep(wLine - 0.003, wLine + 0.003, edge);
    float aCore = 1.0 - smoothstep(0.0,           wLine * 0.6,   edge);
    float aGlow = 1.0 - smoothstep(wLine,         wLine + wGlow, edge);
    float a = saturate(0.90*aLine + 0.45*aCore + 0.55*aGlow);

    return float4(col.rgb * a, a); // 加算ブレンド
}
)";

static std::wstring Asset(const std::wstring& rel) {
	wchar_t buf[MAX_PATH]{};
	GetModuleFileNameW(nullptr, buf, MAX_PATH);
	return (std::filesystem::path(buf).parent_path() / rel).wstring();
}

ComPtr<ID3DBlob> Renderer::Compile(const char* src, const char* entry, const char* target) {
	UINT fl = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	fl |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	ComPtr<ID3DBlob> s, e;
	HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target, fl, 0, &s, &e);
	if (FAILED(hr)) {
		if (e)
			MessageBoxA(nullptr, (const char*)e->GetBufferPointer(), "HLSL Compile Error", MB_OK);
		std::abort();
	}
	return s;
}

std::vector<Renderer::Vertex> Renderer::LoadObj(const std::string& dir, const std::string& name) {
	std::vector<Vertex> out;
	std::ifstream file(dir + "/" + name);
	if (!file.is_open()) {
		MessageBoxA(nullptr, "OBJ open fail", "OBJ", 0);
		std::abort();
	}
	std::vector<Vector3> pos;
	std::vector<Vector2> uv;
	std::string line, id;
	while (std::getline(file, line)) {
		std::istringstream s(line);
		s >> id;
		if (id == "v") {
			Vector3 p{};
			s >> p.x >> p.y >> p.z;
			pos.push_back(p);
		} else if (id == "vt") {
			Vector2 t{};
			s >> t.x >> t.y;
			t.y = 1 - t.y;
			uv.push_back(t);
		} else if (id == "f") {
			for (int i = 0; i < 3; ++i) {
				std::string vdef;
				s >> vdef;
				uint32_t ip, it;
				sscanf_s(vdef.c_str(), "%u/%u", &ip, &it);
				Vertex v{};
				v.pos = XMFLOAT4(pos[ip - 1].x, pos[ip - 1].y, pos[ip - 1].z, 1.0f);
				v.uv = XMFLOAT2(uv[it - 1].x, uv[it - 1].y);
				v.normal = XMFLOAT3(0, 0, 1); // ダミー法線
				out.push_back(v);
			}
		}
	}
	return out;
}

bool Renderer::Initialize(WindowDX& dx) {
	// SRVヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC hd{};
	hd.NumDescriptors = 128;
	hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	dx.Dev()->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&srvHeap_));
	descriptorSize_ = dx.Dev()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// RS / PSO（共通のもの）
	CD3DX12_DESCRIPTOR_RANGE rng;
	rng.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_ROOT_PARAMETER rp[2];
	rp[0].InitAsConstantBufferView(0); // ← 0:b0
	rp[1].InitAsDescriptorTable(1, &rng, D3D12_SHADER_VISIBILITY_PIXEL);
	CD3DX12_STATIC_SAMPLER_DESC smp(0);
	CD3DX12_ROOT_SIGNATURE_DESC rsd;
	rsd.Init(2, rp, 1, &smp, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> sig, err;
	D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
	dx.Dev()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rs_));

	// シェーダ読み込み
	auto vs = Compile(gVSObj, "main", "vs_5_0");
	auto ps = Compile(gPSObj, "main", "ps_5_0");

	D3D12_INPUT_ELEMENT_DESC il[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
	d.pRootSignature = rs_.Get();
	d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	d.InputLayout = {il, _countof(il)};
	d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	d.SampleMask = UINT_MAX;
	d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d.NumRenderTargets = 1;
	d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	d.SampleDesc.Count = 1;
	dx.Dev()->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso_));

	InitLaserPSO(dx.Dev(), rs_.Get());
	InitNeonFramePSO(dx.Dev(), rs_.Get());

	return true;
}

void Renderer::Shutdown() {
	// ---- 内部モデル（デモ用 OBJ） ----
	mdl_.v.clear();
	mdl_.vb.Reset();
	mdl_.cb.Reset();
	mdl_.tex.Reset();
	mdl_.up.Reset();
	mdl_.rs.Reset();
	mdl_.pso.Reset();
	mdl_.srvIndex = 0;

	// ---- スプライト ----
	spr_.vb.Reset();
	spr_.ib.Reset();
	spr_.cb.Reset();
	spr_.cbUv.Reset();
	spr_.rs.Reset();
	spr_.pso.Reset();
	spr_.psoAlpha.Reset();
	spr_.psoAdd.Reset();
	spr_.psoSub.Reset();
	spr_.psoMul.Reset();
	spr_.tex0.Reset();
	spr_.up0.Reset();
	spr_.tex1.Reset();
	spr_.up1.Reset();
	spr_.srvIndex0 = 1;
	spr_.srvIndex1 = 2;

	// ---- 球体 ----
	sph_.vb.Reset();
	sph_.ib.Reset();
	sph_.cb.Reset();
	sph_.cbLight.Reset();
	sph_.rs.Reset();
	sph_.pso.Reset();

	// ---- グリッド ----
	grid_.vb.Reset();
	grid_.cb.Reset();
	grid_.rs.Reset();
	grid_.pso.Reset();
	grid_.vcount = 0;
	grid_.half = 30;
	grid_.cell = 1.0f;
	grid_.y = 0.0f;

	// ---- 複数モデル管理 ----
	for (auto& e : models_) {
		e.cb.Reset();
		e.model.reset();
		e.srvGpu = {};
		e.transform = {};
	}
	models_.clear();

	// ---- 共通ヒープ / ルート / PSO ----
	psoLaser_.Reset();
	pso_.Reset();
	rs_.Reset();
	srvHeap_.Reset();
	descriptorSize_ = 0;
	nextSrvIndex_ = 3;

	// 参照だけなので解放対象ではないが、安全のため
	dx_ = nullptr;

	// 以降、ComPtr はスコープ外で参照カウント 0 になり解放されます
}

// ---------------- Model ----------------
bool Renderer::InitModel(WindowDX& dx) {
	// 頂点
	mdl_.v = LoadObj("Resources", "plane.obj");
	CD3DX12_HEAP_PROPERTIES hpU(D3D12_HEAP_TYPE_UPLOAD);
	auto rdV = CD3DX12_RESOURCE_DESC::Buffer(sizeof(Vertex) * mdl_.v.size());
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdV, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mdl_.vb));
	void* p = nullptr;
	mdl_.vb->Map(0, nullptr, &p);
	memcpy(p, mdl_.v.data(), rdV.Width);
	mdl_.vb->Unmap(0, nullptr);
	mdl_.vbv = {mdl_.vb->GetGPUVirtualAddress(), (UINT)rdV.Width, sizeof(Vertex)};

	// CB
	auto rdC = CD3DX12_RESOURCE_DESC::Buffer(256);
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdC, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mdl_.cb));

	// RS/PSO
	CD3DX12_DESCRIPTOR_RANGE rng;
	rng.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_ROOT_PARAMETER rp[2];
	rp[0].InitAsConstantBufferView(0);
	rp[1].InitAsDescriptorTable(1, &rng, D3D12_SHADER_VISIBILITY_PIXEL);
	CD3DX12_STATIC_SAMPLER_DESC smp(0);
	CD3DX12_ROOT_SIGNATURE_DESC rsd;
	rsd.Init(2, rp, 1, &smp, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> sig, err;
	D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
	dx.Dev()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&mdl_.rs));
	auto vs = Compile(gVSObj, "main", "vs_5_0"), ps = Compile(gPSObj, "main", "ps_5_0");
	D3D12_INPUT_ELEMENT_DESC il[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
	d.pRootSignature = mdl_.rs.Get();
	d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	d.InputLayout = {il, _countof(il)};
	d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // ← モデルは不透明のまま
	d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	d.SampleMask = UINT_MAX;
	d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d.NumRenderTargets = 1;
	d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	d.SampleDesc.Count = 1;
	dx.Dev()->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&mdl_.pso));

	// Texture(uvChecker)
	DirectX::ScratchImage img;
	LoadFromWICFile(Asset(L"Resources/uvChecker.png").c_str(), WIC_FLAGS_FORCE_SRGB, nullptr, img);
	const auto& m = img.GetMetadata();
	CD3DX12_HEAP_PROPERTIES hpD(D3D12_HEAP_TYPE_DEFAULT);
	auto rdT = CD3DX12_RESOURCE_DESC::Tex2D(m.format, m.width, (UINT)m.height);
	dx.Dev()->CreateCommittedResource(&hpD, D3D12_HEAP_FLAG_NONE, &rdT, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mdl_.tex));
	UINT64 up = GetRequiredIntermediateSize(mdl_.tex.Get(), 0, 1);
	auto rdUp = CD3DX12_RESOURCE_DESC::Buffer(up);
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdUp, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mdl_.up));
	D3D12_SUBRESOURCE_DATA sd{img.GetImages()->pixels, (LONG_PTR)img.GetImages()->rowPitch, (LONG_PTR)img.GetImages()->slicePitch};

	auto list = dx.List();
	dx.BeginFrame(); // 一旦Beginして更新
	UpdateSubresources(list, mdl_.tex.Get(), mdl_.up.Get(), 0, 0, 1, &sd);
	auto bar = CD3DX12_RESOURCE_BARRIER::Transition(mdl_.tex.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	list->ResourceBarrier(1, &bar);
	dx.EndFrame();

	// SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
	sv.Format = m.format;
	sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sv.Texture2D.MipLevels = 1;
	dx.Dev()->CreateShaderResourceView(mdl_.tex.Get(), &sv, dx.SRV_CPU(mdl_.srvIndex));
	return true;
}

// ---------------- Sprite（ブレンド対応） ----------------
bool Renderer::InitSprite(WindowDX& dx) {
	// 四角形VB/IB
	Vertex quad[] = {
	    {{0, 0, 0, 1},     {0, 0}, {0, 0, 1}},
	    {{640, 0, 0, 1},   {1, 0}, {0, 0, 1}},
	    {{0, 360, 0, 1},   {0, 1}, {0, 0, 1}},
	    {{640, 360, 0, 1}, {1, 1}, {0, 0, 1}},
	};
	uint32_t idx[] = {0, 1, 2, 1, 3, 2};
	CD3DX12_HEAP_PROPERTIES hpU(D3D12_HEAP_TYPE_UPLOAD);
	auto rdV = CD3DX12_RESOURCE_DESC::Buffer(sizeof(quad));
	auto rdI = CD3DX12_RESOURCE_DESC::Buffer(sizeof(idx));
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdV, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&spr_.vb));
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdI, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&spr_.ib));
	void* p = nullptr;
	spr_.vb->Map(0, nullptr, &p);
	memcpy(p, quad, sizeof(quad));
	spr_.vb->Unmap(0, nullptr);
	spr_.ib->Map(0, nullptr, &p);
	memcpy(p, idx, sizeof(idx));
	spr_.ib->Unmap(0, nullptr);
	spr_.vbv = {spr_.vb->GetGPUVirtualAddress(), sizeof(quad), sizeof(Vertex)};
	spr_.ibv = {spr_.ib->GetGPUVirtualAddress(), sizeof(idx), DXGI_FORMAT_R32_UINT};

	// CB
	auto rdC = CD3DX12_RESOURCE_DESC::Buffer(256);
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdC, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&spr_.cb));
	auto rdU = CD3DX12_RESOURCE_DESC::Buffer(256);
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdU, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&spr_.cbUv));

	// 2種テクスチャ（uvChecker/sample）
	DirectX::ScratchImage img0, img1;
	LoadFromWICFile(Asset(L"Resources/uvChecker.png").c_str(), WIC_FLAGS_FORCE_SRGB, nullptr, img0);
	LoadFromWICFile(Asset(L"Resources/sample.png").c_str(), WIC_FLAGS_FORCE_SRGB, nullptr, img1);
	const auto &m0 = img0.GetMetadata(), m1 = img1.GetMetadata();

	CD3DX12_HEAP_PROPERTIES hpD(D3D12_HEAP_TYPE_DEFAULT);
	auto rdT0 = CD3DX12_RESOURCE_DESC::Tex2D(m0.format, m0.width, (UINT)m0.height);
	auto rdT1 = CD3DX12_RESOURCE_DESC::Tex2D(m1.format, m1.width, (UINT)m1.height);
	dx.Dev()->CreateCommittedResource(&hpD, D3D12_HEAP_FLAG_NONE, &rdT0, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&spr_.tex0));
	dx.Dev()->CreateCommittedResource(&hpD, D3D12_HEAP_FLAG_NONE, &rdT1, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&spr_.tex1));

	UINT64 up0 = GetRequiredIntermediateSize(spr_.tex0.Get(), 0, 1);
	UINT64 up1 = GetRequiredIntermediateSize(spr_.tex1.Get(), 0, 1);
	auto rdUp0 = CD3DX12_RESOURCE_DESC::Buffer(up0);
	auto rdUp1 = CD3DX12_RESOURCE_DESC::Buffer(up1);
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdUp0, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&spr_.up0));
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdUp1, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&spr_.up1));

	D3D12_SUBRESOURCE_DATA sd0{img0.GetImages()->pixels, (LONG_PTR)img0.GetImages()->rowPitch, (LONG_PTR)img0.GetImages()->slicePitch};
	D3D12_SUBRESOURCE_DATA sd1{img1.GetImages()->pixels, (LONG_PTR)img1.GetImages()->rowPitch, (LONG_PTR)img1.GetImages()->slicePitch};

	dx.BeginFrame();
	UpdateSubresources(dx.List(), spr_.tex0.Get(), spr_.up0.Get(), 0, 0, 1, &sd0);
	UpdateSubresources(dx.List(), spr_.tex1.Get(), spr_.up1.Get(), 0, 0, 1, &sd1);
	auto b0 = CD3DX12_RESOURCE_BARRIER::Transition(spr_.tex0.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	auto b1 = CD3DX12_RESOURCE_BARRIER::Transition(spr_.tex1.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	dx.List()->ResourceBarrier(1, &b0);
	dx.List()->ResourceBarrier(1, &b1);
	dx.EndFrame();

	D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
	sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sv.Texture2D.MipLevels = 1;
	sv.Format = m0.format;
	dx.Dev()->CreateShaderResourceView(spr_.tex0.Get(), &sv, dx.SRV_CPU(spr_.srvIndex0));
	sv.Format = m1.format;
	dx.Dev()->CreateShaderResourceView(spr_.tex1.Get(), &sv, dx.SRV_CPU(spr_.srvIndex1));

	// ==== RS / PSO（各ブレンド版を作成） ====
	CD3DX12_DESCRIPTOR_RANGE rng;
	rng.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_ROOT_PARAMETER rp[3];
	rp[0].InitAsConstantBufferView(0);
	rp[1].InitAsConstantBufferView(1);
	rp[2].InitAsDescriptorTable(1, &rng, D3D12_SHADER_VISIBILITY_PIXEL);
	CD3DX12_STATIC_SAMPLER_DESC smp(0);
	CD3DX12_ROOT_SIGNATURE_DESC rsd;
	rsd.Init(3, rp, 1, &smp, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> sig, err;
	D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
	dx.Dev()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&spr_.rs));
	auto vs = Compile(gVSObj, "main", "vs_5_0"), ps = Compile(gPSSprite, "main", "ps_5_0");
	D3D12_INPUT_ELEMENT_DESC il[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	// まず共通部分を埋める
	D3D12_GRAPHICS_PIPELINE_STATE_DESC base{};
	base.pRootSignature = spr_.rs.Get();
	base.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	base.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	base.InputLayout = {il, _countof(il)};
	base.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	base.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	base.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	base.DepthStencilState.DepthEnable = FALSE; // 2D想定
	base.SampleMask = UINT_MAX;
	base.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	base.NumRenderTargets = 1;
	base.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	base.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	base.SampleDesc.Count = 1;

	auto makePSO = [&](const D3D12_BLEND_DESC& bdesc, ComPtr<ID3D12PipelineState>& out) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC d = base;
		d.BlendState = bdesc;
		dx.Dev()->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&out));
	};

	// --- 各ブレンド記述子 ---
	// Opaque（ブレンド無効）
	D3D12_BLEND_DESC blendOpaque = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	// Alpha（Srcα, 1-Srcα）
	D3D12_BLEND_DESC blendAlpha{};
	blendAlpha.AlphaToCoverageEnable = FALSE;
	blendAlpha.IndependentBlendEnable = FALSE;
	auto& rtA = blendAlpha.RenderTarget[0];
	rtA.BlendEnable = TRUE;
	rtA.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rtA.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	rtA.BlendOp = D3D12_BLEND_OP_ADD;
	rtA.SrcBlendAlpha = D3D12_BLEND_ONE;
	rtA.DestBlendAlpha = D3D12_BLEND_ZERO;
	rtA.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rtA.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// Add（加算）
	D3D12_BLEND_DESC blendAdd = blendAlpha;
	blendAdd.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	// Subtract（減算）
	D3D12_BLEND_DESC blendSub = blendAlpha;
	blendSub.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;

	// Multiply（乗算）
	D3D12_BLEND_DESC blendMul = blendAlpha;
	blendMul.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
	blendMul.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	blendMul.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

	// --- 各PSOを作成 ---
	makePSO(blendOpaque, spr_.pso);     // 不透明
	makePSO(blendAlpha, spr_.psoAlpha); // アルファ
	makePSO(blendAdd, spr_.psoAdd);     // 加算
	makePSO(blendSub, spr_.psoSub);     // 減算
	makePSO(blendMul, spr_.psoMul);     // 乗算

	return true;
}

// ==== フレーム境界ラッパー ====
void Renderer::BeginFrame(ID3D12GraphicsCommandList* cmd) {
	assert(cmd && srvHeap_);
	ID3D12DescriptorHeap* heaps[] = {srvHeap_.Get()};
	cmd->SetDescriptorHeaps(1, heaps);
}
void Renderer::EndFrame(ID3D12GraphicsCommandList* /*cmd*/) {}
// void Renderer::BeginFrame(WindowDX& dx, bool useBallTex) {
//	// WindowDXのフレーム開始
//	dx.BeginFrame();
//
//	// このフレームの描画を記録
//	Record(dx, useBallTex);
// }
//
// void Renderer::EndFrame(WindowDX& dx) {
//	// WindowDXのフレーム終了＆Present
//	dx.EndFrame();
// }

// ---------------- Sphere ----------------
bool Renderer::InitSphere(WindowDX& dx) {
	constexpr UINT kDiv = 16;
	auto at = [&](UINT la, UINT lo) { return la * (kDiv + 1) + lo; };
	std::vector<Vertex> v((kDiv + 1) * (kDiv + 1));
	std::vector<uint32_t> idx;
	idx.reserve(kDiv * kDiv * 6);
	const float PI = XM_PI, dLon = XM_2PI / kDiv, dLat = PI / kDiv;

	for (UINT la = 0; la <= kDiv; ++la) {
		float lat = -PI / 2 + la * dLat;
		for (UINT lo = 0; lo <= kDiv; ++lo) {
			float lon = lo * dLon;

			// 球面座標から頂点座標を算出
			float px = cosf(lat) * cosf(lon);
			float py = sinf(lat);
			float pz = cosf(lat) * sinf(lon);

			// ★ここを float4 に修正（末尾に 1.0f）
			v[at(la, lo)].pos = XMFLOAT4(px, py, pz, 1.0f);

			// UV座標
			v[at(la, lo)].uv = XMFLOAT2(lon / XM_2PI, 1.0f - (lat + PI / 2) / PI);

			// 法線は位置ベクトルをそのまま正規化して代用
			v[at(la, lo)].normal = XMFLOAT3(px, py, pz);
		}
	}

	// インデックスバッファ構築
	for (UINT la = 0; la < kDiv; ++la)
		for (UINT lo = 0; lo < kDiv; ++lo) {
			uint32_t a = at(la, lo);
			uint32_t b = at(la, lo + 1);
			uint32_t c = at(la + 1, lo + 1);
			uint32_t d = at(la + 1, lo);
			idx.insert(idx.end(), {a, b, d, b, c, d});
		}

	CD3DX12_HEAP_PROPERTIES hpU(D3D12_HEAP_TYPE_UPLOAD);
	auto rdV = CD3DX12_RESOURCE_DESC::Buffer(sizeof(Vertex) * v.size());
	auto rdI = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * idx.size());
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdV, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&sph_.vb));
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdI, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&sph_.ib));
	void* p = nullptr;
	sph_.vb->Map(0, nullptr, &p);
	memcpy(p, v.data(), rdV.Width);
	sph_.vb->Unmap(0, nullptr);
	sph_.ib->Map(0, nullptr, &p);
	memcpy(p, idx.data(), rdI.Width);
	sph_.ib->Unmap(0, nullptr);
	sph_.vbv = {sph_.vb->GetGPUVirtualAddress(), (UINT)rdV.Width, sizeof(Vertex)};
	sph_.ibv = {sph_.ib->GetGPUVirtualAddress(), (UINT)rdI.Width, DXGI_FORMAT_R32_UINT};

	auto rdC = CD3DX12_RESOURCE_DESC::Buffer(256);
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdC, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&sph_.cb));
	auto rdL = CD3DX12_RESOURCE_DESC::Buffer(256);
	dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdL, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&sph_.cbLight));

	// RS / PSO（球は不透明）
	CD3DX12_DESCRIPTOR_RANGE rng;
	rng.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_ROOT_PARAMETER rp[3];
	rp[0].InitAsConstantBufferView(0);
	rp[1].InitAsConstantBufferView(1);
	rp[2].InitAsDescriptorTable(1, &rng, D3D12_SHADER_VISIBILITY_PIXEL);
	CD3DX12_STATIC_SAMPLER_DESC smp(0);
	CD3DX12_ROOT_SIGNATURE_DESC rsd;
	rsd.Init(3, rp, 1, &smp, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> sig, err;
	D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
	dx.Dev()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&sph_.rs));
	auto vs = Compile(gVSLit, "main", "vs_5_0"), ps = Compile(gPSLit, "main", "ps_5_0");
	D3D12_INPUT_ELEMENT_DESC il[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
	d.pRootSignature = sph_.rs.Get();
	d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	d.InputLayout = {il, _countof(il)};
	d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	d.SampleMask = UINT_MAX;
	d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d.NumRenderTargets = 1;
	d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	d.SampleDesc.Count = 1;
	dx.Dev()->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&sph_.pso));

	// ライト初期値
	CBLight l{
	    {0, 1, -1, 0}
    };
	void* map = nullptr;
	sph_.cbLight->Map(0, nullptr, &map);
	memcpy(map, &l, sizeof(l));
	sph_.cbLight->Unmap(0, nullptr);
	return true;
}

// ---------------- Update CB ----------------
void Renderer::UpdateCB(const Camera& cam, const SpriteTf& spr, const SphereTf& sph, const UVParam& uv, const Vector3& lightDir) {
	sprTf_ = spr;
	sphTf_ = sph;
	uv_ = uv;
	lightDir_ = lightDir;

	// OBJ
	{
		CBCommon cb{};
		cb.col = {1, 1, 1, 1};
		XMMATRIX w = XMMatrixIdentity();
		XMMATRIX v = cam.View();
		XMMATRIX p = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.f), 1280.f / 720.f, 0.1f, 100.f);
		cb.mvp = XMMatrixTranspose(w * v * p);
		void* map = nullptr;
		mdl_.cb->Map(0, nullptr, &map);
		memcpy(map, &cb, sizeof(cb));
		mdl_.cb->Unmap(0, nullptr);
	}
	// Sprite
	{
		CBCommon cb{};
		cb.col = sprTf_.c;
		XMMATRIX w = XMMatrixScaling(sprTf_.s.x, sprTf_.s.y, 1) * XMMatrixTranslation(sprTf_.t.x, sprTf_.t.y, 0);
		XMMATRIX v = XMMatrixIdentity();
		XMMATRIX p = XMMatrixOrthographicOffCenterLH(0, 1280.f, 720.f, 0, 0, 1);
		cb.mvp = XMMatrixTranspose(w * v * p);
		void* map = nullptr;
		spr_.cb->Map(0, nullptr, &map);
		memcpy(map, &cb, sizeof(cb));
		spr_.cb->Unmap(0, nullptr);

		CBSpriteUV uvCB{XMMatrixTranspose(XMMatrixScaling(uv_.scale.x, uv_.scale.y, 1) * XMMatrixRotationZ(XMConvertToRadians(uv_.rot)) * XMMatrixTranslation(uv_.trans.x, uv_.trans.y, 0))};
		spr_.cbUv->Map(0, nullptr, &map);
		memcpy(map, &uvCB, sizeof(uvCB));
		spr_.cbUv->Unmap(0, nullptr);
	}
	// Sphere
	{
		CBCommon cb{};
		cb.col = sphTf_.c;
		XMMATRIX w = XMMatrixScaling(sphTf_.s.x, sphTf_.s.y, sphTf_.s.z) * XMMatrixRotationRollPitchYaw(sphTf_.r.x, sphTf_.r.y, sphTf_.r.z) * XMMatrixTranslation(sphTf_.t.x, sphTf_.t.y, sphTf_.t.z);
		XMMATRIX v = cam.View();
		XMMATRIX p = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.f), 1280.f / 720.f, 0.1f, 100.f);
		cb.mvp = XMMatrixTranspose(w * v * p);
		void* map = nullptr;
		sph_.cb->Map(0, nullptr, &map);
		memcpy(map, &cb, sizeof(cb));
		sph_.cb->Unmap(0, nullptr);

		CBLight lcb{
		    {lightDir_.x, lightDir_.y, lightDir_.z, 0}
        };
		sph_.cbLight->Map(0, nullptr, &map);
		memcpy(map, &lcb, sizeof(lcb));
		sph_.cbLight->Unmap(0, nullptr);
	}
}

// ---------------- Record ----------------
void Renderer::Record(WindowDX& dx, bool useBallTex) {
	auto list = dx.List();

	// OBJ
	list->SetPipelineState(mdl_.pso.Get());
	list->SetGraphicsRootSignature(mdl_.rs.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->IASetVertexBuffers(0, 1, &mdl_.vbv);
	list->SetGraphicsRootConstantBufferView(0, mdl_.cb->GetGPUVirtualAddress());
	list->SetGraphicsRootDescriptorTable(1, dx.SRV_GPU(mdl_.srvIndex));
	list->DrawInstanced((UINT)mdl_.v.size(), 1, 0, 0);

	// Sphere（不透明）
	list->SetPipelineState(sph_.pso.Get());
	list->SetGraphicsRootSignature(sph_.rs.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->IASetVertexBuffers(0, 1, &sph_.vbv);
	list->IASetIndexBuffer(&sph_.ibv);
	list->SetGraphicsRootConstantBufferView(0, sph_.cb->GetGPUVirtualAddress());
	list->SetGraphicsRootConstantBufferView(1, sph_.cbLight->GetGPUVirtualAddress());
	// 球のテクスチャは spriteの2枚のうち選択
	list->SetGraphicsRootDescriptorTable(2, dx.SRV_GPU(useBallTex ? spr_.srvIndex1 : spr_.srvIndex0));
	list->DrawIndexedInstanced(16 * 16 * 6, 1, 0, 0, 0);

	// Sprite（ブレンド切替）
	switch (blendMode_) {
	case BlendMode::Alpha:
		list->SetPipelineState(spr_.psoAlpha.Get());
		break;
	case BlendMode::Add:
		list->SetPipelineState(spr_.psoAdd.Get());
		break;
	case BlendMode::Subtract:
		list->SetPipelineState(spr_.psoSub.Get());
		break;
	case BlendMode::Multiply:
		list->SetPipelineState(spr_.psoMul.Get());
		break;
	default:
		list->SetPipelineState(spr_.pso.Get());
		break; // Opaque
	}
	list->SetGraphicsRootSignature(spr_.rs.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->IASetVertexBuffers(0, 1, &spr_.vbv);
	list->IASetIndexBuffer(&spr_.ibv);
	list->SetGraphicsRootConstantBufferView(0, spr_.cb->GetGPUVirtualAddress());
	list->SetGraphicsRootConstantBufferView(1, spr_.cbUv->GetGPUVirtualAddress());
	list->SetGraphicsRootDescriptorTable(2, dx.SRV_GPU(spr_.srvIndex0)); // uvCheckerを貼る
	list->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

//---------------  複数モデル読み込み  -----------------

int Renderer::LoadModel(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const std::string& filepath) {
	ModelEntry entry;
	entry.model = std::make_unique<Model>();
	entry.model->Load(device, cmd, filepath);

	// SRV割り当て
	int index = nextSrvIndex_++;
	entry.model->CreateSrv(device, srvHeap_.Get(), descriptorSize_, index);
	entry.srvGpu = srvHeap_->GetGPUDescriptorHandleForHeapStart();
	entry.srvGpu.ptr += SIZE_T(index) * descriptorSize_;

	// 定数バッファ
	entry.cb = Model::CreateBufferResource(device, size_t(kCBStride) * kCBSlots);
	models_.push_back(std::move(entry));
	return (int)models_.size() - 1;
}

void Renderer::UpdateModelCBWithColor(int handle, const Camera& cam, const Transform& tf, const Vector4& mulColor) {
	auto& m = models_[handle];

	using namespace DirectX;
	XMMATRIX S = XMMatrixScaling(tf.scale.x, tf.scale.y, tf.scale.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(tf.rotate.x, tf.rotate.y, tf.rotate.z);
	XMMATRIX T = XMMatrixTranslation(tf.translate.x, tf.translate.y, tf.translate.z);
	XMMATRIX world = S * R * T;

	XMMATRIX vp = cam.View() * cam.Proj();
	XMMATRIX wvp = XMMatrixTranspose(world * vp);

	CBCommon cb{};
	cb.mvp = wvp;
	cb.col = mulColor; // ★色指定

	void* map = nullptr;
	m.cb->Map(0, nullptr, &map);
	memcpy(map, &cb, sizeof(CBCommon));
	m.cb->Unmap(0, nullptr);
}

void Renderer::DrawModel(int handle, ID3D12GraphicsCommandList* cmd) {
	auto& m = models_[handle];

	assert(pso_ && rs_ && "Renderer not initialized (pso_/rs_ null)");

	// ヒープを必ずバインド（安全策）
	ID3D12DescriptorHeap* heaps[] = {srvHeap_.Get()};
	cmd->SetDescriptorHeaps(1, heaps);

	// 必ず再設定する
	cmd->SetGraphicsRootSignature(rs_.Get());
	cmd->SetPipelineState(pso_.Get());

	cmd->SetGraphicsRootSignature(rs_.Get());
	cmd->SetPipelineState(pso_.Get());
	cmd->IASetVertexBuffers(0, 1, &m.model->GetVBV());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->SetGraphicsRootConstantBufferView(0, m.cb->GetGPUVirtualAddress());

	// ★ここを 1 に
	if (m.srvGpu.ptr) {
		cmd->SetGraphicsRootDescriptorTable(1, m.srvGpu);
	}

	cmd->DrawInstanced((UINT)m.model->GetVertexCount(), 1, 0, 0);
}

// =============================
// ★ Grid（Unity風の格子）
// =============================
bool Renderer::InitGrid(WindowDX& dx, int half, float cell, float y, float phaseX, float phaseZ) {
	grid_.half = half;
	grid_.cell = cell;
	grid_.y = y;
	grid_.phaseX = phaseX;
	grid_.phaseZ = phaseZ;

	using V = VTXL;

	// 矩形境界（固定）
	const float h = static_cast<float>(half) * cell;
	const float minX = -h, maxX = h;
	const float minZ = -h, maxZ = h;

	// ===== 「境界に一致する最初/最後の整数線」を計算 =====
	auto k0 = [](float minB, float phase, float step) -> int {
		// k*step + phase >= minB  となる最小整数 k
		return static_cast<int>(std::ceil((minB - phase) / step));
	};
	auto k1 = [](float maxB, float phase, float step) -> int {
		// k*step + phase <= maxB  となる最大整数 k
		return static_cast<int>(std::floor((maxB - phase) / step));
	};

	const int kx0 = k0(minX, phaseX, cell);
	const int kx1 = k1(maxX, phaseX, cell);
	const int kz0 = k0(minZ, phaseZ, cell);
	const int kz1 = k1(maxZ, phaseZ, cell);

	grid_.zCount = (kx1 >= kx0) ? static_cast<UINT>(kx1 - kx0 + 1) : 0; // 縦線本数
	grid_.xCount = (kz1 >= kz0) ? static_cast<UINT>(kz1 - kz0 + 1) : 0; // 横線本数

	std::vector<V> v;
	v.reserve((grid_.zCount + grid_.xCount) * 2);

	// --- Z方向（縦線）: x = k*cell + phaseX
	for (int k = kx0; k <= kx1; ++k) {
		const float x = k * cell + phaseX;
		v.push_back({
		    {x, y, minZ}
        });
		v.push_back({
		    {x, y, maxZ}
        });
	}
	// --- X方向（横線）: z = k*cell + phaseZ
	for (int k = kz0; k <= kz1; ++k) {
		const float z = k * cell + phaseZ;
		v.push_back({
		    {minX, y, z}
        });
		v.push_back({
		    {maxX, y, z}
        });
	}

	grid_.vcount = static_cast<UINT>(v.size()); // = (zCount+xCount)*2 頂点

	// ===== VB =====
	CD3DX12_HEAP_PROPERTIES hpU(D3D12_HEAP_TYPE_UPLOAD);
	auto rdV = CD3DX12_RESOURCE_DESC::Buffer(sizeof(V) * v.size());
	HR_CHECK(dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdV, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&grid_.vb)));

	void* mapped = nullptr;
	grid_.vb->Map(0, nullptr, &mapped);
	memcpy(mapped, v.data(), rdV.Width);
	grid_.vb->Unmap(0, nullptr);

	grid_.vbv = {grid_.vb->GetGPUVirtualAddress(), static_cast<UINT>(rdV.Width), sizeof(V)};

	// ===== CB =====
	auto rdC = CD3DX12_RESOURCE_DESC::Buffer(256);
	HR_CHECK(dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdC, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&grid_.cb)));

	// ===== RS / PSO (Line) =====
	CD3DX12_ROOT_PARAMETER rp[1];
	rp[0].InitAsConstantBufferView(0); // b0

	CD3DX12_ROOT_SIGNATURE_DESC rsd;
	rsd.Init(_countof(rp), rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> sig, err;
	HR_CHECK(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));
	HR_CHECK(dx.Dev()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&grid_.rs)));

	auto vs = Compile(gVSLine, "main", "vs_5_0");
	auto ps = Compile(gPSLine, "main", "ps_5_0");

	D3D12_INPUT_ELEMENT_DESC il[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

	D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
	d.pRootSignature = grid_.rs.Get();
	d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	d.InputLayout = {il, _countof(il)};
	d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	d.SampleMask = UINT_MAX;
	d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	d.NumRenderTargets = 1;
	d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	d.SampleDesc.Count = 1;

	HR_CHECK(dx.Dev()->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&grid_.pso)));
	return true;
}

void Renderer::DrawGrid(const Camera& cam, ID3D12GraphicsCommandList* cmd, Vector4 mainColor, Vector4 axisColor, int majorStep) {
	if (!grid_.vb)
		return;

	cmd->SetPipelineState(grid_.pso.Get());
	cmd->SetGraphicsRootSignature(grid_.rs.Get());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	cmd->IASetVertexBuffers(0, 1, &grid_.vbv);

	auto updateCB = [&](const Vector4& col) {
		struct {
			DirectX::XMMATRIX mvp;
			Vector4 col;
		} cb{};
		const auto w = DirectX::XMMatrixIdentity();
		const auto vp = cam.View() * cam.Proj();
		cb.mvp = DirectX::XMMatrixTranspose(w * vp);
		cb.col = col;

		void* map = nullptr;
		grid_.cb->Map(0, nullptr, &map);
		memcpy(map, &cb, sizeof(cb));
		grid_.cb->Unmap(0, nullptr);
		cmd->SetGraphicsRootConstantBufferView(0, grid_.cb->GetGPUVirtualAddress());
	};
	auto drawRange = [&](UINT start, UINT count, const Vector4& col) {
		updateCB(col);
		cmd->DrawInstanced(count, 1, start, 0);
	};

	// 配列配置：先に Z群(縦線) 2頂点×zCount、続けて X群(横線) 2頂点×xCount
	const UINT startZ = 0;
	const UINT startX = grid_.zCount * 2;

	// まず全線をメイン色で
	drawRange(0, grid_.vcount, mainColor);

	if (majorStep <= 0)
		majorStep = 5;

	// 強調：Z群
	for (UINT i = 0; i < grid_.zCount; ++i) {
		if ((int)i % majorStep)
			continue;
		drawRange(startZ + i * 2, 2, axisColor);
	}
	// 強調：X群
	for (UINT i = 0; i < grid_.xCount; ++i) {
		if ((int)i % majorStep)
			continue;
		drawRange(startX + i * 2, 2, axisColor);
	}
}

// スロット指定版
void Renderer::UpdateModelCBWithColorAt(int handle, size_t slot, const Camera& cam, const Transform& tf, const Vector4& mulColor) {


	auto& m = models_[handle];
	using namespace DirectX;

	if (!m.cb || slot >= kCBSlots)
		return; // ← 1024 → kCBSlots に変更


	XMMATRIX S = XMMatrixScaling(tf.scale.x, tf.scale.y, tf.scale.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(tf.rotate.x, tf.rotate.y, tf.rotate.z);
	XMMATRIX T = XMMatrixTranslation(tf.translate.x, tf.translate.y, tf.translate.z);
	XMMATRIX wvp = XMMatrixTranspose(S * R * T * (cam.View() * cam.Proj()));

	CBCommon cb{};
	cb.mvp = wvp;
	cb.col = mulColor;

	uint8_t* base = nullptr;
	m.cb->Map(0, nullptr, reinterpret_cast<void**>(&base));
	std::memcpy(base + slot * kCBStride, &cb, sizeof(CBCommon));
	m.cb->Unmap(0, nullptr);
}

void Renderer::DrawModelAt(int handle, ID3D12GraphicsCommandList* cmd, size_t slot) {
	auto& m = models_[handle];
	if (!m.cb || slot >= kCBSlots)
		return;

	ID3D12DescriptorHeap* heaps[] = {srvHeap_.Get()};
	cmd->SetDescriptorHeaps(1, heaps);

	cmd->SetGraphicsRootSignature(rs_.Get());
	cmd->SetPipelineState(pso_.Get());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &m.model->GetVBV());

	D3D12_GPU_VIRTUAL_ADDRESS gpu = m.cb->GetGPUVirtualAddress() + (slot * kCBStride);
	cmd->SetGraphicsRootConstantBufferView(0, gpu);

	if (m.srvGpu.ptr)
		cmd->SetGraphicsRootDescriptorTable(1, m.srvGpu);

	cmd->DrawInstanced((UINT)m.model->GetVertexCount(), 1, 0, 0);
}

bool Renderer::InitNeonFramePSO(ID3D12Device* device, ID3D12RootSignature* rs) {
	auto vs = Compile(gVSObj, "main", "vs_5_0");       // 既存のモデルVSを流用
	auto ps = Compile(gPSNeonFrame, "main", "ps_5_0"); // ネオン枠PS

	D3D12_INPUT_ELEMENT_DESC il[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	// 加算ブレンド（ONE, ONE）。深度は通常ON（手前/奥の判定は維持）
	D3D12_BLEND_DESC blend{};
	auto& rt = blend.RenderTarget[0];
	rt.BlendEnable = TRUE;
	rt.SrcBlend = D3D12_BLEND_ONE;
	rt.DestBlend = D3D12_BLEND_ONE;
	rt.BlendOp = D3D12_BLEND_OP_ADD;
	rt.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt.DestBlendAlpha = D3D12_BLEND_ONE;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
	d.pRootSignature = rs;
	d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	d.InputLayout = {il, _countof(il)};
	d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	d.BlendState = blend;
	d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // Z有効
	d.SampleMask = UINT_MAX;
	d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d.NumRenderTargets = 1;
	d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	d.SampleDesc.Count = 1;

	return SUCCEEDED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&psoNeonFrame_)));
}

size_t Renderer::MaxModelCBSlots() const { return kCBSlots; }
size_t Renderer::CBStride() const { return kCBStride; }

void Renderer::DrawModelNeonFrame(int handle, ID3D12GraphicsCommandList* cmd) {
	auto& m = models_[handle];
	if (!m.model)
		return;

	// 必要なヒープを再バインド（安全策）
	ID3D12DescriptorHeap* heaps[] = {srvHeap_.Get()};
	cmd->SetDescriptorHeaps(1, heaps);

	cmd->SetGraphicsRootSignature(rs_.Get());
	cmd->SetPipelineState(psoNeonFrame_.Get());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &m.model->GetVBV());

	// C0（mvp, col）は既存 UpdateModelCBWithColor()/At() で詰めたものを使用
	cmd->SetGraphicsRootConstantBufferView(0, m.cb->GetGPUVirtualAddress());

	// SRV不要（テクスチャは使わない）が、RSが SRV テーブルを持っているため 0 を入れてもOK
	// （SRV未使用のままでも動きます）

	cmd->DrawInstanced(m.model->GetVertexCount(), 1, 0, 0);
}

void Renderer::DrawModelNeonFrameAt(int handle, ID3D12GraphicsCommandList* cmd, size_t slot) {
	auto& m = models_[handle];
	if (!m.model)
		return;

	ID3D12DescriptorHeap* heaps[] = {srvHeap_.Get()};
	cmd->SetDescriptorHeaps(1, heaps);

	cmd->SetGraphicsRootSignature(rs_.Get());
	cmd->SetPipelineState(psoNeonFrame_.Get());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &m.model->GetVBV());

	// ★ 安全ラップ
	const size_t safeSlot = (kCBSlots == 0) ? 0 : (slot % kCBSlots);
	const D3D12_GPU_VIRTUAL_ADDRESS gpu = m.cb->GetGPUVirtualAddress() + static_cast<UINT64>(safeSlot) * kCBStride;

	cmd->SetGraphicsRootConstantBufferView(0, gpu);
	cmd->DrawInstanced((UINT)m.model->GetVertexCount(), 1, 0, 0);
}

#include "d3dx12.h" // ← 忘れずに

bool Renderer::InitLaserPSO(ID3D12Device* device, ID3D12RootSignature* rs) {
	// シェーダを再コンパイル（OBJ用と同じ）
	Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob;
	vsBlob = Compile(
	    R"(
        cbuffer CB : register(b0) {
            matrix mvp;
            float4 col;
        };
        struct VSIn {
            float3 pos : POSITION;
            float2 uv  : TEXCOORD;
        };
        struct PSIn {
            float4 svpos : SV_POSITION;
            float2 uv : TEXCOORD;
        };
        PSIn main(VSIn vin) {
            PSIn vout;
            vout.svpos = mul(float4(vin.pos,1), mvp);
            vout.uv = vin.uv;
            return vout;
        })",
	    "main", "vs_5_0");

	psBlob = Compile(
	    R"(
        cbuffer CB : register(b0) {
            matrix mvp;
            float4 col;
        };
        float4 main() : SV_TARGET {
            return col;
        })",
	    "main", "ps_5_0");

	// PSO構築
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rs;

	D3D12_INPUT_ELEMENT_DESC layout[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
	desc.InputLayout = {layout, _countof(layout)};
	desc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
	desc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};

	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	// 加算ブレンド（光る線っぽく）
	D3D12_BLEND_DESC blend{};
	blend.RenderTarget[0].BlendEnable = TRUE;
	blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.BlendState = blend;

	// ===== 深度無効化 =====
	D3D12_DEPTH_STENCIL_DESC depth{};
	depth.DepthEnable = FALSE;
	depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depth.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.DepthStencilState = depth;

	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;

	HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&psoLaser_));
	return SUCCEEDED(hr);
}

} // namespace Engine
