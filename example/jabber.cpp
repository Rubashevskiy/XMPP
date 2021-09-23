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

// Разукрашиваем консоль разными цветами
const std::string black("\033[0;30m"); //black
const std::string red("\033[1;22;31m"); //red 
const std::string green("\033[1;32m"); //green
const std::string yellow("\033[1;33m"); //yellow
const std::string blue("\033[0;34m"); // blue
const std::string magenta("\033[0;35m"); // magenta
const std::string cyan("\033[0;36m"); //cyan
const std::string white("\033[0;37m"); //white
const std::string reset("\033[0m"); //reset

const int def_count_reconnect = 6;

// Очистка консоли от предыдущих сообщений
void clrscr(void) {
  printf("\033[2J"); /* Clear the entire screen. */
  printf("\033[0;0f"); /* Move cursor to the top left hand corner*/
}

class Jabber {
  public:
    Jabber(const Config user_config) : config(user_config) {}
    void connect() {
      xmpp.reset(new XMPP(config)); // Основной класс  Jabber
      // Соединяем сигналы xmpp, со слотами приложения
      conn_xmpp_connect = xmpp->sigOnConnect.connect(boost::bind(&Jabber::OnConnect, this));
      conn_xmpp_disconnect = xmpp->sigOnDisconnect.connect(boost::bind(&Jabber::OnDisconnect, this));
      conn_xmpp_roster = xmpp->sigOnRosterUpdate.connect(boost::bind(&Jabber::OnRoster, this, _1));
      conn_xmpp_recv_message = xmpp->sigOnReceivedMessage.connect(boost::bind(&Jabber::OnReceivedMessage, this, _1, _2, _3));
      conn_xmpp_send_message = xmpp->sigOnSendMessage.connect(boost::bind(&Jabber::OnSendMessage, this, _1, _2, _3));
      conn_xmpp_recv_disp_message = xmpp->sigOnReceivedDisplayed.connect(boost::bind(&Jabber::OnReceivedDisplayed, this, _1, _2));
      conn_xmpp_send_disp_message = xmpp->sigOnSendDisplayed.connect(boost::bind(&Jabber::OnSendDisplayed, this, _1, _2));
      conn_xmpp_error = xmpp->sigOnError.connect(boost::bind(&Jabber::OnError, this, _1));
      // Все готово соединяемся с сервером
      xmpp->connect();
      sleep(1000);
    }

    void disconnect() {
      xmpp->disconnect();
      if (conn_xmpp_connect.connected()) conn_xmpp_connect.disconnect();
      if (conn_xmpp_disconnect.connected()) conn_xmpp_disconnect.disconnect();
      if (conn_xmpp_roster.connected()) conn_xmpp_roster.disconnect();
      if (conn_xmpp_recv_message.connected()) conn_xmpp_recv_message.disconnect();
      if (conn_xmpp_send_message.connected()) conn_xmpp_send_message.disconnect();
      if (conn_xmpp_recv_disp_message.connected()) conn_xmpp_recv_disp_message.disconnect();
      if (conn_xmpp_send_disp_message.connected()) conn_xmpp_send_disp_message.disconnect();
      if (conn_xmpp_error.connected()) conn_xmpp_error.disconnect();
      xmpp.reset();
    }

    void sendMessage(std::string jid_to, std::string body) {
      xmpp->sendMessage(jid_to, body);
    }
  private:
    // Слот XMPP подключился
    void OnConnect() {
      std::cout << green << "XMPP OnConnect" << reset <<std::endl;
      reconnect_cnt = def_count_reconnect;
    }

    // Слот XMPP отключился
    void OnDisconnect() {
      std::cout << red <<"XMPP OnDisconnect" << reset << std::endl;
    }

    // Слот, обновлен ростер
    void OnRoster(std::map<std::string, Contact> client_roster) {
      clrscr();
      roster = client_roster;
      std::cout << green << "Roster List: "  << reset << std::endl;
      for (std::pair<std::string, Contact> element : roster) {
        Contact contact_elm = element.second;
        std::string jid = contact_elm.bareJid();
        std::string name = (roster[jid].name.empty()) ? ("no user name") : (roster[jid].name);
        std::string group = (roster[jid].group.empty()) ? ("no group") : (roster[jid].group);
        std::string show_status = roster[jid].show_status;
        std::string msg_status = roster[jid].msg_status;
        std::cout << "-----------------------------------------------" << std::endl;
        std::cout << green << "  JID: "  << jid << reset << std::endl;
        std::cout << green << "  NAME: " << name << reset << std::endl;
        std::cout << green << "  GROUP: " << group << reset << std::endl;
        std::cout << green << "  STATUS: " << show_status << reset << std::endl;
        std::cout << green << "  MSG_STATUS: " << msg_status << reset << std::endl;
        std::cout << "-----------------------------------------------" << std::endl;
      }
    }

    // Слот "контакт" прислал сообщение
    void OnReceivedMessage(std::string msg_id, std::string msg_jid, std::string msg_body) {
      std::string jid = Contact::bareJid(msg_jid);
      // Текст сообщения
      std::cout << "--------------------------------------------------------" << std::endl;
      std::cout << green << "  MESSAGE ID: " << msg_id << reset << std::endl;
      std::cout << green << "  CONTACT: " << jid << reset << std::endl;
      std::cout << green << "  MESSAGE: " << msg_body << reset << std::endl;
      std::cout << "--------------------------------------------------------" << std::endl;
      // Отсылаем подтверждение о прочтении
      xmpp->sendDisplayedMessage(msg_jid, msg_id);
      // Ответочка(отправляем сообщение) 
      sendMessage(msg_jid, "HELLO I`m Jabber bot");
    }

    // Слот, сообщение отправленно клиенту
    void OnSendMessage(std::string msg_id, std::string msg_jid, std::string msg_body) {
      std::string jid = Contact::bareJid(msg_jid);
      std::cout << "--------------------------------------------------------" << std::endl;
      std::cout << green << "  MESSAGE: " << msg_body << reset << std::endl;
      std::cout << green << "  SEND TO: " << jid << reset << std::endl;
      std::cout << green << "  MESSAGE ID : " << msg_id << reset << std::endl;
      std::cout << "--------------------------------------------------------" << std::endl;
    }

    // Слот, клиент прочитал сообщение
    void OnReceivedDisplayed(std::string msg_id, std::string msg_jid) {
      std::string jid = Contact::bareJid(msg_jid);
      std::cout << "--------------------------------------------------------" << std::endl;
      std::cout << green << "  CLIENT: " << jid << reset << std::endl;
      std::cout << green << "  READ MESSAGE" << reset << std::endl;
      std::cout << green << "  MESSAGE ID:" << msg_id << reset << std::endl;
      std::cout << "--------------------------------------------------------" << std::endl;
    }
   // Слот, клиенту отправленно уведомление о прочтении
   void OnSendDisplayed(std::string msg_id, std::string msg_jid) {
     std::string jid = Contact::bareJid(msg_jid);
     std::cout << green << "  SEND 'Displayed' TO: " << jid << reset << std::endl;
     std::cout << green << "  MSG ID: " << msg_id << reset << std::endl;
   }
   // Слот, произошла ошибка
   void OnError(std::string err_msg) {
     std::cout << red << "XMPP ERROR: " << err_msg << reset << std::endl;
     // Реализация реконекта
     if (reconnect_cnt > 0) {
      std::cout << red << "RECONNECT..."  << reset << std::endl;
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
    boost::signals2::connection conn_xmpp_roster;
    boost::signals2::connection conn_xmpp_recv_message;
    boost::signals2::connection conn_xmpp_send_message;
    boost::signals2::connection conn_xmpp_recv_disp_message;
    boost::signals2::connection conn_xmpp_send_disp_message;
    boost::signals2::connection conn_xmpp_error;
    int reconnect_cnt = def_count_reconnect;
};

int main(int argc, char* argv[]) {
  setlocale(LC_ALL, "ru_RU.UTF-8");

  Config config; //Настройки нашего бота

  // Можно прочитать настройки все(часть) из XML
  if (!config.read("./config/config.xml")) {
    std::cout << red <<"ERROR: READ CONFIG FILE" << reset << std::endl;
    exit(1);
  }
  /*
  // Можно задать настройки вручную
  config.jid = "user_name";  // Имя пользователя
  config.host = "jabber.ru"; // Имя сервера (тестировалось на jabber.ru)
  config.port = 5223; // Порт
  config.password = "password"; // Пароль
  config.resource = "JabberTest"; // Имя клиента
  config.show_status = "online"; // Статус {online, dnd, xa, away, chat, unknown}
  config.msg_status = "My Jabber lite bot"; // Дополнительный статус(текст)
  config.priority = 5; // Приоритет
  config.ssl = true; // Использование SSL(не TLS)
  config.auth = AuthType::AUTO; // Тип авторизации (AUTO, Plain, DigestMD5, SCRAM_SHA1)
  config.keep_alive = true; // Поддержка подключения постоянно активным(keep_alive)
  config.debug = true; // Дебаг режим(вывод в консоль весь поток)
  */
  // Наш бот
  Jabber jabber_bot(config);
  // Подключаемся
  jabber_bot.connect();
  while(1) {}
  return 0;
}
