#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <memory>
#include "file_descriptor.h"

int usage() {
  printf("fastcopy: Copy a file from 'src' to 'dest' using zero copy I/O.\n"
         "Usage: fastcopy <src> <dest>\n");
  return 0;
}

/*
ssize_t splice_copy(int src, int dest) {
        if (!make_nonblocking(src)) {
                return -1;
        }

        if (!make_nonblocking(dest)) {
                return -1;
        }

        int pipefd[2] = { -1, -1 };
        int status = pipe2(pipefd, O_NONBLOCK);
        if (status != 0) {
                return status;
        }

        // TODO(tdial): Get max size from /proc file system.
        const int ONE_MEG = 1024 * 1024;
        status = fcntl(pipefd[1], F_SETPIPE_SZ, ONE_MEG);
        if (status != ONE_MEG) {
                close(pipefd[1]);
                close(pipefd[0]);
                return -1;
        }

        // Determine input file stats.
        struct stat file_info = {0};
        status = fstat(src, &file_info);
        if (status != 0) {
                close(pipefd[1]);
                close(pipefd[0]);
                return -1;
        }

        ssize_t bytes_remaining = file_info.st_size();
  ssize_t queied_in_pipe = 0;

        while (bytes_remaining > 0) {
                ssize_t xferred_to_pipe = splice(src, nullptr, pipefd[1],
nullptr, ONE_MEG, SPLICE_F_NONBLOCK); if (

        }

        close(pipefd[1]);
        close(pipefd[0]);

        return 0;
}
*/


int main(int argc, char *argv[]) {
	// We use the reentrant version of strerror, strerror_r, so we need this.
	char errbuf[256] = {0};

	// Check for proper usage, or request for help.
  if ((argc != 3) ||
			(0 == strcmp(argv[1], "-h")) ||
			(0 == strcmp(argv[1], "--help"))) {
    return usage();
  }

	// Open the input file.
	auto src_file = std::make_shared<FileDescriptor>(open(argv[1], O_RDONLY));
	if (src_file->Get() < 0) {
		fprintf(stderr, "%s: opening '%s' for reading.\n",
				strerror_r(errno, errbuf, sizeof(errbuf)), argv[1]);
		return 1;
	}

	// Open the output file.
	auto dest_file = std::make_shared<FileDescriptor>(
			open(argv[2], O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
	if (dest_file->Get() < 0) {
		fprintf(stderr, "%s: opening '%s' for writing.\n",
				strerror_r(errno, errbuf, sizeof(errbuf)), argv[2]);
		return 1;
	}

/*
  const int src = open(argv[1], O_RDONLY);
  if (src < 0) {
    fprintf(stderr, "error %d occurred opening '%s' for reading.\n", errno,
            argv[1]);
    return 1;
  }

        const int dest = open(argv[2], O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP |
S_IROTH); if (dest < 0) { fprintf(stderr, "error %d occurred opening '%s' for
writing.\n", errno, argv[2]); close(src); return 1;
        }

        ssize_t bytes_copied = splice_copy(src, dest);
  (void)bytes_copied;

        close(dest);
        close(src);
	*/

  return 0;
}

