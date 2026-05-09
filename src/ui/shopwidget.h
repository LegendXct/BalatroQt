#ifndef SHOPWIDGET_H
#define SHOPWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include "../game/gamestate.h"
#include "../card/consumable.h"


class ShopWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ShopWidget(GameState *gs,
                        const QFont &cnFont, const QFont &pixelFont,
                        QWidget *parent = nullptr);

    void refresh();              // 用 GameState 当前数据刷 UI

signals:
    void leaveClicked();
    void packBuyRequested(int slot);

protected:
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void onBuy(int slot);
    void onReroll();

private:
    GameState *mGS;
    QFont mCNFont;
    QFont mPixelFont;

    QWidget     *mPanel     = nullptr;
    QLabel      *mLblGold   = nullptr;
    QPushButton *mBtnReroll = nullptr;
    QPushButton *mBtnLeave  = nullptr;

    struct OfferUi {
        QWidget     *card;
        QLabel      *imageLbl;
        QLabel      *nameLbl;
        QLabel      *descLbl;
        QPushButton *buyBtn;
    };
    QVector<OfferUi> mOfferUi;

    void buildUi();
    void layoutPanel();
    QPixmap offerPixmap(const ShopOffer &) const;
};

#endif
