#include "query-processor.h"
#include "log.h"
#include <QStringList>
#include <QRegExp>
#include <QElapsedTimer>
#include <QDateTime>

class QueryRewriteRule
{
public:
  QueryRewriteRule()
  {
    action = ACT_LOG;
  }

  QRegExp query_rx;
  enum {
    ACT_LOG
   ,ACT_REWRITE
  } action;
  QString rewrite_str;
};

QList<QueryRewriteRule> rules;

extern bool log_to_console;

QRegExp rx_value("\\$\\((\\d+)\\)");
QRegExp rx_now("\\$\\(now(|\\+|-)(|\\d+)(|Y|M|D|h|m|s)\\)");

//---------------------------------------------------------------------------
bool readConfig(const QString &filename)
{
  rules.clear();

  QFile f(filename);
  if (!f.open(QIODevice::ReadOnly))
  {
    qCritical("Unable to open configuration file %s!", f.fileName().toUtf8().constData());
    return false;
  }
  QStringList lines = QString::fromUtf8(f.readAll()).split("\n");
  f.close();

  QueryRewriteRule current_rule;

  for (int line_number=0; line_number < lines.count(); line_number++)
  {
    QString line = lines.at(line_number).trimmed();
    if (line.isEmpty() || line.startsWith("#"))
      continue;

    int eq_pos = line.indexOf("=");
    if (eq_pos <= 0)
    {
      qCritical("Configuration file %s parsing error on line %d", f.fileName().toUtf8().constData(), line_number+1);
      return false;
    }

    QString param_name = line.left(eq_pos).trimmed();
    QString param_value = line.mid(eq_pos+1).trimmed();
    if ((param_value.startsWith("\"") && param_value.endsWith("\"")) ||
        (param_value.startsWith("'") && param_value.endsWith("'")))
    {
      param_value.remove(0, 1);
      param_value.remove(param_value.length()-1, 1);
    }

    while (line_number+1 < lines.count())
    {
      if ((lines.at(line_number+1).startsWith(" ") || lines.at(line_number+1).startsWith("\t")) &&
          !lines.at(line_number+1).trimmed().isEmpty())
      {
        line_number++;
        line = lines.at(line_number).trimmed();
        if ((line.startsWith("\"") && line.endsWith("\"")) ||
            (line.startsWith("'") && line.endsWith("'")))
        {
          line.remove(0, 1);
          line.remove(line.length()-1, 1);
        }
        param_value += line;
      }
      else
        break;
    }

    if (param_name.compare("listen_address", Qt::CaseInsensitive) == 0)
    {
      if (!proxyServer->listen_address.setAddress(param_value))
      {
        qCritical("Configuration file %s parsing error on line %d: invalid parameter \"%s\" value", f.fileName().toUtf8().constData(), line_number+1, param_name.toUtf8().constData());
        return false;
      }
    }
    else if (param_name.compare("listen_port", Qt::CaseInsensitive) == 0)
    {
      bool ok=false;
      proxyServer->listen_port = param_value.toUShort(&ok);
      if (!ok || proxyServer->listen_port < 2)
      {
        qCritical("Configuration file %s parsing error on line %d: invalid parameter \"%s\" value", f.fileName().toUtf8().constData(), line_number+1, param_name.toUtf8().constData());
        return false;
      }
    }
    else if (param_name.compare("dst_address", Qt::CaseInsensitive) == 0)
    {
      proxyServer->dst_server_address = param_value;
      if (!QRegExp("[A-Za-z0-9.-]+").exactMatch(param_value))
      {
        qCritical("Configuration file %s parsing error on line %d: invalid parameter \"%s\" value", f.fileName().toUtf8().constData(), line_number+1, param_name.toUtf8().constData());
        return false;
      }
    }
    else if (param_name.compare("dst_port", Qt::CaseInsensitive) == 0)
    {
      bool ok=false;
      int val = param_value.toInt(&ok);
      if (!ok || val < 2 || val > 65535)
      {
        qCritical("Configuration file %s parsing error on line %d: invalid parameter \"%s\" value", f.fileName().toUtf8().constData(), line_number+1, param_name.toUtf8().constData());
        return false;
      }
      proxyServer->dst_server_port = val;
    }
    else if (param_name.compare("log_file", Qt::CaseInsensitive) == 0)
    {
      f_log.setFileName(param_value);
    }
    else if (param_name.compare("log_verbosity", Qt::CaseInsensitive) == 0)
    {
      bool ok=false;
      log_verbosity = param_value.toInt(&ok);
      if (!ok || log_verbosity < 1)
      {
        qCritical("Configuration file %s parsing error on line %d: invalid parameter \"%s\" value", f.fileName().toUtf8().constData(), line_number+1, param_name.toUtf8().constData());
        return false;
      }
    }
    else if (param_name.compare("log_to_console", Qt::CaseInsensitive) == 0)
    {
      if (param_value == QString("yes") || param_value == QString("true") || param_value == QString("1"))
        log_to_console = true;
    }
    else if (param_name.compare("log_categories", Qt::CaseInsensitive) == 0)
    {
      bool ok=false;
      log_categories = param_value.toUInt(&ok, 16);
      if (!ok)
      {
        qCritical("Configuration file %s parsing error on line %d: invalid parameter \"%s\" value", f.fileName().toUtf8().constData(), line_number+1, param_name.toUtf8().constData());
        return false;
      }
    }
    else if (param_name.compare("query", Qt::CaseInsensitive) == 0)
    {
      if (!current_rule.query_rx.isEmpty())
      {
        rules.append(current_rule);
        current_rule = QueryRewriteRule();
      }
      // add start-of-string and end-of-string patterns if not present
      // because we must match the entire query string
      if (!param_value.startsWith("^"))
        param_value = QString("^")+param_value;
      if (!param_value.startsWith("$"))
        param_value = param_value+QString("$");
      current_rule.query_rx = QRegExp(param_value);
      current_rule.query_rx.setMinimal(true);
    }
    else if (param_name.compare("action", Qt::CaseInsensitive) == 0)
    {
      if (param_value.compare("log", Qt::CaseInsensitive) == 0)
        current_rule.action = QueryRewriteRule::ACT_LOG;
      else if (param_value.compare("rewrite", Qt::CaseInsensitive) == 0)
        current_rule.action = QueryRewriteRule::ACT_REWRITE;
    }
    else if (param_name.compare("rewrite", Qt::CaseInsensitive) == 0)
    {
      current_rule.rewrite_str = param_value;
    }
    else
    {
      qCritical("Configuration file %s parsing error on line %d: invalid parameter \"%s\"", f.fileName().toUtf8().constData(), line_number+1, param_name.toUtf8().constData());
      return false;
    }
  }

  if (!current_rule.query_rx.isEmpty())
    rules.append(current_rule);

  return true;
}

//---------------------------------------------------------------------------
// receives the query and modifies it if neccessary
// returns true if query has been modified
bool processQuery(QByteArray &query, ProxyConnection *conn)
{
  QString query_str = QString::fromUtf8(query);

  bool modified = false;
  for (int rule_index=0; rule_index < rules.count(); rule_index++)
  {
    if (!rules[rule_index].query_rx.exactMatch(query_str))
      continue;

    switch (rules[rule_index].action)
    {
      case QueryRewriteRule::ACT_LOG:
        {
          _qDebug(LOG_SQL_SESSIONS, "Query from %s:%d: %s", conn->peerAddress().toString().toUtf8().constData(), conn->peerPort(), query.constData());
          break;
        }
      case QueryRewriteRule::ACT_REWRITE:
        {
          QString new_query_str = rules[rule_index].rewrite_str;

          bool error = false;
          // replace $(N)
          while (rx_value.indexIn(new_query_str) >= 0)
          {
            int cap_n = rx_value.cap(1).toInt();
            if (cap_n < 1 || cap_n > rules[rule_index].query_rx.captureCount())
            {
              error = true;
              _qWarning(LOG_SQL_SESSIONS, "Query rewrite rule %d error: $(%d) not found in query: %s", rule_index+1, cap_n, query.constData());
              break;
            }
            new_query_str.replace(rx_value.pos(0), rx_value.cap(0).length(), rules[rule_index].query_rx.cap(cap_n));
          }
          // replace $(now...)
          while (rx_now.indexIn(new_query_str) >= 0)
          {
            int diff = !rx_now.cap(2).isEmpty() ? rx_now.cap(2).toInt() : 0;
            if (rx_now.cap(1) == QString("-"))
              diff = -diff;
            QDateTime dt = QDateTime::currentDateTime().toUTC();
            if (rx_now.cap(3) == QString("Y"))
              dt = dt.addYears(diff);
            else if (rx_now.cap(3) == QString("M"))
              dt = dt.addMonths(diff);
            else if (rx_now.cap(3) == QString("D"))
              dt = dt.addDays(diff);
            else if (rx_now.cap(3) == QString("h"))
              dt = dt.addSecs(diff*60*60);
            else if (rx_now.cap(3) == QString("m"))
              dt = dt.addSecs(diff*60);
            else if (rx_now.cap(3) == QString("s"))
              dt = dt.addSecs(diff);
            new_query_str.replace(rx_now.pos(0), rx_now.cap(0).length(), dt.toString("yyyy-MM-dd hh:mm:ss.0+00"));
          }
          if (!error)
          {
            _qDebug(LOG_SQL_SESSIONS, "ORIGINAL query: %s", query.constData());
            query_str = new_query_str;
            query = query_str.toUtf8();
            _qDebug(LOG_SQL_SESSIONS, "MODIFIED query (rule %d): %s", rule_index+1, query.constData());
            modified = true;
          }
          break;
        }
      default:
        break;
    }
  }

  return modified;
}
