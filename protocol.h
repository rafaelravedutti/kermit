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

/* Limites do protocolo */
#define MAX_PACKET_DATA           (0x3F)
#define MAX_PACKET_SEQ            (0x3F)

/* Tipos de pacotes */
#define PACKET_TYPE_NACK          (0x0)
#define PACKET_TYPE_ACK           (0x1)
#define PACKET_TYPE_SHOW          (0x4)
#define PACKET_TYPE_FILESIZE      (0x5)
#define PACKET_TYPE_LS            (0x8)
#define PACKET_TYPE_CD            (0x9)
#define PACKET_TYPE_PUT           (0xA)
#define PACKET_TYPE_GET           (0xB)
#define PACKET_TYPE_OK            (0xC)
#define PACKET_TYPE_DATA          (0xD)
#define PACKET_TYPE_ERROR         (0xE)
#define PACKET_TYPE_END           (0xF)

/* Erros */
#define KERMIT_ERROR_PERM         (0x0)
#define KERMIT_ERROR_DIR_NFOUND   (0x1)
#define KERMIT_ERROR_FULL_DISK    (0x2)
#define KERMIT_ERROR_FILE_NFOUND  (0x3)
#define KERMIT_ERROR_SUCCESS      (0x4)

/* Flags de recepção */
#define KERMIT_NO_TIMEOUT         (0x1)
#define KERMIT_ANSWER             (0x2)

/* Estrutura dos pacotes */
struct kermit_packet {
  unsigned char packet_delim;
  unsigned char packet_len_seq;
  unsigned char packet_seq_type;
  char packet_data_crc[MAX_PACKET_DATA + 1];
};

/* Inicia conexão com RAW socket */
int ConexaoRawSocket(char *device);

/* Obtêm dados dos pacotes */
unsigned char get_kermit_packet_length(struct kermit_packet *packet);
unsigned char get_kermit_packet_seq(struct kermit_packet *packet);
unsigned char get_kermit_packet_type(struct kermit_packet *packet);

/* Envia, recebe e processa erros de pacotes usando o protocolo kermit */
int send_kermit_packet(int socket, const char *data, unsigned int length, unsigned int type);
int recv_kermit_packet(int socket, struct kermit_packet *packet, int flags);
int kermit_error(struct kermit_packet *packet);

/* Dado um tipo esperado, depura um pacote kermit */
void debug_kermit_packet(struct kermit_packet *packet, unsigned char type);
