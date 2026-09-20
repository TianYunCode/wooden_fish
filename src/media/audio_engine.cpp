#include "media/audio_engine.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <cmath>
#include <cstring>

#include "config/layout.h"
#include "resource.h"

namespace muyu::media {

namespace {
constexpr WORD kZenRes[config::kZenTrackCount] = {IDR_ZEN1, IDR_ZEN2, IDR_ZEN3, IDR_ZEN4,
                                                  IDR_ZEN5, IDR_ZEN6};

WAVEFORMATEX ZenWf() {
    WAVEFORMATEX wf = {};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = 44100;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 4;
    wf.nAvgBytesPerSec = 44100 * 4;
    return wf;
}

// 建 reader 并强制协商为 16bit 44.1k 立体声 PCM；主线程建好以就地验证文件可用性，随后交解码线程
bool CreateZenReader(int trackIdx, const std::wstring &file, IMFSourceReader **out) {
    IMFSourceReader *rd = nullptr;
    if (trackIdx == config::kZenCustomIdx) {
        if (FAILED(MFCreateSourceReaderFromURL(file.c_str(), nullptr, &rd)) || !rd)
            return false;
    } else {
        HMODULE mod = GetModuleHandleW(nullptr);
        HRSRC h = FindResourceW(mod, MAKEINTRESOURCEW(kZenRes[trackIdx - 1]),
                                MAKEINTRESOURCEW(10));
        if (!h)
            return false;
        HGLOBAL hg = LoadResource(mod, h);
        if (!hg)
            return false;
        IStream *ist = SHCreateMemStream(static_cast<const BYTE *>(LockResource(hg)),
                                         SizeofResource(mod, h));
        if (!ist)
            return false;
        IMFByteStream *bs = nullptr;
        if (FAILED(MFCreateMFByteStreamOnStream(ist, &bs))) {
            ist->Release();
            return false;
        }
        ist->Release();
        HRESULT hr = MFCreateSourceReaderFromByteStream(bs, nullptr, &rd);
        bs->Release();
        if (FAILED(hr) || !rd) {
            if (rd)
                rd->Release();
            return false;
        }
    }
    IMFMediaType *want = nullptr;
    MFCreateMediaType(&want);
    want->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    want->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    want->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    want->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    want->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    HRESULT hr = rd->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, want);
    want->Release();
    if (FAILED(hr)) {
        rd->Release();
        return false;
    }
    *out = rd;
    return true;
}

// 协商为 16bit 44.1k 立体声 PCM 并读完（敲击音等短样本用）
bool ReadDecoded(IMFSourceReader *rd, std::vector<BYTE> &pcm, WAVEFORMATEX &wf, DWORD &durMs) {
    IMFMediaType *want = nullptr;
    MFCreateMediaType(&want);
    want->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    want->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    want->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    want->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    want->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    HRESULT hr = rd->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, want);
    want->Release();
    if (FAILED(hr))
        return false;

    std::vector<BYTE> out;
    for (;;) {
        DWORD sb = 0;
        LONGLONG ts = 0;
        IMFSample *sample = nullptr;
        if (FAILED(rd->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &sb, &ts,
                                  &sample)))
            break;
        if (sample) {
            IMFMediaBuffer *buf = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf))) {
                BYTE *p = nullptr;
                DWORD len = 0;
                if (SUCCEEDED(buf->Lock(&p, nullptr, &len))) {
                    out.insert(out.end(), p, p + len);
                    buf->Unlock();
                }
                buf->Release();
            }
            sample->Release();
        }
        if (sb & MF_SOURCE_READERF_ENDOFSTREAM)
            break;
    }
    if (out.empty())
        return false;

    pcm.swap(out);
    wf = ZenWf();
    durMs = static_cast<DWORD>(pcm.size() * 1000ULL / wf.nAvgBytesPerSec);
    return true;
}
}  // namespace

bool AudioEngine::DecodeRes(WORD rid, std::vector<BYTE> &pcm, WAVEFORMATEX &wf, DWORD &durMs) {
    HMODULE mod = GetModuleHandleW(nullptr);
    HRSRC h = FindResourceW(mod, MAKEINTRESOURCEW(rid), MAKEINTRESOURCEW(10));
    if (!h)
        return false;
    DWORD sz = SizeofResource(mod, h);
    IStream *ist =
        SHCreateMemStream(static_cast<const BYTE *>(LockResource(LoadResource(mod, h))), sz);
    if (!ist)
        return false;
    IMFByteStream *bs = nullptr;
    if (FAILED(MFCreateMFByteStreamOnStream(ist, &bs))) {
        ist->Release();
        return false;
    }
    IMFSourceReader *rd = nullptr;
    HRESULT hr = MFCreateSourceReaderFromByteStream(bs, nullptr, &rd);
    bs->Release();
    ist->Release();
    if (FAILED(hr) || !rd) {
        if (rd)
            rd->Release();
        return false;
    }
    bool ok = ReadDecoded(rd, pcm, wf, durMs);
    rd->Release();
    return ok;
}

bool AudioEngine::Init() {
    if (!DecodeRes(IDR_KNOCK, knockPcm_, knockWf_, knockDurMs_))
        return false;
    if (FAILED(XAudio2Create(&xa2_, 0, XAUDIO2_DEFAULT_PROCESSOR)) || !xa2_)
        return false;
    if (FAILED(xa2_->CreateMasteringVoice(&master_))) {
        xa2_->Release();
        xa2_ = nullptr;
        return false;
    }
    return true;
}

void AudioEngine::SetVolume(int volIdx) {
    volIdx_ = volIdx;
    if (zenVoice_)
        zenVoice_->SetVolume(0.45f * config::kVolLv[volIdx_] *
                             static_cast<float>(zenFade_));  // 实时改增益，循环不中断
}

void AudioEngine::SetZenFade(double f) {
    zenFade_ = f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
    if (zenVoice_)
        zenVoice_->SetVolume(0.45f * config::kVolLv[volIdx_] * static_cast<float>(zenFade_));
}

void AudioEngine::PlayKnock(int combo) {
    if (!xa2_ || knockPcm_.empty())
        return;
    ULONGLONG now = GetTickCount64();
    for (auto it = voices_.begin(); it != voices_.end();) {
        if (now >= it->endAt) {
            it->voice->Stop(0);
            it->voice->DestroyVoice();
            it = voices_.erase(it);
        } else {
            ++it;
        }
    }
    IXAudio2SourceVoice *v = nullptr;
    if (FAILED(xa2_->CreateSourceVoice(&v, &knockWf_)) || !v)
        return;
    v->SetVolume(config::kVolLv[volIdx_]);
    v->SetFrequencyRatio(1.0f + (combo > 50 ? 50 : combo) * 0.004f);  // 连击越高音调越高
    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = static_cast<UINT32>(knockPcm_.size());
    buf.pAudioData = knockPcm_.data();
    if (SUCCEEDED(v->SubmitSourceBuffer(&buf)) && SUCCEEDED(v->Start(0))) {
        voices_.push_back({v, now + knockDurMs_ + 300});
    } else {
        v->DestroyVoice();
    }
}

void AudioEngine::StopZenStream() {
    if (zenThread_) {
        SetEvent(zenStopEv_);
        WaitForSingleObject(zenThread_, 5000);
        CloseHandle(zenThread_);
        zenThread_ = nullptr;
    }
    if (zenVoice_) {
        zenVoice_->Stop(0);
        zenVoice_->DestroyVoice();
        zenVoice_ = nullptr;
    }
    std::vector<BYTE>().swap(zenPcm_);  // 换曲即释放上一曲的整曲解码数据
}

// 解码线程：一次性把整曲解成 PCM（每个样本块轮询停止事件，可中途放弃），解完才建声部起播；
// 播放本身由 XAudio2 引擎线程推进，与本线程分离
DWORD WINAPI AudioEngine::ZenDecodeThread(LPVOID param) {
    ZenJob *job = static_cast<ZenJob *>(param);
    AudioEngine *self = job->self;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMFSourceReader *rd = job->reader;
    std::vector<BYTE> pcm;
    bool ok = false;
    for (;;) {
        if (WaitForSingleObject(self->zenStopEv_, 0) == WAIT_OBJECT_0)
            break;
        DWORD sb = 0;
        IMFSample *sample = nullptr;
        if (FAILED(rd->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &sb, nullptr,
                                  &sample))) {
            if (sample)
                sample->Release();
            ok = false;
            break;
        }
        if (sample) {
            IMFMediaBuffer *buf = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf))) {
                BYTE *p = nullptr;
                DWORD len = 0;
                if (SUCCEEDED(buf->Lock(&p, nullptr, &len))) {
                    pcm.insert(pcm.end(), p, p + len);
                    buf->Unlock();
                }
                buf->Release();
            }
            sample->Release();
        }
        if (sb & MF_SOURCE_READERF_ENDOFSTREAM) {
            ok = !pcm.empty();
            break;
        }
    }
    rd->Release();
    if (ok && job->trackIdx == config::kZenCustomIdx) {
        // 本地文件整曲首尾各 1.5s 淡入淡出：LOOP_INFINITE 的接缝两头都是静音，无爆音
        int16_t *s = reinterpret_cast<int16_t *>(pcm.data());
        size_t n = pcm.size() / 2;
        double fade = 44100.0 * 1.5;
        for (size_t i = 0; i < n; ++i) {
            double g = 1.0;
            if (i < fade)
                g = i / fade;
            else if (i + fade > n)
                g = (n - i) / fade;
            s[i] = static_cast<int16_t>(s[i] * g);
        }
    }
    if (ok && WaitForSingleObject(self->zenStopEv_, 0) != WAIT_OBJECT_0) {
        IXAudio2SourceVoice *v = nullptr;
        WAVEFORMATEX wf = ZenWf();
        if (FAILED(self->xa2_->CreateSourceVoice(&v, &wf)) || !v) {
            ok = false;
        } else {
            self->zenPcm_.swap(pcm);  // 整曲 PCM 移交引擎，本线程退出后主线程独占管理
            XAUDIO2_BUFFER b = {};
            b.AudioBytes = static_cast<UINT32>(self->zenPcm_.size());
            b.pAudioData = self->zenPcm_.data();
            b.LoopCount = XAUDIO2_LOOP_INFINITE;
            v->SetVolume(0.45f * config::kVolLv[self->volIdx_] *
                         static_cast<float>(self->zenFade_));
            if (FAILED(v->SubmitSourceBuffer(&b)) || FAILED(v->Start(0))) {
                v->DestroyVoice();
                std::vector<BYTE>().swap(self->zenPcm_);
                ok = false;
            } else {
                self->zenVoice_ = v;
            }
        }
    } else {
        ok = false;
    }
    if (!ok)
        std::vector<BYTE>().swap(pcm);
    CoUninitialize();
    delete job;
    return 0;
}

bool AudioEngine::ApplyZen(int trackIdx, const std::wstring &customFile) {
    if (!xa2_)
        return false;
    StopZenStream();  // 上一曲：线程、声部、整曲 PCM 全部清理
    zenFade_ = 1.0;   // 显式选曲视为从头播放，作废进行中的淡出
    if (trackIdx == 0)
        return true;
    if (trackIdx < 1 || trackIdx > config::kZenCustomIdx)
        return false;
    if (trackIdx == config::kZenCustomIdx &&
        (customFile.empty() ||
         GetFileAttributesW(customFile.c_str()) == INVALID_FILE_ATTRIBUTES))
        return false;
    // reader 在主线程建好，坏文件/缺资源可即时如实返回 false
    IMFSourceReader *rd = nullptr;
    if (!CreateZenReader(trackIdx, customFile, &rd))
        return false;
    if (!zenStopEv_)
        zenStopEv_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!zenStopEv_) {
        rd->Release();
        return false;
    }
    ResetEvent(zenStopEv_);
    ZenJob *job = new ZenJob{this, rd, trackIdx, customFile};
    zenThread_ = CreateThread(nullptr, 0, ZenDecodeThread, job, 0, nullptr);
    if (!zenThread_) {
        rd->Release();
        delete job;
        return false;
    }
    return true;
}

void AudioEngine::Shutdown() {
    StopZenStream();
    if (zenStopEv_) {
        CloseHandle(zenStopEv_);
        zenStopEv_ = nullptr;
    }
    for (auto &p : voices_) {
        p.voice->Stop(0);
        p.voice->DestroyVoice();
    }
    voices_.clear();
    if (master_) {
        master_->DestroyVoice();
        master_ = nullptr;
    }
    if (xa2_) {
        xa2_->Release();
        xa2_ = nullptr;
    }
}

}  // namespace muyu::media
