#include "server.h"

std::pair<ServerStep1, ServerStorage> server_step1(ClientStep1 cs1, Capsule user_capsule)
{
    ServerStep1 res;
    ServerStorage ss_res;
    CryptoPP::AutoSeededRandomPool prng;

    auto alpha = CryptoPP::Integer(cs1.alpha.c_str());

    if (a_exp_b_mod_c(alpha, protocol_q, protocol_p) != CryptoPP::Integer("1"))
    {
        throw std::invalid_argument("Error 1");
    }

    if (CryptoPP::Integer(cs1.X_u.c_str()) % protocol_p == CryptoPP::Integer("0"))
    {
        throw std::invalid_argument("Error 2");
    }

    auto x_s = CryptoPP::Integer(prng, CryptoPP::Integer::One(), protocol_q);
    auto X_s = a_exp_b_mod_c(protocol_g, x_s, protocol_p);
    auto beta = a_exp_b_mod_c(alpha, CryptoPP::Integer(user_capsule.ks.c_str()), protocol_p);
    auto id1 = string_to_hash(cs1.user_id + cs1.alpha);
    auto d = string_to_hashed_int(id1, protocol_q);
    auto e = string_to_hashed_int(IntToString(X_s) + cs1.user_id + id1, protocol_q);

    auto tmp1 = (CryptoPP::Integer(cs1.X_u.c_str()) * a_exp_b_mod_c(CryptoPP::Integer(user_capsule.Pu.c_str()), d, protocol_p)) % protocol_p;
    auto K_sess = string_to_hash(IntToString(a_exp_b_mod_c(tmp1, x_s + e * CryptoPP::Integer(user_capsule.ps.c_str()), protocol_p)));

    res.beta = IntToString(beta);
    res.X_s = IntToString(X_s);
    res.C = user_capsule.C;

    ss_res.K_sess = K_sess;
    ss_res.id1 = id1;

    return std::make_pair(res, ss_res);
}

bool server_step2(ClientStep2 cs2, ServerStorage ss)
{
    auto K_sess = ss.K_sess;
    auto A_u = kdf(K_sess, "0", ss.id1);

    return A_u == cs2.A_u;
}

#ifdef PYBIND

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(cryptolib, m)
{
    m.def("server_step1", &server_step1);
    m.def("server_step2", &server_step2);

    py::class_<ServerStep1>(m, "ServerStep1")
        .def(py::init<>())
        .def_readwrite("beta", &ServerStep1::beta)
        .def_readwrite("C", &ServerStep1::C)
        .def_readwrite("X_s", &ServerStep1::X_s);

    py::class_<ServerStorage>(m, "ServerStorage")
        .def(py::init<>())
        .def_readwrite("K_sess", &ServerStorage::K_sess)
        .def_readwrite("id1", &ServerStorage::id1);

    py::class_<ClientStep1>(m, "ClientStep1")
        .def(py::init<>())
        .def_readwrite("user_id", &ClientStep1::user_id)
        .def_readwrite("X_u", &ClientStep1::X_u)
        .def_readwrite("alpha", &ClientStep1::alpha);

    py::class_<Capsule>(m, "Capsule")
        .def(py::init<>())
        .def_readwrite("success", &Capsule::success)
        .def_readwrite("user_id", &Capsule::user_id)
        .def_readwrite("ks", &Capsule::ks)
        .def_readwrite("ps", &Capsule::ps)
        .def_readwrite("Ps", &Capsule::Ps)
        .def_readwrite("Pu", &Capsule::Pu)
        .def_readwrite("C", &Capsule::C)
        .def_readwrite("sk", &Capsule::sk)
        .def_readwrite("pk", &Capsule::pk);

    py::class_<ClientStep2>(m, "ClientStep2")
        .def(py::init<>())
        .def_readwrite("A_u", &ClientStep2::A_u);
}

#endif