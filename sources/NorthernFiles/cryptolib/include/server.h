#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <cstdlib>

#include "cryptopp/integer.h"
#include "cryptopp/osrng.h"
#include "cryptopp/cryptlib.h"

#include "utils.h"

Capsule retrieve_capsule(std::string x);
std::pair<ServerStep1, ServerStorage> server_step1(ClientStep1 cs1, Capsule user_capsule);
bool server_step2(ClientStep2 cs2, ServerStorage ss);

#endif