#include "scoreeffectsoverlays.h"

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

const char *kFlameFragment = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif
uniform float u_time;
uniform float u_amount;
uniform vec4 u_colour_1;
uniform vec4 u_colour_2;
uniform float u_id;
varying vec2 v_tex;
#define PIXEL_SIZE_FAC 60.0
void main() {
    float intensity = min(10.0, u_amount);
    if (intensity < 0.1) { discard; }

    vec2 uv = v_tex - vec2(0.5);
    vec2 floored_uv = floor(uv * PIXEL_SIZE_FAC) / PIXEL_SIZE_FAC;
    vec2 uv_scaled_centered = floored_uv;
    uv_scaled_centered += uv_scaled_centered * 0.01 * (sin(-1.123 * floored_uv.x + 0.2 * u_time) * cos(5.3332 * floored_uv.y + u_time * 0.931));
    vec2 flame_up_vec = vec2(0.0, mod(4.0 * u_time, 10000.0) - 5000.0 + mod(1.781 * u_id, 1000.0));

    float scale_fac = (7.5 + 3.0 / (2.0 + 2.0 * intensity));
    vec2 sv = uv_scaled_centered * scale_fac + flame_up_vec;
    float speed = mod(20.781 * u_id, 100.0) + sin(u_time + u_id) * cos(u_time * 0.151 + u_id);
    vec2 sv2 = vec2(0.0, 0.0);

    for (int i = 0; i < 5; i++) {
        sv2 += sv + 0.05 * sv2.yx * (mod(float(i), 2.0) > 1.0 ? -1.0 : 1.0) + 0.3 * (cos(length(sv) * 0.411) + 0.3344 * sin(length(sv)) - 0.23 * cos(length(sv)));
        sv += 0.5 * vec2(
            cos(cos(sv2.y) + speed * 0.0812) * sin(3.22 + sv2.x - speed * 0.1531),
            sin(-sv2.x * 1.21222 + 0.113785 * speed) * cos(sv2.y * 0.91213 - 0.13582 * speed));
    }

    float smoke_res = max(0.0, ((length((sv - flame_up_vec) / scale_fac * 5.0) + 0.1 * (length(uv_scaled_centered) - 0.5)) * (2.0 / (2.0 + intensity * 0.2))));
    smoke_res = intensity < 0.1 ? 1.0 : smoke_res + max(0.0, 2.0 - 0.3 * intensity) * max(0.0, 2.0 * (uv_scaled_centered.y - 0.5) * (uv_scaled_centered.y - 0.5));

    if (abs(uv.x) > 0.4) smoke_res += 10.0 * (abs(uv.x) - 0.4);
    if (length((uv - vec2(0.0, 0.1)) * vec2(0.19, 1.0)) < min(0.1, intensity * 0.5) && smoke_res > 1.0) {
        smoke_res += min(8.5, intensity * 10.0) * (length((uv - vec2(0.0, 0.1)) * vec2(0.19, 1.0)) - 0.1);
    }

    vec4 ret_col = u_colour_1;
    if (smoke_res > 1.0) {
        discard;
    } else {
        if (uv.y < 0.12) {
            ret_col = ret_col * (1.0 - 0.5 * (0.12 - uv.y)) + 2.5 * (0.12 - uv.y) * u_colour_2;
            ret_col += ret_col * (-2.0 + 0.5 * intensity * smoke_res) * (0.12 - uv.y);
        }
        ret_col.a = clamp(ret_col.a, 0.0, 1.0);
    }
    gl_FragColor = ret_col;
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

FlameShaderWidget::FlameShaderWidget(QWidget *parent)
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
        if (!mClock.isValid()) mClock.start();
        const qint64 now = mClock.elapsed();
        float dt = 1.0f / 60.0f;
        if (mLastTickMs > 0) dt = qBound(0.001f, float(now - mLastTickMs) / 1000.0f, 0.050f);
        mLastTickMs = now;

        // 原版 flame_handler：timer += dt*(1 + intensity*0.2)，real_intensity 用速度平滑追 target。
        mShaderTime += dt * (1.0f + mTargetAmount * 0.2f);
        const float exptime = std::exp(-0.4f * dt);
        if (mIntensityVel < 0.0f) mIntensityVel *= (1.0f - qMin(1.0f, 10.0f * dt));
        mIntensityVel = (1.0f - exptime) * (mTargetAmount - mRealAmount) * dt * 25.0f + exptime * mIntensityVel;
        mRealAmount = qMax(0.0f, mRealAmount + mIntensityVel);

        if (mTargetAmount <= 0.0f && mRealAmount < 0.05f) {
            mTimer.stop();
            hide();
            return;
        }
        if (isVisible()) update();
    });
}

void FlameShaderWidget::start(float amount)
{
    mTargetAmount = qBound(0.0f, amount, 10.0f);
    if (!mClock.isValid()) mClock.start();
    mLastTickMs = mClock.elapsed();
    show();
    raise();
    if (!mTimer.isActive()) mTimer.start(16);
    update();
}

void FlameShaderWidget::stop()
{
    // 原版不是硬切火焰贴图，而是 real_intensity 平滑回落；这里让 target 归零，
    // 等 timer 把真实强度降到 0 后再 hide，避免黑闪/突兀消失。
    mTargetAmount = 0.0f;
    if (!mClock.isValid()) mClock.start();
    mLastTickMs = mClock.elapsed();
    if (!mTimer.isActive()) mTimer.start(16);
}

void FlameShaderWidget::setAmount(float amount)
{
    mTargetAmount = qBound(0.0f, amount, 10.0f);
    if (mTargetAmount > 0.0f && !isVisible()) start(mTargetAmount);
}

bool FlameShaderWidget::isCoolingDown() const
{
    return mTargetAmount <= 0.0f && mRealAmount > 0.05f;
}

void FlameShaderWidget::setColours(const QColor &colour1, const QColor &colour2)
{
    mColour1 = colour1;
    mColour2 = colour2;
    if (isVisible()) update();
}

void FlameShaderWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const bool ok = mProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, kQuadVertex)
                 && mProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, kFlameFragment);
    if (ok) mProgram.bindAttributeLocation("a_pos", 0);
    mReady = ok && mProgram.link();
}

void FlameShaderWidget::resizeGL(int w, int h)
{
    const qreal dpr = devicePixelRatioF();
    glViewport(0, 0, qMax(1, int(w * dpr)), qMax(1, int(h * dpr)));
}

void FlameShaderWidget::paintGL()
{
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!mReady || mRealAmount < 0.1f) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    mProgram.bind();
    mProgram.setUniformValue("u_time", mShaderTime);
    mProgram.setUniformValue("u_amount", mRealAmount);
    mProgram.setUniformValue("u_colour_1", toVec4(mColour1));
    mProgram.setUniformValue("u_colour_2", toVec4(mColour2));
    mProgram.setUniformValue("u_id", mId);
    const qreal dpr = devicePixelRatioF();
    mProgram.setUniformValue("u_screen_size", QVector2D(float(width() * dpr), float(height() * dpr)));
    const GLfloat verts[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f};
    mProgram.enableAttributeArray(0);
    mProgram.setAttributeArray(0, GL_FLOAT, verts, 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    mProgram.disableAttributeArray(0);
    mProgram.release();
}

QVector4D FlameShaderWidget::toVec4(const QColor &c) const
{
    return QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF());
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
