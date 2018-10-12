#include "file_descriptor.h"
#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

int usage() {
  printf("fastcopy: Copy a file from 'src' to 'dest' using splice().\n"
         "Usage: fastcopy <src> <dest>\n");
  return 0;
}

int main(int argc, char *argv[]) {
  // We use the reentrant version of strerror, strerror_r, so we need this.
  char errbuf[256] = {0};
  int status = 0;

  // Check for proper usage, or request for help.
  if ((argc != 3) || (0 == strcmp(argv[1], "-h")) ||
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

  // Get the size of the input (src) file.
  const size_t src_file_size = src_file->GetFileSize();
  if (src_file_size < 0) {
    fprintf(stderr, "%s: failed to get size of input file.\n",
            strerror_r(errno, errbuf, sizeof(errbuf)));
    return 1;
  }

  // Make the input file nonblocking.
  if (!src_file->MakeNonblocking()) {
    fprintf(stderr, "%s: while attempting to make file nonblocking.\n",
            strerror_r(errno, errbuf, sizeof(errbuf)));
    return 1;
  }

  // Open the output file.
  const int flag_bits = O_CREAT | O_WRONLY;
  const int mode_bits = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  auto dest_file =
      std::make_shared<FileDescriptor>(open(argv[2], flag_bits, mode_bits));
  if (dest_file->Get() < 0) {
    fprintf(stderr, "%s: opening '%s' for writing.\n",
            strerror_r(errno, errbuf, sizeof(errbuf)), argv[2]);
    return 1;
  }

  // Make the input file nonblocking.
  if (!dest_file->MakeNonblocking()) {
    fprintf(stderr, "%s: while attempting to make file nonblocking.\n",
            strerror_r(errno, errbuf, sizeof(errbuf)));
    return 1;
  }

  // Create a pipe for use with the splice() call.
  int pipefd[2] = {-1, -1};
  status = pipe2(pipefd, O_NONBLOCK);
  if (status != 0) {
    return status;
  }

  // Create automatic objects.
  auto read_pipe = std::make_shared<FileDescriptor>(pipefd[0]);
  auto write_pipe = std::make_shared<FileDescriptor>(pipefd[1]);

  // Reset the pipe's capacity to be large.
  // TODO(tdial): Figure out a truly reasonable value.
  // TODO(tdial): Make this configurable.
  const int ONE_MEG = 1024 * 1024;
  status = write_pipe->SetPipeCapacity(ONE_MEG);
  if (status != ONE_MEG) {
    fprintf(stderr,
            "failed to set pipe capacity to requested size (%d bytes)\n",
            status);
    return 1;
  }

  // Create an epoll object to manage event-driven I/O.
  auto epoller = std::make_shared<FileDescriptor>(epoll_create1(0));
  if (epoller->Get() < 0) {
    fprintf(stderr, "%s: failed to create epoll object.\n",
            strerror_r(errno, errbuf, sizeof(errbuf)));
    return 1;
  }

  // Aliases to avoid repeated calls to FileDescriptor::Get() in the loop.
  const int EPOLL_FD = epoller->Get();
  const int SOURCE_FILE = src_file->Get();
  const int DEST_FILE = dest_file->Get();
  const int PIPE_READER = read_pipe->Get();
  const int PIPE_WRITER = write_pipe->Get();

  // Add the write side of the pipe to the poll set.
  epoll_event ev;
  ev.events = EPOLLOUT;
  ev.data.fd = PIPE_WRITER;
  status = epoll_ctl(epoller->Get(), EPOLL_CTL_ADD, write_pipe->Get(), &ev);
  if (status == -1) {
    fprintf(stderr, "%s: failed to add write side of pipe to epoll set.\n",
            strerror_r(errno, errbuf, sizeof(errbuf)));
    return 1;
  }

  // Add the read side of the pipe to the poll set.
  ev.events = EPOLLIN;
  ev.data.fd = PIPE_READER;
  status = epoll_ctl(epoller->Get(), EPOLL_CTL_ADD, read_pipe->Get(), &ev);
  if (status == -1) {
    fprintf(stderr, "%s: failed to add write side of pipe to epoll set.\n",
            strerror_r(errno, errbuf, sizeof(errbuf)));
    return 1;
  }

  // We'll dequeue at most 2 events each pass, since there are two fd's.
  const int NUM_EVENTS = 2;
  struct epoll_event events[NUM_EVENTS] = {0};

  // Track the number of bytes written to the output file (the copy).
  size_t bytes_written_to_file = 0;

  // Loop, transferring data using splice operations until we
  // have transffered all of the data to the destination.
  while (bytes_written_to_file < src_file_size) {
    // Wait for readability, writeability on the ends of the pipe.
    const int nfds = epoll_wait(EPOLL_FD, events, NUM_EVENTS, -1);
    if (nfds == -1) {
      fprintf(stderr, "%s: epoll_wait() failed.\n",
              strerror_r(errno, errbuf, sizeof(errbuf)));
      return 1;
    }

    // Loop on returned events.
    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == PIPE_WRITER) {
        // The write side of the pipe is ready for data.
        const ssize_t xferred =
            splice(SOURCE_FILE, nullptr, PIPE_WRITER, nullptr, ONE_MEG,
                   SPLICE_F_MOVE | SPLICE_F_NONBLOCK);

        // Check for unrecoverable error.
        if ((xferred == -1) && (errno != EAGAIN)) {
          fprintf(stderr, "%s: splice() failed.\n",
                  strerror_r(errno, errbuf, sizeof(errbuf)));
          return 1;
        }
      } else if (events[n].data.fd == PIPE_READER) {
        // The read size of the pipe is ready for data.
        const ssize_t xferred =
            splice(PIPE_READER, nullptr, DEST_FILE, nullptr, ONE_MEG,
                   SPLICE_F_MOVE | SPLICE_F_NONBLOCK);

        // Check for unrecoverable error.
        if (xferred == -1) {
          if (errno != EAGAIN) {
            fprintf(stderr, "%s: splice() failed.\n",
                    strerror_r(errno, errbuf, sizeof(errbuf)));
            return 1;
          }
        } else {
          // Record that we transferred data to the output file.
          bytes_written_to_file += xferred;
        }

      } else {
        // Totally unexpected error, most likely impossible.
        fprintf(stderr, "unexpected file descriptor returned from epoll: %d\n",
                events[n].data.fd);
        return 1;
      }
    }
  }

  return 0;
}
