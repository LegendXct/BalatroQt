#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontDatabase>
#include <QGraphicsProxyWidget>
#include <QGraphicsPixmapItem>
#include <algorithm>
#include <QTimer>
#include <QGuiApplication>
#include <QScreen>
#include <QMenuBar>
#include <QStatusBar>
#include <QPauseAnimation>
#include <QSequentialAnimationGroup>
#include <QMenu>
#include <QPropertyAnimation>
#include <QCursor>
#include <QStringList>
#include "shopsignwidget.h"
#include "scoreeffectsoverlays.h"   // FlameShaderWidget（GPU 火焰）
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QPointer>
#include <QVariantAnimation>
#include <QGraphicsDropShadowEffect>
#include <QSlider>
#include <QCheckBox>
#include <QColor>
#include <QProgressBar>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QApplication>
#include <QDialog>
#include <QTabWidget>
#include <QTextEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QFrame>
#include <QAbstractItemView>
#include <QRandomGenerator>
#include <cmath>
#include <limits>
#include "../utils/shadereffects.h"
#include "../audio/audiomanager.h"
#include "balatroinfopanel.h"
#include "cardtooltipformat.h"

namespace {
constexpr int DESIGN_WINDOW_W = 1920;
constexpr int DESIGN_WINDOW_H = 1080;
// 原版 Balatro 的 HUD 约占窗口宽度的 20%（≈ 4.5 / 22 tiles），1920 设计稿对应 ~380 px。
constexpr int DESIGN_LEFT_W   = 380;
constexpr int DESIGN_SCENE_W  = DESIGN_WINDOW_W - DESIGN_LEFT_W;
constexpr int DESIGN_SCENE_H  = DESIGN_WINDOW_H;

double gUiScale = 1.0;

static double calcUiScale(QScreen *screen)
{
    if (!screen) return 1.0;

    // Qt 在开启高 DPI 后，availableGeometry() 返回的就是“逻辑像素”——也是
    // QWidget 自己用来布局的单位。之前混入 devicePixelRatio 取 qMax 会让高 DPR
    // 屏（笔记本系统缩放 150%/175% 等）误以为屏幕特别大，dp(380) 计算出的左侧
    // 信息栏被撑到 600+ 逻辑像素，挤压右侧牌桌。
    // 因此只用 logicalScale；Qt 内部会按设备像素比把整张窗口放大显示，UI 元素
    // 之间相对比例不会变。
    const QSize logical = screen->availableGeometry().size();
    const double logicalScale = qMin(logical.width()  / double(DESIGN_WINDOW_W),
                                     logical.height() / double(DESIGN_WINDOW_H));

    double scale = logicalScale;

    // 调试覆盖：QT_BALATRO_UI_SCALE=1.25 可以临时强制缩放。
    bool ok = false;
    const double overrideScale = QString::fromLocal8Bit(qgetenv("QT_BALATRO_UI_SCALE")).toDouble(&ok);
    if (ok && overrideScale > 0.1) scale = overrideScale;

    return qBound(0.58, scale, 2.35);
}

static int dp(int px)
{
    return qMax(1, int(std::round(px * gUiScale)));
}

static int uiPx(int px)
{
    // 原来为了视觉效果整体放大约 1.55 倍；这里再乘设备缩放系数，
    // 这样 1366×768 会整体缩小，2K/4K 会整体放大。
    return qMax(1, int(std::round(px * 1.55 * gUiScale)));
}

static int overlappedCardStep(int totalW, int cardW, int count, int maxStep)
{
    if (count <= 1) return maxStep;
    const int tightStep = (totalW - cardW) / qMax(1, count - 1);
    // 对齐原版 CardArea：数量超过槽位时压缩步距形成重叠，而不是把整排撑出槽位。
    // 下限留到 18，避免负片/额外槽位叠很多张时仍被最小步距撑出槽位。
    return qBound(18, tightStep, maxStep);
}

static double audioPitchJitter(double spread = 0.04)
{
    const double r = QRandomGenerator::global()->generateDouble() * 2.0 - 1.0;
    return 1.0 + r * spread;
}

static QString musicTrackForPack(PackKind kind)
{
    return kind == PackKind::Celestial ? QStringLiteral("music3")
                                       : QStringLiteral("music2");
}

static QString soundForPackChoice(PackKind kind)
{
    switch (kind) {
    case PackKind::Arcana:    return QStringLiteral("tarot1");
    case PackKind::Celestial: return QStringLiteral("timpani");
    case PackKind::Spectral:  return QStringLiteral("magic_crumple");
    case PackKind::Buffoon:   return QStringLiteral("card1");
    case PackKind::Standard:  return QStringLiteral("cardSlide1");
    }
    return QStringLiteral("card1");
}

static QString soundForConsumable(ConsumableType type)
{
    switch (kindOf(type)) {
    case ConsumableKind::Tarot:    return QStringLiteral("tarot1");
    case ConsumableKind::Planet:   return QString();
    case ConsumableKind::Spectral: return QStringLiteral("magic_crumple");
    }
    return QStringLiteral("generic1");
}

static bool usesOriginalTarotFlip(ConsumableType type)
{
    switch (type) {
    case ConsumableType::Tarot_Magician:
    case ConsumableType::Tarot_Empress:
    case ConsumableType::Tarot_Hierophant:
    case ConsumableType::Tarot_Lovers:
    case ConsumableType::Tarot_Chariot:
    case ConsumableType::Tarot_Justice:
    case ConsumableType::Tarot_Strength:
    case ConsumableType::Tarot_Death:
    case ConsumableType::Tarot_Devil:
    case ConsumableType::Tarot_Tower:
    case ConsumableType::Tarot_Star:
    case ConsumableType::Tarot_Moon:
    case ConsumableType::Tarot_Sun:
    case ConsumableType::Tarot_World:
        return true;
    default:
        return false;
    }
}

static double originalRandomPitch(double base, double range)
{
    return base + QRandomGenerator::global()->generateDouble() * range;
}

static void playSoundLater(QObject *context, int delayMs,
                           const QString &code, double pitch = 1.0, double volume = 1.0)
{
    QPointer<QObject> guard(context);
    QTimer::singleShot(qMax(0, delayMs), context, [guard, code, pitch, volume]() {
        if (!guard) return;
        AudioManager::instance()->play(code, pitch, volume);
    });
}

static void playOriginalDrawCardSound(QObject *context, int index, int count,
                                      bool down, int delayMs = 100, double volume = 1.0)
{
    if (count <= 0) return;
    double percent = double(index + 1) * 100.0 / double(count);
    if (down) percent = 1.0 - percent;
    playSoundLater(context, delayMs, QStringLiteral("card1"),
                   0.85 + percent * 0.2 / 100.0, 0.6 * volume);
}

static void playOriginalDissolveSound(QObject *context, int delayMs = 0)
{
    QPointer<QObject> guard(context);
    QTimer::singleShot(qMax(0, delayMs), context, [guard]() {
        if (!guard) return;
        AudioManager::instance()->play(QStringLiteral("whoosh2"), originalRandomPitch(0.9, 0.2), 0.5);
        AudioManager::instance()->play(QStringLiteral("crumple%1").arg(1 + QRandomGenerator::global()->bounded(5)),
                                       originalRandomPitch(0.9, 0.2), 0.5);
    });
}

static void playOriginalMaterializeSound(QObject *context, int delayMs = 0)
{
    QPointer<QObject> guard(context);
    QTimer::singleShot(qMax(0, delayMs), context, [guard]() {
        if (!guard) return;
        AudioManager::instance()->play(QStringLiteral("whoosh1"), originalRandomPitch(0.6, 0.1), 0.3);
        AudioManager::instance()->play(QStringLiteral("crumple%1").arg(1 + QRandomGenerator::global()->bounded(5)),
                                       originalRandomPitch(1.2, 0.2), 0.8);
    });
}

static void playOriginalBlindWiggleSound(QObject *context, int delayMs = 0)
{
    playSoundLater(context, delayMs, QStringLiteral("tarot2"), 1.0, 0.4);
    playSoundLater(context, delayMs + 60, QStringLiteral("tarot2"), 0.76, 0.4);
}

static void playOriginalTagYepSound(QObject *context, int delayMs = 0)
{
    QPointer<QObject> guard(context);
    QTimer::singleShot(qMax(0, delayMs), context, [guard]() {
        if (!guard) return;
        AudioManager::instance()->play(QStringLiteral("generic1"), originalRandomPitch(0.9, 0.1), 0.8);
        AudioManager::instance()->play(QStringLiteral("holo1"), originalRandomPitch(1.2, 0.1), 0.4);
    });
}

static void playOriginalStatusGenericSound(QObject *context, int delayMs = 0)
{
    QPointer<QObject> guard(context);
    QTimer::singleShot(qMax(0, delayMs), context, [guard]() {
        if (!guard) return;
        const double percent = 0.9 + QRandomGenerator::global()->generateDouble() * 0.2;
        AudioManager::instance()->play(QStringLiteral("generic1"), 0.8 + percent * 0.2, 1.0);
    });
}

static void playOriginalEditionSound(QObject *context, Edition edition, int delayMs = 0)
{
    switch (edition) {
    case Edition::Foil:
        playSoundLater(context, delayMs, QStringLiteral("foil1"), 1.2, 0.4);
        break;
    case Edition::Holographic:
        playSoundLater(context, delayMs, QStringLiteral("holo1"), 1.2 * 1.58, 0.4);
        break;
    case Edition::Polychrome:
        playSoundLater(context, delayMs, QStringLiteral("polychrome1"), 1.2, 0.7);
        break;
    case Edition::Negative:
        playSoundLater(context, delayMs, QStringLiteral("negative"), 1.5, 0.4);
        break;
    case Edition::None:
        break;
    }
}

static Edition changedSelectedHandEdition(const QVector<CardData> &before,
                                          const QVector<CardData> &after,
                                          const QVector<int> &selected)
{
    for (int idx : selected) {
        if (idx < 0 || idx >= before.size() || idx >= after.size()) continue;
        if (before[idx].edition != after[idx].edition)
            return after[idx].edition;
    }
    return Edition::None;
}

static Edition changedJokerEdition(const QVector<Joker> &before, const QVector<Joker> &after)
{
    const int n = qMin(before.size(), after.size());
    for (int i = 0; i < n; ++i) {
        if (before[i].edition != after[i].edition)
            return after[i].edition;
    }
    for (const Joker &j : after) {
        if (j.edition != Edition::None)
            return j.edition;
    }
    return Edition::None;
}

static bool isOriginalSealConsumable(ConsumableType type)
{
    return type == ConsumableType::Spectral_Talisman
        || type == ConsumableType::Spectral_DejaVu
        || type == ConsumableType::Spectral_Trance
        || type == ConsumableType::Spectral_Medium;
}

static bool isOriginalTimpaniConsumable(ConsumableType type)
{
    switch (type) {
    case ConsumableType::Tarot_Fool:
    case ConsumableType::Tarot_Hermit:
    case ConsumableType::Tarot_Temperance:
    case ConsumableType::Tarot_Emperor:
    case ConsumableType::Tarot_HighPriestess:
    case ConsumableType::Tarot_Judgement:
    case ConsumableType::Spectral_Soul:
    case ConsumableType::Spectral_Wraith:
        return true;
    default:
        return false;
    }
}

static bool isOriginalDestroyConsumable(ConsumableType type)
{
    return type == ConsumableType::Tarot_HangedMan
        || type == ConsumableType::Spectral_Familiar
        || type == ConsumableType::Spectral_Grim
        || type == ConsumableType::Spectral_Incantation
        || type == ConsumableType::Spectral_Immolate;
}

static void playOriginalCardFlipSequence(QObject *context, int count, int startDelayMs = 400)
{
    if (count <= 0) return;
    playSoundLater(context, startDelayMs, QStringLiteral("tarot1"), 1.0, 1.0);
    auto pitchFor = [](int index, int count, bool firstFlip) {
        const double denom = count - 0.998;
        const double percent = (index + 0.001) / denom * 0.3;
        return firstFlip ? (1.15 - percent) : (0.85 + percent);
    };
    for (int i = 0; i < count; ++i)
        playSoundLater(context, startDelayMs + 150 * (i + 1),
                       QStringLiteral("card1"), pitchFor(i, count, true), 1.0);
    const int secondStart = startDelayMs + 150 * count + 200;
    for (int i = 0; i < count; ++i)
        playSoundLater(context, secondStart + 150 * (i + 1),
                       QStringLiteral("tarot2"), pitchFor(i, count, false), 0.6);
}

static bool playOriginalConsumableAudio(QObject *context,
                                        ConsumableType type,
                                        const QVector<int> &selected,
                                        const QVector<CardData> &handBefore,
                                        const QVector<CardData> &handAfter,
                                        const QVector<Joker> &jokersBefore,
                                        const QVector<Joker> &jokersAfter,
                                        const QVector<Consumable> &consumablesBefore,
                                        const QVector<Consumable> &consumablesAfter,
                                        int goldBefore,
                                        int goldAfter,
                                        bool allowSelectedTarotFlip)
{
    if (allowSelectedTarotFlip && usesOriginalTarotFlip(type)) {
        playOriginalCardFlipSequence(context, selected.size(), 400);
        return true;
    }

    if (kindOf(type) == ConsumableKind::Planet || type == ConsumableType::Spectral_BlackHole) {
        return true;
    }

    if (isOriginalSealConsumable(type)) {
        playSoundLater(context, 0, QStringLiteral("tarot1"), 1.0, 1.0);
        playSoundLater(context, 100, QStringLiteral("gold_seal"), 1.2, 0.4);
        return true;
    }

    if (type == ConsumableType::Spectral_Aura) {
        playOriginalEditionSound(context, changedSelectedHandEdition(handBefore, handAfter, selected), 400);
        return true;
    }

    if (type == ConsumableType::Spectral_Sigil || type == ConsumableType::Spectral_Ouija) {
        playOriginalCardFlipSequence(context, handBefore.size(), 400);
        return true;
    }

    if (isOriginalDestroyConsumable(type)) {
        playSoundLater(context, 400, QStringLiteral("tarot1"), 1.0, 1.0);
        int dissolveSounds = 0;
        if (type == ConsumableType::Tarot_HangedMan)
            dissolveSounds = qMax(0, selected.size() - 1);
        else if (type == ConsumableType::Spectral_Familiar
                 || type == ConsumableType::Spectral_Grim
                 || type == ConsumableType::Spectral_Incantation)
            dissolveSounds = handBefore.isEmpty() ? 0 : 1;
        else if (type == ConsumableType::Spectral_Immolate)
            dissolveSounds = qMax(0, qMin(5, handBefore.size()) - 1);

        for (int i = 0; i < dissolveSounds; ++i)
            playOriginalDissolveSound(context, 600 + i * 30);

        if (type == ConsumableType::Spectral_Familiar
            || type == ConsumableType::Spectral_Grim
            || type == ConsumableType::Spectral_Incantation)
            playOriginalMaterializeSound(context, 1100);

        if (goldAfter != goldBefore)
            playSoundLater(context, 900, QStringLiteral("coin1"), 1.0, 1.0);
        return true;
    }

    if (isOriginalTimpaniConsumable(type)) {
        const int createdConsumables = qMax(0, consumablesAfter.size() - qMax(0, consumablesBefore.size() - 1));
        const int createdJokers = qMax(0, jokersAfter.size() - jokersBefore.size());
        const int repeats = qMax(1, qMax(createdConsumables, createdJokers));
        for (int i = 0; i < repeats; ++i)
            playSoundLater(context, 400, QStringLiteral("timpani"), 1.0, 1.0);
        if (goldAfter != goldBefore)
            playSoundLater(context, 400, QStringLiteral("coin1"), 1.0, 1.0);
        return true;
    }

    if (type == ConsumableType::Tarot_Wheel
        || type == ConsumableType::Spectral_Ectoplasm
        || type == ConsumableType::Spectral_Hex) {
        const Edition edition = changedJokerEdition(jokersBefore, jokersAfter);
        if (edition != Edition::None) {
            playOriginalEditionSound(context, edition, 400);
            if (type == ConsumableType::Spectral_Hex && jokersBefore.size() > 1)
                playOriginalDissolveSound(context, 420);
        } else if (type == ConsumableType::Tarot_Wheel) {
            playSoundLater(context, 400, QStringLiteral("tarot2"), 1.0, 0.4);
            playSoundLater(context, 460, QStringLiteral("tarot2"), 0.76, 0.4);
        }
        return true;
    }

    if (type == ConsumableType::Spectral_Ankh) {
        if (jokersAfter.size() > jokersBefore.size())
            playOriginalMaterializeSound(context, 400);
        if (jokersBefore.size() > 1)
            playOriginalDissolveSound(context, 750);
        return true;
    }

    if (type == ConsumableType::Spectral_Cryptid) {
        playOriginalMaterializeSound(context, 0);
        return true;
    }

    return false;
}
}

void MainWindow::loadFonts() {
    auto firstFamily = [](int fontId, const QString &fallback) -> QString {
        if (fontId >= 0) {
            const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
            if (!families.isEmpty()) return families.first();
        }
        return fallback;
    };

    auto tryLoadFont = [](const QString &path) -> int {
        const bool isResource = path.startsWith(':');
        if (!isResource && !QFileInfo::exists(path)) {
            qDebug().noquote() << "[Font] not found:" << QDir::toNativeSeparators(path);
            return -1;
        }

        const int id = QFontDatabase::addApplicationFont(path);
        if (id < 0) {
            qDebug().noquote() << "[Font] load failed:" << QDir::toNativeSeparators(path);
            return -1;
        }

        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        qDebug().noquote() << "[Font] loaded:" << QDir::toNativeSeparators(path)
                           << "family =" << families.join(", ");
        return id;
    };

    auto loadFirst = [&](const QStringList &paths) -> int {
        for (const QString &path : paths) {
            const int id = tryLoadFont(path);
            if (id >= 0) return id;
        }
        return -1;
    };

    const int pid = loadFirst({
        ":/fonts/fonts/m6x11plus.ttf",
        QCoreApplication::applicationDirPath() + "/resources/fonts/m6x11plus.ttf",
        QDir::currentPath() + "/resources/fonts/m6x11plus.ttf"
    });

    // 与原版 Balatro game.lua 一致：英文/数字用 m6x11plus.ttf，中文用 NotoSansSC-Bold.ttf。
    // 之前默认优先 汉仪心海行楷W.ttf（行楷风格）会让 UI 偏潇洒，跟原版"无衬线 Bold"观感
    // 差别比较大；这里把 NotoSansSC-Bold.ttf 提到首位，仍保留 汉仪 / m6x11 作为兜底。
    QStringList cnFontCandidates;
    cnFontCandidates << ":/fonts/fonts/NotoSansSC-Bold.ttf";

    auto addFontRoots = [&](const QString &root) {
        if (root.isEmpty()) return;
        const QDir rootDir(root);
        cnFontCandidates << rootDir.filePath("resources/fonts/NotoSansSC-Bold.ttf");
        cnFontCandidates << rootDir.filePath("resouces/fonts/NotoSansSC-Bold.ttf");
    };
    addFontRoots(QCoreApplication::applicationDirPath());
    addFontRoots(QDir::currentPath());
    {
        QDir d(QDir::currentPath());
        for (int i = 0; i < 4; ++i) {
            d.cdUp();
            addFontRoots(d.absolutePath());
        }
    }
    {
        QDir sourceDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
        if (sourceDir.cd("../..")) addFontRoots(sourceDir.absolutePath());
    }

    const int cid = loadFirst(cnFontCandidates);

    const QString pixelFamily = firstFamily(pid, "Arial");
    const QString cnFamily = firstFamily(cid, "Arial");

    qDebug().noquote() << "[Font] appDir =" << QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    qDebug().noquote() << "[Font] currentPath =" << QDir::toNativeSeparators(QDir::currentPath());
    qDebug().noquote() << "[Font] final Chinese family =" << cnFamily;

    mPixelFont = QFont(pixelFamily);
    mPixelFont.setStyleStrategy(QFont::NoAntialias);

    mCNFont = QFont(cnFamily);
    mCNFont.setStyleStrategy(QFont::PreferAntialias);
    if (qApp) qApp->setFont(mCNFont);
}

static QPushButton *makeBtn(const QString &text, const QString &bg, const QString &hover, const QFont &font, QWidget *parent, int h = 50) {
    QPushButton *btn = new QPushButton(text, parent);
    btn->setFixedHeight(h);
    btn->setFont(font);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString(
                           "QPushButton {"
                           " background:%1;"
                           " color:white; border:2px solid rgba(255,255,255,90);"
                           " border-radius:16px; font-size:%3px; font-weight:bold; padding:6px 12px;"
                           "}"
                           "QPushButton:hover {"
                           " background:%2; border:2px solid rgba(255,255,255,170);"
                           "}"
                           "QPushButton:pressed { background:%1; padding-top:8px; }"
                           "QPushButton:disabled { background:#2b3032; color:#758083; border:2px solid #3b4447; }"
                           ).arg(bg, hover).arg(uiPx(16)));
    return btn;
}


static void setLabelScaledText(QLabel *lbl, const QString &text, int nomPx)
{
    QFont f = lbl->font();
    f.setPixelSize(nomPx);
    const int w = lbl->width();
    // 用户反馈10：之前下限 nomPx/2 会在 10+ 位的分数 / 倍率上仍然装不下；
    // 这里把下限放宽到 nomPx/3 但不低于 10px，保证再长的位数也能完整显示。
    const int minPx = qMax(10, nomPx / 3);
    if (w > 20) {
        QFontMetrics fm(f);
        while (f.pixelSize() > minPx && fm.horizontalAdvance(text) > w - 16) {
            f.setPixelSize(f.pixelSize() - 1);
            fm = QFontMetrics(f);
        }
    }
    lbl->setFont(f);
    lbl->setText(text);
}

static QString formatScoreNumber(double num)
{
    if (std::isnan(num)) return QStringLiteral("NaNeInf");
    if (std::isinf(num)) return QStringLiteral("Inf");
    const bool neg = num < 0.0;
    num = std::abs(num);
    if (num >= 100000000000.0) {
        int exp = int(std::floor(std::log10(std::max(num, 1.0))));
        double mantissa = num / std::pow(10.0, exp);
        return QString("%1%2e%3").arg(neg ? "-" : "")
                                  .arg(QString::number(mantissa, 'f', 3))
                                  .arg(exp);
    }
    qint64 n = qRound64(num);
    QString raw = QString::number(n);
    QString out;
    int count = 0;
    for (int i = raw.size() - 1; i >= 0; --i) {
        out.prepend(raw[i]);
        ++count;
        if (count == 3 && i > 0) {
            out.prepend(',');
            count = 0;
        }
    }
    if (neg && out != "0") out.prepend('-');
    return out;
}

static QWidget *makeInfoCard(const QString &title, const QString &body, const QFont &cnFont, QWidget *parent = nullptr,
                             const QString &accent = "#fe5f55")
{
    auto *box = new QWidget(parent);
    box->setAttribute(Qt::WA_StyledBackground, true);
    box->setStyleSheet(QString(
                           "background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(50,67,71,240), stop:1 rgba(24,34,37,240)); border:2px solid %1; border-radius:14px;"
                           ).arg(accent));
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(dp(12), dp(8), dp(12), dp(8));
    v->setSpacing(dp(4));
    auto *t = new QLabel(title, box);
    QFont tf = cnFont; tf.setPixelSize(uiPx(17)); tf.setBold(true);
    t->setFont(tf);
    t->setAlignment(Qt::AlignCenter);
    t->setStyleSheet("color:white; background:transparent; border:none;");
    v->addWidget(t);
    auto *b = new QLabel(body, box);
    QFont bf = cnFont; bf.setPixelSize(uiPx(14)); bf.setBold(true);
    b->setFont(bf);
    b->setAlignment(Qt::AlignCenter);
    b->setWordWrap(true);
    b->setStyleSheet("color:#e7f5f2; background:transparent; border:none;");
    v->addWidget(b, 1);
    return box;
}


// 原版 globals.lua:454-470：HAND_LEVELS 调色板。level 1 → efefef，level 7+ 钳到 caa0ef，level 0 → 红色。
static QString handLevelColor(int level)
{
    static const char *palette[] = {
        "#fe5f55", // level 0 (debuff / disabled) = G.C.RED
        "#efefef", // 1
        "#95acff", // 2
        "#65efaf", // 3
        "#fae37e", // 4
        "#ffc052", // 5
        "#f87d75", // 6
        "#caa0ef", // 7+
    };
    if (level < 0) level = 0;
    if (level > 7) level = 7;
    return QString::fromLatin1(palette[level]);
}

// 各牌型的 1 级基础筹码/倍率。和 RunInfo 面板里的同名 lambda 一致——
// 提到这里是给牌型升级动画用，避免再写一份。
static QPair<int,int> baseChipsMultFor(HandType t) {
    switch (t) {
    case HandType::HighCard:      return {Constants::BASE_HIGH_CARD_CHIPS,      Constants::BASE_HIGH_CARD_MULT};
    case HandType::Pair:          return {Constants::BASE_PAIR_CHIPS,           Constants::BASE_PAIR_MULT};
    case HandType::TwoPair:       return {Constants::BASE_TWO_PAIR_CHIPS,       Constants::BASE_TWO_PAIR_MULT};
    case HandType::ThreeOfAKind:  return {Constants::BASE_THREE_CHIPS,          Constants::BASE_THREE_MULT};
    case HandType::Straight:      return {Constants::BASE_STRAIGHT_CHIPS,       Constants::BASE_STRAIGHT_MULT};
    case HandType::Flush:         return {Constants::BASE_FLUSH_CHIPS,          Constants::BASE_FLUSH_MULT};
    case HandType::FullHouse:     return {Constants::BASE_FULL_HOUSE_CHIPS,     Constants::BASE_FULL_HOUSE_MULT};
    case HandType::FourOfAKind:   return {Constants::BASE_FOUR_CHIPS,           Constants::BASE_FOUR_MULT};
    case HandType::StraightFlush: return {Constants::BASE_STRAIGHT_FLUSH_CHIPS, Constants::BASE_STRAIGHT_FLUSH_MULT};
    case HandType::RoyalFlush:    return {Constants::BASE_ROYAL_FLUSH_CHIPS,    Constants::BASE_ROYAL_FLUSH_MULT};
    case HandType::FiveOfAKind:   return {Constants::BASE_FIVE_CHIPS,           Constants::BASE_FIVE_MULT};
    case HandType::FlushHouse:    return {Constants::BASE_FLUSH_HOUSE_CHIPS,    Constants::BASE_FLUSH_HOUSE_MULT};
    case HandType::FlushFive:     return {Constants::BASE_FLUSH_FIVE_CHIPS,     Constants::BASE_FLUSH_FIVE_MULT};
    }
    return {0, 0};
}

static QString enhancementName(Enhancement e) {
    switch (e) {
    case Enhancement::Bonus: return "奖励牌";
    case Enhancement::Mult: return "倍率牌";
    case Enhancement::Wild: return "万能牌";
    case Enhancement::Glass: return "玻璃牌";
    case Enhancement::Steel: return "钢铁牌";
    case Enhancement::Stone: return "石头牌";
    case Enhancement::Gold: return "黄金牌";
    case Enhancement::Lucky: return "幸运牌";
    default: return "普通牌";
    }
}

static QString enhancementDesc(Enhancement e) {
    switch (e) {
    case Enhancement::Bonus: return "+30 筹码";
    case Enhancement::Mult: return "+4 倍率";
    case Enhancement::Wild: return "可视作任意花色";
    case Enhancement::Glass: return "计分时 ×2 倍率，之后有概率破碎";
    case Enhancement::Steel: return "留在手牌中时 ×1.5 倍率";
    case Enhancement::Stone: return "+50 筹码，没有点数与花色";
    case Enhancement::Gold: return "回合结束若仍在手牌中，获得 $3";
    case Enhancement::Lucky: return "概率获得 +20 倍率或 $20";
    default: return "基础牌面筹码";
    }
}

static QString editionName(Edition e) {
    switch (e) {
    case Edition::Foil: return "闪箔";
    case Edition::Holographic: return "镭射";
    case Edition::Polychrome: return "多彩";
    case Edition::Negative: return "负片";
    default: return "";
    }
}

static QString editionDesc(Edition e) {
    switch (e) {
    case Edition::Foil: return "+50 筹码";
    case Edition::Holographic: return "+10 倍率";
    case Edition::Polychrome: return "×1.5 倍率";
    case Edition::Negative: return "+1 持有槽位";
    default: return "";
    }
}

static QString sealName(Seal s) {
    switch (s) {
    case Seal::Gold: return "金色蜡封";
    case Seal::Red: return "红色蜡封";
    case Seal::Blue: return "蓝色蜡封";
    case Seal::Purple: return "紫色蜡封";
    default: return "";
    }
}

static QString sealDesc(Seal s) {
    switch (s) {
    case Seal::Gold: return "打出并计分后获得 $3";
    case Seal::Red: return "重新触发这张牌 1 次";
    case Seal::Blue: return "回合结束时生成对应星球牌";
    case Seal::Purple: return "弃掉时生成一张塔罗牌";
    default: return "";
    }
}

static QString suitText(Suit s) {
    switch (s) {
    case Suit::Spades: return "黑桃";
    case Suit::Hearts: return "红桃";
    case Suit::Diamonds: return "方块";
    case Suit::Clubs: return "梅花";
    }
    return "";
}

static QString rankText(Rank r) {
    switch (r) {
    case Rank::Jack: return "J";
    case Rank::Queen: return "Q";
    case Rank::King: return "K";
    case Rank::Ace: return "A";
    default: return QString::number(static_cast<int>(r));
    }
}

static QString cardTooltipTitle(const CardData &c) {
    if (c.enhancement == Enhancement::Stone) return "石头牌";
    QString title = suitText(c.suit) + rankText(c.rank);
    if (c.enhancement != Enhancement::None) title += " · " + enhancementName(c.enhancement);
    return title;
}

// 所有 hover 浮窗共享同一份 helper——见 cardtooltipformat.h。
namespace BalatroTooltip = CardTooltipFormat;

static QString cardTooltipBody(const CardData &c) {
    QStringList lines;
    if (c.enhancement == Enhancement::Stone) {
        lines << "+50筹码";
    } else {
        int chips = c.chipValue() + c.permanentBonusChips;
        if (c.enhancement == Enhancement::Bonus) chips += 30;
        lines << QString("+%1筹码").arg(chips);

        switch (c.enhancement) {
        case Enhancement::Mult: lines << "+4倍率"; break;
        case Enhancement::Wild: lines << "可视作任意花色"; break;
        case Enhancement::Glass: lines << "计分时 ×2 倍率"; break;
        case Enhancement::Steel: lines << "留在手牌中时 ×1.5 倍率"; break;
        case Enhancement::Gold: lines << "回合结束若仍在手牌中，获得 $3"; break;
        case Enhancement::Lucky: lines << "概率获得 +20 倍率或 $20"; break;
        default: break;
        }
    }
    if (c.edition != Edition::None)
        lines << editionName(c.edition) + "：" + editionDesc(c.edition);
    if (c.seal != Seal::None)
        lines << sealName(c.seal) + "：" + sealDesc(c.seal);
    if (c.isDebuffed)
        lines << "被 Boss 盲注禁用";
    return lines.join("\n");
}

static QLabel *makeLabel(const QString &text, int px, const QString &color, const QFont &font, QWidget *parent) {
    QLabel *lbl = new QLabel(text, parent);
    lbl->setAlignment(Qt::AlignCenter);
    QFont f = font; f.setPixelSize(uiPx(px));
    lbl->setFont(f);
    lbl->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(color));
    return lbl;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mGameState(new GameState(this))
{
    ui->setupUi(this);

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QSize sg = screen ? screen->availableGeometry().size()
                            : QSize(DESIGN_WINDOW_W, DESIGN_WINDOW_H);
    const qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
    gUiScale = calcUiScale(screen);

    // 左侧 QWidget 和右侧 QGraphicsScene 使用同一套 1920×1080 设计稿比例。
    // 左侧真实尺寸随设备缩放；右侧场景保持设计坐标，再通过 fitInView 等比例映射到实际视口。
    mLeftW = dp(DESIGN_LEFT_W);
    mWinW = sg.width();
    mWinH = sg.height();
    mSceneW = DESIGN_SCENE_W;
    mSceneH = DESIGN_SCENE_H;

    qDebug().noquote() << "[UI Scale] logicalScreen =" << sg.width() << "x" << sg.height()
                       << "dpr =" << QString::number(dpr, 'f', 2)
                       << "physicalApprox =" << int(std::round(sg.width() * dpr)) << "x" << int(std::round(sg.height() * dpr))
                       << "scale =" << QString::number(gUiScale, 'f', 3)
                       << "leftW =" << mLeftW
                       << "scene =" << QString("%1x%2").arg(mSceneW).arg(mSceneH);

    loadFonts();

    menuBar()->hide();
    statusBar()->hide();

    // ── 左面板（永远显示）──
    setupLeftPanel();

    // ── 右半边容器:绿色牌桌永远显示 ──
    mPlayPage = new QWidget;
    mPlayPage->setAttribute(Qt::WA_StyledBackground, true);
    mPlayPage->setStyleSheet("background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #30384d, stop:1 #202839);");
    setupScene();
    setupSceneButtons();
    {
        auto *l = new QVBoxLayout(mPlayPage);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(mView);
        if (mDynamicBg) {
            mDynamicBg->setGeometry(mPlayPage->rect());
            mDynamicBg->lower();
        }
        if (mView) mView->raise();
        mSplashOverlay = new SplashShaderOverlay(mPlayPage);
        mSplashOverlay->setGeometry(mPlayPage->rect());
        mSplashOverlay->hide();
    }

    // ── 整体 central:左面板 + 右半边,横向并列 ──
    auto *container = new QWidget;
    container->setObjectName("RootContainer");
    container->setAttribute(Qt::WA_StyledBackground, true);
    // 只给根容器本身上背景色，避免样式表向左侧面板里的普通 QWidget 级联，形成一块块黑色底框。
    container->setStyleSheet("QWidget#RootContainer { background:#11181b; }");
    auto *cl = new QHBoxLayout(container);
    cl->setContentsMargins(dp(8), dp(8), 0, dp(8));
    cl->setSpacing(0);
    cl->addWidget(mLeftPanel);
    cl->addWidget(mPlayPage, 1);
    setCentralWidget(container);

    // ── 所有 overlay 都挂在 mPlayPage 上,默认隐藏 ──
    mBlindSelectWidget = new BlindSelectWidget(mGameState, mCNFont, mPixelFont, mPlayPage);
    mBlindSelectWidget->hide();

    mShopWidget = new ShopWidget(mGameState, mCNFont, mPixelFont, mPlayPage);
    mShopWidget->hide();

    mRoundEndOverlay = new RoundEndOverlay(mCNFont, mPixelFont, mPlayPage);
    mRoundEndOverlay->hide();
    connect(mRoundEndOverlay, &RoundEndOverlay::nextClicked,
            this, &MainWindow::onNextBlindClicked);

    mPackOpenWidget = new PackOpenWidget(mCNFont, mPixelFont, mPlayPage);
    mPackOpenWidget->hide();
    connect(mPackOpenWidget, &PackOpenWidget::choiceAnimationRequested,
            this, &MainWindow::prepareSlotFlyInAnimation);
    connect(mPackOpenWidget, &PackOpenWidget::choiceMade,
            this, &MainWindow::onPackChoiceMade);
    connect(mPackOpenWidget, &PackOpenWidget::inventoryConsumableRequested,
            this, &MainWindow::onInventoryConsumableUseRequested);
    connect(mPackOpenWidget, &PackOpenWidget::packFinished,
            this, &MainWindow::onPackFinished);

    mDeckViewWidget = new DeckViewWidget(mCNFont, mPixelFont, mPlayPage);
    mDeckViewWidget->hide();
    // 关闭"查看牌组"对话框后立即重判悬停：若鼠标已不在牌堆按钮上，把面板收起、按钮升回原位，
    // 不必等用户挪动鼠标才触发 hoverLeaveEvent。
    connect(mDeckViewWidget, &DeckViewWidget::closed, this, [this]() {
        mDeckViewOpen = false;
        resumeGameProcesses();
        if (!mDeckBackCard || !mView) return;
        const QPoint gp = QCursor::pos();
        const QPoint viewPt = mView->mapFromGlobal(gp);
        const QPointF scenePt = mView->mapToScene(viewPt);
        const bool stillOnDeck =
            mDeckBackCard->isVisible() &&
            mDeckBackCard->sceneBoundingRect().contains(scenePt);
        if (!stillOnDeck) {
            if (mDeckStatsItem) mDeckStatsItem->setVisible(false);
            if (mDeckPeekDeployed) hideDeckPeekPanel();
        }
    });

    setupConnections();
    AudioManager::instance()->initialize();
    AudioManager::instance()->setPitchMod(1.0);
    AudioManager::instance()->setDesiredMusic(QStringLiteral("music1"));
    mPlayPage->installEventFilter(this);

    QTimer::singleShot(0, this, [this]() {
        if (mBlindSelectWidget) mBlindSelectWidget->hide();
        // 启动直接进入主菜单——"继续当前局" 此时灰掉,
        // 必须先点 "开始新的一局" 才会真正初始化游戏状态进入对局界面。
        showMainMenuOverlay();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupLeftPanel() {
    mLeftPanel = new QWidget;
    mLeftPanel->setObjectName("LeftPanel");
    mLeftPanel->setFixedWidth(mLeftW);
    mLeftPanel->setAttribute(Qt::WA_StyledBackground, true);
    mLeftPanel->setStyleSheet(
        "QWidget#LeftPanel { background:#263135; border-right:none; border-radius:0px; }"
        "QWidget#LeftPanel QLabel { border:none; }"
        "QWidget#LeftPanel QPushButton { border:none; }"
        );

    QVBoxLayout *layout = new QVBoxLayout(mLeftPanel);
    layout->setContentsMargins(dp(18), dp(16), dp(18), dp(16));
    layout->setSpacing(dp(10));
    // 顶部 stretch：上方留呼吸空间，主信息区视觉重心落在面板中段。
    layout->addStretch(1);

    // ── 上下文区 ──
    mContextArea = new QStackedWidget(mLeftPanel);
    mContextArea->setFixedHeight(dp(260));
    mContextArea->setStyleSheet("background:transparent;");

    // 页面 0: BlindSelect
    mCtxBlindSelect = new QWidget;
    mCtxBlindSelect->setStyleSheet("background:transparent;");
    {
        auto *vl = new QVBoxLayout(mCtxBlindSelect);
        vl->setContentsMargins(0, dp(16), 0, dp(16));
        vl->setSpacing(dp(2));
        vl->setAlignment(Qt::AlignCenter);

        QFont t1f = mCNFont; t1f.setPixelSize(uiPx(30)); t1f.setBold(true);

        QLabel *l1 = new QLabel("选择你的", mCtxBlindSelect);
        l1->setFont(t1f);
        l1->setStyleSheet("color:white; background:transparent;");
        l1->setAlignment(Qt::AlignCenter);
        vl->addWidget(l1);

        QLabel *l2 = new QLabel("下一个盲注", mCtxBlindSelect);
        l2->setFont(t1f);
        l2->setStyleSheet("color:white; background:transparent;");
        l2->setAlignment(Qt::AlignCenter);
        vl->addWidget(l2);
    }
    mContextArea->addWidget(mCtxBlindSelect);

    // 页面 1: Blind
    // 布局参考原版 G.UIT 的 HUD_blind：顶部色带显示盲注名（DYN_UI.MAIN），
    // 下方深色区域承载筹码图 + “至少得分/奖励”信息（DYN_UI.DARK）。
    mCtxBlind = new QWidget;
    mCtxBlind->setAttribute(Qt::WA_StyledBackground, true);
    mCtxBlind->setStyleSheet("background:#334044; border:none; border-radius:16px;");
    {
        auto *vMain = new QVBoxLayout(mCtxBlind);
        vMain->setContentsMargins(0, 0, 0, 0);
        vMain->setSpacing(0);

        // 顶部色带 —— 盲注名居中，圆角与外层一致。
        mLblBlind = new QLabel("小盲注", mCtxBlind);
        QFont nbf = mCNFont; nbf.setPixelSize(uiPx(20)); nbf.setBold(true);
        mLblBlind->setFont(nbf);
        mLblBlind->setAlignment(Qt::AlignCenter);
        mLblBlind->setStyleSheet(
            "color:white; background:#1679b4;"
            "border-top-left-radius:10px; border-top-right-radius:10px;"
            "padding:6px 10px;");
        mLblBlind->setFixedHeight(dp(48));
        vMain->addWidget(mLblBlind);

        // 下方主体 —— 左 chip，右 至少得分 / 奖励。
        QWidget *body = new QWidget(mCtxBlind);
        body->setStyleSheet("background:transparent;");
        auto *hbl = new QHBoxLayout(body);
        hbl->setContentsMargins(dp(10), dp(10), dp(10), dp(10));
        hbl->setSpacing(dp(10));

        mCtxBlindChipImg = new AnimatedBlindChip(body);
        mCtxBlindChipImg->setDisplaySize(dp(92));
        hbl->addWidget(mCtxBlindChipImg, 0, Qt::AlignVCenter);

        auto *vbl = new QVBoxLayout;
        vbl->setContentsMargins(0, 0, 0, 0);
        vbl->setSpacing(dp(2));
        vbl->addStretch(1);

        QLabel *tt = new QLabel("至少得分", body);
        QFont ttf = mCNFont; ttf.setPixelSize(uiPx(13));
        tt->setFont(ttf);
        tt->setStyleSheet("color:white; background:transparent;");
        tt->setAlignment(Qt::AlignCenter);
        vbl->addWidget(tt);

        mLblTarget = new QLabel("✳ 300", body);
        QFont tf = mPixelFont; tf.setPixelSize(uiPx(28));
        mLblTarget->setFont(tf);
        mLblTarget->setStyleSheet("color:#fe5f55; background:transparent;");
        mLblTarget->setAlignment(Qt::AlignCenter);
        vbl->addWidget(mLblTarget);

        mLblReward = new QLabel("奖励 $$$", body);
        QFont rf = mCNFont; rf.setPixelSize(uiPx(14));
        mLblReward->setFont(rf);
        mLblReward->setStyleSheet("color:#f3b958; background:transparent;");
        mLblReward->setAlignment(Qt::AlignCenter);
        vbl->addWidget(mLblReward);

        vbl->addStretch(1);
        hbl->addLayout(vbl, 1);

        vMain->addWidget(body, 1);
    }
    mContextArea->addWidget(mCtxBlind);

    // 页面 2: Shop
    mCtxShop = new QWidget;
    mCtxShop->setStyleSheet("background:transparent;");
    {
        auto *vl = new QVBoxLayout(mCtxShop);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        vl->setAlignment(Qt::AlignCenter);

        auto *sign = new ShopSignWidget(mCtxShop);
        vl->addWidget(sign, 0, Qt::AlignCenter);

        QLabel *sub = new QLabel("来变强吧!", mCtxShop);
        QFont subf = mCNFont; subf.setPixelSize(uiPx(15));
        sub->setFont(subf);
        sub->setStyleSheet("color:white; background:transparent;");
        sub->setAlignment(Qt::AlignCenter);
        vl->addWidget(sub);
    }
    mContextArea->addWidget(mCtxShop);

    // 页面 3: 空白页 —— 击败盲注溶解 chips 后到点击"提现"前的空档显示空白。
    {
        QWidget *blank = new QWidget;
        blank->setStyleSheet("background:transparent;");
        mContextArea->addWidget(blank);
    }

    layout->addWidget(mContextArea);

    // ── 回合分数 + 目标进度条 ──
    QWidget *scoreBox = new QWidget(mLeftPanel);
    scoreBox->setFixedHeight(dp(142));
    scoreBox->setAttribute(Qt::WA_StyledBackground, true);
    scoreBox->setStyleSheet("background:#334044; border:none; border-radius:16px;");

    auto *scoreVBox = new QVBoxLayout(scoreBox);
    scoreVBox->setContentsMargins(dp(12), dp(8), dp(12), dp(10));
    scoreVBox->setSpacing(dp(6));

    QWidget *scoreTop = new QWidget(scoreBox);
    auto *sbl = new QHBoxLayout(scoreTop);
    sbl->setContentsMargins(0, 0, 0, 0);
    sbl->setSpacing(dp(6));

    QLabel *sTitle = new QLabel("回合\n分数", scoreTop);
    QFont stf = mCNFont; stf.setPixelSize(uiPx(15));
    sTitle->setFont(stf);
    sTitle->setStyleSheet("color:white; background:transparent;");
    sTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    sbl->addWidget(sTitle);

    sbl->addStretch();

    QLabel *scoreChip = new QLabel(scoreTop);
    {
        QPixmap chipsSheet(":/textures/images/chips.png");
        if (!chipsSheet.isNull()) {
            QPixmap pix = chipsSheet.copy(0, 0, 58, 58);
            scoreChip->setPixmap(pix.scaled(dp(34), dp(34), Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
        }
    }
    scoreChip->setFixedSize(dp(36), dp(36));
    scoreChip->setStyleSheet("background:transparent;");
    sbl->addWidget(scoreChip);

    mLblScore = new QLabel("0", scoreTop);
    QFont smf = mPixelFont; smf.setPixelSize(uiPx(38));
    mLblScore->setFont(smf);
    mLblScore->setStyleSheet("color:white; background:transparent;");
    // 回合分数数字水平居中——比标题右侧的"靠右"更对称，看上去像原版的"回合分数 [筹码图] 0"。
    mLblScore->setAlignment(Qt::AlignCenter);
    // 固定宽度：setLabelScaledText 依据标签宽度缩放字号；若标签随内容自适应，
    // 缩放→变窄→再缩放会形成反馈回路把字号一路缩到下限。固定宽度切断该回路。
    mLblScore->setFixedWidth(dp(150));
    sbl->addWidget(mLblScore);
    sbl->addStretch();
    scoreVBox->addWidget(scoreTop);

    mScoreProgressBar = new QProgressBar(scoreBox);
    mScoreProgressBar->setRange(0, 1000);
    mScoreProgressBar->setValue(0);
    mScoreProgressBar->setFixedHeight(dp(34));
    mScoreProgressBar->setTextVisible(true);
    mScoreProgressBar->setFormat("0%");
    mScoreProgressBar->setAlignment(Qt::AlignCenter);
    QFont pbf = mPixelFont; pbf.setPixelSize(uiPx(17));
    mScoreProgressBar->setFont(pbf);
    mScoreProgressBar->setStyleSheet(
        "QProgressBar {"
        " background:rgba(8,18,24,112);"
        " border:none;"
        " border-radius:14px;"
        " color:#eaffff;"
        " text-align:center;"
        " padding:2px;"
        "}"
        "QProgressBar::chunk {"
        " border-radius:11px;"
        " background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #009dff, stop:0.58 #23e6ff, stop:1 #fda200);"
        "}"
        );
    mScoreProgressGlow = new QGraphicsDropShadowEffect(mScoreProgressBar);
    mScoreProgressGlow->setBlurRadius(16);
    mScoreProgressGlow->setOffset(0, 0);
    mScoreProgressGlow->setColor(QColor(35, 230, 255, 120));
    mScoreProgressBar->setGraphicsEffect(mScoreProgressGlow);
    scoreVBox->addWidget(mScoreProgressBar);

    // 进度条发光脉动：原本走 QVariantAnimation 默认 60 FPS valueChanged，
    // 每一帧都改 QGraphicsDropShadowEffect 的 blur/color，触发整条进度条 +
    // 软阴影的重绘——单这一条就持续占用 ~3% CPU 且让其他动画卡顿。
    // 改成 QTimer 50 ms 步进，视觉差异极小（脉动从 60Hz → 20Hz）。
    auto *scorePulse = new QTimer(this);
    scorePulse->setInterval(50);
    scorePulse->setTimerType(Qt::CoarseTimer);
    const double pulsePeriodMs = 1800.0;
    connect(scorePulse, &QTimer::timeout, this, [this, pulsePeriodMs]() {
        if (!mScoreProgressGlow || !mScoreProgressBar) return;
        const double t = std::fmod(double(QDateTime::currentMSecsSinceEpoch()) / pulsePeriodMs, 1.0);
        const double wave = (std::sin(t * 6.28318530718) + 1.0) * 0.5;
        const bool passed = mScoreProgressBar->value() >= mScoreProgressBar->maximum();
        QColor glow = passed ? QColor(255, 176, 0) : QColor(35, 230, 255);
        glow.setAlpha(80 + int(70 * wave));
        mScoreProgressGlow->setColor(glow);
        mScoreProgressGlow->setBlurRadius((passed ? 20 : 14) + int(7 * wave));
    });
    scorePulse->start();

    layout->addWidget(scoreBox);

    // 牌型名行
    QWidget *handNameBox = new QWidget(mLeftPanel);
    handNameBox->setAttribute(Qt::WA_StyledBackground, true);
    handNameBox->setStyleSheet("background:transparent; border:none;");
    handNameBox->setFixedHeight(dp(74));
    auto *hnl = new QHBoxLayout(handNameBox);
    hnl->setContentsMargins(0, 0, 0, 0);
    hnl->setSpacing(dp(6));
    hnl->setAlignment(Qt::AlignCenter);

    mLblHandName = new QLabel("", handNameBox);
    QFont hnf = mCNFont; hnf.setPixelSize(uiPx(24)); hnf.setBold(true);
    mLblHandName->setFont(hnf);
    mLblHandName->setStyleSheet("color:white; background:transparent;");
    mLblHandName->setAlignment(Qt::AlignCenter);
    hnl->addWidget(mLblHandName);

    mLblHandLevel = new QLabel("", handNameBox);
    QFont hlf = mCNFont; hlf.setPixelSize(uiPx(16));
    mLblHandLevel->setFont(hlf);
    mLblHandLevel->setStyleSheet("color:#ff9a00; background:transparent;");
    hnl->addWidget(mLblHandLevel);

    layout->addWidget(handNameBox);

    // 筹码 × 倍率
    QWidget *chipsRow = new QWidget(mLeftPanel);
    chipsRow->setAttribute(Qt::WA_StyledBackground, true);
    chipsRow->setStyleSheet("background:transparent; border:none;");
    chipsRow->setFixedHeight(dp(96));
    QHBoxLayout *chipsLayout = new QHBoxLayout(chipsRow);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    chipsLayout->setSpacing(dp(4));

    mLblChips = new QLabel("0", chipsRow);
    // 数字靠右贴近中间的 ×，让筹码值视觉上"挤"向倍率方向；原版同款。
    mLblChips->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont cf = mPixelFont; cf.setPixelSize(uiPx(42));
    mLblChips->setFont(cf);
    mLblChips->setStyleSheet(
        "background: #009dff; color: white;"
        "border-radius: 8px; padding: 4px 12px;"
        );

    QLabel *lblX = new QLabel("×", chipsRow);
    lblX->setAlignment(Qt::AlignCenter);
    QFont xf = mCNFont; xf.setPixelSize(uiPx(28));
    lblX->setFont(xf);
    lblX->setStyleSheet("color: white;");
    lblX->setFixedWidth(dp(32));

    mLblMult = new QLabel("0", chipsRow);
    // 倍率数字靠左贴近中间的 ×。
    mLblMult->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mLblMult->setFont(cf);
    mLblMult->setStyleSheet(
        "background:#fe5f55; color:white;"
        "border-radius:8px; padding:4px 12px;"
        );

    chipsLayout->addWidget(mLblChips, 1);
    chipsLayout->addWidget(lblX);
    chipsLayout->addWidget(mLblMult, 1);
    layout->addWidget(chipsRow);

    mChipsRowWidget = chipsRow;

    // 两个独立火焰 overlay
    // 改用 GPU 着色器（FlameShaderWidget，直接跑原版 flame.fs）：CPU 占用从
    // per-frame per-pixel fractal noise（CPU 上 ~30000 浮点运算/帧）降到 0。
    auto makeFlame = [this](float idSeed, const QColor &c1, const QColor &c2) {
        auto *w = new FlameShaderWidget(mLeftPanel);
        w->setProperty("flameId", double(idSeed));
        w->setColours(c1, c2);
        // FlameShaderWidget 默认 mId = 13.37；不同火焰用不同 id 让 phase 错开。
        w->setProperty("idSeed", double(idSeed));
        return w;
    };

    mChipFlame = makeFlame(1.0f,
                           QColor(0, 157, 255),
                           QColor(180, 200, 80));
    mMultFlame = makeFlame(2.0f,
                           QColor(254, 95, 85),
                           QColor(255, 180, 80));

    // 30Hz tick: 弹簧 ease real → target
    mFlameTick = new QTimer(this);
    mFlameTick->setTimerType(Qt::CoarseTimer);
    connect(mFlameTick, &QTimer::timeout, this, [this]() {
        const double dt = 1.0 / 16.0;
        auto ease = [dt](double &real, double target) {
            double diff = target - real;
            real += diff * dt * 6.0;
            if (std::abs(diff) < 0.005) real = target;
        };
        ease(mChipFlameReal, mChipFlameTarget);
        ease(mMultFlameReal, mMultFlameTarget);

        const double earned = mDisplayedChips * mDisplayedMult;
        const double required = mGameState ? mGameState->targetScore() : 0.0;
        double audioTarget = 0.0;
        if (required > 0.0 && std::isfinite(earned) && earned >= required) {
            audioTarget = std::max(0.0, std::log(std::max(earned, 1.0)) / std::log(5.0) - 2.0);
        } else if (required > 0.0 && std::isinf(earned)) {
            audioTarget = 10.0;
        }

        auto updateAudioFlame = [dt, audioTarget](double &real,
                                                  double &velocity,
                                                  double &change) {
            const double exptime = std::exp(-0.4 * dt);
            if (velocity < 0.0) velocity *= (1.0 - 10.0 * dt);
            velocity = (1.0 - exptime) * (audioTarget - real) * dt * 25.0
                       + exptime * velocity;
            real = std::max(0.0, real + velocity);
            change = change * (1.0 - 4.0 * dt)
                     + (4.0 * dt) * (real < audioTarget ? 1.0 : 0.0) * real;
        };
        updateAudioFlame(mAudioChipFlameReal, mAudioChipFlameVelocity, mAudioChipFlameChange);
        updateAudioFlame(mAudioMultFlameReal, mAudioMultFlameVelocity, mAudioMultFlameChange);
        AudioManager::instance()->setScoreAmbient(earned,
                                                  required,
                                                  mAudioChipFlameReal,
                                                  mAudioChipFlameChange,
                                                  mAudioMultFlameChange);

        // FlameShaderWidget 内部已自带平滑 + 自动 hide：直接转交 target 数值。
        auto applyVis = [](FlameShaderWidget *w, double real) {
            if (!w) return;
            if (real > 0.05) w->setAmount(float(real));
            else if (w->isVisible()) w->stop();
        };
        applyVis(mChipFlame, mChipFlameReal);
        applyVis(mMultFlame, mMultFlameReal);
    });
    // 火焰可见时每 tick 都跑 paintFlame（CPU per-pixel 分形 noise）。
    // 33ms (30FPS) 仍然是用户卡顿的主因之一；调到 60ms (~16FPS) 视觉差异有限但 CPU 减半。
    mFlameTick->start(60);

    QWidget *bottomRow = new QWidget(mLeftPanel);
    bottomRow->setAttribute(Qt::WA_StyledBackground, true);
    bottomRow->setStyleSheet("background:transparent; border:none;");
    auto *brl = new QHBoxLayout(bottomRow);
    brl->setContentsMargins(0, 0, 0, 0);
    brl->setSpacing(dp(8));

    QWidget *btnCol = new QWidget(bottomRow);
    btnCol->setAttribute(Qt::WA_StyledBackground, true);
    btnCol->setStyleSheet("background:transparent; border:none;");
    auto *btnVbl = new QVBoxLayout(btnCol);
    btnVbl->setContentsMargins(0, 0, 0, 0);
    btnVbl->setSpacing(dp(6));

    // 原版 run_info_button: minw=1.5, minh=1.75 (G.TILESIZE 单位) —— 高 > 宽，比例 ≈ 1.17。
    // 进一步放大到 116 × 136 (≈ 1.17:1)，给字号更多余量并贴近原版视觉占比。
    QPushButton *btnInfo = makeBtn("比赛\n信息", "#fe5f55", "#ff7066", mCNFont, btnCol, dp(136));
    btnInfo->setFixedWidth(dp(116));
    btnInfo->setStyleSheet(QString(
        "QPushButton { background:#fe5f55; color:white; border:2px solid rgba(255,255,255,80);"
        " border-radius:14px; font-size:%1px; font-weight:bold; padding:6px 10px; }"
        "QPushButton:hover { background:#ff7066; border:2px solid rgba(255,255,255,150); }"
        "QPushButton:pressed { background:#d94a42; padding-top:6px; }"
        ).arg(uiPx(18)));
    btnVbl->addWidget(btnInfo);
    connect(btnInfo, &QPushButton::clicked, this, [this]() {
        auto handName = [](HandType t) {
            switch (t) {
            case HandType::HighCard: return QString("高牌");
            case HandType::Pair: return QString("对子");
            case HandType::TwoPair: return QString("两对");
            case HandType::ThreeOfAKind: return QString("三条");
            case HandType::Straight: return QString("顺子");
            case HandType::Flush: return QString("同花");
            case HandType::FullHouse: return QString("葫芦");
            case HandType::FourOfAKind: return QString("四条");
            case HandType::StraightFlush: return QString("同花顺");
            case HandType::RoyalFlush: return QString("皇家同花顺");
            case HandType::FiveOfAKind: return QString("五条");
            case HandType::FlushHouse: return QString("同花葫芦");
            case HandType::FlushFive: return QString("同花五条");
            }
            return QString("未知");
        };
        auto baseScore = [](HandType t) -> QPair<int,int> {
            switch (t) {
            case HandType::HighCard: return {Constants::BASE_HIGH_CARD_CHIPS, Constants::BASE_HIGH_CARD_MULT};
            case HandType::Pair: return {Constants::BASE_PAIR_CHIPS, Constants::BASE_PAIR_MULT};
            case HandType::TwoPair: return {Constants::BASE_TWO_PAIR_CHIPS, Constants::BASE_TWO_PAIR_MULT};
            case HandType::ThreeOfAKind: return {Constants::BASE_THREE_CHIPS, Constants::BASE_THREE_MULT};
            case HandType::Straight: return {Constants::BASE_STRAIGHT_CHIPS, Constants::BASE_STRAIGHT_MULT};
            case HandType::Flush: return {Constants::BASE_FLUSH_CHIPS, Constants::BASE_FLUSH_MULT};
            case HandType::FullHouse: return {Constants::BASE_FULL_HOUSE_CHIPS, Constants::BASE_FULL_HOUSE_MULT};
            case HandType::FourOfAKind: return {Constants::BASE_FOUR_CHIPS, Constants::BASE_FOUR_MULT};
            case HandType::StraightFlush: return {Constants::BASE_STRAIGHT_FLUSH_CHIPS, Constants::BASE_STRAIGHT_FLUSH_MULT};
            case HandType::RoyalFlush: return {Constants::BASE_ROYAL_FLUSH_CHIPS, Constants::BASE_ROYAL_FLUSH_MULT};
            case HandType::FiveOfAKind: return {Constants::BASE_FIVE_CHIPS, Constants::BASE_FIVE_MULT};
            case HandType::FlushHouse: return {Constants::BASE_FLUSH_HOUSE_CHIPS, Constants::BASE_FLUSH_HOUSE_MULT};
            case HandType::FlushFive: return {Constants::BASE_FLUSH_FIVE_CHIPS, Constants::BASE_FLUSH_FIVE_MULT};
            }
            return {0,0};
        };

        // 原版的"运行信息"在按下后从屏幕下方滑入，无原生窗口边框 / 关闭按钮；
        // 这里用一个全屏覆盖层（背景半透明）+ 中央面板替代旧的 QDialog::exec()。
        if (mRunInfoOverlay) { mRunInfoOverlay->deleteLater(); mRunInfoOverlay = nullptr; }
        QWidget *host = centralWidget() ? centralWidget() : this;
        mRunInfoOverlay = new QWidget(host);
        QWidget *overlay = mRunInfoOverlay;
        overlay->setObjectName("RunInfoOverlay");
        overlay->setAttribute(Qt::WA_StyledBackground, true);
        overlay->setStyleSheet(
            "QWidget#RunInfoOverlay { background:rgba(0,0,0,108); }"
            "QFrame#RunInfoPanel { background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(43,60,63,248), stop:1 rgba(18,31,34,248));"
            " border:4px solid #dbe9e7; border-radius:20px; }"
            "QLabel { color:white; background:transparent; border:none; }"
            "QPushButton { font-weight:bold; border:none; }"
            );
        overlay->setGeometry(host->rect());

        auto *outer = new QVBoxLayout(overlay);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);
        outer->addStretch(1);
        auto *centerRow = new QHBoxLayout;
        centerRow->setContentsMargins(0, 0, 0, 0);
        centerRow->addStretch(1);
        auto *panel = new QFrame(overlay);
        panel->setObjectName("RunInfoPanel");
        panel->setFixedSize(qMin(960, int(mWinW * 0.60)), qMin(690, int(mWinH * 0.68)));
        centerRow->addWidget(panel);
        centerRow->addStretch(1);
        outer->addLayout(centerRow);
        outer->addStretch(1);

        auto *root = new QVBoxLayout(panel);
        root->setContentsMargins(18, 12, 18, 12);
        root->setSpacing(8);

        QWidget *top = new QWidget(panel);
        auto *topL = new QVBoxLayout(top);
        topL->setContentsMargins(0,0,0,0);
        topL->setSpacing(3);
        QLabel *arrow = new QLabel("▼", top);
        QFont af = mCNFont; af.setPixelSize(uiPx(24)); af.setBold(true);
        arrow->setFont(af);
        arrow->setAlignment(Qt::AlignCenter);
        arrow->setFixedHeight(26);
        arrow->setStyleSheet("color:#ff5f55;");
        // 不把箭头交给 layout 管理，否则初始位置容易被 layout 拉回最左侧。
        // 只预留一行高度，然后用 move() 精确指向当前 tab 的中心。
        arrow->move(0, 0);
        topL->addSpacing(26);
        QWidget *tabRow = new QWidget(top);
        auto *tabL = new QHBoxLayout(tabRow);
        tabL->setContentsMargins(70,0,70,0);
        tabL->setSpacing(12);
        topL->addWidget(tabRow);
        root->addWidget(top);

        QStackedWidget *pages = new QStackedWidget(panel);
        pages->setStyleSheet("background:transparent; border:none;");
        root->addWidget(pages, 1);

        auto moveArrowToTab = [arrow](QPushButton *tab) {
            if (!arrow || !tab || !arrow->parentWidget()) return;
            QWidget *parent = arrow->parentWidget();
            const QPoint topLeft = tab->mapTo(parent, QPoint(0, 0));
            const int tabW = qMax(1, tab->width());
            // 让 QLabel 占满整个按钮宽度，文字居中；这样箭头视觉中心稳定落在“牌型”按钮正中间。
            arrow->setGeometry(topLeft.x(), 0, tabW, 26);
            arrow->raise();
        };

        auto makeTab = [&](const QString &txt, int pageIdx) {
            auto *b = new QPushButton(txt, tabRow);
            QFont f = mCNFont; f.setPixelSize(uiPx(19)); f.setBold(true);
            b->setFont(f);
            b->setFixedHeight(50);
            b->setStyleSheet(
                "QPushButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ff7a70, stop:1 #d63f39); color:white; border:2px solid rgba(255,255,255,70); border-radius:14px; padding:8px 24px; }"
                "QPushButton:hover { background:#ff756d; border:2px solid rgba(255,255,255,150); }"
                "QPushButton:pressed { background:#bb342f; }"
                );
            connect(b, &QPushButton::clicked, this, [pages, pageIdx, moveArrowToTab, b]() {
                pages->setCurrentIndex(pageIdx);
                QTimer::singleShot(0, b, [moveArrowToTab, b]() { moveArrowToTab(b); });
            });
            tabL->addWidget(b, 1);
            return b;
        };

        auto makeDarkPage = [&](QWidget *parent) {
            auto *w = new QWidget(parent);
            w->setAttribute(Qt::WA_StyledBackground, true);
            w->setStyleSheet("background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(39,58,61,235), stop:1 rgba(16,27,30,235)); border:3px solid rgba(7,14,16,230); border-radius:16px;");
            return w;
        };
        auto makeHandRow = [&](QWidget *parent, const QString &level, int levelNum, const QString &name,
                               const QString &chips, const QString &mult, const QString &played) {
            QWidget *row = new QWidget(parent);
            row->setFixedHeight(48);
            row->setAttribute(Qt::WA_StyledBackground, true);
            // 原版 G.C.UI.BACKGROUND_INACTIVE 深底带细描边；之前用浅色 → 白字看不清。
            row->setStyleSheet("background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 rgba(36,49,53,245), stop:1 rgba(22,32,35,245)); border:2px solid #1a262a; border-radius:14px;");
            auto *h = new QHBoxLayout(row);
            h->setContentsMargins(8,4,8,4);
            h->setSpacing(8);
            auto addPill = [&](const QString &txt, const QString &bg, int w) {
                QLabel *l = new QLabel(txt, row);
                QFont f = mCNFont; f.setPixelSize(uiPx(16)); f.setBold(true);
                l->setFont(f); l->setAlignment(Qt::AlignCenter);
                l->setFixedWidth(w);
                l->setStyleSheet(QString("background:%1; color:white; border-radius:10px; padding:3px 8px;").arg(bg));
                h->addWidget(l);
                return l;
            };
            // 等级标签的底色对齐原版 HAND_LEVELS 调色板：等级越高颜色越鲜艳。
            // 原版 UI_definitions.lua:3047-3048: level 1 文本用深色，其它用浅色——这里统一用深色更清晰。
            const QString lvlBg = handLevelColor(levelNum);
            addPill(level, lvlBg, 92)->setStyleSheet(
                QString("background:%1; color:#23584f; border-radius:10px; padding:3px 8px;").arg(lvlBg));
            QLabel *n = new QLabel(name, row);
            QFont nf = mCNFont; nf.setPixelSize(uiPx(17)); nf.setBold(true);
            n->setFont(nf); n->setAlignment(Qt::AlignCenter); n->setStyleSheet("color:white; background:transparent;");
            h->addWidget(n, 1);
            addPill(chips, "#009dff", 86);
            QLabel *x = new QLabel("X", row);
            QFont xf = mCNFont; xf.setPixelSize(uiPx(18)); xf.setBold(true); x->setFont(xf); x->setStyleSheet("color:#fe5f55; background:transparent;");
            h->addWidget(x);
            addPill(mult, "#fe5f55", 72);
            QLabel *hash = new QLabel("#", row); hash->setFont(xf); hash->setStyleSheet("color:white; background:transparent;"); h->addWidget(hash);
            addPill(played, "#3b4d50", 56);
            return row;
        };
        auto makeInfoTile = [&](QWidget *parent, const QString &title, const QString &body, const QString &accent) {
            QWidget *tile = new QWidget(parent);
            tile->setAttribute(Qt::WA_StyledBackground, true);
            tile->setStyleSheet(QString("background:rgba(14,23,25,215); border:3px solid %1; border-radius:13px;").arg(accent));
            auto *v = new QVBoxLayout(tile); v->setContentsMargins(12,8,12,8); v->setSpacing(5);
            QLabel *t = new QLabel(title, tile); QFont tf2=mCNFont; tf2.setPixelSize(uiPx(19)); tf2.setBold(true); t->setFont(tf2); t->setAlignment(Qt::AlignCenter); t->setStyleSheet(QString("color:%1;").arg(accent)); v->addWidget(t);
            QLabel *b = new QLabel(body, tile); QFont bf=mCNFont; bf.setPixelSize(uiPx(15)); bf.setBold(true); b->setFont(bf); b->setAlignment(Qt::AlignCenter); b->setWordWrap(true); b->setStyleSheet("color:#f4fbfb;"); v->addWidget(b,1);
            return tile;
        };

        QWidget *handPage = makeDarkPage(pages);
        auto *handV = new QVBoxLayout(handPage);
        handV->setContentsMargins(78, 18, 78, 18);
        handV->setSpacing(5);
        QVector<HandType> order = {
            HandType::FlushFive, HandType::FiveOfAKind, HandType::FlushHouse, HandType::Flush,
            HandType::Straight, HandType::ThreeOfAKind, HandType::TwoPair, HandType::Pair, HandType::HighCard
        };
        const auto &levels = mGameState->handLevels();
        for (HandType t : order) {
            HandLevel lv = levels.value(t);
            auto b = baseScore(t);
            handV->addWidget(makeHandRow(handPage,
                                         QString("等级%1").arg(lv.level),
                                         lv.level,
                                         handName(t),
                                         formatScoreNumber(b.first + lv.chipsBonus),
                                         formatScoreNumber(b.second + lv.multBonus),
                                         QString::number(lv.played)));
        }
        pages->addWidget(handPage);

        QWidget *blindPage = makeDarkPage(pages);
        auto *blindH = new QHBoxLayout(blindPage); blindH->setContentsMargins(36, 22, 36, 22); blindH->setSpacing(14);
        BossInfo bi = mGameState->currentBossInfo();
        BossInfo nextBi = bossInfo(mGameState->pendingBossEffect());
        // 原版同 ante 三盲注：小盲 ×1、大盲 ×1.5、Boss ×2 的目标分；并明确奖励 $3/$4/$5。
        const int currentIdx = mGameState->blindIdx();   // 0=小、1=大、2=Boss
        // 已经在 Boss 战时，targetScore 已是当前 ×2；否则 ×3 推算出 Boss 目标。
        const double baseBlind = (currentIdx >= 2) ? (mGameState->targetScore() / 2.0)
                                                   : ((currentIdx == 1) ? (mGameState->targetScore() / 1.5)
                                                                        : double(mGameState->targetScore()));
        const double smallTarget = std::max(1.0, baseBlind);
        const double bigTarget   = smallTarget * 1.5;
        const double bossTarget  = smallTarget * 2.0;
        QString bossName = nextBi.name.isEmpty() ? bi.name : nextBi.name;
        QString bossDesc = nextBi.description.isEmpty() ? bi.description : nextBi.description;
        if (bossName.isEmpty()) { bossName = "Boss 盲注"; bossDesc = "未知效果"; }

        auto makeBlindCard = [&](const QString &title, const QString &target,
                                 const QString &reward, const QString &accent,
                                 const QString &subtitle = QString(),
                                 bool active = false) {
            QWidget *tile = new QWidget(blindPage);
            tile->setAttribute(Qt::WA_StyledBackground, true);
            tile->setStyleSheet(QString(
                "background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                " stop:0 rgba(20,30,33,235), stop:1 rgba(11,19,22,235));"
                " border:3px solid %1; border-radius:14px;"
            ).arg(accent));
            auto *v = new QVBoxLayout(tile); v->setContentsMargins(10, 12, 10, 12); v->setSpacing(6);

            auto *header = new QLabel(title, tile);
            QFont hf = mCNFont; hf.setPixelSize(uiPx(20)); hf.setBold(true);
            header->setFont(hf); header->setAlignment(Qt::AlignCenter);
            header->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(accent));
            v->addWidget(header);

            if (!subtitle.isEmpty()) {
                auto *sub = new QLabel(subtitle, tile);
                QFont sf = mCNFont; sf.setPixelSize(uiPx(13)); sf.setBold(true);
                sub->setFont(sf); sub->setAlignment(Qt::AlignCenter); sub->setWordWrap(true);
                sub->setStyleSheet("color:#cfd9da; background:transparent; border:none;");
                v->addWidget(sub);
            }

            auto *label = new QLabel("过关分数", tile);
            QFont lf = mCNFont; lf.setPixelSize(uiPx(13));
            label->setFont(lf); label->setAlignment(Qt::AlignCenter);
            label->setStyleSheet("color:#9bb6bd; background:transparent; border:none;");
            v->addWidget(label);

            auto *score = new QLabel(target, tile);
            QFont scf = mCNFont; scf.setPixelSize(uiPx(28)); scf.setBold(true);
            score->setFont(scf); score->setAlignment(Qt::AlignCenter);
            score->setStyleSheet("color:#fe5f55; background:transparent; border:none;");
            v->addWidget(score);
            v->addStretch(1);

            auto *rwd = new QLabel("奖励", tile);
            rwd->setFont(lf); rwd->setAlignment(Qt::AlignCenter);
            rwd->setStyleSheet("color:#9bb6bd; background:transparent; border:none;");
            v->addWidget(rwd);

            auto *coins = new QLabel(reward, tile);
            QFont cf = mCNFont; cf.setPixelSize(uiPx(22)); cf.setBold(true);
            coins->setFont(cf); coins->setAlignment(Qt::AlignCenter);
            coins->setStyleSheet("color:#eac058; background:transparent; border:none;");
            v->addWidget(coins);

            if (active) {
                auto *now = new QLabel("正在挑战", tile);
                QFont nf = mCNFont; nf.setPixelSize(uiPx(13)); nf.setBold(true);
                now->setFont(nf); now->setAlignment(Qt::AlignCenter);
                now->setStyleSheet(QString(
                    "color:white; background:%1; border-radius:8px;"
                    " padding:3px 8px;"
                ).arg(accent));
                v->addWidget(now);
            }
            return tile;
        };

        blindH->addWidget(makeBlindCard("小盲注",
                                        formatScoreNumber(smallTarget),
                                        "$$$",
                                        "#4bc292",
                                        QString(),
                                        currentIdx == 0), 1);
        blindH->addWidget(makeBlindCard("大盲注",
                                        formatScoreNumber(bigTarget),
                                        "$$$$",
                                        "#fda200",
                                        QString(),
                                        currentIdx == 1), 1);
        blindH->addWidget(makeBlindCard(bossName,
                                        formatScoreNumber(bossTarget),
                                        "$$$$$",
                                        "#fe5f55",
                                        bossDesc,
                                        currentIdx == 2), 1);
        pages->addWidget(blindPage);

        QWidget *voucherPage = makeDarkPage(pages);
        auto *voucherV = new QVBoxLayout(voucherPage);
        voucherV->setContentsMargins(48, 22, 48, 22);
        voucherV->setSpacing(14);
        voucherV->setAlignment(Qt::AlignTop);

        const auto redeemed = mGameState->redeemedVouchers();
        QLabel *voucherTitle = new QLabel(
            QString("本赛局兑换的优惠券　×%1").arg(redeemed.size()),
            voucherPage);
        QFont vtf=mCNFont; vtf.setPixelSize(uiPx(22)); vtf.setBold(true);
        voucherTitle->setFont(vtf); voucherTitle->setAlignment(Qt::AlignCenter);
        voucherTitle->setStyleSheet("color:#fd682b; background:transparent; border:none;");
        voucherV->addWidget(voucherTitle);

        if (redeemed.isEmpty()) {
            QLabel *empty = new QLabel("本赛局尚未兑换任何优惠券\n进入商店购买可永久增益", voucherPage);
            QFont ef=mCNFont; ef.setPixelSize(uiPx(18)); ef.setBold(true);
            empty->setFont(ef); empty->setAlignment(Qt::AlignCenter); empty->setWordWrap(true);
            empty->setStyleSheet("color:#9bb6bd; background:transparent; border:none;"
                                 " padding:30px 0px;");
            voucherV->addWidget(empty, 1);
        } else {
            QWidget *gridW = new QWidget(voucherPage);
            auto *grid = new QGridLayout(gridW);
            grid->setSpacing(12);
            grid->setContentsMargins(0,0,0,0);
            int vi = 0;
            QPixmap voucherSheet(":/textures/images/Vouchers.png");
            for (VoucherType v : redeemed) {
                const VoucherData vd = voucherData(v);
                QWidget *card = new QWidget(gridW);
                card->setFixedSize(dp(238), dp(108));
                card->setAttribute(Qt::WA_StyledBackground,true);
                // 与盲注卡保持同款渐变 + 优惠券品牌色边框（#fd682b）。
                card->setStyleSheet(
                    "background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                    " stop:0 rgba(20,30,33,235), stop:1 rgba(11,19,22,235));"
                    " border:2px solid #fd682b; border-radius:12px;"
                );
                auto *h = new QHBoxLayout(card); h->setContentsMargins(10,8,10,8); h->setSpacing(10);
                QLabel *img = new QLabel(card); img->setFixedSize(dp(64), dp(84));
                img->setAlignment(Qt::AlignCenter);
                img->setStyleSheet("background:transparent; border:none;");
                if (!voucherSheet.isNull()) {
                    QPoint c = vd.spritePos;
                    QPixmap pm = voucherSheet.copy(c.x()*ConsumableItem::SRC_W, c.y()*ConsumableItem::SRC_H,
                                                   ConsumableItem::SRC_W, ConsumableItem::SRC_H);
                    img->setPixmap(pm.scaled(img->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                h->addWidget(img);

                QWidget *txtBox = new QWidget(card);
                txtBox->setStyleSheet("background:transparent; border:none;");
                auto *tv = new QVBoxLayout(txtBox);
                tv->setContentsMargins(0,0,0,0); tv->setSpacing(2);
                QLabel *name = new QLabel(vd.name, txtBox);
                QFont nf = mCNFont; nf.setPixelSize(uiPx(15)); nf.setBold(true);
                name->setFont(nf);
                name->setStyleSheet("color:#fda200; background:transparent; border:none;");
                tv->addWidget(name);
                QLabel *desc = new QLabel(vd.description, txtBox);
                QFont df = mCNFont; df.setPixelSize(uiPx(12)); df.setBold(true);
                desc->setFont(df); desc->setWordWrap(true);
                desc->setStyleSheet("color:#eaffff; background:transparent; border:none;");
                tv->addWidget(desc, 1);
                h->addWidget(txtBox, 1);

                grid->addWidget(card, vi/2, vi%2); ++vi;
            }
            voucherV->addWidget(gridW, 1, Qt::AlignTop | Qt::AlignHCenter);
        }
        pages->addWidget(voucherPage);

        QWidget *stakePage = makeDarkPage(pages);
        auto *stakeV = new QVBoxLayout(stakePage);
        stakeV->setContentsMargins(48, 22, 48, 22);
        stakeV->setSpacing(14);

        // 顶部：当前赌注名 + 同款描述（与盲注卡牌头部样式呼应）。
        QWidget *header = new QWidget(stakePage);
        header->setAttribute(Qt::WA_StyledBackground, true);
        header->setStyleSheet(
            "background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            " stop:0 rgba(20,30,33,235), stop:1 rgba(11,19,22,235));"
            " border:3px solid #009dff; border-radius:14px;"
        );
        auto *hv = new QVBoxLayout(header); hv->setContentsMargins(14, 12, 14, 12); hv->setSpacing(4);
        QLabel *stakeTitle = new QLabel("蓝注", header);
        QFont sff=mCNFont; sff.setPixelSize(uiPx(22)); sff.setBold(true);
        stakeTitle->setFont(sff); stakeTitle->setAlignment(Qt::AlignCenter);
        stakeTitle->setStyleSheet("color:#009dff; background:transparent; border:none;");
        hv->addWidget(stakeTitle);
        QLabel *stakeBody = new QLabel(
            "弃牌次数 -1\n商店可能会出现永恒小丑牌\n底注提升时过关分数增速更快\n小盲注没有奖励金",
            header);
        QFont sbf=mCNFont; sbf.setPixelSize(uiPx(14)); sbf.setBold(true);
        stakeBody->setFont(sbf); stakeBody->setAlignment(Qt::AlignCenter); stakeBody->setWordWrap(true);
        stakeBody->setStyleSheet("color:#eaffff; background:transparent; border:none;");
        hv->addWidget(stakeBody);
        stakeV->addWidget(header);

        // 下半：当前资源 stat grid——每一项一个小方块，4 列。
        QWidget *gridWrap = new QWidget(stakePage);
        gridWrap->setAttribute(Qt::WA_StyledBackground, true);
        gridWrap->setStyleSheet(
            "background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            " stop:0 rgba(20,30,33,235), stop:1 rgba(11,19,22,235));"
            " border:3px solid #4bc292; border-radius:14px;"
        );
        auto *gw = new QVBoxLayout(gridWrap); gw->setContentsMargins(14, 12, 14, 12); gw->setSpacing(8);
        QLabel *resTitle = new QLabel("当前赛局", gridWrap);
        QFont rtf=mCNFont; rtf.setPixelSize(uiPx(18)); rtf.setBold(true);
        resTitle->setFont(rtf); resTitle->setAlignment(Qt::AlignCenter);
        resTitle->setStyleSheet("color:#4bc292; background:transparent; border:none;");
        gw->addWidget(resTitle);

        auto *grid = new QGridLayout;
        grid->setSpacing(10);
        gw->addLayout(grid);

        auto makeStatCell = [&](const QString &label, const QString &value, const QString &color) {
            QWidget *cell = new QWidget(gridWrap);
            cell->setAttribute(Qt::WA_StyledBackground, true);
            cell->setStyleSheet(
                "background:rgba(8,16,18,200);"
                " border:2px solid #1a262a; border-radius:10px;"
            );
            auto *cv = new QVBoxLayout(cell); cv->setContentsMargins(8, 8, 8, 8); cv->setSpacing(2);
            QLabel *lbl = new QLabel(label, cell);
            QFont lf = mCNFont; lf.setPixelSize(uiPx(12));
            lbl->setFont(lf); lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet("color:#9bb6bd; background:transparent; border:none;");
            cv->addWidget(lbl);
            QLabel *val = new QLabel(value, cell);
            QFont vf = mCNFont; vf.setPixelSize(uiPx(20)); vf.setBold(true);
            val->setFont(vf); val->setAlignment(Qt::AlignCenter);
            val->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(color));
            cv->addWidget(val);
            return cell;
        };

        grid->addWidget(makeStatCell("底注",
                                     QString("%1/8").arg(mGameState->ante()),
                                     "#fda200"), 0, 0);
        grid->addWidget(makeStatCell("金币",
                                     QString("$%1").arg(mGameState->gold()),
                                     "#eac058"), 0, 1);
        grid->addWidget(makeStatCell("小丑",
                                     QString("%1/%2").arg(mGameState->jokers().size())
                                                     .arg(mGameState->jokerSlots()),
                                     "#fe5f55"), 0, 2);
        grid->addWidget(makeStatCell("消耗牌",
                                     QString("%1/%2").arg(mGameState->consumables().size())
                                                     .arg(mGameState->consumableSlots()),
                                     "#a782d1"), 0, 3);
        grid->addWidget(makeStatCell("牌堆",
                                     QString("%1/%2").arg(mGameState->deckRemaining())
                                                     .arg(mGameState->deckTotal()),
                                     "#009dff"), 1, 0);
        grid->addWidget(makeStatCell("出牌",
                                     QString::number(mGameState->handsLeft()),
                                     "#23e6ff"), 1, 1);
        grid->addWidget(makeStatCell("弃牌",
                                     QString::number(mGameState->discardLeft()),
                                     "#ff7066"), 1, 2);
        grid->addWidget(makeStatCell("优惠券",
                                     QString::number(mGameState->redeemedVouchers().size()),
                                     "#fd682b"), 1, 3);
        stakeV->addWidget(gridWrap);
        stakeV->addStretch(1);
        pages->addWidget(stakePage);

        QVector<QPushButton*> tabButtons;
        tabButtons << makeTab("牌型", 0) << makeTab("盲注", 1) << makeTab("优惠券", 2) << makeTab("赌注", 3);
        pages->setCurrentIndex(0);
        QTimer::singleShot(0, overlay, [moveArrowToTab, firstTab = tabButtons.value(0)]() {
            moveArrowToTab(firstTab);
        });
        QTimer::singleShot(80, overlay, [moveArrowToTab, firstTab = tabButtons.value(0)]() {
            moveArrowToTab(firstTab);
        });
        QTimer::singleShot(160, overlay, [moveArrowToTab, firstTab = tabButtons.value(0)]() {
            moveArrowToTab(firstTab);
        });

        auto *back = new QPushButton("返回", panel);
        QFont bf = mCNFont; bf.setPixelSize(uiPx(21)); bf.setBold(true); back->setFont(bf);
        back->setFixedHeight(46);
        back->setStyleSheet("QPushButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffc45b, stop:1 #e58c00); color:white; border:2px solid #ffe2a0; border-radius:14px; } QPushButton:hover { background:#ffb730; }");
        connect(back, &QPushButton::clicked, overlay, [this, overlay]() {
            overlay->hide();
            overlay->deleteLater();
            if (mRunInfoOverlay == overlay) mRunInfoOverlay = nullptr;
            resumeGameProcesses();
        });
        root->addWidget(back);

        // 打开比赛信息：暂停计分动画/火焰/背景等一切局内进程。
        // 必须在面板自身的滑入动画创建之前调用，避免把它一起暂停。
        pauseGameProcesses();

        // 滑入动画：面板从下方升起 + 背景渐显。沿用 mShopWidget 的入场手感。
        overlay->show();
        overlay->raise();
        const QPoint endPos = panel->pos();
        const QPoint startPos(endPos.x(), endPos.y() + qMax(160, host->height() / 2));
        panel->move(startPos);
        auto *anim = new QPropertyAnimation(panel, "pos", overlay);
        anim->setDuration(280);
        anim->setStartValue(startPos);
        anim->setEndValue(endPos);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    });

    // 与 "比赛信息" 同样尺寸：原版 options 按钮 minw=1.5 / minh=1.75。
    QPushButton *btnOptions = makeBtn("选项", "#fda200", "#ffb730", mCNFont, btnCol, dp(136));
    btnOptions->setFixedWidth(dp(116));
    btnOptions->setStyleSheet(QString(
        "QPushButton { background:#fda200; color:white; border:2px solid rgba(255,255,255,80);"
        " border-radius:14px; font-size:%1px; font-weight:bold; padding:6px 10px; }"
        "QPushButton:hover { background:#ffb730; border:2px solid rgba(255,255,255,150); }"
        "QPushButton:pressed { background:#d98a00; padding-top:6px; }"
        ).arg(uiPx(18)));   // 字号与 "比赛信息" 对齐
    btnVbl->addWidget(btnOptions);
    connect(btnOptions, &QPushButton::clicked, this, &MainWindow::showOptionsOverlay);

    brl->addWidget(btnCol);

    QWidget *rightCol = new QWidget(bottomRow);
    rightCol->setAttribute(Qt::WA_StyledBackground, true);
    rightCol->setStyleSheet("background:transparent; border:none;");
    auto *rcvbl = new QVBoxLayout(rightCol);
    rcvbl->setContentsMargins(0, 0, 0, 0);
    rcvbl->setSpacing(dp(6));

    QWidget *handsRow = new QWidget(rightCol);
    handsRow->setFixedHeight(dp(104));
    handsRow->setAttribute(Qt::WA_StyledBackground, true);
    handsRow->setStyleSheet("background:#334044; border:none; border-radius:16px;");
    auto *hrl = new QHBoxLayout(handsRow);
    hrl->setContentsMargins(dp(8), dp(4), dp(8), dp(4));
    hrl->setSpacing(dp(4));

    QWidget *hCell = new QWidget(handsRow);
    hCell->setAttribute(Qt::WA_StyledBackground, true);
    hCell->setStyleSheet("background:transparent; border:none;");
    auto *hcv = new QVBoxLayout(hCell);
    hcv->setContentsMargins(0, 0, 0, 0);
    hcv->setSpacing(0);
    hcv->setAlignment(Qt::AlignCenter);
    hcv->addWidget(makeLabel("出牌", 13, "white", mCNFont, hCell));
    mLblHands = makeLabel("4", 28, "#009dff", mPixelFont, hCell);
    hcv->addWidget(mLblHands);
    hrl->addWidget(hCell);

    QWidget *dCell = new QWidget(handsRow);
    dCell->setAttribute(Qt::WA_StyledBackground, true);
    dCell->setStyleSheet("background:transparent; border:none;");
    auto *dcv = new QVBoxLayout(dCell);
    dcv->setContentsMargins(0, 0, 0, 0);
    dcv->setSpacing(0);
    dcv->setAlignment(Qt::AlignCenter);
    dcv->addWidget(makeLabel("弃牌", 13, "white", mCNFont, dCell));
    mLblDiscards = makeLabel("3", 28, "#fe5f55", mPixelFont, dCell);
    dcv->addWidget(mLblDiscards);
    hrl->addWidget(dCell);

    rcvbl->addWidget(handsRow);

    QWidget *goldRow = new QWidget(rightCol);
    goldRow->setFixedHeight(dp(76));
    goldRow->setAttribute(Qt::WA_StyledBackground, true);
    goldRow->setStyleSheet("background:#304235; border:none; border-radius:16px;");
    auto *gbl = new QHBoxLayout(goldRow);
    gbl->setContentsMargins(dp(10), dp(4), dp(10), dp(4));
    gbl->setSpacing(dp(8));
    gbl->setAlignment(Qt::AlignCenter);

    mLblGold = makeLabel("$4", 31, "#f3b958", mPixelFont, goldRow);
    gbl->addWidget(mLblGold);
    rcvbl->addWidget(goldRow);

    QWidget *anteRow2 = new QWidget(rightCol);
    anteRow2->setAttribute(Qt::WA_StyledBackground, true);
    anteRow2->setStyleSheet("background:transparent; border:none;");
    auto *arl = new QHBoxLayout(anteRow2);
    arl->setContentsMargins(0, 0, 0, 0);
    arl->setSpacing(dp(4));

    QWidget *anteBox = new QWidget(anteRow2);
    anteBox->setFixedHeight(dp(88));
    anteBox->setAttribute(Qt::WA_StyledBackground, true);
    anteBox->setStyleSheet("background:#334044; border:none; border-radius:16px;");
    auto *avbl = new QVBoxLayout(anteBox);
    avbl->setContentsMargins(dp(6), dp(3), dp(6), dp(3));
    avbl->setSpacing(0);
    avbl->setAlignment(Qt::AlignCenter);
    avbl->addWidget(makeLabel("底注", 13, "white", mCNFont, anteBox));
    mLblAnte = makeLabel("1<font color='white'>/8</font>", 23, "#ff9a00", mPixelFont, anteBox);
    mLblAnte->setTextFormat(Qt::RichText);
    avbl->addWidget(mLblAnte);
    arl->addWidget(anteBox);

    QWidget *roundBox = new QWidget(anteRow2);
    roundBox->setFixedHeight(dp(88));
    roundBox->setAttribute(Qt::WA_StyledBackground, true);
    roundBox->setStyleSheet("background:#334044; border:none; border-radius:16px;");
    auto *rvbl = new QVBoxLayout(roundBox);
    rvbl->setContentsMargins(dp(6), dp(3), dp(6), dp(3));
    rvbl->setSpacing(0);
    rvbl->setAlignment(Qt::AlignCenter);
    rvbl->addWidget(makeLabel("回合", 13, "white", mCNFont, roundBox));
    mLblRound = makeLabel("1", 23, "#ff9a00", mPixelFont, roundBox);
    rvbl->addWidget(mLblRound);
    arl->addWidget(roundBox);

    rcvbl->addWidget(anteRow2);

    brl->addWidget(rightCol, 1);
    // 中段 stretch 必须和顶部 stretch 同权重，否则空白被一边吃掉，整组组件就不在垂直正中。
    layout->addStretch(1);
    layout->addWidget(bottomRow);
    layout->addStretch(1);
}


void MainWindow::showOptionsOverlay()
{
    // 打开选项：暂停一切局内进程（计分动画/火焰/背景）。
    // 在面板滑入动画创建之前调用——animateIn 经 singleShot(0) 延后执行，不会被一起暂停。
    pauseGameProcesses();
    // 不再使用 QDialog::exec()：全屏窗口上叠加/关闭顶层原生对话框时，
    // QOpenGLWidget 可能在部分 Windows 显卡驱动上重建后台缓冲，表现为瞬时黑屏。
    // 这里改为普通子 QWidget 覆盖层，和游戏场景共用同一个窗口，不触发原生窗口切换。
    auto animateIn = [this]() {
        if (!mOptionsOverlay) return;
        QWidget *p = mOptionsOverlay->findChild<QFrame*>("OptionsPanel");
        if (!p) return;
        const QPoint endPos = p->pos();
        const QPoint startPos(endPos.x(), endPos.y() + qMax(160, mOptionsOverlay->height() / 2));
        p->move(startPos);
        auto *anim = new QPropertyAnimation(p, "pos", mOptionsOverlay);
        anim->setDuration(280);
        anim->setStartValue(startPos);
        anim->setEndValue(endPos);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    };

    if (mOptionsOverlay) {
        mOptionsOverlay->setGeometry(centralWidget() ? centralWidget()->rect() : rect());
        mOptionsOverlay->raise();
        mOptionsOverlay->show();
        // 复用覆盖层时也补一次滑入动画。
        QTimer::singleShot(0, this, animateIn);
        return;
    }

    QWidget *host = centralWidget() ? centralWidget() : this;
    mOptionsOverlay = new QWidget(host);
    mOptionsOverlay->setObjectName("OptionsOverlay");
    mOptionsOverlay->setAttribute(Qt::WA_StyledBackground, true);
    mOptionsOverlay->setStyleSheet(QString(
        "QWidget#OptionsOverlay { background:rgba(0,0,0,88); }"
        "QFrame#OptionsPanel { background:rgba(36,51,54,245); border:3px solid #dce9e9; border-radius:18px; }"
        "QLabel { color:white; background:transparent; }"
        "QPushButton { background:#fe5f55; color:white; border:none; border-radius:12px;"
        " padding:%1px %2px; font-size:%3px; font-weight:bold; }"
        "QPushButton:hover { background:#ff7066; }"
        "QPushButton:pressed { background:#d94a42; }"
        "QPushButton:disabled { background:#394347; color:#8f9a9c; }"
        "QPushButton#back { background:#fda200; }"
        "QPushButton#back:hover { background:#ffb730; }"
        ).arg(uiPx(14)).arg(uiPx(20)).arg(uiPx(16)));

    auto *root = new QVBoxLayout(mOptionsOverlay);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addStretch(1);

    auto *centerRow = new QHBoxLayout;
    centerRow->setContentsMargins(0, 0, 0, 0);
    centerRow->addStretch(1);

    auto *panel = new QFrame(mOptionsOverlay);
    panel->setObjectName("OptionsPanel");
    panel->setFixedSize(dp(460), dp(720));   // 高度跟随 78×7 + 标题/间距/边距
    auto *v = new QVBoxLayout(panel);
    v->setContentsMargins(dp(30), dp(26), dp(30), dp(26));
    v->setSpacing(dp(10));

    QFont titleFont = mCNFont;
    titleFont.setPixelSize(uiPx(16));
    titleFont.setBold(true);

    auto *title = new QLabel("选项", panel);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    v->addWidget(title);

    const int kBtnH = dp(78);     // 进一步抬高，按钮厚实些与原版主菜单观感对齐
    const QStringList items = {"设置", "开始新的一局", "主菜单", "统计数据", "定制牌组"};
    for (const QString &txt : items) {
        auto *b = new QPushButton(txt, panel);
        b->setFont(titleFont);
        b->setMinimumHeight(kBtnH);
        if (txt == "开始新的一局") {
            connect(b, &QPushButton::clicked, this, &MainWindow::startNewRunFromOptions);
        } else if (txt == "设置") {
            connect(b, &QPushButton::clicked, this, [this]() {
                hideOptionsOverlay();
                showSettingsOverlay();
            });
        } else if (txt == "主菜单") {
            connect(b, &QPushButton::clicked, this, [this]() {
                hideOptionsOverlay();
                showMainMenuOverlay();
            });
        } else if (txt == "统计数据") {
            connect(b, &QPushButton::clicked, this, [this]() {
                hideOptionsOverlay();
                showStatsOverlay();
            });
        } else if (txt == "定制牌组") {
            connect(b, &QPushButton::clicked, this, [this]() {
                hideOptionsOverlay();
                showDeckCustomizeOverlay();
            });
        } else {
            b->setEnabled(false);
        }
        v->addWidget(b);
    }

    auto *back = new QPushButton("返回", panel);
    back->setObjectName("back");
    back->setFont(titleFont);
    back->setMinimumHeight(kBtnH);
    connect(back, &QPushButton::clicked, this, [this]() {
        hideOptionsOverlay();
        resumeGameProcesses();
    });
    v->addWidget(back);

    centerRow->addWidget(panel);
    centerRow->addStretch(1);
    root->addLayout(centerRow);
    root->addStretch(1);

    mOptionsOverlay->setGeometry(host->rect());
    mOptionsOverlay->raise();
    mOptionsOverlay->show();
    // 等 layout 完成后再触发滑入动画——否则 panel.pos() 还是 (0,0)。
    QTimer::singleShot(0, this, animateIn);
}

void MainWindow::hideOptionsOverlay()
{
    if (!mOptionsOverlay) return;
    mOptionsOverlay->hide();
}

void MainWindow::startNewRunFromOptions()
{
    // 丢弃暂停中的计分进程：新开局后不应恢复上一局的定时器/动画。
    for (auto &t : mGameTimers) if (t) { t->stop(); t->deleteLater(); }
    mGameTimers.clear();
    if (mScoreCountAnim) mScoreCountAnim->stop();
    mGamePaused = false;

    // 保持覆盖层可见直到所有状态和界面刷新完成，避免玩家看到半帧清空场景。
    // 不再 setUpdatesEnabled(false)，因为整窗禁用/恢复更新也会在部分机器上触发黑底中间帧。
    resetTransientOverlaysForNewRun();
    mGameState->startGame();
    mHasOngoingRun = true;   // 一旦开过新局,主菜单里 "继续当前局" 就该亮起
    refreshHand();
    refreshJokerSlots();
    refreshConsumableSlots();
    refreshCounters();
    refreshScore();
    refreshGold();

    if (mView) mView->viewport()->update();
    if (mDynamicBg) mDynamicBg->update();
    update();
    hideOptionsOverlay();
}

void MainWindow::showSettingsOverlay()
{
    // 复用 options 覆盖层模式（in-scene QWidget）：避免在全屏 + QOpenGLWidget 场景里弹原生 QDialog。
    QWidget *host = mPlayPage ? mPlayPage : this;

    if (mSettingsOverlay) {
        mSettingsOverlay->setGeometry(host->rect());
        mSettingsOverlay->raise();
        mSettingsOverlay->show();
        return;
    }

    auto *overlay = new QWidget(host);
    overlay->setAttribute(Qt::WA_StyledBackground, true);
    overlay->setStyleSheet("background:rgba(0,0,0,160);");
    mSettingsOverlay = overlay;

    auto *root = new QVBoxLayout(overlay);
    root->setContentsMargins(0, 0, 0, 0);
    root->setAlignment(Qt::AlignCenter);
    root->addStretch(1);

    auto *centerRow = new QHBoxLayout;
    centerRow->setAlignment(Qt::AlignCenter);
    centerRow->addStretch(1);

    auto *panel = new QWidget(overlay);
    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setStyleSheet(
        "background:#374244; border:3px solid #4f6367; border-radius:18px;"
    );
    panel->setFixedWidth(dp(520));

    auto *v = new QVBoxLayout(panel);
    v->setContentsMargins(dp(24), dp(20), dp(24), dp(20));
    v->setSpacing(dp(14));

    QFont titleFont = mCNFont;
    titleFont.setPixelSize(uiPx(30));
    titleFont.setBold(true);
    auto *title = new QLabel("设置", panel);
    title->setFont(titleFont);
    title->setStyleSheet("color:#eaffff; background:transparent; border:none;");
    title->setAlignment(Qt::AlignCenter);
    v->addWidget(title);

    QFont labelFont = mCNFont;
    labelFont.setPixelSize(uiPx(18));
    labelFont.setBold(true);

    // 通用滑条样式 + 行构造器。
    const QString sliderQss = QString(
        "QSlider::groove:horizontal { height:8px; background:#1f2a2c;"
        " border-radius:4px; }"
        "QSlider::sub-page:horizontal { background:#fda200; border-radius:4px; }"
        "QSlider::handle:horizontal { background:#fda200; width:18px;"
        " margin:-6px 0; border-radius:9px; border:2px solid #ffe6a8; }"
        "QSlider::handle:horizontal:hover { background:#ffb730; }"
    );

    auto makeSliderRow = [&](const QString &name, int initialPct,
                             std::function<void(int)> onChange) {
        auto *row = new QWidget(panel);
        row->setStyleSheet("background:transparent; border:none;");
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(dp(10));

        auto *lbl = new QLabel(name, row);
        lbl->setFont(labelFont);
        lbl->setStyleSheet("color:#eaffff; background:transparent; border:none;");
        lbl->setFixedWidth(dp(110));
        h->addWidget(lbl);

        auto *slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(0, 100);
        slider->setValue(initialPct);
        slider->setStyleSheet(sliderQss);
        h->addWidget(slider, 1);

        auto *value = new QLabel(QString::number(initialPct) + "%", row);
        value->setFont(labelFont);
        value->setStyleSheet("color:#fda200; background:transparent; border:none;");
        // dp(58) 在 100% 时容不下三位数 + %，会被截断成 "00%"。给到 dp(80) 留足空间。
        value->setFixedWidth(dp(80));
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(value);

        connect(slider, &QSlider::valueChanged, row, [value, onChange](int v) {
            value->setText(QString::number(v) + "%");
            onChange(v);
        });

        v->addWidget(row);
    };

    auto *audio = AudioManager::instance();
    makeSliderRow("主音量", int(std::round(audio->masterVolume() * 100.0)),
                  [audio](int v) { audio->setMasterVolume(v / 100.0); });
    makeSliderRow("音乐音量", int(std::round(audio->musicVolume() * 100.0)),
                  [audio](int v) { audio->setMusicVolume(v / 100.0); });
    makeSliderRow("音效音量", int(std::round(audio->sfxVolume() * 100.0)),
                  [audio](int v) { audio->setSfxVolume(v / 100.0); });

    // 倍速:1x/2x/4x/8x —— 通过 mGameSpeedFactor 缩短计分链上每个 scheduleGame 的等待时间。
    {
        auto *row = new QWidget(panel);
        row->setStyleSheet("background:transparent; border:none;");
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(dp(10));

        auto *lbl = new QLabel("倍速", row);
        lbl->setFont(labelFont);
        lbl->setStyleSheet("color:#eaffff; background:transparent; border:none;");
        lbl->setFixedWidth(dp(110));
        h->addWidget(lbl);

        auto *btnRow = new QWidget(row);
        btnRow->setStyleSheet("background:transparent; border:none;");
        auto *bh = new QHBoxLayout(btnRow);
        bh->setContentsMargins(0, 0, 0, 0);
        bh->setSpacing(dp(8));

        const QVector<double> speeds = { 1.0, 2.0, 4.0, 8.0 };
        QVector<QPushButton*> btns;
        for (double s : speeds) {
            auto *b = new QPushButton(QString("%1x").arg(int(s)), btnRow);
            b->setFont(labelFont);
            b->setMinimumHeight(dp(34));
            b->setCheckable(true);
            btns.append(b);
            bh->addWidget(b, 1);
        }

        auto applyStyles = [btns]() {
            for (auto *b : btns) {
                const bool on = b->isChecked();
                b->setStyleSheet(QString(
                    "QPushButton { background:%1; color:%2;"
                    " border:2px solid %3; border-radius:10px; padding:4px 10px;"
                    " font-weight:bold; }"
                    "QPushButton:hover { background:%4; }"
                ).arg(on ? "#fda200" : "#1f2a2c",
                      on ? "#101216" : "#eaffff",
                      on ? "#ffe6a8" : "#3b4347",
                      on ? "#ffb730" : "#2a3539"));
            }
        };

        for (int i = 0; i < btns.size(); ++i) {
            const double s = speeds[i];
            btns[i]->setChecked(qFuzzyCompare(s, mGameSpeedFactor));
            connect(btns[i], &QPushButton::clicked, this, [this, btns, speeds, i, applyStyles]() {
                for (int j = 0; j < btns.size(); ++j) btns[j]->setChecked(j == i);
                setGameSpeedFactor(speeds[i]);
                applyStyles();
            });
        }
        applyStyles();

        h->addWidget(btnRow, 1);
        v->addWidget(row);
    }

    auto *back = new QPushButton("返回", panel);
    QFont btnFont = mCNFont; btnFont.setPixelSize(uiPx(20)); btnFont.setBold(true);
    back->setFont(btnFont);
    back->setMinimumHeight(dp(56));
    back->setStyleSheet(
        "QPushButton { background:#fe5f55; color:white;"
        " border:2px solid rgba(255,255,255,90); border-radius:12px;"
        " font-weight:bold; padding:6px 18px; }"
        "QPushButton:hover { background:#ff7066; border:2px solid rgba(255,255,255,170); }"
        "QPushButton:pressed { background:#d94a42; }"
    );
    connect(back, &QPushButton::clicked, this, &MainWindow::hideSettingsOverlay);
    v->addSpacing(dp(6));
    v->addWidget(back);

    centerRow->addWidget(panel);
    centerRow->addStretch(1);
    root->addLayout(centerRow);
    root->addStretch(1);

    overlay->setGeometry(host->rect());
    overlay->raise();
    overlay->show();
}

void MainWindow::hideSettingsOverlay()
{
    if (mSettingsOverlay) mSettingsOverlay->hide();
    resumeGameProcesses();
}

// ────────────────────────────────────────────────────────────────────────────
// 统计数据 / 收藏 / 定制牌组 — 选项菜单的三个二级界面
// 都复用同一种 in-scene QWidget overlay 模式（与 settings/main menu 一致）。
// ────────────────────────────────────────────────────────────────────────────
namespace {
// 便于复用：返回一个深底圆角面板和它的根 QVBoxLayout。
QPair<QWidget*, QVBoxLayout*> makeOverlayPanel(QWidget *parent, int widthDp, const QString &accent)
{
    auto *overlay = new QWidget(parent);
    overlay->setAttribute(Qt::WA_StyledBackground, true);
    overlay->setStyleSheet("background:rgba(0,0,0,170);");
    auto *root = new QVBoxLayout(overlay);
    root->setContentsMargins(0, 0, 0, 0);
    root->setAlignment(Qt::AlignCenter);

    auto *panel = new QWidget(overlay);
    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setStyleSheet(QString(
        "background:#1f2a2c; border:3px solid %1; border-radius:18px;"
    ).arg(accent));
    panel->setFixedWidth(widthDp);
    auto *row = new QHBoxLayout;
    row->setAlignment(Qt::AlignCenter);
    row->addWidget(panel);
    root->addLayout(row);

    auto *v = new QVBoxLayout(panel);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(10);
    return { overlay, v };
}
}

void MainWindow::showStatsOverlay()
{
    QWidget *host = mPlayPage ? mPlayPage : this;
    if (mStatsOverlay) { mStatsOverlay->hide(); mStatsOverlay->deleteLater(); }
    auto p = makeOverlayPanel(host, dp(520), "#4bc292");
    mStatsOverlay = p.first;
    mStatsOverlay->setGeometry(host->rect());

    QFont titleFont = mCNFont; titleFont.setPixelSize(uiPx(28)); titleFont.setBold(true);
    auto *title = new QLabel("统计数据");
    title->setFont(titleFont); title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color:#4bc292; background:transparent; border:none;");
    p.second->addWidget(title);

    QFont rowFont = mCNFont; rowFont.setPixelSize(uiPx(17));
    auto addStat = [&](const QString &name, const QString &value) {
        auto *line = new QWidget;
        line->setAttribute(Qt::WA_StyledBackground, true);
        line->setStyleSheet("background:rgba(8,16,18,200); border:2px solid #1a262a; border-radius:10px;");
        auto *h = new QHBoxLayout(line);
        h->setContentsMargins(12, 8, 12, 8);
        auto *l = new QLabel(name);
        l->setFont(rowFont); l->setStyleSheet("color:#9bb6bd; background:transparent; border:none;");
        h->addWidget(l);
        h->addStretch(1);
        auto *r = new QLabel(value);
        QFont vf = rowFont; vf.setBold(true);
        r->setFont(vf); r->setStyleSheet("color:#fda200; background:transparent; border:none;");
        h->addWidget(r);
        p.second->addWidget(line);
    };

    addStat("当前 Ante",          QString("%1 / 8").arg(mGameState->ante()));
    addStat("当前金币",            QString("$%1").arg(mGameState->gold()));
    addStat("本局已打出手数",      QString::number(mGameState->totalHandsPlayedThisRun()));
    addStat("本局已跳过盲注",      QString::number(mGameState->totalSkipsThisRun()));
    addStat("本局累计弃牌（剩余）",QString::number(mGameState->unusedDiscardsThisRun()));
    addStat("已兑换优惠券数",      QString::number(mGameState->redeemedVouchers().size()));
    addStat("持有小丑",            QString("%1 / %2").arg(mGameState->jokers().size())
                                                     .arg(mGameState->jokerSlots()));
    addStat("持有消耗牌",          QString("%1 / %2").arg(mGameState->consumables().size())
                                                     .arg(mGameState->consumableSlots()));
    addStat("牌堆余 / 总",         QString("%1 / %2").arg(mGameState->deckRemaining())
                                                     .arg(mGameState->deckTotal()));

    auto *back = new QPushButton("返回");
    QFont btnFont = mCNFont; btnFont.setPixelSize(uiPx(20)); btnFont.setBold(true);
    back->setFont(btnFont); back->setMinimumHeight(dp(56));
    back->setStyleSheet(
        "QPushButton { background:#fe5f55; color:white; border:2px solid rgba(255,255,255,90);"
        " border-radius:12px; font-weight:bold; padding:6px 18px; }"
        "QPushButton:hover { background:#ff7066; border:2px solid rgba(255,255,255,170); }"
        "QPushButton:pressed { background:#d94a42; }"
    );
    connect(back, &QPushButton::clicked, this, [this]() {
        if (mStatsOverlay) { mStatsOverlay->hide(); mStatsOverlay->deleteLater(); }
    });
    p.second->addWidget(back);

    mStatsOverlay->raise();
    mStatsOverlay->show();
}

void MainWindow::showCollectionOverlay()
{
    QWidget *host = mPlayPage ? mPlayPage : this;
    if (mCollectionOverlay) { mCollectionOverlay->hide(); mCollectionOverlay->deleteLater(); }
    auto p = makeOverlayPanel(host, dp(640), "#a782d1");
    mCollectionOverlay = p.first;
    mCollectionOverlay->setGeometry(host->rect());

    QFont titleFont = mCNFont; titleFont.setPixelSize(uiPx(28)); titleFont.setBold(true);
    auto *title = new QLabel("收藏");
    title->setFont(titleFont); title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color:#a782d1; background:transparent; border:none;");
    p.second->addWidget(title);

    auto *body = new QLabel(
        "已持有的小丑 / 消耗牌 / 优惠券会在游戏过程中自动登记。\n\n"
        "本局已获取的内容：\n"
        "・小丑：" + QString::number(mGameState->jokers().size()) + " 张\n"
        "・消耗牌：" + QString::number(mGameState->consumables().size()) + " 张\n"
        "・优惠券：" + QString::number(mGameState->redeemedVouchers().size()) + " 张\n\n"
        "完整图鉴（包含未发现的卡牌剪影）正在制作中。"
    );
    QFont bf = mCNFont; bf.setPixelSize(uiPx(16));
    body->setFont(bf); body->setWordWrap(true); body->setAlignment(Qt::AlignCenter);
    body->setStyleSheet("color:#eaffff; background:transparent; border:none; padding:10px 0;");
    p.second->addWidget(body);

    auto *back = new QPushButton("返回");
    QFont btnFont = mCNFont; btnFont.setPixelSize(uiPx(20)); btnFont.setBold(true);
    back->setFont(btnFont); back->setMinimumHeight(dp(56));
    back->setStyleSheet(
        "QPushButton { background:#fe5f55; color:white; border:2px solid rgba(255,255,255,90);"
        " border-radius:12px; font-weight:bold; padding:6px 18px; }"
        "QPushButton:hover { background:#ff7066; border:2px solid rgba(255,255,255,170); }"
        "QPushButton:pressed { background:#d94a42; }"
    );
    connect(back, &QPushButton::clicked, this, [this]() {
        if (mCollectionOverlay) { mCollectionOverlay->hide(); mCollectionOverlay->deleteLater(); }
    });
    p.second->addWidget(back);

    mCollectionOverlay->raise();
    mCollectionOverlay->show();
}

void MainWindow::showDeckCustomizeOverlay()
{
    QWidget *host = mPlayPage ? mPlayPage : this;
    if (mDeckCustomizeOverlay) { mDeckCustomizeOverlay->hide(); mDeckCustomizeOverlay->deleteLater(); }
    auto p = makeOverlayPanel(host, dp(460), "#fda200");
    mDeckCustomizeOverlay = p.first;
    mDeckCustomizeOverlay->setGeometry(host->rect());

    QFont titleFont = mCNFont; titleFont.setPixelSize(uiPx(28)); titleFont.setBold(true);
    auto *title = new QLabel("定制牌组");
    title->setFont(titleFont); title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color:#fda200; background:transparent; border:none;");
    p.second->addWidget(title);

    auto *body = new QLabel("敬请期待");
    QFont bf = mCNFont; bf.setPixelSize(uiPx(28)); bf.setBold(true);
    body->setFont(bf); body->setAlignment(Qt::AlignCenter);
    body->setStyleSheet("color:#eaffff; background:transparent; border:none; padding:36px 0;");
    p.second->addWidget(body);

    auto *back = new QPushButton("返回");
    QFont btnFont = mCNFont; btnFont.setPixelSize(uiPx(20)); btnFont.setBold(true);
    back->setFont(btnFont); back->setMinimumHeight(dp(56));
    back->setStyleSheet(
        "QPushButton { background:#fe5f55; color:white; border:2px solid rgba(255,255,255,90);"
        " border-radius:12px; font-weight:bold; padding:6px 18px; }"
        "QPushButton:hover { background:#ff7066; border:2px solid rgba(255,255,255,170); }"
        "QPushButton:pressed { background:#d94a42; }"
    );
    connect(back, &QPushButton::clicked, this, [this]() {
        if (mDeckCustomizeOverlay) { mDeckCustomizeOverlay->hide(); mDeckCustomizeOverlay->deleteLater(); }
    });
    p.second->addWidget(back);

    mDeckCustomizeOverlay->raise();
    mDeckCustomizeOverlay->show();
}

void MainWindow::showMainMenuOverlay()
{
    // 参考原版 main_menu：大 Balatro 标题 + Play/Continue/Options/Quit 列。
    // 主菜单需要"全屏另一界面",不再像 overlay 那样半透明叠在游戏画面上——
    // 父级挂在 centralWidget(),覆盖左侧面板 + 右侧场景;并用纯不透明背景把游戏整体盖掉。
    QWidget *host = centralWidget() ? centralWidget()
                                    : (mPlayPage ? mPlayPage : this);
    // "继续当前局" 的可用状态依赖 mHasOngoingRun,缓存会让状态过时——
    // 直接销毁并重建,保证启动 vs. 局中调出时的按钮状态都新鲜。
    if (mMainMenuOverlay) {
        mMainMenuOverlay->hide();
        mMainMenuOverlay->deleteLater();
        mMainMenuOverlay = nullptr;
    }

    auto *overlay = new QWidget(host);
    overlay->setAttribute(Qt::WA_StyledBackground, true);
    // 不透明背景:整体覆盖游戏 UI,玩家感觉是切到另一页面而不是浮层。
    overlay->setStyleSheet("background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                           " stop:0 #30384d, stop:1 #202839);");
    mMainMenuOverlay = overlay;

    auto *root = new QVBoxLayout(overlay);
    root->setContentsMargins(0, 0, 0, 0);
    root->setAlignment(Qt::AlignCenter);
    root->addStretch(1);

    // 大标题 BALATRO，复用 balatro.png 中的 logo。
    QPixmap logoPix(":/textures/images/balatro.png");
    if (!logoPix.isNull()) {
        auto *logoLbl = new QLabel(overlay);
        // 原图较大，按场景宽度缩到 ~520 dp。
        QPixmap scaled = logoPix.scaled(dp(520), dp(180),
                                        Qt::KeepAspectRatio, Qt::SmoothTransformation);
        logoLbl->setPixmap(scaled);
        logoLbl->setAlignment(Qt::AlignCenter);
        logoLbl->setStyleSheet("background:transparent;");
        auto *logoRow = new QHBoxLayout;
        logoRow->setAlignment(Qt::AlignCenter);
        logoRow->addWidget(logoLbl);
        root->addLayout(logoRow);
        root->addSpacing(dp(18));
    } else {
        QFont tf = mCNFont; tf.setPixelSize(uiPx(54)); tf.setBold(true);
        auto *title = new QLabel("BALATRO", overlay);
        title->setFont(tf); title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("color:#fda200; background:transparent;");
        root->addWidget(title);
    }

    auto *centerRow = new QHBoxLayout;
    centerRow->setAlignment(Qt::AlignCenter);
    centerRow->addStretch(1);

    auto *panel = new QWidget(overlay);
    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setStyleSheet(
        "background:#374244; border:3px solid #4f6367; border-radius:18px;"
    );
    panel->setFixedWidth(dp(420));
    auto *v = new QVBoxLayout(panel);
    v->setContentsMargins(dp(22), dp(20), dp(22), dp(20));
    v->setSpacing(dp(10));

    QFont btnFont = mCNFont; btnFont.setPixelSize(uiPx(24)); btnFont.setBold(true);

    auto makeMenuButton = [&](const QString &text, const QString &bg, const QString &hover,
                              bool enabled) {
        auto *b = new QPushButton(text, panel);
        b->setFont(btnFont);
        b->setMinimumHeight(dp(64));
        b->setEnabled(enabled);
        b->setStyleSheet(QString(
            "QPushButton { background:%1; color:white;"
            " border:2px solid rgba(255,255,255,90); border-radius:14px;"
            " font-weight:bold; padding:6px 10px; }"
            "QPushButton:hover { background:%2; border:2px solid rgba(255,255,255,170); }"
            "QPushButton:disabled { background:#3b4347; color:#7c8488;"
            " border:2px solid #3b4347; }"
        ).arg(bg, hover));
        v->addWidget(b);
        return b;
    };

    // 继续当前局：仅当已开过至少一局后可用,启动直进主菜单时禁用。
    auto *btnContinue = makeMenuButton("继续当前局", "#4ca893", "#5fbfa8", mHasOngoingRun);
    connect(btnContinue, &QPushButton::clicked, this, &MainWindow::hideMainMenuOverlay);

    // 开始新一局:橙色,主操作。
    auto *btnPlay = makeMenuButton("开始新的一局", "#fda200", "#ffb730", true);
    connect(btnPlay, &QPushButton::clicked, this, [this]() {
        hideMainMenuOverlay();
        startNewRunFromOptions();
    });

    auto *btnSettings = makeMenuButton("设置", "#646eb7", "#7681d0", true);
    connect(btnSettings, &QPushButton::clicked, this, [this]() {
        hideMainMenuOverlay();
        showSettingsOverlay();
    });

    auto *btnQuit = makeMenuButton("退出游戏", "#fe5f55", "#ff7066", true);
    connect(btnQuit, &QPushButton::clicked, this, []() { QCoreApplication::quit(); });

    centerRow->addWidget(panel);
    centerRow->addStretch(1);
    root->addLayout(centerRow);
    root->addStretch(1);

    overlay->setGeometry(host->rect());
    overlay->raise();
    overlay->show();
}

void MainWindow::hideMainMenuOverlay()
{
    if (mMainMenuOverlay) mMainMenuOverlay->hide();
}

void MainWindow::setupScene() {
    mDynamicBg = new DynamicBackgroundItem(mPlayPage);
    mDynamicBg->setGeometry(mPlayPage ? mPlayPage->rect() : QRect(0, 0, mSceneW, mSceneH));
    mDynamicBg->setSceneSize(mDynamicBg->width() > 0 ? mDynamicBg->width() : mSceneW,
                             mDynamicBg->height() > 0 ? mDynamicBg->height() : mSceneH);
    mDynamicBg->setMood(DynamicBackgroundItem::Mood::Default);
    mDynamicBg->show();
    mDynamicBg->lower();

    mView = new QGraphicsView(mScene, mPlayPage);
    mView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    mView->setFrameShape(QFrame::NoFrame);
    mView->setAlignment(Qt::AlignCenter);
    mView->setStyleSheet("background: transparent; border: none;");
    mView->setAttribute(Qt::WA_TranslucentBackground, true);
    mView->setAutoFillBackground(false);
    mView->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    mView->viewport()->setAutoFillBackground(false);
    mView->setBackgroundBrush(QBrush(Qt::NoBrush));

    mView->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    mView->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    mView->setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    mScene->setSceneRect(0, 0, mSceneW, mSceneH);
    mScene->setBackgroundBrush(QBrush(Qt::NoBrush));
    QTimer::singleShot(0, this, [this]() { updateSceneSize(); });

    mJokerCountLabel = mScene->addText("0/5");
    mJokerCountLabel->setDefaultTextColor(QColor("#d7e7d2"));
    { QFont countFont = mCNFont; countFont.setPixelSize(uiPx(14)); countFont.setBold(true); mJokerCountLabel->setFont(countFont); }
    mJokerCountLabel->setZValue(30);

    mConsCountLabel = mScene->addText("0/2");
    mConsCountLabel->setDefaultTextColor(QColor("#d7e7d2"));
    { QFont countFont = mCNFont; countFont.setPixelSize(uiPx(14)); countFont.setBold(true); mConsCountLabel->setFont(countFont); }
    mConsCountLabel->setZValue(30);

    refreshJokerSlotFrames();
    refreshConsumableSlotFrames();

    mPlayBgRect = nullptr;

    mHandCountLabel = mScene->addText("8/8");
    QFont hcf = mCNFont; hcf.setPixelSize(uiPx(13));
    mHandCountLabel->setFont(hcf);
    mHandCountLabel->setDefaultTextColor(QColor("#aaddaa"));
    mHandCountLabel->setZValue(30);

    CardData backData;
    backData.faceUp = false;
    mHandYNormal  = mSceneH - CARD_H - 190;
    mHandYScoring = mSceneH - CARD_H - 130;
    mHandY = mHandYNormal;
    mBtnY  = mSceneH - 118;

    mDeckBackCard = new CardItem(backData);
    // 牌堆按钮永远停在"下沉手牌"那条线（mHandYScoring）：
    // 出牌阶段手牌从 mHandYNormal 滑下来到这里时就和牌堆等高，hover 牌组 / 出牌时无需再让牌堆移动。
    mDeckBackCard->setPos(mSceneW - CARD_W - 60, mHandYScoring);
    mDeckBackCard->setZValue(1);
    // 牌堆是 "查看牌组" 按钮，不应像手牌那样被拖到其他位置。
    mDeckBackCard->setDraggable(false);
    // 也别再随鼠标位置做 3D 倾斜——牌堆按钮要看上去稳稳停在那里。
    mDeckBackCard->setHoverTiltEnabled(false);
    // 严格命中：鼠标在牌面之上的空白带不触发悬浮，避免"还没到牌堆就开始抖动"。
    mDeckBackCard->setStrictHoverShape(true);
    mScene->addItem(mDeckBackCard);
    connect(mDeckBackCard, &CardItem::clicked, this, &MainWindow::onDeckClicked);
    connect(mDeckBackCard, &CardItem::hoverChanged, this, &MainWindow::onDeckHoverChanged);

    mDeckLabel = mScene->addText("52/52");
    QFont df = mCNFont; df.setPixelSize(uiPx(12));
    mDeckLabel->setFont(df);
    mDeckLabel->setDefaultTextColor(QColor("#cccccc"));
    mDeckLabel->setPos(mSceneW - CARD_W - 4, mSceneH - 34);
    mDeckLabel->setZValue(2);

    QRectF deckTextBr = mDeckLabel->boundingRect();
    mDeckLabel->setPos(mSceneW - CARD_W - 60 + (CARD_W - deckTextBr.width()) / 2.0,
                       mHandYScoring + CARD_H + 6);
}

void MainWindow::setupSceneButtons() {
    int btnW = 176;
    int btnH = 96;          // 抬高 → 给 "理牌" 容器内 "点数/花色" 子按钮腾出底部空间。

    // 透明背景 + 圆角按钮在 QGraphicsProxyWidget 里默认会被 QWidget 自身绘制一层硬矩形底，
    // 形成"圆角外面又一圈矩形边框"的视觉残影。给 button 自身加 WA_TranslucentBackground
    // 才能彻底让圆角外区域真空。
    auto makeSceneBtn = [&](const QString &text, const QString &bg, const QString &hover) {
        QPushButton *b = makeBtn(text, bg, hover, mCNFont, nullptr, btnH);
        b->setFixedWidth(btnW);
        b->setAttribute(Qt::WA_TranslucentBackground, true);
        return b;
    };

    mBtnPlay = makeSceneBtn("出牌", "#009dff", "#33b0ff");
    mPlayProxy = mScene->addWidget(mBtnPlay);
    mPlayProxy->setZValue(50);

    // 用 QFrame 让 Qt 走原生绘框路径——QWidget + 透明背景 + stylesheet border 在
    // QGraphicsProxyWidget 里有时会丢失边框；用 QFrame::Box + 自定义 paintEvent 更稳。
    // 这里取最简方案：QWidget 上画一圈白色描边的圆角矩形。
    class SortContainer : public QWidget {
    public:
        using QWidget::QWidget;
    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(QPen(QColor(255, 255, 255, 230), 2));
            p.setBrush(Qt::NoBrush);
            const qreal pad = 1.0;
            p.drawRoundedRect(QRectF(pad, pad, width() - 2 * pad, height() - 2 * pad), 14, 14);
        }
    };
    auto *sortContainer = new SortContainer;
    sortContainer->setFixedSize(btnW, btnH);
    sortContainer->setAttribute(Qt::WA_TranslucentBackground, true);
    sortContainer->setStyleSheet("background:transparent;");
    auto *scbl = new QVBoxLayout(sortContainer);
    scbl->setContentsMargins(8, 6, 8, 8);
    scbl->setSpacing(5);
    scbl->setAlignment(Qt::AlignCenter);

    QLabel *sortLbl = new QLabel("理牌", sortContainer);
    QFont slf = mCNFont; slf.setPixelSize(uiPx(16)); slf.setBold(true);
    sortLbl->setFont(slf);
    sortLbl->setAlignment(Qt::AlignCenter);
    // 容器变透明后字体颜色得改成白色才看得清。
    sortLbl->setStyleSheet("color:#f3f8f4; background:transparent; border:none;");
    scbl->addWidget(sortLbl);

    auto *subRow = new QWidget(sortContainer);
    subRow->setStyleSheet("background:transparent; border:none;");
    auto *subl = new QHBoxLayout(subRow);
    subl->setContentsMargins(0, 0, 0, 0);
    subl->setSpacing(4);
    // 子按钮 36px 高度配合 96px 容器、上 6 + 下 8 margin、与标签 spacing 5，
    // 整体占 22(label)+5+36 = 63px，留出 ≈18px 余量，"点数/花色" 底边不会再被裁。
    mBtnSortNum  = makeBtn("点数", "#fda200", "#ffb730", mCNFont, subRow, 36);
    mBtnSortSuit = makeBtn("花色", "#fda200", "#ffb730", mCNFont, subRow, 36);
    mBtnSortNum->setAttribute(Qt::WA_TranslucentBackground, true);
    mBtnSortSuit->setAttribute(Qt::WA_TranslucentBackground, true);
    subl->addWidget(mBtnSortNum);
    subl->addWidget(mBtnSortSuit);
    scbl->addWidget(subRow);

    mSortProxy = mScene->addWidget(sortContainer);
    mSortProxy->setZValue(50);

    mBtnDiscard = makeSceneBtn("弃牌", "#fe5f55", "#ff7066");
    mDiscardProxy = mScene->addWidget(mBtnDiscard);
    mDiscardProxy->setZValue(50);

    mBtnBestPlay = makeSceneBtn("最佳出牌", "#8a4fd3", "#9b60e8");
    mBestPlayProxy = mScene->addWidget(mBtnBestPlay);
    mBestPlayProxy->setZValue(50);
    connect(mBtnBestPlay, &QPushButton::clicked, this, &MainWindow::onBestPlayHint);

    // 占卜按钮:与"最佳出牌"同色系但偏青绿,位置放在"弃牌"右侧。
    mBtnForesight = makeSceneBtn("占卜", "#2ec4b6", "#46d8c8");
    mForesightProxy = mScene->addWidget(mBtnForesight);
    mForesightProxy->setZValue(50);
    connect(mBtnForesight, &QPushButton::clicked, this, &MainWindow::onForesightClicked);
    mBtnForesight->setEnabled(false);   // 无选中手牌时灰掉

    layoutSceneButtons();
}

void MainWindow::layoutSceneButtons() {
    if (!mPlayProxy || !mSortProxy || !mDiscardProxy || !mBestPlayProxy) return;
    const int btnW = 176;
    const int gap = 16;
    const int slotCount = mForesightProxy ? 5 : 4;
    const int totalW = btnW * slotCount + gap * (slotCount - 1);
    const int startX = (mSceneW - HAND_RIGHT_RESERVE - totalW) / 2;
    const int y = mBtnY;

    // 顺序:最佳出牌 / 出牌 / 理牌 / 弃牌 / 占卜
    mBestPlayProxy->setPos(startX, y);
    mPlayProxy->setPos(startX + (btnW + gap), y);
    mSortProxy->setPos(startX + (btnW + gap) * 2, y);
    mDiscardProxy->setPos(startX + (btnW + gap) * 3, y);
    if (mForesightProxy) mForesightProxy->setPos(startX + (btnW + gap) * 4, y);

    // 记录按钮原位,出牌时滑出屏幕,计分完成后滑回。
    mBestPlayBtnHome = mBestPlayProxy->pos();
    mPlayBtnHome     = mPlayProxy->pos();
    mSortBtnHome     = mSortProxy->pos();
    mDiscardBtnHome  = mDiscardProxy->pos();
    if (mForesightProxy) mForesightBtnHome = mForesightProxy->pos();
}

void MainWindow::updateSceneSize() {
    if (!mPlayPage || !mScene) return;
    const int playW = qMax(1, mPlayPage->width());
    const int playH = qMax(1, mPlayPage->height());

    // 场景高度固定为设计基准 1080；宽度跟随窗口实际纵横比，避免 fitInView 留黑边。
    // 极端超宽 / 超窄屏限制在 [0.85x, 1.7x] 设计宽度之间，否则按钮和手牌会被撑得太散或挤压。
    const double aspect = double(playW) / double(playH);
    const int designH = DESIGN_SCENE_H;
    const int designW = DESIGN_SCENE_W;
    const int minW = int(designW * 0.85);
    const int maxW = int(designW * 1.70);
    int newSceneW = qBound(minW, int(std::round(designH * aspect)), maxW);
    int newSceneH = designH;
    if (newSceneW == mSceneW && newSceneH == mSceneH) {
        fitSceneToView();
        return;
    }
    mSceneW = newSceneW;
    mSceneH = newSceneH;
    mScene->setSceneRect(0, 0, mSceneW, mSceneH);

    // 同步所有依赖 mSceneW/mSceneH 的元素位置。
    mHandYNormal  = mSceneH - CARD_H - 190;
    mHandYScoring = mSceneH - CARD_H - 130;
    mHandY = mHandYNormal;
    mBtnY  = mSceneH - 118;
    if (mDeckBackCard) mDeckBackCard->setPos(mSceneW - CARD_W - 60, mHandYScoring);
    if (mDeckLabel) {
        QRectF br = mDeckLabel->boundingRect();
        mDeckLabel->setPos(mSceneW - CARD_W - 60 + (CARD_W - br.width()) / 2.0,
                           mHandYScoring + CARD_H + 6);
    }
    if (mDeckStatsItem) mDeckStatsItem->setVisible(false);

    layoutSceneButtons();
    // 重排小丑 / 消耗品 / 手牌（依赖 mSceneW 居中）。
    // 小丑因为没有独立的 layout 函数，仍走 refresh 重建；其它项目只重排位置。
    refreshJokerSlotFrames();
    refreshConsumableSlotFrames();
    if (!mJokerItems.isEmpty()) refreshJokerSlots();
    layoutConsumableItems(false);
    layoutHandCards();
    layoutPlayedCards();
    fitSceneToView();
}


QRect MainWindow::sceneRectOnPlayPage(const QPointF &sceneTopLeft, const QSizeF &sceneSize) const
{
    if (!mView || !mPlayPage) return QRect();
    const QPoint viewTL = mView->mapFromScene(sceneTopLeft);
    const QPoint viewBR = mView->mapFromScene(sceneTopLeft + QPointF(sceneSize.width(), sceneSize.height()));
    QRect viewRect(viewTL, viewBR);
    viewRect = viewRect.normalized();
    QPoint pageTL = mView->mapTo(mPlayPage, viewRect.topLeft());
    QSize pageSize(qMax(1, viewRect.width()), qMax(1, viewRect.height()));
    return QRect(pageTL, pageSize);
}

QPixmap MainWindow::deckBackPixmap() const
{
    QPixmap enh(":/textures/images/Enhancers.png");
    if (enh.isNull()) return QPixmap();
    return enh.copy(0, 0, CardItem::SRC_W, CardItem::SRC_H);
}

void MainWindow::animateTopLayerCardToScene(const QPixmap &pixmap, const QPoint &globalCenter,
                                            const QPointF &targetSceneTopLeft, const QSizeF &sceneSize,
                                            bool flipToBack, QGraphicsObject *revealItem)
{
    if (pixmap.isNull() || !mPlayPage || !mView) {
        if (revealItem) revealItem->setOpacity(1.0);
        return;
    }

    QRect targetRect = sceneRectOnPlayPage(targetSceneTopLeft, sceneSize);
    if (!targetRect.isValid() || targetRect.width() <= 1 || targetRect.height() <= 1) {
        targetRect = QRect(QPoint(int(targetSceneTopLeft.x()), int(targetSceneTopLeft.y())),
                           QSize(int(sceneSize.width()), int(sceneSize.height())));
    }

    QSize startSize = targetRect.size();
    QPixmap front = pixmap.scaled(startSize, Qt::KeepAspectRatio, Qt::FastTransformation);
    QPixmap back;
    if (flipToBack) {
        back = deckBackPixmap().scaled(startSize, Qt::KeepAspectRatio, Qt::FastTransformation);
        if (back.isNull()) back = front;
    }

    const QPoint pageCenter = mPlayPage->mapFromGlobal(globalCenter);
    QRect startRect(QPoint(pageCenter.x() - front.width() / 2,
                           pageCenter.y() - front.height() / 2),
                    QSize(front.width(), front.height()));

    auto *ghost = new QLabel(mPlayPage);
    ghost->setAttribute(Qt::WA_TranslucentBackground, true);
    ghost->setScaledContents(true);
    ghost->setPixmap(front);
    ghost->setGeometry(startRect);
    ghost->show();
    ghost->raise();
    if (mShopWidget && mShopWidget->isVisible()) mShopWidget->raise();
    if (mPackOpenWidget && mPackOpenWidget->isVisible()) mPackOpenWidget->raise();
    ghost->raise();  // 明确盖在商店/开包 overlay 上方，避免“从商店下面滑动”。

    auto *anim = new QVariantAnimation(ghost);
    anim->setDuration(flipToBack ? 520 : 380);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    QPointer<QLabel> ghostGuard(ghost);
    QPointer<QGraphicsObject> revealGuard(revealItem);
    connect(anim, &QVariantAnimation::valueChanged, this,
            [ghostGuard, front, back, startRect, targetRect, flipToBack](const QVariant &v) mutable {
        if (!ghostGuard) return;
        const qreal t = v.toDouble();
        QPointF c0 = startRect.center();
        QPointF c1 = targetRect.center();
        QPointF c = c0 + (c1 - c0) * t;

        qreal lift = std::sin(t * 3.14159265358979323846) * 22.0;
        QSizeF baseSize(startRect.width() + (targetRect.width() - startRect.width()) * t,
                        startRect.height() + (targetRect.height() - startRect.height()) * t);
        qreal wScale = 1.0;
        if (flipToBack) {
            qreal fp = qMin<qreal>(1.0, t / 0.62);
            wScale = qMax<qreal>(0.14, std::abs(std::cos(fp * 3.14159265358979323846)));
            ghostGuard->setPixmap((fp >= 0.50) ? back : front);
        }
        QSize sz(qMax(2, int(baseSize.width() * wScale)), qMax(2, int(baseSize.height())));
        ghostGuard->setGeometry(QRect(QPoint(int(c.x() - sz.width() / 2.0),
                                            int(c.y() - sz.height() / 2.0 - lift)), sz));
        ghostGuard->setWindowOpacity(1.0 - 0.10 * t);
    });
    connect(anim, &QVariantAnimation::finished, this, [ghostGuard, revealGuard]() {
        if (revealGuard) revealGuard->setOpacity(1.0);
        if (ghostGuard) ghostGuard->deleteLater();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::prepareSlotFlyInAnimation(const QPixmap &pixmap, const QPoint &globalCenter, int targetArea)
{
    if (targetArea <= 0 || pixmap.isNull() || !mScene || !mView) {
        mPendingSlotFlyIn = PendingSlotFlyIn();
        return;
    }

    QPixmap scaled = pixmap.scaled(CARD_W, CARD_H, Qt::KeepAspectRatio, Qt::FastTransformation);
    QPointF center = mView->mapToScene(mView->mapFromGlobal(globalCenter));
    const QPointF startTopLeft = center - QPointF(scaled.width() / 2.0, scaled.height() / 2.0);

    if (targetArea == 1 || targetArea == 2) {
        mPendingSlotFlyIn.active = true;
        mPendingSlotFlyIn.targetArea = targetArea;
        mPendingSlotFlyIn.sceneStartTopLeft = startTopLeft;
        mPendingSlotFlyIn.sceneSize = QSizeF(scaled.width(), scaled.height());
        mPendingSlotFlyIn.pixmap = pixmap;
        mPendingSlotFlyIn.globalCenter = globalCenter;
        return;
    }

    // 扑克牌购买后加入牌组：顶层飞行，前半段翻成背面，再落到右下方牌堆。
    QPointF target(mDeckBackCard ? mDeckBackCard->pos() : QPointF(mSceneW - CARD_W - 60, mHandYScoring));
    animateTopLayerCardToScene(pixmap, globalCenter, target, QSizeF(CARD_W, CARD_H), true, nullptr);
}

void MainWindow::setupConnections() {
    connect(mBtnPlay, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(mBtnDiscard, &QPushButton::clicked, this, &MainWindow::onDiscardClicked);
    connect(mBtnSortNum,  &QPushButton::clicked, this, &MainWindow::onSortByNum);
    connect(mBtnSortSuit, &QPushButton::clicked, this, &MainWindow::onSortBySuit);

    connect(mGameState, &GameState::handChanged, this, &MainWindow::refreshHand);
    connect(mGameState, &GameState::scoreChanged, this, &MainWindow::refreshScore);
    connect(mGameState, &GameState::goldChanged, this, &MainWindow::refreshGold);
    connect(mGameState, &GameState::handPlayed, this, &MainWindow::onHandPlayed);
    connect(mGameState, &GameState::endRoundCardTriggered, this, [this](const QVector<ScoreEvent> &events) {
        mEndRoundAnimationDelay = qMax(260, 260 + events.size() * 150);
        for (int i = 0; i < events.size(); ++i) {
            const ScoreEvent ev = events[i];
            QTimer::singleShot(i * 150, this, [this, ev]() {
                playScoreEvent(ev);
            });
        }
    });

    connect(mGameState, &GameState::roundWon, this, &MainWindow::onRoundWon);
    connect(mGameState, &GameState::gameOver, this, &MainWindow::onGameOver);
    connect(mGameState, &GameState::jokersChanged, this, &MainWindow::refreshJokerSlots);

    connect(mGameState, &GameState::consumablesChanged, this, &MainWindow::refreshConsumableSlots);
    // 牌型升级动画的"上一次状态"快照：先吃掉当前等级，后续 emit 才会算成升级。
    mPrevHandLevels = mGameState->handLevels();
    mHandLevelInitialized = true;
    connect(mGameState, &GameState::handLevelsChanged, this, &MainWindow::onHandLevelsChanged);

    connect(mGameState, &GameState::shopChanged, this, [this]() {
        refreshCounters();
        refreshGold();
        if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
        if (mPackOpenWidget && mPackOpenWidget->isVisible())
            mPackOpenWidget->setFreeJokerSlots(mGameState->jokerSlots() - mGameState->jokers().size());
    });

    connect(mGameState, &GameState::blindSelectEntered,
            this, &MainWindow::onBlindSelectEntered);
    connect(mGameState, &GameState::blindStarted,
            this, &MainWindow::onBlindStarted);
    connect(mBlindSelectWidget, &BlindSelectWidget::selectClicked,
            this, &MainWindow::onSelectBlindClicked);
    connect(mShopWidget, &ShopWidget::leaveClicked,
            this, &MainWindow::onLeaveShopClicked);
    connect(mShopWidget, &ShopWidget::packBuyRequested,
            this, &MainWindow::onPackBuyRequested);
    connect(mShopWidget, &ShopWidget::shopItemBoughtForAnimation,
            this, &MainWindow::prepareSlotFlyInAnimation);
    connect(mShopWidget, &ShopWidget::shopConsumableUseAnimation,
            this, &MainWindow::spawnShopPlanetUseFloater);

    connect(mBlindSelectWidget, &BlindSelectWidget::skipClicked,
            this, &MainWindow::onSkipBlind);
}

void MainWindow::refreshHand() {
    const auto &hand = mGameState->hand();

    auto matches = [](const CardData &a, const CardData &b) {
        if (a.uid > 0 && b.uid > 0) return a.uid == b.uid;
        return a.rank == b.rank && a.suit == b.suit
               && a.enhancement == b.enhancement && a.seal == b.seal
               && a.edition == b.edition;
    };

    QVector<CardData> selectedData;
    for (int i : mSelected)
        if (i >= 0 && i < mHandCards.size())
            selectedData.append(mHandCards[i]->cardData());

    for (int i = mHandCards.size() - 1; i >= 0; --i) {
        const CardData &d = mHandCards[i]->cardData();
        bool found = false;
        for (const auto &hc : hand) if (matches(hc, d)) { found = true; break; }
        if (!found) {
            mScene->removeItem(mHandCards[i]);
            mHandCards[i]->deleteLater();
            mHandCards.removeAt(i);
        }
    }

    QPointF deckPos(mSceneW - CARD_W - 60, mHandYScoring);
    QVector<CardItem*> reordered;
    QVector<CardItem*> remaining = mHandCards;
    for (const auto &hc : hand) {
        // 计分阶段 game.hand 仍包含正打在 play 区的牌（finalizePlayedHand 之前）；
        // 这些牌的 CardItem 在 mPlayedCards 里，hand 行不应该再为它们生成"复制品"也不能把它们拉回来。
        bool ownedByPlayed = false;
        for (auto *pc : mPlayedCards) {
            if (pc && matches(pc->cardData(), hc)) { ownedByPlayed = true; break; }
        }
        if (ownedByPlayed) continue;

        CardItem *match = nullptr;
        for (int k = 0; k < remaining.size(); ++k) {
            if (matches(remaining[k]->cardData(), hc)) {
                match = remaining[k];
                remaining.removeAt(k);
                break;
            }
        }
        if (!match) {
            match = new CardItem(hc);
            match->setPos(deckPos);
            match->setZValue(10);
            mScene->addItem(match);
            connect(match, &CardItem::clicked,
                    this, &MainWindow::onCardClicked);
            connect(match, &CardItem::dragMoved,
                    this, &MainWindow::onHandCardDragMoved);
            connect(match, &CardItem::dragReleased,
                    this, &MainWindow::onHandCardDragReleased);
            connect(match, &CardItem::hoverChanged,
                    this, [this](CardItem *c, bool hovered) {
                        // 与原版 generate_card_ui 一致：任何手牌悬浮都显示信息——
                        // 普通牌也要看"+10 筹码"这种基础描述。
                        if (!hovered) { hideHoverTooltip(); return; }
                        if (c) showCardHoverTooltip(c);
                    });
        } else {
            match->setCardData(hc);
            // 房屋 Boss 出完第一手后，原本背面朝下的剩余手牌翻回正面。
            // 仅在房屋 Boss 下生效，且不能打断消耗牌自身的翻面动画。
            if (!mSuppressHandReveal
                && mGameState->bossEffect() == BossEffect::TheHouse
                && hc.faceUp && !match->cardData().faceUp)
                match->flip();
        }
        reordered.append(match);
    }
    mHandCards = reordered;

    mSelected.clear();
    for (int i = 0; i < mHandCards.size(); ++i) {
        const CardData &d = mHandCards[i]->cardData();
        bool wasSelected = false;
        for (const auto &sd : selectedData) {
            if (matches(sd, d)) { wasSelected = true; break; }
        }
        mHandCards[i]->setCardSelected(wasSelected);
        if (wasSelected) mSelected.append(i);
    }

    // 蔚蓝铃铛 Boss：强制把锁定的手牌设为选中。
    const int forcedUid = mGameState->ceruleanForcedUid();
    if (forcedUid > 0) {
        for (int i = 0; i < mHandCards.size(); ++i) {
            if (mHandCards[i]->cardData().uid == forcedUid
                && !mSelected.contains(i) && mSelected.size() < 5) {
                mHandCards[i]->setCardSelected(true);
                mSelected.append(i);
            }
        }
    }

    layoutHandCards();
    refreshCounters();
    updateHandPreview();
}

void MainWindow::layoutHandCards() {
    int n = mHandCards.size();
    if (n == 0) return;

    int areaW = mSceneW - HAND_RIGHT_RESERVE;       // 手牌可用宽度
    int available = areaW - 80;
    int step = (n > 1) ? (available - CARD_W) / (n - 1) : 0;
    step = qMin(step, CARD_W - 30);
    int totalW = (n - 1) * step + CARD_W;
    int startX = (areaW - totalW) / 2;              // 在手牌区(不含右侧 deck/tag 区)居中

    mHandCountLabel->setPlainText(
        QString("%1/%2").arg(n).arg(mGameState->handSize()));
    QRectF hcr = mHandCountLabel->boundingRect();
    const qreal labelTargetX = areaW / 2.0 - hcr.width() / 2.0;
    // 把 8/8 放在"手牌底沿 → 出牌按钮顶端"的中点，避免贴着按钮看上去拥挤。
    const qreal handBottom = mHandY + CARD_H;
    const qreal labelTargetY = (handBottom + mBtnY) / 2.0 - 14;
    const qreal labelCurY    = mHandCountLabel->pos().y();
    if (std::abs(labelCurY - labelTargetY) < 1.0) {
        // 没有 Y 位移（只是横向重排），直接放置避免空动画。
        mHandCountLabel->setPos(labelTargetX, labelTargetY);
    } else {
        // 手牌整体下沉 / 升起时 8/8 标签跟着同样的 200ms / OutCubic 曲线走，
        // 与手牌、按钮、查看牌组面板节奏一致。
        if (mHandCountLabelAnim) {
            mHandCountLabelAnim->stop();
            mHandCountLabelAnim->deleteLater();
            mHandCountLabelAnim.clear();
        }
        // 横向位置无需动画——直接对齐到目标 X，垂直方向走插值。
        mHandCountLabel->setPos(labelTargetX, labelCurY);
        auto *anim = new QVariantAnimation(this);
        mHandCountLabelAnim = anim;
        anim->setDuration(200);
        anim->setStartValue(labelCurY);
        anim->setEndValue(labelTargetY);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QVariantAnimation::valueChanged, this,
                [this, labelTargetX](const QVariant &v) {
                    if (mHandCountLabel) mHandCountLabel->setPos(labelTargetX, v.toReal());
                });
        anim->start();
    }

    for (int i = 0; i < n; ++i) {
        bool sel = mSelected.contains(i);
        double t = (-n / 2.0 - 0.5 + (i + 1)) / n;
        double angleDeg = 0.2 * t * 180.0 / M_PI;
        int x = startX + i * step;
        // 选中上提量按 CARD_H 比例（≈26%），卡牌放大后这里同步加大才不会"点了感觉没动"。
        int y = mHandY + (sel ? -CARD_H * 26 / 100 : 0);
        mHandCards[i]->setBaseRotation(angleDeg);
        mHandCards[i]->setZValue(i);
        // 200ms 选中弹起 / 折回。之前 140ms 显得过于"snap"，恢复到放大卡牌之前 220ms 附近的手感。
        mHandCards[i]->moveTo(QPointF(x, y), 200);
    }
}

void MainWindow::clearPlayedCards() {
    for (auto *c : mPlayedCards) {
        mScene->removeItem(c);
        delete c;
    }
    mPlayedCards.clear();
}

void MainWindow::layoutPlayedCards() {
    int n = mPlayedCards.size();
    if (n == 0) return;

    int areaW = mSceneW - HAND_RIGHT_RESERVE;
    int totalW = n * CARD_W + (n - 1) * 10;
    int startX = (areaW - totalW) / 2;
    int y = PLAY_Y + (PLAY_H - CARD_H) / 2;

    for (int i = 0; i < n; ++i) {
        mPlayedCards[i]->setBaseRotation(0);
        mPlayedCards[i]->setPos(startX + i * (CARD_W + 10), y);
    }
}

void MainWindow::refreshScore() {
    if (mGameState->phase() == GamePhase::Shop) {
        if (mScoreCountAnim) {
            mScoreCountAnim->stop();
            mScoreCountAnim->deleteLater();
            mScoreCountAnim = nullptr;
        }
        if (mScoreProgressAnim && mScoreProgressAnim->state() == QAbstractAnimation::Running)
            mScoreProgressAnim->stop();
        setLabelScaledText(mLblScore, "0", uiPx(38));
        mLblTarget->setText(QString::fromUtf8("\342\234\223" "0"));
        updateScoreProgressBar(0.0, false);
        return;
    }

    const double score = mGameState->score();
    setLabelScaledText(mLblScore, formatScoreNumber(score), uiPx(38));
    mLblTarget->setText(formatScoreNumber(mGameState->targetScore()));
    updateScoreProgressBar(score, true);
}

void MainWindow::updateScoreProgressBar(double displayedScore, bool animate)
{
    if (!mScoreProgressBar || !mGameState) return;

    const double target = mGameState->targetScore();
    double ratio = 0.0;
    if (target > 0.0 && std::isfinite(displayedScore)) {
        ratio = displayedScore / target;
    } else if (target > 0.0 && std::isinf(displayedScore)) {
        ratio = 1.0;
    }
    ratio = std::max(0.0, ratio);

    const int barValue = qBound(0, int(std::round(std::min(ratio, 1.0) * 1000.0)), 1000);
    const int percent = qBound(0, int(std::round(std::min(ratio, 1.0) * 100.0)), 100);
    mScoreProgressBar->setFormat(QString("%1%").arg(percent));

    const QString border = barValue >= 1000 ? "#ffb000" : "#27566b";
    const QString chunkEnd = barValue >= 1000 ? "#ffdf68" : "#fda200";
    mScoreProgressBar->setStyleSheet(QString(
        "QProgressBar {"
        " background:rgba(8,18,24,112);"
        " border:3px solid %1;"
        " border-radius:14px;"
        " color:#eaffff;"
        " text-align:center;"
        " padding:2px;"
        "}"
        "QProgressBar::chunk {"
        " border-radius:11px;"
        " background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #009dff, stop:0.58 #23e6ff, stop:1 %2);"
        "}"
        ).arg(border, chunkEnd));

    if (!animate) {
        mScoreProgressBar->setValue(barValue);
        return;
    }

    if (!mScoreProgressAnim) {
        mScoreProgressAnim = new QPropertyAnimation(mScoreProgressBar, "value", this);
        mScoreProgressAnim->setDuration(420);
        mScoreProgressAnim->setEasingCurve(QEasingCurve::OutCubic);
    }
    if (mScoreProgressAnim->state() == QAbstractAnimation::Running)
        mScoreProgressAnim->stop();
    mScoreProgressAnim->setStartValue(mScoreProgressBar->value());
    mScoreProgressAnim->setEndValue(barValue);
    mScoreProgressAnim->start();
}

void MainWindow::refreshGold() {
    mLblGold->setText(QString("$%1").arg(mGameState->gold()));
}

void MainWindow::refreshCounters() {
    mLblHands->setText(QString::number(mGameState->handsLeft()));
    mLblDiscards->setText(QString::number(mGameState->discardLeft()));
    mLblAnte->setText(QString("%1<font color='white'>/8</font>")
                          .arg(mGameState->ante()));
    mLblRound->setText(QString::number(
        (mGameState->ante() - 1) * 3 + mGameState->blindIdx() + 1));

    auto applyBlindStyle = [this](const QString &color) {
        mLblBlind->setStyleSheet(QString(
            "color:white; background:%1;"
            "border-top-left-radius:10px; border-top-right-radius:10px;"
            "padding:6px 10px;")
                                     .arg(color));
    };
    switch (mGameState->blindType()) {
    case BlindType::Small:
        mLblBlind->setText("小盲注");
        applyBlindStyle("#1679b4");
        break;
    case BlindType::Big:
        mLblBlind->setText("大盲注");
        applyBlindStyle("#ae7b1b");
        break;
    case BlindType::Boss: {
        auto info = mGameState->currentBossInfo();
        mLblBlind->setText(QString("Boss · %1").arg(info.name));
        QString col;
        switch (mGameState->bossEffect()) {
        case BossEffect::TheHook:   col = "#a84024"; break;
        case BossEffect::TheClub:   col = "#b9cb92"; break;
        case BossEffect::TheWall:   col = "#8a59a5"; break;
        case BossEffect::ThePlant:  col = "#709284"; break;
        case BossEffect::TheNeedle: col = "#5c6e31"; break;
        default:                    col = "#a84024"; break;
        }
        applyBlindStyle(col);
        mLblBlind->setToolTip(info.description);
        break;
    }
    }

    if (mCtxBlindChipImg) {
        int row = 0;
        switch (mGameState->blindType()) {
        case BlindType::Small: row = 0; break;
        case BlindType::Big:   row = 1; break;
        case BlindType::Boss:  row = bossChipRow(mGameState->bossEffect()); break;
        }
        mCtxBlindChipImg->setBlindRow(row);
    }

    if (mBlindChipLbl) {
        QPixmap sheet(":/textures/images/BlindChips.png");
    }

    bool hasSelected = !mSelected.isEmpty();
    mBtnPlay->setEnabled(mGameState->handsLeft() > 0 && hasSelected);
    mBtnDiscard->setEnabled(mGameState->discardLeft() > 0 && hasSelected);
    // 占卜:仅在有选中手牌且未在动画中时可用。
    if (mBtnForesight)
        mBtnForesight->setEnabled(hasSelected && !mForesightPreviewActive);

    if (mDeckLabel) {
        // 显示 "剩余/总数"，与原版保持一致。
        mDeckLabel->setPlainText(
            QString("%1/%2").arg(formatScoreNumber(mGameState->deckRemaining()))
                            .arg(formatScoreNumber(mGameState->deckTotal())));
        QRectF br = mDeckLabel->boundingRect();
        mDeckLabel->setPos(mSceneW - CARD_W - 60 + (CARD_W - br.width()) / 2.0,
                           mHandYScoring + CARD_H + 6);
    }
    if (mJokerCountLabel) {
        mJokerCountLabel->setPlainText(QString("%1/%2")
                                           .arg(mGameState->jokers().size()).arg(mGameState->jokerSlots()));
        QRectF br = mJokerCountLabel->boundingRect();
        mJokerCountLabel->setPos(40, JOKER_Y + TOP_SLOT_H + 40);
    }
    if (mConsCountLabel) {
        QRectF br = mConsCountLabel->boundingRect();
        int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
        int totalW = TOP_SLOT_W + qMax(0, visualSlots - 1) * (TOP_SLOT_W + 14);
        int startX = mSceneW - 40 - totalW;
        mConsCountLabel->setPos(startX + totalW - br.width() - 2, JOKER_Y + TOP_SLOT_H + 40);
    }
}

void MainWindow::onDeckClicked(CardItem *)
{
    if (!mDeckViewWidget || !mPlayPage) return;
    AudioManager::instance()->play(QStringLiteral("cardFan2"), audioPitchJitter(0.03), 0.65);
    // 标记必须在 open() 之前置位：show() 一瞬间 QGraphicsScene 会把 hoverLeaveEvent
    // 投给牌堆 CardItem；如果不屏蔽，按钮 / 面板会立刻"收回再展开"，造成反复滑动。
    mDeckViewOpen = true;
    // 打开牌组前暂停局内进程；必须早于 open() 内部的滑入动画创建。
    pauseGameProcesses();
    mDeckViewWidget->setGeometry(mPlayPage->rect());
    mDeckViewWidget->open(mGameState->remainingDeckCards(), mGameState->fullDeckCards());
}

void MainWindow::onDeckHoverChanged(CardItem *card, bool hovered)
{
    Q_UNUSED(card);
    // "查看牌组"对话框打开期间忽略所有牌堆 hover：
    // 否则 Qt 在弹窗 show()/close() 瞬间投出的 leave/enter 会让窥探面板反复开合。
    if (mDeckViewOpen) return;
    if (hovered) {
        // CardItem::setStrictHoverShape(true) 已经把 hit-test 收紧到牌面，无需再做额外 rect 检查。
        updateDeckStatsPopup();
        if (mGameState && mGameState->phase() == GamePhase::Blind) {
            showDeckPeekPanel();
        }
    } else {
        if (mDeckStatsItem) mDeckStatsItem->setVisible(false);
        if (mDeckPeekDeployed) hideDeckPeekPanel();
    }
}

void MainWindow::updateDeckStatsPopup()
{
    // 牌堆卡片中央的"查看牌组"小提示——任意阶段都只显示一行文字。
    if (!mDeckBackCard || !mScene) return;

    constexpr int popupW = 140;
    constexpr int popupH = 68;     // 两行字需要更高一些

    if (mDeckStatsItem) {
        mScene->removeItem(mDeckStatsItem);
        delete mDeckStatsItem;
        mDeckStatsItem = nullptr;
    }
    mDeckStatsItem = mScene->addRect(QRectF(0, 0, popupW, popupH),
                                      QPen(QColor(255, 255, 255, 220), 2),
                                      QBrush(QColor(20, 28, 32, 235)));
    mDeckStatsItem->setZValue(900);
    mDeckStatsItem->setAcceptHoverEvents(false);
    mDeckStatsItem->setAcceptedMouseButtons(Qt::NoButton);

    auto *t = new QGraphicsTextItem(mDeckStatsItem);
    QFont f = mCNFont; f.setPixelSize(20); f.setBold(true);
    t->setFont(f);
    t->setDefaultTextColor(Qt::white);
    // 两行显示："查看 / 牌组"。
    t->setHtml(QStringLiteral("<div align='center'>查看<br/>牌组</div>"));
    t->setTextWidth(popupW);
    t->setAcceptHoverEvents(false);
    t->setAcceptedMouseButtons(Qt::NoButton);
    const qreal yOff = (popupH - t->boundingRect().height()) / 2.0;
    t->setPos(0, yOff);

    const QPointF deckPos = mDeckBackCard->pos();
    mDeckStatsItem->setPos(deckPos.x() + (CARD_W - popupW) / 2.0,
                           deckPos.y() + (CARD_H - popupH) / 2.0);
    mDeckStatsItem->setVisible(true);
}

void MainWindow::buildDeckPeekPanel()
{
    if (mDeckPeekPanel) return;

    // 13 个 rank (A K Q J 10 ... 2) × 4 个 suit (S H D C)，每个 rank 一列。
    constexpr int colW = 78;
    constexpr int rowH = 38;
    constexpr int leftW = 110;
    constexpr int rankCount = 13;
    constexpr int suitCount = 4;
    const int panelW = leftW + rankCount * colW + 24;
    const int panelH = (1 + suitCount) * rowH + 24;

    mDeckPeekPanel = mScene->addRect(QRectF(0, 0, panelW, panelH),
                                      QPen(QColor(255, 255, 255, 220), 3),
                                      QBrush(QColor(20, 28, 32, 240)));
    mDeckPeekPanel->setZValue(950);
    mDeckPeekPanel->setVisible(false);
    // 同样不抢 hover；否则面板滑下来盖到牌堆上时也会触发反复开合。
    mDeckPeekPanel->setAcceptHoverEvents(false);
    mDeckPeekPanel->setAcceptedMouseButtons(Qt::NoButton);
}

void MainWindow::showDeckPeekPanel()
{
    if (!mScene || !mGameState) return;
    if (!mDeckPeekPanel) buildDeckPeekPanel();

    // 仅按 remainingDeckCards 统计 ——即"牌堆里还剩多少"，
    // 不再把当前手牌加进来（手牌不在牌堆里，玩家想看的是剩余牌的成分）。
    const QVector<CardData> remaining = mGameState->remainingDeckCards();
    constexpr int rankCount = 13;
    constexpr int suitCount = 4;
    int grid[suitCount][rankCount] = {};
    int suitTot[suitCount] = {0,0,0,0};
    int rankTot[rankCount] = {0};

    auto suitIdx = [](Suit s) {
        switch (s) {
        case Suit::Spades: return 0;
        case Suit::Hearts: return 1;
        case Suit::Clubs: return 2;
        case Suit::Diamonds: return 3;
        }
        return 0;
    };
    auto addCard = [&](const CardData &c) {
        if (c.enhancement == Enhancement::Stone) return;
        int si = suitIdx(c.suit);
        int ri = int(c.rank) - 2;
        if (ri < 0 || ri >= rankCount) return;
        ++grid[si][ri]; ++suitTot[si]; ++rankTot[ri];
    };
    for (const auto &c : remaining) addCard(c);

    // 清掉旧的文字子项再重绘。
    for (auto *it : mDeckPeekPanel->childItems()) {
        mScene->removeItem(it);
        delete it;
    }

    static const char *rankLabels[rankCount] = {"A","K","Q","J","10","9","8","7","6","5","4","3","2"};
    static const int   rankOrder [rankCount] = {12,11,10,9,8,7,6,5,4,3,2,1,0};
    static const char *suitGlyph [suitCount] = {"♠","♥","♣","♦"};
    static const char *suitColor [suitCount] = {"#d8d8d8","#ff5d5d","#7ec8ff","#ffb95a"};

    constexpr int colW = 78;
    constexpr int rowH = 38;
    constexpr int leftW = 110;

    auto addText = [&](const QString &txt, qreal x, qreal y, qreal w, qreal h,
                       const QColor &color, int px, bool bold, bool cn) {
        auto *t = new QGraphicsTextItem(mDeckPeekPanel);
        QFont f = cn ? mCNFont : mPixelFont;
        f.setPixelSize(px);
        f.setBold(bold);
        t->setFont(f);
        t->setDefaultTextColor(color);
        t->setHtml(QStringLiteral("<div align='center'>%1</div>").arg(txt.toHtmlEscaped()));
        t->setTextWidth(w);
        t->setAcceptHoverEvents(false);
        t->setAcceptedMouseButtons(Qt::NoButton);
        qreal yOff = (h - t->boundingRect().height()) / 2.0;
        t->setPos(x, y + yOff);
    };

    const qreal headerY = 10;
    for (int c = 0; c < rankCount; ++c) {
        int r = rankOrder[c];
        qreal x = leftW + c * colW;
        addText(QString::fromLatin1(rankLabels[c]), x, headerY, colW, rowH,
                QColor("#f7f7f7"), 22, true, false);
    }
    addText("总计", 12, headerY, leftW - 16, rowH, QColor("#f7f7f7"), 18, true, true);

    for (int s = 0; s < suitCount; ++s) {
        qreal rowY = headerY + (1 + s) * rowH + 4;
        QColor sc(suitColor[s]);
        addText(QString::fromUtf8(suitGlyph[s]) + "  " + QString::number(suitTot[s]),
                12, rowY, leftW - 16, rowH, sc, 18, true, true);
        for (int c = 0; c < rankCount; ++c) {
            int r = rankOrder[c];
            qreal x = leftW + c * colW;
            int v = grid[s][r];
            QColor col = v > 0 ? sc : QColor("#4a5560");
            addText(QString::number(v), x, rowY, colW, rowH, col, 20, true, false);
        }
    }

    // 把面板水平居中在 (mSceneW - HAND_RIGHT_RESERVE) 内，避免遮住右下角牌堆。
    // 垂直位置位于小丑区下方、"下沉后的手牌"上方的正中间——
    // 因为下面紧接着会把出牌按钮滑出、手牌下移到 mHandYScoring，所以这里用 scoring 位置。
    QRectF br = mDeckPeekPanel->rect();
    const qreal availW = mSceneW - HAND_RIGHT_RESERVE;
    const qreal panelX = qMax<qreal>(20, (availW - br.width()) / 2.0);
    const qreal jokerBottom = JOKER_Y + JOKER_H;
    const qreal handTopDropped = mHandYScoring;
    qreal panelEndY = (jokerBottom + handTopDropped) / 2.0 - br.height() / 2.0;
    panelEndY = qBound<qreal>(jokerBottom + 12, panelEndY, handTopDropped - br.height() - 12);
    // 与"出牌按钮下滑"完全同节奏：滑入距离 160、时长 280ms。
    // 起点不再贴屏幕顶端，而是终点上方 160px 处出现，下滑到 panelEndY。
    const qreal panelStartY = panelEndY - 160.0;
    mDeckPeekPanel->setPos(panelX, panelStartY);
    mDeckPeekPanel->setVisible(true);

    // 滑入动画。先把上一段动画安全停掉——之前用 DeleteWhenStopped + 裸指针残留导致
    // 第二次访问已删除的 QVariantAnimation 直接闪退。
    if (mDeckPeekAnim) {
        mDeckPeekAnim->stop();
        mDeckPeekAnim->deleteLater();
        mDeckPeekAnim.clear();
    }
    auto *anim = new QVariantAnimation(this);
    mDeckPeekAnim = anim;
    // 与手牌 moveTo()、按钮 slideOut() 完全一致：200ms / OutCubic。
    anim->setDuration(200);
    anim->setStartValue(panelStartY);
    anim->setEndValue(panelEndY);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this,
            [this, panelX](const QVariant &v) {
                if (mDeckPeekPanel) mDeckPeekPanel->setPos(panelX, v.toReal());
            });
    // 不再使用 DeleteWhenStopped；动画对象的父对象是 this，正常被 owner 销毁。
    anim->start();

    // 与原版打出手牌时的"按钮整组下滑出屏 + 手牌下移"对齐：
    // hover 牌堆相当于"先窥探一下牌组"，按相同节奏挪让出空间。
    // 牌堆按钮本身已经常驻 mHandYScoring，不需要再动。
    if (!mScoringInProgress) {
        hidePlayControlsForScoring();
        if (mHandY != mHandYScoring) {
            mHandY = mHandYScoring;
            layoutHandCards();
        }
    }
    mDeckPeekDeployed = true;
}

void MainWindow::hideDeckPeekPanel()
{
    if (!mDeckPeekDeployed) return;

    if (mDeckPeekAnim) {
        mDeckPeekAnim->stop();
        mDeckPeekAnim->deleteLater();
        mDeckPeekAnim.clear();
    }
    if (mDeckPeekPanel) {
        const qreal curY = mDeckPeekPanel->y();
        // 收回路径与滑入对称：上滑 160px，对齐按钮"飞回"节奏。
        const qreal endY = curY - 160.0;
        const qreal curX = mDeckPeekPanel->x();
        auto *anim = new QVariantAnimation(this);
        mDeckPeekAnim = anim;
        // 与按钮回位、手牌回升一致：200ms / OutCubic。
        anim->setDuration(200);
        anim->setStartValue(curY);
        anim->setEndValue(endY);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QVariantAnimation::valueChanged, this,
                [this, curX](const QVariant &v) {
                    if (mDeckPeekPanel) mDeckPeekPanel->setPos(curX, v.toReal());
                });
        QPointer<MainWindow> guard(this);
        connect(anim, &QVariantAnimation::finished, this, [guard]() {
            if (guard && guard->mDeckPeekPanel) guard->mDeckPeekPanel->setVisible(false);
        });
        anim->start();
    }

    // 还原按钮位置与手牌行高度。计分进行中时按钮和手牌应保持下沉，不要由 hover 还原。
    if (!mScoringInProgress) {
        showPlayControlsAfterScoring();
        if (mHandY != mHandYNormal) {
            mHandY = mHandYNormal;
            layoutHandCards();
        }
    }
    mDeckPeekDeployed = false;
}

void MainWindow::layoutHandWithDeckPeek(bool peeking)
{
    // 临时把手牌下移 (peek 时下沉 ~80px，露出按钮位置给统计表)；
    // peek 结束恢复到 mHandYNormal。
    const int targetY = peeking ? (mSceneH - CARD_H - 40) : mHandYNormal;
    if (targetY == mHandY) return;
    mHandY = targetY;
    layoutHandCards();
}

void MainWindow::onCardClicked(CardItem *card) {
    int idx = mHandCards.indexOf(card);
    if (idx < 0) return;

    // 蔚蓝铃铛 Boss：被锁定的手牌不能取消选中。
    const bool isForced = mGameState->ceruleanForcedUid() > 0
                          && card->cardData().uid == mGameState->ceruleanForcedUid();

    const bool wasSelected = mSelected.contains(idx);
    if (wasSelected) {
        if (isForced) return;
        mSelected.removeAll(idx);
        card->setCardSelected(false);
    } else {
        if (mSelected.size() < 5) {
            mSelected.append(idx);
            card->setCardSelected(true);
        }
    }

    AudioManager::instance()->play(wasSelected ? QStringLiteral("cardSlide2")
                                               : QStringLiteral("cardSlide1"),
                                   1.0, wasSelected ? 0.3 : 1.0);
    layoutHandCards();
    refreshCounters();
    updateHandPreview();
    refreshConsumableUseButtonState();   // 选牌数量变化 → 重新评估"使用"按钮可用性
    // 选中/取消选中后手牌会上移 / 下落 200ms。期间多刷几次让信息框跟着走，
    // 否则用户能看见信息框停在旧位置或与新位置不同步。
    QPointer<CardItem> cardGuard(card);
    auto repos = [this, cardGuard]() { if (cardGuard) repositionHoverIfFollowingCard(cardGuard); };
    repos();
    QTimer::singleShot(100, this, repos);
    QTimer::singleShot(210, this, repos);
}


void MainWindow::showCardInfo(CardItem *card)
{
    if (!card) return;
    const CardData &d = card->cardData();

    if (!mCardInfoPanel) {
        mCardInfoPanel = new QWidget;
        mCardInfoPanel->setAttribute(Qt::WA_StyledBackground, true);
        mCardInfoPanel->setStyleSheet(
            "background:#f5fbf5;"
            "border:3px solid #223034;"
            "border-radius:8px;"
            );
        auto *infoGlow = new QGraphicsDropShadowEffect(mCardInfoPanel);
        infoGlow->setBlurRadius(14);
        infoGlow->setOffset(0, 4);
        infoGlow->setColor(QColor(0, 0, 0, 135));
        mCardInfoPanel->setGraphicsEffect(infoGlow);
        auto *v = new QVBoxLayout(mCardInfoPanel);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);

        mCardInfoName = new QLabel(mCardInfoPanel);
        QFont nf = mCNFont; nf.setPixelSize(uiPx(22)); nf.setBold(true);
        mCardInfoName->setFont(nf);
        mCardInfoName->setStyleSheet("color:#243235; background:#f8fff8; border:none; border-bottom:2px solid #223034; padding:4px 8px;");
        mCardInfoName->setAlignment(Qt::AlignCenter);
        v->addWidget(mCardInfoName);

        mCardInfoDesc = new QLabel(mCardInfoPanel);
        QFont df = mCNFont; df.setPixelSize(uiPx(18)); df.setBold(true);
        mCardInfoDesc->setFont(df);
        mCardInfoDesc->setStyleSheet("color:#2a86c8; background:#f8fff8; border:none; padding:5px 8px;");
        mCardInfoDesc->setWordWrap(true);
        mCardInfoDesc->setAlignment(Qt::AlignCenter);
        v->addWidget(mCardInfoDesc);

        mCardInfoPanel->setFixedWidth(150);
        mCardInfoProxy = mScene->addWidget(mCardInfoPanel);
        mCardInfoProxy->setZValue(900);
    }

    mCardInfoName->setText(cardTooltipTitle(d));
    mCardInfoDesc->setText(cardTooltipBody(d));
    const int preferredTooltipW = (mCardInfoDesc->text().contains('\n') || mCardInfoName->text().size() > 5) ? 210 : 150;
    mCardInfoPanel->setFixedWidth(preferredTooltipW);
    mCardInfoPanel->adjustSize();

    QPointF p(card->scenePos().x() + CardItem::WIDTH / 2.0 - mCardInfoPanel->width() / 2.0,
              card->scenePos().y() - mCardInfoPanel->height() - 12);
    if (p.x() < 8) p.setX(8);
    if (p.x() + mCardInfoPanel->width() > mSceneW - 8)
        p.setX(mSceneW - mCardInfoPanel->width() - 8);
    if (p.y() < 8)
        p.setY(card->scenePos().y() + CardItem::HEIGHT + 10);
    mCardInfoProxy->setPos(p);
    mCardInfoProxy->show();
}

void MainWindow::hideCardInfo()
{
    if (mCardInfoProxy) mCardInfoProxy->hide();
}

// ──────────────────────────────────────────────────────────────
// 统一的悬浮描述：替代旧的 mCardInfoPanel / mJokerInfoPanel(hover)，
// 风格按 BalatroInfoPanel 实现，对齐原版 generate_card_ui 配色。
// ──────────────────────────────────────────────────────────────
void MainWindow::ensureHoverTooltip()
{
    if (mHoverTooltip) return;
    // 直接作为 mPlayPage 子 widget——绕过 QGraphicsProxyWidget 在带 drop-shadow effect 时的
    // 渲染坑（之前主场景 hover 一直不显示，根因就是这个）。
    mHoverTooltip = new BalatroInfoCluster(mCNFont, mPlayPage ? mPlayPage : this);
    mHoverTooltip->hide();
    mHoverTooltip->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void MainWindow::showHoverTooltipNearScene(QGraphicsObject *anchor, double anchorWidth)
{
    if (!mHoverTooltip || !anchor || !mView) return;
    mHoverTooltip->adjustSize();

    // 场景坐标 → mView viewport → mPlayPage widget 坐标。
    const QPointF sceneTopLeft = anchor->scenePos();
    QPoint viewPt = mView->mapFromScene(sceneTopLeft);
    QWidget *parent = mHoverTooltip->parentWidget();
    QPoint pageTopLeft = parent ? mView->mapTo(parent, viewPt) : viewPt;

    // 锚点宽度也要从场景坐标换算到 widget 像素。
    double sx = mView->transform().m11();
    if (sx <= 0.0) sx = 1.0;
    const int anchorPxW = qMax(1, int(anchorWidth * sx));
    const int anchorPxH = qMax(1, int(anchor->boundingRect().height() * sx));

    int x = pageTopLeft.x() + (anchorPxW - mHoverTooltip->width()) / 2;
    int y = pageTopLeft.y() - mHoverTooltip->height() - 8;
    const int maxX = parent ? parent->width()  - mHoverTooltip->width()  - 6 : x;
    const int maxY = parent ? parent->height() - mHoverTooltip->height() - 6 : y;
    if (x < 6) x = 6;
    if (x > maxX) x = qMax(6, maxX);
    if (y < 6) y = pageTopLeft.y() + anchorPxH + 8;
    if (y > maxY) y = qMax(6, maxY);

    mHoverTooltip->move(x, y);
    mHoverTooltip->raise();
    mHoverTooltip->show();
}

void MainWindow::showCardHoverTooltip(CardItem *card)
{
    if (!card) return;
    ensureHoverTooltip();
    const CardData &c = card->cardData();

    // 手牌 info：所有效果（基础筹码 + 增强 + edition + 蜡封）在同一只面板的中间文字栏内联，
    // 不并排副面板——副面板只用于 "塔罗/幻灵 等会授予增强" 这种引用场景。
    mHoverTooltip->clear();
    QVector<BalatroInfoPanel::Badge> mainBadges;
    if (c.isDebuffed)
        mainBadges.append({QStringLiteral("被禁用"), QColor("#9b3a3a")});
    mHoverTooltip->setMainContent(BalatroTooltip::cardTitleHtml(c),
                                  BalatroTooltip::cardBodyHtml(c),
                                  mainBadges, 160, /*nameHasWhiteBox=*/true);
    mHoverTooltip->relayout();
    showHoverTooltipNearScene(card, CardItem::WIDTH);
    mHoveredCard = card;
}

void MainWindow::repositionHoverIfFollowingCard(CardItem *card)
{
    // 选中/取消选中导致手牌位置位移时被调用。只有当前 hover 的就是这张牌、
    // 信息框仍在显示，才重新贴一次坐标。
    if (!card || !mHoverTooltip || !mHoveredCard) return;
    if (mHoveredCard.data() != card) return;
    if (!mHoverTooltip->isVisible()) return;
    showHoverTooltipNearScene(card, CardItem::WIDTH);
}

QString MainWindow::jokerRuntimeStateSuffix(int idx) const
{
    const auto &js = mGameState->jokers();
    if (idx < 0 || idx >= js.size()) return QString();
    const Joker &j = js[idx];
    const int c = qMax(0, j.counter);
    auto suitName = [](Suit s) -> QString {
        switch (s) {
        case Suit::Spades:   return QStringLiteral("{C:spades}黑桃");
        case Suit::Hearts:   return QStringLiteral("{C:hearts}红桃");
        case Suit::Diamonds: return QStringLiteral("{C:diamonds}方块");
        case Suit::Clubs:    return QStringLiteral("{C:clubs}梅花");
        }
        return QString();
    };
    auto rankName = [](Rank r) -> QString {
        switch (r) {
        case Rank::Jack:  return QStringLiteral("J");
        case Rank::Queen: return QStringLiteral("Q");
        case Rank::King:  return QStringLiteral("K");
        case Rank::Ace:   return QStringLiteral("A");
        default: return QString::number(int(r));
        }
    };
    switch (j.type) {
    // ── 传奇 / 已有 ─────────────────────────────────────────────────
    case JokerType::Yorick:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率，还需弃 {C:attention}%2/23")
            .arg(mGameState->yorickXMult(), 0, 'f', 1)
            .arg(mGameState->yorickDiscardsRemaining());
    case JokerType::Caino:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
            .arg(mGameState->cainoXMult(), 0, 'f', 1);
    case JokerType::DriversLicense: {
        int enhanced = 0;
        for (const CardData &c2 : mGameState->fullDeckCards())
            if (c2.enhancement != Enhancement::None) ++enhanced;
        return QString("\n{C:inactive}当前增强牌：{C:attention}%1/16{} %2")
            .arg(enhanced)
            .arg(enhanced >= 16 ? QStringLiteral("{X:mult,C:white}X3{} 已生效")
                                 : QStringLiteral("{C:inactive}未生效"));
    }
    case JokerType::IceCream:
        return QString("\n{C:inactive}当前：{C:chips}+%1{} 筹码").arg(c);
    case JokerType::Stuntman:
        return QStringLiteral("\n{C:inactive}当前：{C:chips}+250{} 筹码 / 手牌上限 {C:red}-2");
    case JokerType::DNA:
        return mGameState->dnaCanTriggerThisPlay()
                   ? QStringLiteral("\n{C:attention}本次出 1 张可触发")
                   : QStringLiteral("\n{C:inactive}仅本盲注第一次且只出 1 张时触发");
    case JokerType::Blueprint:
        return (idx + 1 < js.size())
                   ? QString("\n{C:inactive}指向右侧：{C:attention}%1").arg(js[idx + 1].name)
                   : QStringLiteral("\n{C:inactive}右侧没有可复制小丑");
    case JokerType::Brainstorm:
        return (!js.isEmpty() && idx != 0)
                   ? QString("\n{C:inactive}指向最左：{C:attention}%1").arg(js.first().name)
                   : QStringLiteral("\n{C:inactive}没有可复制小丑");

    // ── 计数器型：每次触发累积；counter 为当前累积值 ────────────────
    case JokerType::SquareJoker:
        return QString("\n{C:inactive}当前：{C:chips}+%1{} 筹码").arg(c);
    case JokerType::Runner:
        return QString("\n{C:inactive}当前：{C:chips}+%1{} 筹码").arg(c);
    case JokerType::Castle:
        return QString("\n{C:inactive}当前花色：%1{}　{C:chips}+%2{} 筹码")
            .arg(suitName(mGameState->castleSuit())).arg(c);
    case JokerType::WeeJoker:
        return QString("\n{C:inactive}当前：{C:chips}+%1{} 筹码").arg(c);

    case JokerType::GreenJoker:
        return QString("\n{C:inactive}当前：{C:mult}+%1{} 倍率").arg(j.counter);
    case JokerType::RideTheBus:
        return QString("\n{C:inactive}当前：{C:mult}+%1{} 倍率").arg(c);
    case JokerType::SpareTrousers:
        return QString("\n{C:inactive}当前：{C:mult}+%1{} 倍率").arg(c);
    case JokerType::FlashCard:
        return QString("\n{C:inactive}当前：{C:mult}+%1{} 倍率").arg(c);
    case JokerType::FortuneTeller:
        return QString("\n{C:inactive}当前：{C:mult}+%1{} 倍率").arg(c);
    case JokerType::Popcorn:
        return QString("\n{C:inactive}剩余：{C:mult}+%1{} 倍率（回合结束 -4）").arg(c);

    case JokerType::Hologram:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
            .arg(1.0 + 0.25 * c, 0, 'f', 2);
    case JokerType::Constellation:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
            .arg(1.0 + 0.1 * c, 0, 'f', 1);
    case JokerType::Vampire:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
            .arg(1.0 + 0.1 * c, 0, 'f', 1);
    case JokerType::Madness:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
            .arg(1.0 + 0.5 * c, 0, 'f', 1);
    case JokerType::LuckyCat:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
            .arg(1.0 + 0.25 * c, 0, 'f', 2);
    case JokerType::Obelisk:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
            .arg(1.0 + 0.2 * c, 0, 'f', 1);
    case JokerType::GlassJoker:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
            .arg(1.0 + 0.75 * c, 0, 'f', 2);
    case JokerType::HitTheRoad:
        return QString("\n{C:inactive}本回合：{X:mult,C:white}X%1{} 倍率")
            .arg(1.0 + 0.5 * c, 0, 'f', 1);
    case JokerType::Throwback:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
            .arg(1.0 + 0.25 * c, 0, 'f', 2);
    case JokerType::CeremonialDagger:
        return QString("\n{C:inactive}当前：{C:mult}+%1{} 倍率").arg(c);
    case JokerType::SteelJoker:
        return QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
            .arg(1.0 + 0.2 * c, 0, 'f', 1);
    case JokerType::StoneJoker:
        return QString("\n{C:inactive}当前：{C:chips}+%1{} 筹码").arg(c);

    case JokerType::LoyaltyCard:
        return QString("\n{C:inactive}状态：%1{}")
            .arg((j.counter % 6 == 5)
                     ? QStringLiteral("{X:mult,C:white}X4{} 倍率（本手生效）")
                     : QString("{C:attention}%1{} 手后生效").arg(5 - (j.counter % 6)));
    case JokerType::Egg:
        return QString("\n{C:inactive}当前售价：{C:money}$%1").arg(j.sellValue);
    case JokerType::GiftCard:
        return QString("\n{C:inactive}已累计 {C:money}$%1{} 售价加成").arg(c);
    case JokerType::ToTheMoon:
        return QString("\n{C:inactive}当前持有金额按 $5 + $%1{} 利息发放")
            .arg(qMin(mGameState->gold() / 5, 5));

    // ── 当回合随机花色 / 点数：从 GameState 读取 ─────────────────
    case JokerType::AncientJoker:
        return QString("\n{C:inactive}本回合花色：%1").arg(suitName(mGameState->ancientSuit()));
    case JokerType::TheIdol:
        return QString("\n{C:inactive}本回合卡牌：{C:attention}%1{} 的 %2")
            .arg(rankName(mGameState->idolRank())).arg(suitName(mGameState->idolSuit()));
    case JokerType::MailInRebate:
        return QString("\n{C:inactive}本回合点数：{C:attention}%1")
            .arg(rankName(mGameState->mailRank()));

    case JokerType::Ramen:
    case JokerType::Seltzer:
    case JokerType::TurtleBean:
        return QString("\n{C:inactive}剩余 {C:attention}%1{} 次").arg(c);

    default:
        return QString();
    }
}

void MainWindow::showJokerHoverTooltip(int idx)
{
    const auto &js = mGameState->jokers();
    if (idx < 0 || idx >= js.size() || idx >= mJokerItems.size()) return;
    ensureHoverTooltip();
    const Joker &j = js[idx];

    QString desc = j.description + jokerRuntimeStateSuffix(idx);

    QVector<BalatroInfoPanel::Badge> badges;
    const JokerRarity rarity = jokerRarity(j.type);
    // 只放一只稀有度 pill——撑满底栏；售价不再放 info（点击小丑会专门弹"售出 $X"按钮）。
    badges.append({BalatroTooltip::rarityName(rarity), BalatroTooltip::rarityColor(rarity)});

    mHoverTooltip->clear();
    // 小丑名字直接是白字落在暗底上（原版 name_from_rows 传 nil），描述才有白盒。
    mHoverTooltip->setMainContent(j.name, BalatroTooltip::fromLuaMarkup(desc), badges, 175,
                                  /*nameHasWhiteBox=*/false);
    // Edition 作为独立副面板，对齐原版"全息 / 多彩"等单独 info 框。
    if (j.edition != Edition::None) {
        BalatroInfoPanel::SideEntry e;
        e.name = BalatroTooltip::editionName(j.edition);
        e.body = BalatroTooltip::editionBodyHtml(j.edition);
        e.badges.append({BalatroTooltip::editionName(j.edition),
                         BalatroInfoPanel::editionPillColor()});
        e.preferredWidth = 130;
        mHoverTooltip->addSidePanel(e);
    }
    mHoverTooltip->relayout();
    showHoverTooltipNearScene(mJokerItems[idx], JokerItem::WIDTH);
}

void MainWindow::showConsumableHoverTooltip(int idx)
{
    const auto &cs = mGameState->consumables();
    if (idx < 0 || idx >= cs.size() || idx >= mConsumableItems.size()) return;
    ensureHoverTooltip();
    const Consumable &c = cs[idx];

    QVector<BalatroInfoPanel::Badge> badges;
    switch (kindOf(c.type)) {
    case ConsumableKind::Tarot:
        badges.append({QStringLiteral("塔罗牌"), BalatroInfoPanel::tarotPillColor()}); break;
    case ConsumableKind::Planet:
        badges.append({QStringLiteral("行星牌"), BalatroInfoPanel::planetPillColor()}); break;
    case ConsumableKind::Spectral:
        badges.append({QStringLiteral("幻灵牌"), BalatroInfoPanel::spectralPillColor()}); break;
    }

    mHoverTooltip->clear();
    // 消耗牌（塔罗/行星/幻灵）：原版 name_from_rows 也传 nil（无白盒），描述才用白盒。
    mHoverTooltip->setMainContent(c.name, BalatroTooltip::fromLuaMarkup(c.description),
                                  badges, 175, /*nameHasWhiteBox=*/false);
    if (c.negative) {
        BalatroInfoPanel::SideEntry e;
        e.name = QStringLiteral("负片");
        e.body = BalatroTooltip::editionBodyHtml(Edition::Negative);
        e.badges.append({QStringLiteral("负片"), BalatroInfoPanel::editionPillColor()});
        e.preferredWidth = 130;
        mHoverTooltip->addSidePanel(e);
    }
    // 如果这张塔罗 / 幻灵会授予一种增强或蜡封，把对应效果拆到副面板——
    // 玩家不切到帮助页就能看到 "Hierophant -> Bonus -> +30 chips" 这种链路。
    if (Enhancement gE = BalatroTooltip::consumableGrantsEnhancement(c.type);
        gE != Enhancement::None) {
        BalatroInfoPanel::SideEntry e;
        e.name = BalatroTooltip::enhancementName(gE);
        e.body = BalatroTooltip::enhancementBodyHtml(gE);
        e.preferredWidth = 140;
        mHoverTooltip->addSidePanel(e);
    }
    if (Seal gS = BalatroTooltip::consumableGrantsSeal(c.type); gS != Seal::None) {
        BalatroInfoPanel::SideEntry e;
        e.name = BalatroTooltip::sealName(gS);
        e.body = BalatroTooltip::sealBodyHtml(gS);
        const int sealKind = (gS == Seal::Gold ? 0 :
                              gS == Seal::Red  ? 1 :
                              gS == Seal::Blue ? 2 : 3);
        e.badges.append({QStringLiteral("蜡封"), BalatroInfoPanel::sealPillColor(sealKind)});
        e.preferredWidth = 130;
        mHoverTooltip->addSidePanel(e);
    }
    if (BalatroTooltip::consumableGrantsRandomEdition(c.type)) {
        BalatroInfoPanel::SideEntry e;
        e.name = QStringLiteral("随机版本");
        e.body = QStringLiteral("闪箔 / 全息 / 多彩 三选一");
        e.preferredWidth = 130;
        mHoverTooltip->addSidePanel(e);
    }
    mHoverTooltip->relayout();
    showHoverTooltipNearScene(mConsumableItems[idx], ConsumableItem::WIDTH);
}

void MainWindow::hideHoverTooltip()
{
    if (mHoverTooltip) mHoverTooltip->hide();
    mHoveredCard.clear();
}


void MainWindow::onHandCardDragMoved(CardItem *card, QPointF scenePos)
{
    int from = mHandCards.indexOf(card);
    if (from < 0) return;
    int n = mHandCards.size();
    if (n <= 1) return;

    int areaW = mSceneW - HAND_RIGHT_RESERVE;
    int available = areaW - 80;
    int step = (available - CARD_W) / qMax(1, n - 1);
    step = qMin(step, CARD_W - 30);
    int totalW = (n - 1) * step + CARD_W;
    int startX = (areaW - totalW) / 2;

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + CARD_W / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (to == mLastHandCardDragTo) {
        card->setZValue(600);
        return;
    }
    mLastHandCardDragTo = to;

    QVector<CardItem*> visual = mHandCards;
    visual.removeAt(from);
    visual.insert(to, card);

    for (int vi = 0; vi < visual.size(); ++vi) {
        CardItem *ci = visual[vi];
        if (ci == card) continue;
        int realIdx = mHandCards.indexOf(ci);
        bool sel = mSelected.contains(realIdx);
        double t = (-n / 2.0 - 0.5 + (vi + 1)) / n;
        double angleDeg = 0.2 * t * 180.0 / M_PI;
        int x = startX + vi * step;
        // 选中上提量按 CARD_H 比例（≈26%），卡牌放大后这里同步加大才不会"点了感觉没动"。
        int y = mHandY + (sel ? -CARD_H * 26 / 100 : 0);
        ci->setBaseRotation(angleDeg);
        ci->setZValue(10 + vi);
        ci->moveTo(QPointF(x, y), 60);
    }
    card->setZValue(600);
}

void MainWindow::onHandCardDragReleased(CardItem *card, QPointF scenePos)
{
    mLastHandCardDragTo = -1;
    int from = mHandCards.indexOf(card);
    if (from < 0) { layoutHandCards(); return; }

    int n = mHandCards.size();
    if (n <= 1) { layoutHandCards(); return; }

    int areaW = mSceneW - HAND_RIGHT_RESERVE;
    int available = areaW - 80;
    int step = (available - CARD_W) / qMax(1, n - 1);
    step = qMin(step, CARD_W - 30);
    int totalW = (n - 1) * step + CARD_W;
    int startX = (areaW - totalW) / 2;

    int to = 0;
    double x = scenePos.x();
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + CARD_W / 2.0;
        if (x > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (from != to) {
        AudioManager::instance()->play(QStringLiteral("cardSlide1"), audioPitchJitter(0.03), 0.62);
        mBestPlayHintActive = false;
        QVector<int> newSelected;
        for (int s : mSelected) {
            int ns = s;
            if (s == from) {
                ns = to;
            } else if (from < to && s > from && s <= to) {
                ns = s - 1;
            } else if (from > to && s >= to && s < from) {
                ns = s + 1;
            }
            if (!newSelected.contains(ns)) newSelected.append(ns);
        }

        // UI 索引（mHandCards）和 game.mHand 索引不一定一致——计分阶段 game.mHand
        // 还包含 played 牌，UI 行只显示未出的那几张。这里用 uid 把 UI 索引转到 game.mHand
        // 的真实位置再调 moveHandCard，否则换位作用在错误的牌上看不到效果。
        const auto &gameHand = mGameState->hand();
        auto findGameIdx = [&gameHand](CardItem *uiCard) -> int {
            if (!uiCard) return -1;
            const CardData &d = uiCard->cardData();
            for (int i = 0; i < gameHand.size(); ++i) {
                if (d.uid > 0 && gameHand[i].uid == d.uid) return i;
            }
            return -1;
        };
        const int fromGame = findGameIdx(mHandCards[from]);
        // to 在 UI 上是目标 slot；用目标 slot 的牌在 game.mHand 的位置作为 moveHandCard 的 to。
        const int toGame = (to >= 0 && to < mHandCards.size())
                               ? findGameIdx(mHandCards[to])
                               : -1;
        if (fromGame >= 0 && toGame >= 0 && fromGame != toGame)
            mGameState->moveHandCard(fromGame, toGame);

        mSelected = newSelected;
        for (int i = 0; i < mHandCards.size(); ++i)
            mHandCards[i]->setCardSelected(mSelected.contains(i));
        layoutHandCards();
    } else {
        layoutHandCards();
    }
    // 双保险：layoutHandCards 已经会让被拖卡飞回它新的槽位，但计分阶段 game.mHand /
    // mHandCards 的索引可能错位 → 这里再显式给被拖卡补一帧 moveTo，
    // 保证"拖到任意位置释放"都会回到手牌行。
    int finalIdx = mHandCards.indexOf(card);
    if (finalIdx >= 0) {
        const int areaW2 = mSceneW - HAND_RIGHT_RESERVE;
        const int n2 = mHandCards.size();
        const int avail2 = areaW2 - 80;
        int step2 = (n2 > 1) ? (avail2 - CARD_W) / (n2 - 1) : 0;
        step2 = qMin(step2, CARD_W - 30);
        const int total2 = (n2 - 1) * step2 + CARD_W;
        const int startX2 = (areaW2 - total2) / 2;
        const int x2 = startX2 + finalIdx * step2;
        const int y2 = mHandY + (mSelected.contains(finalIdx) ? -CARD_H * 26 / 100 : 0);
        card->moveTo(QPointF(x2, y2), 200);
    }
    refreshCounters();
    updateHandPreview();
}

void MainWindow::onPlayClicked() {
    mBestPlayHintActive = false;
    if (mScoringInProgress) return;
    if (mSelected.isEmpty()) return;
    if (mGameState->blindType() == BlindType::Boss
        && !mGameState->hasJokerType(JokerType::Chicot)) {
        if (mGameState->bossEffect() == BossEffect::TheHook) {
            const int hookCount = qMin(2, mGameState->hand().size());
            for (int i = 0; i < hookCount; ++i)
                AudioManager::instance()->play(QStringLiteral("card1"), 1.0, 1.0);
        } else if (mGameState->bossEffect() == BossEffect::TheTooth) {
            for (int i = 0; i < mSelected.size(); ++i)
                playSoundLater(this, 200 + 230 * i, QStringLiteral("coin1"), 1.0, 1.0);
        }
    }
    for (int i = 0; i < mSelected.size(); ++i)
        playOriginalDrawCardSound(this, i, mSelected.size(), false);
    mScoringInProgress = true;
    if (mBtnPlay) mBtnPlay->setEnabled(false);
    if (mBtnDiscard) mBtnDiscard->setEnabled(false);
    if (mBtnForesight) mBtnForesight->setEnabled(false);

    // 出牌:按钮飞出屏幕 + 手牌下移,8/8 标签随手牌一起下移。
    mHandY = mHandYScoring;
    hidePlayControlsForScoring();

    QVector<int> sortedIdx = mSelected;
    std::sort(sortedIdx.begin(), sortedIdx.end());

    mSelected.clear();

    clearPlayedCards();
    QVector<CardItem*> playedCards;
    for (int i = sortedIdx.size() - 1; i >= 0; --i) {
        int idx = sortedIdx[i];
        CardItem *c = mHandCards.takeAt(idx);
        c->setCardSelected(false);
        c->setZValue(500);
        c->setBaseRotation(0);
        if (!c->cardData().faceUp) c->flip();   // 背面朝下的牌被打出时翻开
        playedCards.prepend(c);
    }
    mPlayedCards = playedCards;

    layoutHandCards();

    int n = mPlayedCards.size();
    int areaW = mSceneW - HAND_RIGHT_RESERVE;
    int totalW = n * CARD_W + (n - 1) * 10;
    int startX = (areaW - totalW) / 2;
    int y = PLAY_Y + (PLAY_H - CARD_H) / 2;
    for (int i = 0; i < n; ++i) {
        QPointF target(startX + i * (CARD_W + 10), y);
        mPlayedCards[i]->moveTo(target, 280);
    }

    mGameState->playCards(sortedIdx);
}

void MainWindow::onDiscardClicked() {
    mBestPlayHintActive = false;
    if (mScoringInProgress) return;
    if (mSelected.isEmpty()) return;
    for (int i = 0; i < mSelected.size(); ++i)
        playOriginalDrawCardSound(this, i, mSelected.size(), true);

    QVector<int> sortedIdx = mSelected;
    std::sort(sortedIdx.begin(), sortedIdx.end());
    mSelected.clear();

    for (int i = sortedIdx.size() - 1; i >= 0; --i) {
        int idx = sortedIdx[i];
        CardItem *c = mHandCards.takeAt(idx);
        c->setCardSelected(false);
        c->setZValue(5);
        if (!c->cardData().faceUp) c->flip();   // 背面朝下的牌被弃掉时翻开

        QPointF target(mSceneW + CARD_W, c->pos().y());
        c->moveTo(target, 350);

        auto *fade = new QPropertyAnimation(c, "opacity", this);
        fade->setDuration(350);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        fade->setEasingCurve(QEasingCurve::InQuad);
        connect(fade, &QPropertyAnimation::finished, c, [this, c]() {
            mScene->removeItem(c);
            c->deleteLater();
        });
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    }

    layoutHandCards();
    mGameState->discardCards(sortedIdx);
}

void MainWindow::onHandPlayed()
{
    const HandResult &r = mGameState->lastResult();
    mShatteredPlayedIndices.clear();
    const bool handNameChanged = mLblHandName && (mLblHandName->text() != r.name);

    if (r.name.contains(QStringLiteral("Boss"))) {
        AudioManager::instance()->play(QStringLiteral("whoosh1"), 0.55, 0.62);
        for (int i = 1; i <= 4; ++i) {
            scheduleGame(100 * (i - 1), [i]() {
                AudioManager::instance()->play(QStringLiteral("cancel"), 0.7 + 0.05 * i, 0.7);
            });
        }
    }

    mLblHandName ->setText(r.name);
    mLblHandLevel->setText(QString("等级%1").arg(r.level));
    mLblHandLevel->setStyleSheet(QString("color:%1; background:transparent;").arg(handLevelColor(r.level)));

    resetScoreFlame();
    if (handNameChanged) {
        scheduleGame(400, []() {
            AudioManager::instance()->play(QStringLiteral("button"), 1.0, 0.4);
        });
    }

    mDisplayedChips = r.baseChips;
    mDisplayedMult  = r.baseMult;
    setLabelScaledText(mLblChips, formatScoreNumber(mDisplayedChips), uiPx(42));
    setLabelScaledText(mLblMult,  formatScoreNumber(mDisplayedMult),  uiPx(42));
    updateFlameIntensity();

    double gained = r.chips * r.mult * r.xmult;
    if (!std::isfinite(gained)) gained = std::numeric_limits<double>::max();

    // ── 提取参与计分的 played 区卡片下标(去重,按 x 排序,对应原版 table.sort by T.x) ──
    QVector<int> scoringIndices;
    QSet<int> seen;
    for (const ScoreEvent &ev : r.events) {
        if (ev.sourceCardIdx >= 0 && !seen.contains(ev.sourceCardIdx)) {
            seen.insert(ev.sourceCardIdx);
            scoringIndices.append(ev.sourceCardIdx);
        }
    }
    std::sort(scoringIndices.begin(), scoringIndices.end(),
              [this](int a, int b) {
                  if (a < 0 || a >= mPlayedCards.size()) return false;
                  if (b < 0 || b >= mPlayedCards.size()) return true;
                  return mPlayedCards[a]->x() < mPlayedCards[b]->x();
              });

    // ── 时序参数 ──
    // 1) onPlayClicked 内 5 张牌 moveTo play 区花了 280ms
    // 2) 卡到位后, 计分卡 staggered highlight 上升 (对应原版 highlight_card 'up' + delay 0.2)
    // 3) 全部升起后再开始 score event
    const int playArrivalMs   = 300;   // 等 onPlayClicked 的 moveTo 完成 + 留一点缓冲
    const int staggerStepMs   = 80;    // 每张卡之间错开 80ms
    const int upDurationMs    = 150;   // 单张卡升起动画 150ms

    for (int i = 0; i < scoringIndices.size(); ++i) {
        int idx = scoringIndices[i];
        int delay = playArrivalMs + i * staggerStepMs;
        const double highlightPitch = 0.85 + ((i + 1.0) - 0.999) / 5.0 * 0.2;
        scheduleGame(delay, [this, idx, upDurationMs, highlightPitch]() {
            if (idx < 0 || idx >= mPlayedCards.size()) return;
            CardItem *c = mPlayedCards[idx];
            if (!c) return;
            AudioManager::instance()->play(QStringLiteral("cardSlide1"), highlightPitch, 1.0);
            // 上升 0.2 * CARD_H (HIGHLIGHT_H 原版常量),保持升起状态不下落。
            QPointF target = c->pos() + QPointF(0, -int(CARD_H * 0.2));
            c->moveTo(target, upDurationMs);
            // 卡牌进入计分态：阴影距离/软化拉满，让卡看上去明显悬浮起来。
            c->setScoringLifted(true);
        });
    }

    // 第一个 score event 在 highlight up 全部完成之后再开始,
    // 给玩家一个清晰的"参与计分的牌已选出"视觉节奏。
    int highlightDoneMs = playArrivalMs
                          + qMax(0, scoringIndices.size() - 1) * staggerStepMs
                          + upDurationMs;
    int delayBase = highlightDoneMs + 180;   // 180ms 缓冲,对应原版 delay(0.2) + 余量
    int delayStep = 180;

    for (int ei = 0; ei < r.events.size(); ++ei) {
        const ScoreEvent ev = r.events[ei];
        int delay = delayBase + ei * delayStep;
        const double percent = 0.3 + 0.08 * ei;
        scheduleGame(delay, [this, ev, percent]() {
            playScoreEvent(ev, percent);
        });
    }

    int finalDelay = delayBase + r.events.size() * delayStep + 260;
    scheduleGame(finalDelay, [this, r, gained, finalDelay]() {
        mDisplayedChips = r.chips;
        mDisplayedMult  = r.mult * r.xmult;
        setLabelScaledText(mLblChips, formatScoreNumber(r.chips),          uiPx(42));
        setLabelScaledText(mLblMult,  formatScoreNumber(mDisplayedMult),   uiPx(42));
        updateFlameIntensity();
        animateScoreTotalThenFinalize(gained, finalDelay);
    });
}

void MainWindow::onSortByNum() {
    mBestPlayHintActive = false;
    mGameState->sortHandByRank();
    AudioManager::instance()->play(QStringLiteral("paper1"), 1.0, 1.0);
}

void MainWindow::onSortBySuit() {
    mBestPlayHintActive = false;
    mGameState->sortHandBySuit();
    AudioManager::instance()->play(QStringLiteral("paper1"), 1.0, 1.0);
}

void MainWindow::onBestPlayHint() {
    if (!mGameState) return;
    if (mGameState->phase() != GamePhase::Blind) return;
    if (mScoringInProgress) return;
    if (mGameState->hand().isEmpty()) return;

    // 第二次点击“最佳出牌”：恢复到第一次点击前的手牌顺序快照——
    // 如果玩家在点最佳出牌之前手动拖动过牌，这里会精确回到那个排列；
    // 没有手动整理过的话快照本身就是当时的点数/花色排序，等价于"取消提示"。
    if (mBestPlayHintActive) {
        mBestPlayHintActive = false;
        mSelected.clear();
        for (CardItem *c : mHandCards)
            if (c) c->setCardSelected(false);

        if (!mBestPlayHintHandOrder.isEmpty())
            mGameState->reorderHandByUids(mBestPlayHintHandOrder);
        mBestPlayHintHandOrder.clear();

        AudioManager::instance()->play(QStringLiteral("cardSlide2"), audioPitchJitter(0.03), 0.55);
        layoutHandCards();
        refreshCounters();
        updateHandPreview();
        return;
    }

    // 遍历所有出牌组合/排列，找当前小丑顺序下分数最高的一种。
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QVector<int> best = mGameState->findBestPlay();
    QApplication::restoreOverrideCursor();
    if (best.isEmpty()) return;

    const int k = best.size();
    // 在动手改顺序之前，记下当前手牌的 uid 顺序——第二次点击时按 uid 回放。
    mBestPlayHintHandOrder.clear();
    for (const CardData &c : mGameState->hand())
        mBestPlayHintHandOrder.append(c.uid);
    // 把最佳出牌按最优顺序移到手牌最前；handChanged 会同步重建 mHandCards。
    mGameState->bringHandCardsToFront(best);
    mBestPlayHintActive = true;
    AudioManager::instance()->play(QStringLiteral("cardSlide1"), audioPitchJitter(0.03), 0.55);

    // 重建后最佳出牌就在最前面，选中前 k 张。
    mSelected.clear();
    for (int i = 0; i < mHandCards.size(); ++i) {
        const bool sel = (i < k);
        mHandCards[i]->setCardSelected(sel);
        if (sel) mSelected.append(i);
    }

    layoutHandCards();
    refreshCounters();
    updateHandPreview();
}

void MainWindow::onForesightClicked()
{
    // 占卜:基于当前选中手牌做一次无副作用得分模拟,把进度条短暂推到
    // "出完这手后" 的位置,然后回退。回合分数标签 mLblScore 不变,增加神秘感。
    if (!mGameState || !mScoreProgressBar) return;
    if (mGameState->phase() != GamePhase::Blind) return;
    if (mScoringInProgress) return;
    if (mSelected.isEmpty()) return;
    if (mForesightPreviewActive) return;

    QVector<int> ordered = mSelected;
    std::sort(ordered.begin(), ordered.end());

    const double projected = mGameState->estimatePlayScore(ordered);
    const double target = mGameState->targetScore();
    const double cur = mGameState->score();
    if (!std::isfinite(target) || target <= 0.0) return;

    const double previewScore = cur + (std::isfinite(projected) ? projected : 0.0);
    const double ratio = qBound(0.0, previewScore / target, 1.0);
    const int previewBarValue = qBound(0, int(std::round(ratio * 1000.0)), 1000);
    const int previewPercent = qBound(0, int(std::round(ratio * 100.0)), 100);
    const int savedBarValue = mScoreProgressBar->value();
    const QString savedStyle = mScoreProgressBar->styleSheet();
    const QString savedFormat = mScoreProgressBar->format();

    // 正在跑的进度条动画先停掉,避免和预览动画打架。
    if (mScoreProgressAnim && mScoreProgressAnim->state() == QAbstractAnimation::Running)
        mScoreProgressAnim->stop();

    mForesightPreviewActive = true;
    if (mBtnForesight) mBtnForesight->setEnabled(false);
    AudioManager::instance()->play(QStringLiteral("tarot1"), 1.25, 0.55);

    // 应用 "预览" 样式:仍保留原本蓝-青渐变的已得分段,边框 + 新增段换成琥珀色,
    // 让玩家一眼就分辨"原始进度 vs. 占卜预测进度"。
    const double splitRatio = (previewScore > 0.0) ? qBound(0.0, cur / previewScore, 1.0) : 0.0;
    const double splitNext = qMin(1.0, splitRatio + 0.04);
    mScoreProgressBar->setStyleSheet(QString(
        "QProgressBar {"
        " background:rgba(8,18,24,112);"
        " border:3px solid #fda200;"
        " border-radius:14px;"
        " color:#fff5d6;"
        " text-align:center;"
        " padding:2px;"
        "}"
        "QProgressBar::chunk {"
        " border-radius:11px;"
        " background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        " stop:0 #009dff, stop:%1 #23e6ff, stop:%2 #ffe28a, stop:1 #ffae33);"
        "}"
        ).arg(splitRatio, 0, 'f', 3)
         .arg(splitNext, 0, 'f', 3));
    mScoreProgressBar->setFormat(QString::fromUtf8("\xe2\x89\x88%1%").arg(previewPercent));

    // 上滑到预测值(280ms),停 1.4s(期间样式表里 chunk 渐变不变,等同于"闪光"段保持显示),
    // 再下滑回原值(280ms),最后还原样式表与百分比文本。
    auto *up = new QPropertyAnimation(mScoreProgressBar, "value", this);
    up->setDuration(280);
    up->setStartValue(savedBarValue);
    up->setEndValue(previewBarValue);
    up->setEasingCurve(QEasingCurve::OutCubic);
    up->start(QAbstractAnimation::DeleteWhenStopped);

    QPointer<MainWindow> self(this);
    QTimer::singleShot(1400 + up->duration(), this, [self, savedBarValue, savedStyle, savedFormat]() {
        if (!self || !self->mScoreProgressBar) {
            if (self) self->mForesightPreviewActive = false;
            return;
        }
        QProgressBar *bar = self->mScoreProgressBar;
        // 先把 chunk 颜色恢复成 savedStyle,这样下滑动画里"减少"的那段不会拖着琥珀色一起退。
        bar->setStyleSheet(savedStyle);
        bar->setFormat(savedFormat);
        auto *back = new QPropertyAnimation(bar, "value", self.data());
        back->setDuration(280);
        back->setStartValue(bar->value());
        back->setEndValue(savedBarValue);
        back->setEasingCurve(QEasingCurve::OutCubic);
        QPointer<MainWindow> me = self;
        QObject::connect(back, &QPropertyAnimation::finished, self.data(), [me]() {
            if (!me) return;
            me->mForesightPreviewActive = false;
            // 选中状态没变 → 让按钮重新可用;refreshCounters 也会做这件事,这里只是兜底。
            if (me->mBtnForesight && !me->mSelected.isEmpty())
                me->mBtnForesight->setEnabled(true);
        });
        back->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void MainWindow::onGameOver(bool won)
{
    if (mGameOverHandled) return;
    mGameOverHandled = true;
    mScoringInProgress = false;
    AudioManager::instance()->setPitchMod(0.5);
    AudioManager::instance()->play(won ? QStringLiteral("win") : QStringLiteral("gong"), 1.0, 0.90);
    if (mBtnPlay) mBtnPlay->setEnabled(false);
    if (mBtnDiscard) mBtnDiscard->setEnabled(false);
    if (mBtnForesight) mBtnForesight->setEnabled(false);
    showGameOverOverlay(won);
}

void MainWindow::fitSceneToView()
{
    if (!mView || !mScene) return;
    const QRectF sr = mScene->sceneRect();
    if (sr.isEmpty() || mView->viewport()->width() <= 1 || mView->viewport()->height() <= 1) return;

    mView->resetTransform();
    mView->fitInView(sr, Qt::KeepAspectRatio);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateSceneSize();

    if (mPlayPage) {
        QRect r = mPlayPage->rect();
        if (mDynamicBg) {
            mDynamicBg->setGeometry(r);
            mDynamicBg->setSceneSize(r.width(), r.height());
            mDynamicBg->lower();
        }
        fitSceneToView();
        if (mBlindSelectWidget) { mBlindSelectWidget->setGeometry(lowerOverlayRect()); if (mBlindSelectWidget->isVisible()) mBlindSelectWidget->raise(); }
        if (mRoundEndOverlay)   { mRoundEndOverlay  ->setGeometry(r);                 if (mRoundEndOverlay->isVisible())   mRoundEndOverlay->raise(); }
        if (mShopWidget)        { mShopWidget       ->setGeometry(shopOverlayRect()); if (mShopWidget->isVisible())        mShopWidget->raise(); }
        if (mPackOpenWidget)    { mPackOpenWidget   ->setGeometry(lowerOverlayRect()); if (mPackOpenWidget->isVisible())    mPackOpenWidget->raise(); }
        if (mSplashOverlay)     { mSplashOverlay    ->setGeometry(r);                 if (mSplashOverlay->isVisible())     mSplashOverlay->raise(); }
        if (mDeckViewWidget)    { mDeckViewWidget   ->setGeometry(r);                 if (mDeckViewWidget->isVisible())    mDeckViewWidget->raise(); }
    }
    if (mOptionsOverlay && centralWidget()) {
        mOptionsOverlay->setGeometry(centralWidget()->rect());
        if (mOptionsOverlay->isVisible()) mOptionsOverlay->raise();
    }
}

void MainWindow::refreshJokerSlotFrames()
{
    for (auto *r : mJokerSlotRects) {
        mScene->removeItem(r);
        delete r;
    }
    mJokerSlotRects.clear();

    int visualSlots = Constants::MAX_JOKER_SLOTS;
    int step = TOP_SLOT_W + 14;
    int totalW = TOP_SLOT_W + qMax(0, visualSlots - 1) * step;
    int available = qMin(mSceneW - 470, 840);
    // 左边距加大到 40：让小丑槽离屏幕左侧/侧边栏有明显的距离（用户最新反馈）。
    int startX = 40;
    if (totalW < available) startX = 40 + (available - totalW) / 2;

    // 原版顶部小丑槽边框是圆角矩形 (r ≈ 0.15)；我们也走 addPath/rounded 而非纯 addRect。
    QRectF bg(startX - 16, JOKER_Y + 12, totalW + 32, TOP_SLOT_H + 18);
    QPainterPath path;
    path.addRoundedRect(bg, 16, 16);
    auto *r = mScene->addPath(path,
                              QPen(Qt::NoPen),
                              QBrush(QColor(0, 0, 0, 44)));
    r->setZValue(0.5);
    mJokerSlotRects.append(r);
}

void MainWindow::refreshJokerSlots()
{
    QVector<QPointF> oldPositions;
    oldPositions.reserve(mJokerItems.size());
    for (auto *ji : mJokerItems) {
        if (ji) oldPositions.append(ji->pos());
    }

    refreshJokerSlotFrames();
    // 仅在选中的小丑已经不存在时才隐藏 sell 面板——之前每次 refresh 都 hide，
    // 会让点击售出按钮之外的任何 refresh（比如 hover 信号、layoutPanels）把 sell 按钮闪没。
    const auto &jsRaw = mGameState->jokers();
    if (mSelectedJokerIdx >= jsRaw.size()) {
        mSelectedJokerIdx = -1;
        hideJokerInfo();
    }

    for (auto *ji : mJokerItems) {
        mScene->removeItem(ji);
        // deleteLater 而不是直接 delete——dragReleased 信号 emit 后还会在 mouseReleaseEvent
        // 里访问 this（animateShadowLift、updateShadowZ）。direct delete 会在信号链里就把
        // JokerItem 析构掉，造成 use-after-free crash。
        ji->deleteLater();
    }
    mJokerItems.clear();

    const auto &js = mGameState->jokers();
    int n = js.size();
    const bool flyInNewJoker = mPendingSlotFlyIn.active
                               && mPendingSlotFlyIn.targetArea == 1
                               && n == oldPositions.size() + 1;
    if (mJokerCountLabel) {
        mJokerCountLabel->setPlainText(QString("%1/%2").arg(n).arg(mGameState->jokerSlots()));
        mJokerCountLabel->setPos(40, JOKER_Y + TOP_SLOT_H + 40);
    }
    if (mPackOpenWidget && mPackOpenWidget->isVisible())
        mPackOpenWidget->setFreeJokerSlots(mGameState->jokerSlots() - n);
    int visualSlots = Constants::MAX_JOKER_SLOTS;
    int visualStep = TOP_SLOT_W + 14;
    int visualW = TOP_SLOT_W + qMax(0, visualSlots - 1) * visualStep;
    int available = qMin(mSceneW - 470, 840);
    int rowStartX = 40;
    if (visualW < available) rowStartX = 40 + (available - visualW) / 2;
    // 用固定步距摆放，n < MAX 时整组小丑在 visualW 范围内水平居中。
    int step = overlappedCardStep(visualW, TOP_SLOT_W, n, visualStep);
    int usedW = (n > 0) ? (TOP_SLOT_W + qMax(0, n - 1) * step) : 0;
    int startX = rowStartX + (visualW - usedW) / 2;
    // 垂直方向：将卡牌相对 slot 框 (高 TOP_SLOT_H + 18) 居中。
    const int slotFrameTopY = JOKER_Y + 12;
    const int slotFrameH    = TOP_SLOT_H + 18;
    const int jokerY = slotFrameTopY + (slotFrameH - TOP_SLOT_H) / 2;
    const int selectedLift = int(TOP_SLOT_H * 0.20);

    // 拖动复位用：把 new index → old index 映射出来（同张牌在 from/to 重排前的位置）。
    auto mapNewIdxToOld = [this, &oldPositions](int newIdx) -> int {
        const int f = mPendingJokerReorder.from;
        const int t = mPendingJokerReorder.to;
        if (f < 0 || t < 0) return -1;
        if (newIdx == t) return f;                  // 拖动的那张
        if (f < t) {                                 // 拖向右：(f, t] 的牌左移
            if (newIdx < f || newIdx > t) return newIdx;
            return newIdx + 1;
        } else {                                     // 拖向左：[t, f) 的牌右移
            if (newIdx < t || newIdx > f) return newIdx;
            return newIdx - 1;
        }
    };

    for (int i = 0; i < js.size(); ++i) {
        int x = startX + i * step;
        // 选中的小丑（如刚点击 sell 后 refresh）抬高 0.2*HEIGHT 保持视觉抬升，与原版
        // highlight_offset 一致。
        int y = jokerY - (i == mSelectedJokerIdx ? selectedLift : 0);
        const QPointF targetPos(x, y);
        auto *ji = new JokerItem(js[i]);

        if (flyInNewJoker && i == js.size() - 1) {
            // 新小丑真实卡先占到目标槽但透明，顶层 QLabel 负责从购买/开包位置飞入。
            ji->setPos(targetPos);
            ji->setOpacity(0.0);
            ji->setZValue(20 + i);
        } else if (flyInNewJoker && i < oldPositions.size()) {
            ji->setPos(oldPositions[i]);
            ji->setZValue(20 + i);
        } else if (mPendingJokerReorder.from >= 0 && mPendingJokerReorder.to >= 0) {
            // 拖动复位：从旧 index 的位置出发 moveTo 目标，避免瞬移。
            const int oldIdx = mapNewIdxToOld(i);
            const QPointF startPos = (oldIdx >= 0 && oldIdx < oldPositions.size())
                                          ? oldPositions[oldIdx] : targetPos;
            ji->setPos(startPos);
            ji->setZValue(20 + i);
            ji->moveTo(targetPos, 220);
        } else {
            ji->setPos(targetPos);
            ji->setZValue(20 + i);
        }

        mScene->addItem(ji);
        mJokerItems.append(ji);
        connect(ji, &JokerItem::pressed, this, &MainWindow::onJokerPressed);
        connect(ji, &JokerItem::dragMoved, this, &MainWindow::onJokerDragMoved);
        connect(ji, &JokerItem::dragReleased, this, &MainWindow::onJokerDragReleased);
        connect(ji, &JokerItem::hoverChanged, this, [this, ji](JokerItem *, bool hovered) {
            int idx = mJokerItems.indexOf(ji);
            // 悬停走统一风格的 BalatroInfoPanel；点击展开的"售出"模式仍走旧的 mJokerInfoPanel
            // 因为它带按钮，需要鼠标可点（普通 hover 浮窗对鼠标透明）。
            if (mSelectedJokerIdx >= 0) return;
            if (hovered && idx >= 0) showJokerHoverTooltip(idx);
            else                     hideHoverTooltip();
        });

        if (flyInNewJoker) {
            if (i == js.size() - 1) {
                animateTopLayerCardToScene(mPendingSlotFlyIn.pixmap,
                                           mPendingSlotFlyIn.globalCenter,
                                           targetPos, QSizeF(TOP_SLOT_W, TOP_SLOT_H),
                                           false, ji);
            } else {
                ji->moveTo(targetPos, 220);
            }
        }
    }

    if (mPendingSlotFlyIn.active && mPendingSlotFlyIn.targetArea == 1)
        mPendingSlotFlyIn = PendingSlotFlyIn();

    // 重建小丑后，如果之前点击选中的索引依然有效，重新挂出 sell 面板并对齐新位置——
    // 之前 hideJokerInfo() 在 refresh 开头无条件清空，导致点击售出按钮"看着像闪一下没了"。
    if (mSelectedJokerIdx >= 0 && mSelectedJokerIdx < mJokerItems.size()) {
        showJokerInfo(mSelectedJokerIdx, true);
    }
}

void MainWindow::showJokerInfo(int idx, bool showSellButton)
{
    const auto &js = mGameState->jokers();
    if (idx < 0 || idx >= js.size() || idx >= mJokerItems.size()) return;
    if (!showSellButton && mSelectedJokerIdx >= 0 && mSelectedJokerIdx != idx) return;
    if (showSellButton) mSelectedJokerIdx = idx;
    else if (mSelectedJokerIdx != idx) mSelectedJokerIdx = -1;
    const Joker &j = js[idx];

    if (!mJokerInfoPanel) {
        mJokerInfoPanel = new QWidget;
        mJokerInfoPanel->setAttribute(Qt::WA_StyledBackground, true);
        mJokerInfoPanel->setStyleSheet(
            "background:rgba(31,37,42,235);"
            "border:2px solid #fda200;"
            "border-radius:14px;"
            );
        auto *vbl = new QVBoxLayout(mJokerInfoPanel);
        vbl->setContentsMargins(dp(12), dp(10), dp(12), dp(10));
        vbl->setSpacing(dp(6));

        mJokerInfoName = new QLabel(mJokerInfoPanel);
        QFont nf = mCNFont; nf.setPixelSize(uiPx(22)); nf.setBold(true);
        mJokerInfoName->setFont(nf);
        mJokerInfoName->setStyleSheet("color:#ffe9a8; background:transparent; border:none;");
        mJokerInfoName->setAlignment(Qt::AlignCenter);
        vbl->addWidget(mJokerInfoName);

        mJokerInfoMeta = new QLabel(mJokerInfoPanel);
        QFont mf = mCNFont; mf.setPixelSize(uiPx(16));
        mJokerInfoMeta->setFont(mf);
        mJokerInfoMeta->setStyleSheet("color:#cbd6dc; background:transparent; border:none;");
        mJokerInfoMeta->setAlignment(Qt::AlignCenter);
        vbl->addWidget(mJokerInfoMeta);

        mJokerInfoDesc = new QLabel(mJokerInfoPanel);
        QFont df = mCNFont; df.setPixelSize(uiPx(16));
        mJokerInfoDesc->setFont(df);
        mJokerInfoDesc->setWordWrap(true);
        mJokerInfoDesc->setAlignment(Qt::AlignCenter);
        mJokerInfoDesc->setStyleSheet("color:white; background:transparent; border:none;");
        mJokerInfoDesc->setFixedWidth(dp(310));
        vbl->addWidget(mJokerInfoDesc);

        mJokerSellButton = new QPushButton(mJokerInfoPanel);
        QFont sf = mCNFont; sf.setPixelSize(uiPx(16)); sf.setBold(true);
        mJokerSellButton->setFont(sf);
        mJokerSellButton->setCursor(Qt::PointingHandCursor);
        mJokerSellButton->setStyleSheet(
            "QPushButton { background:#fe5f55; color:white; border:none; border-radius:10px; padding:7px 14px; }"
            "QPushButton:hover { background:#ff7066; }"
            );
        vbl->addWidget(mJokerSellButton);

        mJokerInfoPanel->setParent(mPlayPage);
        mJokerInfoPanel->hide();
        mJokerInfoProxy = nullptr;
    }

    mJokerInfoName->setText(j.name);
    QString editionText = editionName(j.edition);
    if (editionText.isEmpty()) editionText = "普通";
    QString editionEffect = editionDesc(j.edition);
    QString meta = QString("%1小丑　出售 $%2").arg(editionText).arg(qMax(1, j.sellValue));
    if (!editionEffect.isEmpty()) meta += QString("　%1").arg(editionEffect);
    // 与悬浮 info 共用同一份"运行时状态"后缀；含计数器型 (城堡、跑者、绿小丑等)、
    // 当回合花色 / 点数 (古老、偶像、邮购) 等。
    QString desc = j.description + jokerRuntimeStateSuffix(idx);
    mJokerInfoName->setVisible(!showSellButton);
    mJokerInfoDesc->setVisible(!showSellButton);
    // desc 现在用 {C:xxx} markup（来自 jokerRuntimeStateSuffix），按 HTML 渲染才能上色。
    mJokerInfoDesc->setTextFormat(Qt::RichText);
    mJokerInfoDesc->setText(BalatroTooltip::fromLuaMarkup(desc));
    // hover 模式（showSellButton=false）下面板对鼠标透明，避免"hover 卡牌→显示面板→
    // 鼠标在面板上→卡牌 hover 消失→面板隐藏"的反复闪烁。
    // 点击模式（true）下含有售出按钮，必须可点。
    mJokerInfoPanel->setAttribute(Qt::WA_TransparentForMouseEvents, !showSellButton);

    if (showSellButton) {
        if (auto *lay = mJokerInfoPanel->layout()) {
            lay->setContentsMargins(0, 0, 0, 0);
            lay->setSpacing(0);
        }
        mJokerInfoPanel->setStyleSheet("background:transparent; border:none;");
        mJokerInfoPanel->setFixedSize(dp(76), dp(58));

        mJokerInfoMeta->clear();
        mJokerInfoMeta->setVisible(false);
        mJokerInfoMeta->setFixedHeight(0);

        QFont sf = mCNFont; sf.setPixelSize(uiPx(15)); sf.setBold(true);
        mJokerSellButton->setFont(sf);
        mJokerSellButton->setText(QString("售出\n$%1").arg(qMax(1, j.sellValue)));
        mJokerSellButton->setFixedSize(dp(76), dp(58));
        mJokerSellButton->setStyleSheet(
            "QPushButton { background:#10372f; color:white; border:0px;"
            "border-radius:11px; padding:0px; text-align:center; }"
            "QPushButton:hover { background:#145143; }"
            "QPushButton:pressed { background:#0b2923; }"
            );
    } else {
        if (auto *lay = mJokerInfoPanel->layout()) {
            lay->setContentsMargins(dp(12), dp(10), dp(12), dp(10));
            lay->setSpacing(dp(6));
        }
        mJokerInfoPanel->setMinimumSize(0, 0);
        mJokerInfoPanel->setMaximumSize(16777215, 16777215);
        mJokerInfoPanel->setStyleSheet(
            "background:rgba(31,37,42,235);"
            "border:2px solid #fda200;"
            "border-radius:14px;"
            );
        mJokerInfoPanel->setFixedWidth(dp(350));
        mJokerInfoName->setFixedWidth(dp(314));
        mJokerInfoMeta->setFixedWidth(dp(314));
        mJokerInfoDesc->setFixedWidth(dp(314));
        QFont mf = mCNFont; mf.setPixelSize(uiPx(16)); mf.setBold(false);
        mJokerInfoMeta->setFont(mf);
        mJokerInfoMeta->setVisible(true);
        mJokerInfoMeta->setStyleSheet("color:#cbd6dc; background:transparent; border:none;");
        mJokerInfoMeta->setMinimumHeight(0);
        mJokerInfoMeta->setMaximumHeight(16777215);
        mJokerInfoMeta->setWordWrap(true);
        mJokerInfoMeta->setText(meta);
        mJokerSellButton->setMinimumSize(0, 0);
        mJokerSellButton->setMaximumSize(16777215, 16777215);
    }
    mJokerSellButton->setVisible(showSellButton);
    if (!showSellButton) {
        if (auto *lay = mJokerInfoPanel->layout()) lay->activate();
        mJokerInfoPanel->adjustSize();
        mJokerInfoPanel->resize(dp(350), qBound(dp(142), mJokerInfoPanel->height(), dp(360)));
    }

    disconnect(mJokerSellButton, nullptr, this, nullptr);
    connect(mJokerSellButton, &QPushButton::clicked, this, [this]() {
        if (mSelectedJokerIdx < 0) return;
        const bool bossDisableSound =
            mSelectedJokerIdx < mGameState->jokers().size()
            && mGameState->jokers()[mSelectedJokerIdx].type == JokerType::Luchador
            && mGameState->bossEffect() != BossEffect::None;
        if (mGameState->sellJoker(mSelectedJokerIdx)) {
            if (bossDisableSound) {
                playOriginalStatusGenericSound(this);
                playOriginalBlindWiggleSound(this);
            }
            QTimer::singleShot(200, this, []() {
                AudioManager::instance()->play(QStringLiteral("coin2"), 1.0, 1.0);
                AudioManager::instance()->play(QStringLiteral("coin1"), 1.0, 1.0);
            });
            mSelectedJokerIdx = -1;
            hideJokerInfo();
            if (mPackOpenWidget && mPackOpenWidget->isVisible())
                mPackOpenWidget->setFreeJokerSlots(mGameState->jokerSlots() - mGameState->jokers().size());
            if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
            refreshCounters();
        }
    });

    QPointF jp = mJokerItems[idx]->pos();
    qreal x;
    qreal y;
    if (showSellButton) {
        x = jp.x() + JokerItem::WIDTH + 8;
        y = jp.y() + JokerItem::HEIGHT * 0.5 - 34;
        if (x + 76 > mSceneW - 6) x = jp.x() - 84;
        x = qBound<qreal>(6, x, mSceneW - 82);
        y = qBound<qreal>(6, y, mSceneH - 64);
    } else {
        x = jp.x() + JokerItem::WIDTH / 2.0 - 140;
        x = qBound<qreal>(8, x, mSceneW - 285);
        y = jp.y() + JokerItem::HEIGHT + 10;
        if (mShopWidget && mShopWidget->isVisible()) {
            qreal shopTopSceneY = lowerOverlayRect().y() + 18;
            if (y + mJokerInfoPanel->height() > shopTopSceneY)
                y = jp.y() - mJokerInfoPanel->height() - 12;
        }
    }
    if (mJokerInfoPanel->parentWidget() != mPlayPage) mJokerInfoPanel->setParent(mPlayPage);
    QPoint viewPoint = mView->mapFromScene(QPointF(x, y));
    QPoint pagePoint = mView->mapTo(mPlayPage, viewPoint);
    pagePoint.setX(qBound(dp(6), pagePoint.x(), qMax(dp(6), mPlayPage->width() - mJokerInfoPanel->width() - dp(6))));
    pagePoint.setY(qBound(dp(6), pagePoint.y(), qMax(dp(6), mPlayPage->height() - mJokerInfoPanel->height() - dp(6))));
    mJokerInfoPanel->move(pagePoint);
    mJokerInfoPanel->raise();
    mJokerInfoPanel->show();
}

void MainWindow::hideJokerInfo()
{
    if (mJokerInfoPanel) mJokerInfoPanel->hide();
}

void MainWindow::onJokerPressed(JokerItem *item, Qt::MouseButton btn)
{
    int idx = mJokerItems.indexOf(item);
    if (idx < 0) return;
    if (btn != Qt::LeftButton && btn != Qt::RightButton) return;

    // 切换选中：再次点同一张 = 取消选中并落下；点新张则把旧的落下、新的抬起。
    const int prevSelected = mSelectedJokerIdx;
    AudioManager::instance()->play(prevSelected == idx ? QStringLiteral("cardSlide2")
                                                       : QStringLiteral("cardSlide1"),
                                   1.0, prevSelected == idx ? 0.3 : 1.0);
    if (mSelectedJokerIdx == idx) {
        mSelectedJokerIdx = -1;
        hideJokerInfo();
    } else {
        mSelectedJokerIdx = idx;
        // 先隐藏 hover 浮窗——sell 面板会出现在小丑右侧，两张 info 同时出来视觉很乱。
        hideHoverTooltip();
        showJokerInfo(idx, true);
        item->juiceUp(1.08, 140);
    }
    // 应用选中抬升（被取消选中的也要落回）；不重建 mJokerItems。
    Q_UNUSED(prevSelected);
    applyJokerSelectionLift();
}

void MainWindow::applyJokerSelectionLift()
{
    if (mJokerItems.isEmpty()) return;
    int n = mJokerItems.size();
    const int visualSlots = Constants::MAX_JOKER_SLOTS;
    const int visualStep = TOP_SLOT_W + 14;
    const int visualW = TOP_SLOT_W + qMax(0, visualSlots - 1) * visualStep;
    const int available = qMin(mSceneW - 470, 840);
    int rowStartX = 40;
    if (visualW < available) rowStartX = 40 + (available - visualW) / 2;
    const int step = overlappedCardStep(visualW, TOP_SLOT_W, n, visualStep);
    const int usedW = (n > 0) ? (TOP_SLOT_W + qMax(0, n - 1) * step) : 0;
    const int startX = rowStartX + (visualW - usedW) / 2;
    const int slotFrameTopY = JOKER_Y + 12;
    const int slotFrameH    = TOP_SLOT_H + 18;
    const int baseY = slotFrameTopY + (slotFrameH - TOP_SLOT_H) / 2;
    // 原版 highlight_offset ≈ 0.2 * card_h；这里取 20% TOP_SLOT_H ≈ 40px。
    const int lift = int(TOP_SLOT_H * 0.20);
    for (int i = 0; i < n; ++i) {
        if (!mJokerItems[i]) continue;
        const int x = startX + i * step;
        const int y = (i == mSelectedJokerIdx) ? (baseY - lift) : baseY;
        mJokerItems[i]->moveTo(QPointF(x, y), 180);
    }
}


void MainWindow::onJokerDragMoved(JokerItem *item, QPointF scenePos)
{
    int from = mJokerItems.indexOf(item);
    if (from < 0) return;
    int n = mJokerItems.size();
    if (n <= 1) return;

    // 与 refreshJokerSlots 保持同一套居中逻辑：固定步距 + 整组在 visualW 内居中。
    int visualSlots = Constants::MAX_JOKER_SLOTS;
    int visualStep = TOP_SLOT_W + 14;
    int visualW = TOP_SLOT_W + qMax(0, visualSlots - 1) * visualStep;
    int available = qMin(mSceneW - 470, 840);
    int rowStartX = 40;
    if (visualW < available) rowStartX = 40 + (available - visualW) / 2;
    int step = overlappedCardStep(visualW, TOP_SLOT_W, n, visualStep);
    int usedW = TOP_SLOT_W + qMax(0, n - 1) * step;
    int startX = rowStartX + (visualW - usedW) / 2;
    const int slotFrameTopY = JOKER_Y + 12;
    const int slotFrameH    = TOP_SLOT_H + 18;
    const int jokerY = slotFrameTopY + (slotFrameH - TOP_SLOT_H) / 2;

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + TOP_SLOT_W / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (to == mLastJokerDragTo) {
        item->setZValue(650);
        return;
    }
    mLastJokerDragTo = to;

    QVector<JokerItem*> visual = mJokerItems;
    visual.removeAt(from);
    visual.insert(to, item);
    for (int vi = 0; vi < visual.size(); ++vi) {
        JokerItem *ji = visual[vi];
        if (ji == item) continue;
        int x = startX + vi * step;
        ji->setZValue(20 + vi);
        ji->moveTo(QPointF(x, jokerY), 60);
    }
    item->setZValue(650);
}

void MainWindow::onJokerDragReleased(JokerItem *item, QPointF scenePos)
{
    mLastJokerDragTo = -1;
    int from = mJokerItems.indexOf(item);
    if (from < 0) { refreshJokerSlots(); return; }
    int n = mJokerItems.size();
    if (n <= 1) { refreshJokerSlots(); return; }

    int visualSlots = Constants::MAX_JOKER_SLOTS;
    int visualStep = TOP_SLOT_W + 14;
    int visualW = TOP_SLOT_W + qMax(0, visualSlots - 1) * visualStep;
    int available = qMin(mSceneW - 470, 840);
    int rowStartX = 40;
    if (visualW < available) rowStartX = 40 + (available - visualW) / 2;
    int step = overlappedCardStep(visualW, TOP_SLOT_W, n, visualStep);
    int usedW = TOP_SLOT_W + qMax(0, n - 1) * step;
    int startX = rowStartX + (visualW - usedW) / 2;

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + TOP_SLOT_W / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (from != to) {
        // 标记给 refreshJokerSlots：刚发生 from→to 的拖动复位，新生成的 joker item 应当
        // 从旧 index 的 position 开始 moveTo 目标，避免瞬移。
        AudioManager::instance()->play(QStringLiteral("cardSlide1"), audioPitchJitter(0.03), 0.62);
        mPendingJokerReorder = {from, to};
        mGameState->moveJoker(from, to);
        mPendingJokerReorder = {-1, -1};
    } else refreshJokerSlots();
}


void MainWindow::showConsumableAction(int idx)
{
    const auto &cs = mGameState->consumables();
    if (idx < 0 || idx >= cs.size() || idx >= mConsumableItems.size()) return;
    mSelectedConsumableIdx = idx;
    const Consumable &c = cs[idx];
    AudioManager::instance()->play(QStringLiteral("cardSlide1"), 1.0, 1.0);
    layoutConsumableItems(true);

    if (!mConsumableActionPanel) {
        mConsumableActionPanel = new QWidget;
        mConsumableActionPanel->setAttribute(Qt::WA_StyledBackground, true);
        mConsumableActionPanel->setStyleSheet("background:transparent;");
        auto *v = new QVBoxLayout(mConsumableActionPanel);
        v->setContentsMargins(2, 0, 2, 0);
        v->setSpacing(6);

        mConsumableActionPrice = new QLabel(mConsumableActionPanel);
        mConsumableActionPrice->hide();
        mConsumableActionPrice->setFixedHeight(0);
        v->addWidget(mConsumableActionPrice);

        QFont bf = mCNFont; bf.setPixelSize(uiPx(12)); bf.setBold(true);
        // 用户期望：选中已拥有的消耗牌 → 卡牌右侧上下两个按钮，
        // 上方绿色"售出 $X"，下方红色"使用"。 配色对齐原版 G.C.GREEN/G.C.RED。
        mConsumableSellButton = new QPushButton("售出", mConsumableActionPanel);
        mConsumableSellButton->setFont(bf);
        mConsumableSellButton->setFixedSize(60, 56);
        mConsumableSellButton->setStyleSheet(
            "QPushButton { background:#4ca893; color:white; border:2px solid rgba(255,255,255,90);"
            " border-radius:10px; padding:0px; text-align:center; font-weight:bold; }"
            "QPushButton:hover { background:#5fbfa8; border:2px solid rgba(255,255,255,170); }"
            "QPushButton:pressed { background:#3f8a78; }"
            );
        v->addWidget(mConsumableSellButton);

        mConsumableUseButton = new QPushButton("使用", mConsumableActionPanel);
        mConsumableUseButton->setFont(bf);
        mConsumableUseButton->setFixedSize(60, 56);
        mConsumableUseButton->setStyleSheet(
            "QPushButton { background:#fe5f55; color:white; border:2px solid rgba(255,255,255,90);"
            " border-radius:10px; padding:0px; text-align:center; font-weight:bold; }"
            "QPushButton:hover { background:#ff7066; border:2px solid rgba(255,255,255,170); }"
            "QPushButton:pressed { background:#d94a42; }"
            "QPushButton:disabled { background:#5a4642; color:#a39998;"
            " border:2px solid rgba(255,255,255,40); }"
            );
        v->addWidget(mConsumableUseButton);

        // 上下两个按钮 + spacing 6 + margins 0 = 56+6+56 = 118 高，宽 64 含 margin。
        mConsumableActionPanel->setFixedSize(64, 56 + 6 + 56);
        mConsumableActionProxy = mScene->addWidget(mConsumableActionPanel);
        mConsumableActionProxy->setZValue(5000);

        connect(mConsumableUseButton, &QPushButton::clicked, this, [this]() {
            int idx = mSelectedConsumableIdx;
            if (idx < 0 || idx >= mGameState->consumables().size()) return;

            QVector<int> sel = mSelected;
            std::sort(sel.begin(), sel.end());
            sel.erase(std::unique(sel.begin(), sel.end()), sel.end());

            const Consumable &c = mGameState->consumables()[idx];
            const ConsumableType type = c.type;
            const QString useSound = soundForConsumable(c.type);
            if ((c.needsSelection > 0 && sel.size() < c.needsSelection) ||
                (c.type == ConsumableType::Tarot_Fool && !mGameState->canUseFool())) {
                AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
                flashConsumableActionError();
                return;
            }

            animateConsumableUseThen(idx, [this, idx, sel, type, useSound]() {
                // 对齐原版 card.lua:1106-1149 —— 当塔罗 / 幻灵牌作用到选中手牌时：
                //   1) 选中的牌先 flip 到背面（240ms 动画 + 0.15s 间隔）
                //   2) 应用增强/花色/点数变化
                //   3) 再 flip 回正面，把新外观"翻"出来
                const auto &cs = mGameState->consumables();
                // 该消耗牌是否消耗选中的手牌（如奖励/倍率/换花色等塔罗）。
                const bool usesSelection = (idx >= 0 && idx < cs.size())
                                           && cs[idx].needsSelection > 0;
                const bool needsHandFlip = usesSelection
                                           && !sel.isEmpty()
                                           && usesOriginalTarotFlip(type);

                auto doUseAndRefresh = [this, idx, sel, type, usesSelection, needsHandFlip, useSound]() {
                    const QVector<CardData> handBefore = mGameState->hand();
                    const QVector<Joker> jokersBefore = mGameState->jokers();
                    const QVector<Consumable> consumablesBefore = mGameState->consumables();
                    const int goldBefore = mGameState->gold();
                    if (mGameState->useConsumable(idx, sel)) {
                        const bool handled = needsHandFlip || playOriginalConsumableAudio(
                            this,
                            type,
                            sel,
                            handBefore,
                            mGameState->hand(),
                            jokersBefore,
                            mGameState->jokers(),
                            consumablesBefore,
                            mGameState->consumables(),
                            goldBefore,
                            mGameState->gold(),
                            false);
                        if (!handled && !useSound.isEmpty())
                            AudioManager::instance()->play(useSound, 1.0, 1.0);
                        mSelectedConsumableIdx = -1;
                        hideConsumableAction();
                        // 仅消耗选中手牌的塔罗才清空选择；命运之轮等不选牌的
                        // 消耗牌不应取消玩家已有的手牌选中。
                        if (usesSelection) mSelected.clear();
                        refreshHand();
                        refreshGold();
                        refreshScore();
                        refreshCounters();
                        if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
                    } else {
                        AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
                        flashConsumableActionError();
                        refreshConsumableSlots();
                    }
                };

                if (!needsHandFlip) {
                    doUseAndRefresh();
                    return;
                }

                // 记录受影响的 CardItem 指针（按当前手牌索引）。
                QVector<QPointer<CardItem>> targets;
                for (int i : sel) {
                    if (i >= 0 && i < mHandCards.size())
                        targets.append(QPointer<CardItem>(mHandCards[i]));
                }

                // 翻面序列期间禁止 refreshHand 里的“房屋 Boss 翻正”逻辑，
                // 否则会和这里的手动 flip 打架，导致选中的牌停在背面。
                mSuppressHandReveal = true;

                // 1) flip 翻到背面。flip() 内部用 scale 1→0→1 动画，整段 240ms。
                const int targetCount = targets.size();
                auto flipPitch = [](int index, int count, bool firstFlip) {
                    const double denom = count - 0.998;
                    const double percent = (index + 0.001) / denom * 0.3;
                    return firstFlip ? (1.15 - percent) : (0.85 + percent);
                };

                AudioManager::instance()->play(QStringLiteral("tarot1"), 1.0, 1.0);

                for (int i = 0; i < targetCount; ++i) {
                    QPointer<CardItem> target = targets[i];
                    const double pitch = flipPitch(i, targetCount, true);
                    QTimer::singleShot(150 * (i + 1), this, [target, pitch]() {
                        if (!target) return;
                        target->flip();
                        AudioManager::instance()->play(QStringLiteral("card1"), pitch, 1.0);
                    });
                }

                // 2) 等翻面动画过半后开始改 CardData（此时正在显示 "0" scale，玩家看不到差异）。
                QTimer::singleShot(150 * targetCount + 200, this, [this, doUseAndRefresh, targets, targetCount, flipPitch]() {
                    doUseAndRefresh();
                    // 3) 等 doUse 完成 + 小延迟后，再把牌翻回正面。setCardData 已经保留了 faceUp=false，
                    // 所以现在 mHandCards 里对应的 CardItem 还是背面。
                    QTimer::singleShot(0, this, [this, targets, targetCount, flipPitch]() {
                        for (int i = 0; i < targetCount; ++i) {
                            QPointer<CardItem> target = targets[i];
                            const double pitch = flipPitch(i, targetCount, false);
                            QTimer::singleShot(150 * (i + 1), this, [target, pitch]() {
                                if (!target) return;
                                target->flip();
                                AudioManager::instance()->play(QStringLiteral("tarot2"), pitch, 0.6);
                            });
                        }
                        QTimer::singleShot(150 * targetCount + 20, this, [this]() {
                            mSuppressHandReveal = false;
                        });
                    });
                });
            });
        });
        connect(mConsumableSellButton, &QPushButton::clicked, this, [this]() {
            if (mSelectedConsumableIdx < 0) return;
            if (mGameState->sellConsumable(mSelectedConsumableIdx)) {
                QTimer::singleShot(200, this, []() {
                    AudioManager::instance()->play(QStringLiteral("coin2"), 1.0, 1.0);
                    AudioManager::instance()->play(QStringLiteral("coin1"), 1.0, 1.0);
                });
                mSelectedConsumableIdx = -1;
                hideConsumableAction();
                refreshConsumableSlots();
                refreshGold();
                refreshCounters();
                if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
            }
        });
    }

    mConsumableActionPrice->clear();
    mConsumableActionPrice->setVisible(false);
    mConsumableSellButton->setText(QString("售出\n$%1").arg(qMax(1, c.sellValue)));
    QPointF cp = mConsumableItems[idx]->pos();
    // 消耗牌点中后像手牌一样上浮；按钮列贴在卡牌右侧、与卡片纵向中线对齐。
    const int panelW = mConsumableActionPanel->width();
    const int panelH = mConsumableActionPanel->height();
    // 紧贴卡片右沿，缩小到 +2 让按钮"挂在"卡片上而不是浮在外面。
    qreal x = cp.x() + ConsumableItem::WIDTH + 2;
    qreal y = cp.y() + ConsumableItem::HEIGHT * 0.5 - panelH / 2.0;
    if (x + panelW > mSceneW - 6) x = cp.x() - panelW - 2;
    x = qBound<qreal>(6, x, mSceneW - panelW - 6);
    y = qBound<qreal>(6, y, mSceneH - panelH - 6);
    mConsumableActionProxy->setZValue(5000);
    mConsumableActionProxy->setPos(x, y);
    mConsumableActionPanel->show();
    refreshConsumableUseButtonState();
}

void MainWindow::hideConsumableAction()
{
    if (mConsumableActionPanel) mConsumableActionPanel->hide();
    if (mSelectedConsumableIdx >= 0) {
        mSelectedConsumableIdx = -1;
        layoutConsumableItems(true);
    }
}

void MainWindow::refreshConsumableUseButtonState()
{
    if (!mConsumableUseButton) return;
    const int idx = mSelectedConsumableIdx;
    const auto &cs = mGameState->consumables();
    if (idx < 0 || idx >= cs.size()) {
        mConsumableUseButton->setEnabled(false);
        mConsumableUseButton->setToolTip(QString());
        return;
    }
    const Consumable &c = cs[idx];
    QVector<int> sel = mSelected;
    std::sort(sel.begin(), sel.end());
    sel.erase(std::unique(sel.begin(), sel.end()), sel.end());

    bool ok = true;
    QString reason;
    if (c.needsSelection > 0 && sel.size() < c.needsSelection) {
        ok = false;
        reason = QString("需要选中 %1 张手牌").arg(c.needsSelection);
    } else if (c.maxSelection > 0 && sel.size() > c.maxSelection) {
        ok = false;
        reason = QString("最多选 %1 张手牌").arg(c.maxSelection);
    } else if (c.type == ConsumableType::Tarot_Fool && !mGameState->canUseFool()) {
        ok = false;
        reason = QStringLiteral("没有可复制的消耗牌");
    }

    mConsumableUseButton->setEnabled(ok);
    mConsumableUseButton->setToolTip(ok ? QString() : reason);
}

void MainWindow::refreshConsumableSlotFrames()
{
    for (auto *r : mConsumableSlotRects) {
        mScene->removeItem(r);
        delete r;
    }
    mConsumableSlotRects.clear();

    int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
    int step = TOP_SLOT_W + 14;
    int totalW = TOP_SLOT_W + qMax(0, visualSlots - 1) * step;
    // 右边距 40：让消耗槽离屏幕右沿更远（用户最新反馈"消耗槽离右侧一点距离"）。
    int startX = mSceneW - 40 - totalW;
    // 与小丑牌槽保持一致：同一份半透明黑色底纹，圆角矩形（原版 r ≈ 0.15）。
    QRectF bg(startX - 16, JOKER_Y + 12, totalW + 32, TOP_SLOT_H + 18);
    QPainterPath path;
    path.addRoundedRect(bg, 16, 16);
    auto *r = mScene->addPath(path,
                              QPen(Qt::NoPen),
                              QBrush(QColor(0, 0, 0, 44)));
    r->setZValue(0.5);
    mConsumableSlotRects.append(r);
}

void MainWindow::refreshConsumableSlots()
{
    QVector<QPointF> oldPositions;
    oldPositions.reserve(mConsumableItems.size());
    for (auto *ci : mConsumableItems) {
        if (ci) oldPositions.append(ci->pos());
    }

    refreshConsumableSlotFrames();

    // deleteLater 同样原因：消耗品拖动时 dragReleased 信号回调里会调用 refreshConsumableSlots，
    // 直接 delete 会把当前正在 mouseReleaseEvent 中的 ConsumableItem 析构掉，造成 crash。
    for (auto *ci : mConsumableItems) { mScene->removeItem(ci); ci->deleteLater(); }
    mConsumableItems.clear();

    const auto &cs = mGameState->consumables();
    const bool flyInNewConsumable = mPendingSlotFlyIn.active
                                    && mPendingSlotFlyIn.targetArea == 2
                                    && cs.size() == oldPositions.size() + 1;
    if (mSelectedConsumableIdx >= cs.size()) {
        mSelectedConsumableIdx = -1;
        hideConsumableAction();
    }
    int slotCount = mGameState->consumableSlots();
    if (mConsCountLabel) {
        mConsCountLabel->setPlainText(QString("%1/%2").arg(cs.size()).arg(slotCount));
        QRectF br = mConsCountLabel->boundingRect();
        int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
        int totalW = TOP_SLOT_W + qMax(0, visualSlots - 1) * (TOP_SLOT_W + 14);
        int startX = mSceneW - 40 - totalW;
        mConsCountLabel->setPos(startX + totalW - br.width() - 2, JOKER_Y + TOP_SLOT_H + 40);
    }

    int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
    int totalW = TOP_SLOT_W + qMax(0, visualSlots - 1) * (TOP_SLOT_W + 14);
    int startX = mSceneW - 40 - totalW;
    int step = overlappedCardStep(totalW, TOP_SLOT_W, cs.size(), TOP_SLOT_W + 14);
    auto mapConsNewIdxToOld = [this, &oldPositions](int newIdx) -> int {
        const int f = mPendingConsumableReorder.from;
        const int t = mPendingConsumableReorder.to;
        if (f < 0 || t < 0) return -1;
        if (newIdx == t) return f;
        if (f < t) {
            if (newIdx < f || newIdx > t) return newIdx;
            return newIdx + 1;
        } else {
            if (newIdx < t || newIdx > f) return newIdx;
            return newIdx - 1;
        }
    };

    for (int i = 0; i < cs.size(); ++i) {
        int x = startX + i * step;
        int y = JOKER_Y + 18 + ((i == mSelectedConsumableIdx) ? -42 : 0);
        const QPointF targetPos(x, y);
        auto *ci = new ConsumableItem(cs[i]);

        if (flyInNewConsumable && i == cs.size() - 1) {
            ci->setPos(targetPos);
            ci->setOpacity(0.0);
            ci->setZValue(30 + i);
        } else if (flyInNewConsumable && i < oldPositions.size()) {
            ci->setPos(oldPositions[i]);
            ci->setZValue(30 + i);
        } else if (mPendingConsumableReorder.from >= 0 && mPendingConsumableReorder.to >= 0) {
            // 拖动复位：从旧 index 的位置出发 moveTo 目标。
            const int oldIdx = mapConsNewIdxToOld(i);
            const QPointF startPos = (oldIdx >= 0 && oldIdx < oldPositions.size())
                                          ? oldPositions[oldIdx] : targetPos;
            ci->setPos(startPos);
            ci->setZValue(30 + i);
            ci->moveTo(targetPos, 220);
        } else {
            ci->setPos(targetPos);
            ci->setZValue(30 + i);
        }

        mScene->addItem(ci);
        mConsumableItems.append(ci);

        connect(ci, &ConsumableItem::pressed,
                this, &MainWindow::onConsumablePressed);
        connect(ci, &ConsumableItem::dragMoved,
                this, &MainWindow::onConsumableDragMoved);
        connect(ci, &ConsumableItem::dragReleased,
                this, &MainWindow::onConsumableDragReleased);
        connect(ci, &ConsumableItem::hoverChanged,
                this, [this, ci](ConsumableItem *, bool hovered) {
            // 悬停时弹出 BalatroInfoPanel——点击展开的"使用/出售"小操作面板仍是 mConsumableActionPanel。
            if (mSelectedConsumableIdx >= 0) return;
            int idx = mConsumableItems.indexOf(ci);
            if (hovered && idx >= 0) showConsumableHoverTooltip(idx);
            else                     hideHoverTooltip();
        });

        if (flyInNewConsumable) {
            if (i == cs.size() - 1) {
                animateTopLayerCardToScene(mPendingSlotFlyIn.pixmap,
                                           mPendingSlotFlyIn.globalCenter,
                                           targetPos, QSizeF(TOP_SLOT_W, TOP_SLOT_H),
                                           false, ci);
            } else {
                ci->moveTo(targetPos, 220);
            }
        }
    }

    if (flyInNewConsumable) {
        mPendingSlotFlyIn = PendingSlotFlyIn();
    } else {
        layoutConsumableItems(false);
        if (mPendingSlotFlyIn.active && mPendingSlotFlyIn.targetArea == 2)
            mPendingSlotFlyIn = PendingSlotFlyIn();
    }
}

void MainWindow::layoutConsumableItems(bool animate)
{
    const int n = mConsumableItems.size();
    if (n == 0) return;

    int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
    int totalW = TOP_SLOT_W + qMax(0, visualSlots - 1) * (TOP_SLOT_W + 14);
    int startX = mSceneW - 40 - totalW;
    int step = overlappedCardStep(totalW, TOP_SLOT_W, n, TOP_SLOT_W + 14);

    for (int i = 0; i < n; ++i) {
        ConsumableItem *ci = mConsumableItems[i];
        if (!ci) continue;
        const int x = startX + i * step;
        const int y = JOKER_Y + 18 + ((i == mSelectedConsumableIdx) ? -42 : 0);
        ci->setZValue(30 + i);   // 永远按槽位从左到右叠，不因点击而盖住右侧牌
        if (animate) ci->moveTo(QPointF(x, y), 160);
        else ci->setPos(x, y);
    }
}

void MainWindow::flashConsumableActionError()
{
    if (mConsumableActionPanel && mConsumableActionPanel->isVisible()) {
        mConsumableActionPanel->setStyleSheet(
            "background:rgba(42,18,20,235); border:2px solid #ff6a6a; border-radius:8px;"
            );
        QTimer::singleShot(260, this, [this]() {
            if (mConsumableActionPanel)
                mConsumableActionPanel->setStyleSheet(
                    "background:rgba(18,23,26,230); border:2px solid #2b3135; border-radius:8px;"
                    );
        });
    }
    if (mHandCountLabel) {
        mHandCountLabel->setDefaultTextColor(QColor("#ff8080"));
        QTimer::singleShot(400, this, [this]() {
            if (mHandCountLabel) mHandCountLabel->setDefaultTextColor(QColor("#aaddaa"));
        });
    }
}

void MainWindow::animateConsumableUseThen(int idx, std::function<void()> after)
{
    if (idx < 0 || idx >= mConsumableItems.size() || !mConsumableItems[idx]) {
        if (after) after();
        return;
    }

    auto *item = mConsumableItems[idx];
    QPointer<ConsumableItem> guard(item);
    if (mConsumableActionPanel) mConsumableActionPanel->hide();
    mSelectedConsumableIdx = -1;
    item->setEnabled(false);
    item->setZValue(780);
    item->setTransformOriginPoint(TOP_SLOT_W / 2.0, TOP_SLOT_H / 2.0);

    // 行星牌 / 黑洞:对齐原版 card.lua:1264 use_consumeable —— 卡牌先抬起,然后跟
    // common_events.lua:464 level_up_hand 的三拍同步做 3 次 juice + tarot1。
    // 三拍结束后再触发 useConsumable,让卡牌一直可见到整段升级演出走完才被移除。
    const auto &cs = mGameState->consumables();
    const bool isPlanetLike = (idx >= 0 && idx < cs.size())
        && (kindOf(cs[idx].type) == ConsumableKind::Planet
            || cs[idx].type == ConsumableType::Spectral_BlackHole);

    auto *group = new QParallelAnimationGroup(this);
    auto *posAnim = new QPropertyAnimation(item, "pos", group);
    posAnim->setDuration(170);
    posAnim->setStartValue(item->pos());
    posAnim->setEndValue(item->pos() + QPointF(0, -42));
    posAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto *scaleAnim = new QPropertyAnimation(item, "scale", group);
    scaleAnim->setDuration(170);
    scaleAnim->setStartValue(item->scale());
    scaleAnim->setEndValue(1.13);
    scaleAnim->setEasingCurve(QEasingCurve::OutCubic);

    group->addAnimation(posAnim);
    group->addAnimation(scaleAnim);
    connect(group, &QParallelAnimationGroup::finished, this, [this, group, guard, after, isPlanetLike]() {
        if (guard) guard->setEnabled(true);
        if (!isPlanetLike) {
            if (after) after();
            group->deleteLater();
            return;
        }
        // 行星 / 黑洞:把抬起后的 ConsumableItem 从 mConsumableItems 摘出,这样紧接着触发
        // 的 useConsumable -> consumablesChanged -> refreshConsumableSlots 不会把它一并删掉。
        // 立即调用 after(),让 playHandLevelUpAnimation 与卡牌动画同步开演;detached 的
        // "幽灵"卡片在演出期间做 3 次 juice + tarot1,1200ms 后淡出销毁。
        if (guard) {
            int slot = mConsumableItems.indexOf(guard.data());
            if (slot >= 0) mConsumableItems.removeAt(slot);
            guard->setAcceptedMouseButtons(Qt::NoButton);
            guard->setEnabled(false);
        }
        if (after) after();
        if (!guard) { group->deleteLater(); return; }

        const int beatDelays[3] = { 80, 360, 660 };
        for (int beat = 0; beat < 3; ++beat) {
            QPointer<ConsumableItem> g2 = guard;
            QTimer::singleShot(scaledDelay(beatDelays[beat]), this, [g2]() {
                if (g2) g2->juiceUp(1.18, 220);
                AudioManager::instance()->play(QStringLiteral("tarot1"), 1.0, 1.0);
            });
        }
        // playHandLevelUpAnimation 总长 ~1180ms,1200ms 后开始淡出兜住最后一次 juice 的 down 段。
        QPointer<ConsumableItem> g3 = guard;
        QTimer::singleShot(scaledDelay(1200), this, [this, g3]() {
            if (!g3) return;
            auto *fade = new QVariantAnimation(this);
            fade->setDuration(scaledDelay(260));
            fade->setStartValue(1.0);
            fade->setEndValue(0.0);
            QPointer<ConsumableItem> gg = g3;
            connect(fade, &QVariantAnimation::valueChanged, this, [gg](const QVariant &v) {
                if (gg) gg->setOpacity(v.toDouble());
            });
            connect(fade, &QVariantAnimation::finished, this, [this, gg]() {
                if (!gg) return;
                if (gg->scene()) mScene->removeItem(gg.data());
                gg->deleteLater();
            });
            fade->start(QAbstractAnimation::DeleteWhenStopped);
        });
        group->deleteLater();
    });
    group->start();
}

void MainWindow::spawnShopPlanetUseFloater(int consumableType, const QPoint &globalCenter)
{
    // 把"购买并使用"的星球/黑洞,在它原来在商店里的位置生成一张幽灵 ConsumableItem,
    // 与消耗牌槽内 use 的动画对齐:抬起 + 3 拍 juice + tarot1 + 淡出。整段时长 ~1.5s,
    // 期间侧栏 playHandLevelUpAnimation 也在跑——两边节奏同步。
    if (!mView || !mScene) return;
    auto type = static_cast<ConsumableType>(consumableType);
    Consumable c = createConsumable(type);

    auto *floater = new ConsumableItem(c);
    const QPoint viewPt = mView->mapFromGlobal(globalCenter);
    const QPointF scenePt = mView->mapToScene(viewPt);
    floater->setPos(scenePt - QPointF(TOP_SLOT_W / 2.0, TOP_SLOT_H / 2.0));
    floater->setZValue(800);
    floater->setEnabled(false);
    floater->setAcceptedMouseButtons(Qt::NoButton);
    floater->setAcceptHoverEvents(false);
    floater->setTransformOriginPoint(TOP_SLOT_W / 2.0, TOP_SLOT_H / 2.0);
    mScene->addItem(floater);

    // 抬升:与消耗牌槽内 animateConsumableUseThen 同样 170ms / 上移 42px / scale 1.13。
    QPointer<ConsumableItem> guard(floater);
    auto *group = new QParallelAnimationGroup(this);
    auto *posAnim = new QPropertyAnimation(floater, "pos", group);
    posAnim->setDuration(scaledDelay(170));
    posAnim->setStartValue(floater->pos());
    posAnim->setEndValue(floater->pos() + QPointF(0, -42));
    posAnim->setEasingCurve(QEasingCurve::OutCubic);
    auto *scaleAnim = new QPropertyAnimation(floater, "scale", group);
    scaleAnim->setDuration(scaledDelay(170));
    scaleAnim->setStartValue(1.0);
    scaleAnim->setEndValue(1.13);
    scaleAnim->setEasingCurve(QEasingCurve::OutCubic);
    group->addAnimation(posAnim);
    group->addAnimation(scaleAnim);
    group->start(QAbstractAnimation::DeleteWhenStopped);

    const int beatDelays[3] = { 80, 360, 660 };
    for (int beat = 0; beat < 3; ++beat) {
        QPointer<ConsumableItem> g2 = guard;
        QTimer::singleShot(scaledDelay(170 + beatDelays[beat]), this, [g2]() {
            if (g2) g2->juiceUp(1.18, 220);
            AudioManager::instance()->play(QStringLiteral("tarot1"), 1.0, 1.0);
        });
    }
    QTimer::singleShot(scaledDelay(170 + 1200), this, [this, guard]() {
        if (!guard) return;
        auto *fade = new QVariantAnimation(this);
        fade->setDuration(scaledDelay(260));
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        QPointer<ConsumableItem> gg = guard;
        connect(fade, &QVariantAnimation::valueChanged, this, [gg](const QVariant &v) {
            if (gg) gg->setOpacity(v.toDouble());
        });
        connect(fade, &QVariantAnimation::finished, this, [this, gg]() {
            if (!gg) return;
            if (gg->scene()) mScene->removeItem(gg.data());
            gg->deleteLater();
        });
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void MainWindow::onConsumableClicked(ConsumableItem *item, Qt::MouseButton btn)
{
    onConsumablePressed(item, btn);
}

void MainWindow::onConsumablePressed(ConsumableItem *item, Qt::MouseButton btn)
{
    int idx = mConsumableItems.indexOf(item);
    if (idx < 0) return;

    if (btn == Qt::RightButton) {
        mGameState->sellConsumable(idx);
        QTimer::singleShot(200, this, []() {
            AudioManager::instance()->play(QStringLiteral("coin2"), 1.0, 1.0);
            AudioManager::instance()->play(QStringLiteral("coin1"), 1.0, 1.0);
        });
        hideConsumableAction();
        if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
        return;
    }

    if (mPackOpenWidget && mPackOpenWidget->isVisible() && !mPendingPackHand.isEmpty()) {
        QVector<int> packSel = mPackOpenWidget->selectedHandIndices();
        const ConsumableType type = mGameState->consumables()[idx].type;
        const QString useSound = soundForConsumable(type);
        animateConsumableUseThen(idx, [this, idx, packSel, type, useSound]() {
            const QVector<CardData> packBefore = mPendingPackHand;
            const QVector<Joker> jokersBefore = mGameState->jokers();
            const QVector<Consumable> consumablesBefore = mGameState->consumables();
            const int goldBefore = mGameState->gold();
            if (mGameState->useConsumableOnPackHand(idx, packSel, mPendingPackHand)) {
                const bool handled = playOriginalConsumableAudio(
                    this,
                    type,
                    packSel,
                    packBefore,
                    mPendingPackHand,
                    jokersBefore,
                    mGameState->jokers(),
                    consumablesBefore,
                    mGameState->consumables(),
                    goldBefore,
                    mGameState->gold(),
                    true);
                if (!handled && !useSound.isEmpty())
                    AudioManager::instance()->play(useSound, 1.0, 1.0);
                mPackOpenWidget->setPackHand(mPendingPackHand);
                mPackOpenWidget->setInventoryConsumables(mGameState->consumables());
                refreshConsumableSlots();
                refreshGold();
                refreshCounters();
                if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
            } else {
                AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
                flashConsumableActionError();
                refreshConsumableSlots();
            }
        });
        return;
    }

    if (btn == Qt::LeftButton) {
        // 再次点击已选中的消耗牌 → 取消选中（与手牌选中行为一致）。
        if (idx == mSelectedConsumableIdx) {
            AudioManager::instance()->play(QStringLiteral("cardSlide2"), 1.0, 0.3);
            hideConsumableAction();
        } else {
            showConsumableAction(idx);
        }
    }
}

void MainWindow::onConsumableDragMoved(ConsumableItem *item, QPointF scenePos)
{
    int from = mConsumableItems.indexOf(item);
    if (from < 0) return;
    if (mConsumableActionPanel) mConsumableActionPanel->hide();
    mSelectedConsumableIdx = -1;

    int n = mConsumableItems.size();
    if (n <= 1) {
        item->setZValue(700);
        return;
    }

    int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
    int totalW = TOP_SLOT_W + qMax(0, visualSlots - 1) * (TOP_SLOT_W + 14);
    int startX = mSceneW - 40 - totalW;
    int step = overlappedCardStep(totalW, TOP_SLOT_W, n, TOP_SLOT_W + 14);

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + TOP_SLOT_W / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (to == mLastConsumableDragTo) {
        item->setZValue(700);
        return;
    }
    mLastConsumableDragTo = to;

    QVector<ConsumableItem*> visual = mConsumableItems;
    visual.removeAt(from);
    visual.insert(to, item);
    for (int vi = 0; vi < visual.size(); ++vi) {
        ConsumableItem *ci = visual[vi];
        if (ci == item) continue;
        int x = startX + vi * step;
        int y = JOKER_Y + 18;
        ci->setZValue(30 + vi);
        ci->moveTo(QPointF(x, y), 60);
    }
    item->setZValue(700);
}

void MainWindow::onConsumableDragReleased(ConsumableItem *item, QPointF scenePos)
{
    mLastConsumableDragTo = -1;
    int from = mConsumableItems.indexOf(item);
    if (from < 0) { refreshConsumableSlots(); return; }
    int n = mConsumableItems.size();
    if (n <= 1) { refreshConsumableSlots(); return; }

    int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
    int totalW = TOP_SLOT_W + qMax(0, visualSlots - 1) * (TOP_SLOT_W + 14);
    int startX = mSceneW - 40 - totalW;
    int step = overlappedCardStep(totalW, TOP_SLOT_W, n, TOP_SLOT_W + 14);

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + TOP_SLOT_W / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (from != to) {
        AudioManager::instance()->play(QStringLiteral("cardSlide1"), audioPitchJitter(0.03), 0.62);
        mPendingConsumableReorder = {from, to};
        mGameState->moveConsumable(from, to);
        mPendingConsumableReorder = {-1, -1};
    } else refreshConsumableSlots();
}

void MainWindow::onPackChoiceMade(int chosenIdx, QVector<int> selectedPackHandIdx)
{
    const bool buffoonChoice = (chosenIdx >= 0 && mPendingPack.kind == PackKind::Buffoon);
    if (chosenIdx >= 0) {
        ConsumableType usedType = ConsumableType::Tarot_Fool;
        const bool consumableChoice =
            (mPendingPack.kind == PackKind::Arcana
             || mPendingPack.kind == PackKind::Celestial
             || mPendingPack.kind == PackKind::Spectral)
            && chosenIdx < mPendingPack.consumables.size();
        if (consumableChoice)
            usedType = mPendingPack.consumables[chosenIdx];

        const QVector<CardData> packBefore = mPendingPackHand;
        const QVector<Joker> jokersBefore = mGameState->jokers();
        const QVector<Consumable> consumablesBefore = mGameState->consumables();
        const int goldBefore = mGameState->gold();

        const bool ok = mGameState->applyPackChoice(mPendingPack, chosenIdx,
                                                    selectedPackHandIdx, mPendingPackHand);
        if (ok) {
            if (mPendingPack.kind == PackKind::Standard || mPendingPack.kind == PackKind::Buffoon) {
                AudioManager::instance()->play(QStringLiteral("card1"), 0.8, 0.6);
                AudioManager::instance()->play(QStringLiteral("generic1"), 1.0, 1.0);
            } else if (consumableChoice) {
                const QString fallback = soundForConsumable(usedType);
                const bool handled = playOriginalConsumableAudio(
                    this,
                    usedType,
                    selectedPackHandIdx,
                    packBefore,
                    mPendingPackHand,
                    jokersBefore,
                    mGameState->jokers(),
                    consumablesBefore,
                    mGameState->consumables(),
                    goldBefore,
                    mGameState->gold(),
                    true);
                if (!handled && !fallback.isEmpty())
                    AudioManager::instance()->play(fallback, 1.0, 1.0);
            }
        } else {
            AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
        }
        mPackOpenWidget->setPackHand(mPendingPackHand);
        mPackOpenWidget->setInventoryConsumables(mGameState->consumables());
    }
    refreshConsumableSlots();
    // 小丑包选择后，jokersChanged 已经触发带飞入起点的 refreshJokerSlots。
    // 这里不要再立即刷新一次，否则会把刚开始的飞行动画删掉，看起来像“先生成再滑入”。
    if (!buffoonChoice) refreshJokerSlots();
    refreshGold();
    refreshCounters();
    if (mPackOpenWidget && mPackOpenWidget->isVisible())
        mPackOpenWidget->setFreeJokerSlots(mGameState->jokerSlots() - mGameState->jokers().size());
    if (mShopWidget) mShopWidget->refresh();
}

void MainWindow::onInventoryConsumableUseRequested(int inventoryIdx, QVector<int> selectedPackHandIdx)
{
    const bool hasConsumable = inventoryIdx >= 0 && inventoryIdx < mGameState->consumables().size();
    const ConsumableType type = hasConsumable
        ? mGameState->consumables()[inventoryIdx].type
        : ConsumableType::Tarot_Fool;
    const QString useSound = hasConsumable
        ? soundForConsumable(type)
        : QStringLiteral("cancel");
    const QVector<CardData> packBefore = mPendingPackHand;
    const QVector<Joker> jokersBefore = mGameState->jokers();
    const QVector<Consumable> consumablesBefore = mGameState->consumables();
    const int goldBefore = mGameState->gold();
    if (mGameState->useConsumableOnPackHand(inventoryIdx, selectedPackHandIdx, mPendingPackHand)) {
        const bool handled = playOriginalConsumableAudio(
            this,
            type,
            selectedPackHandIdx,
            packBefore,
            mPendingPackHand,
            jokersBefore,
            mGameState->jokers(),
            consumablesBefore,
            mGameState->consumables(),
            goldBefore,
            mGameState->gold(),
            true);
        if (!handled && !useSound.isEmpty())
            AudioManager::instance()->play(useSound, 1.0, 1.0);
        mPackOpenWidget->setPackHand(mPendingPackHand);
        mPackOpenWidget->setInventoryConsumables(mGameState->consumables());
        refreshConsumableSlots();
        refreshGold();
        refreshCounters();
    } else {
        AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
    }
}

void MainWindow::onPackFinished()
{
    if (!mPendingPackHand.isEmpty())
        mGameState->returnPackHand(mPendingPackHand);
    mPendingPackHand.clear();
    refreshCounters();
    refreshGold();

    if (mPackFromTag) {
        mPackFromTag = false;
        AudioManager::instance()->setPitchMod(1.0);
        AudioManager::instance()->setDesiredMusic(QStringLiteral("music1"));
        showBlindSelectAfterTagPack();
        if (!mQueuedTagPacks.isEmpty()) {
            const PackKind next = mQueuedTagPacks.takeFirst();
            QTimer::singleShot(260, this, [this, next]() {
                consumeImmediateTagPack(next);
            });
            return;
        }
        return;
    }

    if (mShopWidget) {
        showShopOverlay();
    }
}

void MainWindow::onSelectBlindClicked()
{
    AudioManager::instance()->setPitchMod(1.0);
    mGameState->selectCurrentBlind();
}

void MainWindow::onLeaveShopClicked()
{
    mShopWidget->hide();
    AudioManager::instance()->setPitchMod(1.0);
    AudioManager::instance()->setDesiredMusic(QStringLiteral("music1"));
    mGameState->leaveShop();
    refreshConsumableSlots();
}

QRect MainWindow::lowerOverlayRect() const
{
    if (!mPlayPage) return QRect();
    const int y = dp(JOKER_Y + JOKER_H + 10);
    // 牌组在场景里贴右边，按 fitInView 的实际缩放比例换算到 widget 像素宽度。
    // 之前用固定 dp(CARD_W + 150)，在卡牌放大后会把 BlindSelect 的右侧 Boss 卡裁掉。
    const double sceneScale = mSceneH > 0 ? double(mPlayPage->height()) / mSceneH : 1.0;
    const int deckReserve = qMax(0, int(std::round((CARD_W + 60) * sceneScale))) + dp(16);
    return QRect(0, y, qMax(dp(560), mPlayPage->width() - deckReserve), qMax(0, mPlayPage->height() - y));
}

QRect MainWindow::shopOverlayRect() const
{
    if (!mPlayPage) return QRect();

    // 商店只能占用“小丑槽位下方”的区域。上一版为了防止商店底部被裁，
    // 把 overlay 顶到了屏幕上方 8% 的位置；但 QWidget 的透明区域也会吃掉鼠标事件，
    // 于是商店打开时小丑牌 hover / 拖动都会失效。这里重新以 lowerOverlayRect()
    // 为基准，让小丑区域仍然归 QGraphicsView 接收鼠标。商店显示不全的问题交给
    // ShopWidget 内部紧凑布局处理，而不是用透明 overlay 盖住小丑牌。
    const QRect base = lowerOverlayRect();
    const int leftMargin = dp(12);
    const int rightMargin = dp(8);
    const int x = leftMargin;
    const int y = base.y();
    const int w = qMax(dp(640), base.width() - leftMargin - rightMargin);
    const int h = qMax(0, base.height());
    return QRect(x, y, w, h);
}

void MainWindow::showShopOverlay()
{
    AudioManager::instance()->setPitchMod(1.0);
    AudioManager::instance()->setDesiredMusic(QStringLiteral("music4"));
    QTimer::singleShot(320, this, []() {
        AudioManager::instance()->play(QStringLiteral("cardFan2"), 1.0, 1.0);
    });
    if (mDynamicBg) mDynamicBg->setMood(DynamicBackgroundItem::Mood::Shop);

    // 用户期望：进商店后侧栏分数清零、出牌/弃牌显示成下一回合即将的开局值。
    // 原版商店阶段确实把面板上的"本回合数字"切到"下一回合预览"。手动把显示值刷掉，
    // 而 mScore/mHandsLeft 等真正的 state 等 startBlind() 时再重置。
    if (mLblScore)    setLabelScaledText(mLblScore, "0", uiPx(38));
    if (mLblTarget)   mLblTarget->setText("✳ 0");
    // 停掉残留的进度条动画再写值——否则上一手得分的 setValue 动画会把进度条从 0 又拉回去。
    if (mScoreCountAnim) { mScoreCountAnim->stop(); mScoreCountAnim->deleteLater(); mScoreCountAnim = nullptr; }
    if (mScoreProgressAnim && mScoreProgressAnim->state() == QAbstractAnimation::Running)
        mScoreProgressAnim->stop();
    if (mScoreProgressBar) { mScoreProgressBar->setValue(0); mScoreProgressBar->setFormat("0%"); }
    if (mLblChips)    setLabelScaledText(mLblChips, "0", uiPx(42));
    if (mLblMult)     setLabelScaledText(mLblMult,  "0", uiPx(42));
    mDisplayedChips = 0;
    mDisplayedMult  = 0;
    // 出牌/弃牌：显示下一回合预览的开局值（含小丑/优惠券修正）。
    if (mLblHands && mLblDiscards) {
        int previewHands = qMax(1, Constants::INITIAL_HANDS + mGameState->extraHandsPerRoundPreview());
        int previewDiscards = qMax(0, Constants::INITIAL_DISCARDS + mGameState->extraDiscardsPerRoundPreview());
        mLblHands->setText(QString::number(previewHands));
        mLblDiscards->setText(QString::number(previewDiscards));
    }

    if (!mShopWidget || !mPlayPage) return;
    mShopWidget->refresh();
    mShopWidget->setGeometry(shopOverlayRect());
    mShopWidget->raise();
    mShopWidget->show();
    animateShopEntrance();

    // 原版 tag.lua：shop_start / shop_final_pass / voucher_add / tag_add 类 tag 在
    // 进入商店时触发并消耗。这里在 shop overlay 显示时统一移除它们的图标。
    removeObtainedTags(TagType::Coupon, 99);
    removeObtainedTags(TagType::D6, 99);
    removeObtainedTags(TagType::Voucher, 99);
    removeObtainedTags(TagType::Foil, 99);
    removeObtainedTags(TagType::Holographic, 99);
    removeObtainedTags(TagType::Polychrome, 99);
    removeObtainedTags(TagType::Negative, 99);
    removeObtainedTags(TagType::Uncommon, 99);
    removeObtainedTags(TagType::Rare, 99);
    // Investment Tag 在击败 Boss 时通过结算给 $25。打到 Boss 商店后移除。
    if (mGameState->blindType() == BlindType::Boss)
        removeObtainedTags(TagType::Investment, 99);
}

void MainWindow::animateShopEntrance()
{
    if (!mShopWidget || !mPlayPage) return;

    // 取消可能正在执行的旧动画，避免位置/透明度抖动。
    if (auto *oldGroup = mShopWidget->findChild<QParallelAnimationGroup*>("ShopEntranceAnim"))
        oldGroup->stop();
    if (auto *oldEffect = qobject_cast<QGraphicsOpacityEffect*>(mShopWidget->graphicsEffect()))
        oldEffect->deleteLater();

    const QRect end = shopOverlayRect();
    mShopWidget->setGeometry(end);
    const QPoint endPos = end.topLeft();
    const QPoint startPos(endPos.x(), mPlayPage->height() + 20);
    mShopWidget->move(startPos);

    auto *posAnim = new QPropertyAnimation(mShopWidget, "pos");
    posAnim->setDuration(320);
    posAnim->setStartValue(startPos);
    posAnim->setEndValue(endPos);
    posAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto *fade = new QGraphicsOpacityEffect(mShopWidget);
    fade->setOpacity(0.0);
    mShopWidget->setGraphicsEffect(fade);
    auto *fadeAnim = new QPropertyAnimation(fade, "opacity");
    fadeAnim->setDuration(260);
    fadeAnim->setStartValue(0.0);
    fadeAnim->setEndValue(1.0);
    fadeAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto *group = new QParallelAnimationGroup(mShopWidget);
    group->setObjectName("ShopEntranceAnim");
    group->addAnimation(posAnim);
    group->addAnimation(fadeAnim);
    QPointer<ShopWidget> guard(mShopWidget);
    connect(group, &QParallelAnimationGroup::finished, this, [guard]() {
        if (!guard) return;
        // 移除 opacity effect，避免之后绘制时一直走 graphics effect 管线（会让阴影/字号偏色）。
        if (auto *eff = qobject_cast<QGraphicsOpacityEffect*>(guard->graphicsEffect()))
            eff->deleteLater();
    });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

// 替代计分链里的 QTimer::singleShot：创建可暂停的定时器。
void MainWindow::scheduleGame(int delayMs, std::function<void()> fn)
{
    QTimer *t = new QTimer(this);
    t->setSingleShot(true);
    // 设置里的"倍速"通过这一处生效:所有计分链的 delay 都按当前倍率缩短。
    t->setInterval(scaledDelay(delayMs));
    connect(t, &QTimer::timeout, this, [this, t, fn]() {
        mGameTimers.removeAll(t);
        t->deleteLater();
        fn();
    });
    mGameTimers.append(t);
    if (mGamePaused) t->setProperty("pauseRemain", t->interval());
    else             t->start();
}

void MainWindow::pauseGameProcesses()
{
    if (mGamePaused) return;
    mGamePaused = true;

    // 1) 可暂停定时器：记录剩余时间后停表。
    for (auto &t : mGameTimers) {
        if (!t) continue;
        int rem = t->isActive() ? t->remainingTime() : t->interval();
        t->setProperty("pauseRemain", qMax(0, rem));
        t->stop();
    }
    // 2) 回合总分计数动画。只暂停这一个明确跟踪的动画。
    //    不碰火焰计时器与动态背景（QOpenGLWidget）——在其上叠加 widget 覆盖层时
    //    停掉 GL 渲染会在部分驱动上触发上下文重建并崩溃（见 mainwindow.h 注释）。
    //    它们只是环境动画，被半透明菜单遮住，不暂停也无妨。
    if (mScoreCountAnim && mScoreCountAnim->state() == QAbstractAnimation::Running)
        mScoreCountAnim->pause();
}

void MainWindow::resumeGameProcesses()
{
    if (!mGamePaused) return;
    mGamePaused = false;

    for (auto &t : mGameTimers) {
        if (!t) continue;
        t->start(qMax(0, t->property("pauseRemain").toInt()));
    }
    if (mScoreCountAnim && mScoreCountAnim->state() == QAbstractAnimation::Paused)
        mScoreCountAnim->resume();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    // 火焰已改用 GPU 渲染 FlameShaderWidget；之前在这里拦截 mChipFlame/mMultFlame
    // 的 Paint 事件用 BalatroShaders::paintFlame 走 CPU 渲染，现在不再需要。
    if (obj == mPlayPage && ev->type() == QEvent::Resize) {
        QRect r = mPlayPage->rect();
        if (mDynamicBg) {
            mDynamicBg->setGeometry(r);
            mDynamicBg->setSceneSize(r.width(), r.height());
            mDynamicBg->lower();
        }
        updateSceneSize();
        if (mBlindSelectWidget) { mBlindSelectWidget->setGeometry(lowerOverlayRect()); if (mBlindSelectWidget->isVisible()) mBlindSelectWidget->raise(); }
        if (mRoundEndOverlay)   { mRoundEndOverlay  ->setGeometry(r);                 if (mRoundEndOverlay->isVisible())   mRoundEndOverlay->raise(); }
        if (mShopWidget)        { mShopWidget       ->setGeometry(shopOverlayRect()); if (mShopWidget->isVisible())        mShopWidget->raise(); }
        if (mPackOpenWidget)    { mPackOpenWidget   ->setGeometry(lowerOverlayRect()); if (mPackOpenWidget->isVisible())    mPackOpenWidget->raise(); }
        if (mDeckViewWidget)    { mDeckViewWidget   ->setGeometry(r);                 if (mDeckViewWidget->isVisible())    mDeckViewWidget->raise(); }
        if (mOptionsOverlay && centralWidget()) {
            mOptionsOverlay->setGeometry(centralWidget()->rect());
            if (mOptionsOverlay->isVisible()) mOptionsOverlay->raise();
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}


void MainWindow::setBackgroundMoodForPhase()
{
    if (!mDynamicBg || !mGameState) return;
    switch (mGameState->phase()) {
    case GamePhase::BlindSelect:
        mDynamicBg->setMood(DynamicBackgroundItem::Mood::BlindSelect);
        break;
    case GamePhase::Shop:
        mDynamicBg->setMood(DynamicBackgroundItem::Mood::Shop);
        break;
    case GamePhase::Blind:
        // Boss 盲注用专属红色底（对齐原版 G.C.BLIND.Boss）；
        // 大/小盲注沿用默认绿色。
        if (mGameState->blindType() == BlindType::Boss)
            mDynamicBg->setMood(DynamicBackgroundItem::Mood::Boss);
        else
            mDynamicBg->setMood(DynamicBackgroundItem::Mood::Default);
        break;
    default:
        mDynamicBg->setMood(DynamicBackgroundItem::Mood::Default);
        break;
    }
}

void MainWindow::setBackgroundMoodForPack(PackKind kind)
{
    if (!mDynamicBg) return;
    switch (kind) {
    case PackKind::Arcana:    mDynamicBg->setMood(DynamicBackgroundItem::Mood::Tarot); break;
    case PackKind::Spectral:  mDynamicBg->setMood(DynamicBackgroundItem::Mood::Spectral); break;
    case PackKind::Celestial: mDynamicBg->setMood(DynamicBackgroundItem::Mood::Celestial); break;
    case PackKind::Buffoon:   mDynamicBg->setMood(DynamicBackgroundItem::Mood::Buffoon); break;
    case PackKind::Standard:  mDynamicBg->setMood(DynamicBackgroundItem::Mood::Standard); break;
    }
}

void MainWindow::openImmediateTagPack(PackKind kind)
{
    PackSize size = PackSize::Mega;
    if (kind == PackKind::Spectral) size = PackSize::Normal;

    QVector<JokerType> owned;
    for (const Joker &j : mGameState->jokers()) owned.append(j.type);

    ConsumableType telescopePlanet = ConsumableType::Planet_Pluto;
    const bool telescopeActive =
        mGameState->hasVoucher(VoucherType::Telescope) &&
        mGameState->telescopePlanetForPack(telescopePlanet);
    mPendingPack = generatePackContent(kind, size,
                                       mGameState->hasVoucher(VoucherType::OmenGlobe),
                                       telescopeActive,
                                       telescopePlanet,
                                       owned,
                                       false,
                                       mGameState->grosMichelExtinct());
    mPendingPack.spriteVariant =
        QRandomGenerator::global()->bounded(qMax(1, packSpriteVariantCount(kind, size)));
    mPendingPackHand.clear();
    if (kind == PackKind::Arcana || kind == PackKind::Spectral) {
        mPendingPackHand = mGameState->drawPackHand();
        refreshCounters();
    }

    mPackFromTag = true;
    AudioManager::instance()->setPitchMod(1.0);
    AudioManager::instance()->setDesiredMusic(musicTrackForPack(kind));
    playOriginalMaterializeSound(this);
    playSoundLater(this, 400, QStringLiteral("explosion_buildup1"), 1.0, 1.0);
    playSoundLater(this, 1570, QStringLiteral("explosion_release1"), 1.0, 1.0);
    playOriginalMaterializeSound(this, 1700);
    setBackgroundMoodForPack(kind);
    if (mBlindSelectWidget) mBlindSelectWidget->hide();
    if (mShopWidget) mShopWidget->hide();

    int freeJoker = mGameState->jokerSlots() - mGameState->jokers().size();
    mPackOpenWidget->open(mPendingPack, mPendingPackHand,
                          mGameState->consumables(), freeJoker);
    mPackOpenWidget->setGeometry(lowerOverlayRect());
}

void MainWindow::removeObtainedPackTag(PackKind kind)
{
    switch (kind) {
    case PackKind::Standard:  removeObtainedTag(TagType::Standard); break;
    case PackKind::Arcana:    removeObtainedTag(TagType::Charm); break;
    case PackKind::Celestial: removeObtainedTag(TagType::Meteor); break;
    case PackKind::Buffoon:   removeObtainedTag(TagType::Buffoon); break;
    case PackKind::Spectral:  removeObtainedTag(TagType::Ethereal); break;
    }
}

void MainWindow::consumeImmediateTagPack(PackKind kind)
{
    // 原版 Tag:yep() 会先播放标签确认反馈，随后标签从右下角消失并执行 func。
    // 卡包类 tag 也保留这个流程：先让玩家看到获得的 tag，再消费 tag 打开包。
    playOriginalTagYepSound(this, 700);
    QTimer::singleShot(700, this, [this, kind]() {
        removeObtainedPackTag(kind);
        openImmediateTagPack(kind);
    });
}

void MainWindow::showBlindSelectAfterTagPack()
{
    if (mDynamicBg) mDynamicBg->setMood(DynamicBackgroundItem::Mood::BlindSelect);
    if (mBlindSelectWidget && mPlayPage) {
        mBlindSelectWidget->refresh();
        mBlindSelectWidget->setGeometry(lowerOverlayRect());
        mBlindSelectWidget->show();
        mBlindSelectWidget->raise();
        mBlindSelectWidget->arrangeCards(false);
    }
}

void MainWindow::onBlindSelectEntered()
{
    AudioManager::instance()->setPitchMod(1.0);
    AudioManager::instance()->setDesiredMusic(QStringLiteral("music1"));
    if (mDynamicBg) mDynamicBg->setMood(DynamicBackgroundItem::Mood::BlindSelect);
    setContextPage(0);
    setPlayPhaseVisible(false);
    clearPlayedCards();
    if (mShopWidget) mShopWidget->hide();
    if (mPackOpenWidget) mPackOpenWidget->hide();
    if (mRoundEndOverlay) mRoundEndOverlay->hide();
    if (mDeckViewWidget) mDeckViewWidget->hide();
    if (!mBlindSelectWidget || !mPlayPage) return;

    const bool skipped = mGameState->justSkipped();
    mBlindSelectWidget->hide();
    mBlindSelectWidget->setGeometry(lowerOverlayRect());
    mBlindSelectWidget->refresh();

    if (!skipped) {
        mBlindSelectWidget->prepareEntrancePositions();
    } else {
        mBlindSelectWidget->arrangeCards(false);
    }

    mBlindSelectWidget->raise();
    mBlindSelectWidget->show();

    QTimer::singleShot(0, this, [this, skipped]() {
        if (!mBlindSelectWidget || !mPlayPage) return;
        if (mBlindSelectWidget->geometry() != lowerOverlayRect())
            mBlindSelectWidget->setGeometry(lowerOverlayRect());
        mBlindSelectWidget->arrangeCards(!skipped);
    });
}

void MainWindow::onBlindStarted()
{
    AudioManager::instance()->setPitchMod(1.0);
    AudioManager::instance()->setDesiredMusic(
        mGameState->blindType() == BlindType::Boss ? QStringLiteral("music5")
                                                   : QStringLiteral("music1"));
    AudioManager::instance()->play(QStringLiteral("chips1"), originalRandomPitch(0.55, 0.1), 0.42);
    AudioManager::instance()->play(QStringLiteral("gold_seal"), originalRandomPitch(1.85, 0.1), 0.26);
    if (mGameState->blindType() == BlindType::Boss && mGameState->hasJokerType(JokerType::Chicot)) {
        AudioManager::instance()->play(QStringLiteral("timpani"), 1.0, 1.0);
        playOriginalStatusGenericSound(this);
        playOriginalBlindWiggleSound(this);
    }
    if (mGameState->blindType() == BlindType::Boss
        && mGameState->bossEffect() == BossEffect::AmberAcorn
        && mGameState->jokers().size() > 1) {
        playSoundLater(this, 200, QStringLiteral("cardSlide1"), 0.85, 1.0);
        playSoundLater(this, 350, QStringLiteral("cardSlide1"), 1.15, 1.0);
        playSoundLater(this, 500, QStringLiteral("cardSlide1"), 1.0, 1.0);
    }
    // Boss 盲注切到专属红底；其余沿用默认绿底。
    if (mDynamicBg) setBackgroundMoodForPhase();
    clearFloatingScores();
    // 原版（tag.lua）：tag 在 inventory 持有，等真正的"使用时机"才消耗。
    // 之前在这里 clearObtainedTags() 会把所有未触发的 tag 一齐抹掉——
    // 用户能看到 tag 弹出 1 秒就消失，违反原版行为。
    // 这里只移除"开始一个新 blind 时本就要消耗"的 tag：
    //   - Boss Tag：在选 Boss 时已 reroll Boss，进入 Boss 战即消耗。
    //   - Juggle Tag：进入下一回合就生效（修改 mOneRoundHandSizeBonus）。
    //   - Investment Tag：在击败 Boss 时通过结算消耗（这里只补漏：进非 Boss 不消耗）。
    if (mGameState->blindType() == BlindType::Boss) removeObtainedTags(TagType::Boss, 99);
    removeObtainedTags(TagType::Juggle, 99);
    mHandY = mHandYNormal;
    if (mBlindSelectWidget) mBlindSelectWidget->hide();
    if (mShopWidget) mShopWidget->hide();
    if (mPackOpenWidget) mPackOpenWidget->hide();
    if (mRoundEndOverlay) mRoundEndOverlay->hide();
    if (mDeckViewWidget) mDeckViewWidget->hide();
    setContextPage(1);
    setPlayPhaseVisible(true);

    refreshHand();
    refreshScore();
    refreshGold();
    refreshCounters();
    refreshJokerSlots();
    refreshConsumableSlots();
    clearPlayedCards();
    // 必须用 setLabelScaledText 重置字号——上一局如果分数很大，setLabelScaledText 会把字号
    // 自适应缩到很小（见函数实现），直接 setText("0") 不会还原字号，"0" 就会比初始小一圈，
    // 看上去像是 0 的位置偏了（实际上是字小了，靠 Left/Right 对齐后视觉位置不一样）。
    setLabelScaledText(mLblChips, "0", uiPx(42));
    setLabelScaledText(mLblMult,  "0", uiPx(42));
    mDisplayedChips = 0;
    mDisplayedMult  = 0;
    mScoringInProgress = false;
    resetScoreFlame();

    // 确保按钮回到 home 位置(可能上一局结束时按钮滑出去了)
    if (mBestPlayProxy && !mBestPlayBtnHome.isNull()) mBestPlayProxy->setPos(mBestPlayBtnHome);
    if (mPlayProxy && !mPlayBtnHome.isNull())    mPlayProxy->setPos(mPlayBtnHome);
    if (mSortProxy && !mSortBtnHome.isNull())    mSortProxy->setPos(mSortBtnHome);
    if (mDiscardProxy && !mDiscardBtnHome.isNull()) mDiscardProxy->setPos(mDiscardBtnHome);
    if (mForesightProxy && !mForesightBtnHome.isNull()) mForesightProxy->setPos(mForesightBtnHome);

    refreshCounters();
}

void MainWindow::animateCollectRoundCardsThen(std::function<void()> after)
{
    QVector<CardItem*> cards;
    for (auto *c : mHandCards) if (c) cards.append(c);
    for (auto *c : mPlayedCards) if (c) cards.append(c);

    QPointF deckPos(mSceneW - CARD_W - 60, mHandYScoring);
    const int duration = cards.isEmpty() ? 0 : 420;

    for (int i = 0; i < cards.size(); ++i) {
        CardItem *c = cards[i];
        c->setZValue(80 + i);
        c->moveTo(deckPos, duration);
        auto *fade = new QPropertyAnimation(c, "opacity", this);
        fade->setDuration(duration);
        fade->setStartValue(c->opacity());
        fade->setEndValue(0.0);
        fade->setEasingCurve(QEasingCurve::InQuad);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    }

    QTimer::singleShot(duration + 40, this, [this, after]() {
        clearPlayedCards();
        mGameState->collectRoundCardsToDeck();
        for (auto *c : mHandCards) c->setOpacity(1.0);
        refreshHand();
        refreshCounters();
        if (after) after();
    });
}

void MainWindow::onNextBlindClicked()
{
    clearFloatingScores();

    // “提现”按钮现在负责真正把本回合奖励打到金币上；
    // 回合胜利时只展示结算窗口，不提前修改金币数字。
    if (mRoundEndOverlay && mRoundEndOverlay->isVisible()) {
        mGameState->claimRoundPayout();
        AudioManager::instance()->play(QStringLiteral("coin7"), 1.0, 1.0);
        refreshGold();
    }

    auto enterShop = [this]() {
        setContextPage(2);
        setPlayPhaseVisible(false);
        showShopOverlay();
    };

    if (mRoundEndOverlay && mRoundEndOverlay->isVisible()) {
        mRoundEndOverlay->hideToBottom(enterShop);
    } else {
        enterShop();
    }
}

void MainWindow::onRoundWon(int blindReward, int handBonus, int interest)
{
    AudioManager::instance()->play(QStringLiteral("cardFan2"), 1.0, 1.0);
    refreshGold();

    int chipRow = 0;
    switch (mGameState->blindType()) {
    case BlindType::Small: chipRow = 0; break;
    case BlindType::Big:   chipRow = 1; break;
    case BlindType::Boss:  chipRow = bossChipRow(mGameState->bossEffect()); break;
    }

    // 击败盲注：侧边栏的 chip 走 dissolve 破碎效果。配合 round-end 滑入节奏。
    if (mCtxBlindChipImg) mCtxBlindChipImg->startDissolve();
    // 溶解动画 ~700ms 跑完后，把上下文区上滑成空白页（page 3），等待玩家按"提现"再切到 shop。
    // 700ms + 240 上滑动画 ≈ 940ms 让玩家看清 chip 破碎过程。
    QTimer::singleShot(720, this, [this]() {
        setContextPage(3);
    });

    const int pendingPayout = mGameState->pendingRoundPayout();
    const int extraBonus = pendingPayout - blindReward - handBonus - interest;
    mRoundEndOverlay->setData(
        chipRow,
        mGameState->targetScore(),
        blindReward,
        mGameState->handsLeft(),  handBonus,
        interest,
        extraBonus, pendingPayout
        );

    const int delay = mEndRoundAnimationDelay;
    mEndRoundAnimationDelay = 260;
    QTimer::singleShot(delay, this, [this]() {
        animateCollectRoundCardsThen([this]() {
            if (!mRoundEndOverlay || !mPlayPage) return;
            // 让结算面板水平居中时避开右下角牌堆按钮（≈ scene.CARD_W+80，按 scale 换算到 widget 像素）。
            const double scaleW = (mSceneW > 0)
                                      ? double(mPlayPage->width()) / double(mSceneW)
                                      : 1.0;
            const int deckReserve = int(std::round((CARD_W + 80) * scaleW));
            mRoundEndOverlay->setRightReserve(deckReserve);
            mRoundEndOverlay->showFromBottom(mPlayPage->rect());
        });
    });
}

void MainWindow::onPackBuyRequested(int slot)
{
    const auto boosterOffers = mGameState->shop().boosterOffers();
    const int packCost = (slot >= 0 && slot < boosterOffers.size()) ? boosterOffers[slot].cost : 0;
    if (!mGameState->buyPack(slot, mPendingPack)) {
        AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
        return;
    }
    AudioManager::instance()->setPitchMod(1.0);
    AudioManager::instance()->setDesiredMusic(musicTrackForPack(mPendingPack.kind));
    if (packCost != 0)
        AudioManager::instance()->play(QStringLiteral("coin1"), 1.0, 1.0);
    QTimer::singleShot(400, this, []() {
        AudioManager::instance()->play(QStringLiteral("explosion_buildup1"), 1.0, 1.0);
    });
    QTimer::singleShot(1570, this, []() {
        AudioManager::instance()->play(QStringLiteral("explosion_release1"), 1.0, 1.0);
    });
    playOriginalMaterializeSound(this, 1700);

    if (mShopWidget) {
        auto *anim = new QPropertyAnimation(mShopWidget, "pos", this);
        anim->setDuration(220);
        anim->setStartValue(mShopWidget->pos());
        anim->setEndValue(QPoint(mShopWidget->x(), mPlayPage ? mPlayPage->height() + 20 : mShopWidget->y() + 500));
        anim->setEasingCurve(QEasingCurve::InCubic);
        connect(anim, &QPropertyAnimation::finished, mShopWidget, &QWidget::hide);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    mPackFromTag = false;
    setBackgroundMoodForPack(mPendingPack.kind);

    mPendingPackHand.clear();
    if (mPendingPack.kind == PackKind::Arcana || mPendingPack.kind == PackKind::Spectral) {
        mPendingPackHand = mGameState->drawPackHand();
        refreshCounters();
    }

    int freeJoker = mGameState->jokerSlots() - mGameState->jokers().size();
    mPackOpenWidget->open(mPendingPack, mPendingPackHand,
                          mGameState->consumables(), freeJoker);
    QTimer::singleShot(0, this, [this]() {
        if (mPackOpenWidget)
            mPackOpenWidget->setGeometry(lowerOverlayRect());
        if (mDeckViewWidget && mPlayPage)
            mDeckViewWidget->setGeometry(mPlayPage->rect());
    });
}

void MainWindow::resetTransientOverlaysForNewRun()
{
    if (mShopWidget) {
        mShopWidget->hide();
        mShopWidget->move(mShopWidget->x(), mPlayPage ? mPlayPage->height() + 20 : mShopWidget->y());
    }
    if (mPackOpenWidget) {
        mPackOpenWidget->hide();
        mPendingPackHand.clear();
        mPackFromTag = false;
    }
    if (mRoundEndOverlay) mRoundEndOverlay->hide();
    if (mDeckViewWidget) mDeckViewWidget->hide();
    hideGameOverOverlay();
    hideJokerInfo();
    hideCardInfo();
    hideHoverTooltip();
    hideConsumableAction();
    clearFloatingScores();
    clearObtainedTags();
    clearPlayedCards();
    mSelected.clear();
    mShatteredPlayedIndices.clear();
    mGameOverHandled = false;
    mScoringInProgress = false;
    mEndRoundAnimationDelay = 260;
    resetScoreFlame();
    AudioManager::instance()->setPitchMod(1.0);
    AudioManager::instance()->setDesiredMusic(QStringLiteral("music1"));
    if (mDynamicBg) mDynamicBg->setMood(DynamicBackgroundItem::Mood::BlindSelect);
}

void MainWindow::setContextPage(int page)
{
    if (!mContextArea) return;
    if (page < 0 || page >= mContextArea->count()) return;
    const int cur = mContextArea->currentIndex();
    if (cur == page) return;

    // 切页前主动收起 hover 浮窗——否则 Qt 在 leave 事件之前就把旧 widget 隐藏，
    // 浮窗会"残留"显示到下一界面（用户反馈：进商店后看到上一界面的卡牌描述）。
    hideHoverTooltip();
    hideCardInfo();
    if (mJokerInfoProxy) mJokerInfoProxy->hide();
    if (mConsumableActionProxy) mConsumableActionProxy->hide();

    QWidget *oldW = mContextArea->currentWidget();
    QWidget *newW = mContextArea->widget(page);
    const int H = mContextArea->height();
    const int W = mContextArea->width();

    if (!oldW || !newW || H <= 0 || W <= 0) {
        mContextArea->setCurrentIndex(page);
        return;
    }

    // 取消上一段还没跑完的过渡，避免动画堆叠。
    if (mContextTransition) {
        mContextTransition->stop();
        mContextTransition->deleteLater();
        mContextTransition.clear();
    }

    // 两段顺序动画：① 旧页面先从屏幕中上滑到屏幕上方 (-H) ② 新页面再从屏幕上方 (-H) 下滑到位 (0)。
    oldW->setGeometry(0, 0, W, H);
    newW->setGeometry(0, -H, W, H);   // 起始藏在顶部之外
    newW->show();
    newW->raise();

    // 原版 Balatro 的 Moveable 用 exp_times.xy = exp(-50*dt) 的指数阻尼弹簧 + max_vel 上限
    // 推动 VT 追上 T（engine/moveable.lua:405-421, game.lua:2618-2624），观感是
    // "起步快、收尾软、略带回弹"。Qt 内建曲线里最接近的：
    //   - 退出：InBack —— 先小幅后退再加速冲出，模拟弹簧反向蓄力
    //   - 进入：OutBack —— 冲到位时略微越过再回弹，模拟弹簧落定
    QEasingCurve outCurve(QEasingCurve::InBack);
    outCurve.setOvershoot(1.4);
    QEasingCurve inCurve(QEasingCurve::OutBack);
    inCurve.setOvershoot(1.7);

    auto *oldAnim = new QPropertyAnimation(oldW, "pos");
    oldAnim->setDuration(220);
    oldAnim->setStartValue(QPoint(0, 0));
    oldAnim->setEndValue(QPoint(0, -H));
    oldAnim->setEasingCurve(outCurve);

    auto *newAnim = new QPropertyAnimation(newW, "pos");
    newAnim->setDuration(340);                // 入场拉长一点让回弹更可读
    newAnim->setStartValue(QPoint(0, -H));
    newAnim->setEndValue(QPoint(0, 0));
    newAnim->setEasingCurve(inCurve);

    // 关键：旧页面在 newAnim 启动时已经离开了，避免两者在屏外叠加产生闪烁。
    auto *group = new QSequentialAnimationGroup(this);
    group->addAnimation(oldAnim);
    group->addAnimation(newAnim);
    mContextTransition = group;

    QPointer<MainWindow> guard(this);
    connect(group, &QSequentialAnimationGroup::finished, this, [guard, page]() {
        if (!guard || !guard->mContextArea) return;
        // 收尾：让 QStackedLayout 接管，把目标页放回 (0,0)，其它页 hide()。
        guard->mContextArea->setCurrentIndex(page);
        for (int i = 0; i < guard->mContextArea->count(); ++i) {
            QWidget *w = guard->mContextArea->widget(i);
            if (w) w->move(0, 0);
        }
    });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::onSkipBlind(int idx)
{
    const TagType pendingTag = mGameState->blindTag(idx);
    if (pendingTag == TagType::Boss && mBlindSelectWidget) {
        mBlindSelectWidget->animateBossReroll([this, idx]() { completeSkipBlind(idx); });
        return;
    }
    completeSkipBlind(idx);
}

void MainWindow::completeSkipBlind(int /*idx*/)
{
    mGameState->skipCurrentBlind();
    TagType gained = mGameState->lastSkippedTag();
    const int copiedByDouble = mGameState->lastConsumedDoubleTags();
    if (gained != TagType::Double && copiedByDouble > 0)
        transformObtainedTags(TagType::Double, gained, copiedByDouble);
    TagData td = tagData(gained);
    addObtainedTag(gained, td.spritePos.x(), td.spritePos.y());
    AudioManager::instance()->play(QStringLiteral("generic1"), 1.0, 1.0);
    refreshGold();
    refreshConsumableSlots();
    refreshJokerSlots();

    switch (gained) {
    case TagType::Standard:
    case TagType::Charm:
    case TagType::Meteor:
    case TagType::Buffoon:
    case TagType::Ethereal: {
        PackKind kind = PackKind::Standard;
        if (gained == TagType::Charm) kind = PackKind::Arcana;
        else if (gained == TagType::Meteor) kind = PackKind::Celestial;
        else if (gained == TagType::Buffoon) kind = PackKind::Buffoon;
        else if (gained == TagType::Ethereal) kind = PackKind::Spectral;

        mQueuedTagPacks.clear();
        for (int i = 0; i < copiedByDouble; ++i)
            mQueuedTagPacks.append(kind);
        consumeImmediateTagPack(kind);
        break;
    }
    // 原版 tag.lua:immediate 系列：tag 立即把钱/牌型变化打到玩家身上，
    // 视觉提示完一小段时间后从 tag 列表移除——这里 1.4 s 后移除。
    case TagType::Skip:
    case TagType::Garbage:
    case TagType::Handy:
    case TagType::Economy:
    case TagType::Orbital:
    case TagType::TopUp:
        QTimer::singleShot(1400, this, [this, gained, copiedByDouble]() { removeObtainedTags(gained, copiedByDouble + 1); });
        break;
    case TagType::Boss:
        QTimer::singleShot(1400, this, [this]() { removeObtainedTag(TagType::Boss); });
        break;
    default: break;
    }
}

void MainWindow::addObtainedTag(TagType type, int tagCol, int tagRow)
{
    QPixmap sheet(":/textures/images/tags.png");
    if (sheet.isNull()) return;

    QPixmap pix = sheet.copy(tagCol * 68, tagRow * 68, 68, 68)
                      .scaled(48, 48, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation);

    auto *item = new QGraphicsPixmapItem(pix);
    item->setZValue(5);
    mScene->addItem(item);
    mObtainedTagIcons.append({type, item});
    relayoutObtainedTags();
    // 不再立刻消失：标签会一直显示，直到 removeObtainedTag(type) 被相应的"使用时机"调用
    // （开包结束、进入商店、开始下一关 blind）。这样玩家有时间看清自己抢到了哪个 tag。
}

void MainWindow::removeObtainedTag(TagType type)
{
    for (int i = 0; i < mObtainedTagIcons.size(); ++i) {
        if (mObtainedTagIcons[i].type == type) {
            mScene->removeItem(mObtainedTagIcons[i].item);
            delete mObtainedTagIcons[i].item;
            mObtainedTagIcons.removeAt(i);
            relayoutObtainedTags();
            return;
        }
    }
}

void MainWindow::removeObtainedTags(TagType type, int count)
{
    for (int i = 0; i < count; ++i)
        removeObtainedTag(type);
}

void MainWindow::transformObtainedTags(TagType from, TagType to, int count)
{
    if (count <= 0) return;
    QPixmap sheet(":/textures/images/tags.png");
    if (sheet.isNull()) return;
    TagData td = tagData(to);
    QPixmap pix = sheet.copy(td.spritePos.x() * 68, td.spritePos.y() * 68, 68, 68)
                      .scaled(48, 48, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation);
    int changed = 0;
    for (ObtainedTagEntry &entry : mObtainedTagIcons) {
        if (entry.type != from) continue;
        entry.type = to;
        if (entry.item) entry.item->setPixmap(pix);
        if (++changed >= count) break;
    }
    relayoutObtainedTags();
}

void MainWindow::clearObtainedTags()
{
    for (auto &e : mObtainedTagIcons) {
        mScene->removeItem(e.item);
        delete e.item;
    }
    mObtainedTagIcons.clear();
}

void MainWindow::relayoutObtainedTags()
{
    // tag 贴在牌堆右侧；多张沿 Y 方向向上累叠（原本就是这套布局）。
    const int deckRightX = mSceneW - 60;
    const int x = deckRightX + 6;
    for (int i = 0; i < mObtainedTagIcons.size(); ++i) {
        int y = mHandYNormal + (CARD_H - 48) / 2 - i * 56;
        mObtainedTagIcons[i].item->setPos(x, y);
    }
}

void MainWindow::setPlayPhaseVisible(bool v)
{
    if (mBestPlayProxy)  mBestPlayProxy->setVisible(v);
    if (mPlayProxy)      mPlayProxy->setVisible(v);
    if (mSortProxy)      mSortProxy->setVisible(v);
    if (mDiscardProxy)   mDiscardProxy->setVisible(v);
    if (mForesightProxy) mForesightProxy->setVisible(v);
    if (mHandCountLabel) mHandCountLabel->setVisible(v);
    for (auto *c : mHandCards)   c->setVisible(v);
    for (auto *c : mPlayedCards) c->setVisible(v);
}

void MainWindow::updateFlameIntensity()
{
    const double earned = mDisplayedChips * mDisplayedMult;
    const double required = mGameState ? mGameState->targetScore() : 0.0;

    double target = 0.0;
    if (required > 0.0 && std::isfinite(earned) && earned >= required) {
        // 原版: max(0, log5(earned) - 2)，用 double 避免 qint64 溢出后负数/崩溃。
        target = std::max(0.0, std::log(std::max(earned, 1.0)) / std::log(5.0) - 2.0);
        target = std::min(target, 10.0);
    } else if (required > 0.0 && std::isinf(earned)) {
        target = 10.0;
    }
    mChipFlameTarget = target;
    mMultFlameTarget = target;

    // 边框颜色:达标后橙色
    const QString chipBase = "background:#009dff; color:white; border-radius:8px; padding:4px 8px;";
    const QString multBase = "background:#fe5f55; color:white; border-radius:8px; padding:4px 8px;";
    if (target > 0.01) {
        if (mLblChips) mLblChips->setStyleSheet(chipBase + " border:3px solid #ffb000;");
        if (mLblMult)  mLblMult ->setStyleSheet(multBase + " border:3px solid #ffb000;");
    } else {
        if (mLblChips) mLblChips->setStyleSheet(chipBase);
        if (mLblMult)  mLblMult ->setStyleSheet(multBase);
    }

    // 几何:把两个 flame widget 分别贴到 chipsRow 内对应方块顶部上方。
    // 第一次显示时 label geometry 可能还没由 layout 算好,加 fallback。
    if (!mChipsRowWidget || !mLeftPanel || !mLblChips || !mLblMult) return;
    const QPoint chipsRowTL = mChipsRowWidget->mapTo(mLeftPanel, QPoint(0, 0));
    const QRect lblChipsR = mLblChips->geometry();
    const QRect lblMultR  = mLblMult ->geometry();
    int chipsRowH = mChipsRowWidget->height();
    int chipsRowW = mChipsRowWidget->width();

    int lblW1 = lblChipsR.width()  > 4 ? lblChipsR.width()  : qMax(80, chipsRowW / 2 - 20);
    int lblW2 = lblMultR .width()  > 4 ? lblMultR .width()  : qMax(80, chipsRowW / 2 - 20);
    int lblX1 = lblChipsR.x()      > 0 ? lblChipsR.x()      : 0;
    int lblX2 = lblMultR .x()      > 0 ? lblMultR .x()      : (chipsRowW / 2 + 20);

    // 火焰高度 ≈ label 高度的 0.85，只在 label 上方稍微往上窜，避免遮住头顶 hand-name。
    // 紧贴 label 顶（offset = -fh，flame 的底正好对上 label 顶）。
    const int fh = qMax(48, int(chipsRowH * 0.85));

    if (mChipFlame) {
        mChipFlame->setGeometry(chipsRowTL.x() + lblX1,
                                chipsRowTL.y() - fh + 4,    // +4 让 flame 底沿和 label 顶轻微重叠
                                lblW1,
                                fh);
        mChipFlame->raise();
    }
    if (mMultFlame) {
        mMultFlame->setGeometry(chipsRowTL.x() + lblX2,
                                chipsRowTL.y() - fh + 4,
                                lblW2,
                                fh);
        mMultFlame->raise();
    }
}

void MainWindow::resetScoreFlame()
{
    mChipFlameTarget = 0.0;
    mMultFlameTarget = 0.0;
    mChipFlameReal = 0.0;
    mMultFlameReal = 0.0;
    mAudioChipFlameReal = 0.0;
    mAudioMultFlameReal = 0.0;
    mAudioChipFlameVelocity = 0.0;
    mAudioMultFlameVelocity = 0.0;
    mAudioChipFlameChange = 0.0;
    mAudioMultFlameChange = 0.0;
    AudioManager::instance()->setScoreAmbient(0.0, 0.0, 0.0, 0.0, 0.0);
    if (mChipFlame) { mChipFlame->hide(); mChipFlame->update(); }
    if (mMultFlame) { mMultFlame->hide(); mMultFlame->update(); }
    const QString chipBase = "background:#009dff; color:white; border-radius:8px; padding:4px 8px;";
    const QString multBase = "background:#fe5f55; color:white; border-radius:8px; padding:4px 8px;";
    if (mLblChips) mLblChips->setStyleSheet(chipBase);
    if (mLblMult)  mLblMult ->setStyleSheet(multBase);
}

void MainWindow::triggerSplashShader()
{
    // 占位接口
    if (mSplashOverlay) mSplashOverlay->hide();
}

void MainWindow::spawnFloatingText(const QPointF &nearPos, const QString &text, const QColor &color)
{
    auto *fs = new FloatingScore(text, color, mPixelFont);
    fs->setZValue(700);
    QPointF basePos = nearPos + QPointF(CARD_W / 2, -20);
    fs->setPos(basePos);
    mScene->addItem(fs);

    mFloatingScores.append(fs);
    connect(fs, &QObject::destroyed, this, [this, fs]() {
        mFloatingScores.removeAll(fs);
    });

    // ── 阶段 1: hold 期(600ms) ──
    // 对应原版 DynaText float=true + Particles 缓慢自转 r_vel
    // 上下飘动 sin 波 ±3px + 每帧自转 0.4°(约 12°/秒, 对应 r_vel ≈ 0.2 rad/s)。
    auto *bob = new QVariantAnimation(this);
    bob->setDuration(600);
    bob->setStartValue(0.0);
    bob->setEndValue(1.0);
    connect(bob, &QVariantAnimation::valueChanged, fs, [fs, basePos](const QVariant &v) {
        if (!fs) return;
        double t = v.toDouble();
        fs->setPos(basePos + QPointF(0, std::sin(t * 6.28 * 1.2) * 3.0));
        fs->setTiltDeg(fs->tiltDeg() + 0.4);
    });
    bob->start(QAbstractAnimation::DeleteWhenStopped);

    // ── 阶段 2: pop_out(向上飞) + fade,300ms ──
    // 对应原版 args.text:pop_out(3) + 后续 fade alpha → 0
    QTimer::singleShot(600, this, [this, fs, basePos]() {
        if (!fs) return;
        auto *popOut = new QVariantAnimation(this);
        popOut->setDuration(300);
        popOut->setStartValue(0.0);
        popOut->setEndValue(24.0);
        connect(popOut, &QVariantAnimation::valueChanged, fs, [fs, basePos](const QVariant &v) {
            if (!fs) return;
            fs->setPos(basePos + QPointF(0, -v.toDouble()));
            fs->setTiltDeg(fs->tiltDeg() + 0.4);
        });
        popOut->start(QAbstractAnimation::DeleteWhenStopped);

        auto *fade = new QPropertyAnimation(fs, "opacity", this);
        fade->setDuration(300);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        fade->setEasingCurve(QEasingCurve::InQuad);
        connect(fade, &QPropertyAnimation::finished, fs, [this, fs]() {
            if (fs->scene()) mScene->removeItem(fs);
            fs->deleteLater();
        });
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void MainWindow::animateScoreTotalThenFinalize(double gained, int /*delayAfterEvents*/)
{
    double before = mGameState->score();
    double after = before + gained;
    if (!std::isfinite(after)) after = std::numeric_limits<double>::infinity();

    scheduleGame(400, []() {
        AudioManager::instance()->play(QStringLiteral("button"), 0.9, 0.6);
    });
    if (gained > 0 || std::isinf(gained)) {
        scheduleGame(1200, []() {
            AudioManager::instance()->play(QStringLiteral("chips2"), 1.0, 1.0);
        });
    }

    auto *anim = new QVariantAnimation(this);
    mScoreCountAnim = anim;   // 跟踪它，打开菜单时可暂停/恢复
    anim->setDuration(scaledDelay(520));   // 倍速一并影响总分计数动画
    anim->setStartValue(before);
    anim->setEndValue(after);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        const double shown = v.toDouble();
        setLabelScaledText(mLblScore, formatScoreNumber(shown), uiPx(38));
        updateScoreProgressBar(shown, true);
    });
    connect(anim, &QVariantAnimation::finished, this, [this, after]() {
        setLabelScaledText(mLblScore, formatScoreNumber(after), uiPx(38));
        updateScoreProgressBar(after, true);
        // 火焰目标在 900ms 后归零(spring ease 自然熄灭)
        scheduleGame(900, [this]() { resetScoreFlame(); });

        animatePlayedCardsToDiscardThen([this]() {
            mGameState->finalizePlayedHand();
            mScoringInProgress = false;
            // 防御性清掉这些跨流程的"动画占用"标记——曾出现 hand-level-up 动画的
            // scheduleGame(tEnd) 因计分链中跳转/取消未触发，导致 mHandLevelAnimating 卡在 true，
            // 之后选牌走 updateHandPreview 时被早 return 屏蔽（用户反馈：出完牌后悬停不计分）。
            mHandLevelAnimating = false;
            ++mHandLevelAnimToken;   // 让任何还在排队的 scheduleGame 回调 token 不匹配自然 noop

            if (mGameState->phase() == GamePhase::Blind) {
                mHandY = mHandYNormal;
                showPlayControlsAfterScoring();
                layoutHandCards();
                refreshCounters();
                updateHandPreview();   // 主动重算一次，让侧栏 chips/mult 立刻回到默认/选牌 preview。
            }
        });
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::animatePlayedCardsToDiscardThen(std::function<void()> after)
{
    QVector<CardItem*> cards = mPlayedCards;
    QPointF deckPos(mSceneW - CARD_W - 60, mHandYScoring);
    int duration = cards.isEmpty() ? 0 : 420;
    for (int i = 0; i < cards.size(); ++i) {
        CardItem *c = cards[i];
        if (!c) continue;
        // 计分结束、卡牌即将飞向弃牌堆——阴影抬升回落到 rest，避免飞行途中
        // 阴影还停在"悬浮起来"的状态。
        c->setScoringLifted(false);
        c->setZValue(90 + i);

        if (mShatteredPlayedIndices.contains(i)) {
            auto *scale = new QPropertyAnimation(c, "scale", this);
            scale->setDuration(duration);
            scale->setStartValue(c->scale());
            scale->setEndValue(0.65);
            scale->setEasingCurve(QEasingCurve::InBack);
            scale->start(QAbstractAnimation::DeleteWhenStopped);

            auto *fade = new QPropertyAnimation(c, "opacity", this);
            fade->setDuration(duration);
            fade->setStartValue(c->opacity());
            fade->setEndValue(0.0);
            fade->setEasingCurve(QEasingCurve::InQuad);
            fade->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
            c->moveTo(deckPos, duration);
            auto *fade = new QPropertyAnimation(c, "opacity", this);
            fade->setDuration(duration);
            fade->setStartValue(c->opacity());
            fade->setEndValue(0.0);
            fade->setEasingCurve(QEasingCurve::InQuad);
            fade->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    scheduleGame(duration + 40, [this, after]() {
        clearPlayedCards();
        mShatteredPlayedIndices.clear();
        if (after) after();
    });
}

void MainWindow::showGameOverOverlay(bool won)
{
    if (!mGameOverPanel) {
        mGameOverPanel = new QWidget;
        mGameOverPanel->setAttribute(Qt::WA_StyledBackground, true);
        mGameOverPanel->setStyleSheet(
            "background:rgba(18,18,24,235); border:3px solid #fe5f55; border-radius:24px;"
            );
        auto *vl = new QVBoxLayout(mGameOverPanel);
        vl->setContentsMargins(34, 28, 34, 28);
        vl->setSpacing(14);
        auto *title = new QLabel(mGameOverPanel);
        title->setObjectName("gameOverTitle");
        QFont tf = mCNFont; tf.setPixelSize(uiPx(42)); tf.setBold(true);
        title->setFont(tf);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("color:#fe5f55; background:transparent; border:none;");
        vl->addWidget(title);
        auto *body = new QLabel(mGameOverPanel);
        body->setObjectName("gameOverBody");
        QFont bf = mCNFont; bf.setPixelSize(uiPx(18));
        body->setFont(bf);
        body->setAlignment(Qt::AlignCenter);
        body->setWordWrap(true);
        body->setStyleSheet("color:white; background:transparent; border:none;");
        vl->addWidget(body);
        auto *row = new QWidget(mGameOverPanel);
        row->setStyleSheet("background:transparent; border:none;");
        auto *hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(12);
        auto *cont = makeBtn("继续（无尽）", "#2bd96b", "#52e589", mCNFont, row, 48);
        cont->setObjectName("gameOverContinue");
        auto *restart = makeBtn("重新开始", "#009dff", "#33b0ff", mCNFont, row, 48);
        auto *quit = makeBtn("退出", "#fe5f55", "#ff7066", mCNFont, row, 48);
        hl->addWidget(cont);
        hl->addWidget(restart);
        hl->addWidget(quit);
        vl->addWidget(row);
        connect(cont, &QPushButton::clicked, this, [this]() {
            hideGameOverOverlay();
            mGameOverHandled = false;
            mGameState->continueEndless();
            refreshGold();
            refreshCounters();
            refreshJokerSlots();
            refreshConsumableSlots();
            refreshScore();
            if (mView) mView->viewport()->update();
            if (mDynamicBg) mDynamicBg->update();
            update();
        });
        connect(restart, &QPushButton::clicked, this, [this]() {
            resetTransientOverlaysForNewRun();
            for (auto *c : mHandCards) { if (c->scene()) mScene->removeItem(c); c->deleteLater(); }
            mHandCards.clear();
            mGameState->startGame();
            mHasOngoingRun = true;
            refreshGold();
            refreshCounters();
            refreshJokerSlots();
            refreshConsumableSlots();
            refreshScore();
            if (mView) mView->viewport()->update();
            if (mDynamicBg) mDynamicBg->update();
            update();
        });
        connect(quit, &QPushButton::clicked, this, &MainWindow::close);
        mGameOverProxy = mScene->addWidget(mGameOverPanel);
        mGameOverProxy->setZValue(1500);
    }

    auto *title = mGameOverPanel->findChild<QLabel*>("gameOverTitle");
    auto *body = mGameOverPanel->findChild<QLabel*>("gameOverBody");
    if (title) title->setText(won ? "胜利" : "游戏结束");
    if (body) body->setText(won
                          ? "你击败了所有盲注。\n可以继续挑战无尽模式。"
                          : QString("未达到盲注要求\n分数：%1 / %2\n底注：%3")
                                .arg(formatScoreNumber(mGameState->score())).arg(formatScoreNumber(mGameState->targetScore())).arg(mGameState->ante()));
    if (auto *cont = mGameOverPanel->findChild<QPushButton*>("gameOverContinue"))
        cont->setVisible(won);
    mGameOverPanel->adjustSize();
    mGameOverProxy->setPos((mSceneW - mGameOverPanel->width()) / 2.0,
                           mSceneH + 40);
    mGameOverProxy->show();
    auto *anim = new QPropertyAnimation(mGameOverProxy, "pos", this);
    anim->setDuration(360);
    anim->setStartValue(mGameOverProxy->pos());
    anim->setEndValue(QPointF((mSceneW - mGameOverPanel->width()) / 2.0,
                              (mSceneH - mGameOverPanel->height()) / 2.0));
    anim->setEasingCurve(QEasingCurve::OutBack);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::hideGameOverOverlay()
{
    if (mGameOverProxy) mGameOverProxy->hide();
}

void MainWindow::clearFloatingScores()
{
    for (auto *fs : mFloatingScores) {
        if (fs->scene()) mScene->removeItem(fs);
        fs->deleteLater();
    }
    mFloatingScores.clear();
}

// 原版 common_events.lua:464 level_up_hand：升级后侧边栏分三拍演出
//   1) 基础倍率刷新 + 抖动
//   2) 基础筹码刷新 + 抖动
//   3) 等级数刷新（换颜色）+ 抖动
// 期间 hand_text_area 被借用展示被升级的那一手的名字 / 基础值，结束后再清空。
// 这里只有在玩家主动使用行星牌 / 黑洞这类不在计分流程中的升级时才播；
// SpaceJoker / 绯红之心 / The Arm 这类在 onHandPlayed 流程中触发的升级，
// 让位给计分动画，跳过演出（mScoringInProgress 守卫）。
void MainWindow::onHandLevelsChanged()
{
    const auto &nowLevels = mGameState->handLevels();

    // 第一次回调（构造后兜底）：只刷新快照，不播动画。
    if (!mHandLevelInitialized) {
        mPrevHandLevels = nowLevels;
        mHandLevelInitialized = true;
        return;
    }

    // 取出所有"等级提高"的牌型，统计数量。
    // - 单手升级（行星牌 / TheArm / SpaceJoker）：走 playHandLevelUpAnimation，
    //   显示具体牌型 + 旧→新 chips/mult 数值。
    // - 多手升级（黑洞 / 同道之星标签）：走 playAllHandsLevelUpAnimation，
    //   显示 "All" + chips/mult "+"，对齐原版 card.lua:1153 的 Black Hole 演出。
    HandType firstUpgraded = HandType::HighCard;
    HandLevel firstPrevLv{};
    int upgradedCount = 0;
    for (auto it = nowLevels.constBegin(); it != nowLevels.constEnd(); ++it) {
        HandType t = it.key();
        HandLevel before = mPrevHandLevels.value(t);
        if (it.value().level > before.level) {
            if (upgradedCount == 0) {
                firstUpgraded = t;
                firstPrevLv = before;
            }
            ++upgradedCount;
        }
    }
    mPrevHandLevels = nowLevels;
    if (upgradedCount == 0) return;
    if (mScoringInProgress) return;   // 计分流程中（SpaceJoker / The Arm）让位给得分演出
    if (mHandLevelAnimating) return;  // 上一次升级动画还在跑，新一次仅刷新快照即可

    if (upgradedCount > 1) {
        playAllHandsLevelUpAnimation();
        return;
    }
    HandType upgraded = firstUpgraded;
    HandLevel prevLv  = firstPrevLv;

    HandLevel newLv = nowLevels.value(upgraded);
    auto base = baseChipsMultFor(upgraded);
    int prevChips = base.first  + prevLv.chipsBonus;
    int prevMult  = base.second + prevLv.multBonus;
    int newChips  = base.first  + newLv.chipsBonus;
    int newMult   = base.second + newLv.multBonus;

    playHandLevelUpAnimation(upgraded, prevLv.level, newLv.level,
                             prevChips, newChips, prevMult, newMult);
}

void MainWindow::playHandLevelUpAnimation(HandType t, int prevLevel, int newLevel,
                                          int prevChips, int newChips,
                                          int prevMult, int newMult)
{
    if (!mLblHandName || !mLblHandLevel || !mLblChips || !mLblMult) return;

    mHandLevelAnimating = true;
    const int token = ++mHandLevelAnimToken;

    const QString name = HandEvaluator::handTypeName(t);

    // 第 0 步：先把侧边栏切到"被升级牌型"的旧值，对应原版 card.lua:1265 / tag.lua:193
    // 在 level_up_hand 之前那一次 update_hand_text(handname/chips/mult/level=旧值)。
    mLblHandName ->setText(name);
    mLblHandLevel->setText(QString("等级%1").arg(prevLevel));
    mLblHandLevel->setStyleSheet(
        QString("color:%1; background:transparent;").arg(handLevelColor(prevLevel)));
    setLabelScaledText(mLblChips, formatScoreNumber(prevChips), uiPx(42));
    setLabelScaledText(mLblMult,  formatScoreNumber(prevMult),  uiPx(42));

    // 原版 common_events.lua:464 level_up_hand 的三拍：mult / chips / level。
    // 拍间隔从原版 0.9s 压到 ~0.3s——每个色块 500ms 走完，相邻两拍只剩 ~200ms gap，
    // 让 chips 的色块还在淡出时 mult 的色块就开始弹出，画面紧凑但节奏依然清楚。
    const int tBeatMult  = 80;
    const int tBeatChips = 360;
    const int tBeatLevel = 660;
    const int tEnd       = 1180;

    AudioManager::instance()->play(QStringLiteral("button"), 0.8, 0.7);
    scheduleGame(200, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        AudioManager::instance()->play(QStringLiteral("tarot1"), 1.0, 1.0);
    });
    scheduleGame(1100, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        AudioManager::instance()->play(QStringLiteral("tarot1"), 1.0, 1.0);
    });
    scheduleGame(2000, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        AudioManager::instance()->play(QStringLiteral("tarot1"), 1.0, 1.0);
        AudioManager::instance()->play(QStringLiteral("button"), 0.9, 0.7);
    });

    const int deltaMult  = newMult  - prevMult;
    const int deltaChips = newChips - prevChips;

    // 第 1 拍：写入新基础倍率，同帧把整个 mult 框盖上"+m"色块。
    // 色块本身就是弹性 pop-in（spawnLabelDelta 内 0.62→1.14→1.0 过冲），淡出时露出已写好的新数字。
    scheduleGame(tBeatMult, [this, token, newMult, deltaMult]() {
        if (token != mHandLevelAnimToken) return;
        setLabelScaledText(mLblMult, formatScoreNumber(newMult), uiPx(42));
        if (deltaMult != 0) {
            // 原版 common_events.lua:539  cover_colour = mix_colours(G.C.MULT, col, 0.1)
            // 注意 mix_colours(C1,C2,proportionC1) 第三参数是 C1 占比——只有 0.1，所以底色其实是 90% 的 col。
            // col：升级（delta>0）= G.C.GREEN，降级（<0）= G.C.RED。
            // globals.lua:354/361/360：MULT=(254,95,85) GREEN=(75,194,146) RED=(254,95,85)
            //   positive: 0.1·(254,95,85)+0.9·(75,194,146) = (93,184,140)  → #5db88c
            //   negative: 0.1·(254,95,85)+0.9·(254,95,85)  = (254,95,85)   → #fe5f55
            const QColor cover = (deltaMult > 0) ? QColor("#5db88c") : QColor("#fe5f55");
            spawnLabelDelta(mLblMult,
                            (deltaMult > 0 ? QStringLiteral("+%1") : QStringLiteral("%1"))
                                .arg(deltaMult),
                            cover);
        } else {
            // 没有 delta（罕见）就退回直接 juice 数字，避免没有任何视觉反馈。
            juiceLabelPulse(mLblMult, 1.22, 280);
        }
    });

    // 第 2 拍：同上，作用在 chips 上。
    scheduleGame(tBeatChips, [this, token, newChips, deltaChips]() {
        if (token != mHandLevelAnimToken) return;
        setLabelScaledText(mLblChips, formatScoreNumber(newChips), uiPx(42));
        if (deltaChips != 0) {
            // 原版 common_events.lua:517  cover_colour = mix_colours(G.C.CHIPS, col, 0.1)
            // CHIPS=(0,157,255) GREEN=(75,194,146) RED=(254,95,85)
            //   positive: 0.1·(0,157,255)+0.9·(75,194,146) = (68,190,157)  → #44be9d
            //   negative: 0.1·(0,157,255)+0.9·(254,95,85)  = (229,101,102) → #e56566
            const QColor cover = (deltaChips > 0) ? QColor("#44be9d") : QColor("#e56566");
            spawnLabelDelta(mLblChips,
                            (deltaChips > 0 ? QStringLiteral("+%1") : QStringLiteral("%1"))
                                .arg(deltaChips),
                            cover);
        } else {
            juiceLabelPulse(mLblChips, 1.22, 280);
        }
    });

    // 第 3 拍：等级 +N，换 HAND_LEVELS 调色板色 + 字号弹性脉冲（原版只 juice_up，不弹 "+N"）。
    scheduleGame(tBeatLevel, [this, token, newLevel]() {
        if (token != mHandLevelAnimToken) return;
        mLblHandLevel->setText(QString("等级%1").arg(newLevel));
        mLblHandLevel->setStyleSheet(
            QString("color:%1; background:transparent;").arg(handLevelColor(newLevel)));
        juiceLabelPulse(mLblHandLevel, 1.40, 360);
    });

    // 结束：解除冻结、还原成当前选牌的 preview（或清空）。
    scheduleGame(tEnd, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        AudioManager::instance()->play(QStringLiteral("button"), 1.1, 0.7);
        mHandLevelAnimating = false;
        updateHandPreview();
    });
}

void MainWindow::playAllHandsLevelUpAnimation()
{
    if (!mLblHandName || !mLblHandLevel || !mLblChips || !mLblMult) return;

    mHandLevelAnimating = true;
    const int token = ++mHandLevelAnimToken;

    // 第 0 步：对齐 card.lua:1154——handname 改成 "All"，chips/mult 显示
    // 占位的 "..." ，level 暂时清空。
    mLblHandName ->setText(QStringLiteral("所有牌型"));
    mLblHandLevel->setText(QString());
    mLblHandLevel->setStyleSheet(QStringLiteral("color:#dddddd; background:transparent;"));
    setLabelScaledText(mLblChips, QStringLiteral("..."), uiPx(42));
    setLabelScaledText(mLblMult,  QStringLiteral("..."), uiPx(42));

    // 三拍节奏与 playHandLevelUpAnimation 保持一致，色块文本固定为 "+"。
    const int tBeatMult  = 80;
    const int tBeatChips = 360;
    const int tBeatLevel = 660;
    const int tEnd       = 1180;

    AudioManager::instance()->play(QStringLiteral("button"), 0.8, 0.7);
    scheduleGame(200, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        AudioManager::instance()->play(QStringLiteral("tarot1"), 1.0, 1.0);
    });
    scheduleGame(1100, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        AudioManager::instance()->play(QStringLiteral("tarot1"), 1.0, 1.0);
    });
    scheduleGame(2000, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        AudioManager::instance()->play(QStringLiteral("tarot1"), 1.0, 1.0);
        AudioManager::instance()->play(QStringLiteral("button"), 1.1, 0.7);
    });

    scheduleGame(tBeatMult, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        // 倍率框依旧维持 "..."，色块上叠加 "+"——对齐原版 mult='+' StatusText。
        const QColor cover = QColor("#5db88c");
        spawnLabelDelta(mLblMult, QStringLiteral("+"), cover);
    });

    scheduleGame(tBeatChips, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        const QColor cover = QColor("#44be9d");
        spawnLabelDelta(mLblChips, QStringLiteral("+"), cover);
    });

    scheduleGame(tBeatLevel, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        mLblHandLevel->setText(QStringLiteral("+1"));
        mLblHandLevel->setStyleSheet(
            QString("color:%1; background:transparent;").arg(handLevelColor(2)));
        juiceLabelPulse(mLblHandLevel, 1.40, 360);
    });

    scheduleGame(tEnd, [this, token]() {
        if (token != mHandLevelAnimToken) return;
        mHandLevelAnimating = false;
        updateHandPreview();   // 还原成当前选牌的 preview/默认显示
    });
}

void MainWindow::spawnLabelDelta(QLabel *anchor, const QString &text, const QColor &bgColor)
{
    if (!anchor || !anchor->parentWidget()) return;
    QWidget *parent = anchor->parentWidget();
    auto *cover = new QLabel(text, parent);
    cover->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    // 关键：alignment 必须跟 anchor 一致——anchor 是 Right/VCenter（chips）或 Left/VCenter（mult），
    // cover 用同样的 alignment 才能让 "+N" 文本和底下的数字落到同一条竖直边线上，避免视觉割裂。
    cover->setAlignment(anchor->alignment());
    const int basePx = qMax(10, anchor->font().pixelSize());
    QFont initial = mPixelFont;
    initial.setBold(true);
    initial.setPixelSize(qMax(8, int(basePx * 0.62)));   // 起点：缩到 62%，准备弹出
    cover->setFont(initial);
    // 关键改动：去掉边框、padding/圆角和 anchor 完全一致，让 cover 看起来就是"chips/mult 框本身
    // 短暂换了个底色"——对应原版 attention_text({cover = G.hand_text_area.chips.parent, cover_colour = …})，
    // 它直接把 chip 框的 parent 整块盖住、染上微调色，而不是一个独立的"+N 标签"。
    cover->setStyleSheet(QString(
        "background:%1; color:white; border-radius:8px; padding:4px 12px;"
    ).arg(bgColor.name()));
    cover->setGeometry(anchor->geometry());
    cover->show();
    cover->raise();

    auto *eff = new QGraphicsOpacityEffect(cover);
    eff->setOpacity(0.0);
    cover->setGraphicsEffect(eff);

    // 整体加速：弹入 160ms + 悬停 200ms + 淡出 140ms = 500ms 走完一拍。
    // 上一版 980ms 太磨；现在的拍间隔（~300ms）刚好让上一个色块还在淡出时，下一个就弹出。
    const int popDur  = 160;
    const int holdDur = 200;
    const int fadeDur = 140;

    auto *pop = new QVariantAnimation(cover);
    pop->setDuration(popDur);
    pop->setStartValue(0.0);
    pop->setEndValue(1.0);
    connect(pop, &QVariantAnimation::valueChanged, cover, [cover, eff, basePx](const QVariant &v) {
        if (!cover) return;
        const double t = v.toDouble();
        // 分段：0..0.55 ease-out 冲到 1.14；0.55..1.0 ease-out 回到 1.0
        double s;
        if (t < 0.55) {
            const double k = t / 0.55;
            s = 0.62 + (1.14 - 0.62) * (1.0 - std::pow(1.0 - k, 3.0));
        } else {
            const double k = (t - 0.55) / 0.45;
            s = 1.14 - 0.14 * (k * (2.0 - k));   // 1.14 → 1.0
        }
        QFont f = cover->font();
        f.setPixelSize(qMax(8, int(basePx * s)));
        cover->setFont(f);
        // 透明度：前 30% 就拉满，色块来得突然——配合弹性缩放才像"砸"上去。
        eff->setOpacity(std::min(1.0, t / 0.30));
    });

    auto *hold = new QPauseAnimation(holdDur, cover);
    auto *fadeOut = new QPropertyAnimation(eff, "opacity", cover);
    fadeOut->setDuration(fadeDur);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    auto *seq = new QSequentialAnimationGroup(cover);
    seq->addAnimation(pop);
    seq->addAnimation(hold);
    seq->addAnimation(fadeOut);
    connect(seq, &QSequentialAnimationGroup::finished, cover, [cover]() {
        cover->deleteLater();
    });
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

// 原版 Moveable:juice_up 是 scale 脉冲——快速冲到峰值，再带阻尼振荡回弹。
// 用 damped 余弦驱动字号：t=0 立刻冲到峰值，之后 cos × e^(-decay·t) 振荡衰减到 0。
// 视觉上不再是平滑的正弦呼吸，而是"弹一下、再小幅回弹"的弹性感。
void MainWindow::juiceLabelPulse(QLabel *lbl, double scaleUp, int durationMs)
{
    if (!lbl) return;
    QFont baseFont = lbl->font();
    const int basePx = qMax(8, baseFont.pixelSize());

    auto *anim = new QVariantAnimation(lbl);
    anim->setDuration(durationMs);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    connect(anim, &QVariantAnimation::valueChanged, lbl, [lbl, basePx, scaleUp](const QVariant &v) {
        if (!lbl) return;
        const double t = v.toDouble();
        // 阶段 1：0..0.16 内用 ease-out 把脉冲值从 0 拉到 1（峰值）。
        // 阶段 2：0.16..1.0 内用 damped 余弦从 1 衰减到 0，期间一次反向回弹。
        double impulse;
        if (t < 0.16) {
            const double k = t / 0.16;
            impulse = 1.0 - std::pow(1.0 - k, 3.0);
        } else {
            const double k = (t - 0.16) / 0.84;
            impulse = std::cos(k * 6.28318 * 1.25) * std::exp(-3.0 * k);
        }
        const double s = 1.0 + (scaleUp - 1.0) * impulse;
        QFont f = lbl->font();
        f.setPixelSize(qMax(8, int(basePx * s)));
        lbl->setFont(f);
    });
    connect(anim, &QVariantAnimation::finished, lbl, [lbl, basePx]() {
        if (!lbl) return;
        QFont f = lbl->font();
        f.setPixelSize(basePx);
        lbl->setFont(f);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::updateHandPreview()
{
    // 牌型升级动画期间，侧边栏被借用来展示"被升级的那一手"的演出步骤，
    // 不能让选牌/出牌等触发的 preview 把演出值踩掉。动画结束后会再调一次本函数恢复正常。
    if (mHandLevelAnimating) return;

    if (mSelected.isEmpty()) {
        mLblHandName ->setText("");
        mLblHandLevel->setText("");
        // 同 onBlindStarted 的注释：必须经过 setLabelScaledText 把字号还原回 uiPx(42)，
        // 否则上一帧若是大数字，"0" 会沿用缩小后的字号，看起来偏位。
        setLabelScaledText(mLblChips, "0", uiPx(42));
        setLabelScaledText(mLblMult,  "0", uiPx(42));
        return;
    }
    HandResult r = mGameState->previewSelection(mSelected);
    mLblHandName ->setText(r.name);
    mLblHandLevel->setText(QString("等级%1").arg(r.level));
    mLblHandLevel->setStyleSheet(QString("color:%1; background:transparent;").arg(handLevelColor(r.level)));
    setLabelScaledText(mLblChips, formatScoreNumber(r.chips), uiPx(42));
    setLabelScaledText(mLblMult,  formatScoreNumber(r.mult),  uiPx(42));
}

void MainWindow::playScoreEvent(const ScoreEvent &ev, double percent)
{
    CardItem *sourceCard = nullptr;
    JokerItem *sourceJoker = nullptr;

    if (ev.sourceCardIdx >= 0 && ev.sourceCardIdx < mPlayedCards.size())
        sourceCard = mPlayedCards[ev.sourceCardIdx];
    else if (ev.sourceHandIdx >= 0 && ev.sourceHandIdx < mHandCards.size())
        sourceCard = mHandCards[ev.sourceHandIdx];

    if (ev.sourceJokerIdx >= 0 && ev.sourceJokerIdx < mJokerItems.size())
        sourceJoker = mJokerItems[ev.sourceJokerIdx];

    QPointF anchorPos;
    if (sourceCard) anchorPos = sourceCard->pos();
    else if (sourceJoker) anchorPos = sourceJoker->pos();
    else anchorPos = QPointF(mSceneW / 2, mBtnY);

    QColor color;
    QString text;
    bool isXMult = false;
    auto randomStatusPercent = []() {
        return 0.9 + QRandomGenerator::global()->generateDouble() * 0.2;
    };
    if (percent < 0.0) {
        if (ev.kind == ScoreEventKind::RedSealRetrigger
            || ev.kind == ScoreEventKind::JokerRetrigger
            || ev.kind == ScoreEventKind::BlueSealPlanet) {
            percent = randomStatusPercent();
        } else if (ev.sourceHandIdx >= 0 && !mHandCards.isEmpty()) {
            percent = (ev.sourceHandIdx + 1.0 - 0.999) / qMax(1.0, double(mHandCards.size()) - 0.998);
        } else {
            percent = randomStatusPercent();
        }
    }
    const double statusPitch = 0.8 + percent * 0.2;

    switch (ev.kind) {
    case ScoreEventKind::ScoringCardChip:
        AudioManager::instance()->play(QStringLiteral("chips1"), statusPitch, 1.0);
        color = QColor("#009dff");
        text = QString("+%1").arg(formatScoreNumber(ev.intValue));
        mDisplayedChips += ev.intValue;
        setLabelScaledText(mLblChips, formatScoreNumber(mDisplayedChips), uiPx(42));
        break;

    case ScoreEventKind::EditionChip:
        AudioManager::instance()->play(QStringLiteral("foil2"), statusPitch, 0.3);
        color = QColor("#009dff");
        text = QString("+%1").arg(formatScoreNumber(ev.intValue));
        mDisplayedChips += ev.intValue;
        setLabelScaledText(mLblChips, formatScoreNumber(mDisplayedChips), uiPx(42));
        break;

    case ScoreEventKind::JokerChip:
        AudioManager::instance()->play(QStringLiteral("generic1"), statusPitch, 1.0);
        color = QColor("#009dff");
        text = QString("+%1").arg(formatScoreNumber(ev.intValue));
        mDisplayedChips += ev.intValue;
        setLabelScaledText(mLblChips, formatScoreNumber(mDisplayedChips), uiPx(42));
        break;

    case ScoreEventKind::EnhancementMult:
        AudioManager::instance()->play(QStringLiteral("multhit1"), statusPitch, 1.0);
        color = QColor("#fe5f55");
        text = QString("+%1").arg(formatScoreNumber(ev.intValue));
        mDisplayedMult += ev.intValue;
        setLabelScaledText(mLblMult, formatScoreNumber(mDisplayedMult), uiPx(42));
        break;

    case ScoreEventKind::EditionMult:
        AudioManager::instance()->play(QStringLiteral("foil2"), statusPitch, 0.3);
        color = QColor("#fe5f55");
        text = QString("+%1").arg(formatScoreNumber(ev.intValue));
        mDisplayedMult += ev.intValue;
        setLabelScaledText(mLblMult, formatScoreNumber(mDisplayedMult), uiPx(42));
        break;

    case ScoreEventKind::JokerMult:
        AudioManager::instance()->play(QStringLiteral("multhit1"), statusPitch, 1.0);
        color = QColor("#fe5f55");
        text = QString("+%1").arg(formatScoreNumber(ev.intValue));
        mDisplayedMult += ev.intValue;
        setLabelScaledText(mLblMult, formatScoreNumber(mDisplayedMult), uiPx(42));
        break;

    case ScoreEventKind::EnhancementXMult:
    case ScoreEventKind::SteelXMult:
    case ScoreEventKind::JokerXMult:
        AudioManager::instance()->play(QStringLiteral("multhit2"), statusPitch, 0.7);
        color = QColor("#fe5f55");
        text = QString("×%1").arg(QString::number(ev.xmultValue, 'g', 3));
        isXMult = true;
        mDisplayedMult = std::max(1.0, mDisplayedMult * ev.xmultValue);
        if (!std::isfinite(mDisplayedMult)) mDisplayedMult = std::numeric_limits<double>::infinity();
        setLabelScaledText(mLblMult, formatScoreNumber(mDisplayedMult), uiPx(42));
        break;

    case ScoreEventKind::EditionXMult:
        AudioManager::instance()->play(QStringLiteral("foil2"), statusPitch, 0.3);
        color = QColor("#fe5f55");
        text = QString("x%1").arg(QString::number(ev.xmultValue, 'g', 3));
        text = QString("脳%1").arg(QString::number(ev.xmultValue, 'g', 3));
        isXMult = true;
        mDisplayedMult = std::max(1.0, mDisplayedMult * ev.xmultValue);
        if (!std::isfinite(mDisplayedMult)) mDisplayedMult = std::numeric_limits<double>::infinity();
        setLabelScaledText(mLblMult, formatScoreNumber(mDisplayedMult), uiPx(42));
        break;

    case ScoreEventKind::DollarGain:
        AudioManager::instance()->play(QStringLiteral("coin3"), statusPitch, 1.0);
        color = QColor("#f3b958");
        text = QString("+$%1").arg(formatScoreNumber(ev.intValue));
        refreshGold();
        break;

    case ScoreEventKind::RedSealRetrigger:
    case ScoreEventKind::JokerRetrigger:
        AudioManager::instance()->play(QStringLiteral("generic1"), statusPitch, 1.0);
        color = QColor("#ff5f55");
        text = QStringLiteral("再触发");
        break;

    case ScoreEventKind::GlassShatter:
        AudioManager::instance()->playRandom({
            QStringLiteral("glass1"), QStringLiteral("glass2"), QStringLiteral("glass3"),
            QStringLiteral("glass4"), QStringLiteral("glass5"), QStringLiteral("glass6")
        }, 0.9 + QRandomGenerator::global()->generateDouble() * 0.2, 0.5);
        AudioManager::instance()->play(QStringLiteral("generic1"),
                                       0.9 + QRandomGenerator::global()->generateDouble() * 0.2,
                                       0.5);
        color = QColor("#9ee7ff");
        text = QStringLiteral("碎裂");
        if (ev.sourceCardIdx >= 0) mShatteredPlayedIndices.insert(ev.sourceCardIdx);
        break;

    case ScoreEventKind::BlueSealPlanet:
        AudioManager::instance()->play(QStringLiteral("generic1"), statusPitch, 1.0);
        color = QColor("#5aa7ff");
        text = QStringLiteral("+星球牌");
        refreshConsumableSlots();
        break;
    }

    if (sourceCard) {
        if (ev.kind == ScoreEventKind::GlassShatter) {
            // 玻璃牌碎裂:水平 shake (原版 card_eval_status_text 内部 + glass 特殊处理)
            sourceCard->juiceUp(1.28, 260);
            auto *shake = new QSequentialAnimationGroup(sourceCard);
            QPointF base = sourceCard->pos();
            for (int i = 0; i < 4; ++i) {
                auto *a = new QPropertyAnimation(sourceCard, "pos");
                a->setDuration(28);
                a->setStartValue(i == 0 ? base : base + QPointF((i % 2 ? -1 : 1) * 5, 0));
                a->setEndValue(base + QPointF((i % 2 ? 1 : -1) * 5, 0));
                shake->addAnimation(a);
                a->setParent(shake);
            }
            auto *back = new QPropertyAnimation(sourceCard, "pos");
            back->setDuration(40);
            back->setStartValue(base + QPointF(5, 0));
            back->setEndValue(base);
            shake->addAnimation(back);
            back->setParent(shake);
            shake->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
            // 原版 card_eval_status_text 内对源卡只做 card:juice_up(0.6, 0.1) 缩放脉冲,
            // 不做位置上下蹦动。卡片在 onHandPlayed 阶段已 highlight 升起并保持。
            sourceCard->juiceUp(1.18, 210);
        }
    }
    if (sourceJoker) {
        sourceJoker->juiceUp(1.15, 200);
    }

    spawnFloatingText(anchorPos, text, color);

    // ★ 每个事件累加完 displayed chips/mult 后,无条件重算火焰强度。
    // updateFlameIntensity 内部按 earned >= required 判定,未达标 target=0 火焰自然隐藏,
    // 跨过门槛后按 log5 公式渐强。
    updateFlameIntensity();
}

void MainWindow::hidePlayControlsForScoring()
{
    // 对应原版 game.lua:3188 update_hand_played 的 buttons:remove()。
    // 与手牌 moveTo() 同样的 200ms / OutCubic 曲线，让按钮、手牌、查看牌组面板三者
    // 同时启动、同时减速、同时停下，整组运动看上去是一气呵成的。
    auto slideOut = [this](QGraphicsProxyWidget *proxy, QPointF home) {
        if (!proxy) return;
        auto *anim = new QPropertyAnimation(proxy, "pos", this);
        anim->setDuration(200);
        anim->setStartValue(proxy->pos());
        anim->setEndValue(home + QPointF(0, 160));   // 向下 160px,出按钮区域
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    };
    slideOut(mBestPlayProxy, mBestPlayBtnHome);
    slideOut(mPlayProxy,    mPlayBtnHome);
    slideOut(mSortProxy,    mSortBtnHome);
    slideOut(mDiscardProxy, mDiscardBtnHome);
    slideOut(mForesightProxy, mForesightBtnHome);
}

void MainWindow::showPlayControlsAfterScoring()
{
    // 对应原版 update_selecting_hand 的按钮重建,带飞入动画。
    auto slideIn = [this](QGraphicsProxyWidget *proxy, QPointF home) {
        if (!proxy) return;
        auto *anim = new QPropertyAnimation(proxy, "pos", this);
        anim->setDuration(200);
        anim->setStartValue(proxy->pos());
        anim->setEndValue(home);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    };
    slideIn(mBestPlayProxy, mBestPlayBtnHome);
    slideIn(mPlayProxy,    mPlayBtnHome);
    slideIn(mSortProxy,    mSortBtnHome);
    slideIn(mDiscardProxy, mDiscardBtnHome);
    slideIn(mForesightProxy, mForesightBtnHome);
}
