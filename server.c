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

  parse_list_options(params, &all, &list);
  dirlist = get_current_directory_list(all, list, &list_len, &error);

  if(!error) {
    list_ptr = dirlist;

    while(list_len > MAX_PACKET_DATA) {
      send_kermit_packet(socket, list_ptr, MAX_PACKET_DATA, PACKET_TYPE_SHOW, &answer);
      list_ptr += MAX_PACKET_DATA;
      list_len -= MAX_PACKET_DATA;
    }

    send_kermit_packet(socket, list_ptr, list_len, PACKET_TYPE_SHOW, &answer);
    send_kermit_packet(socket, "", 0, PACKET_TYPE_END, &answer);
  } else {
    send_kermit_packet(socket, &error, sizeof(char), PACKET_TYPE_SHOW, &answer);
  }
}

void kermit_server_cd(int socket, const char *params, unsigned int params_length) {
  char error;

  error = change_directory(params, 0);

  if(!error) {
    send_kermit_packet(socket, "", 0, PACKET_TYPE_OK, NULL);
  } else {
    send_kermit_packet(socket, &error, sizeof(char), PACKET_TYPE_ERROR, NULL);
  }
}

void kermit_server_put(int socket, const char *params, unsigned int params_length) {
  FILE *fp;
  struct kermit_packet answer;
  unsigned long int filesize;
  unsigned char type;

  if((fp = fopen_current_dir(params, "wb")) == NULL) {
    perror("fopen");
    return;
  }

  send_kermit_packet(socket, "", 0, PACKET_TYPE_OK, NULL);
  recv_kermit_packet(socket, &answer);

  if(get_kermit_packet_type(&answer) == PACKET_TYPE_FILESIZE) {
    filesize = *((unsigned long int *) answer.packet_data_crc);
    send_kermit_packet(socket, "", 0, PACKET_TYPE_OK, NULL);
  }

  if(!kermit_error(&answer)) {
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
  }

  fclose(fp);
}

void kermit_server_get(int socket, const char *params, unsigned int params_length) {
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

  recv_kermit_packet(socket, &answer);
  send_kermit_packet(socket, (char *) &filesize, sizeof(unsigned long int), PACKET_TYPE_FILESIZE, &answer);

  if(get_kermit_packet_type(&answer) == PACKET_TYPE_OK) {
    while(filesize > MAX_PACKET_DATA) {
      fread(buffer, sizeof(char), MAX_PACKET_DATA, fp);
      send_kermit_packet(socket, buffer, MAX_PACKET_DATA, PACKET_TYPE_DATA, &answer);
      filesize -= MAX_PACKET_DATA;
    }

    fread(buffer, sizeof(char), filesize, fp);
    send_kermit_packet(socket, buffer, filesize, PACKET_TYPE_DATA, &answer);
    send_kermit_packet(socket, "", 0, PACKET_TYPE_END, &answer);
  }

  fclose(fp);
}

void server_listen(int socket) {
  struct kermit_packet packet;
  unsigned char type, length;

  while(!recv_kermit_packet(socket, &packet)) {
    type = get_kermit_packet_type(&packet);
    length = get_kermit_packet_length(&packet);

    if(type == PACKET_TYPE_LS) {
      kermit_server_ls(socket, packet.packet_data_crc, length);
    } else if(type == PACKET_TYPE_CD) {
      kermit_server_cd(socket, packet.packet_data_crc, length);
    } else if(type == PACKET_TYPE_PUT) {
      kermit_server_put(socket, packet.packet_data_crc, length);
    } else if(type == PACKET_TYPE_GET) {
      kermit_server_get(socket, packet.packet_data_crc, length);
    }
  }
}
