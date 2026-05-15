#ifndef SPLASHSHADEROVERLAY_H
#define SPLASHSHADEROVERLAY_H

#include <QColor>
#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QTimer>
#include <QVector2D>
#include <QVector4D>

class SplashShaderOverlay : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit SplashShaderOverlay(QWidget *parent = nullptr);

    void trigger(const QColor &colour1 = QColor(0, 157, 255),
                 const QColor &colour2 = QColor(255, 255, 255),
                 float vortSpeed = 1.0f,
                 float midFlash = 0.35f,
                 int durationMs = 720,
                 float maxOpacity = 0.32f);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QVector4D toVec4(const QColor &c) const;
    float elapsedSeconds() const;

    QOpenGLShaderProgram mProgram;
    QTimer mFrameTimer;
    QElapsedTimer mClock;
    QColor mColour1 = QColor(0, 157, 255);
    QColor mColour2 = QColor(255, 255, 255);
    float mVortSpeed = 1.0f;
    float mMidFlash = 0.0f;
    float mVortOffset = 0.0f;
    float mMaxOpacity = 0.0f;
    int mDurationMs = 0;
    bool mReady = false;
};

#endif // SPLASHSHADEROVERLAY_H
