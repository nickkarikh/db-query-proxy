#include "proxy-server.h"
#include "log.h"
#include "query-processor.h"
#include <QtEndian>

ProxyServer *proxyServer = NULL;

//---------------------------------------------------------------------------
ProxyServer::ProxyServer(QObject *parent): QTcpServer(parent)
{
  // settings
  listen_address = QHostAddress::LocalHost;
  listen_port = 5433;

  dst_server_address = QString("127.0.0.1");
  dst_server_port = 5432;

  client_max_buffer_size = 128*1024*1024;
  server_connect_timeout = 5;
  client_req_rcv_timeout = 5;

  check_timer = new QTimer(this);
  check_timer->setInterval(1000);
  connect(check_timer, SIGNAL(timeout()), this, SLOT(slot_server_check_timeout()));
  check_timer->start();
}

//---------------------------------------------------------------------------
ProxyServer::~ProxyServer()
{
  if (this->isListening())
    this->stop();
}

//---------------------------------------------------------------------------
void ProxyServer::start()
{
  if (this->isListening())
  {
    _qDebug(LOG_PROXY_SERVER, "Stopped TCP listening server on %s:%d", listen_address.toString().toUtf8().constData(), serverPort());
    this->close();
  }

  if (!this->listen(listen_address, listen_port))
  {
    qCritical("Unable to start TCP listening server on %s:%d", listen_address.toString().toUtf8().constData(), listen_port);
    return;
  }
  _qDebug(LOG_PROXY_SERVER, "Started TCP listening server on %s:%d", listen_address.toString().toUtf8().constData(), serverPort());
}

//---------------------------------------------------------------------------
void ProxyServer::stop()
{
  if (this->isListening())
  {
    _qDebug(LOG_PROXY_SERVER, "Stopped TCP listening server on %s:%d", listen_address.toString().toUtf8().constData(), serverPort());
    this->close();
  }

  for (int i=0; i < clients.count(); i++)
    delete clients.at(i);
  clients.clear();
}

//---------------------------------------------------------------------------
void ProxyServer::incomingConnection(int socketDescriptor)
{
  ProxyConnection *client = new ProxyConnection(ProxyConnection::CONN_CLIENT);
  client->setSocketDescriptor(socketDescriptor);
  if (client_max_buffer_size > 0)
    client->setReadBufferSize(client_max_buffer_size);
  else
    client->setReadBufferSize(65535);
  connect(client, SIGNAL(disconnected()), this, SLOT(client_disconnected()));
  connect(client, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(client_error(QAbstractSocket::SocketError)));
  clients.append(client);
  _qDebug(LOG_PROXY_SERVER, "Incoming connection from %s:%d, initializing new endpoint connection to %s:%d",
          client->peerAddress().toString().toUtf8().constData(), client->peerPort(),
          dst_server_address.toUtf8().constData(), dst_server_port);

  ProxyConnection *server = new ProxyConnection(ProxyConnection::CONN_SERVER);
  if (client_max_buffer_size > 0)
    server->setReadBufferSize(client_max_buffer_size);
  else
    server->setReadBufferSize(65535);
  connect(server, SIGNAL(connected()), this, SLOT(slot_connected_to_server()));
  connect(server, SIGNAL(disconnected()), this, SLOT(server_disconnected()));
  connect(server, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(client_error(QAbstractSocket::SocketError)));

  client->paired_conn = server;
  server->paired_conn = client;

  server->t_last_snd.restart();
  server->connectToHost(dst_server_address, dst_server_port);
}

//---------------------------------------------------------------------------
void ProxyServer::slot_connected_to_server()
{
  ProxyConnection *server = static_cast<ProxyConnection *>(sender());
  Q_ASSERT(server != NULL);
  ProxyConnection *client = server->paired_conn;
  _qDebug(LOG_PROXY_SERVER, "Endpoint connected to %s:%d for client %s:%d",
          server->peerAddress().toString().toUtf8().constData(), server->peerPort(),
          client->peerAddress().toString().toUtf8().constData(), client->peerPort());
  if (!server->output_buffer.isEmpty())
    server->flush_output_buffer();
}

//---------------------------------------------------------------------------
void ProxyServer::slot_server_check_timeout()
{
  for (int i=clients.count()-1; i >= 0; i--)
  {
    ProxyConnection *client = clients.at(i);
    ProxyConnection *server = client->paired_conn;

    if (client->state() == QAbstractSocket::ConnectedState &&
        client->t_incomplete_req_started.isValid() &&
        qAbs(client->t_incomplete_req_started.elapsed()) >= client_req_rcv_timeout*1000)
    {
      _qDebug(LOG_PROXY_SERVER, "Request transmission timeout for client %s:%d",
              client->peerAddress().toString().toUtf8().constData(), client->peerPort());
      client->abort();
      continue;
    }

    // check for server connect timeout
    if (server)
    {
      if ((server->state() == QAbstractSocket::ConnectingState ||
           server->state() == QAbstractSocket::HostLookupState) &&
          qAbs(server->t_last_snd.elapsed()) >= server_connect_timeout*1000)
      {
        _qDebug(LOG_PROXY_SERVER, "Endpoint connect timeout for client %s:%d",
                client->peerAddress().toString().toUtf8().constData(), client->peerPort());
        if (client->state() == QAbstractSocket::ConnectedState)
          client->disconnectFromHost();
        else
          client->abort();
        continue;
      }
    }
  }
}

//---------------------------------------------------------------------------
void ProxyServer::client_disconnected()
{
  ProxyConnection *client = static_cast<ProxyConnection *>(sender());
  Q_ASSERT(client != NULL);
  _qDebug(LOG_PROXY_CLIENT, "Client %s:%d has disconnected", client->peerAddress().toString().toUtf8().constData(), client->peerPort());
  clients.removeOne(client);
  client->deleteLater();
}

//---------------------------------------------------------------------------
void ProxyServer::server_disconnected()
{
  ProxyConnection *server = static_cast<ProxyConnection *>(sender());
  ProxyConnection *client = server->paired_conn;
  if (!client)
    return;
  _qDebug(LOG_PROXY_SERVER, "Endpoint for client %s:%d disconnected - closing client connection",
          client->peerAddress().toString().toUtf8().constData(), client->peerPort());
  if (client->state() == QAbstractSocket::ConnectedState)
    client->disconnectFromHost();
  else
    client->abort();
}

//---------------------------------------------------------------------------
void ProxyServer::client_error(QAbstractSocket::SocketError error)
{
  if (error == QAbstractSocket::RemoteHostClosedError)
    return;

  ProxyConnection *client = qobject_cast<ProxyConnection *>(sender());
  _qDebug(LOG_PROXY_CLIENT, "Client %s:%d error: %s", client->peerAddress().toString().toUtf8().constData(), client->peerPort(), client->errorString().toUtf8().constData());
  if (client->state() == QAbstractSocket::ConnectedState)
    client->disconnectFromHost();
  else
    client->abort();
}

//---------------------------------------------------------------------------
void ProxyServer::server_error(QAbstractSocket::SocketError error)
{
  if (error == QAbstractSocket::RemoteHostClosedError)
    return;

  ProxyConnection *server = qobject_cast<ProxyConnection *>(sender());
  ProxyConnection *client = server->paired_conn;
  if (!client)
    return;

  _qDebug(LOG_PROXY_SERVER, "Client %s:%d endpoint error: %s", client->peerAddress().toString().toUtf8().constData(), client->peerPort(), client->errorString().toUtf8().constData());
  if (client->state() == QAbstractSocket::ConnectedState)
    client->disconnectFromHost();
  else
    client->abort();
}






//---------------------------------------------------------------------------
ProxyConnection::ProxyConnection(ConnectionType _type, QObject *parent): QTcpSocket(parent)
{
  type = _type;
  pgsql_startup_msg_sent = false;
  paired_conn = NULL;
  connect(this, SIGNAL(readyRead()), this, SLOT(slot_readyRead()));
  connect(this, SIGNAL(bytesWritten(qint64)), this, SLOT(slot_bytesWritten(qint64)));
  t_last_rcv.start();
  t_last_snd.start();
  t_incomplete_req_started.invalidate();
}

//---------------------------------------------------------------------------
void ProxyConnection::slot_readyRead()
{
  t_last_rcv.restart();
  QByteArray data;
  int max_read_size = readBufferSize()-input_buffer.length();
  if (paired_conn && !paired_conn->output_buffer.isEmpty())
  {
    if (readBufferSize()-paired_conn->output_buffer.length() < max_read_size)
      max_read_size = readBufferSize()-paired_conn->output_buffer.length();
  }

  if (max_read_size > 0)
  {
    data = this->read(max_read_size);
    if (!data.isEmpty())
      _qDebug(LOG_TRAFFIC_BYTES, "%d bytes received from %s:%d", (int)data.length(), peerAddress().toString().toUtf8().constData(), peerPort());
  }

  if (data.isEmpty())
    return;

  if (type == CONN_CLIENT)
  {
    input_buffer.append(data);

    while (!input_buffer.isEmpty())
    {
      if (input_buffer.length() < 5)
      {
        if (!t_incomplete_req_started.isValid())
          t_incomplete_req_started.start();
        return;
      }

      quint8 msg_type = 0;
      int pos = 0;
      if (pgsql_startup_msg_sent)
        msg_type = input_buffer.at(pos++);
      quint32 msg_len = qFromBigEndian<quint32>((const uchar *)input_buffer.constData()+pos);

      if ((uint)input_buffer.length() < pos+msg_len)
      {
        if (!t_incomplete_req_started.isValid())
          t_incomplete_req_started.start();
        return;
      }

      t_incomplete_req_started.invalidate();

      if (!pgsql_startup_msg_sent)
      {
        // SSL request
        if (msg_len == 8 && input_buffer.left(8) == QByteArray::fromHex("0000000804d2162f"))
        {
          input_buffer.remove(0, msg_len+pos);
          output_buffer.append('N');
          flush_output_buffer();
          return;
        }
        pgsql_startup_msg_sent = true;
      }

      // simple query
      if (msg_type == 'Q')
      {
        int query_len = input_buffer.indexOf('\0', 5)-4;
        if (query_len > 0)
        {
          QByteArray query = input_buffer.mid(5, query_len);
          if (processQuery(query, this))
          {
            input_buffer.replace(5, query_len, query.constData(), query.length()+1);
            msg_len = 4+query.length()+1;
            qToBigEndian(msg_len, (uchar *)input_buffer.data()+pos);
          }
        }
      }
      // parse query
      else if (msg_type == 'P')
      {
        int statement_len = input_buffer.indexOf('\0', 5)-4;
        if (statement_len >= 0)
        {
          QByteArray statement = input_buffer.mid(5, statement_len);
          Q_UNUSED(statement);
          int query_len = input_buffer.indexOf('\0', 5+statement_len)-statement_len-4;
          if (query_len > 0)
          {
            QByteArray query = input_buffer.mid(5+statement_len, query_len);
            if (processQuery(query, this))
            {
              input_buffer.replace(5+statement_len, query_len, query.constData(), query.length()+1);
              msg_len -= query_len;
              msg_len += query.length()+1;
              qToBigEndian(msg_len, (uchar *)input_buffer.data()+pos);
            }
          }
        }
      }

      if (paired_conn)
      {
        paired_conn->output_buffer.append(input_buffer.left(pos+msg_len));
        input_buffer.remove(0, pos+msg_len);
        paired_conn->flush_output_buffer();
      }
    }

    return;
  }
  else
  {
    if (paired_conn)
    {
      paired_conn->output_buffer.append(data);
      paired_conn->flush_output_buffer();
    }
  }
}

//---------------------------------------------------------------------------
void ProxyConnection::flush_output_buffer()
{
  if (output_buffer.isEmpty() || state() != QAbstractSocket::ConnectedState)
    return;
  qint64 len = this->write(output_buffer);
  if (len <= 0)
    return;
  _qDebug(LOG_TRAFFIC_BYTES, "%d bytes sent to %s:%d", (int)len, peerAddress().toString().toUtf8().constData(), peerPort());
  output_buffer.remove(0, len);
  t_last_snd.restart();
}

//---------------------------------------------------------------------------
void ProxyConnection::slot_bytesWritten(qint64)
{
  flush_output_buffer();
  if (paired_conn && paired_conn->bytesAvailable() > 0)
    paired_conn->slot_readyRead();
  if (type == CONN_SERVER && !paired_conn && bytesToWrite() == 0 && output_buffer.isEmpty())
    this->deleteLater();
}

//---------------------------------------------------------------------------
ProxyConnection::~ProxyConnection()
{
  _qDebug(LOG_INTERNAL_MEM, "~ProxyConnection()");
  if (paired_conn)
  {
    paired_conn->paired_conn = NULL;

    if (type == CONN_CLIENT)
    {
      if (paired_conn->output_buffer.isEmpty() && paired_conn->bytesToWrite() == 0)
        delete paired_conn;
    }
  }
}
