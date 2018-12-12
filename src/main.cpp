#include <QCoreApplication>
#include <QStringList>
#include "log.h"
#include "daemon.h"
#include "proxy-server.h"
#include "query-processor.h"

QString config_filename;

//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  QCoreApplication *app = new QCoreApplication(argc, argv);

#ifdef Q_OS_LINUX
  config_filename = QString("/etc/")+app->applicationName()+QString(".conf");
#else
  config_filename = app->applicationName()+QString(".conf");
#endif
  log_verbosity = 4;
  f_log.setFileName(app->applicationName()+QString(".log"));

  for (int i=1; i < app->arguments().count(); i++)
  {
    if (app->arguments().at(i) == QString("-f") && i+1 < app->arguments().count())
    {
      config_filename = app->arguments().at(++i);
      continue;
    }
    if (app->arguments().at(i) == QString("-d"))
    {
      log_verbosity = 5;
      continue;
    }
    if (app->arguments().at(i) == QString("-e") && i+1 < app->arguments().count())
    {
      QStringList args = app->arguments();
      for (int j=0; j <= i; j++)
        args.removeFirst();
      QString esc = QRegExp::escape(args.join(" "));
      fprintf(stdout, "Escaped RegExp: %s\r\n", esc.toUtf8().constData());
      delete app;
      return 1;
    }
    qCritical("Unknown argument: %s", app->arguments().at(i).toUtf8().constData());
    fprintf(stdout, "Usage: %s [-f config_filename] [-d]\r\n"
                    "-f config_filename - read configuration from specified file (defaults to /etc/%s.conf)\r\n"
                    "-d                 - write debug messages to log file\r\n",
                    app->applicationName().toUtf8().constData(),
                    app->applicationName().toUtf8().constData());
  }

  myDaemon = new MyDaemon;

  proxyServer = new ProxyServer;

  if (!readConfig(config_filename))
  {
    delete proxyServer;
    delete myDaemon;
    delete app;
    return 1;
  }

  log_init();
  _qDebug(LOG_GENERAL, "%s started", app->applicationName().toUtf8().constData());

  QObject::connect(app, SIGNAL(aboutToQuit()), proxyServer, SLOT(stop()));
  proxyServer->start();

  int res = app->exec();

  delete proxyServer;

  delete myDaemon;

  _qDebug(LOG_GENERAL, "%s stopped", app->applicationName().toUtf8().constData());
  log_stop();
  delete app;

  return res;
}

