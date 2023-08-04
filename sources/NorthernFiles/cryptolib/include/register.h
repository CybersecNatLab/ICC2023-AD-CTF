#ifndef REGISTER_H
#define REGISTER_H

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <string>

#include "cryptopp/integer.h"
#include "cryptopp/osrng.h"
#include "cryptopp/cryptlib.h"
#include "cryptopp/rsa.h"

#include "utils.h"

Capsule register_user(std::string username, std::string password);

#endif