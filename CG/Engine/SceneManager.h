#pragma once
// ==================================================
// SceneManager.h
//   ・シーン登録（名前→Factory）
//   ・現在シーンの保持/切替/更新/描画
//   ・即時切替と「次フレーム切替（リクエスト）」に対応
// ==================================================
#include "IScene.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "WindowDX.h"

namespace Engine {

class SceneManager {
public:
	// シーンを生成するファクトリ（ゲーム側で登録）
	using Factory = std::function<std::unique_ptr<IScene>()>;

public:
	SceneManager() = default;
	~SceneManager() = default;

	// シーンの登録：同名があれば上書き
	void Register(const std::string& name, Factory factory);

	// 即時にシーンを切り替える（Initialize() を呼ぶ）
	// 返り値: true=成功 / false=未登録
	bool Change(const std::string& name);

	// 次フレームでの切替を予約（安全にフレーム境界で切替したい時）
	void RequestChange(const std::string& name);

	// 現在シーンの Update / Draw を呼ぶ
	// ・Update内で IsEnd()==true になったら Next() へ遷移
	// ・RequestChange が入っている場合も境界で遷移
	void Update();
	void Draw();

	// 現在のシーン取得（null 可）／名前取得
	IScene* Current() const { return current_.get(); }
	const std::string& CurrentName() const { return currentName_; }

	void SetDX(WindowDX* dx) { dx_ = dx; }

	// 全登録/状態クリア（終了時などに）
	void Clear();

private:
	std::unordered_map<std::string, Factory> factories_;
	std::unique_ptr<IScene> current_;
	std::string currentName_;

	// フレーム境界で反映するための保留先
	std::string pendingNext_;

	WindowDX* dx_ = nullptr;

};

} // namespace Engine
