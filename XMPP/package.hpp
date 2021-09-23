#ifndef PACKET_HPP
#define PACKET_HPP

#include <sstream>
#include <ostream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <stdlib.h>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

#include "../pugixml/pugixml.hpp"

namespace XMPPBUS {

  enum class PackageType {
    HELLO,
    AUTH,
    IQ,
    IQ_BIND,
    IQ_SESSION,
    IQ_ROSTER,
    PRESENCE,
    MESSAGE,
    NOVALID,
    SKIP,
    CLEAR,
    STOP
  };

  class Package {
    public:
      Package();
      std::string packageHello(const std::string user, const std::string host);
      std::string packageBindResource(const std::string resource);
      std::string packageNewSession();
      std::string packageRoster();
      std::string packageShow(const std::string show);
      std::string packagePriority(const int priority);
      std::string packageStatus(const std::string status);
      std::string packageMessage(const std::string id, const std::string to, const std::string from, const std::string body);
      std::string packageNoDiscoInfo(const std::string id, const std::string to, const std::string from);
      std::string packageDisplayedMessage(const std::string id, const std::string to, const std::string from, const std::string id_msg);
      std::string packageEndSession();
      std::vector<std::string> packageDivData(std::string &buffer);
      PackageType validatePack(const std::string data, pugi::xml_document &xdoc);
      ~Package();
    private:
      std::string bind_id;
      std::string session_id;
      std::string roster_id;
  };
}

#endif
