#ifndef QUERYPROCESSOR_H
#define QUERYPROCESSOR_H

#include <QString>
#include "proxy-server.h"

bool readConfig(const QString &filename);
bool processQuery(QByteArray &query, ProxyConnection *conn);

#endif // QUERYPROCESSOR_H

