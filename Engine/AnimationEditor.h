#pragma once
#include "Animation.h"
#include "AnimationPlayer.h"
#include <string>

namespace Engine {
class AnimationEditorPanel {
public:
	void SetClip(AnimClip* c) { clip_ = c; }
	void Draw(AnimationPlayer& player); // 実装は .cpp

private:
	AnimClip* clip_ = nullptr;
	float timelineZoom_ = 1.0f;
	float cursorSec_ = 0.0f;
	int selectedRow_ = 0;
	bool recording_ = false;
};
} // namespace Engine
