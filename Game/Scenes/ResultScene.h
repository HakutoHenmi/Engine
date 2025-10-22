#pragma once
// リザルトシーン（ヘッダ）

#include "IScene.h"
#include <string>
#include "WindowDX.h"

namespace Engine {

class ResultScene : public IScene {
public:
	void Initialize(WindowDX* dx) override;
	void Update() override;
	void Draw() override;

	bool IsEnd() const override;
	std::string Next() const override;

private:
	bool end_ = false;
	std::string next_;

WindowDX* dx_ = nullptr;

	bool prevSpace_ = false;
	bool waitRelease_ = true;
};

} // namespace Engine
