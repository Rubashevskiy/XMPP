#ifndef NETWORK_CLIENT_HPP
#define NETWORK_CLIENT_HPP

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/smart_ptr.hpp>
#include <memory>
#include <sstream>
#include <queue>
#include <iostream>
#include <boost/system/error_code.hpp>
#include "boost/signals2.hpp"

namespace XMPPBUS {
 namespace Network { 

  enum class ConnectionState {
    Connected,
    Disconnected,
    Error
  };

   class NetworkClient {
     public:
       NetworkClient(boost::shared_ptr<boost::asio::io_service> IOService, 
                     const std::string &host,
                     const int port,
                     const bool ssl,
                     const bool debug_mod = false);
       ~NetworkClient();
       void connect();
       void disconnect();
       std::string readData();
       void writeData(const std::string &data);
       ConnectionState getTransportState();
       std::string lastError();

       boost::signals2::signal<void (void)> sigOnNetworkConnect; // Сигнал подключения
       boost::signals2::signal<void (void)> sigOnNetworkDisconnect;//Сигнал отключение
       boost::signals2::signal<void (void)> sigOnNetworkData; //Сигнал получили данные
       boost::signals2::signal<void (void)> sigOnNetworkError; //Сигнал ошибка
     private:
       void reset();
       void asyncRead();
       void handleRead(boost::asio::ip::tcp::socket *active_socket,
                      char *active_data_buffer,
                      const boost::system::error_code &error,
                      std::size_t bytes_transferred);
       void asyncWrite();
       void handleWrite(boost::asio::ip::tcp::socket *active_socket,
                       std::shared_ptr<std::string> data,
                       const boost::system::error_code &error);
       bool verifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx);
     private:
       volatile ConnectionState current_connection_state;
       boost::shared_ptr<boost::asio::io_service> io_service;
       boost::scoped_ptr<boost::asio::ssl::context> ssl_context;
       boost::scoped_ptr<boost::asio::ip::tcp::socket> tcp_socket;
       boost::scoped_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>> ssl_socket;
       boost::asio::mutable_buffers_1 ssl_buffer;
       boost::asio::mutable_buffers_1 non_ssl_buffer;
       boost::asio::io_service::strand sync_strand;
       boost::shared_mutex outgoing_data_mutex;
       std::queue<std::shared_ptr<std::string>> outgoing_data;
       std::stringstream *read_data_stream;
       std::stringstream read_data_stream_non_ssl;
       std::stringstream read_data_stream_ssl;
       static const int read_data_buffer_size = 1024;
       char read_data_buffer_non_ssl[read_data_buffer_size];
       char read_data_buffer_ssl[read_data_buffer_size];
       char * read_data_buffer;
       std::string server_host;
       std::string last_error;
       int  server_port;
       bool ssl_connection = false;
       bool debug = false;
       bool flushing;
    };
  }
}

#endif // NETWORK_CLIENT_HPP