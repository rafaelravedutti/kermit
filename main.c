#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <protocol.h>

int main(int argc, const char *argv[]) {
  struct kermit_packet packet;
  char buffer[1024];
  char *device;
  int s;

  if(argc < 3) {
    printf("USO: %s [interface] [cliente/servidor]\n", argv[0]);
    return 0;
  }

  device = strdup(argv[1]);

  if(device == NULL) {
    printf("Erro ao duplicar nome do dispositivo!\n");
    return -1;
  }

  s = ConexaoRawSocket(device);

  if(strcmp(argv[2], "cliente") == 0) {
    do {
      fgets(buffer, sizeof buffer, stdin);
      send_kermit_packet(s, buffer, sizeof buffer, 0);
    } while(strcmp(buffer, "fim") != 0);
  } else if(strcmp(argv[2], "servidor") == 0) {
    do {
      recv_kermit_packet(s, &packet);
      printf("%s\n", packet.packet_data_crc);
    } while(strcmp(packet.packet_data_crc, "fim") != 0);
  } else {
    printf("USO: %s [interface] [cliente/servidor]\n", argv[0]);
  }

  close(s);

  free(device);
  return 0;
}
