#include "render/painter.h"

#include <windows.h>
#include <gdiplus.h>
#include <cstring>
#include <cmath>
#include <string>

#include "config/layout.h"
#include "core/util.h"
#include "render/skins.h"

namespace muyu::render {

void Render(HWND hwnd, const AppState &st, media::Assets &assets) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0) return;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HDC mem = CreateCompatibleDC(nullptr);
    HBITMAP dib = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP old = static_cast<HBITMAP>(SelectObject(mem, dib));
    memset(bits, 0, static_cast<size_t>(w) * h * 4);

    {
        using namespace Gdiplus;
        Graphics g(mem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        float s = static_cast<float>(st.EffScale());
        g.ScaleTransform(s, s);

        ULONGLONG now = GetTickCount64();
        long long dt = static_cast<long long>(now - st.knockAt);
        ImageAttributes *skin = GetSkinAttr(st.skinIdx);
        RectF fishRect(static_cast<REAL>(config::kFishLeft), static_cast<REAL>(config::kFishTop),
                       static_cast<REAL>(config::kFishW), static_cast<REAL>(config::kFishH));

        // 鱼身：受压均匀缩放 1→0.94→1 正弦往返
        double squash = 1.0;
        if (dt >= 0 && dt < static_cast<long long>(config::kSquashMs))
            squash = 1.0 - 0.06 * std::sin(kPi * dt / config::kSquashMs);
        Matrix save;
        g.GetTransform(&save);
        g.TranslateTransform(static_cast<REAL>(config::kFishCx), static_cast<REAL>(config::kFishCy));
        g.ScaleTransform(static_cast<REAL>(squash), static_cast<REAL>(squash));
        g.TranslateTransform(static_cast<REAL>(-config::kFishCx), static_cast<REAL>(-config::kFishCy));
        // 功德达成：鱼身后金色光晕
        if (st.goalIdx > 0 && st.daily >= config::kGoals[st.goalIdx]) {
            double pulse = 0.75 + 0.25 * std::sin(static_cast<double>(now) / 400.0);
            for (int i = 4; i >= 1; --i) {
                SolidBrush halo(Color(static_cast<BYTE>(16 * pulse), 0xFF, 0xD8, 0x66));
                g.FillEllipse(&halo,
                              static_cast<REAL>(config::kFishCx - 190 - i * 10),
                              static_cast<REAL>(config::kFishCy - 170 - i * 10),
                              static_cast<REAL>(380 + i * 20), static_cast<REAL>(380 + i * 20));
            }
        }
        g.DrawImage(assets.fish, fishRect, 0, 0, assets.fish->GetWidth(), assets.fish->GetHeight(),
                    UnitPixel, skin);
        g.SetTransform(&save);

        // 音波纹
        if (dt >= 0 && dt < static_cast<long long>(config::kRippleMs)) {
            double t = static_cast<double>(dt) / config::kRippleMs;
            for (int i = 0; i < 2; ++i) {
                double tt = t - i * 0.18;
                if (tt <= 0 || tt >= 1) continue;
                Pen pen(Color(static_cast<BYTE>(150 * (1 - tt)), 255, 240, 210), 2.5f);
                double rx = 16 + 56 * tt;
                g.DrawEllipse(&pen, static_cast<REAL>(config::kHitX - rx),
                              static_cast<REAL>(config::kHitY - rx * 0.55), static_cast<REAL>(rx * 2),
                              static_cast<REAL>(rx * 1.1));
            }
        }

        // 木槌：绕柄端挥动——急落(EaseOut)砸下、缓起(EaseInOut)回位
        double deg = config::kRestDeg;
        if (dt >= 0 && dt < static_cast<long long>(config::kBackMs)) {
            double t = dt < static_cast<long long>(config::kSwingMs)
                           ? EaseOutCubic(static_cast<double>(dt) / config::kSwingMs)
                           : 1.0 - EaseInOut(static_cast<double>(dt - config::kSwingMs) /
                                             (config::kBackMs - config::kSwingMs));
            deg = config::kRestDeg + (config::kHitDeg - config::kRestDeg) * t;
        }
        g.TranslateTransform(static_cast<REAL>(config::kPivotX), static_cast<REAL>(config::kPivotY));
        g.RotateTransform(static_cast<REAL>(deg));
        g.DrawImage(assets.gu,
                    RectF(static_cast<REAL>(config::kStickX - config::kPivotX),
                          static_cast<REAL>(config::kStickY - config::kPivotY),
                          static_cast<REAL>(config::kStickSize), static_cast<REAL>(config::kStickSize)),
                    0, 0, assets.gu->GetWidth(), assets.gu->GetHeight(), UnitPixel, skin);
        g.SetTransform(&save);

        FontFamily fam(L"Microsoft YaHei UI");
        // 漂浮文字
        {
            Font f(&fam, 20, FontStyleBold, UnitPixel);
            for (const auto &ft : st.floats) {
                double t = static_cast<double>(now - ft.born) / config::kFloatMs;
                if (t < 0 || t > 1) continue;
                SolidBrush br(ft.gold ? Color(static_cast<BYTE>(255 * (1 - t)), 0xD4, 0xAF, 0x00)
                                      : Color(static_cast<BYTE>(255 * (1 - t)), 0x7A, 0x4A, 0x1E));
                g.DrawString(ft.word.c_str(), -1, &f,
                             PointF(static_cast<REAL>(ft.x),
                                    static_cast<REAL>(ft.y - 110 * EaseOutCubic(t))),
                             &br);
            }
        }
        // 功德计数：金色大字，木鱼正上方居中；小窗口时保证至少 13 物理像素高，防止缩到看不见
        {
            double mpx = 20.0;
            if (mpx * s < 13.0) mpx = 13.0 / s;
            Font f(&fam, static_cast<REAL>(mpx), FontStyleBold, UnitPixel);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            SolidBrush br(Color(235, 0xC8, 0x9A, 0x2E));
            std::wstring mt = U8("功德 ") + std::to_wstring(st.merit);
            g.DrawString(mt.c_str(), -1, &f,
                         RectF(static_cast<REAL>(config::kFishCx - 165), 20, 330,
                               static_cast<REAL>(mpx * 1.9)),
                         &sf, &br);
        }
        // 今日目标：木鱼正下方居中，文字+进度条
        if (st.goalIdx > 0) {
            double gpx = 12.0;
            if (gpx * s < 11.0) gpx = 11.0 / s;
            Font f(&fam, static_cast<REAL>(gpx), FontStyleRegular, UnitPixel);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            SolidBrush br(Color(200, 0x6B, 0x52, 0x3A));
            std::wstring gt = U8("今日 ") + std::to_wstring(st.daily) + L"/" +
                              std::to_wstring(config::kGoals[st.goalIdx]);
            g.DrawString(gt.c_str(), -1, &f,
                         RectF(static_cast<REAL>(config::kFishCx - 165), 417, 330, 20), &sf, &br);
            double ratio = static_cast<double>(st.daily) / config::kGoals[st.goalIdx];
            if (ratio > 1.0) ratio = 1.0;
            const double bw = 200, bx = config::kFishCx - bw / 2, by = 440;
            SolidBrush trackBg(Color(70, 0x6B, 0x52, 0x3A));
            g.FillRectangle(&trackBg, static_cast<REAL>(bx), static_cast<REAL>(by),
                            static_cast<REAL>(bw), 5.0f);
            SolidBrush fill(ratio >= 1.0 ? Color(230, 0xD4, 0xAF, 0x00) : Color(200, 0xB8, 0x86, 0x2A));
            g.FillRectangle(&fill, static_cast<REAL>(bx), static_cast<REAL>(by),
                            static_cast<REAL>(bw * ratio), 5.0f);
        }
    }

    RECT wr;
    GetWindowRect(hwnd, &wr);
    POINT dst = {wr.left, wr.top};
    SIZE sz = {w, h};
    POINT src = {0, 0};
    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd, nullptr, &dst, &sz, mem, &src, 0, &bf, ULW_ALPHA);

    SelectObject(mem, old);
    DeleteObject(dib);
    DeleteDC(mem);
}

bool AnimActive(const AppState &st) {
    ULONGLONG dt = GetTickCount64() - st.knockAt;
    if (dt < config::kFloatMs + 100 || !st.floats.empty())
        return true;
    return st.goalIdx > 0 && st.daily >= config::kGoals[st.goalIdx];  // 达成光晕持续呼吸
}

}  // namespace muyu::render
