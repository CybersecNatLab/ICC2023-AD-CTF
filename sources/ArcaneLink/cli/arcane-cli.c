#include <stddef.h>
#include <stdio.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <time.h>
#include <limits.h>
#include <inttypes.h>

#define CMD_PEEK_MSG ((uint32_t)0x00)
#define CMD_PUSH_MSG ((uint32_t)0x01)
#define CMD_POP_MSG  ((uint32_t)0x02)
#define CMD_GEN_KEY  ((uint32_t)0x03)
#define CMD_CHK_KEY  ((uint32_t)0x04)

#define STATUS_OK    ((uint32_t)0x00000000)
#define STATUS_BUSY  ((uint32_t)0x01000000)
#define STATUS_ERROR ((uint32_t)0x02000000)

#define ERR_INVALID_UID ((uint32_t)0x03)
#define ERR_INVALID_KEY ((uint32_t)0x04)
#define ERR_MALLOC      ((uint32_t)0x05)
#define ERR_MSGSND      ((uint32_t)0x06)
#define ERR_MSGRCV      ((uint32_t)0x07)
#define ERR_UNK_CMD     ((uint32_t)0x08)
#define ERR_ENOMSG      ((uint32_t)0x09)

#define MSG_SZ 0x100

typedef struct __attribute__((packed)) guest_req {
	uint32_t status; /* [out] */
	uint32_t cmd;    /* [in]  */
	uint32_t mid;    /* [in] if CMD == CMD_PUSH_MSG, CMD_PEEK_MSG */
	uint32_t uid;    /* [in] if CMD == CMD_PEEK_MSG */
	uint64_t key;    /* [in] if CMD == CMD_PEEK_MSG */
	char buf[MSG_SZ];
} guest_req_t;

static char *name;

const char *custom_err_msg(uint32_t errcode) {
	const char *msg;
	switch (errcode) {
		case ERR_INVALID_UID:
			msg = "Invalid uid";
			break;
		case ERR_INVALID_KEY:
			msg = "Invalid key";
			break;
		case ERR_MALLOC:
			msg = "Device memory error";
			break;
		case ERR_MSGSND:
			msg = "Device send error";
			break;
		case ERR_MSGRCV:
			msg = "Device receive error";
			break;
		case ERR_UNK_CMD:
			msg = "Unknown CMD";
			break;
		case ERR_ENOMSG:
			msg = "No such message";
			break;
		default:
			msg = "Unknown error code";
	}
	return msg;
}

void execute_req(uint32_t cmd, volatile guest_req_t *req) {
	const struct timespec sleep_time = { .tv_sec = 0, .tv_nsec = 10000000};
	uint32_t ret;

	// Set an invalid status as a sanity check
	req->status = 0xdeadbeef;

	// Issue command
	req->cmd = cmd;

	while (req->status == STATUS_BUSY)
		nanosleep(&sleep_time, NULL);

	ret = req->status;

	switch (ret & 0xff000000) {
		case STATUS_OK:
			puts("OK");
			break;
		case STATUS_ERROR:
			printf("ERROR: %s\n", custom_err_msg(ret & 0xff));
			exit(1);
			break;
		default:
			printf("UNEXPECTED STATUS: 0x%x\n", ret);
			exit(1);
			break;
	}

	switch (cmd) {
		case CMD_PEEK_MSG:
			puts((void *)req->buf);
			break;
		case CMD_PUSH_MSG:
			break;
		case CMD_POP_MSG:
			puts((void *)req->buf);
			break;
		case CMD_GEN_KEY:
			printf("Key: 0x%" PRIx64 "\n", req->key);
			break;
		case CMD_CHK_KEY:
			if (ret & 0xffffff)
				puts("Invalid key");
			else
				puts("Key is valid");
			break;
		default:
			/* unreachable */
			break;
	}
}

void setup_req(uint32_t cmd, char **args, volatile guest_req_t *req) {
	switch (cmd) {
		case CMD_PUSH_MSG:
			req->mid = atoi(args[2]);
			strncpy((char *)req->buf, args[3], sizeof(req->buf));
			req->buf[sizeof(req->buf) - 1] = '\0';
			break;
		case CMD_POP_MSG:
			req->mid = atoi(args[2]);
			break;
		case CMD_PEEK_MSG:
			req->uid = atoi(args[2]);
			req->mid = atoi(args[3]);
			req->key = strtoull(args[4], NULL, 0);
			break;
		case CMD_GEN_KEY:
			break;
		case CMD_CHK_KEY:
			req->uid = atoi(args[2]);
			req->key = strtoull(args[3], NULL, 0);
			break;
		default:
			err(1, "BAD COMMAND");
	}
}

void usage(void) {
	fprintf(stderr, "Usage:\n"
		"    %1$s push <id> <msg>\n"
		"    %1$s pop  <id>\n"
		"    %1$s peek <uid> <index> <key>\n"
		"    %1$s genkey\n"
		"    %1$s chkkey <uid> <key>\n"
		"Examples:\n"
		"    %1$s push 1337 Hello\n"
		"    %1$s pop  1337\n"
		"    %1$s peek 1000 0 0x133713371337\n"
		"    %1$s chkkey 1000 0x133713371337\n",
		name
	);
	exit(1);
}

int main(int argc, char **argv) {
	char *cmd_str;
	int fd;
	volatile guest_req_t *req;
	uint32_t cmd;

	name = argv[0] ?: "arcane-cli";

	if (argc < 2) usage();

	if ((fd = open("/dev/arcane", O_RDWR)) < 0)
		err(1, "open");

	req = (void *)mmap(NULL, 0x1000, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (req == MAP_FAILED)
		err(1, "mmap");

	cmd_str = argv[1];

	if (!strcasecmp(cmd_str, "push")) {
		if (argc != 4) usage();
		cmd = CMD_PUSH_MSG;
	} else if (!strcasecmp(cmd_str, "pop")) {
		if (argc != 3) usage();
		cmd = CMD_POP_MSG;
	} else if (!strcasecmp(cmd_str, "peek")) {
		if (argc != 5) usage();
		cmd = CMD_PEEK_MSG;
	} else if (!strcasecmp(cmd_str, "genkey")) {
		if (argc != 2) usage();
		cmd = CMD_GEN_KEY;
	} else if (!strcasecmp(cmd_str, "chkkey")) {
		if (argc != 4) usage();
		cmd = CMD_CHK_KEY;
	} else {
		usage();
	}

	setup_req(cmd, argv, req);
	execute_req(cmd, req);
	return 0;
}
