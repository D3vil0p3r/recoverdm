void * mymalloc(size_t size, char *what);
void * myrealloc(void *oldp, size_t newsize, char *what);
off64_t get_filesize(char *filename);
int copy_block(int fd_in, int fd_out, off64_t block_size);
void myseek(int fd, off64_t offset);
