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
        fprintf(stdout, "%s", answer.packet_data_crc);
      }

      send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
      recv_kermit_packet(socket, &answer);
      type = get_kermit_packet_type(&answer);
    }

    send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
  }
}

void kermit_client_cd(int socket, const char *params, unsigned int params_length) {
  struct kermit_packet answer;

  send_kermit_packet(socket, params, params_length, PACKET_TYPE_CD, &answer);

  if(!kermit_error(&answer)) {
    debug_kermit_packet(&answer, PACKET_TYPE_OK);
  }
}

void kermit_client_put(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  char buffer[MAX_PACKET_DATA];
  struct kermit_packet answer;
  unsigned long int filesize;

  if((fp = fopen_current_dir(params, "rb")) == NULL) {
    perror("fopen");
    return;
  }

  fseek(fp, 0, SEEK_END);
  filesize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  send_kermit_packet(socket, params, params_length, PACKET_TYPE_PUT, &answer);

  if(!kermit_error(&answer)) {
    debug_kermit_packet(&answer, PACKET_TYPE_OK);
    send_kermit_packet(socket, (char *) &filesize, sizeof(unsigned long int), PACKET_TYPE_FILESIZE, &answer);

    if(!kermit_error(&answer)) {
      debug_kermit_packet(&answer, PACKET_TYPE_OK);

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
  struct kermit_packet answer;
  unsigned long int filesize;
  unsigned char type;

  if((fp = fopen_current_dir(params, "wb")) == NULL) {
    perror("fopen");
    return;
  }

  send_kermit_packet(socket, params, params_length, PACKET_TYPE_GET, &answer);

  if(!kermit_error(&answer)) {
    debug_kermit_packet(&answer, PACKET_TYPE_FILESIZE);
    filesize = *((unsigned long int *) answer.packet_data_crc);
    send_kermit_packet(socket, "", 0, PACKET_TYPE_OK, NULL);

    recv_kermit_packet(socket, &answer);
    send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
    type = get_kermit_packet_type(&answer);
    while(type != PACKET_TYPE_END) {
      if(type == PACKET_TYPE_DATA) {
        fwrite(answer.packet_data_crc, sizeof(char), get_kermit_packet_length(&answer), fp);
      }

      recv_kermit_packet(socket, &answer);
      send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
      type = get_kermit_packet_type(&answer);
    }

    send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
  }

  fclose(fp);
}

void exec_command(int socket, const char *command) {
  char *cmd, *params, *dup, *dirlist;
  char error;
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

    if(params != NULL) {
      params_length = strlen(params);
    } else {
      params_length = 0;
    }

    if(strcmp(cmd, "lls") == 0) {
      parse_list_options(params, &all, &list);

      dirlist = get_current_directory_list(all, list, &list_len, &error);

      if(error == KERMIT_ERROR_PERM) {
        fprintf(stdout, "Permissão negada!\n");
      }

      if(dirlist != NULL) {
        fprintf(stdout, "%s", dirlist);
        free(dirlist);
      }

    } else if(strcmp(cmd, "lcd") == 0) {
      if(params != NULL) {
        change_directory(params, 1);
      } else {
        fprintf(stdout, "Sintaxe inválida, use: lcd diretório\n");
      }

    } else if(strcmp(cmd, "ls") == 0) {
      kermit_client_ls(socket, params, params_length);

    } else if(strcmp(cmd, "cd") == 0) {
      if(params != NULL) {
        kermit_client_cd(socket, params, params_length);
      } else {
        fprintf(stdout, "Sintaxe inválida, use: cd diretório\n");
      }

    } else if(strcmp(cmd, "put") == 0) {
      if(params != NULL) {
        kermit_client_put(socket, params, params_length);
      } else {
        fprintf(stdout, "Sintaxe inválida, use: put arquivo\n");
      }

    } else if(strcmp(cmd, "get") == 0) {
      if(params != NULL) {
        kermit_client_get(socket, params, params_length);
      } else {
        fprintf(stdout, "Sintaxe inválida, use: get arquivo\n");
      }

    } else {
      fprintf(stdout, "Comando inválido, use:\nlls [-la]\tls [-la]\nlcd diretório\tcd diretório\nget arquivo\tput arquivo\n");
    }
  }

  free(dup);
}
