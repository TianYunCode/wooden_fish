#pragma once
// 音频子系统：Media Foundation 把内嵌 mp3 解码为 PCM 常驻内存，XAudio2 多声部混音播放。
// 敲击音为短样本，每次点击新建声部即时播放；禅定音为长样本无限循环。

#include <windows.h>
#include <mmsystem.h>
#include <xaudio2.h>
#include <string>
#include <vector>

namespace muyu::media {

class AudioEngine {
public:
    // 解码内嵌敲击音并创建 XAudio2 设备；失败返回 false（禅定音曲目在启用时才解码）
    bool Init();
    void Shutdown();

    // 音量档 0静音/1小/2中/3大：对敲击音与禅定音同时生效（禅定音实时改增益，不中断循环）
    void SetVolume(int volIdx);
    void PlayKnock(int combo);  // 连击越高音调越高
    // 禅定音 0关 1..kZenTrackCount 选曲 kZenCustomIdx=本地文件；曲目懒解码，仅当前曲目常驻
    // 返回 false 表示所选曲目无法解码/播放（关与已成功时返回 true）
    bool ApplyZen(int trackIdx, const std::wstring &customFile = L"");

private:
    struct Playing {
        IXAudio2SourceVoice *voice;
        ULONGLONG endAt;  // 按样本时长推算的播完时刻，到点回收
    };
    struct ZenTrack {
        std::vector<BYTE> pcm;
        WAVEFORMATEX wf = {};
    };

    static bool DecodeRes(WORD rid, std::vector<BYTE> &pcm, WAVEFORMATEX &wf, DWORD &durMs);
    static bool DecodeFile(const wchar_t *path, std::vector<BYTE> &pcm, WAVEFORMATEX &wf,
                           DWORD &durMs);
    bool EnsureZenLoaded(int trackIdx, const std::wstring &customFile);

    std::vector<BYTE> knockPcm_;
    WAVEFORMATEX knockWf_ = {};
    DWORD knockDurMs_ = 0;
    ZenTrack zen_;
    int zenLoaded_ = 0;  // 当前常驻曲目号，0=未加载
    std::wstring zenLoadedFile_;  // zenLoaded_==kZenCustomIdx 对应的文件

    IXAudio2 *xa2_ = nullptr;
    IXAudio2MasteringVoice *master_ = nullptr;
    int volIdx_ = 2;
    std::vector<Playing> voices_;
    IXAudio2SourceVoice *zenVoice_ = nullptr;
};

}  // namespace muyu::media
