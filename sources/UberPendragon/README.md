# Uber Pendragon

| Service       | Uber Pendragon                                                                       |
| :------------ | :----------------------------------------------------------------------------------- |
| Author(s)     | Francesco Felet <@PhiQuadro>, Vittorio Mignini <@M1gnus>, Matteo Protopapa <@matpro> |
| Store(s)      | 1                                                                                    |
| Category(ies) | crypto                                                                               |
| Port(s)       | 80, 5000                                                                             |
| FlagId(s)     | username of the flag holder                                                          |
| Checker(s)    | [store1](/checkers/UberPendragon/checker.py)                                         |

## Description

Uber Pendragon is a free transport service that allows you to book a ride, possibly shared with other users, on a dragon. The ride should start from one of the five available dragon stations and finish at one of the wonderful points of interest or the home of one of the passengers. Upon arrival the respective welcome message is displayed - in the case of a private residence, it is the doorbell of the owner.

In addition, the service allows you to monitor flight traffic through its frontend.

## Architecture

The service implements a basic client-server architecture. Users are supposed to use the `client.py` to interact with the backend.
During registration, the user's password is processed locally to produce a public key (by default, on the `secp256k1` curve), which is stored by the server alongside the username, address and doorbell details.

There isn't a proper login: authentication is guaranteed by the ability to sign a message composed of travel details, through a signature scheme resembling [musig](https://eprint.iacr.org/2018/068.pdf). This allows the aggregation of signatures of the same message, hence it is possible for multiple users to book the same ride with only one valid aggregate signature.

## Vulnerabilities

### Store 1: Doorbells

The flags of this service are stored as the name on the displayed doorbell.

#### Vuln 1: Improper key aggregation

The function `Hagg`, which is meant to be a random oracle used to build a coefficient meant to disrupt rogue key attacks in the plain public key model, is not a hash function. In particular, it is defined as $$ H*{agg}(L, X*{i}) = sum(L + X*{i}) $$ where $L$ and $X*{i}$ are bytes objects representing the concatenation of the $y$ coordinates of the public keys involved in the signature and the $y$ coordinate of the $i$-th public key respectively. This results in the outputs of $H_{agg}$ being relatively small integers, fairly close to each other. Furthermore, an (obvious) algebraic property catches the eye: $sum(X_{1} + X_{2}) = sum(X_{1}) + sum(X_{2})$.

This defies completely the purpose of adding those coefficients into the equation, once again allowing for a (slightly more sophisticated) rogue key attack.

Let $T = t \cdot G$ be the target public key (for which the adversary wants to forge a valid signature), $G$ being the generator point on the elliptic curve we are working on. Let also $\overline{X} = sum(X.y)$ be the (integer) sum of the bytes of the $y$ component of public key $X$.

Writing down the equation representing the resulting aggregate key, with $L$ being the (ordered) list of $n$ public keys $T, X_{0}, X_{1}, \dots, X_{n-2}$ and $X_{i} = x_{i} \cdot G$:

$$
\begin{aligned}
X_{agg} &= H_{agg}(L, T) \cdot T + \sum_{i = 0}^{n-2} H_{agg}(L, X_{i}) \cdot X_{i} \\
        &= \left(\left(\overline{L} + \overline{T}\right)t + \sum_{i = 0}^{n-2} \left(\overline{L} + \overline{X_{i}}\right)x_{i}\right)\cdot G
\end{aligned}
$$

Clearly, we want to somehow cancel out $t$, the private key of our target, to fully control the signature. Note that if we register a user with public key $T_{-1} = -T$ we have $(-T).y = -(T.y) \pmod{p} = p - T.y$, therefore $\overline{T} \neq \overline{(-T)}$ and the naive attack won't suffice.

Let $T_{k_{j}} = k_{j} \cdot T$ be multiples of the target public key for some (not necessarily distinct) integers $k_{j}$. If we set $X_{i} = T_{k_{j}}$ in the equation above (leaving at least one "true" public key for which we know the corresponding private, say $X_{n-2}$), we obtain:

$$
\begin{aligned}
X_{agg} &= \left(\left(\overline{L} + \overline{X_{n-2}}\right)x_{n-2} + \left(\overline{L} + \overline{T}\right)t + \sum_{j = 0}^{n-3} \left(\overline{L} + \overline{T_{k_{j}}}\right)k_{j}t \right) \cdot G \\
        &= \left(\left(\overline{L} + \overline{X_{n-2}}\right)x_{n-2} + \left(\overline{L}\left(1 + \sum_{j=0}^{n-3} k_{j}\right) + \left(\overline{T} + \sum_{j=0}^{n-3}k_{j}\overline{T_{k_{j}}}\right)\right)t \right) \cdot G
\end{aligned}
$$

Therefore, if we manage to find (possibly repeated) integers $k_{j}$ such that:

$$
\left\{
\begin{aligned}
0 &= 1 + \sum_{j=0}^{n-3} k_{j} \\
0 &= \overline{T} + \sum_{j=0}^{n-3}k_{j}\overline{T_{k_{j}}}
\end{aligned}
\right.
$$

we can forge a valid signature for $T$. Note that these equations must hold modulo the order of $G$, but given that $\overline{T_{k_{j}}} \ll ord(G)$ and are very close to each other it is more consistent to solve the problem over the integers. Given that we can send at most 12 users at once per ride, we are restricted to find a solution within at most 10 "dummy" keys.

### Patch

Players cannot change `Hagg` with a proper hash function, because the scheme is bound to the client, which is used by the checker.

The situation resembles a possible real world scenario in which a vulnerable client has been widely distributed and a hotfix needs to be applied server side. In this case, players need to build an "intrusion detection system" to reject signatures which could be attacks.

In particular, it is not wise to attempt to fingerprint attackers with external observations (for example rejecting every request with a duplicate public key: it may be that two different users share the same password). Therefore, players need to properly check if a given set of public keys is hiding a forgery: this happens for example when there is a subset of public keys for which the corresponding aggregate key "nullifies" (i.e. is the identity element of the group being used in the scheme). This gives a check with complexity $O(2^{n})$ where $n$ is the number of public keys being sent by the client: given that it can be at most 12, players can brute 4096 possible subsets in a relatively small amount of time.

To avoid possible DoS, optimizations can be introduced: for example we need to check only $$ \sum\_{i=1}^{\lfloor \frac{n}{2} \rfloor} \binom{n}{i} $$ subsets and for each one of those if it is either equal to the full aggregate public key or the identity element.

One can optimize even further with dynamic programming techniques or trying to delegate the whole check to a dedicated compiled binary.

Nevertheless, our testing patch (including only the first mentioned optimization) written in Python with `fastecdsa` performed checks under plausible load in approximately one second at most, which should be fairly safe given that purposeful DoS is prohibited by the competition rules.

## Exploits

| store | exploit                                          |
| :---: | :----------------------------------------------- |
|   1   | [exploit.py](/exploits/UberPendragon/exploit.py) |
