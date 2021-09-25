#ifndef JABBER_HPP
#define JABBER_HPP

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include "boost/signals2.hpp"
#include <boost/enable_shared_from_this.hpp>
#include <boost/any.hpp>

#include <sstream>
#include <iostream>
#include <atomic>
#include <wchar.h>
#include <locale>
#include <vector>
#include <map>

#include "networkclient.hpp"
#include "config.hpp"
#include "contact.hpp"
#include "package.hpp"
#include "authorization.hpp"

namespace XMPPBUS {
  // Статус XMPP клиента
  enum class XMPPStatus {
    None,       // Инициализация
    Connecting, // Подключен к серверу(Транспортный уровень TCP)
    Auth,       // Клиент авторизирован
    Connected,  // Клиент подключен и готов к работе
    Disconnect  // Клиент отключен
  };
  // Класс для работы по протоколу XMPP(тестировался на jabber.ru)
  class XMPP : public boost::enable_shared_from_this<XMPP> {
    public:
      // Конструктор
      XMPP(const Config user_config);
      // Подключение к серверу
      void connect();
      // Отключение от сервера
      void disconnect();
      // Отправка сообщения контакту
      void sendMessage(std::string jid_to, std::string body);
      // Отправка контакту уведомления о прочтении - "две галочки :)"
      void sendDisplayedMessage(const std::string jid_to, const std::string id_msg);
      ~XMPP();
      //{signals}
      boost::signals2::signal<void (void)> sigOnConnect;                                          //Сигнал подключились к серверу
      boost::signals2::signal<void (void)> sigOnDisconnect;                                       //Сигнал отключились от сервера
      boost::signals2::signal<void (std::string)> sigOnError;                                     //Сигнал возникла ошибка
      boost::signals2::signal<void (std::string, std::string, std::string)> sigOnReceivedMessage; //Сигнал полученно сообщение
      boost::signals2::signal<void (std::string, std::string, std::string)> sigOnSendMessage;     //Сигнал отправленно сообщение
      boost::signals2::signal<void (std::string, std::string)> sigOnReceivedDisplayed;            //Сигнал полученно подтверждение о прочтении
      boost::signals2::signal<void (std::string, std::string)> sigOnSendDisplayed;                //Сигнал отправленно подтверждение о прочтении
      boost::signals2::signal<void (std::map<std::string, Contact>)> sigOnRosterUpdate;           //Сигнал обновлен ростер
    private:
      void slotOnTransportConnect();
      void slotOnTransportDisconnect();
      void slotOnTransportError();
      void slotOnTransportData();

      void parseHello(const pugi::xml_document &xdoc);
      void parseAuth(const pugi::xml_document &xdoc);
      void parseRoster(const pugi::xml_document &xdoc);
      void parseIq(const pugi::xml_document &xdoc);
      void parsePresence(const pugi::xml_document &xdoc);
      void parseMessage(const pugi::xml_document &xdoc);

      void startPack();
      void sendKeepAlive();
      void writeData(const std::string &data);
      void run();
    private:
      boost::shared_ptr<boost::asio::io_service> io_service;
      boost::shared_ptr<boost::thread> IOThread;
      boost::shared_ptr<XMPPBUS::Network::NetworkClient> transport;
      boost::shared_ptr<XMPPBUS::Authorization> auth;
      boost::scoped_ptr<boost::asio::deadline_timer> keep_alive_timer;
      boost::shared_ptr<XMPPBUS::Package> package;
      boost::signals2::connection conn_transport_connect;
      boost::signals2::connection conn_transport_disconnect;
      boost::signals2::connection conn_transport_data;
      boost::signals2::connection conn_transport_error;
      boost::posix_time::ptime last_write;
      std::map<std::string, Contact> roster_list;
      Config config;
      XMPPStatus current_xmpp_status = XMPPStatus::None;
      std::string data_buffer;
      bool keep_alive_flag = true;
      const int keep_alive_second = 90;
  };
}

#endif
