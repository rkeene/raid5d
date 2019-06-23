#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

typedef struct {
	unsigned long long block;
	unsigned char *buf;
} buf_cache_st;

buf_cache_st buf_cache[80];
unsigned char *bufs[1024];
unsigned long blocksize = 128 * 1024;
int fds[1024];
int missing_idx = -1;
int num_disks;

uint64_t ntohll(uint64_t n) {
        uint64_t retval;

#if __BYTE_ORDER == __BIG_ENDIAN
        retval = n;
#else
        retval = ((uint64_t) ntohl(n & 0xFFFFFFFFLLU)) << 32;
        retval |= ntohl((n & 0xFFFFFFFF00000000LLU) >> 32);
#endif
        return(retval);
}

uint64_t htonll(uint64_t n) {
        uint64_t retval;

#if __BYTE_ORDER == __BIG_ENDIAN
        retval = n;
#else
        retval = ((uint64_t) htonl(n & 0xFFFFFFFFLLU)) << 32;
        retval |= htonl((n & 0xFFFFFFFF00000000LLU) >> 32);
#endif
        return(retval);
}

int compute_parity(unsigned char *bufs[], unsigned int cnt, unsigned long blocksize, int outidx) {
	int idx;
	int i;

	if (outidx == -1) {
		/* No missing disk */
		return(0);
	}

	for (i = 0; i < blocksize; i++) {
		bufs[outidx][i] = 0;
	}

	for (idx = 0; idx < cnt; idx++) {
		if (idx == outidx) {
			continue;
		}

		for (i = 0; i < blocksize; i++) {
			bufs[outidx][i] ^= bufs[idx][i];
		}
	}

	return(0);
}

unsigned char *get_block(unsigned long long block) {
	unsigned long long in_block, in_buf;
	ssize_t read_ret;
	off_t lseek_ret;
	int idx, parity_idx;
	int cache_key;

	cache_key = block % (sizeof(buf_cache) / sizeof(buf_cache[0]));
	if (buf_cache[cache_key].buf != NULL) {
		if (buf_cache[cache_key].block == block) {
			return(buf_cache[cache_key].buf);
		}
	}

	in_block = block / (num_disks - 1);
	parity_idx = in_block % num_disks;
	in_buf = block % (num_disks - 1);

	if (in_buf >= parity_idx) {
		in_buf++;
	}

	if (in_buf != missing_idx) {
		idx = in_buf;

		lseek_ret = lseek(fds[idx], in_block * blocksize, SEEK_SET);
		if (lseek_ret == -1) {
			return(NULL);
		}

		read_ret = read(fds[idx], bufs[idx], blocksize);
		if (read_ret != blocksize) {
			perror("read");

			return(NULL);
		}

		buf_cache[cache_key].block = block;
		if (buf_cache[cache_key].buf == NULL) {
			buf_cache[cache_key].buf = malloc(blocksize);
		}
		memcpy(buf_cache[cache_key].buf, bufs[in_buf], blocksize);

		return(bufs[in_buf]);
	}

	for (idx = 0; idx < num_disks; idx++) {
		if (fds[idx] != -1) {
			lseek_ret = lseek(fds[idx], in_block * blocksize, SEEK_SET);
			if (lseek_ret == -1) {
				return(NULL);
			}

			read_ret = read(fds[idx], bufs[idx], blocksize);
			if (read_ret != blocksize) {
				perror("read");

				return(NULL);
			}
		}
	}

	compute_parity(bufs, num_disks, blocksize, missing_idx);

	buf_cache[cache_key].block = block;
	if (buf_cache[cache_key].buf == NULL) {
		buf_cache[cache_key].buf = malloc(blocksize);
	}

	memcpy(buf_cache[cache_key].buf, bufs[in_buf], blocksize);

	return(bufs[in_buf]);
}

unsigned char *get_bytes(unsigned long long start) {
	static unsigned char *tmpbuf = NULL, *tmp_block;
	unsigned long long block, offset;

	if (tmpbuf == NULL) {
		tmpbuf = malloc(blocksize * 2);
	}

	block = start / blocksize;
	offset = start % blocksize;

	tmp_block = get_block(block);
	if (tmp_block == NULL) {
		return(NULL);
	}
	memcpy(tmpbuf, tmp_block, blocksize);

	tmp_block = get_block(block + 1);
	if (tmp_block == NULL) {
		return(NULL);
	}
	memcpy(tmpbuf + blocksize, tmp_block, blocksize);

	return(tmpbuf + offset);
}

int raid5_init(char **disks, int disks_cnt) {
	int idx;

	num_disks = disks_cnt;

	for (idx = 0; idx < (sizeof(buf_cache) / sizeof(buf_cache[0])); idx++) {
		buf_cache[idx].block = 0;
		buf_cache[idx].buf = NULL;
	}

	for (idx = 0; idx < num_disks; idx++) {
		if (strcmp(disks[idx], "MISSING") == 0) {
			fds[idx] = -1;
			missing_idx = idx;

			continue;
		}

		fds[idx] = open(disks[idx], O_RDONLY);

		if (fds[idx] < 0) {
			perror(disks[idx]);

			return(1);
		}
	}

	for (idx = 0; idx < num_disks; idx++) {
		bufs[idx] = malloc(blocksize);
	}

	return(0);
}

int accept_conn(unsigned short port) {
	struct sockaddr_in localname;
	int master_sock, sock;
	int bind_ret, listen_ret;
	pid_t pid;

	while (1) {
		master_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

		if (master_sock < 0) {
			perror("socket");

			sleep(1);

			continue;
		}

		break;
	}

	localname.sin_family = AF_INET;
	localname.sin_port = htons(port);
	localname.sin_addr.s_addr = INADDR_ANY;

	while (1) {
		bind_ret = bind(master_sock, (struct sockaddr *) &localname, sizeof(localname));
		if (bind_ret < 0) {
			perror("bind");

			sleep(1);

			continue;
		}

		break;
	}

	while (1) {
		listen_ret = listen(master_sock, 1);
		if (listen_ret < 0) {
			perror("listen");

			sleep(1);

			continue;
		}

		break;
	}

	while (1) {
		sock = accept(master_sock, NULL, NULL);

		pid = fork();

		if (pid == 0) {
			break;
		}

		close(sock);
	}

	close(master_sock);

	return(sock);
}

#define NBD_REQUEST_MAGIC 0x25609513
#define NBD_REPLY_MAGIC 0x67446698
#define NBD_INIT_MAGIC 0x00420281861253LL

enum {
	NBD_CMD_READ = 0,
	NBD_CMD_WRITE = 1,
	NBD_CMD_DISC = 2,
	NBD_CMD_FLUSH = 3,
	NBD_CMD_TRIM = 4
};

void process_sock(int sock) {
	unsigned char buf[124], *reply_buf;
	uint32_t magic, len, type;
	uint64_t from, handle;
	uint32_t reply_magic = htonl(NBD_REPLY_MAGIC), reply_error;
	uint64_t init_magic = htonll(NBD_INIT_MAGIC), init_size = htonll(773094113280LL);

	write(sock, "NBDMAGIC", 8);
	write(sock, &init_magic, sizeof(init_magic));
	write(sock, &init_size, sizeof(init_size));
	write(sock, "\0\0\0\0", 4);
	write(sock, buf, 124);

	while (1) {
		read(sock, &magic, sizeof(magic));
		read(sock, &type, sizeof(type));
		read(sock, &handle, sizeof(handle));
		read(sock, &from, sizeof(from));
		read(sock, &len, sizeof(len));

		magic = ntohl(magic);
		type = ntohl(type);
		from = ntohll(from);
		len = ntohl(len);

		if (magic != NBD_REQUEST_MAGIC) {
			printf("Invalid packet, aborting.\n");

			return;
		}

		if (type != NBD_CMD_READ) {
			reply_error = htonl(EPERM);

			write(sock, &reply_magic, sizeof(reply_magic));
			write(sock, &reply_error, sizeof(reply_error));
			write(sock, &handle, sizeof(handle));

			continue;
		}

		reply_buf = get_bytes(from);

		if (reply_buf == NULL) {
			reply_error = htonl(EINVAL);

			write(sock, &reply_magic, sizeof(reply_magic));
			write(sock, &reply_error, sizeof(reply_error));
			write(sock, &handle, sizeof(handle));

			continue;
		}

		reply_error = 0;

		write(sock, &reply_magic, sizeof(reply_magic));
		write(sock, &reply_error, sizeof(reply_error));
		write(sock, &handle, sizeof(handle));
		write(sock, reply_buf, len);
	}
}

int main(int argc, char **argv) {
	unsigned short port;
	pid_t pid;
	int sock;

	if (argc <= 2) {
		fprintf(stderr, "Usage: raid5d <port> <disk>...\n");

		return(1);
	}

	argc--;
	argv++;

	port = atoi(argv[0]);

	argc--;
	argv++;

	if (raid5_init(argv, argc) != 0) {
		return(1);
	}

	pid = fork();
	if (pid != 0) {
		return(0);
	}

	pid = fork();
	if (pid != 0) {
		return(0);
	}

	setsid();
	chdir("/");

	sock = accept_conn(port);
	process_sock(sock);

	return(0);
}
