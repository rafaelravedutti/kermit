#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <local.h>
#include <protocol.h>

void kermit_client_ls(int socket, const char *params, unsigned int params_length) {
  struct kermit_packet answer;
  unsigned char type;

  send_kermit_packet(socket, params, params_length, PACKET_TYPE_LS, &answer);

  if(!kermit_error(&answer)) {
    type = get_kermit_packet_type(&answer);
    while(type != PACKET_TYPE_END) {
      if(type == PACKET_TYPE_SHOW) {
        printf("%s", answer.packet_data_crc);
      }

      recv_kermit_packet(socket, &answer, 1);
      type = get_kermit_packet_type(&answer);
    }
  }
}

void kermit_client_cd(int socket, const char *params, unsigned int params_length) {
  struct kermit_packet answer;
  unsigned char type;

  send_kermit_packet(socket, params, params_length, PACKET_TYPE_CD, &answer);

  if(!kermit_error(&answer)) {
    type = get_kermit_packet_type(&answer);
    if(type != PACKET_TYPE_OK) {
      printf("Invalid packet type, expected 0x%x, received 0x%x\n", PACKET_TYPE_OK, type);
    }
  }
}

void kermit_client_put(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  char buffer[MAX_PACKET_DATA];
  struct kermit_packet answer;
  unsigned long int filesize;
  unsigned char type;

  if((fp = fopen_current_dir(params, "rb")) == NULL) {
    perror("fopen");
    return;
  }

  fseek(fp, 0, SEEK_END);
  filesize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  send_kermit_packet(socket, params, params_length, PACKET_TYPE_PUT, &answer);

  if(!kermit_error(&answer)) {
    type = get_kermit_packet_type(&answer);
    if(type != PACKET_TYPE_OK) {
      printf("Invalid packet type, expected 0x%x, received 0x%x\n", PACKET_TYPE_OK, type);
    }

    send_kermit_packet(socket, (char *) &filesize, sizeof(unsigned long int), PACKET_TYPE_FILESIZE, &answer);

    if(!kermit_error(&answer)) {
      type = get_kermit_packet_type(&answer);
      if(type != PACKET_TYPE_OK) {
        printf("Invalid packet type, expected 0x%x, received 0x%x\n", PACKET_TYPE_OK, type);
      }

      while(filesize > MAX_PACKET_DATA) {
        fread(buffer, sizeof(char), MAX_PACKET_DATA, fp);
        send_kermit_packet(socket, buffer, MAX_PACKET_DATA, PACKET_TYPE_DATA, &answer);
        filesize -= MAX_PACKET_DATA;
      }

      fread(buffer, sizeof(char), filesize, fp);
      send_kermit_packet(socket, buffer, filesize, PACKET_TYPE_DATA, &answer);
      send_kermit_packet(socket, "", 0, PACKET_TYPE_END, &answer);
    }
  }

  fclose(fp);
}

void kermit_client_get(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  char buffer[MAX_PACKET_DATA];
  struct kermit_packet answer;
  unsigned long int filesize;
  unsigned char type;

  if((fp = fopen_current_dir(params, "wb")) == NULL) {
    perror("fopen");
    return;
  }

  send_kermit_packet(socket, params, params_length, PACKET_TYPE_GET, &answer);

  if(!kermit_error(&answer)) {
    type = get_kermit_packet_type(&answer);
    if(type != PACKET_TYPE_FILESIZE) {
      printf("Invalid packet type, expected 0x%x, received 0x%x\n", PACKET_TYPE_FILESIZE, type);
    }

    filesize = *((unsigned long int *) answer.packet_data_crc);
    send_kermit_packet(socket, "", 0, PACKET_TYPE_OK, NULL);

    type = get_kermit_packet_type(&answer);
    while(type != PACKET_TYPE_END) {
      if(type == PACKET_TYPE_DATA) {
        fwrite(answer.packet_data_crc, sizeof(char), get_kermit_packet_length(&answer), fp);
      }

      recv_kermit_packet(socket, &answer, 1);
      type = get_kermit_packet_type(&answer);
    }
  }

  fclose(fp);
}

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

void exec_command(int socket, const char *command) {
  char *cmd, *params, *dup, *dirlist;
  int all, list;
  unsigned int length, params_length, list_len;

  if(command[0] == '\0' || command[0] == '\n') {
    return;
  }

  length = strlen(command);
  dup = strdup(command);

  if(dup == NULL) {
    fprintf(stderr, "exec_command(): Falha ao alocar memória para dup!\n");
    return;
  }

  if(dup[length - 1] == '\n') {
    dup[--length] = '\0';
  }

  cmd = strtok(dup, " ");

  if(cmd != NULL) {
    params = strtok(NULL, " ");
    params_length = strlen(params);

    if(strcmp(cmd, "lls") == 0) {
      parse_list_options(params, &all, &list);

      dirlist = get_current_directory_list(all, list, &list_len);
      printf("%s", dirlist);
      free(dirlist);

    } else if(strcmp(cmd, "lcd") == 0) {
      if(params != NULL) {
        change_directory(params, 1);
      }

    } else if(strcmp(cmd, "ls") == 0) {
      kermit_client_ls(socket, params, params_length);
    } else if(strcmp(cmd, "cd") == 0) {
      kermit_client_cd(socket, params, params_length);
    } else if(strcmp(cmd, "put") == 0) {
      kermit_client_put(socket, params, params_length);
    } else if(strcmp(cmd, "get") == 0) {
      kermit_client_get(socket, params, params_length);
    } else {
      printf("Comando inválido, use:\nlls [-la]\tls[-la]\nlcd [diretório]\tcd [diretório]\nget [arquivo]\tput [arquivo]\n");
    }
  }

  free(dup);
}
