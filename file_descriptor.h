#ifndef FILE_DESCRIPTOR_H_
#define FILE_DESCRIPTOR_H_

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// The FileDescriptor class is designed to take ownership of a UN*X file
// descriptor (file, socket, etc.) It provides access to the raw descriptor
// via the Get() function, and allows the caller to take explicit ownership
// via a call to the Dismiss() function. (Ownership can be restored to the
// FileDescriptor instance via a call to Dismiss(), passing 'false'.
//
// The class also supports several methods that are common to most, if not
// all file descriptors (such as setting the nonblocking flag.) These 
// functions typically return a boolean 'true' indicating success or a
// 'false' indicating failure; the caller should check errno to learn the
// reason in case of a failure.

class FileDescriptor {
 public:
	// Default constructor.
	FileDescriptor() : fd_(-1), close_(false) {}

	// Construct and take ownership of integer file descriptor.
	FileDescriptor(int fd) : fd_(fd), close_(true) {}

	// Destructor.
	~FileDescriptor() {
		Close();
	}

	// Set the blocking mode on the descriptor.
	// Note: This is static so that it can be used on any descriptor,
	// regardless of whether that descriptor is owned by an instance.
	static bool SetBlockingMode(int fd, bool blocking) {
		// Shortcut for known bad files.
		if (fd == -1) {
			errno = EBADF;
			return false;
		}

		// Get the current flags.
		int flags = fcntl(fd, F_GETFL);
		if (flags == -1)  {
			return false;
		}

		if (blocking) {
			// Make the descriptor blocking by clearing the nonblocking flag.
			flags &= ~O_NONBLOCK;
		} else {
			// Make the descriptor nonblocking by setting the nonblocking flag.
			flags |= O_NONBLOCK;
		}

		// Reset flags.
		const int status = fcntl(fd, F_SETFL, flags);
		return (status == 0);
	}

	// Close the file referenced by the descriptor.
	int Close() {
		if ((!close_) || (fd_ < 0)) {
			// Descriptpr was never set, or was already closed.
			return 0;
		}
		const int status = close(fd_);
		if (status != 0) {
	    return status;
	  }
    fd_ = -1;
    close_ = false;
		return 0;
	}

	// By default, causes file not to be closed on destruction.
	// If 'false' is passed, will cancel a previous dismissal
	// unless the file descriptor is invalid already.
	bool Dismiss(bool dismiss = true) {
		if (dismiss && (fd_ < 0)) {
  		return false;
		}
		close_ = dismiss;
		return true;
	}

	// Return file descriptor.
	int Get() const { return fd_; }

	// Put the file object referenced by the descriptor into blocking mode.
	bool SetBlocking() {
		return SetBlockingMode(fd_, true);
	}

	// Put the file object referenced by the descriptor into nonblocking mode.
	bool SetNonblocking() {
		return SetBlockingMode(fd_, false);
	}

 private:
	FileDescriptor(const FileDescriptor&);
	FileDescriptor& operator=(const FileDescriptor&);

	int fd_;
	bool close_;
};

#endif  // FILE_DESCRIPTOR_H_
