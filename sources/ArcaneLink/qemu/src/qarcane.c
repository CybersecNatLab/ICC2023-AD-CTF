#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "qemu/thread.h"
#include "qemu/typedefs.h"

#include <pthread.h>
#include <sys/random.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <err.h>
#include <sys/shm.h>
#include <sys/param.h>

// Make it easier to reverse, avoid inlined SSE/AVX memcpy
#pragma GCC push_options
#pragma GCC target "no-avx,no-avx2,no-sse,no-sse2,no-sse4.1,no-sse4.2,no-ssse3"

#define atomic_load(p) __atomic_load_n((p), __ATOMIC_RELAXED)
#define atomic_store(p, v) __atomic_store_n((p), (v), __ATOMIC_RELAXED)

#define TYPE_ARCANE "arcane"
#define ARCANE(obj) OBJECT_CHECK(ArcaneState, (obj), TYPE_ARCANE)

#define STARTING_UID 1000
#define N_UIDS 128

/*
    Used to talk with the users
*/
#define MSG_SZ 0x100

typedef struct __attribute__((packed)) guest_req {
    uint32_t status;  /* [out] */
    uint32_t cmd;     /* [in]  */
    uint32_t mid;     /* [in] if cmd == CMD_PUSH_MSG, CMD_PEEK_MSG */
    uint32_t uid;     /* [in] if cmd == CMD_PEEK_MSG */
    uint64_t key;     /* [in] if cmd == CMD_PEEK_MSG, [out] if cmd == CMD_GEN_KEY */
    char buf[MSG_SZ]; /* [in] if cmd == CMD_PUSH_MSG, [out] if cmd == CMD_PEEK_MSG, CMD_POP_MSG */
} guest_req_t;

/*
    IPC msg
*/
typedef struct msg {
    long mtype;
    char mtext[MSG_SZ];
} msg_t;

/*
    Struct to share data with cmd handler threads
*/
typedef struct cmd_args {
    QemuMutex *mutex;
    guest_req_t *req;
} cmd_args_t;

const hwaddr g_guest_pages_phys = 0x13370000;

/*
    Backing store of the mmio pages
*/
void *g_guest_memory = NULL;

/*
    Message queues
*/
uint32_t g_msg_queues[N_UIDS];

/*
    Shmem memory used to share keys between qemu instances
*/
uint64_t *g_keys;
#define SHM_KEYS_SIZE (roundup(N_UIDS*sizeof(uint64_t), 0x1000))
int g_shm_keys_id;

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion guest_region;
} ArcaneState;

/*
    Mutexes used to sync threads, one per uid
*/
QemuMutex g_mutexes[N_UIDS];

/*
    List of cmds
*/
#define CMD_PEEK_MSG 0x00
#define CMD_PUSH_MSG 0x01
#define CMD_POP_MSG  0x02
#define CMD_GEN_KEY  0x03
#define CMD_CHK_KEY  0x04

/*
    Possible return codes of a cmd
*/
#define STATUS_OK    0x00000000
#define STATUS_BUSY  0x01000000
#define STATUS_ERROR 0x02000000

/*
    Specific errcodes
*/
#define ERR_INVALID_UID    0x03
#define ERR_INVALID_KEY    0x04
#define ERR_MALLOC         0x05
#define ERR_MSGSND         0x06
#define ERR_MSGRCV         0x07
#define ERR_UNK_CMD        0x08
#define ERR_ENOMSG         0x09

/*
    Logging utility macros
*/
#define arcanelog(...)                                         \
    do {                                                       \
        qemu_log_mask(ARCANE_LOG, "ArcaneLink: " __VA_ARGS__); \
    } while (0)

#define arcanelog_rw(...)                                         \
    do {                                                          \
        qemu_log_mask(ARCANE_LOG_RW, "ArcaneLink: " __VA_ARGS__); \
    } while (0)

#pragma GCC push_options
#pragma GCC optimize ("O0")
static __attribute__((noinline)) uint32_t get_uid_from_addr(hwaddr addr) {
    assert(addr >= g_guest_pages_phys);
    assert(addr < (g_guest_pages_phys + (N_UIDS * 0x1000)));
    addr &= ~0xfffLU;
    return STARTING_UID + ((addr - g_guest_pages_phys) / 0x1000);
}

static __attribute__((noinline)) int get_qid_from_uid(uint32_t uid) {
    if (uid < STARTING_UID || uid >= (STARTING_UID + N_UIDS))
        return -1;
    return g_msg_queues[uid - STARTING_UID];
}

static __attribute__((noinline)) int auth_uid(uint32_t uid, uint64_t key) {
    uint64_t saved_key;

    if (uid < STARTING_UID || uid >= (STARTING_UID + N_UIDS)) {
        arcanelog("    [-] auth ko: invalid uid\n");
        return ERR_INVALID_UID;
    }

    saved_key = atomic_load(&(g_keys[uid - STARTING_UID]));

    if (memcmp((void*)&saved_key, (void*)&key, sizeof(saved_key))) {
        arcanelog("    [-] auth ko: invalid key\n");
        return ERR_INVALID_KEY;
    }

    qemu_log("ArcaneLink:    [+] auth ok for %u\n", uid);
    return 0;
}

static void *arcane_thread_worker(void* arg) {
    cmd_args_t *args = (cmd_args_t*)arg;
    volatile guest_req_t *req = args->req;
    QemuMutex *mutex = args->mutex;
    uint32_t uid, ret;
    int qid;
    uint64_t key;
    msg_t msg;

    arcanelog("[*] %s: Executing CMD 0x%x\n", __func__, req->cmd);

    switch (req->cmd) {
        case CMD_GEN_KEY: {
            while (getrandom(&key, sizeof(key), 0) == -1 && errno == EINTR) ;

            uid = get_uid_from_addr((hwaddr)req - (hwaddr)g_guest_memory + g_guest_pages_phys);
            atomic_store(&(g_keys[uid - STARTING_UID]), key);

            req->key = key;
            ret = STATUS_OK;
            break;
        }
        case CMD_CHK_KEY: {
            uint64_t saved_key;

            uid = req->uid;
            key = req->key;

            if (uid < STARTING_UID || uid >= (STARTING_UID + N_UIDS)) {
                ret = STATUS_ERROR | ERR_INVALID_UID;
                break;
            }

            saved_key = atomic_load(&(g_keys[uid - STARTING_UID]));
            ret = memcmp(&saved_key, &key, sizeof(saved_key));

            // [BUG] Leak key with side channel
            ret = STATUS_OK | ((uint16_t)ret);
            break;
        }
        case CMD_PUSH_MSG: {
            uid = get_uid_from_addr((hwaddr)req - (hwaddr)g_guest_memory + g_guest_pages_phys);
            qid = get_qid_from_uid(uid);

            if (qid == -1) {
                arcanelog("    [-] invalid uid\n");
                ret = STATUS_ERROR | ERR_INVALID_UID;
                break;
            }

            msg.mtype = req->mid;
            memcpy(msg.mtext, (void*)(req->buf), sizeof(msg.mtext));

            if (msgsnd(qid, &msg, sizeof(msg.mtext), IPC_NOWAIT) < 0) {
                arcanelog("    [-] msgsnd() failed\n");
                ret = STATUS_ERROR | ERR_MSGSND;
                break;
            }
            ret = STATUS_OK;
            break;
        }
        case CMD_POP_MSG: {
            uid = get_uid_from_addr((hwaddr)req - (hwaddr)g_guest_memory + g_guest_pages_phys);
            qid = get_qid_from_uid(uid);

            if (qid == -1) {
                arcanelog("    [-] invalid uid\n");
                ret = STATUS_ERROR | ERR_INVALID_UID;
                break;
            }

            if (msgrcv(qid, &msg, sizeof(msg.mtext), req->mid,  MSG_NOERROR | IPC_NOWAIT) < 0) {
                if (errno == ENOMSG)
                    ret = STATUS_ERROR | ERR_ENOMSG;
                else
                    ret = STATUS_ERROR | ERR_MSGRCV;

                arcanelog("    [-] msgrcv() failed\n");
                break;
            }

            memcpy((void *)req->buf, &msg.mtext, sizeof(msg.mtext));
            ret = STATUS_OK;
            break;
        }
        case CMD_PEEK_MSG: {
            key = req->key;

            if ((ret = auth_uid(req->uid, key)) != 0) {
                arcanelog("    [-] auth failed\n");
                ret |= STATUS_ERROR;
                break;
            }

            qid = get_qid_from_uid(req->uid);
            if (qid == -1) {
                arcanelog("    [-] invalid uid\n");
                ret = STATUS_ERROR | ERR_INVALID_UID;
                break;
            }

            if (msgrcv(qid, &msg, sizeof(msg.mtext), req->mid,  MSG_COPY | MSG_NOERROR | IPC_NOWAIT) < 0) {
                if (errno == ENOMSG)
                    ret = STATUS_ERROR | ERR_ENOMSG;
                else
                    ret = STATUS_ERROR | ERR_MSGRCV;

                arcanelog("    [-] msgrcv() failed\n");
                break;
            }

            memcpy((void*)(req->buf), msg.mtext, sizeof(msg.mtext));
            ret = STATUS_OK;
            break;
        }
        default:
            arcanelog("[*] %s: Unknown command\n", __func__);
            ret = STATUS_ERROR | ERR_UNK_CMD;
            break;
    }

    req->status = ret;
    qemu_mutex_unlock(mutex);

    g_free(args);
    return NULL;
}
#pragma GCC pop_options

// hwaddr addr is actually an offset in the memory region
static void arcane_guest_write(void *opaque, hwaddr addr, uint64_t val, unsigned size) {
    guest_req_t *req = (void *)((uint8_t *)g_guest_memory + (addr & ~0xfffLU));
    uint64_t pgoff = addr & 0xfff;
    uint32_t mutex_idx = get_uid_from_addr(g_guest_pages_phys + addr) - STARTING_UID;
    cmd_args_t *args = NULL;
    QemuThread th;

    arcanelog_rw("[+] %s(addr=0x%lx, pgoff=0x%lx, val=0x%lx, size=0x%x)\n", __func__, addr, pgoff, val, size);

    // Disallow writing beyond guest request struct
    if ((pgoff + size) >= sizeof(guest_req_t))
        return;

    // [BUG] uid must not be overwritten when a cmd (CMD_PEEK_MSG) is running
    if (pgoff >= offsetof(guest_req_t, uid) &&
            pgoff < (offsetof(guest_req_t, uid) + sizeof_field(guest_req_t, uid))) {

        // thread is running, dont overwrite uid
        if (qemu_mutex_trylock(&g_mutexes[mutex_idx])) {
            goto out;
        }

        // Reflect write to the backing memory
        memcpy((uint8_t *)g_guest_memory + addr, &val, size);
        qemu_mutex_unlock(&g_mutexes[mutex_idx]);
        goto out;
    }

    // Reflect write to the backing memory
    memcpy((uint8_t *)g_guest_memory + addr, &val, size);

    // Whenever cmd is overwritten we handle the cmd
    if (pgoff != offsetof(guest_req_t, cmd) ||
            size != sizeof_field(guest_req_t, cmd)) {
        goto out;
    }

    if (qemu_mutex_trylock(&g_mutexes[mutex_idx])) {
        arcanelog("[-] arcane_guest_write() lock failed\n");
        goto out;
    }

    args = g_malloc0(sizeof(*args));
    if (!args) {
        arcanelog("[-] g_malloc0() failed\n");
        req->status = STATUS_ERROR | ERR_MALLOC;
        goto out;
    }

    args->req = req;
    args->mutex = &g_mutexes[mutex_idx];
    req->status = STATUS_BUSY;

    qemu_thread_create(&th, "arcane-thread", arcane_thread_worker, args, QEMU_THREAD_DETACHED);
out:
    return;
}

static uint64_t arcane_guest_read(void *opaque, hwaddr addr, unsigned size) {
    uint64_t mask = -1UL >> (8 * (8 - size));

    arcanelog_rw("[+] %s(addr=0x%lx, size=0x%x)\n", __func__, addr, size);

    // Disallow reading beyond guest request struct
    if ((addr & 0xfffUL) >= sizeof(guest_req_t))
        return 0xffffffffffffffffUL & mask;

    return *(uint64_t *)((uint8_t *)g_guest_memory + addr) & mask;
}

static const MemoryRegionOps arcane_guest_io_ops = {
    .write = arcane_guest_write,
    .read = arcane_guest_read,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
    .valid.unaligned = true,
    .impl.min_access_size = 1,
    .impl.max_access_size = 8,
    .impl.unaligned = true,
    .endianness = DEVICE_NATIVE_ENDIAN
};

__attribute__((constructor)) static void arcane_gbl_init(void) {
    arcanelog("[+] arcane_gbl_init()\n");
}

static void arcane_init(Object *d) {
    arcanelog("[+] arcane_init()\n");
    return;
}

static void arcane_realize(DeviceState *d, Error **errp) {

    arcanelog("[+] arcane_realize()\n");

    SysBusDevice *dev = SYS_BUS_DEVICE(d);
    ArcaneState *sio = ARCANE(d);
    Object *obj = OBJECT(sio);
    MemoryRegion *sysbus = sysbus_address_space(dev);

    // Create one msgqueue per uid
    for (int i = 0; i < N_UIDS; i++) {
        if ((g_msg_queues[i] = msgget(STARTING_UID + i, 0666 | IPC_CREAT)) == -1) {
            qemu_log("msgget failed\n");
            exit(1);
        }
    }

    // Alloc backing memory of the mmio pages
    g_guest_memory = mmap(NULL, 0x1000 * N_UIDS, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (g_guest_memory == MAP_FAILED) {
        qemu_log("mmap failed\n");
        exit(1);
    }

    // Init mutexes
    for (int i = 0; i < N_UIDS; i++)
        qemu_mutex_init(&g_mutexes[i]);

    // Create shmem for keys
    if ((g_shm_keys_id = shmget(0xdeadc0de, SHM_KEYS_SIZE, 0644 | IPC_CREAT)) == -1) {
        qemu_log("shmget failed\n");
        exit(1);
    }

    // Attach segment to VMA
    if ((g_keys = shmat(g_shm_keys_id, NULL, 0)) == (void *)-1) {
        qemu_log("shmat failed\n");
        exit(1);
    }

    // Even if both qemu instances race this call it's not a problem,
    // we just want the keys to not be predictable
    while (getrandom(g_keys, SHM_KEYS_SIZE, 0) == -1 && errno == EINTR) ;

    memory_region_init_io(&sio->guest_region, obj, &arcane_guest_io_ops, sio, TYPE_ARCANE, 0x1000 * N_UIDS);
    sysbus_init_mmio(dev, &sio->guest_region);
    memory_region_add_subregion(sysbus, g_guest_pages_phys, &sio->guest_region);
}

static void arcane_reset(DeviceState *d) {
    arcanelog("[+] arcane_reset()\n");
    ArcaneState *sio = ARCANE(d);
    (void)!sio;
}

static void arcane_class_init(ObjectClass *klass, void *data) {
    arcanelog("[+] arcane_class_init()\n");

    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = arcane_realize;
    dc->reset = arcane_reset;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo arcane_info = {
    .name = TYPE_ARCANE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ArcaneState),
    .instance_init = arcane_init,
    .class_init = arcane_class_init
};

static void arcane_register_types(void) {
    arcanelog("[+] arcane_register_types()\n");

    type_register_static(&arcane_info);
}

type_init(arcane_register_types)
