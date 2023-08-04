#ifndef FILES_H
#define FILES_H

#include <iostream>
#include <cstdlib>

#include "cryptopp/rsa.h"

#include "utils.h"

std::pair<std::string, std::string> encrypt_file(std::string raw_file, Capsule user);
std::string retrieve_file(std::string enc_file, std::string enc_key, Capsule user, std::string password);
std::string share_file(Capsule owner, Capsule receiver, std::string owner_password, std::string file_key_owner);

#endif