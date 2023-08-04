#define _WINSOCKAPI_
#include <windows.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <httpserv.h>
#include <sal.h>
#include <string>
#include <string_view>
#include <winsock.h>

#define API_URL L"/api/macro"
#define MAX_REQUEST_SIZE (128 * 1024)
#define MACRO_EXE_PATH "C:\\Windows\\System32\\inetsrv\\macro.exe"

/* Uncomment to disable IP check for testing. */
// #define DISABLE_IP_CHECK

static unsigned char hexdecode_nibble(char ch, bool *success)
{
    *success = true;
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'f')
        return 10 + (ch - 'a');
    *success = false;
    return 0;
}

static bool hexdecode(void *buf, size_t len)
{
    if (len % 2)
        return false;

    char *src = static_cast<char *>(buf);
    unsigned char *dst = static_cast<unsigned char *>(buf);
    for (size_t i = 0; i < len; i += 2) {
        bool success;
        unsigned char hi = hexdecode_nibble(src[i], &success);
        if (!success)
            return false;
        unsigned char lo = hexdecode_nibble(src[i + 1], &success);
        if (!success)
            return false;
        dst[i / 2] = (hi << 4) | lo;
    }

    return true;
}

static std::string hexencode(const std::string_view &input)
{
    std::string output;
    for (auto ch : input) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02hhx",  (unsigned char)ch);
        output.append(buf);
    }
    return output;
}

static std::ofstream create_temp_file(char *filename)
{
    char temp_path[MAX_PATH+1];
    DWORD ret = GetTempPathA(MAX_PATH, temp_path);
    if (ret == 0 || ret > MAX_PATH)
        return {};

    if (GetTempFileNameA(temp_path, "mcr", 0, filename) == 0)
        return {};

    return std::ofstream(filename, std::ios::out | std::ios::binary);
}

static std::string run_macro(const std::string_view &username, const std::string_view &script,
                             const std::string_view &input, bool *success)
{
    char script_filename[MAX_PATH+1], input_filename[MAX_PATH+1];
    std::ofstream script_fs, input_fs;
    std::string output, cmd;
    FILE *pipe = NULL;

    *success = false;

    script_fs = create_temp_file(script_filename);
    if (!script_fs.is_open())
        goto cleanup;
    script_fs << script;
    script_fs.flush();

    input_fs = create_temp_file(input_filename);
    if (!input_fs.is_open())
        goto cleanup;
    input_fs << input;
    input_fs.flush();

    cmd = MACRO_EXE_PATH;
    cmd.append(" ");
    cmd.append(hexencode(username));
    cmd.append(" \"");
    cmd.append(script_filename);
    cmd.append("\" \"");
    cmd.append(input_filename);
    cmd.append("\" 2>&1");
    pipe = _popen(cmd.c_str(), "rb");
    if (!pipe)
        goto cleanup;

    while (true) {
        char buf[1024];
        size_t count = fread(buf, 1, sizeof(buf), pipe);
        output.append(buf, count);
        if (feof(pipe))
            break;
        if (ferror(pipe))
            goto cleanup;
    }

    *success = true;

cleanup:
    if (pipe)
        _pclose(pipe);
    if (script_fs.is_open()) {
        script_fs.close();
        remove(script_filename);
    }
    if (input_fs.is_open()) {
        input_fs.close();
        remove(input_filename);
    }
    return output;
}

class CModule : public CHttpModule
{
public:
    REQUEST_NOTIFICATION_STATUS OnBeginRequest(IN IHttpContext *pHttpContext,
                                               IN IHttpEventProvider *pProvider)
    {
        HRESULT hr;

        DWORD script_name_len = 0;
        PCWSTR script_name = pHttpContext->GetScriptName(&script_name_len);
        if (!script_name || !script_name_len || wcscmp(script_name, API_URL))
            return RQ_NOTIFICATION_CONTINUE;

        IHttpRequest *pHttpRequest = pHttpContext->GetRequest();
        if (!pHttpRequest)
            return RQ_NOTIFICATION_CONTINUE;

        PCSTR http_method = pHttpRequest->GetHttpMethod();
        if (!http_method || strcmp(http_method, "POST"))
            return RQ_NOTIFICATION_CONTINUE;

#ifndef DISABLE_IP_CHECK
        PSOCKADDR remote_addr = pHttpRequest->GetRemoteAddress();
        if (!remote_addr || remote_addr->sa_family != AF_INET)
            return RQ_NOTIFICATION_CONTINUE;
#endif

        IHttpResponse *pHttpResponse = pHttpContext->GetResponse();
        if (!pHttpResponse)
            return RQ_NOTIFICATION_CONTINUE;
        pHttpResponse->Clear();

#ifndef DISABLE_IP_CHECK
        if (((PSOCKADDR_IN)remote_addr)->sin_addr.S_un.S_addr != htonl(0x7f000001)) {
            pHttpResponse->SetStatus(403, "Forbidden");
            return RQ_NOTIFICATION_FINISH_REQUEST;
        }
#endif

        DWORD req_size = pHttpRequest->GetRemainingEntityBytes();
        if (req_size > MAX_REQUEST_SIZE) {
            pHttpResponse->SetStatus(413, "Content Too Large");
            return RQ_NOTIFICATION_FINISH_REQUEST;
        }

        char *req_buf = static_cast<char *>(pHttpContext->AllocateRequestMemory(req_size));
        if (!req_buf) {
            pProvider->SetErrorStatus(HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY));
            return RQ_NOTIFICATION_FINISH_REQUEST;
        }

        DWORD req_recv = 0;
        while (req_recv != req_size) {
            DWORD count;
            hr = pHttpRequest->ReadEntityBody(req_buf + req_recv, req_size - req_recv,
                                              false, &count);
            if (FAILED(hr)) {
                pProvider->SetErrorStatus(hr);
                return RQ_NOTIFICATION_FINISH_REQUEST;
            }
            req_recv += count;
        }

        char *sep = static_cast<char *>(memchr(req_buf, ';', req_size));
        if (!sep)  {
            pHttpResponse->SetStatus(400, "Bad Request");
            return RQ_NOTIFICATION_FINISH_REQUEST;
        }

        char *username = req_buf;
        size_t username_hex_len = sep - req_buf;
        char *script = username + username_hex_len + 1;

        sep = static_cast<char *>(memchr(script, ';', req_size - (script - req_buf)));
        if (!sep)  {
            pHttpResponse->SetStatus(400, "Bad Request");
            return RQ_NOTIFICATION_FINISH_REQUEST;
        }

        size_t script_hex_len = sep - script;
        char *input = script + script_hex_len + 1;
        size_t input_hex_len = req_size - (input - req_buf);

        if (
            !hexdecode(username, username_hex_len) ||
            !hexdecode(script, script_hex_len) ||
            !hexdecode(input, input_hex_len)
        ) {
            pHttpResponse->SetStatus(400, "Bad Request");
            return RQ_NOTIFICATION_FINISH_REQUEST;
        }

        bool success;
        std::string output = run_macro(
            std::string_view(username, username_hex_len / 2),
            std::string_view(script, script_hex_len / 2),
            std::string_view(input, input_hex_len / 2), &success);
        if (!success) {
            pHttpResponse->SetStatus(500, "Internal Server Error");
            return RQ_NOTIFICATION_FINISH_REQUEST;
        }

        pHttpResponse->SetHeader(HttpHeaderContentType, "text/plain", 10, TRUE);

        HTTP_DATA_CHUNK data_chunk;
        data_chunk.DataChunkType = HttpDataChunkFromMemory;
        data_chunk.FromMemory.pBuffer = output.data();
        data_chunk.FromMemory.BufferLength = (ULONG)output.size();

        DWORD resp_sent;
        hr = pHttpResponse->WriteEntityChunks(&data_chunk, 1, FALSE, TRUE, &resp_sent);
        if (FAILED(hr)) {
            pProvider->SetErrorStatus(hr);
            return RQ_NOTIFICATION_FINISH_REQUEST;
        }

        return RQ_NOTIFICATION_FINISH_REQUEST;
    }
};

class CModuleFactory : public IHttpModuleFactory
{
public:
    HRESULT GetHttpModule(OUT CHttpModule **ppModule, 
                          IN IModuleAllocator *pAllocator)
    {
        UNREFERENCED_PARAMETER(pAllocator);
        CModule *pModule = new CModule;
        if (!pModule)
            return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
        *ppModule = pModule;
        return S_OK;
    }

    void Terminate()
    {
        delete this;
    }
};

HRESULT __stdcall RegisterModule(DWORD dwServerVersion,
                                 IHttpModuleRegistrationInfo *pModuleInfo,
                                 IHttpServer *pGlobalInfo)
{
    UNREFERENCED_PARAMETER(dwServerVersion);
    UNREFERENCED_PARAMETER(pGlobalInfo);
    return pModuleInfo->SetRequestNotifications(
        new CModuleFactory, RQ_BEGIN_REQUEST, 0);
}
