#include "render/skins.h"

#include <array>
#include <algorithm>

namespace muyu::render {

namespace {

using Matrix = Gdiplus::ColorMatrix;

Matrix Identity() {
    Matrix m{};
    m.m[0][0] = m.m[1][1] = m.m[2][2] = m.m[3][3] = 1.0f;
    return m;
}

const std::array<Matrix, 4> &SkinMatrices() {
    static const std::array<Matrix, 4> matrices = [] {
        std::array<Matrix, 4> m{};
        m[0] = Identity();

        // 鎏金：经典 sepia 暖金
        m[1].m[0][0] = 0.393f; m[1].m[1][0] = 0.769f; m[1].m[2][0] = 0.189f;
        m[1].m[0][1] = 0.349f; m[1].m[1][1] = 0.686f; m[1].m[2][1] = 0.131f;
        m[1].m[0][2] = 0.272f; m[1].m[1][2] = 0.534f; m[1].m[2][2] = 0.131f;
        m[1].m[3][3] = 1.0f;

        // 水墨：去色偏冷
        m[2].m[0][0] = 0.33f; m[2].m[1][0] = 0.53f; m[2].m[2][0] = 0.14f;
        m[2].m[0][1] = 0.33f; m[2].m[1][1] = 0.53f; m[2].m[2][1] = 0.14f;
        m[2].m[0][2] = 0.37f; m[2].m[1][2] = 0.55f; m[2].m[2][2] = 0.18f;
        m[2].m[4][2] = 0.06f;
        m[2].m[3][3] = 1.0f;

        // 霓虹：棕→紫红、高光偏青
        m[3].m[0][0] = 0.85f; m[3].m[1][0] = 0.0f; m[3].m[2][0] = 0.45f;
        m[3].m[0][1] = 0.05f; m[3].m[1][1] = 0.25f; m[3].m[2][1] = 0.25f;
        m[3].m[0][2] = 0.35f; m[3].m[1][2] = 0.0f; m[3].m[2][2] = 0.85f;
        m[3].m[3][3] = 1.0f;
        return m;
    }();
    return matrices;
}

}  // namespace

Gdiplus::ImageAttributes *GetSkinAttr(int skinIdx) {
    using namespace Gdiplus;
    static ImageAttributes *ia = nullptr;
    static int built = -1;
    const int normalized = std::clamp(skinIdx, 0, 3);
    if (built == normalized)
        return ia;

    delete ia;
    ia = new ImageAttributes();
    ia->SetColorMatrix(&SkinMatrices()[normalized]);
    built = normalized;
    return ia;
}

}  // namespace muyu::render
