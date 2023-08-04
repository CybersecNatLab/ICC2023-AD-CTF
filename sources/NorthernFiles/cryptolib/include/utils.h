#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <cstdlib>
#include <string>

#include "cryptopp/integer.h"
#include "cryptopp/osrng.h"
#include "cryptopp/cryptlib.h"
#include "cryptopp/sha.h"
#include "cryptopp/hex.h"
#include "cryptopp/rijndael.h"
#include "cryptopp/modes.h"
#include "cryptopp/filters.h"
#include "cryptopp/gcm.h"
#include "cryptopp/rsa.h"

const int TAG_SIZE = 16;

const CryptoPP::Integer protocol_p("139819559489970727773674514602432894408386272642962276533082166612215714937961147220287901099797128817068030425326484141320785278983839593637016322911208624564325864627844438289920030667788568549384478133762504762305164349026988017587624141607584664958660506073490093963772276523455507226281029423012934325287");
const CryptoPP::Integer protocol_q("69909779744985363886837257301216447204193136321481138266541083306107857468980573610143950549898564408534015212663242070660392639491919796818508161455604312282162932313922219144960015333894284274692239066881252381152582174513494008793812070803792332479330253036745046981886138261727753613140514711506467162643");
const CryptoPP::Integer protocol_g("2");

struct ClientStep1
{
    std::string user_id;
    std::string X_u;
    std::string alpha;
};

struct ClientStep2
{
    std::string A_u;
};

struct ServerStep1
{
    std::string beta;
    std::string C;
    std::string X_s;
};

struct ClientStorage
{
    std::string r;
    std::string user_id;
    std::string X_u;
    std::string alpha;
    std::string x_u;
};

struct ServerStorage
{
    std::string K_sess;
    std::string id1;
};

struct Capsule
{
    bool success;
    std::string user_id;
    std::string ks;
    std::string ps;
    std::string Ps;
    std::string Pu;
    std::string C;
    std::string sk;
    std::string pk;
};

CryptoPP::Integer string_to_hashed_int(std::string password, CryptoPP::Integer p);
std::string key_from_int(CryptoPP::Integer x);
std::string string_to_hash(std::string password);
std::string kdf(std::string key, std::string id, std::string content);
std::string generate_random_key();
std::vector<std::string> unpack_values(std::string inp);
std::string aes_256_encrypt(std::string key, std::string msg, std::string ad);
std::string aes_256_decrypt(std::string key, std::string full_ctx);
std::string privkey_from_capsule(Capsule user, std::string password);

#endif