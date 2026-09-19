#include "media/audio_engine.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <cstring>
#include <utility>

#include "config/layout.h"
#include "resource.h"

namespace muyu::media {

namespace {
constexpr WORD kZenRes[config::kZenTrackCount] = {IDR_ZEN1, IDR_ZEN2, IDR_ZEN3, IDR_ZEN4, IDR_ZEN5};

// 协商为 16bit 44.1k 立体声 PCM 并读完；maxBytes>0 时超长截断（本地文件控内存）
bool ReadDecoded(IMFSourceReader *rd, std::vector<BYTE> &pcm, WAVEFORMATEX &wf, DWORD &durMs,
                 size_t maxBytes) {
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
        if (FAILED(rd->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &sb, &ts, &sample)))
            break;
        if (sample) {
            IMFMediaBuffer *buf = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf))) {
                BYTE *p = nullptr;
                DWORD len = 0;
                if (SUCCEEDED(buf->Lock(&p, nullptr, &len))) {
                    size_t old = out.size();
                    if (maxBytes && old + len > maxBytes)
                        len = static_cast<DWORD>(maxBytes - old);
                    if (len) {
                        out.resize(old + len);
                        memcpy(out.data() + old, p, len);
                    }
                    buf->Unlock();
                }
                buf->Release();
            }
            sample->Release();
        }
        if (maxBytes && out.size() >= maxBytes)
            break;
        if (sb & MF_SOURCE_READERF_ENDOFSTREAM)
            break;
    }
    if (out.empty())
        return false;

    pcm.swap(out);
    wf = {};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = 44100;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 4;
    wf.nAvgBytesPerSec = 44100 * 4;
    durMs = static_cast<DWORD>(pcm.size() * 1000ULL / wf.nAvgBytesPerSec);
    return true;
}

// 就地首尾 1.5s 线性淡入淡出，让任意本地文件循环衔接处无爆音
void ApplyFade(std::vector<BYTE> &pcm, const WAVEFORMATEX &wf) {
    int16_t *s = reinterpret_cast<int16_t *>(pcm.data());
    size_t n = pcm.size() / 2;
    double fade = wf.nSamplesPerSec * 1.5;
    for (size_t i = 0; i < n; ++i) {
        double g = 1.0;
        if (i < fade)
            g = i / fade;
        else if (i + fade > n)
            g = (n - i) / fade;
        s[i] = static_cast<int16_t>(s[i] * g);
    }
}
}  // namespace

bool AudioEngine::DecodeRes(WORD rid, std::vector<BYTE> &pcm, WAVEFORMATEX &wf, DWORD &durMs) {
    HMODULE mod = GetModuleHandleW(nullptr);
    HRSRC h = FindResourceW(mod, MAKEINTRESOURCEW(rid), MAKEINTRESOURCEW(10));
    if (!h) return false;
    DWORD sz = SizeofResource(mod, h);
    IStream *ist = SHCreateMemStream(static_cast<const BYTE *>(LockResource(LoadResource(mod, h))), sz);
    if (!ist) return false;
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
        if (rd) rd->Release();
        return false;
    }
    bool ok = ReadDecoded(rd, pcm, wf, durMs, 0);
    rd->Release();
    return ok;
}

bool AudioEngine::DecodeFile(const wchar_t *path, std::vector<BYTE> &pcm, WAVEFORMATEX &wf,
                             DWORD &durMs) {
    IMFSourceReader *rd = nullptr;
    if (FAILED(MFCreateSourceReaderFromURL(path, nullptr, &rd)) || !rd)
        return false;
    size_t cap = static_cast<size_t>(config::kZenMaxMs) * 44100 * 4 / 1000;
    bool ok = ReadDecoded(rd, pcm, wf, durMs, cap);
    rd->Release();
    if (ok)
        ApplyFade(pcm, wf);
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
        zenVoice_->SetVolume(0.45f * config::kVolLv[volIdx_]);  // 实时改增益，循环不中断
}

void AudioEngine::PlayKnock(int combo) {
    if (!xa2_ || knockPcm_.empty()) return;
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
    if (FAILED(xa2_->CreateSourceVoice(&v, &knockWf_)) || !v) return;
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

bool AudioEngine::EnsureZenLoaded(int trackIdx, const std::wstring &customFile) {
    bool same = trackIdx == zenLoaded_ &&
                (trackIdx != config::kZenCustomIdx || customFile == zenLoadedFile_);
    if (same && !zen_.pcm.empty())
        return true;
    if (trackIdx < 1 || trackIdx > config::kZenCustomIdx || !xa2_)
        return false;
    DWORD durMs = 0;
    ZenTrack t;
    bool ok;
    if (trackIdx == config::kZenCustomIdx) {
        ok = !customFile.empty() &&
             GetFileAttributesW(customFile.c_str()) != INVALID_FILE_ATTRIBUTES &&
             DecodeFile(customFile.c_str(), t.pcm, t.wf, durMs);
    } else {
        ok = DecodeRes(kZenRes[trackIdx - 1], t.pcm, t.wf, durMs);
    }
    if (!ok)
        return false;
    zen_ = std::move(t);  // 旧曲目 PCM 随之释放
    zenLoaded_ = trackIdx;
    zenLoadedFile_ = customFile;
    return true;
}

bool AudioEngine::ApplyZen(int trackIdx, const std::wstring &customFile) {
    if (!xa2_)
        return false;
    if (zenVoice_) {
        zenVoice_->Stop(0);
        zenVoice_->DestroyVoice();
        zenVoice_ = nullptr;
    }
    if (trackIdx == 0) {
        zen_ = ZenTrack{};
        zenLoaded_ = 0;
        zenLoadedFile_.clear();
        return true;
    }
    if (!EnsureZenLoaded(trackIdx, customFile) || zen_.pcm.empty())
        return false;
    if (FAILED(xa2_->CreateSourceVoice(&zenVoice_, &zen_.wf)) || !zenVoice_)
        return false;
    zenVoice_->SetVolume(0.45f * config::kVolLv[volIdx_]);
    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = static_cast<UINT32>(zen_.pcm.size());
    buf.pAudioData = zen_.pcm.data();
    buf.LoopCount = XAUDIO2_LOOP_INFINITE;
    if (FAILED(zenVoice_->SubmitSourceBuffer(&buf)) || FAILED(zenVoice_->Start(0))) {
        zenVoice_->DestroyVoice();
        zenVoice_ = nullptr;
        return false;
    }
    return true;
}

void AudioEngine::Shutdown() {
    if (zenVoice_) {
        zenVoice_->Stop(0);
        zenVoice_->DestroyVoice();
        zenVoice_ = nullptr;
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
