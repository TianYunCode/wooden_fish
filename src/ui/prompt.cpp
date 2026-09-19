#include "ui/prompt.h"

#include <vector>

namespace muyu::ui {

namespace {

constexpr WORD kIdLabel = 101;
constexpr WORD kIdEdit = 100;

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
