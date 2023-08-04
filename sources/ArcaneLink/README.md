# ArcaneLink

| Service       | ArcaneLink                                              |
| :------------ | :------------------------------------------------------ |
| Author(s)     | Marco Bonelli (@mebeim), Vincenzo Bonforte (@bonfee)    |
| Store(s)      | 1                                                       |
| Category(ies) | pwn, system                                             |
| Port(s)       | 1337                                                    |
| FlagId(s)     | VM users' UID                                           |
| Checker(s)    | [store1](../../checkers/ArcaneLink/checker/__main__.py) |

## Description

This challenge consists of a QEMU device (ArcaneLink) that is meant to function
as a intra-vm communication channel between different QEMU VMs running on the
same host, and a VM manager backend to handle access to the VMs on demand.

There are two QEMU virtual machines (guest VMs) of two different architectures
(i386 and arm64) running through libvirt using a custom QEMU binary, and managed
through a Python backend. Guest VMs run Linux Debian 12, and users connecting to
the server are able to create temporary sessions and execute arbitrary commands
through a shell on either of the two guest VMs.

The guest VMs can interact with the ArcaneLink QEMU device through memory-mapped
I/O, with the help of a simple kernel module. A set of physical memory pages are
reserved by the device, and each one is dedicated to a single UID inside the
Linux guest.

The challenge has multiple components (see their linked README documents for
more information):

- [`backend`](../../sources/ArcaneLink/backend): Python3 backend that runs on
  each player's vulnbox and manages user sessions on the guest VMs.
- [`cli`](../../sources/ArcaneLink/cli): command-line interface program to
  easily interact with the ArcaneLink device from the shell, installed inside
  the guest VMs as `arcane-cli`.
- [`jail`](../../sources/ArcaneLink/jail): a nsjail wrapper around the bash
  shell that limits the actiona of players operating inside guest VMs. It is
  installed inside the guest VMs as `nsjail-bash` and executed as shell whenever
  a user wants to connect to the guest VMs through the service.
- [`kmod`](../../sources/ArcaneLink/kmod): helper kernel module that allows
  direct communication with the ArcaneLink device. It is installed inside the
  guest VMs and automatically loaded at startup. It allows userspace programs to
  `open` and `mmap` the `/dev/arcane` device to create a memory area to talk to
  the QEMU device directly through MMIO.
- [`qemu`](../../sources/ArcaneLink/qemu): QEMU 7.2.1 patches and source code
  for the ArcaneLink device itself.
- [`token_server`](../../sources/ArcaneLink/token_server): an external service
  that provides temporary signed JWT tokens to rate-limit access to the
  challenge.

The main component of interest is `qemu`, and secondly `backend`.

### ArcaneLink driver

The ArcaneLink QEMU driver manages per-user-id message queues. Through this
driver, users with the same UID across the two guest VMs can communicate. It
accepts 4 simple commands through MMIO:

- `CMD_PEEK_MSG` (0x00): return an existing message from any user given the
  message index, the user id to peek from, and the user key.
- `CMD_PUSH_MSG` (0x01): push a new message given an id and a content.
- `CMD_POP_MSG` (0x02): pop an existing message given its id.
- `CMD_GEN_KEY` (0x03): generate and return a new random user key for the
  current user.
- `CMD_CHK_KEY` (0x04): check whether a given key correctly authenticates a
  given user.

The first 3 commands are implemented in QEMU through Linux System V message IPC
on the host. Specifically, they mirror almost exactly the functionality of the
[`msgsnd(2)`](https://manned.org/msgsnd.2) and
[`msgrcv(2)`](https://manned.org/msgrcv.2) syscalls. The keys are simple 8-byte
values stored in memory using System V shared memory IPC, i.e.,
[`shmget(2)`](https://manned.org/shmget.2) and
[`shmat(2)`](https://manned.org/shmat.2).

Guest requests to issue commands have the following structure:

```c
#define MSG_SZ 0x100
typedef struct __attribute__((packed)) guest_req {
    uint32_t status;  /* [out] */
    uint32_t cmd;     /* [in]  */
    uint32_t mid;     /* [in] if cmd == CMD_PUSH_MSG, CMD_PEEK_MSG */
    uint32_t uid;     /* [in] if cmd == CMD_PEEK_MSG */
    uint64_t key;     /* [in] if cmd == CMD_PEEK_MSG, [out] if cmd == CMD_GEN_KEY */
    char buf[MSG_SZ]; /* [in] if cmd == CMD_PUSH_MSG, [out] if cmd == CMD_PEEK_MSG, CMD_POP_MSG */
} guest_req_t;
```

This structure is present at the beginning of each physical page reserved for
users in the guest. After mapping the page through the `/dev/arcane` device and
filling the fields with data, a memory write at the offset of the `cmd` field
will trigger the request in the ArcaneLink device. The `status` is atomically
set to `STATUS_BUSY` (`0x01000000`) when the write is issued and changes to
either `STATUS_OK` (`0x00000000`) or `STATUS_ERROR|err_code`
(`0x02000000|err_code`) when the request is completed.

## Vulnerabilities

Flags are stored in ArcaneLink messages pushed by the checker with
`CMD_PUSH_MSG`, mixed with other randomly generated hexadecimal strings.

### Vuln 1 - Info leak when checking key

The `CMD_CHK_KEY` command uses the `memcmp()` function to compare the provided
key with the correct one (instead of e.g. a simple `uint64_t` integer
comparison). The lowest 16-bits of the return code of `memcmp()` are then
returned ORed with `STATUS_OK` like this:

```c
return STATUS_OK|(uint16_t)memcmp(actual_key, req->key, 8);
```

This leaks one byte of information about the key. Specifically, `memcmp()`
returns the difference of the first two non-matching bytes found, so sending a
carefully crafted key and adjusting one byte at a time for a total of 8 requests
is enough to leak an entire key for a given UID. After doing this, the key can
be used to peek all of the user's messages with `CMD_MSG_PEEK`.

### Vuln 2 - Double fetch in message peek command + race condition

The ArcaneLink QEMU device handles guest requests by spawning QEMU threads.
There can be at most one active request (thread) per guest UID, and while a
request is pending for a given UID, any additional requests are suppressed.

The `CMD_PEEK_MSG` command performs a double-fetch on the `uid` field of guest
requests, accessing `req->uid` two times in a row, before and after
authenticating the reqiest with the supplied UID and key:

```c
    case CMD_PEEK_MSG: {
        key = req->key;

        /* First fetch */
        if ((ret = auth_uid(req->uid, key)) != 0) {
            arcanelog("    [-] auth failed\n");
            ret |= STATUS_ERROR;
            break;
        }

        // Second fetch after successful authentication
        qid = get_qid_from_uid(req->uid);
        if (qid == -1) {
            arcanelog("    [-] invalid uid\n");
            ret = STATUS_ERROR | ERR_INVALID_UID;
            break;
        }

        if (msgrcv(qid, &msg, sizeof(msg.mtext), req->mid,  MSG_COPY | MSG_NOERROR | IPC_NOWAIT) < 0) {
            /* ... */
        }
        /* ... */
        break;
    }
```

If the UID is somehow changed *after the autentication* but *before message
retrieval*, then an arbitrary message of an arbitrary user can be peeked.

As the `arcane_guest_write()` handler for MMIO writes uses locking to safeguard
writes at offsets within the `uid` field, it is not possible to directly change
its value until the current pending request is complete:

```c
    if (pgoff >= offsetof(guest_req_t, uid) &&
        pgoff < (offsetof(guest_req_t, uid) + sizeof_field(guest_req_t, uid))) {

        // A request is already pending, don't overwrite uid
        if (qemu_mutex_trylock(&g_mutexes[mutex_idx])) {
            goto out;
        }

        // Reflect write to the backing memory
        memcpy((uint8_t *)g_guest_memory + addr, &val, size);
        qemu_mutex_unlock(&g_mutexes[mutex_idx]);
        goto out;
    }
```

However, no locking is done by the driver before modifying the request structure
in case the write offset is outside the `uid` field. The `->uid` field is at
offset 12 and has size 4. The `->idx` field is at offset 8 and also has size 4.
Issuing an 8-byte memory write at offset 8 within the MMIO area will therefore
overwrite both `->idx` and `->uid`, without any locking. This allows modifying
the UID of a request while the request was already scheduled for a different
UID, which can easily happen between the double-fetch window.

Writing at any other offset will not work as QEMU will split unaligned memory
writes in single 1-byte writes, so e.g. issuing an 8-byte write at offset `5`
will result in 8 1-byte writes at offsets `5,6,7,8,9,10,11,12` - the first 3
(`5,6,7`) and the last one (`12`) would happen without problem, but the ones
overlapping `->uid` (`8,9,10,11`) would be blocked by the locking.

This vulnerability is exploitable from both guest VMs, however, performing an
atomic 8-byte write in the i386 VM requires specialized instructions from the
MMX or SSE instruction set extensions (such as `movq m64, mmX`), so it will not
be easily generated by GCC without using intrinsics/inline asm (assuming the
exploit is written in C). It is therefore easier to simply cross compile the
exploit for the arm64 VM.


### Patching the vulnerabilities

Due to the way the System V shared memory region containing keys is initialized
on QEMU startup, i.e. randomly through
[`getrandom(2)`](https://manned.org/getrandom.2), restarting any of the VMs
managed by the service will cause all existing keys to be wiped and replaced by
new random ones. This means that any existing sessions will see their previously
valid keys become invalid. This breaks the checker when trying to retrieve valid
flags for previous ticks, so the call to `getrandom()` must be removed when
applying any kind of patch to the QEMU binaries and restarting the VMs.

#### Vulnerability 1

A simple patch for this vulnerability would be to only return `STATUS_OK|0` or
`STATUS_OK|1` by changing a couple of assembly instructions to adjust the value
of the `eax` register returned by `memcmp()` in `arcane_thread_worker()` before
it is ORed with `STATUS_OK` and returned.

#### Vulnerability 2

Two possible patches are:

1. Eliminate the double fetch present in `arcane_thread_worker()` by
   saving/restoring the original value somewhere (e.g. memory or an unused
   register).
2. Redirect the control flow in `arcane_guest_write()` to always perform the
   locking and *only then* `memcpy()` the user data. This requires patching an
   immediate value used to compare the write offset to decide whether to try
   locking, plus a jump instruction after the locking.

Changing the maximum allowed write size from 8 down to 4 would also make this
vulnerability unexploitable, and should be as easy as editing a couple bytes in
the `.bss` section of the QEMU binary, where `arcane_guest_io_ops` is defined:

```c
static const MemoryRegionOps arcane_guest_io_ops = {
    .valid.max_access_size = 8, // <<<
    .impl.max_access_size = 8, // <<<
};
```

This however would not suffice and bring the service down, as both the checker
and the `arcane-cli` binary that it uses perform 8-byte writes on the `->key`
field when using `CMD_CHK_KEY` or `CMD_PEEK_MSG` in the arm64 VM.


## Exploits

See [the readme](../../exploits/ArcaneLink/README.md) in the exploits directory
for more information.

| Store | Exploit                                                                                                                                      |
| :---: | :------------------------------------------------------------------------------------------------------------------------------------------- |
|   1   | [exploit_leak_key.c](../../exploits/ArcaneLink/exploit_leak_key.c), [exploit_leak_key.py](../../exploits/ArcaneLink/exploit_leak_key.py)      |
|   2   | [exploit_race_cond.c](../../exploits/ArcaneLink/exploit_race_cond.c), [exploit_race_cond.py](../../exploits/ArcaneLink/exploit_race_cond.py) |
