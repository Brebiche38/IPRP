#include <stdlib.h>
#include <stdio.h>

typedef struct {
	iprp_link_t link;
	iprp_host_t host;
	iprp_ind_bitmap_t inds;
	struct in_addr dest_addr[IPRP_MAX_INDS];
} iprp_peerbase_t;

int peerbase_load(const char *path, iprp_peerbase_t *base);

int main(int argc, char const *argv[]) {
	if (argc != 3) return EXIT_FAILURE;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	inet_aton(argv[1], &addr.sin_addr);
	addr.sin_port = htons(atoi(argv[2]));

	char path[26];
	snprintf(path, 26, "")
}

int peerbase_load(const char *path, iprp_peerbase_t *base) {
	if (path == NULL || base == NULL) return -1;

	FILE* reader = fopen(path, "r");
	if (!reader) {
		ERR("Unable to read peerbase file", errno);
	}

	fread(base, sizeof(iprp_peerbase_t), 1, reader);

	fclose(reader);

	return 0;
}