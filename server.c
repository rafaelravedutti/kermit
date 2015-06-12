#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <local.h>
#include <protocol.h>

void kermit_server_ls(int socket, const char *params, unsigned int params_length) {
  struct kermit_packet answer;
  char *dirlist, *list_ptr;
  char error;
  int all, list;
  unsigned int list_len;

  fprintf(stdout, "LISTING current directory...\n");

  parse_list_options(params, &all, &list);
  dirlist = get_current_directory_list(all, list, &list_len, &error);

  fprintf(stdout, "List generated, sending it over network...\n");

  if(error == KERMIT_ERROR_SUCCESS) {
    list_ptr = dirlist;

    while(list_len > MAX_PACKET_DATA) {
      fprintf(stdout, "Sending list block with %u bytes of data...", MAX_PACKET_DATA);
      send_kermit_packet(socket, list_ptr, MAX_PACKET_DATA, PACKET_TYPE_SHOW, &answer);
      fprintf(stdout, "Done!\n");
      list_ptr += MAX_PACKET_DATA;
      list_len -= MAX_PACKET_DATA;
    }

    fprintf(stdout, "Sending list block with %u bytes of data... ", list_len);
    send_kermit_packet(socket, list_ptr, list_len, PACKET_TYPE_SHOW, &answer);
    fprintf(stdout, "Done!\nSending END message...\n");
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
  unsigned long int filesize;
  unsigned char type;
  char error;

  fprintf(stdout, "PUTTING file \"%s\"...\n", params);
  if((fp = fopen_current_dir(params, "wb")) == NULL) {
    perror("fopen");
    return;
  }

  fprintf(stdout, "File opened for writing, sending OK message over network... ");
  send_kermit_packet(socket, "", 0, PACKET_TYPE_OK, NULL);
  fprintf(stdout, "Done!\nWaiting for answer (File size packet)...\n");
  recv_kermit_packet(socket, &answer);

  error = KERMIT_ERROR_SUCCESS;

  if(get_kermit_packet_type(&answer) == PACKET_TYPE_FILESIZE) {
    filesize = *((unsigned long int *) answer.packet_data_crc);
    error = check_avaiable_space(filesize);

    if(error != KERMIT_ERROR_SUCCESS) {
      fprintf(stdout, "No space avaiable on disk, sending error message... ");
      send_kermit_packet(socket, &error, sizeof(char), PACKET_TYPE_SHOW, &answer);
      fprintf(stdout, "Done!\n");
    } else {
      fprintf(stdout, "GOT file size \"%lu bytes\", sending OK message over network... ", filesize);
      send_kermit_packet(socket, "", 0, PACKET_TYPE_OK, NULL);
      fprintf(stdout, "Done!\n");
    }
  }

  if(error == KERMIT_ERROR_SUCCESS) {
    fprintf(stdout, "Waiting for answer (File block)...\n");
    recv_kermit_packet(socket, &answer);
    fprintf(stdout, "GOT answer, sending acknowledgement... ");
    send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
    fprintf(stdout, "Done!\n");
    type = get_kermit_packet_type(&answer);
    while(type != PACKET_TYPE_END) {
      if(type == PACKET_TYPE_DATA) {
        fprintf(stdout, "GOT file block with \"%d bytes\" of data! Writing... ", get_kermit_packet_length(&answer));
        fwrite(answer.packet_data_crc, sizeof(char), get_kermit_packet_length(&answer), fp);
        fprintf(stdout, "Done!\n");
      }

      fprintf(stdout, "Sending acknowledgement over network... ");
      send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
      fprintf(stdout, "Done!\nWaiting for answer (File block or END)...\n");
      recv_kermit_packet(socket, &answer);
      type = get_kermit_packet_type(&answer);
    }

    send_kermit_packet(socket, "", 0, PACKET_TYPE_ACK, NULL);
    fprintf(stdout, "PUT operation successfully done!\n");
  }

  fclose(fp);
}

void kermit_server_get(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  char buffer[MAX_PACKET_DATA];
  struct kermit_packet answer;
  unsigned long int filesize;

  fprintf(stdout, "GETTING file \"%s\"...\n", params);

  if((fp = fopen_current_dir(params, "rb")) == NULL) {
    perror("fopen");
    return;
  }

  fseek(fp, 0, SEEK_END);
  filesize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  fprintf(stdout, "File opened for reading (File size: \"%lu bytes\")\n", filesize);
  fprintf(stdout, "Sending file size over network and waiting for answer...\n");
  send_kermit_packet(socket, (char *) &filesize, sizeof(unsigned long int), PACKET_TYPE_FILESIZE, &answer);

  if(get_kermit_packet_type(&answer) == PACKET_TYPE_OK) {
    fprintf(stdout, "GOT OK message, transferring data...\n");
    while(filesize > MAX_PACKET_DATA) {
      fread(buffer, sizeof(char), MAX_PACKET_DATA, fp);
      fprintf(stdout, "Sending file block with \"%u bytes\" of data... ", MAX_PACKET_DATA);
      send_kermit_packet(socket, buffer, MAX_PACKET_DATA, PACKET_TYPE_DATA, &answer);
      fprintf(stdout, "Done!\n");
      filesize -= MAX_PACKET_DATA;
    }

    fread(buffer, sizeof(char), filesize, fp);
    fprintf(stdout, "Sending file block with \"%lu bytes\" of data... ", filesize);
    send_kermit_packet(socket, buffer, filesize, PACKET_TYPE_DATA, &answer);
    fprintf(stdout, "Done!\nSending END message over network...\n");
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

  while(!recv_kermit_packet(socket, &packet)) {
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
