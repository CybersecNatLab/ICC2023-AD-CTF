#define _CRT_SECURE_NO_WARNINGS /* sprintf */

#include <Windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <stdexcept>
#include <wincrypt.h>
#include <winhttp.h>

#include "builtins.h"
#include "util.h"

#define PRIVKEY_PATH "C:\\inetpub\\keys\\private.pem"
#define CHECK_OWNER_API_SERVER L"127.0.0.1"
#define CHECK_OWNER_API_PORT 80
#define CHECK_OWNER_API_URL "/api/checkowner/"

#define MAX_RNG_CALLS 2
#define MAX_SIGN_CALLS 2

extern "C" __declspec(dllimport) void ProcessPrng(void *, std::size_t);

namespace lang {
namespace builtins {

static std::size_t g_rng_num_calls;

std::unique_ptr<objects::Object> NativeFunction::clone() {
    return std::make_unique<NativeFunction>(*this);
}

std::string NativeFunction::describe() {
    return std::format("[NativeFunction {:s}]", name_);
}

std::unique_ptr<objects::Object> NativeFunction::call(
    exec::Interpreter &interp, std::vector<std::unique_ptr<objects::Object>> &args
) {
    return func_(interp, args);
}

RNGFunction::RNGFunction(const std::string &name, GenT gen) : Function(name), gen_(gen) {
    set_ref_param(0);
}

std::unique_ptr<objects::Object> RNGFunction::clone() {
    return std::make_unique<RNGFunction>(*this);
}

std::string RNGFunction::describe() {
    return std::format("[RNGFunction {:s} @ &H{:x}]",
        name_, reinterpret_cast<std::uintptr_t>(gen_));
}

std::unique_ptr<objects::Object> RNGFunction::call(
    exec::Interpreter &, std::vector<std::unique_ptr<objects::Object>> &args
) {
    g_rng_num_calls++;
    if (g_rng_num_calls > MAX_RNG_CALLS)
        throw std::runtime_error(name_ + ": maximum number of RNG calls exceeded");

    if (args.size() > 2)
        throw std::runtime_error(name_ + ": wrong number of arguments");

    if (args.size() == 0) {
        int64_t result = 0;
        gen_(&result, sizeof(result));
        return std::make_unique<objects::Integer>(result);
    }

    auto dst = args[0].get();
    objects::String *entropy = nullptr;

    /* BUG: type of dst is not checked when called with two arguments.
     * This leads to type confusion and the ability to write random
     * bytes to arbitrary addresses by confusing IntegerArray as String. */
    if (args.size() == 1 && !dst->is_str()) {
        throw std::runtime_error(name_ + ": wrong type of argument");
    } else if (args.size() == 2) {
        auto entropy_obj = args[1].get();
        if (!entropy_obj->is_str())
            throw std::runtime_error(name_ + ": wrong type of arguments");
        entropy = entropy_obj->as_str();
        if (entropy->length_ == 0)
            throw std::runtime_error(name_ + ": empty entropy");
    }

    auto dst_str = dst->as_str();
    if (dst_str->length_ < 128)
        throw std::runtime_error(name_ + ": request too small");
    gen_(dst_str->str_, dst_str->length_);

    if (entropy) {
        for (std::size_t i = 0; i < dst_str->length_; i++)  {
            std::size_t j = (i + (unsigned char)entropy->str_[i % entropy->length_]) % dst_str->length_;
            unsigned char tmp = dst_str->str_[i];
            dst_str->str_[i] = dst_str->str_[j];
            dst_str->str_[j] = tmp;
        }
    }

    return std::make_unique<objects::Integer>(dst_str->length_);
}

SignTokenFunction::SignTokenFunction(const std::string &name, const std::string &key_path)
        : Function(name), num_calls_(0) {
    set_ref_param(0);

    BCRYPT_ALG_HANDLE alg_hash;
    if (BCryptOpenAlgorithmProvider(&alg_hash, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0)
        throw std::runtime_error("cannot open hash algorithm provider");

    if (BCryptCreateHash(alg_hash, &hash_, NULL, 0, NULL, 0, BCRYPT_HASH_REUSABLE_FLAG) < 0)
        throw std::runtime_error("cannot create hash engine");

    BCRYPT_ALG_HANDLE alg_sign;
    if (BCryptOpenAlgorithmProvider(&alg_sign, BCRYPT_ECDSA_P256_ALGORITHM, NULL, 0) < 0)
        throw std::runtime_error("cannot open signature algorithm provider");

    auto key_pem = util::read_file(key_path);

    DWORD key_raw_size = 0;
    if (!CryptStringToBinaryA(key_pem.c_str(), 0, CRYPT_STRING_BASE64HEADER, NULL,
                              &key_raw_size, NULL, NULL))
        throw std::runtime_error("cannot get private key decoded size");
    if (key_raw_size != 121)
        throw std::runtime_error("invalid private key size");

    auto key_raw = new unsigned char[key_raw_size];
    if (!CryptStringToBinaryA(key_pem.c_str(), 0, CRYPT_STRING_BASE64HEADER, key_raw,
                              &key_raw_size, NULL, NULL))
        throw std::runtime_error("cannot decode private key");

    unsigned char key_blob[sizeof(BCRYPT_ECCKEY_BLOB) + 32*3];

    BCRYPT_ECCKEY_BLOB *key_blob_hdr = (BCRYPT_ECCKEY_BLOB *)key_blob;
    key_blob_hdr->dwMagic = BCRYPT_ECDSA_PRIVATE_P256_MAGIC;
    key_blob_hdr->cbKey = 32;

    unsigned char *key_blob_data = key_blob + sizeof(BCRYPT_ECCKEY_BLOB);
    memcpy(key_blob_data, key_raw + 57, 32); /* X */
    memcpy(key_blob_data + 32, key_raw + 57 + 32, 32); /* Y */
    memcpy(key_blob_data + 64, key_raw + 7, 32); /* d */

    delete[] key_raw;

    if (BCryptImportKeyPair(alg_sign, NULL, BCRYPT_ECCPRIVATE_BLOB, &key_,
                            key_blob, sizeof(key_blob), 0) < 0)
        throw std::runtime_error("cannot import private key");
}

std::unique_ptr<objects::Object> SignTokenFunction::clone() {
    return std::make_unique<SignTokenFunction>(*this);
}

std::string SignTokenFunction::describe() {
    return std::format("[SignTokenFunction {:s}]", name_);
}

std::unique_ptr<objects::Object> SignTokenFunction::call(
    exec::Interpreter &interp, std::vector<std::unique_ptr<objects::Object>> &args
) {
    num_calls_++;
    if (num_calls_ > MAX_SIGN_CALLS)
        throw std::runtime_error(name_ + ": maximum number of calls exceeded");

    if (args.size() != 1)
        throw std::runtime_error(name_ + ": wrong number of arguments");
    auto arg = args[0].get();
    if (!arg->is_str())
        throw std::runtime_error(name_ + ": document ID must be a string");

    HINTERNET http_session = WinHttpOpen(L"WinHTTP/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!http_session)
        throw std::runtime_error("cannot create HTTP session");

    HINTERNET http_conn = WinHttpConnect(http_session, CHECK_OWNER_API_SERVER,
                                         CHECK_OWNER_API_PORT, 0);
    if (!http_conn) {
        WinHttpCloseHandle(http_session);
        throw std::runtime_error("cannot connect to HTTP API");
    }

    auto url = CHECK_OWNER_API_URL + arg->as_str()->to_std() + "/" + util::g_username;
    auto url_wchar = std::wstring(url.begin(), url.end());
    HINTERNET http_req = WinHttpOpenRequest(http_conn, L"GET", url_wchar.c_str(), NULL,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!http_req) {
        WinHttpCloseHandle(http_conn);
        WinHttpCloseHandle(http_session);
        throw std::runtime_error("cannot open HTTP request");
    }

    if (!WinHttpSendRequest(http_req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, NULL)) {
        WinHttpCloseHandle(http_req);
        WinHttpCloseHandle(http_conn);
        WinHttpCloseHandle(http_session);
        throw std::runtime_error("cannot send HTTP request");
    }

    if (!WinHttpReceiveResponse(http_req, NULL)) {
        WinHttpCloseHandle(http_req);
        WinHttpCloseHandle(http_conn);
        WinHttpCloseHandle(http_session);
        throw std::runtime_error("cannot receive HTTP response");
    }

    DWORD status_code = 0;
    DWORD status_code_len = sizeof(status_code);
    BOOL ret = WinHttpQueryHeaders(http_req,
                                   WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                   WINHTTP_HEADER_NAME_BY_INDEX, &status_code,
                                   &status_code_len, WINHTTP_NO_HEADER_INDEX);
    WinHttpCloseHandle(http_req);
    WinHttpCloseHandle(http_conn);
    WinHttpCloseHandle(http_session);
    if (!ret)
        throw std::runtime_error("cannot query HTTP status code");

    if (status_code == 403)
        throw std::runtime_error("document not owned by current user");
    if (status_code != 200)
        throw std::runtime_error("HTTP API error");

    /* BUG: reentrancy. Since the document ID is taken as a refence, overriding Len
     * with a script function allows changing the ID after the owner check.
     * This length check has no purpose apart from introducing this bug. */
    std::vector<std::unique_ptr<Object>> len_args;
    len_args.push_back(arg->clone());
    auto doc_id_len = interp.call("Len", len_args);
    if (!doc_id_len->is_int())
        throw std::runtime_error("invalid type returned from Len");
    if (doc_id_len->as_int()->value_ > 100)
        throw std::runtime_error("document ID is too long");

    auto doc_id = arg->as_str();
    if (BCryptHashData(hash_, (PUCHAR)doc_id->str_, (ULONG)doc_id->length_, 0) < 0)
        throw std::runtime_error("cannot hash document ID (1)");
    unsigned char digest[32];
    if (BCryptFinishHash(hash_, digest, sizeof(digest), 0) < 0)
        throw std::runtime_error("cannot hash document ID (2)");

    ULONG sig_size = 0;
    unsigned char sig[32*2];
    if (BCryptSignHash(key_, NULL, digest, sizeof(digest), sig,
                       sizeof(sig), &sig_size, 0) < 0)
        throw std::runtime_error("cannot sign document ID");

    char sig_hex[sizeof(sig)*2 + 1];
    sig_hex[0] = '\0';
    for (size_t i = 0; i < sig_size; i++)
        sprintf(sig_hex + i*2, "%02x", sig[i]);

    return std::make_unique<objects::String>(sig_hex);
}

#define NATIVE(name) \
    static std::unique_ptr<objects::Object> NATIVE_ ## name \
    ([[maybe_unused]] exec::Interpreter &interp, std::vector<std::unique_ptr<objects::Object>> &args)

NATIVE(Asc) {
    if (args.size() != 1)
        throw std::runtime_error("Asc: wrong number of arguments");
    auto arg = args[0].get();
    if (!arg->is_str())
        throw std::runtime_error("Asc: wrong type of argument");
    auto str = arg->as_str();
    if (str->length_ == 0)
        throw std::runtime_error("Asc: empty string");
    return std::make_unique<objects::Integer>(static_cast<unsigned char>(str->str_[0]));
}

NATIVE(Chr) {
    if (args.size() != 1)
        throw std::runtime_error("Chr: wrong number of arguments");
    auto arg = args[0].get();
    if (!arg->is_int())
        throw std::runtime_error("Chr: wrong type of argument");
    return std::make_unique<objects::String>(
        std::string(1, static_cast<char>(arg->as_int()->value_)));
}

NATIVE(CBool) {
    if (args.size() != 1)
        throw std::runtime_error("CBool: wrong number of arguments");
    auto arg = args[0].get();
    bool value = true;
    if (arg->is_bool())
        value = arg->as_bool()->value_;
    else if (arg->is_int())
        value = arg->as_int()->value_ != 0;
    else if (arg->is_str())
        value = arg->as_str()->length_ != 0;
    return std::make_unique<objects::Boolean>(value);
}

NATIVE(CInt) {
    if (args.size() != 1)
        throw std::runtime_error("CInt: wrong number of arguments");
    auto arg = args[0].get();
    std::int64_t value;
    if (arg->is_bool())
        value = arg->as_bool()->value_ ? 1 : 0;
    else if (arg->is_int())
        value = arg->as_int()->value_;
    else if (arg->is_str())
        value = util::str_to_int(arg->as_str()->to_std());
    else
        throw std::runtime_error("CInt: wrong type of argument");
    return std::make_unique<objects::Integer>(value);
}

NATIVE(CStr) {
    if (args.size() != 1)
        throw std::runtime_error("CStr: wrong number of arguments");
    auto arg = args[0].get();
    std::string value;
    if (arg->is_bool()) {
        value = arg->as_bool()->value_ ? "True" : "False";
    } else if (arg->is_int()) {
        value = std::to_string(arg->as_int()->value_);
    } else if (arg->is_int_array()) {
        value = "[";
        auto arr = arg->as_int_array();
        for (std::size_t i = 0; i < arr->length_; i++) {
            if (i != 0)
                value += ", ";
            value += std::to_string(arr->at(i));
        }
        value += "]";
    } else if (arg->is_str()) {
        value = arg->as_str()->to_std();
    } else {
        throw std::runtime_error("CStr: wrong type of argument");
    }
    return std::make_unique<objects::String>(value);
}

NATIVE(Describe) {
    if (args.size() != 1)
        throw std::runtime_error("Describe: wrong number of arguments");
    return std::make_unique<objects::String>(args[0]->describe());
}

NATIVE(Len) {
    if (args.size() != 1)
        throw std::runtime_error("Len: wrong number of arguments");
    auto arg = args[0].get();
    if (!arg->is_str())
        throw std::runtime_error("Len: wrong type of argument");
    return std::make_unique<objects::Integer>(arg->as_str()->length_);
}

NATIVE(Mid) {
    auto arg_count = args.size();
    if (arg_count < 2 || arg_count > 3)
        throw std::runtime_error("Mid: wrong number of arguments");

    auto arg0 = args[0].get();
    auto arg1 = args[1].get();
    if (!arg0->is_str() || !arg1->is_int())
        throw std::runtime_error("Mid: wrong type of arguments");

    auto str = arg0->as_str();

    auto arg1_int = arg1->as_int()->value_;
    if (arg1_int < 1)
        throw std::runtime_error("Mid: invalid start");
    std::size_t start = arg1_int - 1;
    if (start >= str->length_)
        return std::make_unique<objects::String>("");

    std::size_t length = str->length_ - start;
    if (arg_count == 3) {
        auto arg2 = args[2].get();
        if (!arg2->is_int())
            throw std::runtime_error("Mid: wrong type of arguments");
        auto arg2_int = arg2->as_int()->value_;
        if (arg2_int < 0)
            throw std::runtime_error("Mid: invalid length");
        if (static_cast<std::size_t>(arg2_int) < length)
            length = arg2_int;
    }

    return std::make_unique<objects::String>(
        std::string(str->str_ + start, length));
}

NATIVE(RandSeed) {
    if (args.size() != 1)
        throw std::runtime_error("RandSeed: wrong number of arguments");
    auto arg = args[0].get();
    if (!arg->is_int())
        throw std::runtime_error("RandSeed: wrong type of argument");
    util::fast_rng_seed(static_cast<std::uint8_t>(arg->as_int()->value_));
    return std::move(args[0]);
}

NATIVE(String) {
    if (args.size() != 2)
        throw std::runtime_error("String: wrong number of arguments");

    auto arg0 = args[0].get();
    if (!arg0->is_int())
        throw std::runtime_error("String: wrong type of arguments");

    auto arg0_int = arg0->as_int()->value_;
    if (arg0_int < 0)
        throw std::runtime_error("String: invalid length");
    std::size_t length = arg0_int;

    char ch;
    auto arg1 = args[1].get();
    if (arg1->is_int()) {
        ch = static_cast<char>(arg1->as_int()->value_);
    } else if (arg1->is_str()) {
        auto arg1_str = arg1->as_str();
        if (arg1_str->length_ == 0)
            throw std::runtime_error("String: empty string");
        ch = arg1_str->str_[0];
    } else {
        throw std::runtime_error("String: wrong type of arguments");
    }

    return std::make_unique<objects::String>(std::string(length, ch));
}

#undef NATIVE

static void RNG_fast(void *buffer, std::size_t size) {
    auto p = static_cast<std::uint8_t *>(buffer);
    for (std::size_t i = 0; i < size; i++)
        p[i] = util::fast_rng_rand();
}

void register_native(exec::Context &ctx, const std::string &name, NativeFunction::FuncT func) {
    ctx.var(name, true)->set(std::make_unique<NativeFunction>(name, func));
}

void register_rng(exec::Context &ctx, const std::string &name, RNGFunction::GenT gen) {
    ctx.var(name, true)->set(std::make_unique<RNGFunction>(name, gen));
}

void register_builtins(exec::Context &ctx) {
#define NATIVE(name) do { register_native(ctx, #name, NATIVE_ ## name); } while (0)
    NATIVE(Asc);
    NATIVE(Chr);
    NATIVE(CBool);
    NATIVE(CInt);
    NATIVE(CStr);
    NATIVE(Describe);
    NATIVE(Len);
    NATIVE(Mid);
    NATIVE(RandSeed);
    NATIVE(String);
#undef NATIVE
#define RNG(name, gen) do { register_rng(ctx, #name, (gen)); } while (0)
    RNG(CryptoRand, ProcessPrng);
    RNG(FastRand, RNG_fast);
#undef RNG
    ctx.var("SignToken", true)->set(
        std::make_unique<SignTokenFunction>("SignToken", PRIVKEY_PATH));
}

} // namespace builtins
} // namespace lang
