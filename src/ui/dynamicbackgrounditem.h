#ifndef DYNAMICBACKGROUNDITEM_H
#define DYNAMICBACKGROUNDITEM_H

#include <QColor>
#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QTimer>
#include <memory>

class DynamicBackgroundItem : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    enum class Mood {
        Default,
        Shop,
        Tarot,
        Spectral,
        Celestial,
        Buffoon,
        Standard,
        BlindSelect,
        Boss,
    };

    explicit DynamicBackgroundItem(QWidget *parent = nullptr);
    ~DynamicBackgroundItem() override;

    void setSceneSize(qreal w, qreal h);
    void setMood(Mood mood);
    void setPaused(bool p) {
        if (p) mTimer.stop();
        else if (!mTimer.isActive()) { mLastTick = mClock.elapsed() / 1000.0; mTimer.start(16); }
    }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    qreal mW = 1280;
    qreal mH = 720;
    qreal mTime = 0.0;
    qreal mSpinTime = 0.0;
    double mLastTick = 0.0;
    Mood mMood = Mood::Default;
    QTimer mTimer;
    QElapsedTimer mClock;
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
    bool mProgramReady = false;

    QColor mCurrentA;
    QColor mCurrentB;
    QColor mCurrentC;
    QColor mTargetA;
    QColor mTargetB;
    QColor mTargetC;
    double mCurrentContrast = 1.0;
    double mTargetContrast = 1.0;
    double mCurrentSpin = 0.0;
    double mTargetSpin = 0.0;
    bool mVisualsReady = false;

    QColor baseA() const;
    QColor baseB() const;
    QColor accent() const;
    double targetContrast() const;
    double targetSpin() const;
    void updateTargets();
    void easeVisuals(double dt);
    void sendColourUniform(const char *name, const QColor &c);
};

#endif // DYNAMICBACKGROUNDITEM_H
