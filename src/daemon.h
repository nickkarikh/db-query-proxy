#ifndef DAEMON_H
#define DAEMON_H

#include <QObject>
#include <QSocketNotifier>
#include <QtGlobal>

class MyDaemon : public QObject
{
    Q_OBJECT

  public:
    MyDaemon(QObject *parent = 0);
    ~MyDaemon();

  public slots:
    // Qt signal handlers.
    void handleSignal();

signals:

  private:

    QSocketNotifier *sn;
};

extern MyDaemon *myDaemon;


#endif // DAEMON_H
