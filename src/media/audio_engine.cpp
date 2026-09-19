#include "media/audio_engine.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <cstring>

#include "config/layout.h"
#include "resource.h"

namespace muyu::media {

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

    IMFMediaType *want = nullptr;
    MFCreateMediaType(&want);
    want->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    want->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    want->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    hr = rd->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, want);
    want->Release();
    if (FAILED(hr)) {
        rd->Release();
        return false;
    }

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
                    out.resize(old + len);
                    memcpy(out.data() + old, p, len);
                    buf->Unlock();
                }
                buf->Release();
            }
            sample->Release();
        }
        if (sb & MF_SOURCE_READERF_ENDOFSTREAM)
            break;
    }

    UINT32 ch = 2, rate = 44100;
    IMFMediaType *got = nullptr;
    if (SUCCEEDED(rd->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &got))) {
        got->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &ch);
        got->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        got->Release();
    }
    rd->Release();
    if (out.empty())
        return false;

    pcm.swap(out);
    WORD nch = static_cast<WORD>(ch);
    wf = {};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = nch;
    wf.nSamplesPerSec = rate;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = nch * 2;
    wf.nAvgBytesPerSec = rate * nch * 2;
    durMs = static_cast<DWORD>(pcm.size() * 1000ULL / wf.nAvgBytesPerSec);
    return true;
}

bool AudioEngine::Init() {
    DWORD zenDur = 0;
    if (!DecodeRes(IDR_KNOCK, knockPcm_, knockWf_, knockDurMs_))
        return false;
    DecodeRes(IDR_ZEN, zenPcm_, zenWf_, zenDur);  // 失败仅影响禅定音
    if (FAILED(XAudio2Create(&xa2_, 0, XAUDIO2_DEFAULT_PROCESSOR)) || !xa2_)
        return false;
    if (FAILED(xa2_->CreateMasteringVoice(&master_))) {
        xa2_->Release();
        xa2_ = nullptr;
        return false;
    }
    return true;
}

void AudioEngine::PlayKnock(int volIdx, int combo) {
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
    v->SetVolume(config::kVolLv[volIdx]);
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

void AudioEngine::ApplyZen(bool on) {
    if (!xa2_)
        return;
    if (zenVoice_) {
        zenVoice_->Stop(0);
        zenVoice_->DestroyVoice();
        zenVoice_ = nullptr;
    }
    if (!on || zenPcm_.empty())
        return;
    if (FAILED(xa2_->CreateSourceVoice(&zenVoice_, &zenWf_)) || !zenVoice_)
        return;
    zenVoice_->SetVolume(0.45f);
    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = static_cast<UINT32>(zenPcm_.size());
    buf.pAudioData = zenPcm_.data();
    buf.LoopCount = XAUDIO2_LOOP_INFINITE;
    if (FAILED(zenVoice_->SubmitSourceBuffer(&buf)) || FAILED(zenVoice_->Start(0))) {
        zenVoice_->DestroyVoice();
        zenVoice_ = nullptr;
    }
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
