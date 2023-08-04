# NorthernFiles

| Service       | NorthernFiles                                      |
| :------------ | :------------------------------------------------- |
| Author(s)     | Lorenzo Leonardini <@pianka>, Matteo Rossi <@mr96> |
| Store(s)      | File content & file metadata                       |
| Category(ies) | crypto, web                                        |
| Port(s)       | 80                                                 |
| FlagId(s)     | {user, file}                                       |
| Checker(s)    | [store1](/checkers/NorthernFiles/dist/checker.js)  |

## Description

NorthernFiles is an E2E encrypted file storage service. You can upload and share encrypted files via a web platform and a CLI.

## Vulnerabilities

### Store 1: File content & metadata

#### Vuln 1: SSRF

The CLI supports a legacy authentication method, which uses the private key to sign each request instead of using a cryptographic proof with the `auth` microservices. The request signature is checked by the `auth_proxy`. In case of success an access token is retrieved from the `auth` server, from the `/api/auth/internal/token/<uuid:id>` endpoint. Nginx is configured so that this endpoint is not accessible from the outside, but an SSRF vulnerability would allow us to impersonate any user and read their files' metadata.

Nginx is configured in a weird way, so that it follows redirects internally. We can exploit this together with an open redirect in the `/logout` endpoint.

The exploit loads the page `/logout?url=http://auth/internal/token/<uuid>`, where `<uuid>` is the id of the target user. The response will contain an authentication token for the target user, which can then be used to access their files.

Fix: remove the weird nginx configuration or fix the open redirect

#### Vuln 2: Login bypass

Login works with an [OPAQUE](https://eprint.iacr.org/2018/163.pdf)-like scheme, to allow passwords to be unknown to the server at any time. The client part is done in the wasm library, while the server part is done by python and the compiled python module in the backend.

During registration the server receives a `Capsule` object, containing two randoms, $k_s$ and $p_s$, and two other values calculated by the client, $P_s$ and $P_u$. Additionally, an AES-GCM encrypted "vault" $C$ containing the secret user data $p_u$ and, in the additional data, $P_u$ and $P_s$, is sent. The encryption key is derived from the user's password.

In the login protocol, the client generates the values $\alpha$ and $X_u=g^{x_u}$ and sends them to the server. These values are used, together with the previous ones, to generale a session key that is then used in an `HKDF` instantiation to login. If we can calculate this key without knowing the password, we are done.

The key is calculated as $K=H((X_uP_u^d)^{x_s+e\cdot p_s} \pmod{p})$ where $H$ is an hash function. We control $X_u$, and we can receive $P_u$ by attempting a login. We also control $d=H(H(id+\alpha))$, where `id` is the user id. So we can make $K$ fallback to a trivial key by setting, for example, $X_u=P_u^{-d}$.

Fix: add a check that the generated key is not a trivial one.

## Exploits

| store | exploit                                                                      |
| :---: | :--------------------------------------------------------------------------- |
|   1   | [exploit1-ssrf.py](/exploits/NorthernFiles/exploit1-ssrf.py)                 |
|   1   | [exploit2-login-bypass.py](/exploits/NorthernFiles/exploit2-login-bypass.py) |
