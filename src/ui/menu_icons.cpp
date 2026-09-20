#include "ui/menu_icons.h"

#include <gdiplus.h>
#include <shlwapi.h>
#include <map>
#include <string>

namespace muyu::ui {

namespace {

// 逻辑尺寸（96dpi 基准，绘制时乘 dpi）
constexpr int kPadL = 14;   // 左边距
constexpr int kIconSz = 20; // 图标边长
constexpr int kGap = 12;    // 图标与文字间距
constexpr int kPadR = 14;   // 右边距
constexpr int kArrowW = 14; // 子菜单箭头（系统绘制）预留宽
constexpr int kRowH = 34;   // 行高

// key → 位图缓存；加载失败也记 nullptr，避免反复 FindResource
std::map<std::wstring, Gdiplus::Bitmap *> g_icons;
HFONT g_menuFont = nullptr;
const std::wstring kEmpty;  // itemData 缺失时的兜底空串

Gdiplus::Bitmap *Icon(const std::wstring &key) {
    auto it = g_icons.find(key);
    if (it != g_icons.end())
        return it->second;
    Gdiplus::Bitmap *bmp = nullptr;
    // rc.exe 里带引号的字符串资源名会连引号一起存且转大写，查找须还原这两点
    std::wstring name = L"\"IC_" + key + L"\"";
    for (auto &ch : name)
        if (ch >= L'a' && ch <= L'z')
            ch -= L'a' - L'A';
    if (HRSRC res = FindResourceW(nullptr, name.c_str(), RT_RCDATA)) {
        if (HGLOBAL hg = LoadResource(nullptr, res)) {
            if (IStream *s = SHCreateMemStream(
                    static_cast<const BYTE *>(LockResource(hg)), SizeofResource(nullptr, res))) {
                auto *cand = new Gdiplus::Bitmap(s, TRUE);
                if (cand->GetLastStatus() == Gdiplus::Ok)
                    bmp = cand;
                else
                    delete cand;
                s->Release();
            }
        }
    }
    g_icons[key] = bmp;
    return bmp;
}

HFONT MenuFont() {
    if (!g_menuFont) {
        NONCLIENTMETRICSW ncm{};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        g_menuFont = CreateFontIndirectW(&ncm.lfMenuFont);
    }
    return g_menuFont;
}

}  // namespace

bool OnMeasureMenu(LPMEASUREITEMSTRUCT ms, double dpi) {
    if (ms->CtlType != ODT_MENU)
        return false;
    if (ms->itemID == static_cast<UINT>(-1)) {  // 分隔符：矮行，宽度由其余项决定
        ms->itemWidth = 0;
        ms->itemHeight = static_cast<UINT>(9 * dpi + 0.5);
        return true;
    }
    // 文字存在 dwItemData 指向的 MenuDrawData 里（MEASUREITEMSTRUCT 拿不到菜单句柄）
    const auto *d = reinterpret_cast<const MenuDrawData *>(ms->itemData);
    const std::wstring &text = d ? d->text : kEmpty;
    HDC dc = GetDC(nullptr);
    HFONT old = static_cast<HFONT>(SelectObject(dc, MenuFont()));
    SIZE sz{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &sz);
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);
    ms->itemWidth = static_cast<UINT>((kPadL + kIconSz + kGap) * dpi + sz.cx +
                                      (kPadR + kArrowW) * dpi);
    ms->itemHeight = static_cast<UINT>(kRowH * dpi + 0.5);
    return true;
}

void OnDrawMenu(LPDRAWITEMSTRUCT dis, double dpi) {
    if (dis->CtlType != ODT_MENU)
        return;
    RECT rc = dis->rcItem;
    if (dis->itemID == static_cast<UINT>(-1)) {  // 分隔符：一条内缩细线
        FillRect(dis->hDC, &rc, GetSysColorBrush(COLOR_MENU));
        RECT ln{rc.left + static_cast<int>(kPadL * dpi + 0.5), (rc.top + rc.bottom) / 2,
                rc.right - static_cast<int>(kPadR * dpi + 0.5), (rc.top + rc.bottom) / 2 + 1};
        FillRect(dis->hDC, &ln, GetSysColorBrush(COLOR_3DLIGHT));
        return;
    }
    const auto *d = reinterpret_cast<const MenuDrawData *>(dis->itemData);
    if (!d)
        return;

    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    bool sel = (dis->itemState & ODS_SELECTED) != 0;
    bool checked = (dis->itemState & ODS_CHECKED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool sub = d->sub;

    // 双缓冲：以 (0,0,w,h) 本地坐标画到内存位图，再一次性 BitBlt 到目标矩形，避免选中闪烁
    HDC mem = CreateCompatibleDC(dis->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(dis->hDC, w, h);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(mem, bmp));
    RECT local{0, 0, w, h};
    FillRect(mem, &local, GetSysColorBrush(sel ? COLOR_MENUHILIGHT : COLOR_MENU));

    HFONT old = static_cast<HFONT>(SelectObject(mem, MenuFont()));
    SetBkMode(mem, TRANSPARENT);
    int padL = static_cast<int>(kPadL * dpi + 0.5);
    int iconSz = static_cast<int>(kIconSz * dpi + 0.5);
    int gap = static_cast<int>(kGap * dpi + 0.5);
    int padR = static_cast<int>(kPadR * dpi + 0.5);
    int arrowW = static_cast<int>(kArrowW * dpi + 0.5);

    // 图标槽：勾选项画 ✓（与参考图一致），否则画 Lucide 图标
    if (checked) {
        SetTextColor(mem, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT));
        RECT ir{padL, 0, padL + iconSz, h};
        DrawTextW(mem, L"\u2713", 1, &ir, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    } else if (!d->icon.empty()) {
        if (Gdiplus::Bitmap *ic = Icon(d->icon)) {
            Gdiplus::Graphics g(mem);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
            float x = static_cast<float>(padL);
            float y = (static_cast<float>(h) - iconSz) / 2.0f;
            float szf = static_cast<float>(iconSz);
            if (disabled) {  // 置灰：降到 35% 不透明
                Gdiplus::ColorMatrix dm{};
                dm.m[0][0] = dm.m[1][1] = dm.m[2][2] = dm.m[4][4] = 1.0f;
                dm.m[3][3] = 0.35f;
                Gdiplus::ImageAttributes ia;
                ia.SetColorMatrix(&dm);
                g.DrawImage(ic, Gdiplus::RectF(x, y, szf, szf), 0, 0,
                            static_cast<float>(ic->GetWidth()),
                            static_cast<float>(ic->GetHeight()), Gdiplus::UnitPixel, &ia);
            } else {
                g.DrawImage(ic, Gdiplus::RectF(x, y, szf, szf));
            }
        }
    }

    // 文字
    SetTextColor(mem, disabled ? GetSysColor(COLOR_GRAYTEXT)
                               : GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT));
    RECT tr{padL + iconSz + gap, 0, w - padR - (sub ? arrowW : 0), h};
    DrawTextW(mem, d->text.c_str(), static_cast<int>(d->text.size()), &tr,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    SelectObject(mem, old);
    BitBlt(dis->hDC, rc.left, rc.top, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

void FreeMenuIcons() {
    for (auto &kv : g_icons)
        delete kv.second;
    g_icons.clear();
    if (g_menuFont) {
        DeleteObject(g_menuFont);
        g_menuFont = nullptr;
    }
}

}  // namespace muyu::ui
