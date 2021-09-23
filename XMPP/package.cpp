#include "package.hpp"

using namespace XMPPBUS;

Package::Package() {};

std::string Package::packageHello(const std::string user, const std::string host) {
  std::stringstream ss;
  ss << "<?xml version='1.0' encoding='utf-8'?>"
     << "<stream:stream"
     << " from = '" << user << "'"
     << " to = '" << host << "'"
     << " version='1.0'"
     << " xml:lang='en'"
     << " xmlns='jabber:client'"
     << " xmlns:stream='http://etherx.jabber.org/streams'>";
  return ss.str();
}

std::string Package::packageBindResource(const std::string resource) {
  bind_id = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
  std::stringstream ss;
  ss << "<iq type='set' "
     << "id='" << bind_id << "'>"
     << "<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'>"
     << "<resource>" << resource << "</resource>"
     << "</bind>"
     << "</iq>";
  return ss.str();
}

std::string Package::packageNewSession() {
  session_id = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
  std::stringstream ss;
  ss << "<iq type='set' "
     << "id='" << session_id << "'>"
     << "<session xmlns='urn:ietf:params:xml:ns:xmpp-session'/>"
     << "</iq>";  
  return ss.str(); 
}

std::string Package::packageRoster() {
  roster_id = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
  std::stringstream ss;
  ss << "<iq type='get' "
      << "id='" << roster_id << "'>"
      << "<query xmlns='jabber:iq:roster'/>"
      << "</iq>";
      return ss.str(); 
    }

std::string Package::packageShow(const std::string show) {
  std::stringstream ss;
  ss << "<presence><show>"
     << show
     << "</show></presence>";
  return ss.str();
}

std::string Package::packagePriority(const int priority) {
  std::stringstream ss;
  ss << "<presence><priority>" 
     << priority 
     << "</priority></presence>";
  return ss.str();
}

std::string Package::packageStatus(const std::string status) {
  std::stringstream ss;
  ss << "<presence><status>"
     << status 
     << "</status></presence>";
  return ss.str();
}

std::string Package::packageMessage(const std::string id, const std::string to, const std::string from, const std::string body) {
  std::stringstream ss;
  ss << "<message type='chat' "
     << " to = '" << to << "'"
     << " id = '" << id << "'"
     << " from = '" << from << "'>"
     << "<body>" << body << "</body>"
     << "</message>";
  return ss.str();
}

std::string Package::packageDisplayedMessage(const std::string id, const std::string to, const std::string from, const std::string id_msg) {
  std::stringstream ss;
  ss << "<message"
     << " from='" << from << "'"
     << " to='" << to << "'"
     << " xml:lang='en'"
     << " id='" << id << "'"
     << " type='chat'>"
     << "<displayed xmlns='urn:xmpp:chat-markers:0'"
     << " id='" << id_msg << "'/>"
     << "</message>";
  return ss.str();
}

std::string Package::packageNoDiscoInfo(const std::string id, const std::string to, const std::string from) {
  std::stringstream ss;
  ss << "<iq type='error'"
     << " from='" << from << "'"
     << " to='" << to << "'"
     << " id='" << id << "'>"
     << "<query xmlns='http://jabber.org/protocol/disco#info'/>"
     << "<error type='cancel'>"
     << "<item-not-found xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
     << "</error>"
     << "</iq>";
  return ss.str();
}

std::string Package::packageEndSession() {
  std::stringstream ss;
  ss << "<presence type='unavailable'><status>Logged out</status></presence>";
  return ss.str();
}

std::vector<std::string> Package::packageDivData(std::string &buffer) {
  std::vector<std::string> dv{"stream:features", "challenge", "response", "iq", "presence", "message"};
  std::vector<std::string> result{};
  for (const auto &check_dv : dv) {
    std::string st_tg = "<" + check_dv;
    std::string fn_tg = "</" + check_dv +">";
    int i = -1;
    int j = -1;
    do {
      std::string tmp_pack{};
      int i = buffer.find(st_tg);
      int j = buffer.find(fn_tg);
      if ((std::string::npos != i) && (std::string::npos != j)) {
        tmp_pack = buffer.substr(i, j + fn_tg.size());
        buffer = buffer.erase(i, j + fn_tg.size());
        result.push_back(tmp_pack);
      }
    }
    while ((i != -1) && (j != -1));
  }
  boost::trim(buffer);
  if ((!buffer.empty()) && (buffer != " ")) result.push_back(buffer);    
  return result;
}

PackageType Package::validatePack(const std::string data, pugi::xml_document &xdoc) {
  if (std::string::npos != data.find("</stream:stream>")) return PackageType::STOP;
  if (std::string::npos != data.find("<stream:stream")) return PackageType::CLEAR;
  std::stringstream buffer;
  buffer.str(data);
  pugi::xml_parse_result valid = xdoc.load(buffer, pugi::parse_full&~pugi::parse_eol, pugi::encoding_auto);
  if (!valid) return PackageType::NOVALID;
  std::ostringstream o;
  xdoc.save(o, "\t", pugi::format_no_declaration);
  std::string root_name = xdoc.root().first_child().name();
  if ("stream:features" == root_name) return PackageType::HELLO;
  else if ("challenge" == root_name) return PackageType::AUTH;
  else if ("success" == root_name) return PackageType::AUTH;
  else if ("iq" == root_name) {
    pugi::xml_node iq_node = xdoc.select_single_node("//iq").node();
    if (std::string(iq_node.attribute("id").value()) == bind_id) return PackageType::IQ_BIND;
    if (std::string(iq_node.attribute("id").value()) == session_id) return PackageType::IQ_SESSION;
    if (std::string(iq_node.attribute("id").value()) == roster_id) return PackageType::IQ_ROSTER;
    else return PackageType::IQ;
  }
  else if ("presence" == root_name) return PackageType::PRESENCE;
  else if ("message" == root_name) return PackageType::MESSAGE;
  else return PackageType::SKIP;
}

Package::~Package() {}