#include "media/assets.h"

#include <windows.h>
#include <shlwapi.h>

#include "resource.h"

namespace muyu::media {

namespace {

Gdiplus::Bitmap *LoadResBitmap(int id) {
    HMODULE mod = GetModuleHandleW(nullptr);
    HRSRC h = FindResourceW(mod, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));  // 10 = RCDATA
    if (!h) return nullptr;
    DWORD sz = SizeofResource(mod, h);
    HGLOBAL hg = LoadResource(mod, h);
    IStream *st = SHCreateMemStream(static_cast<const BYTE *>(LockResource(hg)), sz);
    if (!st) return nullptr;
    Gdiplus::Bitmap *b = Gdiplus::Bitmap::FromStream(st);
    st->Release();
    if (!b || b->GetLastStatus() != Gdiplus::Ok) {
        delete b;
        return nullptr;
    }
    return b;
}

}  // namespace

bool Assets::Load() {
    fish = LoadResBitmap(IDR_FISH);
    gu = LoadResBitmap(IDR_GU);
    glow = LoadResBitmap(IDR_GLOW);  // 可选素材
    return fish && gu;
}

void Assets::Free() {
    delete fish;
    delete gu;
    delete glow;
    fish = gu = glow = nullptr;
}

}  // namespace muyu::media
