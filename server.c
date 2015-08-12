#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <local.h>
#include <protocol.h>

/* Executa o comando ls no lado do servidor */
void kermit_server_ls(int socket, const char *params, unsigned int params_length) {
  char *dirlist, *list_ptr;
  char error;
  double percent;
  int all, list;
  unsigned int list_send, list_len;

  fprintf(stdout, "LISTING current directory...\n");

  /* Realiza um parse das opções do ls (-a e -l) */
  parse_list_options(params, &all, &list);
  /* Obtêm a mensagem com a lista de arquivos do diretório corrente */
  dirlist = get_current_directory_list(all, list, &list_len, &error);
  list_send = list_len;

  fprintf(stdout, "List generated, sending it over network...\n");

  /* Se não houve erro ao processar a lista */
  if(error == KERMIT_ERROR_SUCCESS) {
    list_ptr = dirlist;

    /* Vai enviando a lista em blocos de tamanho MAX_PACKET_DATA */
    while(list_send > MAX_PACKET_DATA) {
      percent = ((double)(list_len - list_send) / (double) list_len) * 100.0;
      fprintf(stdout, "\rSending list block... (%.2f%%)", percent);
      send_kermit_packet(socket, list_ptr, MAX_PACKET_DATA, PACKET_TYPE_SHOW);
      list_ptr += MAX_PACKET_DATA;
      list_send -= MAX_PACKET_DATA;
    }

    /* Envia o último pacote de dados e uma mensagem indicando o final dos dados */
    send_kermit_packet(socket, list_ptr, list_send, PACKET_TYPE_SHOW);
    fprintf(stdout, "\rSending list block... Done!      \n");
    send_kermit_packet(socket, "", 0, PACKET_TYPE_END);
    fprintf(stdout, "LIST operation successfully done!\n");
  /* Caso tenha ocorrido erro durante o processamento da lista */
  } else {
    /* Envia um pacote contendo o código do erro para o cliente */
    fprintf(stdout, "LIST operation not possible, sending error over network... ");
    send_kermit_packet(socket, &error, sizeof(char), PACKET_TYPE_ERROR);
    fprintf(stdout, "Done!\n");
  }
}

/* Comando cd do lado do servidor */
void kermit_server_cd(int socket, const char *params, unsigned int params_length) {
  char error;

  fprintf(stdout, "CHANGING DIRECTORY to \"%s\"\n", params);

  /* Altera o diretório corrente para o especificado */
  error = change_directory(params, 0);

  /* Se não houve erro durante a mudança de diretório */
  if(error == KERMIT_ERROR_SUCCESS) {
    /* Envia um pacote indicando que a operação foi realizada com sucesso para o cliente */
    fprintf(stdout, "Directory now is \"%s\", sending OK message over network...\n", get_current_directory());
    send_kermit_packet(socket, "", 0, PACKET_TYPE_OK);
    fprintf(stdout, "CD operation successfully done!\n");
  /* Caso ocorra algum erro durante a mudança de diretório */
  } else {
    /* Envia um pacote contendo o código do erro para o cliente */
    fprintf(stdout, "CD operation not possible, sending error over network... ");
    send_kermit_packet(socket, &error, sizeof(char), PACKET_TYPE_ERROR);
    fprintf(stdout, "Done!\n");
  }
}

/* Comando put do lado do servidor */
void kermit_server_put(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  struct kermit_packet answer;
  char error;
  double percent;
  unsigned char type;
  unsigned long int filesize, wrote_data;

  fprintf(stdout, "PUTTING file \"%s\"...\n", params);

  /* Envia mensagen indicando a recepção do comando e espera resposta com o tamanho do arquivo */
  send_kermit_packet(socket, "", 0, PACKET_TYPE_OK);
  recv_kermit_packet(socket, &answer, 0);

  /* Por padrão, nenhum erro encontrado */
  error = KERMIT_ERROR_SUCCESS;

  /* Se a mensagem é do tipo que indica o tamanho de um arquivo */
  if(get_kermit_packet_type(&answer) == PACKET_TYPE_FILESIZE) {
    /* Obtêm o tamanho definido no payload da mensagem */
    filesize = *((unsigned long int *) answer.packet_data_crc);
    /* Verifica se há espaço suficiente para armazenar o arquivo */
    error = check_avaiable_space(filesize);

    /* Se ocorreu erro, então não há espaço disponível, informa o erro ao cliente */
    if(error != KERMIT_ERROR_SUCCESS) {
      fprintf(stdout, "No space avaiable on disk, sending error message... ");
      send_kermit_packet(socket, &error, sizeof(char), PACKET_TYPE_ERROR);
      fprintf(stdout, "Done!\n");
    /* Caso contrário, informa ao cliente para enviar o conteúdo do arquivo */
    } else {
      fprintf(stdout, "GOT file size \"%lu bytes\", sending OK message over network... ", filesize);
      send_kermit_packet(socket, "", 0, PACKET_TYPE_OK);
      fprintf(stdout, "Done!\n");
    }
  }

  /* Se não foi encontrado nenhum erro até agora */
  if(error == KERMIT_ERROR_SUCCESS) {
    /* Abre arquivo a ser transferido em modo de escrita */
    if((fp = fopen_current_dir(params, "wb")) == NULL) {
      perror("fopen");
      return;
    }

    /* Aguarda resposta com o conteúdo do arquivo */
    fprintf(stdout, "File opened for writing, waiting for answer (file block)...\n");
    recv_kermit_packet(socket, &answer, 0);
    fprintf(stdout, "Done!\n");

    /* Prossegue lendo as mensagens com o conteúdo em blocos e escrevendo no arquivo */
    wrote_data = 0;
    type = get_kermit_packet_type(&answer);

    while(type != PACKET_TYPE_END) {
      if(type == PACKET_TYPE_DATA) {
        /* Exibe status e acumula quantidade de bytes escritos */
        percent = ((double) wrote_data / (double) filesize) * 100.0;
        fprintf(stdout, "\rReceiving and writing file... (%.2f%%)", percent);
        fwrite(answer.packet_data_crc, sizeof(char), get_kermit_packet_length(&answer), fp);
        wrote_data += get_kermit_packet_length(&answer);
      }

      /* Recebe próximo bloco */
      recv_kermit_packet(socket, &answer, 0);
      type = get_kermit_packet_type(&answer);
    }

    /* Informa que o processo terminou e fecha o arquivo */
    fprintf(stdout, "\rReceiving and writing file... Done!      \n");
    fprintf(stdout, "PUT operation successfully done!\n");
    fclose(fp);
  }
}

/* Comando get do lado do servidor */
void kermit_server_get(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  char buffer[MAX_PACKET_DATA];
  struct kermit_packet answer;
  double percent;
  unsigned long int filesize, data_send;

  fprintf(stdout, "GETTING file \"%s\"...\n", params);

  /* Abre o arquivo a ser enviado em modo leitura */
  if((fp = fopen_current_dir(params, "rb")) == NULL) {
    perror("fopen");
    return;
  }

  /* Obtêm o tamanho do arquivo a ser enviado */
  fseek(fp, 0, SEEK_END);
  data_send = filesize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  /* Envia o tamanho do arquivo para o cliente e aguarda resposta */
  fprintf(stdout, "File opened for reading (File size: \"%lu bytes\")\n", filesize);
  fprintf(stdout, "Sending file size over network and waiting for answer...\n");
  send_kermit_packet(socket, (char *) &filesize, sizeof(unsigned long int), PACKET_TYPE_FILESIZE);
  recv_kermit_packet(socket, &answer, 0);

  /* Se não houve erro reportado pelo cliente */
  if(get_kermit_packet_type(&answer) == PACKET_TYPE_OK) {
    /* Vai enviando o arquivo em blocos de MAX_PACKET_DATA */
    fprintf(stdout, "GOT OK message, transferring data...\n");
    while(data_send > MAX_PACKET_DATA) {
      /* Reporta status e lê arquivo para enviar o próximo bloco */
      percent = ((double)(filesize - data_send) / (double) filesize) * 100.0;
      fprintf(stdout, "\rReading and sending file... (%.2f%%)", percent);
      fread(buffer, sizeof(char), MAX_PACKET_DATA, fp);
      send_kermit_packet(socket, buffer, MAX_PACKET_DATA, PACKET_TYPE_DATA);
      data_send -= MAX_PACKET_DATA;
    }

    /* Envia o último bloco e uma mensagem indicando o final da transmissão */
    fread(buffer, sizeof(char), data_send, fp);
    send_kermit_packet(socket, buffer, data_send, PACKET_TYPE_DATA);
    fprintf(stdout, "\rReading and sending file... Done!      \n");
    send_kermit_packet(socket, "", 0, PACKET_TYPE_END);
    fprintf(stdout, "GET operation successfully done!\n");
  /* Se ocorreu erro reportado pelo cliente, informa que ocorreu erro */
  } else {
    fprintf(stdout, "Error on client side!\n");
  }

  /* Fecha arquivo */
  fclose(fp);
}

/* Função de escuta do servidor */
void server_listen(int socket) {
  char params[MAX_PACKET_DATA];
  struct kermit_packet packet;
  unsigned char type, length;

  fprintf(stdout, "Server started and listening...\n");

  /* Aguarda por comandos do cliente (sem timeout) */
  while(!recv_kermit_packet(socket, &packet, KERMIT_NO_TIMEOUT)) {
    type = get_kermit_packet_type(&packet);
    length = get_kermit_packet_length(&packet);

    /* Copia o payload do pacote para os parâmetros */
    memcpy(params, packet.packet_data_crc, length);
    params[length] = '\0';

    /* Executa ação dependendo do tipo do pacote */
    if(type == PACKET_TYPE_LS) {
      kermit_server_ls(socket, params, length);
    } else if(type == PACKET_TYPE_CD) {
      kermit_server_cd(socket, params, length);
    } else if(type == PACKET_TYPE_PUT) {
      kermit_server_put(socket, params, length);
    } else if(type == PACKET_TYPE_GET) {
      kermit_server_get(socket, params, length);
    }
  }
}
