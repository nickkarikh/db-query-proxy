#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QTimer>

class ProxyConnection: public QTcpSocket
{
  Q_OBJECT
public:
  enum ConnectionType
  {
    CONN_CLIENT
   ,CONN_SERVER
  };

  ProxyConnection(ConnectionType _type, QObject *parent=NULL);
  ~ProxyConnection();

  ConnectionType type;
  ProxyConnection *paired_conn;

  QElapsedTimer t_last_rcv;
  QElapsedTimer t_last_snd;

  QElapsedTimer t_incomplete_req_started;

  QByteArray input_buffer;
  QByteArray output_buffer;

  void flush_output_buffer();

private slots:
  void slot_readyRead();
  void slot_bytesWritten(qint64 bytes);

private:

  bool pgsql_startup_msg_sent;

};

class ProxyServer: public QTcpServer
{
  Q_OBJECT
public:

  ProxyServer(QObject *parent=NULL);
  ~ProxyServer();

  QHostAddress listen_address;
  quint16 listen_port;

  QString dst_server_address;
  quint16 dst_server_port;

  quint64 client_max_buffer_size;
  int server_connect_timeout;
  int client_req_rcv_timeout;
  QTimer *check_timer;

  QList<ProxyConnection *> clients;

public slots:
  void start();
  void stop();

private slots:
  void client_disconnected();
  void client_error(QAbstractSocket::SocketError);

  void server_disconnected();
  void server_error(QAbstractSocket::SocketError error);

  void slot_connected_to_server();
  void slot_server_check_timeout();

protected:
    void incomingConnection(int socketDescriptor);

private:
};

extern ProxyServer *proxyServer;

#endif // PROXY_SERVER_H
