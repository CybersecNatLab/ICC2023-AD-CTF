ArcaneLink - External Token Server
==================================

This directory contains a simple Python3 HTTP server to generate temporary JWT
tokens for rate-limiting purposes. This server should only be deployed once
externally and accesible by all teams.

```bash
# On an infrastructure server (reachable from teams' vulnboxes)
docker compose up
```

Tokens are generated for pairs of `sourceIP,targetIP`, containing the `targetIP`
as audience. Each `sourceIP,targetIP` pair can only have *one* valid token at
any time. Tokens are then validated by each backend instance (through
`arcanelink.backend`) when accepting a connection (see below).

Implemented endpoints are:

- `/token`: accepts a `target` query parameter with the IP of the desired
  vulnbox to connect to, and returning a temporary JWT token to be used for
  authorization. The same token will be returned until expired as long as the
  same host requests a token for the same target host.
- `/checker_token`: requires a `password` and a `target` query parameter. It
  is normally hidden (returns 404 unless the password is correct) and can be
  used by the checker to generate arbitrary tokens. It always generates and
  returns a new token.


Obtaining a token
-----------------

Send an HTTP GET request to `/token?target=<team_id>` to obtain a token to
connect to the vulnbox of a given team.

```none
$ curl 'http://<token-server-endpoint>/token?target=3
"eyJhbGciOiJFUzI1Ni..."
```

**Note that** the token returned by the token server is a JSON string and
therefore enclosed in double quotes (`"`), which must be stripped.
