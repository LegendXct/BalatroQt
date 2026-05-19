#ifndef FINALPOSTPROCESSWIDGET_H
#define FINALPOSTPROCESSWIDGET_H

#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPointer>
#include <QTimer>
#include <QVector>
#include <memory>

class QWidget;
class QOpenGLFramebufferObject;

// 原版 game.lua 的最后阶段是：先把完整游戏画面画到 G.CANVAS，
// 再把 G.CANVAS 送进 CRT.fs / flash.fs / vortex.fs 这类全屏 shader pass。
// 这个 widget 做同一件事：抓取统一 source widget 的最终画面，写入 OpenGL FBO，
// 然后用全屏网格跑 post-process shader，最后输出到窗口。
class FinalPostProcessWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit FinalPostProcessWidget(QWidget *source, QWidget *parent = nullptr);
    ~FinalPostProcessWidget() override;

    void setSourceWidget(QWidget *source);
    void setCrtAmount(float amount100);      // 对齐 G.SETTINGS.GRAPHICS.crt，默认 70。
    void setBloomOption(float option);       // 对齐 G.SETTINGS.GRAPHICS.bloom，默认 1。
    void setGlitchAmount(float amount);
    void triggerFlash(float midFlash = 1.0f, int durationMs = 900);
    void triggerVortex(float amount = 2.0f, int durationMs = 650);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void ensureFramebuffer();
    void rebuildMesh();
    void renderSourceToFramebuffer();
    void drawPostProcessed();
    QVector<float> mPositions;
    QVector<float> mTexCoords;

    QPointer<QWidget> mSource;
    QTimer mTimer;
    QElapsedTimer mClock;
    std::unique_ptr<QOpenGLFramebufferObject> mFramebuffer;
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
    bool mProgramReady = false;

    float mCrtAmount = 70.0f;
    float mBloomOption = 1.0f;
    float mGlitchAmount = 0.0f;
    float mFlashMid = 0.0f;
    float mFlashTime = 0.0f;
    float mFlashDuration = 0.9f;
    float mVortexStart = 0.0f;
    float mVortexDuration = 0.65f;
    float mVortexAmount = 0.0f;
    bool mFlashActive = false;
    bool mVortexActive = false;
};

#endif // FINALPOSTPROCESSWIDGET_H
