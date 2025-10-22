#include "Audio.h"
#include <Windows.h>
#include <fstream>
#pragma comment(lib, "xaudio2.lib")

namespace Engine {

bool Audio::Initialize() {
	if (FAILED(XAudio2Create(&xa_, 0, XAUDIO2_DEFAULT_PROCESSOR)))
		return false;
	if (FAILED(xa_->CreateMasteringVoice(&master_)))
		return false;
	return true;
}

bool Audio::ParseWav(const wchar_t* path, std::vector<BYTE>& data, WAVEFORMATEX& wfx) {
	std::ifstream f(path, std::ios::binary);
	if (!f)
		return false;
	auto RDW = [&]() {
		uint32_t v;
		f.read((char*)&v, 4);
		return v;
	};
	auto RWW = [&]() {
		uint16_t v;
		f.read((char*)&v, 2);
		return v;
	};
	char riff[4];
	f.read(riff, 4);
	RDW();
	f.read(riff, 4); // "RIFF" ... "WAVE"
	bool fmt = false, dat = false;
	while (!f.eof()) {
		char id[4];
		if (!f.read(id, 4))
			break;
		uint32_t sz = RDW();
		if (!strncmp(id, "fmt ", 4)) {
			wfx.wFormatTag = RWW();
			wfx.nChannels = RWW();
			wfx.nSamplesPerSec = RDW();
			wfx.nAvgBytesPerSec = RDW();
			wfx.nBlockAlign = RWW();
			wfx.wBitsPerSample = RWW();
			if (sz > 16)
				f.seekg(sz - 16, std::ios::cur);
			fmt = true;
		} else if (!strncmp(id, "data", 4)) {
			data.resize(sz);
			f.read((char*)data.data(), sz);
			dat = true;
		} else
			f.seekg(sz, std::ios::cur);
	}
	return fmt && dat;
}

bool Audio::LoadWav(const wchar_t* path) {
	if (!ParseWav(path, wav_, wfx_))
		return false;
	buf_ = {};
	buf_.pAudioData = wav_.data();
	buf_.AudioBytes = (UINT32)wav_.size();
	buf_.Flags = XAUDIO2_END_OF_STREAM;
	if (src_) {
		src_->DestroyVoice();
		src_ = nullptr;
	}
	if (FAILED(xa_->CreateSourceVoice(&src_, &wfx_)))
		return false;
	return true;
}

void Audio::Play() {
	if (!src_)
		return;
	src_->Stop();
	src_->FlushSourceBuffers();
	src_->SubmitSourceBuffer(&buf_);
	src_->Start();
}

void Audio::Shutdown() {
	if (src_) {
		src_->DestroyVoice();
		src_ = nullptr;
	}
	if (master_) {
		master_->DestroyVoice();
		master_ = nullptr;
	}
	xa_.Reset();
}

} // namespace Engine
