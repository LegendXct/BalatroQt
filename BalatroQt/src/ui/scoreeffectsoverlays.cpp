#include "scoreeffectsoverlays.h"
#include "../utils/gpueffectsrenderer.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QtGlobal>
#include <cmath>

namespace {
const char *kQuadVertex = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif
attribute vec2 a_pos;
varying vec2 v_tex;
varying vec2 v_screen;
uniform vec2 u_screen_size;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
    v_tex = a_pos * 0.5 + 0.5;
    v_screen = v_tex * u_screen_size;
}
)GLSL";

const char *kFlashFragment = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif
uniform float u_time;
uniform float u_mid_flash;
uniform vec2 u_screen_size;
varying vec2 v_screen;
#define PIXEL_SIZE_FAC 700.0
void main() {
    float pixel_size = length(u_screen_size.xy) / PIXEL_SIZE_FAC;
    vec2 uv = (floor(v_screen.xy * (1.0 / pixel_size)) * pixel_size - 0.5 * u_screen_size.xy) / length(u_screen_size.xy);
    float mid_white = min(1.0,
        (u_time > 2.5 ? max(0.0, sqrt(u_time - 2.5) - 60.0 * length(uv)) : 0.0)
      + (u_time > 11.0 ? max(0.0, (u_time - 11.0) * (u_time - 11.0) - 5.0 * length(uv)) : 0.0));
    float a = clamp(u_mid_flash * mid_white, 0.0, 1.0);
    if (a <= 0.001) discard;
    gl_FragColor = vec4(1.0, 1.0, 1.0, a);
}
)GLSL";
}

FlameTile::FlameTile(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    mTimer.setTimerType(Qt::PreciseTimer);
    connect(&mTimer, &QTimer::timeout, this, [this]() { tick(); });
}

void FlameTile::setColours(const QColor &colour1, const QColor &colour2)
{
    mColour1 = colour1;
    mColour2 = colour2;
}

void FlameTile::setBaseColour(const QColor &base)
{
    mBase = base;
    update();
}

void FlameTile::setBoxGeometry(int boxHeight, int radius)
{
    mBoxH = boxHeight;
    mBoxRadius = radius;
    update();
}

void FlameTile::setFlameId(float id)
{
    mId = id;
}

void FlameTile::setAmount(float amount)
{
    mAmount = qBound(0.0, double(amount), 10.0);
    if (mAmount >= 0.05) {
        if (!mClock.isValid()) mClock.start();
        if (!mTimer.isActive()) { mLastMs = mClock.elapsed(); mTimer.start(33); }
    }
    // 降到 0 时由 tick() 在下一拍清掉火焰并停表；底框始终保留。
}

void FlameTile::stop()
{
    mAmount = 0.0;
    mFlame = QPixmap();
    mTimer.stop();
    update();
}

void FlameTile::tick()
{
    if (!mClock.isValid()) mClock.start();
    const qint64 now = mClock.elapsed();
    double dt = 1.0 / 60.0;
    if (mLastMs > 0) dt = qBound(0.001, double(now - mLastMs) / 1000.0, 0.05);
    mLastMs = now;

    // 原版 flame_handler：timer += dt*(1 + intensity*0.2)，让火焰随强度上窜更快。
    mTime += dt * (1.0 + mAmount * 0.2);

    if (mAmount >= 0.1 && width() > 0 && height() > 0) {
        // 火焰渲染整块瓦片（含底框区）；底框会盖住火焰下半段，露在底框上方的火焰底边因此
        // 正好被底框顶切平 → 与底框严丝合缝、无缝隙。
        // 火焰像素化(shader PIXEL_SIZE_FAC=60)，低分辨率渲染再放大也不糊；限到 ~140 宽省开销。
        const int rw = qMin(width(), 140);
        const QSize rs(rw, qMax(1, height() * rw / qMax(1, width())));
        QPixmap px = BalatroShaders::renderFlamePixmapGpu(
            rs, float(mTime), float(mAmount), mColour1, mColour2, mId);
        if (!px.isNull()) mFlame = px;
    } else {
        mFlame = QPixmap();
    }
    update();
    if (mAmount < 0.05) mTimer.stop();
}

void FlameTile::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const int boxH = mBoxH > 0 ? qMin(mBoxH, height()) : height();

    // 1) 先画火焰，铺满整块瓦片（一直延伸进底框区）。横向画到比控件略宽（±12%）让火身占满
    //    底框宽度（shader 火焰列约占 80% 宽，拉宽补满）。
    if (mAmount >= 0.1 && !mFlame.isNull()) {
        const int over = int(width() * 0.12);
        p.drawPixmap(QRect(-over, 0, width() + 2 * over, height()), mFlame);
    }

    // 2) 再画底框盖在火焰下半段上：露在底框上方的火焰底边被底框顶切平 → 平直、贴合、无缝。
    //    底框带 emboss 浮雕（对齐原版 hand_chip_area 的 emboss=0.05）：顶部略亮、底部略暗，
    //    顶部更亮也让它与上方火焰过渡更自然。
    const QRectF boxR(0, height() - boxH, width(), boxH);
    QLinearGradient grad(boxR.topLeft(), boxR.bottomLeft());
    grad.setColorAt(0.0, mBase.lighter(122));
    grad.setColorAt(0.5, mBase);
    grad.setColorAt(1.0, mBase.darker(115));
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRoundedRect(boxR, mBoxRadius, mBoxRadius);
}

FlashShaderOverlay::FlashShaderOverlay(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_AlwaysStackOnTop, true);
    setAutoFillBackground(false);
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);
    hide();

    mTimer.setTimerType(Qt::PreciseTimer);
    connect(&mTimer, &QTimer::timeout, this, [this]() {
        if (!isVisible()) return;
        if (mDurationMs > 0 && mClock.isValid() && mClock.elapsed() >= mDurationMs) {
            mTimer.stop();
            hide();
            return;
        }
        update();
    });
}

void FlashShaderOverlay::trigger(float midFlash, int durationMs)
{
    mMidFlash = qBound(0.0f, midFlash, 2.0f);
    mDurationMs = qMax(1, durationMs);
    mClock.restart();
    show();
    raise();
    if (!mTimer.isActive()) mTimer.start(16);
    update();
}

void FlashShaderOverlay::initializeGL()
{
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const bool ok = mProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, kQuadVertex)
                 && mProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, kFlashFragment);
    if (ok) mProgram.bindAttributeLocation("a_pos", 0);
    mReady = ok && mProgram.link();
}

void FlashShaderOverlay::resizeGL(int w, int h)
{
    const qreal dpr = devicePixelRatioF();
    glViewport(0, 0, qMax(1, int(w * dpr)), qMax(1, int(h * dpr)));
}

void FlashShaderOverlay::paintGL()
{
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!mReady || !mClock.isValid() || mDurationMs <= 0) return;
    const float life = qBound(0.0f, float(mClock.elapsed()) / float(mDurationMs), 1.0f);
    const float fade = 1.0f - life;
    if (fade <= 0.001f) return;
    // flash.fs 的白圈从 time > 2.5 开始；这里把短动画映射到原版公式的有效窗口。
    const float shaderTime = 2.5f + life * 1.4f;
    const qreal dpr = devicePixelRatioF();
    mProgram.bind();
    mProgram.setUniformValue("u_time", shaderTime);
    mProgram.setUniformValue("u_mid_flash", mMidFlash * fade);
    mProgram.setUniformValue("u_screen_size", QVector2D(float(width() * dpr), float(height() * dpr)));
    const GLfloat verts[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f};
    mProgram.enableAttributeArray(0);
    mProgram.setAttributeArray(0, GL_FLOAT, verts, 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    mProgram.disableAttributeArray(0);
    mProgram.release();
}
