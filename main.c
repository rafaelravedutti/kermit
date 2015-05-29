#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <protocol.h>
#include <local.h>
#include <server.h>
#include <client.h>

int main(int argc, const char *argv[]) {
  char buffer[MAX_PACKET_DATA];
  char *device;
  int socket;
  unsigned int length;

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
      fgets(buffer, sizeof(buffer), stdin);

      length = strlen(buffer);
      buffer[length - 1] = '\0';

      exec_command(socket, buffer);
    } while(strcmp(buffer, "fim") != 0);
  } else if(strcmp(argv[2], "servidor") == 0) {
    server_listen(socket);
  } else {
    printf("USO: %s [interface] [cliente/servidor]\n", argv[0]);
  }

  close(socket);

  free(device);
  return 0;
}
