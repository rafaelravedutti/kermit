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
#include <local.h>
#include <protocol.h>

/* Comando ls do lado do cliente */
void kermit_client_ls(int socket, const char *params, unsigned int params_length) {
  struct kermit_packet answer;
  unsigned char type, length;

  /* Envia um pacote para o servidor com o comando ls e aguarda resposta */
  send_kermit_packet(socket, params, params_length, PACKET_TYPE_LS);
  recv_kermit_packet(socket, &answer, 0);

  /* Se a resposta não veio com erro */
  if(!kermit_error(&answer)) {

    /* Obtêm o tipo e o comprimento dos dados da resposta */
    type = get_kermit_packet_type(&answer);
    length = get_kermit_packet_length(&answer);

    /* Segue imprindo a resposta até encontrar um pacote indicando o fim da mensagem */
    while(type != PACKET_TYPE_END) {
      if(type == PACKET_TYPE_SHOW) {
        answer.packet_data_crc[length] = '\0';
        fprintf(stdout, "%s", answer.packet_data_crc);
      }

      /* Aguarda próximo pacote/resposta */
      recv_kermit_packet(socket, &answer, 0);
      type = get_kermit_packet_type(&answer);
    }
  }
}

/* Comando cd do lado do cliente */
void kermit_client_cd(int socket, const char *params, unsigned int params_length) {
  struct kermit_packet answer;

  /* Envia pacote para o servidor com o comando cd e aguarda resposta */
  send_kermit_packet(socket, params, params_length, PACKET_TYPE_CD);
  recv_kermit_packet(socket, &answer, 0);

  /* Se não ocorreu nenhum erro, então o comando foi bem sucedido, caso contrário exibe o erro */
  if(!kermit_error(&answer)) {
    debug_kermit_packet(&answer, PACKET_TYPE_OK);
  }
}

/* Comando put do lado do cliente */
void kermit_client_put(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  char buffer[MAX_PACKET_DATA];
  struct kermit_packet answer;
  unsigned long int filesize;

  /* Abre arquivo que será enviado em modo de leitura */
  if((fp = fopen_current_dir(params, "rb")) == NULL) {
    perror("fopen");
    return;
  }

  /* Obtêm o tamanho do arquivo */
  fseek(fp, 0, SEEK_END);
  filesize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  /* Envia pacote para o servidor com o comando put e aguarda resposta */
  send_kermit_packet(socket, params, params_length, PACKET_TYPE_PUT);
  recv_kermit_packet(socket, &answer, 0);

  /* Se a resposta não indica nenhum erro */
  if(!kermit_error(&answer)) {
    debug_kermit_packet(&answer, PACKET_TYPE_OK);

    /* Envia pacote para o servidor informando o tamanho do arquivo */
    send_kermit_packet(socket, (char *) &filesize, sizeof(unsigned long int), PACKET_TYPE_FILESIZE);
    recv_kermit_packet(socket, &answer, 0);

    /* Se a resposta não indica nenhum erro */
    if(!kermit_error(&answer)) {
      debug_kermit_packet(&answer, PACKET_TYPE_OK);

      /* Enquanto houver maior quantidade de dados que o buffer suporta, vai segmentando e transferindo o arquivo */
      while(filesize > MAX_PACKET_DATA) {
        fread(buffer, sizeof(char), MAX_PACKET_DATA, fp);
        send_kermit_packet(socket, buffer, MAX_PACKET_DATA, PACKET_TYPE_DATA);
        filesize -= MAX_PACKET_DATA;
      }

      /* Envia o pacote final com o conteúdo do arquivo e a mensagem indicando o fim da transmissão de dados */
      fread(buffer, sizeof(char), filesize, fp);
      send_kermit_packet(socket, buffer, filesize, PACKET_TYPE_DATA);
      send_kermit_packet(socket, "", 0, PACKET_TYPE_END);
    }
  }

  fclose(fp);
}

/* Comando get no lado do cliente */
void kermit_client_get(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  struct kermit_packet answer;
  unsigned long int filesize;
  unsigned char type;
  char error;

  /* Envia pacote com o comando get para o servidor e aguarda resposta */
  send_kermit_packet(socket, params, params_length, PACKET_TYPE_GET);
  recv_kermit_packet(socket, &answer, 0);

  /* Se a resposta não indica nenhum erro do lado do servidor */
  if(!kermit_error(&answer)) {
    debug_kermit_packet(&answer, PACKET_TYPE_FILESIZE);

    /* Obtêm o tamanho do arquivo a ser baixado na resposta */
    filesize = *((unsigned long int *) answer.packet_data_crc);

    /* Verifica se há espaço disponível para armazenar o arquivo */
    error = check_avaiable_space(filesize);

    /* Caso não haja, envia mensagem de erro para o servidor */
    if(error != KERMIT_ERROR_SUCCESS) {
      fprintf(stdout, "Não há espaço disponível para salvar este arquivo!\n");
      send_kermit_packet(socket, &error, sizeof(char), PACKET_TYPE_ERROR);
    } else {

      /* Abre arquivo que será baixado em modo de escrita */
      if((fp = fopen_current_dir(params, "wb")) == NULL) {
        perror("fopen");
        return;
      }

      /* Envia pacote para o servidor indicando que está pronto para receber os dados do arquivo */
      send_kermit_packet(socket, "", 0, PACKET_TYPE_OK);

      /* Vai recebendo as respostas segmentadas de acordo com o tamanho do buffer */
      recv_kermit_packet(socket, &answer, 0);
      type = get_kermit_packet_type(&answer);

      /* Enquanto não recebe mensagem indicando o fim dos dados, vai escrevendo o conteúdo no arquivo */
      while(type != PACKET_TYPE_END) {
        if(type == PACKET_TYPE_DATA) {
          fwrite(answer.packet_data_crc, sizeof(char), get_kermit_packet_length(&answer), fp);
        }

        recv_kermit_packet(socket, &answer, 0);
        type = get_kermit_packet_type(&answer);
      }

      /* Ao receber a mensagem de fim, fecha o arquivo */
      fclose(fp);
    }
  }
}

/* Interpreta e executa um comando digitado pelo cliente */
void exec_command(int socket, const char *command) {
  char *cmd, *params, *dup, *dirlist;
  char error;
  int all, list;
  unsigned int length, params_length, list_len;

  /* Se o comando é vazio não faz nada */
  if(command[0] == '\0' || command[0] == '\n') {
    return;
  }

  /* Obtêm o comprimento do comando e o duplica */
  length = strlen(command);
  dup = strdup(command);


  /* Caso ocorra erro de memória na duplicação, reporta o problema */
  if(dup == NULL) {
    fprintf(stderr, "exec_command(): Falha ao alocar memória para dup!\n");
    return;
  }

  /* Define fim do comando substituindo '\n' por '\0' no final */
  if(dup[length - 1] == '\n') {
    dup[--length] = '\0';
  }

  /* Obtêm o comando digitado (string até o primeiro espaço) */
  cmd = strtok(dup, " ");

  /* Se o comando não é nulo */
  if(cmd != NULL) {
    /* Obtêm os parâmetros digitados */
    params = strtok(NULL, " ");

    /* Obtêm o comprimento dos parâmetros (0 se forem nulos) */
    if(params != NULL) {
      params_length = strlen(params);
    } else {
      params_length = 0;
    }

    /* Comando local ls */
    if(strcmp(cmd, "lls") == 0) {
      /* Faz um parse nos argumentos (-l e -a) */
      parse_list_options(params, &all, &list);

      /* Obtêm a lista de arquivos do diretório corrente */
      dirlist = get_current_directory_list(all, list, &list_len, &error);

      /* Se o usuário não tiver permissão, reporta o erro */
      if(error == KERMIT_ERROR_PERM) {
        fprintf(stdout, "Permissão negada!\n");
      }

      /* Imprime a lista e libera a região de memória ocupada pela mesma */
      if(dirlist != NULL) {
        fprintf(stdout, "%s", dirlist);
        free(dirlist);
      }

    /* Comando local cd */
    } else if(strcmp(cmd, "lcd") == 0) {
      /* Se o parâmetro não é nulo (diretório a ser acessado), então o acessa */
      if(params != NULL) {
        change_directory(params, 1);
      /* Caso contrário, reporta erro de sintaxe */
      } else {
        fprintf(stdout, "Sintaxe inválida, use: lcd diretório\n");
      }

    /* Comando ls remoto */
    } else if(strcmp(cmd, "ls") == 0) {
      /* Executa ls no lado do cliente */
      kermit_client_ls(socket, params, params_length);

    /* Comando cd remoto */
    } else if(strcmp(cmd, "cd") == 0) {
      /* Se o parâmetro não é nulo, então executa cd no lado do cliente */
      if(params != NULL) {
        kermit_client_cd(socket, params, params_length);
      /* Caso contrário, reporta erro de sintaxe */
      } else {
        fprintf(stdout, "Sintaxe inválida, use: cd diretório\n");
      }

    /* Comando put */
    } else if(strcmp(cmd, "put") == 0) {
      /* Se o parâmetro não é nulo, então executa put no lado do cliente */
      if(params != NULL) {
        kermit_client_put(socket, params, params_length);
      /* Caso contrário, reporta erro de sintaxe */
      } else {
        fprintf(stdout, "Sintaxe inválida, use: put arquivo\n");
      }

    /* Comando get */
    } else if(strcmp(cmd, "get") == 0) {
      /* Se o parâmetro não é nulo, então executa get no lado do cliente */
      if(params != NULL) {
        kermit_client_get(socket, params, params_length);
      /* Caso contrário, reporta erro de sintaxe */
      } else {
        fprintf(stdout, "Sintaxe inválida, use: get arquivo\n");
      }

    /* Caso contrário, o comando não existe e informa os comandos ao usuário */
    } else {
      fprintf(stdout, "Comando inválido, use:\nlls [-la]\tls [-la]\nlcd diretório\tcd diretório\nget arquivo\tput arquivo\n");
    }
  }

  /* Libera memória de comando duplicado */
  free(dup);
}
