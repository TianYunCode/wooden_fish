#pragma once
// 音频子系统：Media Foundation 把内嵌 mp3 解码为 PCM 常驻内存，XAudio2 多声部混音播放。
// 敲击音为短样本，每次点击新建声部即时播放；禅定音为长样本无限循环。

#include <windows.h>
#include <mmsystem.h>
#include <xaudio2.h>
#include <vector>

namespace muyu::media {

class AudioEngine {
public:
    // 解码内嵌音频并创建 XAudio2 设备；敲击音失败返回 false（禅定音失败仅禁用禅定音）
    bool Init();
    void Shutdown();

    // volIdx: 0静音/1小/2中/3大；combo 越高音调越高
    void PlayKnock(int volIdx, int combo);
    void ApplyZen(bool on);

private:
    struct Playing {
        IXAudio2SourceVoice *voice;
        ULONGLONG endAt;  // 按样本时长推算的播完时刻，到点回收
    };

    static bool DecodeRes(WORD rid, std::vector<BYTE> &pcm, WAVEFORMATEX &wf, DWORD &durMs);

    std::vector<BYTE> knockPcm_, zenPcm_;
    WAVEFORMATEX knockWf_ = {}, zenWf_ = {};
    DWORD knockDurMs_ = 0;

    IXAudio2 *xa2_ = nullptr;
    IXAudio2MasteringVoice *master_ = nullptr;
    std::vector<Playing> voices_;
    IXAudio2SourceVoice *zenVoice_ = nullptr;
};

}  // namespace muyu::media
