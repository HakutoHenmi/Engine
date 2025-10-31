// AnimationEditor.cpp（全置換）
#include "AnimationEditor.h"
#include "AnimationJson.h" // 使わないなら消してOK
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <imgui.h>
#include <string>

namespace Engine {

// ---- ここでユーティリティ（内部関数） ----
template<class T> static bool AddKeyAt(AnimTrack<T>& tr, float t, const T& v) {
	auto it = std::lower_bound(tr.keys.begin(), tr.keys.end(), t, [](const AnimKeyframe<T>& k, float tt) { return k.t < tt; });
	if (it != tr.keys.end() && std::fabs(it->t - t) < 1e-4f) {
		it->v = v;
		return true;
	}
	tr.keys.insert(it, AnimKeyframe<T>{t, v, Interp::Linear});
	return true;
}

// ---- ★ メンバー関数として定義（Engine::AnimationEditorPanel::Draw） ----
void AnimationEditorPanel::Draw(AnimationPlayer& player) {
	ImGui::Begin("Animation Editor");

	if (!clip_) {
		ImGui::TextUnformatted("No clip.");
		ImGui::End();
		return;
	}

	// クリップ基本情報
	ImGui::InputText("Name", clip_->name.data(), (int)clip_->name.capacity() + 1);
	ImGui::DragFloat("Length (sec)", &clip_->length, 0.01f, 0.1f, 600.0f);
	ImGui::Checkbox("Loop", &clip_->loop);
	ImGui::DragFloat("Zoom", &timelineZoom_, 0.01f, 0.1f, 10.0f);

	// 再生コントロール
	auto& st = player.State();
	if (ImGui::Button(st.playing ? "Pause" : "Play"))
		st.playing = !st.playing;
	ImGui::SameLine();
	if (ImGui::Button("Stop")) {
		st.playing = false;
		st.time = 0.0f;
	}
	ImGui::SameLine();
	ImGui::Checkbox("PingPong", &st.pingpong);
	ImGui::SameLine();
	ImGui::DragFloat("Speed", &st.speed, 0.01f, -4.f, 4.f);

	// スクラブバー
	ImGui::SliderFloat("Time", &st.time, 0.f, clip_->length);
	if (!st.playing)
		cursorSec_ = st.time;

	// 現在値（キー追加の初期値に使う）
	auto cur = player.Sample();

	// 行UI: 有効/無効 + その場でキー追加
	auto row = [this](const char* label, AnimTrack<float>& tr, float currentValue) {
		ImGui::Checkbox(label, &tr.enabled);
		ImGui::SameLine();
		if (ImGui::Button((std::string("+##") + label).c_str())) {
			AddKeyAt(tr, cursorSec_, currentValue);
		}
		ImGui::SameLine();
		ImGui::Text("keys:%d", (int)tr.keys.size());
	};

	if (ImGui::CollapsingHeader("Position", ImGuiTreeNodeFlags_DefaultOpen)) {
		row("posX", clip_->posX, cur.pos[0]);
		row("posY", clip_->posY, cur.pos[1]);
		row("posZ", clip_->posZ, cur.pos[2]);
	}
	if (ImGui::CollapsingHeader("Rotation(Euler)", ImGuiTreeNodeFlags_DefaultOpen)) {
		row("rotX", clip_->rotX, cur.rot[0]);
		row("rotY", clip_->rotY, cur.rot[1]);
		row("rotZ", clip_->rotZ, cur.rot[2]);
	}
	if (ImGui::CollapsingHeader("Scale", ImGuiTreeNodeFlags_DefaultOpen)) {
		row("sclX", clip_->sclX, cur.scl[0]);
		row("sclY", clip_->sclY, cur.scl[1]);
		row("sclZ", clip_->sclZ, cur.scl[2]);
	}
	if (ImGui::CollapsingHeader("Color", ImGuiTreeNodeFlags_DefaultOpen)) {
		row("colR", clip_->colR, cur.col[0]);
		row("colG", clip_->colG, cur.col[1]);
		row("colB", clip_->colB, cur.col[2]);
		row("colA", clip_->colA, cur.col[3]);
	}

	if (ImGui::Button("Save .anim.json")) {
		// JSONへ変換
		nlohmann::json j = *clip_;
		std::string filePath = "Resources/Anim/" + clip_->name + ".anim.json";

		// 書き出し
		std::ofstream ofs(filePath);
		if (ofs.is_open()) {
			ofs << j.dump(2); // 整形して保存
			ofs.close();
			OutputDebugStringA(("Saved: " + filePath + "\n").c_str());
		} else {
			OutputDebugStringA(("Failed to save: " + filePath + "\n").c_str());
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Reload")) {
		std::string filePath = "Resources/Anim/" + clip_->name + ".anim.json";

		// 読み込み
		std::ifstream ifs(filePath);
		if (ifs.is_open()) {
			nlohmann::json j;
			ifs >> j;
			ifs.close();

			*clip_ = j.get<AnimClip>();
			player.SetClip(clip_);
			OutputDebugStringA(("Reloaded: " + filePath + "\n").c_str());
		} else {
			OutputDebugStringA(("Failed to open: " + filePath + "\n").c_str());
		}
	}

	ImGui::End();
}

} // namespace Engine
