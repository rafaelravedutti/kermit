/*
 * kermit - File transfer program using Kermit-like protocol
 *
 * Copyright (C) 2015  Rafael Ravedutti Lucio Machado
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

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

/* Diretório atual e anterior */
static char current_dir[PATH_MAX], previous_dir[PATH_MAX];

/* Realiza um parse das opções do ls (-a e -l) */
void parse_list_options(const char *params, int *all, int *list) {
  char *p;

  /* Por padrão as opções não estão definidas */
  *all = *list = 0;

  /* Se existem parâmetros iniciando com '-' */
  if(params != NULL && params[0] == '-') {
    /* Percorre todos os parâmetros, se encontrar um 'a' define all e se encontrar um 'l' define list */
    for(p = (char *) params + 1; *p != '\0' && *p != ' '; ++p) {
      if(*p == 'a') {
        *all = 1;
      } else if(*p == 'l') {
        *list = 1;
      }
    }
  }
}

/* Inicializa diretórios atual e anterior */
void init_directory() {
  getcwd(current_dir, sizeof(current_dir));
  previous_dir[0] = '\0';
}

/* Obtêm o diretório atual */
char *get_current_directory() {
  return current_dir;
}

/* Obtêm o dígito referente ao formato de um inodo */
char get_format_digit(long int format) {
  switch(format) {
    /* Block device */
    case S_IFBLK:
      return 'b';
    /* Character device */
    case S_IFCHR:
      return 'c';
    /* Directory */
    case S_IFDIR:
      return 'd';
    /* FIFO/Pipe */
    case S_IFIFO:
      return 'p';
    /* Link */
    case S_IFLNK:
      return 'l';
    /* Regular file */
    case S_IFREG:
      return '-';
    /* Socket */
    case S_IFSOCK:
      return 's';
  }

  return '?';
}

/* Formata as permissões de um inodo em "rwxrwxrwx" */
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

/* Obtêm o número de digitos de um número */
unsigned int get_digits(off_t num) {
  unsigned int result = 1;

  while(num >= 10) {
    ++result;
    num /= 10;
  }

  return result;
}

/* Imprime as informações de um arquivo */
int print_file_info(char *dest, const char *filename, const char *abs_path, unsigned int size_digits) {
    struct stat filestat;
    struct passwd *ownerpasswd;
    struct group *groupentry;
    char modtime[32], fileperm[10], filetype;
    unsigned int i;

    /* Obtêm as informações do arquivo */
    if(stat(abs_path, &filestat) == -1) {
      perror("stat");
      return 0;
    }

    /* Tipo do arquivo */
    filetype = get_format_digit(filestat.st_mode & S_IFMT);
    /* Dono do arquivo */
    ownerpasswd = getpwuid(filestat.st_uid);
    /* Grupo do arquivo */
    groupentry = getgrgid(filestat.st_gid);
    /* Permissões no formato rwxrwxrwx */
    formatperm(fileperm, filestat.st_mode);
    /* Data de modificação do arquivo */
    strftime(modtime, sizeof(modtime), "%b %d %H:%M", localtime(&filestat.st_mtime));

    /* Formata o resultado com as informações */
    sprintf(dest, "%c%s %ld %s %s", filetype, fileperm, filestat.st_nlink, ownerpasswd->pw_name, groupentry->gr_name);

    /* Alinha de acordo com o número de dígitos do tamanho */
    for(i = get_digits(filestat.st_size); i < size_digits; ++i) {
      sprintf(dest, "%s ", dest);
    }

    /* Concatena com o resto dos dados */
    return sprintf(dest, "%s %ld %s %s\n", dest, filestat.st_size, modtime, filename);
}

/* Altera o diretório corrente */
int change_directory(const char *path, int verbose) {
  char new_dir[PATH_MAX];
  char *path_dir, *dir;
  unsigned int dir_length;

  /* Se o caminho especificado é '-', altera para o diretório anterior */
  if(strcmp(path, "-") == 0) {
    strncpy(new_dir, previous_dir, sizeof(new_dir));
    strncpy(previous_dir, current_dir, sizeof(previous_dir));
    strncpy(current_dir, new_dir, sizeof(current_dir));
    return KERMIT_ERROR_SUCCESS;
  }

  /* Duplica o caminho especificado em memória */
  path_dir = strdup(path);

  if(path_dir == NULL) {
    fprintf(stderr, "change_directory(): Falha ao alocar memória para path_dir!\n");
    return -1;
  }

  /* Verifica se o caminho especificado é absoluto ou relativo */
  if(path_dir[0] != '/') {
    strncpy(new_dir, current_dir, sizeof(new_dir));
  } else {
    new_dir[0] = '\0';
  }

  /* Obtêm o tamanho do novo diretório corrente */
  dir_length = strlen(new_dir);

  /* Vai percorrendo os diretórios do caminho */
  dir = strtok(path_dir, "/");
  while(dir != NULL) {
    /* Se o tamanho do próximo caminho é maior que 0 e diferente de '.' (diretório corrente) */
    if(strlen(dir) > 0 && strcmp(dir, ".") != 0) {
      /* Se o próximo caminho é '..' (diretório anterior), apaga o caminho a esquerda */
      if(strcmp(dir, "..") == 0) {
        while(dir_length > 0 && new_dir[dir_length] != '/') {
          new_dir[dir_length--] = '\0';
        }

        new_dir[dir_length--] = '\0';
      /* Caso contrário, concatena o caminho ao novo diretório corrente */
      } else {
        dir_length += strlen(dir) + 1;

        /* Verifica se o tamanho ainda é válido */
        if(dir_length > PATH_MAX) {
          fprintf(stderr, "%s/%s: Nome do diretório ultrapassou o tamanho máximo de largura do sistema!\n", current_dir, dir);
          return -2;
        }

        /* Faz a concatenação */
        sprintf(new_dir, "%s/%s", new_dir, dir);

        /* Se o diretório não existe, reporta o erro */
        if(access(new_dir, F_OK) == -1) {
          if(verbose != 0) {
            fprintf(stdout, "%s/%s: Diretório não encontrado!\n", current_dir, dir);
          }

          return KERMIT_ERROR_DIR_NFOUND;
        }

        /* Se o usuário não tem permissão de acesso, reporta o erro */
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

  /* Armazena o novo diretório corrente no atual e o atual no anterior */
  strncpy(previous_dir, current_dir, sizeof(previous_dir));
  strncpy(current_dir, new_dir, sizeof(current_dir));
  free(path_dir);
  return KERMIT_ERROR_SUCCESS;
}

/* Obtêm a lista de arquivos e diretórios no diretório corrente */
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

  /* Se o usuário não tem permissão de leitura no diretório corrente, retorna o erro */
  if(access(current_dir, R_OK) == -1) {
    *error = KERMIT_ERROR_PERM;
    return NULL;
  }

  /* Abre o diretório corrente */
  directory = opendir(current_dir);
  if(directory != NULL) {

    /* Percorre as entidades do diretório corrente */
    while((ent = readdir(directory)) != NULL) {
      if(all != 0 || ent->d_name[0] != '.') {
        /* Se é impressão em modo lista, obtêm as informações da entidade atual */
        if(list != 0) {
          sprintf(absolute_path, "%s/%s", current_dir, ent->d_name);
          if(stat(absolute_path, &filestat) == -1) {
            char staterr[64];
            sprintf(staterr, "stat(\"%s\")", ent->d_name);
            perror(staterr);
            return NULL;
          }

          /* Armazena o maior tamanho dentre as entidades em max_size */
          if(max_size < filestat.st_size) {
            max_size = filestat.st_size;
          }

          alloc_size += strlen(ent->d_name) + 64;
        } else {
          alloc_size += strlen(ent->d_name) + 2;
        }
      }
    }

    /* Fecha o diretório */
    closedir(directory);
  }

  /* Obtêm os dígitos do maior tamanho para alinhamento */
  size_digits = get_digits(max_size);
  /* Aloca a mensagem da lista */
  list_base = list_ptr = (char *) malloc(alloc_size + 5);

  if(list_base != NULL) {
    /* Abre o diretório corrente */
    directory = opendir(current_dir);
    if(directory != NULL) {
      /* Percorre as entidades do diretório */
      while((ent = readdir(directory)) != NULL) {
        if(all != 0 || ent->d_name[0] != '.') {
          /* Se a impressão é em modo lista, imprime as informações na mensagem da lista */
          if(list != 0) {
            sprintf(absolute_path, "%s/%s", current_dir, ent->d_name);
            list_ptr += print_file_info(list_ptr, ent->d_name, absolute_path, size_digits);
          /* Caso contrário, imprime apenas o nome da entidade */
          } else {
            list_ptr += sprintf(list_ptr, "%s  ", ent->d_name);
          }
        }
      }

      /* Fecha o diretório */
      closedir(directory);
    }

    /* Adiciona nova linha no final dos nomes das entidades se não for impressão em modo lista */
    if(list == 0) {
      *list_ptr++ = '\n';
    }

    /* Adiciona caractere de fim na mensagem da lista */
    *list_ptr++ = '\0';
    /* Calcula tamanho da mensagem */
    *length = list_ptr - list_base;
  } else {
    *length = 0;
    fprintf(stderr, "get_current_directory_list(): Erro ao alocar memória para list!\n");
  }

  return list_base;
}

/* Abre um arquivo a partir do diretório corrente */
FILE *fopen_current_dir(const char *path, const char *mode) {
  char absolute_path[PATH_MAX];

  /* Verifica se o tamanho do caminho inteiro do arquivo é válido */
  if(strlen(path) + strlen(current_dir) + 1 > PATH_MAX) {
    fprintf(stderr, "fopen_current_dir(): Nome do arquivo ultrapassou o tamanho máximo de largura do sistema!\n");
    return NULL;
  }

  /* Gera o caminho absoluto do arquivo e o abre */
  sprintf(absolute_path, "%s/%s", current_dir, path);
  return fopen(absolute_path, mode);
}

/* Verifica se há determinado espaço no diretório corrente */
int check_avaiable_space(unsigned int size) {
  struct statvfs stat;

  /* Obtêm status do sistema de arquivos virtual (VFS) */
  if(statvfs(current_dir, &stat) == -1) {
    perror("statvfs");
    return -1;
  }

  /* Se o tamanho ultrapassa o espaço disponível, retorna erro */
  if(size > stat.f_bavail * stat.f_bsize) {
    return KERMIT_ERROR_FULL_DISK;
  }

  return KERMIT_ERROR_SUCCESS;
}
