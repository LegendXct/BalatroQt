#include "audiomanager.h"

#include <QAbstractButton>
#include <QAudioOutput>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

AudioManager *AudioManager::instance()
{
    static AudioManager *manager = new AudioManager(QCoreApplication::instance());
    return manager;
}

AudioManager::AudioManager(QObject *parent)
    : QObject(parent),
      mFadeTimer(new QTimer(this))
{
    // 音乐音量过渡每 16 ms 跑一次会和背景着色器/动画抢主线程；50 ms 对淡入淡出听感无明显差别。
    mFadeTimer->setInterval(50);
    mFadeTimer->setTimerType(Qt::CoarseTimer);
    connect(mFadeTimer, &QTimer::timeout, this, &AudioManager::updateMusicVolumes);
    mClock.start();
    mLastFadeMs = mClock.elapsed();
}

void AudioManager::initialize()
{
    if (mInitialized) return;
    mInitialized = true;

    if (QCoreApplication::instance() && !mButtonFilterInstalled) {
        QCoreApplication::instance()->installEventFilter(this);
        mButtonFilterInstalled = true;
    }

    restartMusic();
}

QString AudioManager::normalizedCode(QString code) const
{
    code = code.trimmed();
    if (code.endsWith(QStringLiteral(".ogg"), Qt::CaseInsensitive))
        code.chop(4);
    return code;
}

QUrl AudioManager::resolveSoundUrl(const QString &soundCode) const
{
    const QString code = normalizedCode(soundCode);
    if (code.isEmpty()) return QUrl();
    if (mUrlCache.contains(code)) return mUrlCache.value(code);

    const QStringList resourcePaths = {
        QStringLiteral(":/sounds/%1.ogg").arg(code),
        QStringLiteral(":/sounds/sounds/%1.ogg").arg(code),
    };
    for (const QString &path : resourcePaths) {
        if (QFile::exists(path)) {
            const QUrl url(QStringLiteral("qrc%1").arg(path));
            mUrlCache.insert(code, url);
            return url;
        }
    }

    QStringList roots;
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) roots << appDir;
    roots << QDir::currentPath();

    QDir up(appDir);
    for (int i = 0; i < 6 && up.cdUp(); ++i)
        roots << up.absolutePath();

    QDir sourceDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
    if (sourceDir.cd(QStringLiteral("../..")))
        roots << sourceDir.absolutePath();

    QSet<QString> seen;
    const QStringList relativeFiles = {
        QStringLiteral("resources/%1.ogg").arg(code),
        QStringLiteral("resources/sounds/%1.ogg").arg(code),
    };
    for (const QString &root : roots) {
        const QString canonicalRoot = QDir(root).absolutePath();
        if (seen.contains(canonicalRoot)) continue;
        seen.insert(canonicalRoot);
        const QDir dir(canonicalRoot);
        for (const QString &rel : relativeFiles) {
            const QString file = dir.absoluteFilePath(rel);
            if (QFileInfo::exists(file)) {
                const QUrl url = QUrl::fromLocalFile(file);
                mUrlCache.insert(code, url);
                return url;
            }
        }
    }

    mUrlCache.insert(code, QUrl());
    return QUrl();
}

void AudioManager::ensureMusicTracks()
{
    if (!mTracks.isEmpty()) return;

    const QStringList musicCodes = {
        QStringLiteral("music1"),
        QStringLiteral("music2"),
        QStringLiteral("music3"),
        QStringLiteral("music4"),
        QStringLiteral("music5"),
    };

    for (const QString &code : musicCodes) {
        const QUrl url = resolveSoundUrl(code);
        if (!url.isValid()) continue;

        MusicTrack track;
        track.code = code;
        track.player = new QMediaPlayer(this);
        track.output = new QAudioOutput(track.player);
        track.output->setVolume(0.0);
        track.player->setAudioOutput(track.output);
        track.player->setSource(url);
        track.player->setPlaybackRate(track.originalPitch * mPitchMod);
        track.player->setLoops(QMediaPlayer::Infinite);
        track.player->play();

        mTracks.append(track);
    }

    if (!mTracks.isEmpty() && !mFadeTimer->isActive()) {
        mLastFadeMs = mClock.elapsed();
        mFadeTimer->start();
    }
}

void AudioManager::ensureAmbientTracks()
{
    if (!mAmbientTracks.isEmpty()) return;

    auto addTrack = [this](const QString &code, double pitch) {
        AmbientTrack track;
        track.code = code;
        track.originalPitch = pitch;
        mAmbientTracks.append(track);
    };

    addTrack(QStringLiteral("ambientFire2"), 1.05);
    addTrack(QStringLiteral("ambientFire1"), 1.10);
    addTrack(QStringLiteral("ambientFire3"), 1.00);
    addTrack(QStringLiteral("ambientOrgan1"), 0.70);
}

void AudioManager::startAmbient(AmbientTrack &track)
{
    if (track.player && track.output) return;

    const QUrl url = resolveSoundUrl(track.code);
    if (!url.isValid()) return;

    track.player = new QMediaPlayer(this);
    track.output = new QAudioOutput(track.player);
    track.output->setVolume(0.0);
    track.player->setAudioOutput(track.output);
    track.player->setSource(url);
    track.player->setPlaybackRate(track.originalPitch);
    track.player->setLoops(QMediaPlayer::Infinite);
    track.player->play();
}

void AudioManager::restartMusic()
{
    ensureMusicTracks();
    for (MusicTrack &track : mTracks) {
        if (!track.player || !track.output) continue;
        track.currentVolume = 0.0;
        track.output->setVolume(0.0);
        track.player->setPosition(0);
        track.player->setPlaybackRate(track.originalPitch * mPitchMod);
        track.player->play();
    }
    updateMusicVolumes();
}

void AudioManager::stopAll()
{
    for (MusicTrack &track : mTracks) {
        if (track.player) track.player->stop();
        if (track.output) track.output->setVolume(0.0);
        track.currentVolume = 0.0;
    }
    for (AmbientTrack &track : mAmbientTracks) {
        if (track.player) track.player->stop();
        if (track.output) track.output->setVolume(0.0);
        track.currentVolume = 0.0;
    }
}

void AudioManager::setDesiredMusic(const QString &trackCode)
{
    mDesiredMusic = normalizedCode(trackCode);
    ensureMusicTracks();
    updateMusicVolumes();
}

void AudioManager::setPitchMod(double pitchMod)
{
    mPitchMod = qBound(0.25, pitchMod, 2.0);
    for (MusicTrack &track : mTracks) {
        if (track.player)
            track.player->setPlaybackRate(track.originalPitch * mPitchMod);
    }
}

void AudioManager::setScoreAmbient(double earnedScore,
                                   double requiredScore,
                                   double chipFlameReal,
                                   double chipFlameChange,
                                   double multFlameChange)
{
    mAmbientEarnedScore = earnedScore;
    mAmbientRequiredScore = requiredScore;
    mAmbientChipFlameReal = chipFlameReal;
    mAmbientChipFlameChange = chipFlameChange;
    mAmbientMultFlameChange = multFlameChange;
    ensureAmbientTracks();
    if (!mFadeTimer->isActive()) {
        mLastFadeMs = mClock.elapsed();
        mFadeTimer->start();
    }
}

void AudioManager::updateMusicVolumes()
{
    const qint64 now = mClock.elapsed();
    const double dt = qMax(0.0, (now - mLastFadeMs) / 1000.0);
    mLastFadeMs = now;

    const double blend = qBound(0.0, dt * 3.0, 1.0);
    for (MusicTrack &track : mTracks) {
        const double target = (track.code == mDesiredMusic) ? 1.0 : 0.0;
        track.currentVolume = target * blend + (1.0 - blend) * track.currentVolume;

        if (track.output) {
            const double volume = track.currentVolume
                                  * track.originalVolume
                                  * mMasterVolume
                                  * mMusicVolume;
            track.output->setVolume(qBound(0.0, volume, 1.0));
        }
        if (track.player && track.player->playbackState() != QMediaPlayer::PlayingState)
            track.player->play();
    }
    updateAmbientVolumes(dt);
}

void AudioManager::updateAmbientVolumes(double dt)
{
    if (mAmbientTracks.isEmpty()) return;

    double earned = std::isfinite(mAmbientEarnedScore) ? mAmbientEarnedScore : 0.0;
    double required = std::isfinite(mAmbientRequiredScore) ? mAmbientRequiredScore : 0.0;
    const double log5 = std::log(5.0);

    double flames = std::min(1.0, (mAmbientChipFlameReal + mAmbientChipFlameChange) / 10.0);
    flames = qMax(0.0, flames);

    double organ = 0.0;
    if (required > 0.0 && earned > 0.0) {
        organ = std::max(std::min(0.4, 0.1 * std::log(earned / (required + 1.0)) / log5), 0.0);
    }

    auto targetFor = [flames, organ, this](const QString &code) {
        if (code == QStringLiteral("ambientFire2"))
            return 0.9 * ((flames > 0.3) ? 1.0 : flames / 0.3);
        if (code == QStringLiteral("ambientFire1"))
            return 0.8 * ((flames > 0.3) ? (flames - 0.3) / 0.7 : 0.0);
        if (code == QStringLiteral("ambientFire3"))
            return 0.4 * (mAmbientChipFlameChange + mAmbientMultFlameChange);
        if (code == QStringLiteral("ambientOrgan1"))
            return 0.6 * ((mMusicVolume + 1.0) / 2.0) * organ;
        return 0.0;
    };

    for (AmbientTrack &track : mAmbientTracks) {
        const double target = targetFor(track.code);
        track.currentVolume = track.currentVolume * (1.0 - dt) + dt * target;

        if (track.currentVolume <= 0.0) {
            if (track.output) track.output->setVolume(0.0);
            if (track.player) track.player->stop();
            continue;
        }

        startAmbient(track);
        if (track.output) {
            const double volume = track.currentVolume
                                  * mMasterVolume
                                  * mSfxVolume;
            track.output->setVolume(qBound(0.0, volume, 1.0));
        }
        if (track.player) {
            track.player->setPlaybackRate(track.originalPitch);
            if (track.player->playbackState() != QMediaPlayer::PlayingState)
                track.player->play();
        }
    }
}

void AudioManager::play(const QString &soundCode, double pitch, double volume)
{
    const QString code = normalizedCode(soundCode);
    if (code.isEmpty()) return;
    if (code.startsWith(QStringLiteral("music"))) {
        setDesiredMusic(code);
        return;
    }

    const QUrl url = resolveSoundUrl(code);
    if (!url.isValid()) return;

    auto *voice = new QObject(this);
    auto *player = new QMediaPlayer(voice);
    auto *output = new QAudioOutput(voice);
    output->setVolume(qBound(0.0, volume * mMasterVolume * mSfxVolume, 1.0));
    player->setAudioOutput(output);
    player->setSource(url);
    player->setPlaybackRate(qBound(0.05, pitch, 3.0));

    connect(player, &QMediaPlayer::mediaStatusChanged, voice,
            [voice](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia)
            voice->deleteLater();
    });
    connect(player, &QMediaPlayer::playbackStateChanged, voice,
            [voice, player](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::StoppedState && player->position() > 0)
            voice->deleteLater();
    });
    QTimer::singleShot(12000, voice, &QObject::deleteLater);

    player->play();
}

void AudioManager::setMasterVolume(double v)
{
    mMasterVolume = qBound(0.0, v, 1.0);
    if (!mFadeTimer->isActive()) mFadeTimer->start();
    updateMusicVolumes();
}

void AudioManager::setMusicVolume(double v)
{
    mMusicVolume = qBound(0.0, v, 1.0);
    if (!mFadeTimer->isActive()) mFadeTimer->start();
    updateMusicVolumes();
}

void AudioManager::setSfxVolume(double v)
{
    mSfxVolume = qBound(0.0, v, 1.0);
}

void AudioManager::playRandom(const QStringList &soundCodes, double pitch, double volume)
{
    if (soundCodes.isEmpty()) return;
    const int idx = QRandomGenerator::global()->bounded(int(soundCodes.size()));
    play(soundCodes[idx], pitch, volume);
}

bool AudioManager::eventFilter(QObject *watched, QEvent *event)
{
    auto *button = qobject_cast<QAbstractButton *>(watched);
    if (!button || !button->isEnabled())
        return QObject::eventFilter(watched, event);

    auto playForButton = [this, button]() {
        const QString configured = button->property("balatroAudio").toString();
        if (configured == QStringLiteral("none")) return;
        const QString code = configured.isEmpty() ? QStringLiteral("button") : configured;
        const double volume = (code == QStringLiteral("button")) ? 0.3 : 0.72;
        play(code, 1.0, volume);
    };

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton &&
            button->rect().contains(mouse->position().toPoint())) {
            playForButton();
        }
    } else if (event->type() == QEvent::KeyRelease) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return ||
            key->key() == Qt::Key_Enter ||
            key->key() == Qt::Key_Space) {
            playForButton();
        }
    }

    return QObject::eventFilter(watched, event);
}
