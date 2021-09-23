#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <string>
#include <sstream>

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/cryptlib.h>
#include <cryptopp/hex.h>
#include <cryptopp/hmac.h>
#include <cryptopp/sha.h>
#include <cryptopp/base64.h>
#include <cryptopp/queue.h>
#include <cryptopp/md5.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

#include "../pugixml/pugixml.hpp"

namespace XMPPBUS {

  enum class AuthType {
    AUTO,
    Plain,
    DigestMD5,
    SCRAM_SHA1
  };

  enum class AuthStatus {
    OK,
    PROCESS,
    ERROR
  };

  class Authorization {
    public:
      Authorization(const std::string host, const std::string user, const std::string password, const XMPPBUS::AuthType used_auth_type);
      void setAuthorizationMechanism(pugi::xpath_node_set mechanisms);
      XMPPBUS::AuthStatus getBeginData(std::string &buffer_data);
      XMPPBUS::AuthStatus setProcessData(const pugi::xml_document &auth_pack, std::string &data_buffer);
      std::string lastError();
      ~Authorization();
    private:
      XMPPBUS::AuthStatus getBeginPLAIN(std::string &data_buffer);
      XMPPBUS::AuthStatus getBeginDigestMD5(std::string &data_buffer);
      XMPPBUS::AuthStatus getBeginSCRAMSHA1(std::string &data_buffer);
      
      XMPPBUS::AuthStatus setDataPLAIN(const pugi::xml_document &auth_pack);
      XMPPBUS::AuthStatus setDataDigestMD5(const pugi::xml_document &auth_pack, std::string &data_buffer);
      XMPPBUS::AuthStatus setDataSCRAMSHA1(const pugi::xml_document &auth_pack, std::string &data_buffer);

      std::string decodeBase64(std::string input);
      std::string encodeBase64(std::string input);
      std::string getHA1(std::string x, std::string nonce, std::string cnonce);
      std::string getMD5Hex(std::string input);
      std::map<std::string, std::string> challengeTokenize(const std::string data);
      std::string stringToLower(std::string s);
    private:
      XMPPBUS::AuthType auth_type;
      std::string auth_host;
      std::string auth_user;
      std::string auth_password;
      std::string last_error;
      std::string selected_nounce;
      std::string server_proof;
      bool suppor_SASL_Plain      = false;
      bool suppor_SASL_DigestMD5  = false;
      bool suppor_SASL_SCRAM_SHA1 = false;
    };
}

#endif