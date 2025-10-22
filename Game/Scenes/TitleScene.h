#pragma once
// タイトルシーン（ヘッダ）

#include "IScene.h"
#include <string>
#include "WindowDX.h"

namespace Engine {

class TitleScene : public IScene {
public:
	// 初期化・更新・描画
	void Initialize(WindowDX* dx) override; 
	void Update() override;
	void Draw() override;

	// 遷移管理
	bool IsEnd() const override;
	std::string Next() const override;

private:
	// 遷移フラグ
	bool end_ = false;
	std::string next_;

	WindowDX* dx_ = nullptr;

	// 入力のトリガー制御用
	bool prevSpace_ = false;  // 前フレームの押下状態
	bool waitRelease_ = true; // 「このシーンに入ってから一度離すまで反応しない」
};

} // namespace Engine
