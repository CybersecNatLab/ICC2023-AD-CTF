#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/kdf.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/err.h>

typedef void (*read_func_t)(void *buf, size_t count);
typedef void (*write_func_t)(const void *buf, size_t count);
typedef union {
	void *raw_ptr;
	void (*entry_func)(read_func_t r, write_func_t w);
} code_t;

static BIO *stdin_bio, *stdout_bio, *encrypted_stdin_bio, *encrypted_stdout_bio;
static bool dh_done = false;

static int err = 0;
#define checknz(v) do { err++; if (!(v)) { ERR_print_errors(dh_done ? encrypted_stdout_bio : stdout_bio); exit(err); } } while (0)
#define checkgz(v) do { err++; if ((v) <= 0) { ERR_print_errors(dh_done ? encrypted_stdout_bio : stdout_bio); exit(err); } } while (0)

static const char *dhparams = "\
-----BEGIN DH PARAMETERS-----\n\
MIIBCAKCAQEA3noIn2jYmm7yIsXA9hyZMYxUtOC6T3u9t3k3IYkxSvXL1w6NPR1n\n\
cPf8DzNHrkF3FSycJyYuDopWt9cipVPjI7IQLrDwVvLvbefxoAiC/oFBbxG41sjG\n\
/05dH9KHi+oCPInaBJZ8nr5xOi/9djjxGccws4osBOi6Zej1OwotoZuwTbeoYZfi\n\
FmyJElxU4rkVqSMDhDzKrsiRYj6B8OG07QNqbrtSlb4GPbcModvtwb7un/gSRuIz\n\
maAUH0+SgwTmDFtLAyPVC5Xl0qTgfTj1VA/nLEgFd7gWZoQ9KXZF5QxCswu7kSTf\n\
os6D0zfuXQzsHB5N0MdMYY4017NF6Y1EcwIBAg==\n\
-----END DH PARAMETERS-----\n";

static void rd(BIO *b, unsigned char *buf, size_t count) {
	size_t tot = 0;
	int n;

	while (tot < count) {
		n = BIO_read(b, buf + tot, count - tot);
		if (n <= 0 && BIO_should_retry(b))
			continue;
		checkgz(n);
		tot += n;
	}
}

static void wr(BIO *b, const unsigned char *buf, size_t count) {
	size_t tot = 0;
	int n;

	while (tot < count) {
		n = BIO_write(b, buf + tot, count - tot);
		if (n <= 0 && BIO_should_retry(b))
			continue;
		checkgz(n);
		tot += n;
	}
}

static void plain_read(void *buf, size_t count) {
	rd(stdin_bio, buf, count);
}

static void plain_write(const void *buf, size_t count) {
	wr(stdout_bio, buf, count);
}

void read_and_decrypt(void *buf, size_t count)  {
	rd(encrypted_stdin_bio, buf, count);
}

void encrypt_and_write(const void *buf, size_t count) {
	wr(encrypted_stdout_bio, buf, count);
}

static void dh(void) {
	const size_t iv_len = EVP_CIPHER_iv_length(EVP_aes_256_ctr());
	size_t key_len = EVP_CIPHER_key_length(EVP_aes_256_ctr());
	unsigned char *secret, *key, *my_iv, *peer_iv;
	size_t secret_len;
	EVP_PKEY *params = NULL, *my_key = NULL, *peer_key = NULL;
	EVP_PKEY_CTX *pctx;
	BIO *dh_bio;

	checknz(dh_bio = BIO_new_mem_buf(dhparams, strlen(dhparams)));
	checknz(PEM_read_bio_Parameters(dh_bio, &params));
	BIO_free(dh_bio);

	checknz(pctx = EVP_PKEY_CTX_new(params, NULL));
	checknz(EVP_PKEY_keygen_init(pctx));
	checknz(EVP_PKEY_keygen(pctx, &my_key));
	EVP_PKEY_CTX_free(pctx);
	EVP_PKEY_free(params);

	checknz(PEM_write_bio_PUBKEY(stdout_bio, my_key));
	checknz(PEM_read_PUBKEY(stdin, &peer_key, NULL, NULL));

	checknz(pctx = EVP_PKEY_CTX_new(my_key, NULL));
	checkgz(EVP_PKEY_derive_init(pctx));
	checkgz(EVP_PKEY_derive_set_peer(pctx, peer_key));
	checkgz(EVP_PKEY_derive(pctx, NULL, &secret_len));
	checknz(secret_len <= 2048);
	checknz(secret = OPENSSL_malloc(secret_len));
	checkgz(EVP_PKEY_derive(pctx, secret, &secret_len));
	EVP_PKEY_free(my_key);
	EVP_PKEY_free(peer_key);
	EVP_PKEY_CTX_free(pctx);

	checknz(pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL));
	checkgz(EVP_PKEY_derive_init(pctx));
	checkgz(EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()));
	checkgz(EVP_PKEY_CTX_set1_hkdf_key(pctx, secret, secret_len));
	checkgz(EVP_PKEY_CTX_set1_hkdf_salt(pctx, (unsigned char *)"ajeje", 5));
	checkgz(EVP_PKEY_CTX_add1_hkdf_info(pctx, (unsigned char *)"brazorf", 7));

	checknz(key = OPENSSL_malloc(key_len));
	checkgz(EVP_PKEY_derive(pctx, key, &key_len));
	EVP_PKEY_CTX_free(pctx);
	OPENSSL_free(secret);

	checknz(my_iv = OPENSSL_malloc(iv_len));
	checknz(peer_iv = OPENSSL_malloc(iv_len));
	checkgz(RAND_bytes(my_iv, iv_len));
	plain_write(my_iv, iv_len);
	plain_read(peer_iv, iv_len);

	checknz(encrypted_stdout_bio = BIO_new(BIO_f_cipher()));
	checknz(BIO_set_cipher(encrypted_stdout_bio, EVP_aes_256_ctr(), key, my_iv, 1));
	OPENSSL_free(my_iv);
	dh_done = true;

	checknz(encrypted_stdin_bio = BIO_new(BIO_f_cipher()));
	checknz(BIO_set_cipher(encrypted_stdin_bio, EVP_aes_256_ctr(), key, peer_iv, 0));
	OPENSSL_free(peer_iv);

	BIO_push(encrypted_stdout_bio, stdout_bio);
	BIO_push(encrypted_stdin_bio, stdin_bio);
}

int main(void) {
	uint32_t sz;
	code_t code;

	checknz(stdin_bio = BIO_new_fd(STDIN_FILENO, BIO_NOCLOSE));
	checknz(stdout_bio = BIO_new_fd(STDOUT_FILENO, BIO_NOCLOSE));

	dh();

	read_and_decrypt(&sz, sizeof(sz));
	sz = ntohl(sz);

	code.raw_ptr = mmap(NULL, sz, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (code.raw_ptr == MAP_FAILED) {
		encrypt_and_write("[wrapper] mmap failed\n", 22);
		_exit(1);
	}

	read_and_decrypt(code.raw_ptr, sz);
	code.entry_func(&read_and_decrypt, &encrypt_and_write);
	return 0;
}
