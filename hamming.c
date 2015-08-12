#include <stdio.h>
#include <stdlib.h>
#include <hamming.h>

/* Imprime um número em binário */
void printbin(unsigned char value, unsigned int nbits) {
    unsigned int i;

    /* Imprime o valor em inteiro */
    fprintf(stdout, "[%d] ", value);

    /* Imprime cada bit do valor */
    for(i = nbits; i > 0; --i) {
      fprintf(stdout, "%c", ((value >> (i - 1)) & 0x1 ? '1' : '0'));
    }

    fprintf(stdout, "\n");
}

/* Codifica um conjunto de 4 bits para 7 bits (Hamming 4:7) */
unsigned char get_encoded_bits(unsigned char value) {
  unsigned char result = 0, parity = 0;
  unsigned int i;
  unsigned int data_positions[] = {7, 6, 5, 3};
  unsigned int parity_positions[] = {4, 2, 1};

  /* Insere os bits de dados nas posições e calcula a paridade (XOR das posições com bit 1 */
  for(i = 0; i < 4; ++i) {
    if(value & (1 << i)) {
      result |= 1 << (7 - data_positions[i]);
      parity ^= data_positions[i];
    }
  }

  /* Insere os bits da paridade nas posições */
  for(i = 0; i < 3; ++i) {
    if(parity & (1 << i)) {
      result |= 1 << (7 - parity_positions[i]);
    }
  }

  return result;
}

/* Decodifica um conjunto de 7 bits para 4 bits (Hamming 4:7) */
unsigned char get_decoded_bits(unsigned char value) {
  unsigned char result = 0, parity = 0, error = 0;
  unsigned int i;
  unsigned int data_positions[] = {7, 6, 5, 3};
  unsigned int parity_positions[] = {4, 2, 1};

  /* Obtêm o valor e calcula a paridade (XOR das posições com bit 1) */
  for(i = 0; i < 4; ++i) {
    if(value & (1 << (7 - data_positions[i]))) {
      result |= (1 << i);
      parity ^= data_positions[i];
    }
  }

  /* Obtêm a paridade codificada */
  for(i = 0; i < 3; ++i) {
    if(value & (1 << (7 - parity_positions[i]))) {
      error |= (1 << i);
    }
  }

  /* Verifica se há algum erro */
  error ^= parity;

  /* Se houver, aplica a correção */
  if(error != 0) {
    if(result & (1 << (error - 1))) {
      result &= ~(1 << (error - 1));
    } else {
      result |= 1 << (error - 1);
    }
  }

  return result;
}

/* Codifica uma mensagem em Hamming 4:7 */
char *hamming_encode(const char *message, unsigned int length, unsigned int *ham_length) {
    char *hamming, *msg_ptr, *ham_ptr;

    /* Calcula o tamanho e aloca a mensagem codificada */
    *ham_length = length * 2;
    hamming = ham_ptr = (char *) malloc(*ham_length);

    /* Se foi alocada, vai percorrendo a mensagem e codificando cada conjunto de 4 bits */
    if(hamming != NULL) {
      for(msg_ptr = (char *) message; length--; ++msg_ptr) {
        *ham_ptr++ = get_encoded_bits(*msg_ptr >> 4);
        *ham_ptr++ = get_encoded_bits(*msg_ptr & 0xF);
      }
    }

    return hamming;
}

/* Decodifica uma mensagem em Hamming 4:7 */
char *hamming_decode(const char *hamming, unsigned int length, unsigned int *msg_length) {
  char *message, *ham_ptr, *msg_ptr;
  unsigned char first_nibble, second_nibble;

  /* Calcula o tamanho e aloca a mensagem decodificada */
  *msg_length = (length / 2) + 1;
  message = msg_ptr = (char *) malloc(*msg_length);

  /* Se foi alocada, vai percorrendo a mensagem e decodificando cada conjunto de 7 bits */
  if(message != NULL) {
    for(ham_ptr = (char *) hamming; length > 1; length -= 2, ham_ptr += 2) {
      first_nibble = get_decoded_bits(ham_ptr[0]);
      second_nibble = get_decoded_bits(ham_ptr[1]);

      *msg_ptr++ = (first_nibble << 4) | second_nibble;
    }

    *msg_ptr++ = '\0';
  }

  return message;
}
