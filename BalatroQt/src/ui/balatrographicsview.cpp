#include "balatrographicsview.h"

#include <QOpenGLContext>
#include <QOpenGLWidget>
#include <QPainter>
#include <QResizeEvent>
#include <QSurfaceFormat>
#include <QVector2D>
#include <QVector4D>
#include <algorithm>
#include <cmath>

namespace {
static int clamp255(double v)
{
    return int(std::max(0.0, std::min(255.0, std::round(v))));
}

static QColor rgb(int r, int g, int b, int a = 255)
{
    return QColor(clamp255(r), clamp255(g), clamp255(b), clamp255(a));
}

static QColor scaleColour(const QColor &c, double k)
{
    return rgb(c.red() * k, c.green() * k, c.blue() * k, c.alpha());
}

static QColor lightenColour(const QColor &c, double amount)
{
    return rgb(c.red() + (255 - c.red()) * amount,
               c.green() + (255 - c.green()) * amount,
               c.blue() + (255 - c.blue()) * amount,
               c.alpha());
}

static QColor mixColour(const QColor &a, const QColor &b, double t)
{
    t = std::max(0.0, std::min(1.0, t));
    return rgb(a.red() + (b.red() - a.red()) * t,
               a.green() + (b.green() - a.green()) * t,
               a.blue() + (b.blue() - a.blue()) * t,
               a.alpha() + (b.alpha() - a.alpha()) * t);
}

static QColor bossColour(BossEffect effect)
{
    switch (effect) {
    case BossEffect::TheOx:          return rgb(0xb9, 0x5b, 0x08);
    case BossEffect::TheHook:        return rgb(0xa8, 0x40, 0x24);
    case BossEffect::TheMouth:       return rgb(0xae, 0x71, 0x8e);
    case BossEffect::TheFish:        return rgb(0x3e, 0x85, 0xbd);
    case BossEffect::TheClub:        return rgb(0xb9, 0xcb, 0x92);
    case BossEffect::TheManacle:     return rgb(0x57, 0x57, 0x57);
    case BossEffect::TheTooth:       return rgb(0xb5, 0x2d, 0x2d);
    case BossEffect::TheWall:        return rgb(0x8a, 0x59, 0xa5);
    case BossEffect::TheHouse:       return rgb(0x51, 0x86, 0xa8);
    case BossEffect::TheMark:        return rgb(0x6a, 0x38, 0x47);
    case BossEffect::TheWheel:       return rgb(0x50, 0xbf, 0x7c);
    case BossEffect::TheArm:         return rgb(0x68, 0x65, 0xf3);
    case BossEffect::ThePsychic:     return rgb(0xef, 0xc0, 0x3c);
    case BossEffect::TheGoad:        return rgb(0xb9, 0x5c, 0x96);
    case BossEffect::TheWater:       return rgb(0xc6, 0xe0, 0xeb);
    case BossEffect::TheEye:         return rgb(0x4b, 0x71, 0xe4);
    case BossEffect::ThePlant:       return rgb(0x70, 0x92, 0x84);
    case BossEffect::TheNeedle:      return rgb(0x5c, 0x6e, 0x31);
    case BossEffect::TheHead:        return rgb(0xac, 0x9d, 0xb4);
    case BossEffect::TheWindow:      return rgb(0xa9, 0xa2, 0x95);
    case BossEffect::TheSerpent:     return rgb(0x43, 0x9a, 0x4f);
    case BossEffect::ThePillar:      return rgb(0x7e, 0x67, 0x52);
    case BossEffect::TheFlint:       return rgb(0xe5, 0x6a, 0x2f);
    case BossEffect::AmberAcorn:     return rgb(0xfd, 0xa2, 0x00);
    case BossEffect::CeruleanBell:   return rgb(0x00, 0x9c, 0xfd);
    case BossEffect::CrimsonHeart:   return rgb(0xac, 0x32, 0x32);
    case BossEffect::VerdantLeaf:    return rgb(0x56, 0xa7, 0x86);
    case BossEffect::VioletVessel:   return rgb(0x8a, 0x71, 0xe1);
    case BossEffect::None:           return rgb(0xa8, 0x40, 0x24);
    }
    return rgb(0xa8, 0x40, 0x24);
}

static QColor bossNewColour(BossEffect effect)
{
    const QColor black = rgb(0x37, 0x42, 0x44);
    return lightenColour(mixColour(bossColour(effect), black, 0.3), 0.1);
}

static constexpr const char *kVertexShader = R"GLSL(
attribute vec2 position;
varying vec2 v_pos;
void main()
{
    v_pos = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";

static constexpr const char *kCrtVertexShader = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif
attribute highp vec2 a_pos;
attribute highp vec2 a_uv;
varying highp vec2 v_uv;
void main()
{
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)GLSL";

static constexpr const char *kCrtFragmentShader = R"GLSL(
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
varying highp vec2 v_uv;

#define BUFF 0.01
#define BLOOM_AMT 3

highp vec4 scene_tex(highp vec2 tc)
{
    return texture2D(u_scene, clamp(tc, vec2(0.0), vec2(1.0)));
}

void main()
{
    highp vec2 tc = v_uv;

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

    gl_FragColor = (final_col * (1.0 - bloom) + bloom * col) * mask;
}
)GLSL";

static constexpr const char *kFragmentShader = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif
uniform highp float time;
uniform highp float spin_time;
uniform highp vec4 colour_1;
uniform highp vec4 colour_2;
uniform highp vec4 colour_3;
uniform highp float contrast;
uniform highp float spin_amount;
uniform highp vec2 screen_size;
varying highp vec2 v_pos;

#define PIXEL_SIZE_FAC 700.0
#define SPIN_EASE 0.5

void main()
{
    highp vec2 screen_coords = v_pos * screen_size;
    highp float screen_len = length(screen_size);
    highp float pixel_size = screen_len / PIXEL_SIZE_FAC;
    highp vec2 uv = (floor(screen_coords.xy * (1.0 / pixel_size)) * pixel_size - 0.5 * screen_size.xy) / screen_len - vec2(0.12, 0.0);
    highp float uv_len = length(uv);

    highp float speed = (spin_time * SPIN_EASE * 0.2) + 302.2;
    highp float new_pixel_angle = atan(uv.y, uv.x) + speed - SPIN_EASE * 20.0 * (spin_amount * uv_len + (1.0 - spin_amount));
    highp vec2 mid = (screen_size.xy / screen_len) / 2.0;
    uv = (vec2((uv_len * cos(new_pixel_angle) + mid.x), (uv_len * sin(new_pixel_angle) + mid.y)) - mid);

    uv *= 30.0;
    speed = time * 2.0;
    highp vec2 uv2 = vec2(uv.x + uv.y);

    for (int i = 0; i < 5; i++) {
        uv2 += sin(max(uv.x, uv.y)) + uv;
        uv += 0.5 * vec2(cos(5.1123314 + 0.353 * uv2.y + speed * 0.131121), sin(uv2.x - 0.113 * speed));
        uv -= 1.0 * cos(uv.x + uv.y) - 1.0 * sin(uv.x * 0.711 - uv.y);
    }

    highp float contrast_mod = (0.25 * contrast + 0.5 * spin_amount + 1.2);
    highp float paint_res = min(2.0, max(0.0, length(uv) * 0.035 * contrast_mod));
    highp float c1p = max(0.0, 1.0 - contrast_mod * abs(1.0 - paint_res));
    highp float c2p = max(0.0, 1.0 - contrast_mod * abs(paint_res));
    highp float c3p = 1.0 - min(1.0, c1p + c2p);

    highp vec4 ret_col = (0.3 / max(contrast, 0.001)) * colour_1
        + (1.0 - 0.3 / max(contrast, 0.001)) * (colour_1 * c1p + colour_2 * c2p + vec4(c3p * colour_3.rgb, c3p * colour_1.a));
    gl_FragColor = ret_col;
}
)GLSL";
}

BalatroGraphicsView::BalatroGraphicsView(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    auto *glViewport = new QOpenGLWidget(this);
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::DefaultRenderableType);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSamples(0);
    glViewport->setFormat(fmt);
    glViewport->setAutoFillBackground(false);
    setViewport(glViewport);

    updateTargets();
    mCurrentA = mTargetA;
    mCurrentB = mTargetB;
    mCurrentC = mTargetC;
    mCurrentContrast = mTargetContrast;
    mCurrentSpin = mTargetSpin;
    mVisualsReady = true;

    mClock.start();
    mTimer.setTimerType(Qt::PreciseTimer);
    connect(&mTimer, &QTimer::timeout, this, [this]() {
        const double now = mClock.elapsed() / 1000.0;
        const double dt = std::max(0.001, std::min(0.080, now - mLastTick));
        mLastTick = now;
        mTime = now;
        mSpinTime += dt * mCurrentSpin;
        easeVisuals(dt);
        viewport()->update();
    });
    mTimer.start(16);
}

BalatroGraphicsView::~BalatroGraphicsView()
{
    if (!mSceneTexture) return;
    if (auto *glViewport = qobject_cast<QOpenGLWidget *>(viewport())) {
        glViewport->makeCurrent();
        if (!mFunctionsReady) {
            initializeOpenGLFunctions();
            mFunctionsReady = true;
        }
        glDeleteTextures(1, &mSceneTexture);
        mSceneTexture = 0;
        glViewport->doneCurrent();
    }
}

void BalatroGraphicsView::setMood(Mood mood)
{
    if (mMood == mood) return;
    mMood = mood;
    updateTargets();
    viewport()->update();
}

void BalatroGraphicsView::setBossEffect(BossEffect effect)
{
    if (mBossEffect == effect) return;
    mBossEffect = effect;
    if (mMood == Mood::Boss)
        updateTargets();
    viewport()->update();
}

void BalatroGraphicsView::setPaused(bool paused)
{
    if (paused) {
        mTimer.stop();
    } else if (!mTimer.isActive()) {
        mLastTick = mClock.elapsed() / 1000.0;
        mTimer.start(16);
    }
}

void BalatroGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    viewport()->update();
}

void BalatroGraphicsView::drawBackground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect);
    if (!painter || !QOpenGLContext::currentContext()) {
        QGraphicsView::drawBackground(painter, rect);
        return;
    }

    painter->beginNativePainting();
    renderBackgroundNative();
    painter->endNativePainting();
}

void BalatroGraphicsView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);
    if (!painter || !QOpenGLContext::currentContext()) return;

    painter->beginNativePainting();
    renderCrtNative();
    painter->endNativePainting();
}

bool BalatroGraphicsView::ensureProgram()
{
    if (!QOpenGLContext::currentContext()) return false;
    if (!mFunctionsReady) {
        initializeOpenGLFunctions();
        mFunctionsReady = true;
    }
    if (mProgramReady && mProgram) return true;

    mProgram = std::make_unique<QOpenGLShaderProgram>();
    mProgramReady = mProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)
                 && mProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader)
                 && mProgram->link();
    return mProgramReady;
}

bool BalatroGraphicsView::ensureCrtProgram()
{
    if (!QOpenGLContext::currentContext()) return false;
    if (!mFunctionsReady) {
        initializeOpenGLFunctions();
        mFunctionsReady = true;
    }
    if (mCrtProgramReady && mCrtProgram) return true;

    mCrtProgram = std::make_unique<QOpenGLShaderProgram>();
    mCrtProgramReady = mCrtProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kCrtVertexShader)
                    && mCrtProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kCrtFragmentShader)
                    && mCrtProgram->link();
    return mCrtProgramReady;
}

void BalatroGraphicsView::renderBackgroundNative()
{
    if (!ensureProgram()) return;

    const qreal dpr = devicePixelRatioF();
    const int w = qMax(1, int(std::round(viewport()->width() * dpr)));
    const int h = qMax(1, int(std::round(viewport()->height() * dpr)));
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glClearColor(mCurrentA.redF(), mCurrentA.greenF(), mCurrentA.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    mProgram->bind();
    mProgram->setUniformValue("time", GLfloat(mTime));
    mProgram->setUniformValue("spin_time", GLfloat(mSpinTime));
    mProgram->setUniformValue("contrast", GLfloat(mCurrentContrast));
    mProgram->setUniformValue("spin_amount", GLfloat(mCurrentSpin));
    mProgram->setUniformValue("screen_size", QVector2D(GLfloat(w), GLfloat(h)));
    sendColourUniform("colour_1", mCurrentA);
    sendColourUniform("colour_2", mCurrentB);
    sendColourUniform("colour_3", mCurrentC);

    static const GLfloat verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    const int loc = mProgram->attributeLocation("position");
    mProgram->enableAttributeArray(loc);
    mProgram->setAttributeArray(loc, GL_FLOAT, verts, 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    mProgram->disableAttributeArray(loc);
    mProgram->release();
}

void BalatroGraphicsView::renderCrtNative()
{
    if (!ensureCrtProgram()) return;

    const qreal dpr = devicePixelRatioF();
    const int w = qMax(1, int(std::round(viewport()->width() * dpr)));
    const int h = qMax(1, int(std::round(viewport()->height() * dpr)));

    if (!mSceneTexture) {
        glGenTextures(1, &mSceneTexture);
        glBindTexture(GL_TEXTURE_2D, mSceneTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        mSceneTextureSize = QSize();
    } else {
        glBindTexture(GL_TEXTURE_2D, mSceneTexture);
    }

    if (mSceneTextureSize != QSize(w, h)) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        mSceneTextureSize = QSize(w, h);
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, w, h);

    // QGraphicsScene has already painted into the QOpenGLWidget framebuffer.
    // Copy that GPU framebuffer directly, then redraw it through the original
    // CRT.fs math. This keeps the final pass off the CPU path.
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    mCrtProgram->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mSceneTexture);
    mCrtProgram->setUniformValue("u_scene", 0);
    mCrtProgram->setUniformValue("u_screen_size", QVector2D(GLfloat(w), GLfloat(h)));
    mCrtProgram->setUniformValue("u_time", GLfloat(400.0 + mTime));

    const float effectiveCrt = 70.0f * 0.3f;
    const float crt01 = effectiveCrt / 100.0f;
    mCrtProgram->setUniformValue("u_distortion_fac", QVector2D(1.0f + 0.07f * crt01, 1.0f + 0.10f * crt01));
    mCrtProgram->setUniformValue("u_scale_fac", QVector2D(1.0f - 0.008f * crt01, 1.0f - 0.008f * crt01));
    mCrtProgram->setUniformValue("u_feather_fac", 0.01f);
    mCrtProgram->setUniformValue("u_noise_fac", 0.001f * crt01);
    mCrtProgram->setUniformValue("u_bloom_fac", 0.0f);
    mCrtProgram->setUniformValue("u_crt_intensity", 0.16f * crt01);
    mCrtProgram->setUniformValue("u_glitch_intensity", 0.0f);
    mCrtProgram->setUniformValue("u_scanlines", float(h) * 0.75f);

    static const GLfloat positions[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    static const GLfloat uvs[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };

    const int posLoc = mCrtProgram->attributeLocation("a_pos");
    const int uvLoc = mCrtProgram->attributeLocation("a_uv");
    mCrtProgram->enableAttributeArray(posLoc);
    mCrtProgram->setAttributeArray(posLoc, GL_FLOAT, positions, 2);
    mCrtProgram->enableAttributeArray(uvLoc);
    mCrtProgram->setAttributeArray(uvLoc, GL_FLOAT, uvs, 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    mCrtProgram->disableAttributeArray(uvLoc);
    mCrtProgram->disableAttributeArray(posLoc);
    mCrtProgram->release();
}

void BalatroGraphicsView::sendColourUniform(const char *name, const QColor &c)
{
    if (!mProgram) return;
    mProgram->setUniformValue(name, QVector4D(GLfloat(c.redF()), GLfloat(c.greenF()), GLfloat(c.blueF()), GLfloat(c.alphaF())));
}

QColor BalatroGraphicsView::baseA() const
{
    const QColor black = rgb(0x37, 0x42, 0x44);
    const QColor red = rgb(0xFE, 0x5F, 0x55);
    const QColor blindSmall = rgb(0x50, 0x84, 0x6e);
    switch (mMood) {
    case Mood::Tarot:       return scaleColour(black, 0.80);
    case Mood::Spectral:    return scaleColour(black, 0.80);
    case Mood::Standard:    return red;
    case Mood::Buffoon:     return black;
    case Mood::Celestial:   return scaleColour(black, 0.90);
    case Mood::Shop:        return scaleColour(blindSmall, 0.90);
    case Mood::BlindSelect: return scaleColour(blindSmall, 0.90);
    case Mood::Boss:        return isFinisherBoss(mBossEffect) ? red : bossColour(mBossEffect);
    case Mood::Default:     return scaleColour(blindSmall, 0.90);
    }
    return scaleColour(blindSmall, 0.90);
}

QColor BalatroGraphicsView::baseB() const
{
    const QColor black = rgb(0x37, 0x42, 0x44);
    const QColor blue = rgb(0x00, 0x9d, 0xff);
    const QColor blindSmall = rgb(0x50, 0x84, 0x6e);
    switch (mMood) {
    case Mood::Tarot:       return scaleColour(rgb(0x88, 0x67, 0xa5), 1.30);
    case Mood::Spectral:    return scaleColour(rgb(0x45, 0x84, 0xfa), 1.30);
    case Mood::Standard:    return scaleColour(rgb(0x2c, 0x35, 0x36), 1.30);
    case Mood::Buffoon:     return scaleColour(rgb(0xff, 0x9a, 0x00), 1.30);
    case Mood::Celestial:   return scaleColour(black, 1.30);
    case Mood::Shop:        return scaleColour(blindSmall, 1.30);
    case Mood::BlindSelect: return scaleColour(blindSmall, 1.30);
    case Mood::Boss:        return isFinisherBoss(mBossEffect) ? blue : scaleColour(bossNewColour(mBossEffect), 1.30);
    case Mood::Default:     return scaleColour(blindSmall, 1.30);
    }
    return scaleColour(blindSmall, 1.30);
}

QColor BalatroGraphicsView::accent() const
{
    const QColor black = rgb(0x37, 0x42, 0x44);
    const QColor blindSmall = rgb(0x50, 0x84, 0x6e);
    switch (mMood) {
    case Mood::Tarot:       return scaleColour(rgb(0x88, 0x67, 0xa5), 0.40);
    case Mood::Spectral:    return scaleColour(rgb(0x45, 0x84, 0xfa), 0.40);
    case Mood::Standard:    return scaleColour(rgb(0x2c, 0x35, 0x36), 0.40);
    case Mood::Buffoon:     return scaleColour(rgb(0xff, 0x9a, 0x00), 0.40);
    case Mood::Celestial:   return scaleColour(black, 0.70);
    case Mood::Shop:        return scaleColour(blindSmall, 0.70);
    case Mood::BlindSelect: return scaleColour(blindSmall, 0.70);
    case Mood::Boss:        return isFinisherBoss(mBossEffect) ? scaleColour(black, 0.60) : scaleColour(bossNewColour(mBossEffect), 0.40);
    case Mood::Default:     return scaleColour(blindSmall, 0.70);
    }
    return scaleColour(blindSmall, 0.70);
}

double BalatroGraphicsView::targetContrast() const
{
    switch (mMood) {
    case Mood::Tarot:       return 1.5;
    case Mood::Spectral:    return 2.0;
    case Mood::Standard:    return 3.0;
    case Mood::Buffoon:     return 2.0;
    case Mood::Celestial:   return 3.0;
    case Mood::Shop:        return 1.0;
    case Mood::BlindSelect: return 1.0;
    case Mood::Boss:        return isFinisherBoss(mBossEffect) ? 3.0 : 2.0;
    case Mood::Default:     return 1.0;
    }
    return 1.0;
}

double BalatroGraphicsView::targetSpin() const
{
    switch (mMood) {
    case Mood::Tarot:       return 0.0;
    case Mood::Spectral:    return 0.0;
    case Mood::Standard:    return 0.0;
    case Mood::Buffoon:     return 0.0;
    case Mood::Celestial:   return 0.0;
    case Mood::Shop:        return 0.0;
    case Mood::BlindSelect: return 0.0;
    case Mood::Boss:        return isFinisherBoss(mBossEffect) ? 0.5 : 0.25;
    case Mood::Default:     return 0.0;
    }
    return 0.0;
}

void BalatroGraphicsView::updateTargets()
{
    mTargetA = baseA();
    mTargetB = baseB();
    mTargetC = accent();
    mTargetContrast = targetContrast();
    mTargetSpin = targetSpin();

    if (!mVisualsReady) {
        mCurrentA = mTargetA;
        mCurrentB = mTargetB;
        mCurrentC = mTargetC;
        mCurrentContrast = mTargetContrast;
        mCurrentSpin = mTargetSpin;
        mVisualsReady = true;
    }
}

void BalatroGraphicsView::easeVisuals(double dt)
{
    const double a = 1.0 - std::exp(-dt * 5.5);
    mCurrentA = mixColour(mCurrentA, mTargetA, a);
    mCurrentB = mixColour(mCurrentB, mTargetB, a);
    mCurrentC = mixColour(mCurrentC, mTargetC, a);
    mCurrentContrast += (mTargetContrast - mCurrentContrast) * a;
    mCurrentSpin += (mTargetSpin - mCurrentSpin) * a;
}
