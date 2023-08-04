#include "client.h"

std::pair<ClientStep1, ClientStorage> client_step1(std::string username, std::string password) {
    ClientStep1 res;
    ClientStorage cs_res;

    CryptoPP::AutoSeededRandomPool prng;
    auto r = CryptoPP::Integer(prng, CryptoPP::Integer::One(), protocol_q);
    auto x_u = CryptoPP::Integer(prng, CryptoPP::Integer::One(), protocol_q);
    auto X_u = a_exp_b_mod_c(protocol_g, x_u, protocol_p);
    auto alpha = a_exp_b_mod_c(string_to_hashed_int(password, protocol_p), r, protocol_p);

    cs_res.r = IntToString(r);
    cs_res.alpha = IntToString(alpha);
    cs_res.x_u = IntToString(x_u);
    cs_res.user_id = username;
    cs_res.X_u = IntToString(X_u);

    res.alpha = IntToString(alpha);
    res.user_id = username;
    res.X_u = IntToString(X_u);

    return std::make_pair(res, cs_res);
}

ClientStep2 client_step2(ServerStep1 ss1, ClientStorage cs) {
    ClientStep2 res;

    auto invr = CryptoPP::Integer(cs.r.c_str()).InverseMod(protocol_q);
    auto rw_int = a_exp_b_mod_c(CryptoPP::Integer(ss1.beta.c_str()), invr, protocol_p);

    auto rw = key_from_int(rw_int);
    auto msg = aes_256_decrypt(rw, ss1.C);
    auto msg_exploded = unpack_values(msg);

    auto id1 = string_to_hash(cs.user_id + cs.alpha);

    auto d = string_to_hashed_int(id1, protocol_q);
    auto e = string_to_hashed_int(ss1.X_s + cs.user_id + id1, protocol_q);

    auto tmp1 = (CryptoPP::Integer(ss1.X_s.c_str()) * a_exp_b_mod_c(CryptoPP::Integer(msg_exploded[2].c_str()), e, protocol_p)) % protocol_p;
    auto K_sess = string_to_hash(IntToString(a_exp_b_mod_c(tmp1, CryptoPP::Integer(cs.x_u.c_str()) + d * CryptoPP::Integer(msg_exploded[0].c_str()), protocol_p)));
    auto A_u = kdf(K_sess, "0", id1);
    res.A_u = A_u;

    return res;
}


#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
using namespace emscripten;

EMSCRIPTEN_BINDINGS(capsule) {
    function("client_step1", &client_step1);
    function("client_step2", &client_step2);

    value_array<std::pair<ClientStep1, ClientStorage>>("pair<ClientStep1, ClientStorage>")
        .element(&std::pair<ClientStep1, ClientStorage>::first)
        .element(&std::pair<ClientStep1, ClientStorage>::second);
}

#endif