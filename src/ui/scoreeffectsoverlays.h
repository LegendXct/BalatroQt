#ifndef SCOREEFFECTSOVERLAYS_H
#define SCOREEFFECTSOVERLAYS_H

#include <QColor>
#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QTimer>
#include <QVector2D>
#include <QVector4D>

class FlameShaderWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit FlameShaderWidget(QWidget *parent = nullptr);
    void start(float amount = 1.0f);
    void stop();
    void setAmount(float amount);
    bool isCoolingDown() const;
    void setColours(const QColor &colour1, const QColor &colour2);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QVector4D toVec4(const QColor &c) const;

    QOpenGLShaderProgram mProgram;
    QTimer mTimer;
    QElapsedTimer mClock;
    QColor mColour1 = QColor(255, 111, 0, 235);
    QColor mColour2 = QColor(255, 230, 120, 255);
    float mTargetAmount = 0.0f;
    float mRealAmount = 0.0f;
    float mIntensityVel = 0.0f;
    float mShaderTime = 0.0f;
    qint64 mLastTickMs = 0;
    float mId = 13.37f;
    bool mReady = false;
};

class FlashShaderOverlay : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit FlashShaderOverlay(QWidget *parent = nullptr);
    void trigger(float midFlash = 0.65f, int durationMs = 360);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QOpenGLShaderProgram mProgram;
    QTimer mTimer;
    QElapsedTimer mClock;
    float mMidFlash = 0.0f;
    int mDurationMs = 0;
    bool mReady = false;
};

#endif // SCOREEFFECTSOVERLAYS_H
