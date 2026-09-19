#include "render/skins.h"

namespace muyu::render {

Gdiplus::ImageAttributes *GetSkinAttr(int skinIdx) {
    using namespace Gdiplus;
    static ImageAttributes *ia = nullptr;
    static int built = -1;
    if (built == skinIdx)
        return ia;
    delete ia;
    ia = new ImageAttributes();
    ColorMatrix m{};
    if (skinIdx == 1) {  // 鎏金：经典 sepia 暖金
        m.m[0][0] = 0.393f; m.m[1][0] = 0.769f; m.m[2][0] = 0.189f;
        m.m[0][1] = 0.349f; m.m[1][1] = 0.686f; m.m[2][1] = 0.131f;
        m.m[0][2] = 0.272f; m.m[1][2] = 0.534f; m.m[2][2] = 0.131f;
    } else if (skinIdx == 2) {  // 水墨：去色偏冷
        m.m[0][0] = 0.33f; m.m[1][0] = 0.53f; m.m[2][0] = 0.14f;
        m.m[0][1] = 0.33f; m.m[1][1] = 0.53f; m.m[2][1] = 0.14f;
        m.m[0][2] = 0.37f; m.m[1][2] = 0.55f; m.m[2][2] = 0.18f;
        m.m[4][2] = 0.06f;  // 抬一点蓝
    } else if (skinIdx == 3) {  // 霓虹：棕→紫红、高光偏青
        m.m[0][0] = 0.85f; m.m[1][0] = 0.0f; m.m[2][0] = 0.45f;
        m.m[0][1] = 0.05f; m.m[1][1] = 0.25f; m.m[2][1] = 0.25f;
        m.m[0][2] = 0.35f; m.m[1][2] = 0.0f; m.m[2][2] = 0.85f;
    } else {  // 原木：单位矩阵
        m.m[0][0] = m.m[1][1] = m.m[2][2] = m.m[3][3] = 1.0f;
    }
    m.m[3][3] = 1.0f;
    ia->SetColorMatrix(&m);
    built = skinIdx;
    return ia;
}

}  // namespace muyu::render
