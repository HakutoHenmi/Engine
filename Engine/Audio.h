#pragma once
// ===============================
//  Audio : XAudio2 + WAV ロード
// ===============================
#include <vector>
#include <wrl.h>
#include <xaudio2.h>

namespace Engine {

class Audio {
public:
	bool Initialize();
	bool LoadWav(const wchar_t* path);
	void Play();
	void Shutdown();

private:
	bool ParseWav(const wchar_t* path, std::vector<BYTE>& data, WAVEFORMATEX& wfx);

private:
	Microsoft::WRL::ComPtr<IXAudio2> xa_;
	IXAudio2MasteringVoice* master_ = nullptr;
	IXAudio2SourceVoice* src_ = nullptr;
	std::vector<BYTE> wav_;
	WAVEFORMATEX wfx_{};
	XAUDIO2_BUFFER buf_{};
};

} // namespace Engine
