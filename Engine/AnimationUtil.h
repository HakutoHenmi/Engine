// AnimationUtil.h
#pragma once
#include "Animation.h"
#include <algorithm>
#include <cmath>

namespace Engine {

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

template<class T> inline T SampleAnimTrack(const AnimTrack<T>& tr, float time) {
	if (tr.keys.empty())
		return T{};
	if (tr.keys.size() == 1)
		return tr.keys[0].v;

	if (time <= tr.keys.front().t)
		return tr.keys.front().v;
	if (time >= tr.keys.back().t)
		return tr.keys.back().v;

	// 上側境界を二分探索
	auto it = std::upper_bound(tr.keys.begin(), tr.keys.end(), time, [](float t, const AnimKeyframe<T>& k) { return t < k.t; });
	size_t i1 = size_t(it - tr.keys.begin());
	size_t i0 = i1 - 1;

	const auto& k0 = tr.keys[i0];
	const auto& k1 = tr.keys[i1];
	float u = (time - k0.t) / (k1.t - k0.t);

	if (k0.interp == Interp::Step)
		return k0.v;
	return Lerp(k0.v, k1.v, u);
}

} // namespace Engine
