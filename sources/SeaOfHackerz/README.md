# SeaOfHackerz

| Service       | SeaOfHackerz                                           |
| :------------ | :----------------------------------------------------- |
| Author(s)     | Vittorio Mignini <@M1gnus>, Matteo Protopapa <@matpro> |
| Store(s)      | 1                                                      |
| Category(ies) | crypto, web                                            |
| Port(s)       | 80, 5000                                               |
| FlagId(s)     | User IDs                                               |
| Checker(s)    | [store1](/checkers/SeaOfHackerz/checker.py)            |

## Description

SeaOfHackerz is a website in which you can register a user, and each user owns a ship. Users impersonate pirates, so they can customize the style of their ships and the content of its inventory. Finally, a user can attack another one, trying to guess a secret number: if the number is correct, then the guess is successful. After a given number of successful guesses, the attack is considered successful and the winning pirate can rob the inventory of the losing one.

## Vulnerabilities

### Store 1: Inventory treasure

The flags are hidden in a treasure inside the ship's inventory.

#### Vuln 1: SQL injection

The `login` endpoint uses a query which is not sanitized:

```python
cur.execute("SELECT * from users WHERE username = '%s' AND password = '%s'" % (data["username"], data["password"]))
```

This leads to a classical sql injection vulnerability. To patch this vulnerability is enough to use a prepared statement, exactly as with the other queries.

#### Vuln 2: Cookie forgery

Again inside the `login` endpoint, a forgeable session cookie is set. In fact, it uses cryptographic functions, but the seed for the random generator is the user id, known to the attackers. Therefore, one can easily forge a valid cookie for a given user.

The patch is to create a cookie using information not known to the attackers, or even better to use the standard way offered by flask.

#### Vuln 3: Attack RNG

The attack phase is based on an RNG called `cryptographically_secure_prng`, which turns out to be a LCG. This kind of RNG is very weak, and given 6 observations, one is confident enough to recover the internal state of the RNG and to predict future values.

The patch here is to substitute the RNG with a more robust one, maybe using the `os.urandom` function.

## Exploits

| store | exploit                                                       |
| :---: | :------------------------------------------------------------ |
|   1   | [exploit_sqli.py](/exploits/SeaOfHackerz/exploit_sqli.py)     |
|   2   | [exploit_cookie.py](/exploits/SeaOfHackerz/exploit_cookie.py) |
|   3   | [exploit_rng.py](/exploits/SeaOfHackerz/exploit_rng.py)       |
