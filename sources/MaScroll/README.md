# MaScroll

| Service       | MaScroll                                                                              |
| :------------ | :------------------------------------------------------------------------------------ |
| Author(s)     | Andrea Biondo <@abiondo>, Riccardo Bonafede <@bonaff>, Francesco Felet <@PhiQuadro>   |
| Store(s)      | 2                                                                                     |
| Category(ies) | crypto, pwn, web                                                                      |
| Port(s)       | 80                                                                                    |
| FlagId(s)     | Usernames, document IDs                                                               |
| Checker(s)    | [store1](/checkers/MaScroll-1/checker.py) / [store2](/checkers/MaScroll-2/checker.py) |

## Description

A Markdown document processor with support for VBA-like macros.

## Vulnerabilities

### Store 1: Usernames

In this store, the player knows the username that owns a document containing the flag.
The store is exploited through the web application.

#### Vuln 1: Windows's Files madness

PHP's file functions behave differently on a Windows system. First, files are case insensitive, second, because of how PHP reads and writes files, it is possible to use some characters as wildcards. Because of this, the web application suffers a discrepancy regarding the way it stores files and saves data in the database. The attacker can leverage this discrepancy to:

1. confuse the application by registering two users that will point to the same folder
2. Use the wildcard `<` character to read a file (in this case the flag) without knowing its name

### Store 2: Document IDs

In this store, the player knows the ID of a document containing the flag.
The store is exploited through the macro interpreter.

#### Vuln 1: Reentrancy in `SignToken`

The `SignToken` builtin calls `Len` after checking the document ownership, but before signing the document ID.
Since `SignToken` takes its argument by reference, the attacker can override `Len` and change the document ID before signing.
This allows the attacker to sign arbitrary document IDs.

#### Vuln 2: Type confusion in `FastRand` / `CryptoRand`

The `FastRand` and `CryptoRand` builtins suffer from a type confusion vulnerability: when called with two arguments, the type of the first argument is not checked.
This can be exploited to perform arbitrary writes of random bytes by passing an `IntegerArray` crafted to look like a `String`.
The address of `bcryptprimitives.dll` can be leaked through `Describe(CryptoRand)`.
By overwriting global data in `bcryptprimitives.dll`, it is possible to force the Windows crypto API RNG to return predictable random values.
Since this generator is used by the crypto API for choosing `k` for ECDSA signatures, the attacker can obtain signatures with known `k`, recover the private signing key, and sign arbitrary document IDs.

## Exploits

| store | exploit                                                               |
| :---: | :-------------------------------------------------------------------- |
|   1   | [exploit-1-wildcard.py](/exploits/MaScroll/exploit-1-wildcard.py)     |
|   2   | [exploit-2-confusion.py](/exploits/MaScroll/exploit-2-confusion.py)   |
|   2   | [exploit-2-reentrancy.py](/exploits/MaScroll/exploit-2-reentrancy.py) |

## Deployment notes

The `setup.ps1` script deploys the challenge on a Windows Server 2022 VM.

**WARNING:** all VMs must have the same version of `%WINDIR%\System32\bcryptprimitives.dll`.
The challenge is configured for the current version at the time of ICC 2023:

```text
File    : bcryptprimitives.dll
Version : 10.0.20348.1487
Date    : 2023-08-03
SHA256  : 70cf99d02b2ab5d7da174db50e5a7b01dde1560da68504b07efbb996c98f73b5
```

If your version differs, the flag store 2 checker and the type confusion exploit need to be adjusted:

1. Use `tools/bcryptprimitives.dll` to get the necessary constants for the next steps.
2. In `checkers/MaScroll-2/checker.py`, fix `PROCESSPRNG_LO12`.
3. In `exploits/MaScroll/exploit-2-confusion.py`, fix `PROCESSPRNG_OFF`, `G_TRUSTEDENVIRONMENT_OFF`, and `G_ROOTAESRNGSTATE_OFF`.

Fixing the checker requires no binary expertise, as the script in step 1 will calculate `PROCESSPRNG_LO12` for step 2 for you.
The script, however, does not produce `G_TRUSTEDENVIRONMENT_OFF` and `G_ROOTAESRNGSTATE_OFF` to fix the exploit, because calculating them requires symbols from the Microsoft symbol server.
Please use IDA, which will download the symbols for you, and follow the comments above the constants in the exploit for more information about what offsets you need to provide.
