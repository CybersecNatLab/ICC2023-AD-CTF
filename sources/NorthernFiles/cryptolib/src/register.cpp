#include "register.h"

Capsule register_user(std::string username, std::string password) {
    Capsule result;
    bool success = false;

    CryptoPP::AutoSeededRandomPool prng;
    auto ks = CryptoPP::Integer(prng, CryptoPP::Integer::One(), protocol_q);
    auto ps = CryptoPP::Integer(prng, CryptoPP::Integer::One(), protocol_q);
    auto pu = CryptoPP::Integer(prng, CryptoPP::Integer::One(), protocol_q);

    auto Ps = a_exp_b_mod_c(protocol_g, ps, protocol_p);
    auto Pu = a_exp_b_mod_c(protocol_g, pu, protocol_p);
    auto rw_int = a_exp_b_mod_c(string_to_hashed_int(password, protocol_p), ks, protocol_p);

    auto rw_key = key_from_int(rw_int);
    auto pt = IntToString(pu);
    auto ad = IntToString(Pu) + "|" + IntToString(Ps);

    auto C = aes_256_encrypt(rw_key, pt, ad);
    CryptoPP::InvertibleRSAFunction params;
    params.GenerateRandomWithKeySize(prng, 1024);

    CryptoPP::RSA::PrivateKey privateKey(params);
    CryptoPP::RSA::PublicKey publicKey(params);

    std::string pbkey, pvkey;
    CryptoPP::HexEncoder encoder;
    encoder.Attach(new CryptoPP::StringSink(pbkey));
    publicKey.Save(encoder);
    encoder.Attach(new CryptoPP::StringSink(pvkey));
    privateKey.Save(encoder);

    if (C.length() > 32) {
        auto sanity_check = aes_256_decrypt(rw_key, C);
        if (pt + "|" + ad == sanity_check) {
            success = true;
            result.ks = IntToString(ks);
            result.ps = IntToString(ps);
            result.Ps = IntToString(Ps);
            result.Pu = IntToString(Pu);
            result.C = C;
            result.sk = aes_256_encrypt(rw_key, pvkey, "|");
            result.pk = pbkey;
            result.user_id = username;
        }
    }

    result.success = success;
    return result;
}


#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
using namespace emscripten;

#include <iostream>
#include <cstdlib>

#include "cryptopp/integer.h"
#include "cryptopp/osrng.h"
#include "cryptopp/cryptlib.h"

#include "utils.h"

EMSCRIPTEN_BINDINGS(register_user) {
    function("register_user", &register_user);
}

#endif