#pragma once
// 内嵌图像素材：启动时从 exe 的 RCDATA 资源解码为 GDI+ Bitmap，常驻内存

#include <windows.h>
#include <objidl.h>  // WIN32_LEAN_AND_MEAN 下 gdiplus 所需的 IStream
#include <gdiplus.h>

namespace muyu::media {

struct Assets {
    Gdiplus::Bitmap *fish = nullptr;
    Gdiplus::Bitmap *gu = nullptr;

    bool Load();  // 两张图全部成功才返回 true
    void Free();
};

}  // namespace muyu::media
