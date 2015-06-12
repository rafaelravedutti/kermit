#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <protocol.h>

static char current_dir[PATH_MAX], previous_dir[PATH_MAX];

void parse_list_options(const char *params, int *all, int *list) {
  char *p;

  *all = *list = 0;

  if(params != NULL && params[0] == '-') {
    for(p = (char *) params + 1; *p != '\0' && *p != ' '; ++p) {
      if(*p == 'a') {
        *all = 1;
      } else if(*p == 'l') {
        *list = 1;
      }
    }
  }
}

void init_directory() {
  getcwd(current_dir, sizeof(current_dir));
  previous_dir[0] = '\0';
}

char *get_current_directory() {
  return current_dir;
}

char get_format_digit(long int format) {
  switch(format) {
    case S_IFBLK:
      return 'b';
    case S_IFCHR:
      return 'c';
    case S_IFDIR:
      return 'd';
    case S_IFIFO:
      return 'p';
    case S_IFLNK:
      return 'l';
    case S_IFREG:
      return '-';
    case S_IFSOCK:
      return 's';
  }

  return '?';
}

void formatperm(char dest[], long int mode) {
  dest[0] = (mode & 0400) ? 'r' : '-';
  dest[1] = (mode & 0200) ? 'w' : '-';
  dest[2] = (mode & 0100) ? 'x' : '-';
  dest[3] = (mode & 0040) ? 'r' : '-';
  dest[4] = (mode & 0020) ? 'w' : '-';
  dest[5] = (mode & 0010) ? 'x' : '-';
  dest[6] = (mode & 0004) ? 'r' : '-';
  dest[7] = (mode & 0002) ? 'w' : '-';
  dest[8] = (mode & 0001) ? 'x' : '-';
  dest[9] = '\0';
}

unsigned int get_digits(off_t num) {
  unsigned int result = 1;

  while(num >= 10) {
    ++result;
    num /= 10;
  }

  return result;
}

int print_file_info(char *dest, const char *filename, const char *abs_path, unsigned int size_digits) {
    struct stat filestat;
    struct passwd *ownerpasswd;
    struct group *groupentry;
    char modtime[32], fileperm[10], filetype;
    unsigned int i;

    if(stat(abs_path, &filestat) == -1) {
      perror("stat");
      return 0;
    }

    filetype = get_format_digit(filestat.st_mode & S_IFMT);
    ownerpasswd = getpwuid(filestat.st_uid);
    groupentry = getgrgid(filestat.st_gid);
    formatperm(fileperm, filestat.st_mode);
    strftime(modtime, sizeof(modtime), "%b %d %H:%M", localtime(&filestat.st_mtime));

    sprintf(dest, "%c%s %ld %s %s", filetype, fileperm, filestat.st_nlink, ownerpasswd->pw_name, groupentry->gr_name);

    for(i = get_digits(filestat.st_size); i < size_digits; ++i) {
      sprintf(dest, "%s ", dest);
    }

    return sprintf(dest, "%s %ld %s %s\n", dest, filestat.st_size, modtime, filename);
}

int change_directory(const char *path, int verbose) {
  char new_dir[PATH_MAX];
  char *path_dir, *dir;
  unsigned int dir_length;

  if(strcmp(path, "-") == 0) {
    strncpy(new_dir, previous_dir, sizeof(new_dir));
    strncpy(previous_dir, current_dir, sizeof(previous_dir));
    strncpy(current_dir, new_dir, sizeof(current_dir));
    return KERMIT_ERROR_SUCCESS;
  }

  path_dir = strdup(path);
  if(path_dir == NULL) {
    fprintf(stderr, "change_directory(): Falha ao alocar memória para path_dir!\n");
    return -1;
  }

  if(path_dir[0] != '/') {
    strncpy(new_dir, current_dir, sizeof(new_dir));
  } else {
    new_dir[0] = '\0';
  }

  dir_length = strlen(new_dir);

  dir = strtok(path_dir, "/");
  while(dir != NULL) {
    if(strlen(dir) > 0 && strcmp(dir, ".") != 0) {
      if(strcmp(dir, "..") == 0) {
        while(dir_length > 0 && new_dir[dir_length] != '/') {
          new_dir[dir_length--] = '\0';
        }

        new_dir[dir_length--] = '\0';
      } else {
        dir_length += strlen(dir) + 1;

        if(dir_length > PATH_MAX) {
          fprintf(stderr, "%s/%s: Nome do diretório ultrapassou o tamanho máximo de largura do sistema!\n", current_dir, dir);
          return -2;
        }

        sprintf(new_dir, "%s/%s", new_dir, dir);

        if(access(new_dir, F_OK) == -1) {
          if(verbose != 0) {
            fprintf(stdout, "%s/%s: Diretório não encontrado!\n", current_dir, dir);
          }

          return KERMIT_ERROR_DIR_NFOUND;
        }

        if(access(new_dir, X_OK) == -1) {
          if(verbose != 0) {
            fprintf(stdout, "%s/%s: Você não tem permissão para acessar este diretório!\n", current_dir, dir);
          }

          return KERMIT_ERROR_PERM;
        }
      }
    }

    dir = strtok(NULL, "/");
  }

  strncpy(previous_dir, current_dir, sizeof(previous_dir));
  strncpy(current_dir, new_dir, sizeof(current_dir));
  free(path_dir);
  return KERMIT_ERROR_SUCCESS;
}

char *get_current_directory_list(int all, int list, unsigned int *length, char *error) {
  DIR *directory;
  struct dirent *ent;
  struct stat filestat;
  char absolute_path[PATH_MAX];
  char *list_base, *list_ptr;
  off_t max_size = 0;
  unsigned int size_digits, alloc_size;

  alloc_size = 0;
  *error = KERMIT_ERROR_SUCCESS;

  if(access(current_dir, R_OK) == -1) {
    *error = KERMIT_ERROR_PERM;
    return NULL;
  }

  directory = opendir(current_dir);
  if(directory != NULL) {
    while((ent = readdir(directory)) != NULL) {
      if(all != 0 || ent->d_name[0] != '.') {
        if(list != 0) {
          sprintf(absolute_path, "%s/%s", current_dir, ent->d_name);
          if(stat(absolute_path, &filestat) == -1) {
            char staterr[64];
            sprintf(staterr, "stat(\"%s\")", ent->d_name);
            perror(staterr);
            return NULL;
          }

          if(max_size < filestat.st_size) {
            max_size = filestat.st_size;
          }

          alloc_size += strlen(ent->d_name) + 64;
        } else {
          alloc_size += strlen(ent->d_name) + 2;
        }
      }
    }

    closedir(directory);
  }

  size_digits = get_digits(max_size);
  list_base = list_ptr = (char *) malloc(alloc_size + 5);

  if(list_base != NULL) {
    directory = opendir(current_dir);
    if(directory != NULL) {
      while((ent = readdir(directory)) != NULL) {
        if(all != 0 || ent->d_name[0] != '.') {
          if(list != 0) {
            sprintf(absolute_path, "%s/%s", current_dir, ent->d_name);
            list_ptr += print_file_info(list_ptr, ent->d_name, absolute_path, size_digits);
          } else {
            list_ptr += sprintf(list_ptr, "%s  ", ent->d_name);
          }
        }
      }

      closedir(directory);
    }

    if(list == 0) {
      *list_ptr++ = '\n';
    }

    *list_ptr++ = '\0';
    *length = list_ptr - list_base;
  } else {
    *length = 0;
    fprintf(stderr, "get_current_directory_list(): Erro ao alocar memória para list!\n");
  }

  return list_base;
}

FILE *fopen_current_dir(const char *path, const char *mode) {
  char absolute_path[PATH_MAX];

  if(strlen(path) + strlen(current_dir) + 1 > PATH_MAX) {
    fprintf(stderr, "fopen_current_dir(): Nome do arquivo ultrapassou o tamanho máximo de largura do sistema!\n");
    return NULL;
  }

  sprintf(absolute_path, "%s/%s", current_dir, path);
  return fopen(absolute_path, mode);
}

int check_avaiable_space(unsigned int size) {
  struct statvfs stat;

  if(statvfs(current_dir, &stat) == -1) {
    perror("statvfs");
    return -1;
  }

  if(size > stat.f_bavail * stat.f_bsize) {
    return KERMIT_ERROR_FULL_DISK;
  }

  return KERMIT_ERROR_SUCCESS;
}
