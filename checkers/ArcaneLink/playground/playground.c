#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <asm/unistd.h>

#ifdef __i386__
#define NR_MMAP __NR_mmap2
#elif defined(__aarch64__)
#define NR_MMAP __NR_mmap
#else
#error "Unsupported arch/abi"
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define htonl(n) (((((uint32_t)(n) & 0xFF)) << 24) | \
                  ((((uint32_t)(n) & 0xFF00)) << 8) | \
                  ((((uint32_t)(n) & 0xFF0000)) >> 8) | \
                  ((((uint32_t)(n) & 0xFF000000)) >> 24))

#define ntohl(n) (((((uint32_t)(n) & 0xFF)) << 24) | \
                  ((((uint32_t)(n) & 0xFF00)) << 8) | \
                  ((((uint32_t)(n) & 0xFF0000)) >> 8) | \
                  ((((uint32_t)(n) & 0xFF000000)) >> 24))
#define htonll(x) (((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#else
#define htonl(n) (n)
#define ntohl(n) (n)
#define htonll(n) (n)
#define ntohll(n) (n)
#endif

#define IS_ERR_VALUE(x) ((unsigned long)(void *)(x) >= (unsigned long)-4095)

#define CMD_PEEK_MSG ((uint32_t)0x00)
#define CMD_PUSH_MSG ((uint32_t)0x01)
#define CMD_POP_MSG  ((uint32_t)0x02)
#define CMD_GEN_KEY  ((uint32_t)0x03)
#define CMD_CHK_KEY  ((uint32_t)0x04)

#define STATUS_OK    ((uint32_t)0x00000000)
#define STATUS_BUSY  ((uint32_t)0x01000000)
#define STATUS_ERROR ((uint32_t)0x02000000)

#define MSG_SZ 0x100

typedef struct __attribute__((packed)) guest_req {
	uint32_t status;
	uint32_t cmd;
	uint32_t mid;
	uint32_t uid;
	uint64_t key;
	char buf[MSG_SZ];
} guest_req_t;

typedef void (*read_func_t)(void *buf, size_t count);
typedef void (*write_func_t)(const void *buf, size_t count);

read_func_t enc_read;
write_func_t enc_write;

static long raw_syscall(int nr, long a1, long a2, long a3, long a4, long a5, long a6) {
	long res;

#ifdef __i386__
	register long rbp asm("rbp") = a6;
	asm volatile (
		"int $0x80"
		: "=a" (res)
		: "0"(nr), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5), "r"(rbp)
		: "memory"
	);
#elif defined(__aarch64__)
	register int w8 asm("w8") = nr;
	register long x0 asm("x0") = a1;
	register long x1 asm("x1") = a2;
	register long x2 asm("x2") = a3;
	register long x3 asm("x3") = a4;
	register long x4 asm("x4") = a5;
	register long x5 asm("x5") = a6;
	asm volatile (
		"svc \0430" // "svc #0" breaks syntax highlighting lol
		: "+r" (x0)
		: "r"(w8), "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
		: "memory"
	);
	return x0;
#else
#error "Unsupported arch/abi"
#endif

	return res;
}

#define syscall0(nr) raw_syscall((nr), 0, 0, 0, 0, 0, 0)
#define syscall1(nr, a1) raw_syscall((nr), (long)(a1), 0, 0, 0, 0, 0)
#define syscall2(nr, a1, a2) raw_syscall((nr), (long)(a1), (long)(a2), 0, 0, 0, 0)
#define syscall3(nr, a1, a2, a3) raw_syscall((nr), (long)(a1), (long)(a2), (long)(a3), 0, 0, 0)
#define syscall4(nr, a1, a2, a3, a4) raw_syscall((nr), (long)(a1), (long)(a2), (long)(a3), (long)(a4), 0, 0)
#define syscall5(nr, a1, a2, a3, a4, a5) raw_syscall((nr), (long)(a1), (long)(a2), (long)(a3), (long)(a4), (long)(a5), 0)
#define syscall6(nr, a1, a2, a3, a4, a5, a6) raw_syscall((nr), (long)(a1), (long)(a2), (long)(a3), (long)(a4), (long)(a5), (long)(a6))

static void raw_memset_fast(void *buf, uint64_t val, size_t count) {
	for (size_t i = 0; i < count / 8; i++)
		((uint64_t *)buf)[i] = val;
}

static void enc_readln(char *buf) {
	// No bound check, peer is trusted
	while (1) {
		enc_read((void *)buf, 1);

		if (*buf == '\n') {
			*buf = '\0';
			break;
		}

		buf++;
	}
}

static void enc_writeln(const char *buf) {
	// No bound check, peer is trusted
	while (*buf)
		enc_write((const void *)(buf++), 1);
	enc_write("\n", 1);
}

static void enc_writeln_ulong_hex(unsigned long val) {
	const char hex[] = "0123456789abcdef";
	enc_write("0x", 2);

	for (int i = sizeof(val) - 1; i >= 0; i--) {
		uint8_t hi = (val >> (i * 8 + 4)) & 0xf;
		uint8_t lo = (val >> (i * 8    )) & 0xf;
		enc_write(hex + hi, 1);
		enc_write(hex + lo, 1);
	}

	enc_write("\n", 1);
}

static uint32_t enc_read_u32(void) {
	uint32_t val;
	enc_read(&val, sizeof(val));
	return ntohl(val);
}

static uint64_t enc_read_u64(void) {
	uint64_t val;
	enc_read(&val, sizeof(val));
	return ntohll(val);
}

static void clean_req_and_exit(volatile guest_req_t *req, int retval) {
	raw_memset_fast((void *)req, 0, sizeof(guest_req_t));
	syscall1(__NR_exit_group, retval);
}

/**
 * A simple interactive program that reads commands and executes arbitrary
 * operations through MMIO on the /dev/arcane device.
 */
static void playground(volatile guest_req_t *req) {
	const struct timespec sleep_time = { .tv_sec = 0, .tv_nsec = 10000000};
	uint32_t cmd, status;
	uint64_t key, res;

	while (1) {
		enc_writeln("READY");
		cmd = enc_read_u32();

		switch (cmd) {
			case CMD_PEEK_MSG:
				req->mid = enc_read_u32();
				req->uid = enc_read_u32();
				req->key = enc_read_u64();
				// This ^^^ should be compiled as a a 64-bit store in arm64:
				//
				//     str x0, [x19, #16]
				//
				// This way the checker will fail if players patch the challenge
				// reducing the max size (8) for MMIO writes in the QEMU device.
				break;
			case CMD_PUSH_MSG:
				req->mid = enc_read_u32();
				enc_readln((char *)req->buf);
				req->buf[sizeof(req->buf) - 1] = '\0';
				break;
			case CMD_POP_MSG:
				req->mid = enc_read_u32();
				break;
			case CMD_GEN_KEY:
				break;
			case CMD_CHK_KEY:
				req->uid = enc_read_u32();
				req->key = enc_read_u64();
				// ^^^ Same reasoning as above for this.
				break;
			case 0xffffffff:
				clean_req_and_exit(req, 0);
				break;
			default:
				enc_writeln("BAD COMMAND");
				clean_req_and_exit(req, 1);
				continue;
		}

		// Set an invalid status as a sanity check
		req->status = 0xdeadbeef;
		// Issue command
		req->cmd = cmd;

		while ((status = req->status) == STATUS_BUSY)
			syscall2(__NR_nanosleep, &sleep_time, NULL);

		switch (status & 0xff000000) {
			case STATUS_OK:
				enc_writeln("OK");
				break;
			case STATUS_ERROR:
				enc_write("ERROR: ", 7);
				enc_writeln_ulong_hex(status & 0xffffff);
				continue;
			default:
				enc_write("UNEXPECTED STATUS: ", 19);
				enc_writeln_ulong_hex(status);
				continue;
		}

		switch (cmd) {
			case CMD_PEEK_MSG:
				enc_writeln((char *)req->buf);
				break;
			case CMD_PUSH_MSG:
				break;
			case CMD_POP_MSG:
				enc_writeln((char *)req->buf);
				break;
			case CMD_GEN_KEY:
				key = htonll(req->key);
				enc_write(&key, sizeof(key));
				break;
			case CMD_CHK_KEY:
				enc_write((uint8_t[]){!(status & 0xffffff)}, sizeof(uint8_t));
				break;
			default:
				/* unreachable */
				break;
		}
	}
}

void __attribute__((section(".entry"))) entry(read_func_t r, write_func_t w) {
	volatile guest_req_t *req;
	int fd;

	enc_read = r;
	enc_write = w;

	fd = syscall3(__NR_openat, AT_FDCWD, "/dev/arcane", O_RDWR);
	if (fd < 0) {
		enc_write("open failed: ", 13);
		enc_writeln_ulong_hex((unsigned long)fd);
		syscall1(__NR_exit_group, 1);
	}

	req = (void *)syscall6(NR_MMAP, 0, 0x1000, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (IS_ERR_VALUE(req)) {
		enc_write("mmap failed: ", 13);
		enc_writeln_ulong_hex((unsigned long)req);
		syscall1(__NR_exit_group, 1);
	}

	playground(req);
	clean_req_and_exit(req, 1);
}
