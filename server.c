#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <local.h>
#include <protocol.h>

void kermit_server_ls(int socket, const char *params, unsigned int params_length) {
  struct kermit_packet answer;
  char *dirlist, *list_ptr;
  char error;
  double percent;
  int all, list;
  unsigned int list_send, list_len;

  fprintf(stdout, "LISTING current directory...\n");

  parse_list_options(params, &all, &list);
  dirlist = get_current_directory_list(all, list, &list_len, &error);
  list_send = list_len;

  fprintf(stdout, "List generated, sending it over network...\n");

  if(error == KERMIT_ERROR_SUCCESS) {
    list_ptr = dirlist;

    while(list_send > MAX_PACKET_DATA) {
      percent = ((double)(list_len - list_send) / (double) list_len) * 100.0;
      fprintf(stdout, "\rSending list block... (%.2f%%)", percent);
      send_kermit_packet(socket, list_ptr, MAX_PACKET_DATA, PACKET_TYPE_SHOW, &answer);
      list_ptr += MAX_PACKET_DATA;
      list_send -= MAX_PACKET_DATA;
    }

    send_kermit_packet(socket, list_ptr, list_send, PACKET_TYPE_SHOW, &answer);
    fprintf(stdout, "\rSending list block... Done!      \n");
    send_kermit_packet(socket, "", 0, PACKET_TYPE_END, &answer);
    fprintf(stdout, "LIST operation successfully done!\n");
  } else {
    fprintf(stdout, "LIST operation not possible, sending error over network... ");
    send_kermit_packet(socket, &error, sizeof(char), PACKET_TYPE_SHOW, &answer);
    fprintf(stdout, "Done!\n");
  }
}

void kermit_server_cd(int socket, const char *params, unsigned int params_length) {
  char error;

  fprintf(stdout, "CHANGING DIRECTORY to \"%s\"\n", params);
  error = change_directory(params, 0);

  if(error == KERMIT_ERROR_SUCCESS) {
    fprintf(stdout, "Directory now is \"%s\", sending OK message over network...\n", get_current_directory());
    send_kermit_packet(socket, "", 0, PACKET_TYPE_OK, NULL);
    fprintf(stdout, "CD operation successfully done!\n");
  } else {
    fprintf(stdout, "CD operation not possible, sending error over network... ");
    send_kermit_packet(socket, &error, sizeof(char), PACKET_TYPE_ERROR, NULL);
    fprintf(stdout, "Done!\n");
  }
}

void kermit_server_put(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  struct kermit_packet answer;
  char error;
  double percent;
  unsigned char type;
  unsigned long int filesize, wrote_data;

  fprintf(stdout, "PUTTING file \"%s\"...\n", params);

  send_kermit_packet(socket, "", 0, PACKET_TYPE_OK, NULL);
  recv_kermit_packet(socket, &answer, 0);

  error = KERMIT_ERROR_SUCCESS;

  if(get_kermit_packet_type(&answer) == PACKET_TYPE_FILESIZE) {
    filesize = *((unsigned long int *) answer.packet_data_crc);
    error = check_avaiable_space(filesize);

    if(error != KERMIT_ERROR_SUCCESS) {
      fprintf(stdout, "No space avaiable on disk, sending error message... ");
      send_kermit_packet(socket, &error, sizeof(char), PACKET_TYPE_ERROR, &answer);
      fprintf(stdout, "Done!\n");
    } else {
      fprintf(stdout, "GOT file size \"%lu bytes\", sending OK message over network... ", filesize);
      send_kermit_packet(socket, "", 0, PACKET_TYPE_OK, NULL);
      fprintf(stdout, "Done!\n");
    }
  }

  if(error == KERMIT_ERROR_SUCCESS) {
    if((fp = fopen_current_dir(params, "wb")) == NULL) {
      perror("fopen");
      return;
    }

    fprintf(stdout, "File opened for writing, waiting for answer (file block)...\n");
    recv_kermit_packet(socket, &answer, 0);
    fprintf(stdout, "GOT answer, sending acknowledgement... ");
    send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
    fprintf(stdout, "Done!\n");

    wrote_data = 0;
    type = get_kermit_packet_type(&answer);

    while(type != PACKET_TYPE_END) {
      if(type == PACKET_TYPE_DATA) {
        percent = ((double) wrote_data / (double) filesize) * 100.0;
        fprintf(stdout, "\rReceiving and writing file... (%.2f%%)", percent);
        fwrite(answer.packet_data_crc, sizeof(char), get_kermit_packet_length(&answer), fp);
        wrote_data += get_kermit_packet_length(&answer);
      }

      send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
      recv_kermit_packet(socket, &answer, 0);
      type = get_kermit_packet_type(&answer);
    }

    fprintf(stdout, "\rReceiving and writing file... Done!      \n");
    send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
    fprintf(stdout, "PUT operation successfully done!\n");
    fclose(fp);
  }
}

void kermit_server_get(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  char buffer[MAX_PACKET_DATA];
  struct kermit_packet answer;
  double percent;
  unsigned long int filesize, data_send;

  fprintf(stdout, "GETTING file \"%s\"...\n", params);

  if((fp = fopen_current_dir(params, "rb")) == NULL) {
    perror("fopen");
    return;
  }

  fseek(fp, 0, SEEK_END);
  data_send = filesize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  fprintf(stdout, "File opened for reading (File size: \"%lu bytes\")\n", filesize);
  fprintf(stdout, "Sending file size over network and waiting for answer...\n");
  send_kermit_packet(socket, (char *) &filesize, sizeof(unsigned long int), PACKET_TYPE_FILESIZE, &answer);

  if(get_kermit_packet_type(&answer) == PACKET_TYPE_OK) {
    fprintf(stdout, "GOT OK message, transferring data...\n");
    while(data_send > MAX_PACKET_DATA) {
      percent = ((double)(filesize - data_send) / (double) filesize) * 100.0;
      fprintf(stdout, "\rReading and sending file... (%.2f%%)", percent);
      fread(buffer, sizeof(char), MAX_PACKET_DATA, fp);
      send_kermit_packet(socket, buffer, MAX_PACKET_DATA, PACKET_TYPE_DATA, &answer);
      data_send -= MAX_PACKET_DATA;
    }

    fread(buffer, sizeof(char), data_send, fp);
    send_kermit_packet(socket, buffer, data_send, PACKET_TYPE_DATA, &answer);
    fprintf(stdout, "\rReading and sending file... Done!      \n");
    send_kermit_packet(socket, "", 0, PACKET_TYPE_END, &answer);
    fprintf(stdout, "GET operation successfully done!\n");
  } else {
    fprintf(stdout, "Error on client side!\n");
  }

  fclose(fp);
}

void server_listen(int socket) {
  char params[MAX_PACKET_DATA];
  struct kermit_packet packet;
  unsigned char type, length;

  fprintf(stdout, "Server started and listening...\n");

  while(!recv_kermit_packet(socket, &packet, KERMIT_NO_TIMEOUT)) {
    type = get_kermit_packet_type(&packet);
    length = get_kermit_packet_length(&packet);

    memcpy(params, packet.packet_data_crc, length);
    params[length] = '\0';

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
