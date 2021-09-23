#include "networkclient.hpp"

using namespace XMPPBUS::Network;
using namespace std;
using namespace boost::asio::ip;
using namespace boost::asio;

NetworkClient::NetworkClient(boost::shared_ptr<boost::asio::io_service> IOService, 
                     const std::string &host,
                     const int port,
                     const bool ssl,
                     const bool debug_mod)
              :
              ssl_buffer( boost::asio::buffer(read_data_buffer_ssl, read_data_buffer_size) ),
              non_ssl_buffer( boost::asio::buffer(read_data_buffer_non_ssl, read_data_buffer_size) ),
              sync_strand(*IOService)
{
  this->io_service = IOService;
  this->server_host = host;
  this->server_port = port;
  this->ssl_connection = ssl;
  this->debug = debug_mod;
  flushing = false;
}

NetworkClient::~NetworkClient() {};

void NetworkClient::connect() {
  try {
    reset();
    read_data_buffer = read_data_buffer_non_ssl;
    read_data_stream = &read_data_stream_non_ssl;

    tcp::resolver resolver(*io_service);
    tcp::resolver::query query(server_host, std::to_string(server_port));
    boost::system::error_code io_error = boost::asio::error::host_not_found;
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    boost::asio::connect(tcp_socket->lowest_layer(), endpoint_iterator, io_error);

    if (io_error) {
      current_connection_state = ConnectionState::Error;
      last_error = io_error.message();
      sigOnNetworkError();
      return;
    }

    if (ssl_connection) {
      read_data_buffer = read_data_buffer_ssl;
      read_data_stream = &read_data_stream_ssl;

      boost::system::error_code error;
      ssl_socket->handshake(boost::asio::ssl::stream_base::client, error);

      if (error) {
        last_error =  error.message();
        current_connection_state = ConnectionState::Error;
        sigOnNetworkError();
        return;
      }
    }
    current_connection_state = ConnectionState::Connected;
    sigOnNetworkConnect();
  }
  catch(const boost::system::system_error& ex) {
    last_error = ex.what();
    current_connection_state = ConnectionState::Error;
    sigOnNetworkError();
    return;
  }
}

std::string NetworkClient::readData() {
  std::string data = read_data_stream->str();
  read_data_stream->str(string("")); 
  return data;
};

void NetworkClient::writeData(const string &data) {
  if( current_connection_state != ConnectionState::Connected ) return;
  if (debug) {
    std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << std::endl;
    std::cout << data << std::endl;
    std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"  << std::endl;
  }
  {
    boost::unique_lock<boost::shared_mutex> WriteLock(outgoing_data_mutex);
    std::shared_ptr<std::string> SharedData (new std::string(data));
    outgoing_data.push( SharedData );
    if (!flushing) asyncWrite();
  }
  asyncRead();
}

void NetworkClient::disconnect() {
  try {
    tcp_socket->shutdown(boost::asio::socket_base::shutdown_type::shutdown_send);
    boost::system::error_code error;
    tcp_socket->close(error);
    if (!error) {
      current_connection_state = ConnectionState::Disconnected;
      sigOnNetworkDisconnect();
    }
    else {
      last_error = error.message();
      current_connection_state = ConnectionState::Error;
      sigOnNetworkError();
    }
  }
  catch (const boost::system::system_error& ex) {
    last_error = ex.what();
    current_connection_state = ConnectionState::Error;
    sigOnNetworkDisconnect();
  }
}


ConnectionState NetworkClient::getTransportState() {
  return current_connection_state;
}

std::string NetworkClient::lastError() {
  return last_error;
}

void NetworkClient::reset() {
  current_connection_state = ConnectionState::Disconnected;

  ssl_context.reset(new boost::asio::ssl::context(boost::asio::ssl::context::tls));
  tcp_socket.reset(new boost::asio::ip::tcp::socket(*io_service));
  ssl_socket.reset(new boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>(*tcp_socket, *ssl_context));

  read_data_stream = &read_data_stream_non_ssl;
  read_data_buffer = read_data_buffer_non_ssl;
  memset(read_data_buffer_ssl, 0, read_data_buffer_size);
  memset(read_data_buffer_non_ssl, 0, read_data_buffer_size);
  read_data_stream->str(string(""));

  ssl_socket->set_verify_mode(boost::asio::ssl::verify_peer);
  ssl_context->set_default_verify_paths();
  ssl_socket->set_verify_callback(boost::bind(&NetworkClient::verifyCertificate, this, _1, _2));
}

void NetworkClient::asyncRead() {
  if (ssl_connection) {
    sync_strand.post([&, this] {
      boost::asio::async_read(*ssl_socket, ssl_buffer,
                              boost::asio::transfer_at_least(1),
                              sync_strand.wrap(
                                boost::bind(&NetworkClient::handleRead,
                                this,
                                tcp_socket.get(),
                                read_data_buffer_ssl,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred)
                              ));
    });
  }
  else {
    boost::asio::async_read(*tcp_socket, non_ssl_buffer,
      boost::asio::transfer_at_least(1),
        sync_strand.wrap(boost::bind(&NetworkClient::handleRead,
                                     this,
                                     tcp_socket.get(),
                                     read_data_buffer_non_ssl,
                                     boost::asio::placeholders::error,
                                     boost::asio::placeholders::bytes_transferred)
                                  ));
  }
}

void NetworkClient::handleRead(
    boost::asio::ip::tcp::socket *active_socket,
    char * active_data_buffer,
    const boost::system::error_code& error,
    std::size_t bytes_transferred)
{
  if (error == boost::system::errc::operation_canceled) return;
  if( current_connection_state != ConnectionState::Connected ) return;

  read_data_buffer = active_data_buffer;

  if (read_data_buffer == read_data_buffer_ssl) read_data_stream = &read_data_stream_ssl;
  else read_data_stream = &read_data_stream_non_ssl;

  if (error) {
    last_error = error.message();
    current_connection_state = ConnectionState::Error;
    sigOnNetworkError();
    return;
  }

  if( bytes_transferred > 0 ) {
    std::stringstream tss;
    tss.write(read_data_buffer, bytes_transferred);
    read_data_stream->str(read_data_stream->str() + tss.str());
    if (debug) {
      std::cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << std::endl;
      std::cout << read_data_stream->str() << std::endl;
      std::cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << std::endl;
    }
    sigOnNetworkData();
  }
  asyncRead();
}

void NetworkClient::asyncWrite() {
  if(outgoing_data.empty()) return;

  flushing = true;

  std::shared_ptr<std::string> data = outgoing_data.front();
  outgoing_data.pop();

  boost::system::error_code ec;
  if (ssl_connection) {
    sync_strand.post([&,data, this] {
      boost::asio::async_write(*ssl_socket, boost::asio::buffer(*data),
        sync_strand.wrap(
        boost::bind(&NetworkClient::handleWrite,
          this,
          tcp_socket.get(),
          data,
          boost::asio::placeholders::error)
      ));
    });
  }
  else {
    boost::asio::async_write(*tcp_socket, boost::asio::buffer(*data),
      sync_strand.wrap(boost::bind(&NetworkClient::handleWrite,
        this,
        tcp_socket.get(),
        data,
        boost::asio::placeholders::error)
      ));
  }
}

void NetworkClient::handleWrite(boost::asio::ip::tcp::socket *active_socket,
                                std::shared_ptr<std::string> data,
                                const boost::system::error_code &error)
{
  if( current_connection_state != ConnectionState::Connected ) return;

  if (error == boost::system::errc::operation_canceled) {
    return;
  }

  if (error) {
    last_error = error.message();
    current_connection_state = ConnectionState::Error;
    sigOnNetworkError();
    return;
  }

  {
    boost::unique_lock<boost::shared_mutex> WriteLock(outgoing_data_mutex);
    if (outgoing_data.empty()) {
      flushing = false;
      return;
    }
    asyncWrite();
  }
}

bool NetworkClient::verifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx) {
  return preverified;
}

