/* Realiza um parse das opções do ls (-a e -l) */
void parse_list_options(const char *params, int *all, int *list);
/* Inicializa diretórios atual e anterior */
void init_directory();
/* Obtêm o diretório atual */
char *get_current_directory();
/* Altera o diretório corrente */
int change_directory(const char *path, int verbose);
/* Obtêm a lista de arquivos e diretórios no diretório corrente */
char *get_current_directory_list(int all, int list, unsigned int *length, char *error);
/* Abre um arquivo a partir do diretório corrente */
FILE *fopen_current_dir(const char *path, const char *mode);
/* Verifica se há determinado espaço no diretório corrente */
int check_avaiable_space(unsigned int size);
