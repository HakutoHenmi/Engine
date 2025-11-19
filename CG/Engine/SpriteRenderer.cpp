#include "SpriteRenderer.h"
#include <cassert>
#include <d3dcompiler.h>
#include <d3dx12.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace Engine {

using namespace DirectX;
using Microsoft::WRL::ComPtr;

SpriteRenderer* SpriteRenderer::sInstance_ = nullptr;

static ComPtr<ID3DBlob> CompileSpriteShader(const char* src, const char* entry, const char* target) {
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	ComPtr<ID3DBlob> blob, err;
	HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target, flags, 0, &blob, &err);
	if (FAILED(hr)) {
		if (err) {
			::MessageBoxA(nullptr, (const char*)err->GetBufferPointer(), "Sprite HLSL Error", MB_OK);
		}
		std::abort();
	}
	return blob;
}

// シンプルなスプライト用HLSL
static const char* gVSSprite2D = R"(
cbuffer C0 : register(b0)
{
    float4x4 mvp;
    float4 col;
    float4 srcUV; // u0,v0,u1,v1
};

struct VSIn {
    float4 pos : POSITION;
    float2 uv  : TEXCOORD;
};

struct VSOut {
    float4 svpos : SV_Position;
    float2 uv    : TEXCOORD;
};

VSOut main(VSIn i) {
    VSOut o;
    o.svpos = mul(i.pos, mvp);
    o.uv    = i.uv;
    return o;
}
)";

static const char* gPSSprite2D = R"(
cbuffer C0 : register(b0)
{
    float4x4 mvp;
    float4 col;
    float4 srcUV; // u0,v0,u1,v1
};

Texture2D T : register(t0);
SamplerState S : register(s0);

float4 main(float4 svpos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    // uv(0～1)をsrcUV範囲にリマップ
    float2 uv2 = float2(
        lerp(srcUV.x, srcUV.z, uv.x),
        lerp(srcUV.y, srcUV.w, uv.y)
    );
    float4 tex = T.Sample(S, uv2);
    return tex * col;
}
)";

bool SpriteRenderer::Initialize(WindowDX* dx) {
	dx_ = dx;
	sInstance_ = this;
	return CreateResources_();
}

void SpriteRenderer::Shutdown() {
	vb_.Reset();
	ib_.Reset();
	cb_.Reset();
	rs_.Reset();
	pso_.Reset();
	vbv_ = {};
	ibv_ = {};
	dx_ = nullptr;
	sInstance_ = nullptr;
}

SpriteRenderer* SpriteRenderer::Instance() { return sInstance_; }

bool SpriteRenderer::CreateResources_() {
	assert(dx_);
	auto* dev = dx_->Dev();

	// ==== Quad VB/IB (0,0)-(1,1) ====
	Vertex v[4] = {
	    {{0, 0, 0, 1}, {0, 0}},
	    {{1, 0, 0, 1}, {1, 0}},
	    {{0, 1, 0, 1}, {0, 1}},
	    {{1, 1, 0, 1}, {1, 1}},
	};
	uint32_t idx[6] = {0, 1, 2, 1, 3, 2};

	CD3DX12_HEAP_PROPERTIES hpU(D3D12_HEAP_TYPE_UPLOAD);
	auto rdV = CD3DX12_RESOURCE_DESC::Buffer(sizeof(v));
	auto rdI = CD3DX12_RESOURCE_DESC::Buffer(sizeof(idx));

	HRESULT hr = dev->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdV, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vb_));
	if (FAILED(hr))
		return false;

	hr = dev->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdI, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ib_));
	if (FAILED(hr))
		return false;

	void* p = nullptr;
	vb_->Map(0, nullptr, &p);
	memcpy(p, v, sizeof(v));
	vb_->Unmap(0, nullptr);

	ib_->Map(0, nullptr, &p);
	memcpy(p, idx, sizeof(idx));
	ib_->Unmap(0, nullptr);

	vbv_.BufferLocation = vb_->GetGPUVirtualAddress();
	vbv_.SizeInBytes = sizeof(v);
	vbv_.StrideInBytes = sizeof(Vertex);

	ibv_.BufferLocation = ib_->GetGPUVirtualAddress();
	ibv_.SizeInBytes = sizeof(idx);
	ibv_.Format = DXGI_FORMAT_R32_UINT;

	// CB
	auto rdC = CD3DX12_RESOURCE_DESC::Buffer(256); // 1つだけ使う
	hr = dev->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdC, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cb_));
	if (FAILED(hr))
		return false;

	// RootSig
	CD3DX12_DESCRIPTOR_RANGE rng;
	rng.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

	CD3DX12_ROOT_PARAMETER rp[2];
	rp[0].InitAsConstantBufferView(0); // b0
	rp[1].InitAsDescriptorTable(1, &rng, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC smp(0);
	smp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	smp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	smp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	smp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

	CD3DX12_ROOT_SIGNATURE_DESC rsd;
	rsd.Init(2, rp, 1, &smp, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> sig, err;
	hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
	if (FAILED(hr)) {
		if (err) {
			::MessageBoxA(nullptr, (const char*)err->GetBufferPointer(), "Sprite RS Error", MB_OK);
		}
		return false;
	}

	hr = dev->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rs_));
	if (FAILED(hr))
		return false;

	// PSO
	auto vs = CompileSpriteShader(gVSSprite2D, "main", "vs_5_0");
	auto ps = CompileSpriteShader(gPSSprite2D, "main", "ps_5_0");

	D3D12_INPUT_ELEMENT_DESC il[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
	d.pRootSignature = rs_.Get();
	d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	d.InputLayout = {il, _countof(il)};
	d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	d.BlendState.RenderTarget[0].BlendEnable = TRUE;
	d.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	d.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	d.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	d.DepthStencilState.DepthEnable = FALSE; // 2D想定
	d.SampleMask = UINT_MAX;
	d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d.NumRenderTargets = 1;
	d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	d.SampleDesc.Count = 1;

	hr = dev->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso_));
	if (FAILED(hr))
		return false;

	return true;
}

void SpriteRenderer::Begin() { batching_ = true; }

void SpriteRenderer::End() { batching_ = false; }

void SpriteRenderer::DrawSprite(const Sprite& sprite) {
	// 即時描画版
	bool prev = batching_;
	batching_ = true;
	DrawInternal_(sprite);
	batching_ = prev;
}

void SpriteRenderer::Draw(const Sprite& sprite) {
	// バッチ中の1枚
	DrawInternal_(sprite);
}

void SpriteRenderer::DrawInternal_(const Sprite& sprite) {
	if (!dx_)
		return;
	auto* cmd = dx_->List();

	if (!TextureManager::Instance().IsValid(sprite.tex))
		return;

	// パイプライン設定
	cmd->SetPipelineState(pso_.Get());
	cmd->SetGraphicsRootSignature(rs_.Get());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &vbv_);
	cmd->IASetIndexBuffer(&ibv_);

	// CB作成
	CBCb cb{};
	// 画面サイズはひとまず固定値（既存コードに合わせて 1280x720）
	const float screenW = 1280.0f;
	const float screenH = 720.0f;

	// ワールド：0〜1のクワッドを size に拡大して pos に平行移動
	XMMATRIX S = XMMatrixScaling(sprite.size.x, sprite.size.y, 1.0f);
	XMMATRIX T = XMMatrixTranslation(sprite.pos.x, sprite.pos.y, 0.0f);
	XMMATRIX w = S * T;

	// ビューは単位、プロジェクションは左上(0,0)、右下(1280,720)の正射影
	XMMATRIX v = XMMatrixIdentity();
	XMMATRIX p = XMMatrixOrthographicOffCenterLH(0.0f, screenW, screenH, 0.0f, 0.0f, 1.0f);

	XMMATRIX mvp = XMMatrixTranspose(w * v * p);
	XMStoreFloat4x4(&cb.mvp, mvp);
	cb.color = sprite.color;
	cb.srcUV = sprite.srcUV;

	void* mapped = nullptr;
	cb_->Map(0, nullptr, &mapped);
	memcpy(mapped, &cb, sizeof(cb));
	cb_->Unmap(0, nullptr);

	cmd->SetGraphicsRootConstantBufferView(0, cb_->GetGPUVirtualAddress());

	// テクスチャSRV
	auto gpu = TextureManager::Instance().GetGPU(sprite.tex);
	cmd->SetGraphicsRootDescriptorTable(1, gpu);

	cmd->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

} // namespace Engine
