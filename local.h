void parse_list_options(const char *params, int *all, int *list);
void init_directory();
char *get_current_directory();
int change_directory(const char *path, int verbose);
char *get_current_directory_list(int all, int list, unsigned int *length, char *error);
FILE *fopen_current_dir(const char *path, const char *mode);
int check_avaiable_space(unsigned int size);
