#include "Stage.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <fstream>
#include <sstream>

namespace Engine {

//---------------------------------------------
// CSVローダ：BOM対応・空白/空要素無視（空セルは "0"）
//---------------------------------------------
std::vector<std::vector<std::string>> Stage::LoadCsvRobust(const std::string& path) {
	std::ifstream file(path);
	assert(file.is_open() && "CSV not found");

	auto StripBOM = [](std::string& s) {
		if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
			s.erase(0, 3);
	};

	std::vector<std::vector<std::string>> data;
	std::string line;
	bool first = true;

	while (std::getline(file, line)) {
		line.erase(std::remove_if(line.begin(), line.end(), [](unsigned char c) { return c == '\r' || c == '\n'; }), line.end());
		if (line.empty())
			continue;
		if (first) {
			StripBOM(line);
			first = false;
		}

		std::vector<std::string> row;
		row.reserve(256);
		std::string cell;
		for (size_t i = 0, start = 0; i <= line.size(); ++i) {
			if (i == line.size() || line[i] == ',') {
				cell = line.substr(start, i - start);
				cell.erase(std::remove_if(cell.begin(), cell.end(), ::isspace), cell.end());
				row.push_back(cell.empty() ? "0" : cell);
				start = i + 1;
			}
		}
		if (!row.empty())
			data.push_back(std::move(row));
	}
	return data;
}

//---------------------------------------------
// グリッド→ワールド座標（セル中心）
//---------------------------------------------
inline void Stage::gridToWorld(int gx, int gz, float& outX, float& outZ) const {
	const float fx = static_cast<float>(gx) + 0.5f;
	const float fz = static_cast<float>(gz) + 0.5f;

	if (anchor_ == GridAnchor::Center) {
		outX = (fx - maxCols_ * 0.5f) * pitchX_;
		outZ = (fz - rows_ * 0.5f) * pitchZ_;
	} else {
		outX = fx * pitchX_;
		outZ = fz * pitchZ_;
	}
}

void Stage::SetPlayerEffect(const Vector3& pos, float radius, float riseLerp, float fallLerp) {
	playerPos_ = pos;
	glowRadius_ = (std::max)(0.01f, radius);
	glowRise_ = std::clamp(riseLerp, 0.0f, 1.0f);
	glowFall_ = std::clamp(fallLerp, 0.0f, 1.0f);
}

//---------------------------------------------
// 初期化
//---------------------------------------------
void Stage::Initialize(
    Renderer& renderer, WindowDX& dx, const Camera& camera, const std::string& mapCsvPath, const std::string& angleCsvPath, float tileW, float tileD, float tileH, GridAnchor anchor,
    ModelOrigin modelOrigin, float gapX, float gapZ) {
	camera_ = &camera;
	tileWidth_ = tileW;
	tileDepth_ = tileD;
	tileHeight_ = tileH;
	anchor_ = anchor;
	modelOrigin_ = modelOrigin;
	gapX_ = gapX;
	gapZ_ = gapZ;
	pitchX_ = tileWidth_ + gapX_;
	pitchZ_ = tileDepth_ + gapZ_;

	tiles_.clear();
	wallAABBs_.clear();
	prismAABBs_.clear();
	prismAngles_.clear();

	// モデル読み込み
	wallModelHandle_ = renderer.LoadModel(dx.Dev(), dx.List(), "Resources/cube/cube.obj");
	prismModelHandle_ = renderer.LoadModel(dx.Dev(), dx.List(), "Resources/cube/cube.obj");
	sensorModelHandle_ = renderer.LoadModel(dx.Dev(), dx.List(), "Resources/cube/cube.obj");

	// CSV読込
	const auto mapData = LoadCsvRobust(mapCsvPath);
	const auto angleData = LoadCsvRobust(angleCsvPath);
	rows_ = static_cast<int>(mapData.size());
	maxCols_ = 0;
	for (auto& r : mapData)
		maxCols_ = std::max<int>(maxCols_, (int)r.size());

	//---------------------------------------------
	// ブロック（壁/プリズム/リフト）配置
	//---------------------------------------------
	for (int z = 0; z < rows_; ++z) {
		for (int x = 0; x < (int)mapData[z].size(); ++x) {
			const std::string& cell = mapData[z][x];
			const std::string angStr = (z < (int)angleData.size() && x < (int)angleData[z].size()) ? angleData[z][x] : "0";
			if (cell != "1" && cell != "2" && cell != "3")
				continue;

			// ---- ★ 壁ブロックだけ複数段積む設定 ----
			const int wallStackCount = 5; // 壁の段数
			const bool isWall = (cell == "1");
			const bool isPrism = (cell == "2");
			const bool isLift = (cell == "3");

			// 壁のみ 5 段、それ以外は 1 段
			const int stackCount = isWall ? wallStackCount : 1;

			for (int h = 0; h < stackCount; ++h) {
				Tile t{};
				t.isWall = isWall;
				t.isPrism = isPrism;
				t.isLift = isLift;
				t.prismAngle = std::stof(angStr);
				t.modelHandle = t.isWall ? wallModelHandle_ : (t.isPrism ? prismModelHandle_ : sensorModelHandle_);

				constexpr float kSrcCube = 2.0f;
				const float sizeX = tileWidth_ - gapX_;
				const float sizeZ = tileDepth_ - gapZ_;
				const float sizeY = tileHeight_;
				t.transform.scale = {sizeX / kSrcCube, sizeY / kSrcCube, sizeZ / kSrcCube};

				float cx = 0, cz = 0;
				gridToWorld(x, z, cx, cz);

				// ★ 段ごとに高さをずらす（壁だけ h>0 で積み上がる）
				const float baseY = sizeY * 0.5f + h * sizeY;
				t.transform.translate = {cx, baseY, cz};

				if (t.isPrism) {
					t.transform.rotate = {0, XMConvertToRadians(t.prismAngle), 0};
				}

				// AABB
				Vector3 c = t.transform.translate;
				Vector3 half{sizeX * 0.5f, sizeY * 0.5f, sizeZ * 0.5f};
				const float e = 0.02f;
				t.aabb.min = {c.x - half.x - e, c.y - half.y - e, c.z - half.z - e};
				t.aabb.max = {c.x + half.x + e, c.y + half.y + e, c.z + half.z + e};

				if (t.isWall || t.isLift) {
					wallAABBs_.push_back(t.aabb); // ← リフトは1段なので1回だけ入る
				}
				if (t.isPrism) {
					prismAABBs_.push_back(t.aabb);
					prismAngles_.push_back(t.prismAngle);
				}

				t.baseY = t.transform.translate.y;
				tiles_.push_back(std::move(t));
			}
		}
	}

	//-------------------------------------
	// ★ 床タイル：CSV上の "1" 以外のマスに敷く
	//-------------------------------------
	{
		constexpr float kSrc = 2.0f;
		const float groundThick = (std::max)(0.05f, tileHeight_ * 0.25f);
		const float groundY = -0.20f; // 少し高めに（沈み防止）

		for (int z = 0; z < rows_; ++z) {
			for (int x = 0; x < maxCols_; ++x) {
				const std::string& cell = (x < (int)mapData[z].size()) ? mapData[z][x] : "0";
				if (cell == "1")
					continue; // 壁の下には敷かない

				Tile g{};
				g.isGround = true;
				g.modelHandle = wallModelHandle_; // cube.obj 流用

				const float sx = tileWidth_ - gapX_;
				const float sz = tileDepth_ - gapZ_;
				g.transform.scale = {sx / kSrc, groundThick / kSrc, sz / kSrc};

				float cx = 0, cz = 0;
				gridToWorld(x, z, cx, cz);
				g.transform.translate = {cx, groundY + groundThick * 0.5f, cz};

				tiles_.push_back(std::move(g));
			}
		}
	}
}

//---------------------------------------------
void Stage::Update() {
	for (auto& t : tiles_) {
		if (!t.isLift)
			continue;
		float targetY = t.isUp ? (t.baseY - 2.0f) : t.baseY;
		float diff = targetY - t.transform.translate.y;
		t.transform.translate.y += diff * 0.1f;
		t.activeCollision = !t.isUp;
	}

	// ★ 追加：床タイルの発光ターゲットを距離で決め、なめらかに追従
	for (auto& t : tiles_) {
		if (!t.isGround)
			continue;

		const float dx = t.transform.translate.x - playerPos_.x;
		const float dz = t.transform.translate.z - playerPos_.z;
		const float dist = std::sqrt(dx * dx + dz * dz);

		// 半径内で 1→外側で 0 へ線形に減衰
		float target = 1.0f - (dist / glowRadius_);
		if (target < 0.0f)
			target = 0.0f;
		if (target > 1.0f)
			target = 1.0f;

		// 近づいた時は素早く上げ、離れた時はゆっくり戻す
		const float k = (target > t.glow) ? glowRise_ : glowFall_;
		t.glow += (target - t.glow) * k; // 逐次LERP
	}
}

//---------------------------------------------
void Stage::TriggerLiftBlocks() {
	for (auto& t : tiles_)
		if (t.isLift)
			t.isUp = !t.isUp;
}

//---------------------------------------------
std::vector<AABB> Stage::GetWallsDynamic() const {
	std::vector<AABB> out;
	out.reserve(tiles_.size());
	for (const auto& t : tiles_) {
		if (t.isWall || (t.isLift && t.activeCollision))
			out.push_back(t.aabb);
	}
	return out;
}

//---------------------------------------------
// 描画
//---------------------------------------------
void Stage::Draw(Renderer& renderer, ID3D12GraphicsCommandList* cmd) {
	if (!camera_)
		return;

	size_t slot = 0;
	const size_t maxSlots = renderer.MaxModelCBSlots(); // ← ここから取得

	// --- 通常ブロック ---
	for (const auto& t : tiles_) {
		if (t.modelHandle < 0)
			continue;
		if (!(t.isWall || t.isPrism || t.isLift))
			continue;

		const Vector4 col = t.isPrism ? Vector4{1, 0, 0, 1} : (t.isLift ? Vector4{0, 1, 0, 1} : Vector4{1, 1, 1, 1});

		const size_t s = (maxSlots == 0) ? 0 : (slot % maxSlots);
		renderer.UpdateModelCBWithColorAt(t.modelHandle, s, *camera_, t.transform, col);
		renderer.DrawModelAt(t.modelHandle, cmd, s);
		++slot;

		if (t.isWall) {
			renderer.UpdateModelCBWithColor(t.modelHandle, *camera_, t.transform, {0.20f, 0.75f, 1.0f, 1.0f});
			renderer.DrawModelNeonFrame(t.modelHandle, cmd);
		}
	}

	// --- 床（CSV=1以外）→ ネオン（距離に応じて紫へ） ---
	const Vector4 baseCyan = {0.20f, 0.80f, 1.00f, 1.0f};
	const Vector4 purple = {0.85f, 0.35f, 1.00f, 1.0f};

	for (const auto& t : tiles_) {
		if (t.modelHandle < 0 || !t.isGround)
			continue;

		// 0..1 の発光値で色をLerp
		const float f = std::clamp(t.glow, 0.0f, 1.0f);
		const Vector4 col = {baseCyan.x + (purple.x - baseCyan.x) * f, baseCyan.y + (purple.y - baseCyan.y) * f, baseCyan.z + (purple.z - baseCyan.z) * f, 1.0f};

		const size_t s = (maxSlots == 0) ? 0 : (slot % maxSlots);
		renderer.UpdateModelCBWithColorAt(t.modelHandle, s, *camera_, t.transform, col);
		renderer.DrawModelNeonFrameAt(t.modelHandle, cmd, s);
		++slot;
	}
}

} // namespace Engine
