#ifndef CONTACT_H
#define CONTACT_H

#include <string>

namespace XMPPBUS {

  struct Contact {
    Contact() {}
    Contact(const Contact &other) {
      this->jid = other.jid;
      this->show_status = other.show_status;
      this->msg_status = other.msg_status;
      this->name = other.name;
      this->group = other.group;
    }
    std::string user() {
      auto pos_user = jid.find("@");
      if (std::string::npos != pos_user) return jid.substr(0, pos_user);
      else return "";
    }
    std::string resource() {
      auto pos_res = jid.find("/");
      if (std::string::npos != pos_res) return jid.substr(pos_res);
      else return "";
    }

    std::string bareJid() {
      return bareJid(jid);
    }

    static std::string bareJid(std::string &othe_jid) {
      auto pos_res = othe_jid.find("/");
      if (std::string::npos != pos_res) return othe_jid.substr(0, pos_res);
      else return othe_jid;
    }

    std::string jid;
    std::string show_status;
    std::string msg_status;
    std::string name;
    std::string group;
  };

}

#endif