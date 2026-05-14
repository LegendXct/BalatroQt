#include "splashshaderoverlay.h"

#include <QDateTime>
#include <QtMath>

namespace {
const char *kVertex = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif
attribute vec2 a_pos;
varying vec2 v_screen;
uniform vec2 u_screen_size;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
    v_screen = (a_pos * 0.5 + 0.5) * u_screen_size;
}
)GLSL";

const char *kFragment = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif

uniform float u_time;
uniform float u_vort_speed;
uniform vec4 u_colour_1;
uniform vec4 u_colour_2;
uniform float u_mid_flash;
uniform float u_vort_offset;
uniform vec2 u_screen_size;
uniform float u_opacity;
varying vec2 v_screen;

#define PIXEL_SIZE_FAC 700.0
#define BLACK (0.6*vec4(79.0/255.0,99.0/255.0,103.0/255.0,1.0/0.6))

void main() {
    float pixel_size = length(u_screen_size.xy) / PIXEL_SIZE_FAC;
    vec2 uv = (floor(v_screen.xy * (1.0 / pixel_size)) * pixel_size - 0.5 * u_screen_size.xy) / length(u_screen_size.xy);
    float uv_len = length(uv);

    float speed = u_time * u_vort_speed;
    float new_pixel_angle = atan(uv.y, uv.x) + (2.2 + 0.4 * min(6.0, speed)) * uv_len - 1.0 - speed * 0.05 - min(6.0, speed) * speed * 0.02 + u_vort_offset;
    vec2 mid = (u_screen_size.xy / length(u_screen_size.xy)) / 2.0;
    vec2 sv = vec2((uv_len * cos(new_pixel_angle) + mid.x), (uv_len * sin(new_pixel_angle) + mid.y)) - mid;

    sv *= 30.0;
    speed = u_time * 6.0 * u_vort_speed + u_vort_offset + 1033.0;
    vec2 uv2 = vec2(sv.x + sv.y);

    for (int i = 0; i < 5; i++) {
        uv2 += sin(max(sv.x, sv.y)) + sv;
        sv  += 0.5 * vec2(cos(5.1123314 + 0.353 * uv2.y + speed * 0.131121), sin(uv2.x - 0.113 * speed));
        sv  -= 1.0 * cos(sv.x + sv.y) - 1.0 * sin(sv.x * 0.711 - sv.y);
    }

    float smoke_res = min(2.0, max(-2.0, 1.5 + length(sv) * 0.12 - 0.17 * min(10.0, u_time * 1.2 - 4.0)));
    if (smoke_res < 0.2) {
        smoke_res = (smoke_res - 0.2) * 0.6 + 0.2;
    }

    float c1p = max(0.0, 1.0 - 2.0 * abs(1.0 - smoke_res));
    float c2p = max(0.0, 1.0 - 2.0 * smoke_res);
    float cb = 1.0 - min(1.0, c1p + c2p);

    vec4 ret_col = u_colour_1 * c1p + u_colour_2 * c2p + vec4(cb * BLACK.rgb, cb * u_colour_1.a);
    float mod_flash = max(u_mid_flash * 0.8, max(c1p, c2p) * 5.0 - 4.4) + u_mid_flash * max(c1p, c2p);
    vec4 col = ret_col * (1.0 - mod_flash) + mod_flash * vec4(1.0, 1.0, 1.0, 1.0);
    // 原版 splash 是叠加在已有画面上的亮色漩涡/白闪，不应该把 BLACK 区域当成实心黑幕盖住全屏。
    // 只把有涡流权重或白闪的区域输出 alpha，避免计分达标时闪黑屏。
    float visible = clamp(max(max(c1p, c2p), mod_flash), 0.0, 1.0);
    col.a = u_opacity * visible;
    gl_FragColor = col;
}
)GLSL";
}

SplashShaderOverlay::SplashShaderOverlay(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);
    hide();

    mFrameTimer.setTimerType(Qt::PreciseTimer);
    connect(&mFrameTimer, &QTimer::timeout, this, [this]() {
        if (!isVisible()) return;
        if (mDurationMs > 0 && mClock.isValid() && mClock.elapsed() >= mDurationMs) {
            mFrameTimer.stop();
            hide();
            return;
        }
        update();
    });
}

void SplashShaderOverlay::trigger(const QColor &colour1, const QColor &colour2, float vortSpeed, float midFlash, int durationMs, float maxOpacity)
{
    mColour1 = colour1;
    mColour2 = colour2;
    mVortSpeed = vortSpeed;
    mMidFlash = midFlash;
    mDurationMs = qMax(1, durationMs);
    mMaxOpacity = qBound(0.0f, maxOpacity, 1.0f);
    mVortOffset = float(std::fmod(180.30630262 * double(QDateTime::currentSecsSinceEpoch()), 100000.0));
    mClock.restart();
    show();
    raise();
    if (!mFrameTimer.isActive()) mFrameTimer.start(16);
    update();
}

void SplashShaderOverlay::initializeGL()
{
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const bool shadersOk = mProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertex)
                        && mProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragment);
    if (shadersOk) mProgram.bindAttributeLocation("a_pos", 0);
    mReady = shadersOk && mProgram.link();
}

void SplashShaderOverlay::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void SplashShaderOverlay::paintGL()
{
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!mReady || !mClock.isValid() || mDurationMs <= 0) return;

    float t = elapsedSeconds();
    float life = qBound(0.0f, float(mClock.elapsed()) / float(mDurationMs), 1.0f);
    float fade = (life < 0.18f) ? (life / 0.18f) : (1.0f - life);
    fade = qBound(0.0f, fade, 1.0f);
    float opacity = mMaxOpacity * fade;
    if (opacity <= 0.001f) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mProgram.bind();
    mProgram.setUniformValue("u_time", t);
    mProgram.setUniformValue("u_vort_speed", mVortSpeed);
    mProgram.setUniformValue("u_colour_1", toVec4(mColour1));
    mProgram.setUniformValue("u_colour_2", toVec4(mColour2));
    mProgram.setUniformValue("u_mid_flash", mMidFlash * (1.0f - life));
    mProgram.setUniformValue("u_vort_offset", mVortOffset);
    mProgram.setUniformValue("u_screen_size", QVector2D(float(width()), float(height())));
    mProgram.setUniformValue("u_opacity", opacity);

    const GLfloat verts[] = {
        -1.f, -1.f,
         1.f, -1.f,
        -1.f,  1.f,
         1.f,  1.f
    };
    mProgram.enableAttributeArray(0);
    mProgram.setAttributeArray(0, GL_FLOAT, verts, 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    mProgram.disableAttributeArray(0);
    mProgram.release();
}

QVector4D SplashShaderOverlay::toVec4(const QColor &c) const
{
    QColor cc = c;
    return QVector4D(cc.redF(), cc.greenF(), cc.blueF(), cc.alphaF());
}

float SplashShaderOverlay::elapsedSeconds() const
{
    return mClock.isValid() ? float(mClock.elapsed()) / 1000.0f : 0.0f;
}
