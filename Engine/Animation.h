// Animation.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Engine {

enum class Interp : uint8_t { Step, Linear /*, Cubic*/ };

template<class T> struct AnimKeyframe {
	float t = 0.f; // 秒
	T v{};         // 値
	Interp interp = Interp::Linear;
};

template<class T> struct AnimTrack {
	std::string name; // 例: "posX"
	std::vector<AnimKeyframe<T>> keys;
	bool enabled = true;
};

// クリップ（MVP）
struct AnimClip {
	std::string name = "NewClip";
	float length = 3.0f;
	bool loop = true;

	// TRS + Color を float トラックで
	AnimTrack<float> posX, posY, posZ;
	AnimTrack<float> rotX, rotY, rotZ;
	AnimTrack<float> sclX, sclY, sclZ;
	AnimTrack<float> colR, colG, colB, colA;

	struct Event {
		float t;
		std::string label;
	};
	std::vector<Event> events;
};

} // namespace Engine
