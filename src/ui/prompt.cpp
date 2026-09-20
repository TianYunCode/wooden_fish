#include "ui/prompt.h"

#include <algorithm>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <string>
#include <vector>

#include "core/util.h"

namespace muyu::ui {

namespace {

constexpr WORD kIdLabel = 101;
constexpr WORD kIdEdit = 100;
constexpr WORD kIdText = 103;
constexpr WORD kIdList = 104;
constexpr WORD kIdFooter = 105;

using LedgerRows = std::vector<std::pair<DWORD, unsigned long long>>;

struct PromptCtx {
    const wchar_t *label;
    int defVal, minV, maxV;
    int val;
};

template <class T>
void Put(std::vector<BYTE> &b, T v) {
    const BYTE *p = reinterpret_cast<const BYTE *>(&v);
    b.insert(b.end(), p, p + sizeof(T));
}

void Align4(std::vector<BYTE> &b) {
    while (b.size() % 4)
        b.push_back(0);
}

void PutW(std::vector<BYTE> &b, const wchar_t *s) {
    for (; *s; ++s)
        Put(b, *s);
    Put(b, static_cast<wchar_t>(0));
}

// 千分位：1234567 -> "1,234,567"
std::wstring GroupDigits(unsigned long long v) {
    wchar_t s[32];
    std::swprintf(s, std::size(s), L"%llu", v);
    std::wstring src(s), out;
    int n = 0;
    for (auto it = src.rbegin(); it != src.rend(); ++it) {
        if (n && n % 3 == 0)
            out.push_back(L',');
        out.push_back(*it);
        ++n;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

// 控件模板：atom 0x80=BUTTON 0x81=EDIT 0x82=STATIC
void AddItem(std::vector<BYTE> &b, DWORD style, int x, int y, int cx, int cy, WORD id, WORD atom,
             const wchar_t *text) {
    Align4(b);
    Put(b, style);
    Put(b, DWORD(0));
    Put(b, static_cast<INT16>(x));
    Put(b, static_cast<INT16>(y));
    Put(b, static_cast<INT16>(cx));
    Put(b, static_cast<INT16>(cy));
    Put(b, id);
    Put(b, WORD(0xFFFF));
    Put(b, atom);
    PutW(b, text);
    Put(b, WORD(0));
}

// 类名以字符串形式写入的控件模板项（公共控件类未注册全局 atom，只能走内联字符串）。
// 注意：0xFFFF 前缀表示序数类，字符串形式直接内联、不加前缀
void AddItemClass(std::vector<BYTE> &b, DWORD style, int x, int y, int cx, int cy, WORD id,
                  const wchar_t *cls, const wchar_t *text) {
    Align4(b);
    Put(b, style);
    Put(b, DWORD(0));
    Put(b, static_cast<INT16>(x));
    Put(b, static_cast<INT16>(y));
    Put(b, static_cast<INT16>(cx));
    Put(b, static_cast<INT16>(cy));
    Put(b, id);
    PutW(b, cls);
    PutW(b, text);
    Put(b, WORD(0));
}

struct LedgerUI {
    const LedgerRows *rows = nullptr;
    HFONT font = nullptr, bold = nullptr;
    HIMAGELIST spacer = nullptr;
};

HFONT MakeUiFont(UINT dpi, int weight, const wchar_t *face) {
    return CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

INT_PTR CALLBACK LedgerDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto *ui = reinterpret_cast<LedgerUI *>(GetWindowLongPtrW(h, DWLP_USER));
    switch (msg) {
    case WM_INITDIALOG: {
        ui = new LedgerUI;
        ui->rows = reinterpret_cast<const LedgerRows *>(lp);
        SetWindowLongPtrW(h, DWLP_USER, reinterpret_cast<LONG_PTR>(ui));
        UINT dpi = GetDpiForWindow(h);
        ui->font = MakeUiFont(dpi, FW_NORMAL, L"Microsoft YaHei UI");
        ui->bold = MakeUiFont(dpi, FW_SEMIBOLD, L"Microsoft YaHei UI");

        HWND lv = GetDlgItem(h, kIdList);
        HWND footer = GetDlgItem(h, kIdFooter);
        SendMessageW(lv, WM_SETFONT, reinterpret_cast<WPARAM>(ui->font), TRUE);
        if (HWND hdr = reinterpret_cast<HWND>(SendMessageW(lv, LVM_GETHEADER, 0, 0)))
            SendMessageW(hdr, WM_SETFONT, reinterpret_cast<WPARAM>(ui->bold), TRUE);
        SendMessageW(footer, WM_SETFONT, reinterpret_cast<WPARAM>(ui->bold), TRUE);

        // 1px 全透明垫高图：把行高撑到 26px，表格不再挤成一团
        const BYTE blank[26 * 4] = {};  // 1bpp 全黑位图 + 黑掩码 = 完全透明
        if (HBITMAP bmp = CreateBitmap(1, 26, 1, 1, blank)) {
            ui->spacer = ImageList_Create(1, 26, ILC_COLOR24 | ILC_MASK, 1, 1);
            ImageList_AddMasked(ui->spacer, bmp, RGB(0, 0, 0));
            DeleteObject(bmp);
            SendMessageW(lv, LVM_SETIMAGELIST, LVSIL_SMALL,
                         reinterpret_cast<LPARAM>(ui->spacer));
        }
        SendMessageW(lv, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                     LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

        LVCOLUMNW col{};
        col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
        col.fmt = LVCFMT_CENTER;
        col.cx = MulDiv(150, dpi, 96);
        col.pszText = const_cast<LPWSTR>(L"日期");
        SendMessageW(lv, LVM_INSERTCOLUMNW, 0, reinterpret_cast<LPARAM>(&col));
        col.fmt = LVCFMT_RIGHT;
        col.cx = MulDiv(120, dpi, 96);
        col.pszText = const_cast<LPWSTR>(L"当日功德");
        SendMessageW(lv, LVM_INSERTCOLUMNW, 1, reinterpret_cast<LPARAM>(&col));

        const DWORD today = TodayYmd();
        unsigned long long sum = 0;
        int idx = 0;
        wchar_t buf[64];
        for (auto it = ui->rows->rbegin(); it != ui->rows->rend(); ++it, ++idx) {  // 最近在前
            sum += it->second;
            buf[0] = 0;
            std::swprintf(buf, std::size(buf), L"%04u-%02u-%02u", it->first / 10000,
                          it->first / 100 % 100, it->first % 100);
            if (it->first == today)
                std::wcscat(buf, L"（今日）");
            LVITEMW li{};
            li.mask = LVIF_TEXT;
            li.iItem = idx;
            li.pszText = buf;
            SendMessageW(lv, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&li));
            std::wstring g = GroupDigits(it->second);
            LVITEMW si{};
            si.mask = LVIF_TEXT;
            si.iItem = idx;
            si.iSubItem = 1;
            si.pszText = g.data();
            SendMessageW(lv, LVM_SETITEMTEXTW, idx, reinterpret_cast<LPARAM>(&si));
        }
        // 第二列吃满剩余客户端宽（客户区已扣除垂直滚动条）
        RECT rc{};
        GetClientRect(lv, &rc);
        SendMessageW(lv, LVM_SETCOLUMNWIDTH, 1, rc.right - MulDiv(150, dpi, 96) - MulDiv(8, dpi, 96));

        wchar_t f[128];
        if (ui->rows->empty())
            std::wcscpy(f, L"尚无记录——敲击后会自动记下每天最终的功德");
        else
            std::swprintf(f, std::size(f), L"在册 %d 日 · 累计 %s 功德",
                          static_cast<int>(ui->rows->size()), GroupDigits(sum).c_str());
        SetWindowTextW(footer, f);
        SetFocus(GetDlgItem(h, IDCANCEL));
        return FALSE;
    }
    case WM_NOTIFY: {
        auto *nm = reinterpret_cast<LPNMHDR>(lp);
        if (nm->idFrom == kIdList && nm->code == NM_CUSTOMDRAW) {
            auto *cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(nm);
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT:
                return CDRF_NOTIFYSUBITEMDRAW;
            case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
                if (!(cd->nmcd.uItemState & CDIS_SELECTED)) {
                    cd->clrTextBk =
                        (cd->nmcd.dwItemSpec & 1) ? RGB(250, 246, 238) : RGB(255, 255, 255);
                    cd->clrText = RGB(0x3c, 0x35, 0x2a);
                }
                return CDRF_NEWFONT;
            }
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
            EndDialog(h, LOWORD(wp));
            return TRUE;
        }
        break;
    case WM_DESTROY:
        if (ui) {
            if (ui->font)
                DeleteObject(ui->font);
            if (ui->bold)
                DeleteObject(ui->bold);
            if (ui->spacer)
                ImageList_Destroy(ui->spacer);  // 列表控件不回收外部 SETIMAGELIST 的表
            delete ui;
            SetWindowLongPtrW(h, DWLP_USER, 0);
        }
        break;
    }
    return FALSE;
}

INT_PTR CALLBACK DlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto *ctx = reinterpret_cast<PromptCtx *>(lp);
        SetWindowLongPtrW(h, DWLP_USER, lp);
        SetWindowTextW(GetDlgItem(h, kIdLabel), ctx->label);
        SetDlgItemInt(h, kIdEdit, static_cast<UINT>(ctx->defVal), FALSE);
        SendDlgItemMessageW(h, kIdEdit, EM_SETSEL, 0, static_cast<LPARAM>(-1));
        SetFocus(GetDlgItem(h, kIdEdit));
        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            auto *ctx = reinterpret_cast<PromptCtx *>(GetWindowLongPtrW(h, DWLP_USER));
            BOOL ok = FALSE;
            int v = static_cast<int>(GetDlgItemInt(h, kIdEdit, &ok, FALSE));
            if (ok && v >= ctx->minV && v <= ctx->maxV) {
                ctx->val = v;
                EndDialog(h, IDOK);
            } else {
                MessageBeep(MB_ICONWARNING);
                SetFocus(GetDlgItem(h, kIdEdit));
            }
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) {
            EndDialog(h, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

}  // namespace

INT_PTR CALLBACK TextDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowTextW(GetDlgItem(h, kIdText), reinterpret_cast<const wchar_t *>(lp));
        SetFocus(GetDlgItem(h, IDCANCEL));
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
            EndDialog(h, LOWORD(wp));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void ShowTextDialog(HWND parent, const wchar_t *title, const std::wstring &text, int w, int h) {
    std::vector<BYTE> b;
    Align4(b);
    Put(b, DWORD(WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT | DS_CENTER));
    Put(b, DWORD(WS_EX_DLGMODALFRAME));
    Put(b, WORD(2));  // 控件数
    Put(b, INT16(0));
    Put(b, INT16(0));
    Put(b, INT16(w));
    Put(b, INT16(h));
    Put(b, WORD(0));  // 无菜单
    Put(b, WORD(0));  // 窗口类为默认 #32770
    PutW(b, title);
    Put(b, WORD(9));  // DS_SETFONT 字号：9pt（模板按整点存储）
    PutW(b, L"MS Shell Dlg");
    AddItem(b,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_READONLY |
                ES_AUTOVSCROLL | WS_VSCROLL,
            7, 7, w - 14, h - 29, kIdText, 0x0081, L"");
    AddItem(b, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, (w - 76) / 2, h - 17, 76, 15,
            IDCANCEL, 0x0080, L"关闭");
    DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
                            reinterpret_cast<LPCDLGTEMPLATEW>(b.data()), parent, TextDlgProc,
                            reinterpret_cast<LPARAM>(text.c_str()));
}

void ShowLedgerDialog(HWND parent, const LedgerRows &rows) {
    constexpr int w = 272, h = 178;  // DLU
    std::vector<BYTE> b;
    Align4(b);
    Put(b, DWORD(WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT | DS_CENTER));
    Put(b, DWORD(WS_EX_DLGMODALFRAME));
    Put(b, WORD(3));  // 列表 + 合计行 + 关闭按钮
    Put(b, INT16(0));
    Put(b, INT16(0));
    Put(b, INT16(w));
    Put(b, INT16(h));
    Put(b, WORD(0));
    Put(b, WORD(0));
    PutW(b, L"功德簿");
    Put(b, WORD(9));
    PutW(b, L"MS Shell Dlg");
    AddItemClass(b,
                 WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
                     LVS_SHOWSELALWAYS | WS_VSCROLL,
                 7, 7, w - 14, h - 39, kIdList, L"SysListView32", L"");
    AddItem(b, WS_CHILD | WS_VISIBLE | SS_CENTER, 7, h - 29, w - 14, 9, kIdFooter, 0x0082, L"");
    AddItem(b, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, (w - 50) / 2, h - 17, 50, 14,
            IDCANCEL, 0x0080, L"关闭");
    DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
                            reinterpret_cast<LPCDLGTEMPLATEW>(b.data()), parent, LedgerDlgProc,
                            reinterpret_cast<LPARAM>(&rows));
}

bool PromptNumber(HWND parent, const wchar_t *title, const wchar_t *label, int defVal, int minV,
                  int maxV, int &out) {
    PromptCtx ctx{label, defVal, minV, maxV, 0};
    std::vector<BYTE> b;
    Align4(b);
    Put(b, DWORD(WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT | DS_CENTER));
    Put(b, DWORD(WS_EX_DLGMODALFRAME));
    Put(b, WORD(4));      // 控件数（DLGTEMPLATE 紧随扩展风格）
    Put(b, INT16(0));
    Put(b, INT16(0));
    Put(b, INT16(230));
    Put(b, INT16(72));
    Put(b, WORD(0));      // 无菜单
    Put(b, WORD(0));      // 窗口类为默认 #32770
    PutW(b, title);
    Put(b, WORD(9));      // DS_SETFONT 字号：9pt（模板按整点存储）
    PutW(b, L"MS Shell Dlg");
    AddItem(b, WS_CHILD | WS_VISIBLE | SS_LEFT, 7, 7, 216, 10, kIdLabel, 0x0082, L"");
    AddItem(b, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, 7, 24,
            216, 14, kIdEdit, 0x0081, L"");
    AddItem(b, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 40, 48, 80, 19, IDOK, 0x0080,
            L"确定");
    AddItem(b, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 126, 48, 80, 19, IDCANCEL,
            0x0080, L"取消");
    if (DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
                                reinterpret_cast<LPCDLGTEMPLATEW>(b.data()), parent, DlgProc,
                                reinterpret_cast<LPARAM>(&ctx)) != IDOK)
        return false;
    out = ctx.val;
    return true;
}

}  // namespace muyu::ui
