#ifndef AUDIOMANAGER_H
#define AUDIOMANAGER_H

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVector>

class QAudioOutput;
class QMediaPlayer;

class AudioManager : public QObject
{
    Q_OBJECT

public:
    static AudioManager *instance();

    void initialize();
    void restartMusic();
    void stopAll();

    void setDesiredMusic(const QString &trackCode);
    void setPitchMod(double pitchMod);
    void setScoreAmbient(double earnedScore,
                         double requiredScore,
                         double chipFlameReal,
                         double chipFlameChange,
                         double multFlameChange);

    void play(const QString &soundCode, double pitch = 1.0, double volume = 1.0);
    void playRandom(const QStringList &soundCodes, double pitch = 1.0, double volume = 1.0);

    // 设置界面用：实时调整音量。范围 0..1，立即生效。
    void setMasterVolume(double v);
    void setMusicVolume(double v);
    void setSfxVolume(double v);
    double masterVolume() const { return mMasterVolume; }
    double musicVolume() const { return mMusicVolume; }
    double sfxVolume() const { return mSfxVolume; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit AudioManager(QObject *parent = nullptr);

    struct MusicTrack {
        QString code;
        QMediaPlayer *player = nullptr;
        QAudioOutput *output = nullptr;
        double currentVolume = 0.0;
        double originalVolume = 0.6;
        double originalPitch = 0.7;
    };

    struct AmbientTrack {
        QString code;
        QMediaPlayer *player = nullptr;
        QAudioOutput *output = nullptr;
        double currentVolume = 0.0;
        double originalPitch = 1.0;
    };

    QString normalizedCode(QString code) const;
    QUrl resolveSoundUrl(const QString &soundCode) const;
    void ensureMusicTracks();
    void ensureAmbientTracks();
    void startAmbient(AmbientTrack &track);
    void updateMusicVolumes();
    void updateAmbientVolumes(double dt);

    QVector<MusicTrack> mTracks;
    QVector<AmbientTrack> mAmbientTracks;
    mutable QHash<QString, QUrl> mUrlCache;
    QString mDesiredMusic = QStringLiteral("music1");
    QTimer *mFadeTimer = nullptr;
    QElapsedTimer mClock;
    qint64 mLastFadeMs = 0;
    bool mInitialized = false;
    bool mButtonFilterInstalled = false;
    double mMasterVolume = 0.5;
    double mMusicVolume = 1.0;
    double mSfxVolume = 1.0;
    double mPitchMod = 1.0;
    double mAmbientEarnedScore = 0.0;
    double mAmbientRequiredScore = 0.0;
    double mAmbientChipFlameReal = 0.0;
    double mAmbientChipFlameChange = 0.0;
    double mAmbientMultFlameChange = 0.0;
};

#endif // AUDIOMANAGER_H
