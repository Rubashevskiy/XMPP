#include "authorization.hpp"

using namespace XMPPBUS;

Authorization::Authorization(const std::string host, const std::string user, const std::string password, const XMPPBUS::AuthType used_auth_type) 
         : auth_host(host), auth_user(user), auth_password(password), auth_type(used_auth_type) {}

void Authorization::setAuthorizationMechanism(pugi::xpath_node_set mechanisms) {
  for (auto it = mechanisms.begin(); it != mechanisms.end(); it++) {
    pugi::xml_node node = it->node();
    std::string mechanism = std::string(node.child_value());
    if      (mechanism == "DIGEST-MD5") suppor_SASL_DigestMD5 = true;
    else if (mechanism == "SCRAM-SHA-1") suppor_SASL_SCRAM_SHA1 = true;
    else if (mechanism == "PLAIN") suppor_SASL_Plain = true;
  }

  if (XMPPBUS::AuthType::AUTO == auth_type) {
    if      (suppor_SASL_DigestMD5)  auth_type = XMPPBUS::AuthType::DigestMD5;
    else if (suppor_SASL_SCRAM_SHA1) auth_type = XMPPBUS::AuthType::SCRAM_SHA1;
    else if (suppor_SASL_Plain)      auth_type = XMPPBUS::AuthType::Plain;
  }
}

XMPPBUS::AuthStatus Authorization::getData(const pugi::xml_document &auth_pack, std::string &data_buffer) {
  switch (auth_type) {
    case XMPPBUS::AuthType::Plain: return dataPLAIN(auth_pack, data_buffer);
    case XMPPBUS::AuthType::DigestMD5: return dataDigestMD5(auth_pack, data_buffer);
    case XMPPBUS::AuthType::SCRAM_SHA1: return dataSCRAMSHA1(auth_pack, data_buffer);
    case  XMPPBUS::AuthType::AUTO: {
      last_error = "ERROR: Authorization type data missing";
      return XMPPBUS::AuthStatus::ERROR;
    }
  }
  last_error = "ERROR: Unknown error";
  return XMPPBUS::AuthStatus::ERROR;
}

std::string Authorization::lastError() {
  return last_error;
}

Authorization::~Authorization() {}

XMPPBUS::AuthStatus Authorization::dataPLAIN(const pugi::xml_document &auth_pack, std::string &data_buffer) {
  if (auth_pack.root().first_child().empty()) {
    if ((XMPPBUS::AuthType::Plain == auth_type) && (!suppor_SASL_Plain)) {
      last_error = "ERROR: SERVER NOT SUPPORT AUTH TYPE<PLAIN>";
      return AuthStatus::ERROR;
    }
    byte data[1024];
    int offset = 0;
    data[offset++] = 0;

    memcpy(data+offset, auth_user.c_str(), auth_user.length());
    offset+=auth_user.length();
    data[offset++] = 0;

    memcpy(data+offset, auth_password.c_str(), auth_password.length());
    offset += auth_password.length();

    std::string encoded_response;
    CryptoPP::ArraySource css(data, offset, true,
      new CryptoPP::Base64Encoder(new CryptoPP::StringSink(encoded_response), false));

    std::stringstream ss;
    ss << "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='PLAIN'>"
       << encoded_response
       << "</auth>";
    data_buffer = ss.str();
    return XMPPBUS::AuthStatus::PROCESS;
  }
  pugi::xml_node challenge = auth_pack.select_single_node("//challenge").node();
  pugi::xml_node success = auth_pack.select_single_node("//success").node();
  pugi::xml_node failure = auth_pack.select_single_node("//failure").node();

  if (success) {
    return AuthStatus::OK;
  }
  else if (failure) {
    last_error = "ERROR: user or password incorrect";
    return AuthStatus::ERROR;
  }
  else if (challenge) {
    last_error = "ERROR: unknown SASL PLAIN protocol";
    return AuthStatus::ERROR;
  }
  else {
    last_error = "ERROR: unknown SASL PLAIN protocol";
    return AuthStatus::ERROR;
  }
}

XMPPBUS::AuthStatus Authorization::dataDigestMD5(const pugi::xml_document &auth_pack, std::string &data_buffer) {
  if (auth_pack.root().first_child().empty()) {
    if ((XMPPBUS::AuthType::DigestMD5 == auth_type) && (!suppor_SASL_DigestMD5)) {
      last_error = "ERROR: SERVER NOT SUPPORT AUTH TYPE<DigestMD5>";
      return AuthStatus::ERROR;
    }
    data_buffer = "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='DIGEST-MD5'/>";
    return XMPPBUS::AuthStatus::PROCESS;
  }
  pugi::xml_node challenge = auth_pack.select_single_node("//challenge").node();
  pugi::xml_node success = auth_pack.select_single_node("//success").node();
  pugi::xml_node failure = auth_pack.select_single_node("//failure").node();

  if (success) {
    return AuthStatus::OK;
  }
  else if (failure) {
    last_error = "ERROR: user or password incorrect";
    return AuthStatus::ERROR;
  }

  std::string challenge_decode = decodeBase64(challenge.child_value());
  std::map<std::string, std::string> challengeMap = challengeTokenize(challenge_decode);

  if (challengeMap.end() != challengeMap.find("rspauth")) {
    data_buffer = "<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl'/>";
    return AuthStatus::PROCESS;
  }

  std::string nonce = challengeMap["nonce"];
  std::string qop = challengeMap["qop"];

  std::string cnonce = boost::lexical_cast<std::string>(boost::uuids::random_generator()());;
  std::string nc = "00000001";

  std::stringstream stream;
  stream << auth_user << ':' << auth_host << ':' << auth_password;
  std::string X = stream.str();

  stream.str("");
  stream << "AUTHENTICATE:xmpp/" << auth_host;
  std::string A2 = stream.str();
  std::string HA1 = getHA1(X, nonce, cnonce);
  std::string HA2 = getMD5Hex(A2);

  stream.str("");
  stream << HA1 << ':' << nonce << ':' << nc << ':' << cnonce << ':' << qop << ':' << HA2;
  std::string KD = stream.str();
  std::string Z = getMD5Hex(KD);
  stream.str("");
  stream << "username=\"" << auth_user << "\""
         << ",realm=\"" << auth_host << "\""
         << ",nonce=\"" << nonce << "\""
         << ",cnonce=\"" << cnonce << "\""
         << ",nc=" << nc
         << ",qop=" << qop
         << ",digest-uri=\"xmpp/"<< auth_host << "\""
         << ",response="<< Z;
  std::string response = encodeBase64(stream.str());

  stream.str("");
  stream << "<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
         << response
         << "</response>";
  data_buffer = stream.str();
  return AuthStatus::PROCESS;
}

XMPPBUS::AuthStatus Authorization::dataSCRAMSHA1(const pugi::xml_document &auth_pack, std::string &data_buffer) {
  if (auth_pack.root().first_child().empty()) {
    if ((XMPPBUS::AuthType::SCRAM_SHA1 == auth_type) && (!suppor_SASL_SCRAM_SHA1)) {
      last_error = "ERROR: SERVER NOT SUPPORT AUTH TYPE<SCRAM_SHA1>";
      return AuthStatus::ERROR;
    }
    selected_nounce = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
    std::stringstream ss;
    ss<< "n,,n=" << auth_user << ",r=" << selected_nounce;
    std::string request = ss.str();
    request = encodeBase64(request);
    ss.str("");
    ss <<  "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='SCRAM-SHA-1'>"
       << request
       << "</auth>";
    data_buffer = ss.str();
    return AuthStatus::PROCESS;
  }
  pugi::xml_node xml_success_node = auth_pack.select_single_node("//success").node();
  pugi::xml_node challenge_node = auth_pack.select_single_node("//challenge").node();
  pugi::xml_node failure_node = auth_pack.select_single_node("//failure").node();

  if (xml_success_node) {
    std::string success_node = decodeBase64(xml_success_node.child_value());
    std::map<std::string, std::string> successMap = challengeTokenize(success_node);
    if (successMap.end() != successMap.find("v")) {
      if (successMap["v"] == server_proof) {
        return AuthStatus::OK;
      }
      else {
        last_error = "ERROR: user or password incorrect";
        return AuthStatus::ERROR;
      }
    }
  }
  else if (failure_node) {
    std::string failure_decode = decodeBase64(failure_node.child_value());
    last_error = "ERROR: user or password incorrect";
    return AuthStatus::ERROR;
  }

  std::string challenge_decode = decodeBase64(challenge_node.child_value());
  std::map<std::string, std::string> challengeMap = challengeTokenize(challenge_decode);

  std::string r = challengeMap["r"];
  std::string s = challengeMap["s"];
  int NrIterations = std::stoi(challengeMap["i"]);
  std::string n = auth_user;

  byte SaltBytes[1024];

  CryptoPP::Base64Decoder decoder;
  decoder.Put((byte*)s.c_str(), s.length());
  decoder.MessageEnd();

  int SaltLength =decoder.Get(SaltBytes, 1024-4);
  SaltBytes[SaltLength++] = 0;
  SaltBytes[SaltLength++] = 0;
  SaltBytes[SaltLength++] = 0;
  SaltBytes[SaltLength++] = 1;

  std::string c = "biws";
  std::string p= "";


  byte Result[CryptoPP::SHA1::DIGESTSIZE ];
  byte tmp[CryptoPP::SHA1::DIGESTSIZE ];
  byte ClientKey[CryptoPP::SHA1::DIGESTSIZE ];
  byte StoredKey[CryptoPP::SHA1::DIGESTSIZE ];
  byte Previous[CryptoPP::SHA1::DIGESTSIZE ];
  byte ClientSignature[CryptoPP::SHA1::DIGESTSIZE ];
  byte ClientProof[CryptoPP::SHA1::DIGESTSIZE ];
  byte ServerKey[CryptoPP::SHA1::DIGESTSIZE ];
  byte ServerSignature[CryptoPP::SHA1::DIGESTSIZE ];

  int digestlength =CryptoPP::SHA1::DIGESTSIZE;

  // Calculate Result
  CryptoPP::HMAC<CryptoPP::SHA1> hmacFromPassword((byte*)auth_password.c_str(),
                                                         auth_password.length());
  hmacFromPassword.CalculateDigest( Result, SaltBytes, SaltLength);

  memcpy(Previous, Result, digestlength);
  for(int i = 1; i < NrIterations; i++) {
    hmacFromPassword.CalculateDigest( tmp, Previous, digestlength);
    for( int j = 0; j < digestlength; j++) {
      Result[j] = Result[j]^tmp[j];
      Previous[j] = tmp[j];
    }
  }

  CryptoPP::HMAC<CryptoPP::SHA1> hmacFromSaltedPassword(Result, digestlength);
  hmacFromSaltedPassword.CalculateDigest( ClientKey, (byte*)"Client Key", strlen("Client Key"));

  CryptoPP::SHA1 hash;
  hash.CalculateDigest( StoredKey, ClientKey, digestlength);

  std::stringstream ss;
  ss << "n=" << n << ",r=" << selected_nounce;
  ss << "," << challenge_decode;
  ss << ",c=" << c;
  ss << ",r=" << r;

  std::string AuthMessage = ss.str();

  CryptoPP::HMAC<CryptoPP::SHA1> hmacFromStoredKey(StoredKey, digestlength);
  hmacFromStoredKey.CalculateDigest( ClientSignature, (byte*)AuthMessage.c_str(), AuthMessage.length());

  for(int i = 0; i < digestlength; i++) {
    ClientProof[i] = ClientKey[i] ^ ClientSignature[i];
  }

  hmacFromSaltedPassword.CalculateDigest( ServerKey,(byte*)"Server Key", strlen("Server Key"));

  CryptoPP::HMAC< CryptoPP::SHA1> hmacFromServerKey(ServerKey, digestlength);
  hmacFromServerKey.CalculateDigest( ServerSignature, (byte*)AuthMessage.c_str(), AuthMessage.length());
              CryptoPP::ArraySource(ServerSignature, digestlength, true,
                                    new CryptoPP::Base64Encoder(
                                    new CryptoPP::StringSink(server_proof), false));

  CryptoPP::ArraySource(ClientProof, digestlength, true,
                        new CryptoPP::Base64Encoder(
                        new CryptoPP::StringSink(p), false));
  ss.str("");
  ss << "c=" << c << ",r=" << r << ",p=" << p;
  std::string Response = ss.str();

  std::string Base64EncodedResponse = encodeBase64(Response);

  ss.str("");
  ss << "<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>"
     << Base64EncodedResponse
     << "</response>";
  data_buffer = ss.str();
  return AuthStatus::PROCESS;
}

std::string Authorization::decodeBase64(std::string input) {
  std::string decoded;
  CryptoPP::StringSource(input, true, new CryptoPP::Base64Decoder(new CryptoPP::StringSink(decoded)));
  return decoded;
}

std::string Authorization::encodeBase64(std::string input) {
  byte data[1024];
  int offset = 0;
  memcpy(data + offset, input.c_str(), input.length());
  offset += input.length();

  std::string encoded_data;
  CryptoPP::ArraySource ss(data, offset, true, new CryptoPP::Base64Encoder(new CryptoPP::StringSink(encoded_data), false));
  return encoded_data;
}

std::string Authorization::getHA1(std::string x, std::string nonce, std::string cnonce) {
  CryptoPP::Weak::MD5 hash;
  byte digest[CryptoPP::Weak::MD5::DIGESTSIZE];
  int digestlength =CryptoPP::Weak::MD5::DIGESTSIZE;
  hash.CalculateDigest( digest, reinterpret_cast<const unsigned char *>(x.c_str()), x.length());

  std::stringstream stream;
  stream << ":" << nonce << ":" << cnonce;
  std::string AuthentiationDetails = stream.str();
  int TLen = (int)digestlength + (int)AuthentiationDetails.length();
  byte *T = new byte[TLen];
  memcpy(T, digest, digestlength);
  memcpy(T+digestlength, AuthentiationDetails.c_str(), AuthentiationDetails.length());
  hash.CalculateDigest( digest, reinterpret_cast<const unsigned char *>( T ), TLen );

  CryptoPP::HexEncoder Hexit;
  std::string TOutput;
  Hexit.Attach( new CryptoPP::StringSink( TOutput ) );

  Hexit.Put( digest, digestlength );
  Hexit.MessageEnd();

  return stringToLower(TOutput);
}

std::string Authorization::getMD5Hex(std::string input) {
  CryptoPP::Weak::MD5 hash;
  byte digest[CryptoPP::Weak::MD5::DIGESTSIZE];
  int length =CryptoPP::Weak::MD5::DIGESTSIZE;

  CryptoPP::HexEncoder Hexit;
  std::string tmp_output;
  Hexit.Attach( new CryptoPP::StringSink(tmp_output));

  hash.CalculateDigest(digest, reinterpret_cast<const unsigned char *>(input.c_str()), input.length());
  Hexit.Put(digest, length);
  Hexit.MessageEnd();
  return stringToLower(tmp_output);
}

std::map<std::string, std::string> Authorization::challengeTokenize(const std::string data) {
  std::map<std::string, std::string> result;
  std::string temp_data = data;
  size_t start_pos = 0;
  while(std::string::npos != (start_pos = temp_data.find("\"", start_pos))) {
    temp_data.replace(start_pos, 1, "");
    start_pos++;
  }
  std::vector<std::string> v_token;
  std::string::size_type pos = 0;
  std::string::size_type prev_pos = 0;
  while ((pos = temp_data.find(',', pos)) != std::string::npos) {
    std::string substring(temp_data.substr(prev_pos, pos-prev_pos));
    v_token.push_back(substring);
    prev_pos = ++pos;
  }
  v_token.push_back(temp_data.substr(prev_pos, pos-prev_pos));

  for (const auto &token : v_token) {
    int pos = token.find('=');
    if (std::string::npos != pos) {
      result[token.substr(0, pos)] = token.substr(pos+1);
    }
  }
  return result;
}

std::string Authorization::stringToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return std::tolower(c);});
  return s;
}
