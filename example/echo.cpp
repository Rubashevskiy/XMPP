#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <cstring>
#include <vector>

#include "../XMPP/xmpp.hpp"
#include "../XMPP/config.hpp"

#include <boost/bind.hpp>
#include "boost/signals2.hpp"
#include <sstream>
#include <atomic>


using namespace std;
using namespace XMPPBUS;

const int def_count_reconnect = 6;


class Echo {
  public:
    Echo(const Config user_config) : config(user_config) {}
    void connect() {
      xmpp.reset(new XMPP(config)); // Основной класс  Jabber
      // Соединяем сигналы xmpp, со слотами приложения
      conn_xmpp_connect = xmpp->sigOnConnect.connect(boost::bind(&Echo::OnConnect, this));
      conn_xmpp_disconnect = xmpp->sigOnDisconnect.connect(boost::bind(&Echo::OnDisconnect, this));
      conn_xmpp_recv_message = xmpp->sigOnReceivedMessage.connect(boost::bind(&Echo::OnReceivedMessage, this, _1, _2, _3));
      conn_xmpp_error = xmpp->sigOnError.connect(boost::bind(&Echo::OnError, this, _1));
      // Все готово соединяемся с сервером
      xmpp->connect();
      sleep(1000);
    }

    void disconnect() {
      xmpp->disconnect();
      if (conn_xmpp_connect.connected()) conn_xmpp_connect.disconnect();
      if (conn_xmpp_disconnect.connected()) conn_xmpp_disconnect.disconnect();
      if (conn_xmpp_recv_message.connected()) conn_xmpp_recv_message.disconnect();
      if (conn_xmpp_error.connected()) conn_xmpp_error.disconnect();
      xmpp.reset();
    }

    void sendMessage(std::string jid_to, std::string body) {
      xmpp->sendMessage(jid_to, body);
    }
  private:
    // Слот XMPP подключился
    void OnConnect() {
      std::cout  << "XMPP OnConnect"  << std::endl;
      reconnect_cnt = def_count_reconnect;
    }

    // Слот XMPP отключился
    void OnDisconnect() {
      std::cout <<"XMPP OnDisconnect"  << std::endl;
    }

    // Слот "контакт" прислал сообщение
    void OnReceivedMessage(std::string msg_id, std::string msg_jid, std::string msg_body) {
      std::string jid = Contact::bareJid(msg_jid);
      std::cout << "  CONTACT: " << jid << " SEND MASSAGE: " << msg_body  << std::endl;
      // Отсылаем подтверждение о прочтении
      xmpp->sendDisplayedMessage(msg_jid, msg_id);
      // Ответочка(отправляем сообщение обратно)
      sendMessage(msg_jid, msg_body);
    }

   // Слот, произошла ошибка
   void OnError(std::string err_msg) {
     std::cout << "XMPP ERROR: " << err_msg  << std::endl;
     // Реализация реконекта
     if (reconnect_cnt > 0) {
      std::cout << "RECONNECT..." << std::endl;
      --reconnect_cnt;
      disconnect();
      sleep(60000);
      connect();
     }
   }
  private:
    boost::shared_ptr<XMPP> xmpp;
    std::map<std::string, Contact> roster;
    Config config;
    boost::signals2::connection conn_xmpp_connect;
    boost::signals2::connection conn_xmpp_disconnect;
    boost::signals2::connection conn_xmpp_recv_message;
    boost::signals2::connection conn_xmpp_error;
    int reconnect_cnt = def_count_reconnect;
};

int main(int argc, char* argv[]) {
  setlocale(LC_ALL, "ru_RU.UTF-8");

  Config config; //Настройки нашего бота
  /*
  // Можно прочитать настройки все(часть) из XML
  if (!config.read("./config/config.xml")) {
    std::cout  <<"ERROR: READ CONFIG FILE" << reset << std::endl;
    exit(1);
  }
  */

  // Можно задать настройки вручную
  config.jid = "user_name";  // Имя пользователя
  config.host = "jabber.ru"; // Имя сервера (тестировалось на jabber.ru)
  config.port = 5223; // Порт
  config.password = "password"; // Пароль
  config.resource = "EchoXMPP"; // Имя клиента
  config.show_status = "online"; // Статус {online, dnd, xa, away, chat, unknown}
  config.msg_status = "My XMPP ECHO BOT"; // Дополнительный статус(текст)
  config.priority = 5; // Приоритет
  config.ssl = true; // Использование SSL(не TLS)
  config.auth = AuthType::AUTO; // Тип авторизации (AUTO, Plain, DigestMD5, SCRAM_SHA1)
  config.keep_alive = true; // Поддержка подключения постоянно активным(keep_alive)
  config.debug = false; // Дебаг режим(вывод в консоль весь поток)

  // Наш бот
  Echo echo(config);
  // Подключаемся
  echo.connect();
  // Уходим в бесконечный цикл
  while(true) {}
  return 0;
}
