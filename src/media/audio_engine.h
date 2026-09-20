#pragma once
// 音频子系统：Media Foundation 解码 mp3 → PCM，XAudio2 多声部混音播放。
// 敲击音为短样本，启动时整段解完常驻，每次点击新建声部即时播放。
// 禅定音为长曲目：独立解码线程一次解完整曲 PCM，解完才开始播放（无限循环），
// 播放由 XAudio2 引擎线程完成，与解码线程分离；换曲立即销毁旧声部并释放旧曲 PCM。

#include <windows.h>
#include <mmsystem.h>
#include <xaudio2.h>
#include <string>
#include <vector>

struct IMFSourceReader;

namespace muyu::media {

class AudioEngine {
public:
    // 解码内嵌敲击音并创建 XAudio2 设备；失败返回 false（禅定音曲目在启用时才解码）
    bool Init();
    void Shutdown();

    // 音量档 0静音/1小/2中/3大：对敲击音与禅定音同时生效（禅定音实时改增益，不中断循环）
    void SetVolume(int volIdx);
    // 禅定音淡出系数 0..1（睡眠定时到点时 UI 逐档下调；显式选曲自动复位为 1）
    void SetZenFade(double f);
    void PlayKnock(int combo);  // 连击越高音调越高
    // 禅定音 0关 1..kZenTrackCount 选曲 kZenCustomIdx=本地文件；换曲即释放旧曲并起独立解码线程
    // 返回 false 表示所选曲目无法打开/非法（关与成功启动解码时返回 true；声音在整曲解完后响起）
    bool ApplyZen(int trackIdx, const std::wstring &customFile = L"");

private:
    struct Playing {
        IXAudio2SourceVoice *voice;
        ULONGLONG endAt;  // 按样本时长推算的播完时刻，到点回收
    };
    struct ZenJob {
        AudioEngine *self;
        IMFSourceReader *reader;  // 主线程已建好并协商 PCM 的首遍 reader，解码线程接管
        int trackIdx;
        std::wstring file;
    };

    static bool DecodeRes(WORD rid, std::vector<BYTE> &pcm, WAVEFORMATEX &wf, DWORD &durMs);
    void StopZenStream();                    // 停线程→毁声部→释放整曲 PCM
    static DWORD WINAPI ZenDecodeThread(LPVOID param);

    std::vector<BYTE> knockPcm_;
    WAVEFORMATEX knockWf_ = {};
    DWORD knockDurMs_ = 0;

    IXAudio2 *xa2_ = nullptr;
    IXAudio2MasteringVoice *master_ = nullptr;
    int volIdx_ = 2;
    double zenFade_ = 1.0;  // 禅定淡出系数，仅 UI 线程写
    std::vector<Playing> voices_;

    // 禅定播放状态：整曲 PCM 只在播放期间存在；线程先退出，主线程才碰 voice/PCM，无并发竞争
    IXAudio2SourceVoice *zenVoice_ = nullptr;
    std::vector<BYTE> zenPcm_;
    HANDLE zenThread_ = nullptr;
    HANDLE zenStopEv_ = nullptr;
};

}  // namespace muyu::media
