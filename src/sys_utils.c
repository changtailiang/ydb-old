#define _XOPEN_SOURCE 500 // pread

#include "ydb_internal.h"


#include <stdio.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// pread
#include <unistd.h>
#include <errno.h>

// stat
#include <sys/stat.h>


int safe_unlink(const char *old_path) {
	char new_path[256];
	snprintf(new_path, sizeof(new_path), "%s~", old_path);
	if(rename(old_path, new_path) != 0) {
		log_perror("rename(%s, %s)", old_path, new_path);
		return(-1);
	}
	return(0);
}

int unlink_with_history(const char *old_path, const char *new_base, int versions) {
	char new_path1[256];
	char new_path2[256];
	int i;
	for(i=(versions-1); i > 0; i--) {
		snprintf(new_path1, sizeof(new_path1), "%s%02i", new_base, i);
		snprintf(new_path2, sizeof(new_path2), "%s%02i", new_base, i+1);
		/* ignore errors */
		rename(new_path1, new_path2);
	}
	/* TODO: if file doesn't exits, just move on */
	snprintf(new_path1, sizeof(new_path1), "%s%02i", new_base, 1);
	if(rename(old_path, new_path1) != 0) {
		log_perror("rename(%s, %s)", old_path, new_path1);
		return(-1);
	}
	return(0);
}

int max_descriptors() {
	/* It's impossible to know the exact number.
	   We leave 128 descriptors to application. */
	struct rlimit rlimit;
	int r = getrlimit(RLIMIT_NOFILE, &rlimit);
	if(r != 0)
		return(1024 - 128); // guess the number
	int max_fd = rlimit.rlim_cur;
	return(max_fd - 128);
}

/* Write exactly `count` bytes to file. Returns -1 on error. */
int writeall(int fd, void *sbuf, size_t count) {
	char *buf = (char *)sbuf;
	size_t pos = 0;
	while(pos < count) {
		errno = 0;
		int r = write(fd, &buf[pos], count-pos);
		if(errno == EINTR)
			continue;
		if(r < 0) {
			log_perror("write()");
			return(r);
		}
		pos += r;
	}
	return(pos);
}

int preadall(int fd, void *sbuf, size_t count, size_t offset) {
	char *buf = (char *)sbuf;
	size_t pos = 0;
	while(pos < count) {
		errno = 0;
		ssize_t r = pread(fd, buf+pos, count-pos, offset+pos);
		if(r == 0) {
			if(errno == EINTR)
				continue;
			return(-1);
		}
		if(r < 0) {
			log_perror("pread()");
			return(-1);
		}
		pos += r;
	}
	return(pos);
}



int get_fd_size(int fd, u64 *size) {
	struct stat st;
	if(fstat(fd, &st) != 0){
		log_perror("stat()");
		return(-1);
	}
	if(size)
		*size = st.st_size;
	return(0);
}


