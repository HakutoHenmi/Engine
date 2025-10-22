#pragma once
// リザルト（ベース）

#include "IScene.h"
#include "WindowDX.h"
#include <string>

namespace Engine {

class ResultScene : public IScene {
public:
	void Initialize(WindowDX* dx) override;
	void Update() override;
	void Draw() override;

	bool IsEnd() const override { return end_; }
	std::string Next() const override { return next_; }

private:
	WindowDX* dx_ = nullptr;

	bool end_ = false;
	std::string next_{};

	bool waitRelease_ = true;
	bool prevPressed_ = false;
};

} // namespace Engine
