#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <QObject>

class GameState : public QObject
{
    Q_OBJECT
public:
    explicit GameState(QObject *parent = nullptr);

signals:
};

#endif // GAMESTATE_H
