#ifndef SHOPWIDGET_H
#define SHOPWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "../game/gamestate.h"
#include "../card/consumable.h"

class QEvent;

class ShopWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ShopWidget(GameState *gs,
                        const QFont &cnFont, const QFont &pixelFont,
                        QWidget *parent = nullptr);
    void refresh();

signals:
    void leaveClicked();
    void packBuyRequested(int slot);

protected:
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onReroll();

private:
    struct OfferUi {
        QWidget     *card     = nullptr;
        QLabel      *imageLbl = nullptr;   // 当前 cpp 没用,保留以备拓展
        QLabel      *nameLbl  = nullptr;
        QLabel      *priceLbl = nullptr;
        QPushButton *cardBtn  = nullptr;
        // 仅塔罗/星球/幻灵牌使用：原版 "BUY"/"BUY AND USE" 双按钮。
        QWidget     *actionRow = nullptr;
        QPushButton *buyBtn    = nullptr;
        QPushButton *useBtn    = nullptr;
    };

    void buildUi();
    void layoutPanel();
    void buildInfoPanel();
    void showOfferInfo(QWidget *source);
    void hideOfferInfo();
    OfferUi createOfferSlot(QWidget *parent, bool isBooster);
    QPixmap offerPixmap(const ShopOffer &o) const;

    void onBuyShop(int slot);
    void onBuyAndUseShop(int slot);    // 商店里"购买并使用"塔罗/星球/幻灵牌
    void onBuyBooster(int slot);
    void onBuyVoucher(int slot);
    QPixmap playingCardPixmap(const CardData &c) const;

    GameState *mGS;
    QFont mCNFont;
    QFont mPixelFont;

    QWidget     *mPanel        = nullptr;
    QPushButton *mBtnNextRound = nullptr;
    QPushButton *mBtnReroll    = nullptr;
    QLabel      *mLblGold      = nullptr;

    QWidget *mInfoPanel = nullptr;
    QLabel *mInfoTitle = nullptr;
    QLabel *mInfoBody = nullptr;

    QVector<OfferUi> mShopUi;
    QVector<OfferUi> mVoucherUi;
    QVector<OfferUi> mBoosterUi;

    // 商店里选中的塔罗/星球/幻灵牌槽位（-1 = 未选中）。
    // 选中后才出现"购买"和"购买&使用"两个按钮，单击同一张牌或别处会取消选中。
    int mSelectedShopSlot = -1;
    int mSelectedVoucherSlot = -1;
    int mSelectedBoosterSlot = -1;

    void onShopCardClicked(int slot);
    void onVoucherCardClicked(int slot);
    void onBoosterCardClicked(int slot);
    void positionSlotActionButtons(OfferUi &ou, bool hasUseBtn);
};

#endif
