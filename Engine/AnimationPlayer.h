// AnimationPlayer.h
#pragma once
#include "Animation.h"
#include "AnimationUtil.h"

namespace Engine {

struct AnimState {
	float time = 0.f;
	float speed = 1.f;
	bool playing = true;
	bool pingpong = false;
	bool forward = true;
};

struct TRSColor {
	float pos[3]{};
	float rot[3]{};
	float scl[3]{1, 1, 1};
	float col[4]{1, 1, 1, 1};
};

class AnimationPlayer {
public:
	void SetClip(const AnimClip* clip) {
		clip_ = clip;
		state_ = {};
	}
	void Update(float dt) {
		if (!clip_ || !state_.playing)
			return;
		float advance = dt * state_.speed * (state_.forward ? 1.f : -1.f);
		state_.time += advance;

		if (clip_->loop) {
			if (!state_.pingpong) {
				while (state_.time < 0)
					state_.time += clip_->length;
				while (state_.time > clip_->length)
					state_.time -= clip_->length;
			} else {
				if (state_.time < 0) {
					state_.time = -state_.time;
					state_.forward = !state_.forward;
				}
				if (state_.time > clip_->length) {
					state_.time = clip_->length - (state_.time - clip_->length);
					state_.forward = !state_.forward;
				}
			}
		} else {
			if (state_.time < 0)
				state_.time = 0;
			if (state_.time > clip_->length) {
				state_.time = clip_->length;
				state_.playing = false;
			}
		}
	}

	TRSColor Sample() const {
		TRSColor o{};
		if (!clip_)
			return o;
		float t = state_.time;
		o.pos[0] = SampleAnimTrack(clip_->posX, t);
		o.pos[1] = SampleAnimTrack(clip_->posY, t);
		o.pos[2] = SampleAnimTrack(clip_->posZ, t);
		o.rot[0] = SampleAnimTrack(clip_->rotX, t);
		o.rot[1] = SampleAnimTrack(clip_->rotY, t);
		o.rot[2] = SampleAnimTrack(clip_->rotZ, t);
		o.scl[0] = SampleAnimTrack(clip_->sclX, t);
		o.scl[1] = SampleAnimTrack(clip_->sclY, t);
		o.scl[2] = SampleAnimTrack(clip_->sclZ, t);
		o.col[0] = SampleAnimTrack(clip_->colR, t);
		o.col[1] = SampleAnimTrack(clip_->colG, t);
		o.col[2] = SampleAnimTrack(clip_->colB, t);
		o.col[3] = SampleAnimTrack(clip_->colA, t);
		return o;
	}

	AnimState& State() { return state_; }
	const AnimClip* Clip() const { return clip_; }

private:
	const AnimClip* clip_ = nullptr;
	AnimState state_{};
};

} // namespace Engine
