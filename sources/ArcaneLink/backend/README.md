ArcaneLink - Challenge Backend
==============================

The backend is composed of two modules:

- `backend`: the virtual machine manager, responsible for managing
  sessions and their associated users inside each virtual machine.
- `server`: a simple TCP server exposed to the outside, which will
  accept connections and talk to the `backend` through a TCP connection.


Connecting to the service
-------------------------

First, request a new temporary token from the token server at
`/token?target=<target_ip>`:

```bash
curl 'http://<token-server-endpoint>/token?target=<target_ip>
```

**Note that** the token returned by the token server is a JSON string and
therefore enclosed in double quotes (`"`), which must be stripped.

Then use the token to authenticate with the service running on the vulnbox at
`<target_ip>`.

```none
$ nc <target_ip> 1337
Token: <enter token here>
...
```
