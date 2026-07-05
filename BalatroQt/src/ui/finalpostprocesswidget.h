#ifndef FINALPOSTPROCESSWIDGET_H
#define FINALPOSTPROCESSWIDGET_H

#include <QElapsedTimer>
#include <QImage>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPointer>
#include <QSize>
#include <QTimer>
#include <QVector>
#include <memory>

class QWidget;

// Original game.lua draws the complete scene to G.CANVAS first, then sends that
// final frame through CRT.fs / flash.fs / vortex.fs as a full-screen pass.
// This widget mirrors that pipeline: capture one complete Qt frame, upload it
// as a texture, then draw the post-processed result over the window.
class FinalPostProcessWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit FinalPostProcessWidget(QWidget *source, QWidget *parent = nullptr);
    ~FinalPostProcessWidget() override;

    void setSourceWidget(QWidget *source);
    void setCrtAmount(float amount100);
    void setBloomOption(float option);
    void setGlitchAmount(float amount);
    void triggerFlash(float midFlash = 1.0f, int durationMs = 900);
    void triggerVortex(float amount = 2.0f, int durationMs = 650);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void rebuildMesh();
    void captureSourceSnapshot();
    bool uploadSnapshotTexture();
    void drawPostProcessed();

    QVector<float> mPositions;
    QVector<float> mTexCoords;

    QPointer<QWidget> mSource;
    QImage mSnapshot;
    QSize mTextureSize;
    QTimer mTimer;
    QElapsedTimer mClock;
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
    unsigned int mSceneTexture = 0;
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
