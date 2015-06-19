#define MAX_PACKET_DATA           (0x3F)
#define MAX_PACKET_SEQ            (0x3F)

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

#define KERMIT_ERROR_PERM         (0x0)
#define KERMIT_ERROR_DIR_NFOUND   (0x1)
#define KERMIT_ERROR_FULL_DISK    (0x2)
#define KERMIT_ERROR_FILE_NFOUND  (0x3)
#define KERMIT_ERROR_SUCCESS      (0x4)

#define KERMIT_NO_TIMEOUT         (0x1)
#define KERMIT_ANSWER             (0x2)

struct kermit_packet {
  unsigned char packet_delim;
  unsigned char packet_len_seq;
  unsigned char packet_seq_type;
  char packet_data_crc[MAX_PACKET_DATA + 1];
};

int ConexaoRawSocket(char *device);

unsigned char get_kermit_packet_length(struct kermit_packet *packet);
unsigned char get_kermit_packet_seq(struct kermit_packet *packet);
unsigned char get_kermit_packet_type(struct kermit_packet *packet);

int send_kermit_packet(int socket, const char *data, unsigned int length, unsigned int type, struct kermit_packet *answer);
int recv_kermit_packet(int socket, struct kermit_packet *packet, int flags);
int kermit_error(struct kermit_packet *packet);

void debug_kermit_packet(struct kermit_packet *packet, unsigned char type);
