#pragma once
// 皮肤调色：GDI+ ColorMatrix，同一素材四种观感（原木/鎏金/水墨/霓虹）

#include <windows.h>
#include <objidl.h>  // WIN32_LEAN_AND_MEAN 下 gdiplus 所需的 IStream
#include <gdiplus.h>

namespace muyu::render {

// 返回当前皮肤的 ImageAttributes（内部按 skinIdx 缓存，指针在下一次切换前有效）
Gdiplus::ImageAttributes *GetSkinAttr(int skinIdx);

}  // namespace muyu::render
