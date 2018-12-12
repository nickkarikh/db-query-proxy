#include "daemon.h"
#include "log.h"
#include "proxy-server.h"
#include "query-processor.h"
#include <QCoreApplication>

#ifdef Q_OS_LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#endif

MyDaemon *myDaemon;

extern QString config_filename;

#ifdef Q_OS_LINUX
static void signalHandler(int signum);
static int sigFd[2];

//---------------------------------------------------------------------------
static int setup_unix_signal_handlers()
{
  struct sigaction sa;

  sa.sa_handler = signalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_flags |= SA_RESTART;

  if (sigaction(SIGHUP, &sa, 0))
  {
    qCritical("Failed to install SIGHUP handler");
    return 1;
  }
  if (sigaction(SIGTERM, &sa, 0))
  {
    qCritical("Failed to install SIGTERM handler");
    return 1;
  }
  if (sigaction(SIGINT, &sa, 0))
  {
    qCritical("Failed to install SIGINT handler");
    return 1;
  }

  return 0;
}
#endif

//---------------------------------------------------------------------------
MyDaemon::MyDaemon(QObject *parent): QObject(parent)
{
  sn = NULL;
#ifdef Q_OS_LINUX
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd))
    qFatal("Couldn't create signal socketpair");
  sn = new QSocketNotifier(sigFd[1], QSocketNotifier::Read, this);
  connect(sn, SIGNAL(activated(int)), this, SLOT(handleSignal()));

  setup_unix_signal_handlers();
#endif
}

//---------------------------------------------------------------------------
MyDaemon::~MyDaemon()
{
#ifdef Q_OS_LINUX
  close(sigFd[0]);
  close(sigFd[1]);
#endif
}

#ifdef Q_OS_LINUX
//---------------------------------------------------------------------------
void signalHandler(int signum)
{
  char a = (char)signum;
  write(sigFd[0], &a, sizeof(a));
}
#endif

//---------------------------------------------------------------------------
void MyDaemon::handleSignal()
{
#ifdef Q_OS_LINUX
  sn->setEnabled(false);
  char signum;
  ::read(sigFd[1], &signum, sizeof(signum));

  if (signum == SIGTERM)
  {
    _qDebug(LOG_GENERAL, "Got SIGTERM - exiting");
    // do Qt stuff
    QCoreApplication::exit(0);
    return;
  }
  else if (signum == SIGINT)
  {
    _qDebug(LOG_GENERAL, "Got SIGINT - exiting");
    // do Qt stuff
    QCoreApplication::exit(0);
    return;
  }
  else if (signum == SIGHUP)
  {
    _qDebug(LOG_GENERAL, "Got SIGHUP");
    if (proxyServer->isListening())
    {
      _qDebug(LOG_PROXY_SERVER, "Stopped TCP listening server on %s:%d", proxyServer->listen_address.toString().toUtf8().constData(), proxyServer->serverPort());
      proxyServer->close();
    }
    log_stop();
    readConfig(config_filename);
    log_init();
    proxyServer->start();
  }
  else
  {
    _qDebug(LOG_GENERAL, "Got signal %d", (int)signum);
  }

  sn->setEnabled(true);
#endif
}
