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

// 主菜单红蓝漩涡背景：把原版 splash 漩涡 shader 渲到离屏 FBO 再交回一张不透明 QPixmap。
// 用普通 QLabel + 定时器逐帧刷新（QLabel 是普通控件，避免在 QWidget overlay 里再嵌一个
// QOpenGLWidget——后者在部分驱动上无法盖住底层 GL 场景）。GL 失败时返回空 QPixmap。
QPixmap renderSplashBackgroundGpu(const QSize &size, float timeSeconds,
                                  const QColor &colour1, const QColor &colour2,
                                  float vortSpeed, bool *ok = nullptr);

// 计分火焰：离屏渲染原版 flame.fs，返回带透明边的 QPixmap（火焰形状之外是透明）。
// 贴到普通控件背景上即可，数字浮在其上；避免半透明 QOpenGLWidget 的合成问题。
QPixmap renderFlamePixmapGpu(const QSize &size, float timeSeconds, float amount,
                             const QColor &colour1, const QColor &colour2,
                             float id, bool *ok = nullptr);

void clearGpuEffectCache();

} // namespace BalatroShaders

#endif
