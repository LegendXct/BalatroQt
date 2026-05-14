#ifndef BALATRO_GPU_EFFECTS_RENDERER_H
#define BALATRO_GPU_EFFECTS_RENDERER_H

#include <QColor>
#include <QPixmap>
#include "../card/carddata.h"

namespace BalatroShaders {

// 原版 Balatro 的版本特效本质是 GPU 片元 shader：牌面先正常绘制一次，
// 再把 foil/holo/polychrome/negative_shine 等 shader 按相同混合顺序叠到牌上。
// 这里用离屏 OpenGL FBO 在显卡上渲染同一套逐像素公式，再把结果交回 Qt Widgets。
// 如果某些机器 OpenGL 初始化失败，调用方会自动回退到 CPU 版本。
QPixmap renderEditionPixmapGpu(const QPixmap &base, Edition edition, double intensity = 1.0, bool *ok = nullptr);
QPixmap renderShaderPixmapGpu(const QPixmap &base, const QString &shaderName, double intensity = 1.0, bool overlayOnBase = true, bool *ok = nullptr);
void clearGpuEffectCache();

} // namespace BalatroShaders

#endif
