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
#include <unistd.h>
#include <protocol.h>
#include <local.h>
#include <server.h>
#include <client.h>

/* Função principal */
int main(int argc, const char *argv[]) {
  char buffer[MAX_PACKET_DATA];
  char *device;
  int socket;
  unsigned int length;

  /* Se não há argumentos suficientes, reporta erro de sintaxe */
  if(argc < 3) {
    printf("USO: %s [interface] [cliente/servidor]\n", argv[0]);
    return 0;
  }

  /* Interface/dispositivo de acesso */
  device = strdup(argv[1]);

  if(device == NULL) {
    printf("Erro ao duplicar nome do dispositivo!\n");
    return -1;
  }

  /* Realiza conexão com o RAW socket */
  fprintf(stdout, "Initializing RAW socket connection... ");
  socket = ConexaoRawSocket(device);
  fprintf(stdout, "Done!\n");

  /* Inicializa diretório corrente */
  init_directory();
  fprintf(stdout, "Current directory initialized to \"%s\"\n", get_current_directory());

  /* Se deve operar em modo cliente, aguarda comandos */
  if(strcmp(argv[2], "cliente") == 0) {
    do {
      printf("%s > ", get_current_directory());
      fgets(buffer, sizeof(buffer), stdin);

      length = strlen(buffer);
      buffer[length - 1] = '\0';

      exec_command(socket, buffer);
    } while(strcmp(buffer, "fim") != 0);
  /* Se deve operar em modo servidor, aguarda requisições */
  } else if(strcmp(argv[2], "servidor") == 0) {
    server_listen(socket);
  /* Caso não seja especificado nem cliente nem servidor, reporta erro de sintaxe */
  } else {
    printf("USO: %s [interface] [cliente/servidor]\n", argv[0]);
  }

  /* Fecha socket */
  close(socket);

  /* Libera espaço de memória ocupado pelo nome da interface/dispositivo */
  free(device);
  return 0;
}
