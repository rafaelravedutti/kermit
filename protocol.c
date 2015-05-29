#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <protocol.h>
#include <hamming.h>

static unsigned int current_send_seq = 0, current_recv_seq = 0;

int ConexaoRawSocket(char *device) {
  int soquete;
  struct ifreq ir;
  struct sockaddr_ll endereco;
  struct packet_mreq mr;

  soquete = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL)); /* cria socket */
  if(soquete == -1) {
    perror("socket");
    exit(-1);
  }

  memset(&ir, 0, sizeof(struct ifreq)); /* dispositivo eth0 */
  memcpy(ir.ifr_name, device, sizeof(ir.ifr_name));
  if(ioctl(soquete, SIOCGIFINDEX, &ir) == -1) {
    perror("ioctl");
    exit(-1);
  }

  memset(&endereco, 0, sizeof(endereco)); /* IP do dispositivo */
  endereco.sll_family = AF_PACKET;
  endereco.sll_protocol = htons(ETH_P_ALL);
  endereco.sll_ifindex = ir.ifr_ifindex;
  if(bind(soquete, (struct sockaddr *)&endereco, sizeof(endereco)) == -1) {
    perror("bind");
    exit(-1);
  }

  memset(&mr, 0, sizeof(mr)); /* Modo Promiscuo */
  mr.mr_ifindex = ir.ifr_ifindex;
  mr.mr_type = PACKET_MR_PROMISC;
  if(setsockopt(soquete, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1) {
    perror("setsockopt");
    exit(-1);
  }

  return soquete;
}

unsigned char get_kermit_packet_length(struct kermit_packet *packet) {
  return (packet->packet_len_seq >> 2) & 0x3F;
}

unsigned char get_kermit_packet_seq(struct kermit_packet *packet) {
  return ((packet->packet_len_seq & 0x3) << 4) | ((packet->packet_seq_type >> 4) & 0xF);
}

unsigned char get_kermit_packet_type(struct kermit_packet *packet) {
  return packet->packet_seq_type & 0xF;
}

char get_vertical_parity(struct kermit_packet *packet) {
  char result = 0;
  char *ptr;
  unsigned int i, length;

  length = get_kermit_packet_length(packet);
  for(i = 0, ptr = ((char *) packet) + 1; i < length; ++i, ++ptr) {
    result ^= *ptr;
  }

  return result;
}

int send_kermit_packet(int socket, const char *data, unsigned int length, unsigned int type, struct kermit_packet *answer) {
  struct kermit_packet packet;
  char *encoded_packet;
  char crc;
  unsigned int ep_length;

  packet.packet_delim = 0x7E;
  packet.packet_len_seq = ((length & 0x3F) << 2) | ((current_send_seq >> 4) & 0x3);
  packet.packet_seq_type = ((current_send_seq & 0xF) << 4) | (type & 0xF);
  memcpy(packet.packet_data_crc, data, length);

  crc = get_vertical_parity(&packet);
  memcpy(packet.packet_data_crc + length, &crc, sizeof(crc));

  encoded_packet = hamming_encode((char *) &packet, length + 4, &ep_length);
  encoded_packet[1] = 0x7E;

  do {
    if(send(socket, encoded_packet + 1, ep_length - 1 + 8, 0) < 0) {
      perror("send");
      return -1;
    }

    if(answer != NULL) {
      if(wait_kermit_answer(socket, answer) < 0) {
        fprintf(stdout, "Connection Timed Out!\n");
        return -2;
      }
    }
  } while(answer != NULL && get_kermit_packet_type(answer) == PACKET_TYPE_NACK);

  current_send_seq = (current_send_seq + 1) % MAX_PACKET_SEQ;

  free(encoded_packet);
  return 0;
}

int recv_kermit_packet(int socket, struct kermit_packet *packet) {
  char encoded_packet[sizeof(struct kermit_packet) * 2];
  char *decoded_packet;
  int received = 0;
  unsigned int length, dp_length;

  while(!received && recv(socket, encoded_packet, sizeof(encoded_packet), 0) > 0) {
    if(encoded_packet[0] == 0x7E) {
      decoded_packet = hamming_decode(encoded_packet + 1, (sizeof(struct kermit_packet) * 2) - 1, &dp_length);
      memcpy(((char *) packet) + 1, decoded_packet, dp_length);
      free(decoded_packet);

      if(get_kermit_packet_seq(packet) == current_recv_seq) {
        length = get_kermit_packet_length(packet);
        if(get_vertical_parity(packet) == packet->packet_data_crc[length]) {
          received = 1;
        }
      }

      if(!received) {
        send_kermit_packet(socket, "", 0, PACKET_TYPE_NACK, NULL);
      }
    }
  }

  if(!received) {
    perror("recv");
    return -1;
  }

  current_recv_seq = (current_recv_seq + 1) % MAX_PACKET_SEQ;
  return 0;
}

int wait_kermit_answer(int socket, struct kermit_packet *answer) {
  recv_kermit_packet(socket, answer);
  return 0;
}

int kermit_error(struct kermit_packet *packet) {
  char *error;

  if(get_kermit_packet_type(packet) == PACKET_TYPE_ERROR) {
    error = packet->packet_data_crc;

    if(*error == KERMIT_ERROR_PERM) {
      fprintf(stdout, "Permissão negada!\n");
    } else if(*error == KERMIT_ERROR_DIR_NFOUND) {
      fprintf(stdout, "Diretório inexistente!\n");
    } else if(*error == KERMIT_ERROR_FULL_DISK) {
      fprintf(stdout, "Não há espaço suficiente em disco!\n");
    } else if(*error == KERMIT_ERROR_FILE_NFOUND) {
      fprintf(stdout, "Arquivo inexistente!\n");
    } else {
      fprintf(stdout, "Ocorreu um erro durante a operação!\n");
    }

    return 1;
  }

  return 0;
}

void print_kermit_packet(struct kermit_packet *packet) {
  char crc;
  unsigned int length;

  length = get_kermit_packet_length(packet);
  crc = packet->packet_data_crc[length];
  packet->packet_data_crc[length] = '\0';

  printf( "Kermit Packet\n"
          "\tDelim: %d\n"
          "\tLength: %d\n"
          "\tSequence: %d\n"
          "\tType: %d\n"
          "\tData: %s\n"
          "\tCRC: %x\n", packet->packet_delim, length, get_kermit_packet_seq(packet), get_kermit_packet_type(packet), packet->packet_data_crc, crc);

  packet->packet_data_crc[length] = crc;
}
