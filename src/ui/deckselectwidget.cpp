#include "deckselectwidget.h"
#include "../card/carditem.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <functional>

namespace {
// 选项卡片（卡背 + 牌组名），整块可点。
class DeckOptionFrame : public QWidget
{
public:
    DeckOptionFrame(const QPixmap &back, const QString &name,
                    const QFont &cnFont, std::function<void()> onClick,
                    QWidget *parent)
        : QWidget(parent), mOnClick(std::move(onClick))
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setCursor(Qt::PointingHandCursor);
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(14, 14, 14, 10);
        v->setSpacing(8);
        auto *img = new QLabel(this);
        img->setPixmap(back.scaled(142, 190, Qt::KeepAspectRatio,
                                   Qt::FastTransformation));   // 像素风：最近邻放大
        img->setAlignment(Qt::AlignCenter);
        img->setStyleSheet("background:transparent; border:none;");
        v->addWidget(img);
        auto *nameLab = new QLabel(name, this);
        QFont f = cnFont; f.setPixelSize(20); f.setBold(true);
        nameLab->setFont(f);
        nameLab->setStyleSheet("color:white; background:transparent; border:none;");
        nameLab->setAlignment(Qt::AlignCenter);
        v->addWidget(nameLab);
        setSelected(false);
    }
    void setSelected(bool on)
    {
        setStyleSheet(QString("background:#3a4a4d; border-radius:12px;"
                              " border:3px solid %1;")
                          .arg(on ? "#fda200" : "transparent"));
    }
protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) mOnClick();
        QWidget::mousePressEvent(e);
    }
private:
    std::function<void()> mOnClick;
};
} // namespace

DeckSelectWidget::DeckSelectWidget(const QFont &cnFont, QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background:rgba(10,12,14,200);");   // 半透明压暗主菜单

    auto *outer = new QVBoxLayout(this);
    outer->setAlignment(Qt::AlignCenter);

    auto *panel = new QWidget(this);
    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setStyleSheet("background:#4f6367; border-radius:16px;");
    panel->setFixedWidth(640);   // 三张牌组卡背并排
    auto *pv = new QVBoxLayout(panel);
    pv->setContentsMargins(24, 20, 24, 20);
    pv->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("选择牌组"), panel);
    QFont tf = cnFont; tf.setPixelSize(26); tf.setBold(true);
    title->setFont(tf);
    title->setStyleSheet("color:white; background:transparent;");
    title->setAlignment(Qt::AlignCenter);
    pv->addWidget(title);

    // —— 牌组选项行：经工厂取多态实例读名称/描述，新增牌组只需扩 GameDeckId ——
    const QPixmap back = CardItem::cardBackPixmap();
    const QVector<QPair<GameDeckId, QPixmap>> options = {
        { GameDeckId::Base,  back },
        { GameDeckId::Queue, hueShifted(back, 150) },   // 队列牌组卡背：色相偏移区分
        { GameDeckId::Stack, hueShifted(back, 280) },   // 栈牌组卡背：再偏一个角度
    };
    auto *row = new QHBoxLayout();
    row->setSpacing(18);
    for (int i = 0; i < options.size(); ++i) {
        const auto deck = createGameDeck(options[i].first);
        auto *frame = new DeckOptionFrame(options[i].second, deck->name(), cnFont,
                                          [this, i]() { select(i); }, panel);
        mOptionFrames.append(frame);
        row->addWidget(frame);
    }
    pv->addLayout(row);

    mDescLabel = new QLabel(panel);
    QFont df = cnFont; df.setPixelSize(16);
    mDescLabel->setFont(df);
    mDescLabel->setStyleSheet("color:#d8e4e6; background:transparent;");
    mDescLabel->setWordWrap(true);
    mDescLabel->setAlignment(Qt::AlignCenter);
    mDescLabel->setMinimumHeight(64);
    pv->addWidget(mDescLabel);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(14);
    auto makeBtn = [&](const QString &text, const QString &bg, const QString &dark) {
        auto *b = new QPushButton(text, panel);
        QFont f = cnFont; f.setPixelSize(20); f.setBold(true);
        b->setFont(f);
        b->setMinimumHeight(52);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QPushButton { background:%1; color:white; border:none;"
            " border-radius:10px; border-bottom:5px solid %2; }"
            "QPushButton:pressed { border-bottom:2px solid %2; margin-top:3px; }")
                             .arg(bg, dark));
        btnRow->addWidget(b);
        return b;
    };
    auto *btnBack  = makeBtn(QStringLiteral("返回"),     "#fe5f55", "#c44840");
    auto *btnStart = makeBtn(QStringLiteral("开始游戏"), "#009dff", "#0077c2");
    connect(btnBack,  &QPushButton::clicked, this, [this]() { emit cancelled(); });
    connect(btnStart, &QPushButton::clicked, this,
            [this]() { emit startRequested(mSelected); });
    pv->addLayout(btnRow);

    outer->addWidget(panel);
    select(0);
}

void DeckSelectWidget::select(int idx)
{
    mSelected = static_cast<GameDeckId>(idx);
    for (int i = 0; i < mOptionFrames.size(); ++i)
        static_cast<DeckOptionFrame*>(mOptionFrames[i])->setSelected(i == idx);
    const auto deck = createGameDeck(mSelected);
    mDescLabel->setText(deck->description());
}

QPixmap DeckSelectWidget::hueShifted(const QPixmap &src, int dh)
{
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = QColor::fromRgba(line[x]);
            int h, s, v;
            c.getHsv(&h, &s, &v);
            if (h < 0) continue;   // 无色相（灰阶）像素不动
            QColor shifted;
            shifted.setHsv((h + dh) % 360, s, v, c.alpha());
            line[x] = shifted.rgba();
        }
    }
    return QPixmap::fromImage(img);
}
