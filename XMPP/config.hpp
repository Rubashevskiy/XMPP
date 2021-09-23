#ifndef JID_H
#define JID_H

#include <string>

#include "../pugixml/pugixml.hpp"

#include "authorization.hpp"

namespace XMPPBUS {

  struct Config {
    Config() {}

    Config(const Config &other) {
      this->jid = other.jid;
      this->host = other.host;
      this->password = other.password;
      this->resource = other.resource;
      this->show_status = other.show_status;
      this->msg_status = other.msg_status;
      this->auth = other.auth;
      this->port = other.port;
      this->priority = other.priority;
      this->keep_alive = other.keep_alive;
      this->ssl = other.ssl;
      this->debug = other.debug;
    }

    std::string getJid() {
      if (resource.empty()) return jid + "@" + host;
      else return jid + "@" + host + "/" + resource;
    }
    
    bool read(const std::string &config_path) {
      try {
        pugi::xml_document xdoc;
        pugi::xml_parse_result load_result = xdoc.load_file(config_path.c_str(), pugi::parse_full&~pugi::parse_eol, pugi::encoding_auto);
        if (!load_result) return false;
        std::string buffer;
        pugi::xml_node jid_node = xdoc.select_single_node("//jid").node();
        jid = xdoc.select_single_node("//jid").node().child_value();
        host = xdoc.select_single_node("//host").node().child_value();
        buffer = xdoc.select_single_node("//port").node().child_value();
        port = std::stoi(buffer);
        password = xdoc.select_single_node("//password").node().child_value();
        buffer = xdoc.select_single_node("//auth").node().child_value();
        if ("DigestMD5" == buffer) auth = AuthType::DigestMD5;
        else if ("SCRAM_SHA1" == buffer) auth = AuthType::SCRAM_SHA1;
        else if ("Plain" == buffer) auth = AuthType::Plain;
        else auth = AuthType::AUTO;
        buffer = xdoc.select_single_node("//ssl").node().child_value();
        ssl = ("true" == buffer) ? (true) : (false);
        resource = xdoc.select_single_node("//resource").node().child_value();
        show_status = xdoc.select_single_node("//show_status").node().child_value();
        msg_status = xdoc.select_single_node("//msg_status").node().child_value();
        buffer = xdoc.select_single_node("//priority").node().child_value();
        priority = std::stoi(buffer);
        buffer = xdoc.select_single_node("//keep_alive").node().child_value();
        keep_alive = ("true" == buffer) ? (true) : (false);
        buffer = xdoc.select_single_node("//debug").node().child_value();
        debug = ("true" == buffer) ? (true) : (false);
        return true;
      }
      catch (...) {
        return false;
      }

    }

    std::string jid;
    std::string host;
    std::string password;
    std::string resource;
    std::string show_status;
    std::string msg_status;
    AuthType auth;
    int  port;
    int priority;
    bool keep_alive;
    bool ssl;
    bool debug;
  };
}

#endif