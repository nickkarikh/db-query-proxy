#ifndef LOG_H
#define LOG_H

#include <QFile>

extern int log_categories;
extern QFile f_log;

enum LogCategories {
  LOG_GENERAL          = 0x00000001
 ,LOG_INTERNAL_PROC    = 0x00000002
 ,LOG_INTERNAL_MEM     = 0x00000004
 ,LOG_SQL_SESSIONS     = 0x00000010
 ,LOG_PROXY_SERVER     = 0x00000100
 ,LOG_PROXY_CLIENT     = 0x00000200
 ,LOG_TRAFFIC_BYTES    = 0x00000400
};

extern int log_verbosity;

#define _qDebug(category, str, args...) if ((log_categories & (category)) == (category)) qDebug(str, ##args);
#define _qWarning(category, str, args...) if ((log_categories & (category)) == (category)) qWarning(str, ##args);

void log_init();
void log_stop();

#endif // LOG_H
