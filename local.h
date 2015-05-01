int change_directory(const char *path, int verbose);
char *get_current_directory_list(int all, int list, unsigned int *length);
FILE *fopen_current_dir(const char *path, const char *mode);
