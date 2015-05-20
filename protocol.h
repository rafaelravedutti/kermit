#define MAX_PACKET_DATA    (0x3F)
#define MAX_PACKET_SEQ     (0x3F)

struct kermit_packet {
  unsigned char packet_delim;
  unsigned char packet_len_seq;
  unsigned char packet_seq_type;
  char packet_data_crc[MAX_PACKET_DATA + 1];
};

int ConexaoRawSocket(char *device);

int send_kermit_packet(int socket, const char *data, unsigned int length, unsigned int type);
int recv_kermit_packet(int socket, struct kermit_packet *packet);
unsigned char get_kermit_packet_length(struct kermit_packet *packet);
unsigned char get_kermit_packet_seq(struct kermit_packet *packet);
unsigned char get_kermit_packet_type(struct kermit_packet *packet);
