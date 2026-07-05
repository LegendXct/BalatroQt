#include "finalpostprocesswidget.h"

#include <QSurfaceFormat>
#include <QVector2D>
#include <algorithm>
#include <cmath>

namespace {
static constexpr int GRID = 64;

static constexpr const char *kVertexShader = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif
attribute highp vec2 a_pos;      // top-left based 0..1 screen position
attribute highp vec2 a_uv;
uniform highp vec2 u_screen_size;
uniform highp float u_vortex_amt;
varying highp vec2 v_uv;
varying highp vec2 v_screen;

void main()
{
    v_uv = a_uv;
    highp vec2 vertex_position = a_pos * u_screen_size;

    // 原版 vortex.fs 的 vertex stage，应用到 64x64 网格而不是 4 顶点全屏 quad，
    // 保证全屏 framebuffer pass 也有足够细分来表现原版弯曲。
    if (abs(u_vortex_amt) > 0.0001) {
        highp vec2 uv = (vertex_position.xy - 0.5 * u_screen_size.xy) / length(u_screen_size.xy);
        highp float effectRadius = 1.6 - 0.05 * u_vortex_amt;
        highp float effectAngle = 0.5 + 0.15 * u_vortex_amt;
        highp float len = length(uv * vec2(u_screen_size.x / max(u_screen_size.y, 0.0001), 1.0));
        highp float angle = atan(uv.y, uv.x) + effectAngle * smoothstep(effectRadius, 0.0, len);
        highp float radius = length(uv);
        highp vec2 center = 0.5 * u_screen_size.xy / length(u_screen_size.xy);
        vertex_position.x = (radius * cos(angle) + center.x) * length(u_screen_size.xy);
        vertex_position.y = (radius * sin(angle) + center.y) * length(u_screen_size.xy);
    }

    v_screen = vertex_position;
    highp vec2 clip = vec2(vertex_position.x / max(u_screen_size.x, 0.0001) * 2.0 - 1.0,
                           1.0 - vertex_position.y / max(u_screen_size.y, 0.0001) * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
}
)GLSL";

static constexpr const char *kFragmentShader = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif
uniform sampler2D u_scene;
uniform highp vec2 u_screen_size;
uniform highp float u_time;
uniform highp vec2 u_distortion_fac;
uniform highp vec2 u_scale_fac;
uniform highp float u_feather_fac;
uniform highp float u_noise_fac;
uniform highp float u_bloom_fac;
uniform highp float u_crt_intensity;
uniform highp float u_glitch_intensity;
uniform highp float u_scanlines;
uniform highp float u_flash_time;
uniform highp float u_mid_flash;
varying highp vec2 v_uv;
varying highp vec2 v_screen;

#define BUFF 0.01
#define BLOOM_AMT 3
#define PIXEL_SIZE_FAC 700.0

highp vec4 scene_tex(highp vec2 tc)
{
    return texture2D(u_scene, tc);
}

void main()
{
    highp vec2 tc = v_uv;
    highp vec2 orig_tc = tc;

    // CRT.fs 原公式：recenter -> scale -> bulge -> edge feather -> sample。
    tc = tc * 2.0 - vec2(1.0);
    tc *= u_scale_fac;
    tc += (tc.yx * tc.yx) * tc * (u_distortion_fac - 1.0);

    highp float mask = (1.0 - smoothstep(1.0 - u_feather_fac, 1.0, abs(tc.x) - BUFF))
                     * (1.0 - smoothstep(1.0 - u_feather_fac, 1.0, abs(tc.y) - BUFF));

    tc = (tc + vec2(1.0)) / 2.0;

    highp float offset_l = 0.0;
    highp float offset_r = 0.0;
    if (u_glitch_intensity > 0.01) {
        highp float timefac = 3.0 * u_time;
        offset_l = 50.0 * (-3.5 + sin(timefac * 0.512 + tc.y * 40.0)
            + sin(-timefac * 0.8233 + tc.y * 81.532)
            + sin(timefac * 0.333 + tc.y * 30.3)
            + sin(-timefac * 0.1112331 + tc.y * 13.0));
        offset_r = -50.0 * (-3.5 + sin(timefac * 0.6924 + tc.y * 29.0)
            + sin(-timefac * 0.9661 + tc.y * 41.532)
            + sin(timefac * 0.4423 + tc.y * 40.3)
            + sin(-timefac * 0.13321312 + tc.y * 11.0));

        if (u_glitch_intensity > 1.0) {
            offset_l = 50.0 * (-1.5 + sin(timefac * 0.512 + tc.y * 4.0)
                + sin(-timefac * 0.8233 + tc.y * 1.532)
                + sin(timefac * 0.333 + tc.y * 3.3)
                + sin(-timefac * 0.1112331 + tc.y * 1.0));
            offset_r = -50.0 * (-1.5 + sin(timefac * 0.6924 + tc.y * 19.0)
                + sin(-timefac * 0.9661 + tc.y * 21.532)
                + sin(timefac * 0.4423 + tc.y * 20.3)
                + sin(-timefac * 0.13321312 + tc.y * 5.0));
        }
        tc.x = tc.x + 0.001 * u_glitch_intensity * clamp(offset_l, clamp(offset_r, -1.0, 0.0), 1.0);
    }

    highp vec4 crt_tex = scene_tex(tc);
    highp float artifact_amplifier = (abs(clamp(offset_l, clamp(offset_r, -1.0, 0.0), 1.0)) * u_glitch_intensity > 0.9 ? 3.0 : 1.0);
    highp float crt_amount_adjusted = max(0.0, u_crt_intensity / (0.16 * 0.3)) * artifact_amplifier;

    if (crt_amount_adjusted > 0.0000001) {
        highp float aberr = 0.0005 * (1.0 + 10.0 * (artifact_amplifier - 1.0)) * 1600.0 / max(u_screen_size.x, 1.0);
        crt_tex.r = crt_tex.r * (1.0 - crt_amount_adjusted) + crt_amount_adjusted * scene_tex(tc + vec2( aberr, 0.0)).r;
        crt_tex.g = crt_tex.g * (1.0 - crt_amount_adjusted) + crt_amount_adjusted * scene_tex(tc + vec2(-aberr, 0.0)).g;
    }

    highp vec3 rgb_result = crt_tex.rgb * (1.0 - (1.0 * u_crt_intensity * artifact_amplifier));

    if (sin(u_time + tc.y * 200.0) > 0.85) {
        if (offset_l < 0.99 && offset_l > 0.01) rgb_result.r = rgb_result.g * 1.5;
        if (offset_r > -0.99 && offset_r < -0.01) rgb_result.g = rgb_result.r * 1.5;
    }

    highp vec3 rgb_scanline = vec3(
        clamp(-0.3 + 2.0 * sin(tc.y * u_scanlines - 3.14 / 4.0) - 0.8 * clamp(sin(tc.x * u_scanlines * 4.0), 0.4, 1.0), -1.0, 2.0),
        clamp(-0.3 + 2.0 * cos(tc.y * u_scanlines) - 0.8 * clamp(cos(tc.x * u_scanlines * 4.0), 0.0, 1.0), -1.0, 2.0),
        clamp(-0.3 + 2.0 * cos(tc.y * u_scanlines - 3.14 / 3.0) - 0.8 * clamp(cos(tc.x * u_scanlines * 4.0 - 3.14 / 4.0), 0.0, 1.0), -1.0, 2.0));
    rgb_result += crt_tex.rgb * rgb_scanline * u_crt_intensity * artifact_amplifier;

    highp float x = (tc.x - mod(tc.x, 0.002)) * (tc.y - mod(tc.y, 0.0013)) * u_time * 1000.0;
    x = mod(x, 13.0) * mod(x, 123.0);
    highp float dx = mod(x, 0.11) / 0.11;
    rgb_result = (1.0 - clamp(u_noise_fac * artifact_amplifier, 0.0, 1.0)) * rgb_result
               + dx * clamp(u_noise_fac * artifact_amplifier, 0.0, 1.0) * vec3(1.0);

    rgb_result -= vec3(0.55 - 0.02 * (artifact_amplifier - 1.0 - crt_amount_adjusted * u_bloom_fac * 0.7));
    rgb_result = rgb_result * (1.0 + 0.14 + crt_amount_adjusted * (0.012 - u_bloom_fac * 0.12));
    rgb_result += vec3(0.5);

    highp vec4 final_col = vec4(rgb_result, 1.0);

    highp vec4 col = vec4(0.0);
    highp float bloom = 0.0;
    if (u_bloom_fac > 0.00001 && u_crt_intensity > 0.000001) {
        bloom = 0.03 * max(0.0, u_crt_intensity / (0.16 * 0.3));
        highp float bloom_dist = 0.0015 * float(BLOOM_AMT);
        highp float cutoff = 0.6;
        for (int i = -BLOOM_AMT; i <= BLOOM_AMT; ++i) {
            for (int j = -BLOOM_AMT; j <= BLOOM_AMT; ++j) {
                highp vec4 samp = scene_tex(tc + (bloom_dist / float(BLOOM_AMT)) * vec2(float(i), float(j)));
                samp.r = max(1.0 / (1.0 - cutoff) * samp.r - 1.0 / (1.0 - cutoff) + 1.0, 0.0);
                samp.g = max(1.0 / (1.0 - cutoff) * samp.g - 1.0 / (1.0 - cutoff) + 1.0, 0.0);
                samp.b = max(1.0 / (1.0 - cutoff) * samp.b - 1.0 / (1.0 - cutoff) + 1.0, 0.0);
                col += min(min(samp.r, samp.g), samp.b) * (2.0 - abs(float(i + j)) / float(BLOOM_AMT + BLOOM_AMT));
            }
        }
        col /= float(BLOOM_AMT * BLOOM_AMT);
        col.a = final_col.a;
    }

    highp vec4 out_col = (final_col * (1.0 - bloom) + bloom * col) * mask;

    // flash.fs 原公式，作为最终 framebuffer pass 中的白闪混合。
    if (u_mid_flash > 0.0001) {
        highp float pixel_size = length(u_screen_size.xy) / PIXEL_SIZE_FAC;
        highp vec2 fuv = (floor(v_screen.xy * (1.0 / pixel_size)) * pixel_size - 0.5 * u_screen_size.xy) / length(u_screen_size.xy);
        highp float mid_white = min(1.0,
            (u_flash_time > 2.5 ? max(0.0, sqrt(u_flash_time - 2.5) - 60.0 * length(fuv)) : 0.0)
          + (u_flash_time > 11.0 ? max(0.0, (u_flash_time - 11.0) * (u_flash_time - 11.0) - 5.0 * length(fuv)) : 0.0));
        highp float a = clamp(u_mid_flash * mid_white, 0.0, 1.0);
        out_col = mix(out_col, vec4(1.0), a);
    }

    gl_FragColor = out_col;
}
)GLSL";
}

FinalPostProcessWidget::FinalPostProcessWidget(QWidget *source, QWidget *parent)
    : QOpenGLWidget(parent)
    , mSource(source)
{
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::DefaultRenderableType);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setAlphaBufferSize(8);
    fmt.setSamples(0);
    setFormat(fmt);

    setAutoFillBackground(false);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_AlwaysStackOnTop, true);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);

    mClock.start();
    mTimer.setTimerType(Qt::PreciseTimer);
    connect(&mTimer, &QTimer::timeout, this, [this]() {
        if (mFlashActive) {
            mFlashTime += 16.0f / 1000.0f * (13.0f / std::max(0.05f, mFlashDuration));
            if (mFlashTime > 13.25f) { mFlashActive = false; mFlashMid = 0.0f; }
        }
        if (mVortexActive) {
            const float elapsed = (mClock.elapsed() / 1000.0f) - mVortexStart;
            if (elapsed >= mVortexDuration) { mVortexActive = false; mVortexAmount = 0.0f; }
        }
        if (isVisible()) {
            captureSourceSnapshot();
            update();
        }
    });
    mTimer.start(16);
}

FinalPostProcessWidget::~FinalPostProcessWidget()
{
    makeCurrent();
    if (mSceneTexture) {
        glDeleteTextures(1, &mSceneTexture);
        mSceneTexture = 0;
    }
    mProgram.reset();
    doneCurrent();
}

void FinalPostProcessWidget::setSourceWidget(QWidget *source)
{
    mSource = source;
    update();
}

void FinalPostProcessWidget::setCrtAmount(float amount100)
{
    mCrtAmount = std::max(0.0f, std::min(100.0f, amount100));
    update();
}

void FinalPostProcessWidget::setBloomOption(float option)
{
    mBloomOption = option;
    update();
}

void FinalPostProcessWidget::setGlitchAmount(float amount)
{
    mGlitchAmount = std::max(0.0f, amount);
    update();
}

void FinalPostProcessWidget::triggerFlash(float midFlash, int durationMs)
{
    mFlashMid = std::max(0.0f, midFlash);
    mFlashDuration = std::max(0.05f, durationMs / 1000.0f);
    mFlashTime = 0.0f;
    mFlashActive = true;
    show();
    raise();
    update();
}

void FinalPostProcessWidget::triggerVortex(float amount, int durationMs)
{
    mVortexAmount = amount;
    mVortexDuration = std::max(0.05f, durationMs / 1000.0f);
    mVortexStart = mClock.elapsed() / 1000.0f;
    mVortexActive = true;
    show();
    raise();
    update();
}

void FinalPostProcessWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    mProgram = std::make_unique<QOpenGLShaderProgram>();
    const bool ok = mProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)
                 && mProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader)
                 && mProgram->link();
    mProgramReady = ok;
    glGenTextures(1, &mSceneTexture);
    rebuildMesh();
}

void FinalPostProcessWidget::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);
    rebuildMesh();
}

void FinalPostProcessWidget::rebuildMesh()
{
    mPositions.clear();
    mTexCoords.clear();
    mPositions.reserve(GRID * GRID * 6 * 2);
    mTexCoords.reserve(GRID * GRID * 6 * 2);
    auto add = [this](float x, float y) {
        mPositions << x << y;
        mTexCoords << x << y;
    };
    for (int y = 0; y < GRID; ++y) {
        for (int x = 0; x < GRID; ++x) {
            const float x0 = float(x) / float(GRID);
            const float y0 = float(y) / float(GRID);
            const float x1 = float(x + 1) / float(GRID);
            const float y1 = float(y + 1) / float(GRID);
            add(x0, y0); add(x1, y0); add(x0, y1);
            add(x1, y0); add(x1, y1); add(x0, y1);
        }
    }
}

void FinalPostProcessWidget::captureSourceSnapshot()
{
    if (!mSource || !mSource->isVisible() || mSource->width() <= 0 || mSource->height() <= 0) {
        return;
    }

    // Do not call QWidget::grab() from paintGL(). On Qt/MinGW Release builds
    // that can re-enter QWidget backing-store rendering while an OpenGL paint
    // is active and crash. The timer captures a complete UI frame first; the
    // GL pass only uploads and post-processes that cached frame.
    mSnapshot = mSource->grab().toImage().convertToFormat(QImage::Format_RGBA8888);
}

bool FinalPostProcessWidget::uploadSnapshotTexture()
{
    if (!mSceneTexture) return false;
    if (mSnapshot.isNull()) captureSourceSnapshot();
    if (mSnapshot.isNull()) return false;

    const QImage image = mSnapshot.convertToFormat(QImage::Format_RGBA8888);
    mTextureSize = image.size();

    glBindTexture(GL_TEXTURE_2D, mSceneTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void FinalPostProcessWidget::drawPostProcessed()
{
    if (!mSceneTexture || mTextureSize.isEmpty() || !mProgramReady || !mProgram) return;

    glViewport(0, 0, std::max(1, int(width() * devicePixelRatioF())), std::max(1, int(height() * devicePixelRatioF())));
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_BLEND);

    const float real = mClock.elapsed() / 1000.0f;
    const float effectiveCrt = mCrtAmount * 0.3f; // game.lua 在发送 CRT uniform 前做 crt *= 0.3。
    const float crt01 = effectiveCrt / 100.0f;
    const float vortex = mVortexActive
        ? mVortexAmount * std::max(0.0f, 1.0f - ((real - mVortexStart) / std::max(0.05f, mVortexDuration)))
        : 0.0f;

    mProgram->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mSceneTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    mProgram->setUniformValue("u_scene", 0);
    mProgram->setUniformValue("u_screen_size", QVector2D(float(mTextureSize.width()), float(mTextureSize.height())));
    mProgram->setUniformValue("u_time", 400.0f + real);
    mProgram->setUniformValue("u_distortion_fac", QVector2D(1.0f + 0.07f * crt01, 1.0f + 0.10f * crt01));
    mProgram->setUniformValue("u_scale_fac", QVector2D(1.0f - 0.008f * crt01, 1.0f - 0.008f * crt01));
    mProgram->setUniformValue("u_feather_fac", 0.01f);
    mProgram->setUniformValue("u_bloom_fac", mBloomOption - 1.0f);
    mProgram->setUniformValue("u_noise_fac", 0.001f * crt01);
    mProgram->setUniformValue("u_crt_intensity", 0.16f * crt01);
    mProgram->setUniformValue("u_glitch_intensity", mGlitchAmount);
    mProgram->setUniformValue("u_scanlines", float(mTextureSize.height()) * 0.75f);
    mProgram->setUniformValue("u_flash_time", mFlashTime);
    mProgram->setUniformValue("u_mid_flash", mFlashActive ? mFlashMid : 0.0f);
    mProgram->setUniformValue("u_vortex_amt", vortex);

    const int posLoc = mProgram->attributeLocation("a_pos");
    const int uvLoc = mProgram->attributeLocation("a_uv");
    mProgram->enableAttributeArray(posLoc);
    mProgram->enableAttributeArray(uvLoc);
    mProgram->setAttributeArray(posLoc, GL_FLOAT, mPositions.constData(), 2);
    mProgram->setAttributeArray(uvLoc, GL_FLOAT, mTexCoords.constData(), 2);
    glDrawArrays(GL_TRIANGLES, 0, mPositions.size() / 2);
    mProgram->disableAttributeArray(posLoc);
    mProgram->disableAttributeArray(uvLoc);
    glBindTexture(GL_TEXTURE_2D, 0);
    mProgram->release();
}

void FinalPostProcessWidget::paintGL()
{
    if (!mProgramReady || !mProgram) return;
    if (!uploadSnapshotTexture()) return;
    drawPostProcessed();
}
