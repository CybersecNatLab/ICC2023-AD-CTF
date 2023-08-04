#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <cstdlib>
#include <string>

#include "cryptopp/integer.h"
#include "cryptopp/osrng.h"
#include "cryptopp/cryptlib.h"

#include "utils.h"

std::pair<ClientStep1, ClientStorage> client_step1(std::string username, std::string password);
ClientStep2 client_step2(ServerStep1 ss1, ClientStorage cs);

#endif