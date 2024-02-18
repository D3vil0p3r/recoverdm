ssize_t READ (int fd, char *whereto, size_t len);
ssize_t WRITE(int fd, char *whereto, size_t len);
ssize_t phantom_write(int fd, char *in, size_t nbytes);
