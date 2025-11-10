// SceneManager.cpp
#include "SceneManager.h"

namespace Engine {

void SceneManager::Register(const std::string& name, Factory factory) { factories_[name] = std::move(factory); }

bool SceneManager::Change(const std::string& name) {
	auto it = factories_.find(name);
	if (it == factories_.end()) {
		// 未登録
		return false;
	}
	// 生成→Initialize→着座
	current_ = it->second();
	currentName_ = name;
	if (current_) {
		current_->Initialize(dx_);
	}
	// 即時切替のため保留はクリア
	pendingNext_.clear();
	return true;
}

void SceneManager::RequestChange(const std::string& name) {
	pendingNext_ = name; // 次の Update/Draw の境界で Change() を実施
}

void SceneManager::Update() {
	// フレーム開始時に、もし保留の遷移があれば反映
	if (!pendingNext_.empty()) {
		Change(pendingNext_);
		// Change 内で pendingNext_ はクリアされる
	}

	if (current_) {
		current_->Update();

		// シーン側の終了要求
		if (current_->IsEnd()) {
			const std::string next = current_->Next();
			if (!next.empty()) {
				Change(next);
			}
		}
	}
}

void SceneManager::Draw() {
	if (current_) {
		current_->Draw();
	}
}

void SceneManager::Clear() {
	current_.reset();
	currentName_.clear();
	pendingNext_.clear();
	factories_.clear();
}

} // namespace Engine
