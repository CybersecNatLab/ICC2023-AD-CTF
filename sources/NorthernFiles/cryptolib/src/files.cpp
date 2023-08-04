#include "files.h"

std::pair<std::string, std::string> encrypt_file(std::string raw_file, Capsule user)
{
    CryptoPP::AutoSeededRandomPool rng;
    auto eph_key = generate_random_key();
    std::string enc_eph_key, encrypted_file, enc_eph_key_hex;

    CryptoPP::RSAES_OAEP_SHA_Encryptor temp;
    CryptoPP::HexDecoder decoder;
    decoder.Put((CryptoPP::byte *)user.pk.c_str(), user.pk.size());
    decoder.MessageEnd();
    temp.AccessKey().Load(decoder);

    CryptoPP::StringSource ss(eph_key, true, new CryptoPP::PK_EncryptorFilter(rng, temp, new CryptoPP::StringSink(enc_eph_key)));

    encrypted_file = aes_256_encrypt(eph_key, raw_file, "|");

    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(enc_eph_key_hex));
    CryptoPP::StringSource(enc_eph_key, true, new CryptoPP::Redirector(encoder));
    return std::make_pair(encrypted_file, enc_eph_key_hex);
}

std::string share_file(Capsule owner, Capsule receiver, std::string owner_password, std::string file_key_owner)
{
    CryptoPP::AutoSeededRandomPool rng;

    auto ks = CryptoPP::Integer(owner.ks.c_str());
    auto rw_int = a_exp_b_mod_c(string_to_hashed_int(owner_password, protocol_p), ks, protocol_p);
    auto rw_key = key_from_int(rw_int);

    auto dec_key = aes_256_decrypt(rw_key, owner.sk);

    dec_key = dec_key.substr(0, dec_key.size() - 2);

    CryptoPP::RSAES_OAEP_SHA_Decryptor temp;
    CryptoPP::HexDecoder decoder;
    decoder.Put((CryptoPP::byte *)dec_key.c_str(), dec_key.size());
    decoder.MessageEnd();
    temp.AccessKey().Load(decoder);

    std::string decrypted_key, enc_key_unhex;
    CryptoPP::StringSource(file_key_owner, true, new CryptoPP::HexDecoder(new CryptoPP::StringSink(enc_key_unhex)));
    CryptoPP::StringSource ss(enc_key_unhex, true, new CryptoPP::PK_DecryptorFilter(rng, temp, new CryptoPP::StringSink(decrypted_key)));

    std::string enc_user2_key, enc_user2_key_hex;

    CryptoPP::RSAES_OAEP_SHA_Encryptor temp2;
    decoder.Detach();
    decoder.Put((CryptoPP::byte *)receiver.pk.c_str(), receiver.pk.size());
    decoder.MessageEnd();
    temp2.AccessKey().Load(decoder);

    CryptoPP::StringSource ss2(decrypted_key, true, new CryptoPP::PK_EncryptorFilter(rng, temp2, new CryptoPP::StringSink(enc_user2_key)));

    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(enc_user2_key_hex));
    CryptoPP::StringSource(enc_user2_key, true, new CryptoPP::Redirector(encoder));

    return enc_user2_key_hex;
}

std::string retrieve_file(std::string enc_file, std::string enc_key, Capsule user, std::string password)
{
    CryptoPP::AutoSeededRandomPool rng;

    auto ks = CryptoPP::Integer(user.ks.c_str());
    auto rw_int = a_exp_b_mod_c(string_to_hashed_int(password, protocol_p), ks, protocol_p);
    auto rw_key = key_from_int(rw_int);

    auto dec_key = aes_256_decrypt(rw_key, user.sk);

    dec_key = dec_key.substr(0, dec_key.size() - 2);

    CryptoPP::RSAES_OAEP_SHA_Decryptor temp;
    CryptoPP::HexDecoder decoder;
    decoder.Put((CryptoPP::byte *)dec_key.c_str(), dec_key.size());
    decoder.MessageEnd();
    temp.AccessKey().Load(decoder);

    std::string decrypted_key, enc_key_unhex;
    CryptoPP::StringSource(enc_key, true, new CryptoPP::HexDecoder(new CryptoPP::StringSink(enc_key_unhex)));
    CryptoPP::StringSource ss(enc_key_unhex, true, new CryptoPP::PK_DecryptorFilter(rng, temp, new CryptoPP::StringSink(decrypted_key)));

    auto file = aes_256_decrypt(decrypted_key, enc_file);

    return file.substr(0, file.size() - 2);
}

#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
using namespace emscripten;

EMSCRIPTEN_BINDINGS(files)
{
    function("encrypt_file", &encrypt_file);
    function("share_file", &share_file);
    function("retrieve_file", &retrieve_file);

    value_array<std::pair<std::string, std::string>>("pair<string, string>")
        .element(&std::pair<std::string, std::string>::first)
        .element(&std::pair<std::string, std::string>::second);
}

#endif