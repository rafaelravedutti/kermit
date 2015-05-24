#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <protocol.h>
#include <local.h>
#include <client.h>

int main(int argc, const char *argv[]) {
  struct kermit_packet packet;
  char buffer[MAX_PACKET_DATA];
  char *device;
  int socket;

  if(argc < 3) {
    printf("USO: %s [interface] [cliente/servidor]\n", argv[0]);
    return 0;
  }

  device = strdup(argv[1]);

  if(device == NULL) {
    printf("Erro ao duplicar nome do dispositivo!\n");
    return -1;
  }

  socket = ConexaoRawSocket(device);

  init_directory();

  if(strcmp(argv[2], "cliente") == 0) {
    do {
      printf("%s > ", get_current_directory());
      fgets(buffer, sizeof buffer, stdin);
      // exec_command(s, buffer);
      send_kermit_packet(socket, buffer, strlen(buffer), 0, NULL);
    } while(strcmp(buffer, "fim") != 0);
  } else if(strcmp(argv[2], "servidor") == 0) {
    do {
      recv_kermit_packet(socket, &packet, 0);
      printf("%s\n", packet.packet_data_crc);
    } while(strcmp(packet.packet_data_crc, "fim") != 0);
  } else {
    printf("USO: %s [interface] [cliente/servidor]\n", argv[0]);
  }

  close(socket);

  free(device);
  return 0;
}
