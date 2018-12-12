#include "log.h"
#include <QFile>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTextCodec>

int log_categories = 0
                   | LOG_SQL_SESSIONS
                   | LOG_PROXY_SERVER
                   | LOG_PROXY_CLIENT;

QFile f_log;
int log_verbosity = 0;

bool log_to_console = false;

QElapsedTimer t_log_flush;

//---------------------------------------------------------------------------
#if QT_VERSION >= 0x050000
void myMessageOutput(QtMsgType type, const QMessageLogContext &, const QString &msg)
#else
void myMessageOutput(QtMsgType type, const char *msg)
#endif
{
  switch (log_verbosity)
  {
    case 0:
      return;
    case 1:
      if (type != QtFatalMsg)
        return;
      break;
    case 2:
      if (type != QtFatalMsg && type != QtCriticalMsg)
        return;
      break;
    case 3:
    case 4:
      if (type != QtFatalMsg && type != QtCriticalMsg && type != QtWarningMsg)
        return;
      break;
    default:
      break;
  }

  if (log_verbosity == 0)
    return;
  QString fullstr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz ");
  switch (type)
  {
    case QtDebugMsg:
      fullstr += QString("debug: ");
      break;
    case QtWarningMsg:
      fullstr += QString("Warning: ");
      break;
    case QtCriticalMsg:
      fullstr += QString("CRITICAL: ");
      break;
    case QtFatalMsg:
      fullstr += QString("FATAL: ");
      break;
    default:
      fullstr += QString("Unknown: ");
      break;
  }
#if QT_VERSION >= 0x050000
  fullstr += msg;
#else
  fullstr += QString::fromUtf8(msg);
#endif
  fullstr += QString("\n");
  if (f_log.isOpen())
  {
    f_log.write(fullstr.toUtf8());
    if (type != QtDebugMsg || qAbs(t_log_flush.elapsed()) > 1000)
    {
      f_log.flush();
      t_log_flush.restart();
    }
  }
  if (log_to_console || type == QtCriticalMsg || type == QtFatalMsg)
  {
    fprintf((type == QtDebugMsg) ? stdout : stderr, "%s", QTextCodec::codecForLocale()->fromUnicode(fullstr).constData());
    fflush((type == QtDebugMsg) ? stdout : stderr);
  }
}

//---------------------------------------------------------------------------
void log_init()
{
#if QT_VERSION >= 0x050000
  qInstallMessageHandler(myMessageOutput);
#else
  qInstallMsgHandler(myMessageOutput);
#endif

  t_log_flush.restart();
  if (!f_log.fileName().isEmpty() && log_verbosity >= 1)
    f_log.open(QIODevice::Append);
}

//---------------------------------------------------------------------------
void log_stop()
{
  if (f_log.isOpen())
    f_log.close();
}
