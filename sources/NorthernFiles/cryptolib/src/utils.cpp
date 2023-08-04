#include "utils.h"

CryptoPP::Integer string_to_hashed_int(std::string password, CryptoPP::Integer p)
{
    CryptoPP::SHA256 hash;
    std::string digest, hexdigest;
    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(hexdigest));
    CryptoPP::StringSource(password, true, new CryptoPP::HashFilter(hash, new CryptoPP::StringSink(digest)));
    CryptoPP::StringSource(digest, true, new CryptoPP::Redirector(encoder));
    return a_exp_b_mod_c(CryptoPP::Integer((hexdigest + "h").c_str()), 2, p);
}

std::string key_from_int(CryptoPP::Integer x)
{
    CryptoPP::SHA256 hash;
    std::string digest;
    CryptoPP::StringSource(CryptoPP::IntToString(x), true, new CryptoPP::HashFilter(hash, new CryptoPP::StringSink(digest)));
    return digest;
}

std::vector<std::string> unpack_values(std::string inp)
{
    std::reverse(inp.begin(), inp.end());
    std::vector<std::string> elements;
    size_t startpos = -1;
    size_t oldstart;
    for (int i = 0; i < 2; i++)
    {
        oldstart = startpos;
        startpos = inp.find("|", oldstart + 1);
        if (startpos != std::string::npos)
        {
            auto to_push = inp.substr(oldstart + 1, startpos - oldstart - 1);
            std::reverse(to_push.begin(), to_push.end());
            elements.push_back(to_push);
        }
    }
    if (startpos != std::string::npos)
    {
        auto to_push = inp.substr(startpos + 1);
        std::reverse(to_push.begin(), to_push.end());
        elements.push_back(to_push);
    }
    std::reverse(elements.begin(), elements.end());
    return elements;
}

std::string string_to_hash(std::string password)
{
    CryptoPP::SHA256 hash;
    std::string digest, hexdigest;
    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(hexdigest));
    CryptoPP::StringSource(password, true, new CryptoPP::HashFilter(hash, new CryptoPP::StringSink(digest)));
    CryptoPP::StringSource(digest, true, new CryptoPP::Redirector(encoder));
    return hexdigest;
}

std::string kdf(std::string key, std::string id, std::string content)
{
    CryptoPP::byte derived[CryptoPP::SHA256::DIGESTSIZE];
    CryptoPP::HKDF<CryptoPP::SHA256> hkdf;
    hkdf.DeriveKey(derived, sizeof(derived), reinterpret_cast<const CryptoPP::byte *>(key.c_str()), (size_t)key.length(),
                   reinterpret_cast<const CryptoPP::byte *>(id.c_str()), (size_t)id.length(),
                   reinterpret_cast<const CryptoPP::byte *>(content.c_str()), content.length());
    std::string result;
    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(result));
    encoder.Put(derived, sizeof(derived));
    encoder.MessageEnd();
    return result;
}

std::string generate_random_key()
{
    CryptoPP::AutoSeededRandomPool prng;
    CryptoPP::SecByteBlock random_key(CryptoPP::AES::MAX_KEYLENGTH);
    prng.GenerateBlock(random_key, random_key.size());
    std::string key_str(reinterpret_cast<const char *>(&random_key[0]), random_key.size());
    return key_str;
}

std::string privkey_from_capsule(Capsule user, std::string password)
{
    CryptoPP::AutoSeededRandomPool rng;
    std::string result;

    auto ks = CryptoPP::Integer(user.ks.c_str());
    auto rw_int = a_exp_b_mod_c(string_to_hashed_int(password, protocol_p), ks, protocol_p);
    auto rw_key = key_from_int(rw_int);

    auto dec_key = aes_256_decrypt(rw_key, user.sk);

    dec_key = dec_key.substr(0, dec_key.size() - 2);

    CryptoPP::HexDecoder decoder;
    decoder.Put((CryptoPP::byte *)dec_key.c_str(), dec_key.size());
    decoder.MessageEnd();

    CryptoPP::RSA::PrivateKey rsaPrivate;
    rsaPrivate.Load(decoder);

    CryptoPP::HexEncoder encoder;
    encoder.Attach(new CryptoPP::StringSink(result));
    rsaPrivate.Save(encoder);

    return result;
}

std::string aes_256_encrypt(std::string key, std::string msg, std::string ad = "")
{
    std::string ctx, hexctx;
    CryptoPP::AutoSeededRandomPool prng;

    CryptoPP::SecByteBlock aes_key(reinterpret_cast<const unsigned char *>(key.c_str()), CryptoPP::AES::MAX_KEYLENGTH);
    CryptoPP::SecByteBlock aes_iv(CryptoPP::AES::BLOCKSIZE);

    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(hexctx));
    prng.GenerateBlock(aes_iv, aes_iv.size());

    try
    {
        CryptoPP::GCM<CryptoPP::AES>::Encryption enc;
        enc.SetKeyWithIV(aes_key, aes_key.size(), aes_iv, aes_iv.size());
        CryptoPP::AuthenticatedEncryptionFilter ef(enc, new CryptoPP::StringSink(ctx), false, TAG_SIZE);
        ef.ChannelPut(CryptoPP::AAD_CHANNEL, reinterpret_cast<const unsigned char *>(ad.data()), ad.size());
        ef.ChannelMessageEnd(CryptoPP::AAD_CHANNEL);
        ef.ChannelPut(CryptoPP::DEFAULT_CHANNEL, reinterpret_cast<const unsigned char *>(msg.data()), msg.size());
        ef.ChannelMessageEnd(CryptoPP::DEFAULT_CHANNEL);
    }
    catch (CryptoPP::Exception &e)
    {
        std::cerr << "Caught Exception..." << std::endl;
        std::cerr << e.what() << std::endl;
        std::cerr << std::endl;
    }

    std::string aes_iv_str(reinterpret_cast<const char *>(&aes_iv[0]), aes_iv.size());
    CryptoPP::StringSource(aes_iv_str + ctx + "|" + ad, true, new CryptoPP::Redirector(encoder));

    return hexctx;
}

std::string aes_256_decrypt(std::string key, std::string full_ctx)
{
    std::string raw_full_ctx, ad;
    CryptoPP::StringSource(full_ctx, true, new CryptoPP::HexDecoder(new CryptoPP::StringSink(raw_full_ctx)));
    auto unpacked = unpack_values(raw_full_ctx);

    if (unpacked.size() != 3)
        return "";

    std::string raw_ctx = unpacked[0];
    ad = unpacked[1] + "|" + unpacked[2];
    std::string plain;

    CryptoPP::SecByteBlock aes_key(reinterpret_cast<const unsigned char *>(key.c_str()), CryptoPP::AES::MAX_KEYLENGTH);
    CryptoPP::SecByteBlock aes_iv(reinterpret_cast<const unsigned char *>(raw_ctx.substr(0, CryptoPP::AES::BLOCKSIZE).c_str()), CryptoPP::AES::BLOCKSIZE);

    try
    {
        CryptoPP::GCM<CryptoPP::AES>::Decryption dec;
        dec.SetKeyWithIV(aes_key, aes_key.size(), aes_iv, aes_iv.size());
        std::string enc = raw_ctx.substr(CryptoPP::AES::BLOCKSIZE, raw_ctx.length() - TAG_SIZE - CryptoPP::AES::BLOCKSIZE);
        std::string mac = raw_ctx.substr(raw_ctx.length() - TAG_SIZE);
        CryptoPP::AuthenticatedDecryptionFilter df(dec, NULL, 1 | 16, TAG_SIZE);
        df.ChannelPut(CryptoPP::DEFAULT_CHANNEL, reinterpret_cast<const unsigned char *>(mac.data()), mac.size());
        df.ChannelPut(CryptoPP::AAD_CHANNEL, reinterpret_cast<const unsigned char *>(ad.data()), ad.size());
        df.ChannelPut(CryptoPP::DEFAULT_CHANNEL, reinterpret_cast<const unsigned char *>(enc.data()), enc.size());
        df.ChannelMessageEnd(CryptoPP::AAD_CHANNEL);
        df.ChannelMessageEnd(CryptoPP::DEFAULT_CHANNEL);

        bool b = false;
        b = df.GetLastResult();

        if (!b)
            return "";

        std::string retrieved;
        size_t n = (size_t)(-1);
        df.SetRetrievalChannel(CryptoPP::DEFAULT_CHANNEL);
        n = (size_t)df.MaxRetrievable();
        retrieved.resize(n);

        if (n > 0)
        {
            df.Get((unsigned char *)retrieved.data(), n);
        }
        plain = retrieved;
    }
    catch (CryptoPP::Exception &e)
    {
        std::cerr << "Caught Exception..." << std::endl;
        std::cerr << e.what() << std::endl;
        std::cerr << std::endl;
    }

    return plain + "|" + ad;
}

#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
using namespace emscripten;

EMSCRIPTEN_BINDINGS(capsule)
{
    function("privkey_from_capsule", &privkey_from_capsule);

    value_object<Capsule>("Capsule")
        .field("success", &Capsule::success)
        .field("user_id", &Capsule::user_id)
        .field("ks", &Capsule::ks)
        .field("ps", &Capsule::ps)
        .field("Ps", &Capsule::Ps)
        .field("Pu", &Capsule::Pu)
        .field("C", &Capsule::C)
        .field("sk", &Capsule::sk)
        .field("pk", &Capsule::pk);

    value_object<ClientStep1>("ClientStep1")
        .field("user_id", &ClientStep1::user_id)
        .field("X_u", &ClientStep1::X_u)
        .field("alpha", &ClientStep1::alpha);

    value_object<ClientStep2>("ClientStep2")
        .field("A_u", &ClientStep2::A_u);

    value_object<ServerStep1>("ServerStep1")
        .field("beta", &ServerStep1::beta)
        .field("C", &ServerStep1::C)
        .field("X_s", &ServerStep1::X_s);

    value_object<ClientStorage>("ClientStorage")
        .field("r", &ClientStorage::r)
        .field("user_id", &ClientStorage::user_id)
        .field("X_u", &ClientStorage::X_u)
        .field("alpha", &ClientStorage::alpha)
        .field("x_u", &ClientStorage::x_u);

    value_object<ServerStorage>("ServerStorage")
        .field("K_sess", &ServerStorage::K_sess)
        .field("id1", &ServerStorage::id1);
}

#endif