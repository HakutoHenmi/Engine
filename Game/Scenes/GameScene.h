#pragma once
#include "Camera.h"
#include "IScene.h"
#include "Input.h"
#include "Player.h"
#include "Renderer.h"
#include "Stage.h"
#include "WindowDX.h"

namespace Engine {

class GameScene : public IScene {
public:
	void Initialize(WindowDX* dx) override;
	void Update() override;
	void Draw() override;

	bool IsEnd() const override { return end_; }
	std::string Next() const override { return next_; }

private:
	WindowDX* dx_ = nullptr;
	Renderer renderer_;
	Input input_;

	Camera camPlay_;
	Camera camDebug_;
	Camera* activeCam_ = nullptr;
	bool useDebugCam_ = false;
	bool prevF1_ = false;

	Stage stage_;
	Player player_;

	bool gridVisible_ = true;

	bool end_ = false;
	std::string next_;
	bool waitRelease_ = true;
	bool prevPressed_ = false;
};

} // namespace Engine
