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
        static std::string nodeToStr(const pugi::xml_node &node) {
      if (node) {
        return std::string(node.child_value());
      }
      else return std::string("");
    };

    static int nodeToInt(const pugi::xml_node &node) {
      if (node) {
        try {
          return std::stoi(std::string(node.child_value()));
        }
        catch (...) {
          return 0;
        }
      }
      else return 0;
    };

    static bool nodeToBool(const pugi::xml_node &node) {
      if (node) {
        std::string check = std::string(node.child_value());
        if (("true" == check) || ("True" == check) || ("1" == check)) return true;
        else return false;
      }
      else return false;
    };

    static AuthType nodeToAuth(const pugi::xml_node &node) {
      if (node) {
        std::string check = std::string(node.child_value());
        if ("DigestMD5" == check) return AuthType::DigestMD5;
        else if ("SCRAM_SHA1" == check) return AuthType::SCRAM_SHA1;
        else if ("Plain" == check) return AuthType::Plain;
        else return AuthType::AUTO;
      }
      else return AuthType::AUTO;
    };

    bool read(const std::string &config_path) {
      try {
        pugi::xml_document xdoc;
        pugi::xml_parse_result load_result = xdoc.load_file(config_path.c_str(), pugi::parse_full&~pugi::parse_eol, pugi::encoding_auto);
        if (!load_result) return false;
        std::string buffer;
        jid = nodeToStr(xdoc.select_single_node("//jid").node());
        host = nodeToStr(xdoc.select_single_node("//host").node());
        port = nodeToInt(xdoc.select_single_node("//port").node());
        password = nodeToStr(xdoc.select_single_node("//password").node());
        auth = nodeToAuth(xdoc.select_single_node("//auth").node());
        ssl = nodeToBool(xdoc.select_single_node("//ssl").node());
        resource = nodeToStr(xdoc.select_single_node("//resource").node());
        show_status = nodeToStr(xdoc.select_single_node("//show_status").node());
        msg_status = nodeToStr(xdoc.select_single_node("//msg_status").node());
        priority = nodeToInt(xdoc.select_single_node("//priority").node());
        keep_alive = nodeToBool(xdoc.select_single_node("//keep_alive").node());
        debug = nodeToBool(xdoc.select_single_node("//debug").node());
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