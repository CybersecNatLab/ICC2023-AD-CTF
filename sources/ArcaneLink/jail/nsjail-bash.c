#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

static char nsjail_bin[] = "/usr/local/bin/nsjail";
static char nsjail_cfg[] = "/usr/local/etc/nsjail-bash.cfg";

static char **concat_args(char **a, char **b) {
	size_t a_len = 0, b_len = 0;
	char **res = NULL;

	for (char **p = a; *p; p++) a_len++;
	for (char **p = b; *p; p++) b_len++;

	res = malloc(sizeof(char *) * (a_len + b_len + 1));
	if (!res)
		err(1, "malloc()");

	for (size_t i = 0; i < a_len; i++) res[i] = a[i];
	for (size_t i = 0; i < b_len; i++) res[a_len + i] = b[i];
	res[a_len + b_len] = NULL;

	return res;
}

void main(int argc, char **argv) {
	char hostname[128] = "nsjail-bash";
	char jail_uid_str[32] = {0};
	char jail_gid_str[32] = {0};
	uid_t jail_uid;
	gid_t jail_gid;
	FILE *fp;

	if (argc < 1)
		errx(1, "Usage: nsjail-bash [BASH_ARGS ...]");

	jail_uid = getuid();
	jail_gid = getgid();

	if (geteuid() != 0 || jail_uid == 0 || jail_gid == 0)
		errx(1, "This program must be run as suid root from a non-root user");

	gethostname(hostname, sizeof(hostname) - 1);
	snprintf(jail_uid_str, sizeof(jail_uid_str), "%u", jail_uid);
	snprintf(jail_gid_str, sizeof(jail_gid_str), "%u", jail_gid);

	char **argp = concat_args((char *[]){
		nsjail_bin,
		"--really_quiet",
		"--config", nsjail_cfg,
		"--hostname", hostname,
		"--user", jail_uid_str,
		"--group", jail_gid_str,
		"--",
		"/bin/bash",
		NULL
	}, argv + 1);

	if ((fp = fopen("/proc/self/oom_score_adj", "w"))) {
		fputs("1000\n", fp);
		fclose(fp);
	}

	execv(nsjail_bin, argp);
	err(1, "execv()");
}
