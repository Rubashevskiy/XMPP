#include "xmpp.hpp"

using namespace std;
using namespace pugi;
using namespace XMPPBUS;

XMPP::XMPP(const Config user_config) : config(user_config) {
  io_service.reset(new boost::asio::io_service());
  transport.reset(new Network::NetworkClient(io_service, config.host, config.port, config.ssl, config.debug));
  auth.reset(new XMPPBUS::Authorization(config.host, config.jid, config.password, config.auth));
  package.reset(new XMPPBUS::Package());
  conn_transport_connect = transport->sigOnNetworkConnect.connect(boost::bind(&XMPP::slotOnTransportConnect, this));
  conn_transport_disconnect = transport->sigOnNetworkDisconnect.connect(boost::bind(&XMPP::slotOnTransportDisconnect, this));
  conn_transport_data = transport->sigOnNetworkData.connect(boost::bind(&XMPP::slotOnTransportData, this));
  conn_transport_error = transport->sigOnNetworkError.connect(boost::bind(&XMPP::slotOnTransportError, this));
}

void XMPP::connect() {
  transport->connect();
  IOThread.reset(new boost::thread(boost::bind(&XMPP::run, this)));
}

void XMPP::disconnect() {
  if (current_xmpp_status  != XMPPStatus::Disconnect) {
    if (transport->getTransportState() != XMPPBUS::Network::ConnectionState::Disconnected) {
      writeData(package->packageEndSession());
      transport->disconnect();
    }
  }
  current_xmpp_status  = XMPPStatus::Disconnect;
}

void XMPP::sendMessage(std::string jid_to, std::string body) {
  std::string id_msg = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
  writeData(package->packageMessage(id_msg, jid_to, config.getJid(), body));
  sigOnSendMessage(id_msg, jid_to, body);
}

void  XMPP::sendDisplayedMessage(const std::string jid_to, const std::string id_msg) {
  std::string id = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
  writeData(package->packageDisplayedMessage(id, jid_to, config.getJid(), id_msg));
  sigOnSendDisplayed(id_msg, jid_to);
}

XMPP::~XMPP() {
  if (conn_transport_connect.connected()) conn_transport_connect.disconnect();
  if (conn_transport_disconnect.connected()) conn_transport_disconnect.disconnect();
  if (conn_transport_data.connected()) conn_transport_data.disconnect();
  if (conn_transport_error.connected()) conn_transport_error.disconnect();

  io_service.reset();
  transport.reset();
  auth.reset();
  package.reset();
  IOThread.reset();
  keep_alive_timer.reset();
}

void XMPP::slotOnTransportConnect() {
  current_xmpp_status = XMPPStatus::Connected;
  writeData(package->packageHello(config.jid, config.host));
}

void XMPP::slotOnTransportDisconnect() {
  if (current_xmpp_status  != XMPPStatus::Disconnect) {
    sigOnDisconnect();
  }
  current_xmpp_status  = XMPPStatus::Disconnect;
};

void XMPP::slotOnTransportError() {
  sigOnError(transport->lastError());
  disconnect();
}

void XMPP::slotOnTransportData() {
  // Установка флага успешного считывания данных
  keep_alive_flag = true;
  // Если отключились игнорим данные
  if (current_xmpp_status  == XMPPStatus::Disconnect) return;
  // Забираем данные из буфера транспорта.
  std::string transport_data = transport->readData();
  // Данные короче 4 символов считаем, как ответ на ping
  if (transport_data.size() < 4) return;
  data_buffer += transport_data;
  // Данные иногда приходят одним пакетом и пакет требуется разбивать на части
  for (auto div_data : package->packageDivData(data_buffer)) {
    pugi::xml_document xdoc;
    switch (package->validatePack(div_data, xdoc)) {
      case PackageType::HELLO: parseHello(xdoc); break;
      case PackageType::AUTH: parseAuth(xdoc); break;
      case PackageType::IQ: parseIq(xdoc); break;
      case PackageType::IQ_BIND: writeData(package->packageNewSession()); break;
      case PackageType::IQ_SESSION: writeData(package->packageRoster()); break;
      case PackageType::IQ_ROSTER: parseRoster(xdoc);
      case PackageType::PRESENCE: parsePresence(xdoc); break;
      case PackageType::MESSAGE: parseMessage(xdoc); break;
      case PackageType::STOP: disconnect(); break;
      case PackageType::SKIP: break;
      case PackageType::CLEAR: data_buffer.clear(); break;
      case PackageType::NOVALID: {
        data_buffer += div_data;
        break;
      }
    }
  }
}

void XMPP::parseHello(const pugi::xml_document &xdoc) {
  if (current_xmpp_status == XMPPStatus::Auth) {
    writeData(package->packageBindResource(config.resource));
  } 
  else {
    auth->setAuthorizationMechanism(xdoc.select_nodes("//mechanism"));
    parseAuth(pugi::xml_document());
  }
}

void XMPP::parseAuth(const pugi::xml_document &xdoc) {
  
  std::string buffer;
  AuthStatus status;

  switch (auth->getData(xdoc, buffer)) {
    case AuthStatus::PROCESS: {
      writeData(buffer);
      break;
    }
    case AuthStatus::OK: {
      current_xmpp_status = XMPPStatus::Auth;
      writeData(package->packageHello(config.jid, config.host));
      break;
    }
    case AuthStatus::ERROR: {
      sigOnError(auth->lastError());
      disconnect();
    }
  }
}

void XMPP::parseRoster(const pugi::xml_document &xdoc) {
  pugi::xml_node query_nodes = xdoc.select_single_node("//query").node();
  for (pugi::xml_node_iterator it = query_nodes.begin(); it != query_nodes.end(); ++it) {
    Contact contact;
    contact.jid = std::string(it->attribute("jid").value());
    contact.name = std::string(it->attribute("name").value());
    contact.show_status = "unknown";
    pugi::xml_node group_node = query_nodes.select_single_node("//group").node();
    if (group_node) contact.group = std::string(group_node.child_value());
    roster_list[contact.bareJid()] = contact;
  }
  sigOnRosterUpdate(roster_list);
  if (XMPPStatus::Connecting != current_xmpp_status) startPack();
}

void XMPP::parseIq(const pugi::xml_document &xdoc) {
  pugi::xml_node iq_nodes = xdoc.select_single_node("//iq").node();
  std::string to = std::string(iq_nodes.attribute("to").value());
  std::string from = std::string(iq_nodes.attribute("from").value());
  std::string type = std::string(iq_nodes.attribute("type").value());
  if (Contact::bareJid(to) != Contact::bareJid(from)) {
    if ("get" == type) {
      pugi::xml_node query_nodes = iq_nodes.select_single_node("//query").node();
      if ("http://jabber.org/protocol/disco#info" == std::string(query_nodes.attribute("xmlns").value())) {
        std::string id = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
        writeData(package->packageNoDiscoInfo(id, from, to));
      }
    }
  }
}

void XMPP::parsePresence(const pugi::xml_document &xdoc) { 
  pugi::xml_node presence = xdoc.select_single_node("//presence").node();
  std::string to = std::string(presence.attribute("to").value());
  std::string from = std::string(presence.attribute("from").value());
  if ((Contact::bareJid(to) == Contact::bareJid(from)) ||
      (roster_list.find(Contact::bareJid(from)) != roster_list.end())) {
    return;
  }
  roster_list[Contact::bareJid(from)].jid = from;
  std::string type = std::string(presence.attribute("type").value());
  if (std::string("unavailable") == type) {
    roster_list[Contact::bareJid(from)].show_status = "unknown";
    sigOnRosterUpdate(roster_list);
    return;
  }
  roster_list[Contact::bareJid(from)].show_status = "online";
  pugi::xml_node show_tag = presence.child("show");
  if (show_tag) roster_list[Contact::bareJid(from)].show_status = show_tag.text().as_string();
  pugi::xml_node status_tag = presence.child("status");
  if (status_tag) roster_list[Contact::bareJid(from)].msg_status = status_tag.text().as_string();
  sigOnRosterUpdate(roster_list);
}

void XMPP::parseMessage(const pugi::xml_document &xdoc) {
  pugi::xml_node message = xdoc.select_single_node("//message").node();
  std::string id = message.attribute("id").value();
  std::string from = std::string(message.attribute("from").value());
  pugi::xml_node body = message.child("body");
  if (body)  {
    sigOnReceivedMessage(id, from, body.text().as_string());
    return;
  }
  pugi::xml_node displayed = message.select_single_node("//displayed").node();
  if (displayed) {
    std::string id_displayed = displayed.attribute("id").value();
    sigOnReceivedDisplayed(id_displayed, from);
  }
}

void XMPP::writeData(const std::string &data) {
  transport->writeData(data);
  last_write = boost::posix_time::microsec_clock::universal_time();
}

void XMPP::sendKeepAlive() {
  if (keep_alive_timer == nullptr) return;

  if (last_write > (boost::posix_time::microsec_clock::universal_time() - boost::posix_time::seconds(keep_alive_second))) {
     keep_alive_timer->expires_from_now(boost::posix_time::seconds(keep_alive_second));
     keep_alive_timer->async_wait(boost::bind(&XMPP::sendKeepAlive, this));
     return;
  }
  keep_alive_timer->async_wait(boost::bind(&XMPP::sendKeepAlive, this));
  if (!keep_alive_flag) {
    sigOnError("ERROR: KEEP ALIVE(NO PING DATA)");
    disconnect();
  }
  writeData(" ");
  keep_alive_flag = false;
}

void XMPP::startPack() {
  writeData(package->packageShow(config.show_status));
  writeData(package->packagePriority(config.priority));
  writeData(package->packageStatus(config.msg_status));
  if (config.keep_alive) {
    keep_alive_timer.reset(new boost::asio::deadline_timer(*io_service, boost::posix_time::seconds(keep_alive_second)));
    keep_alive_flag = true;
    sendKeepAlive();
  }
  current_xmpp_status = XMPPStatus::Connecting;
  sigOnConnect();
}

void XMPP::run() {
  try {
    io_service->run();
  }
  catch(...) {
    if (current_xmpp_status  != XMPPStatus::Disconnect) {
      sigOnError("ERROR: Failed to io_service");
      disconnect();
    }
  }
}
