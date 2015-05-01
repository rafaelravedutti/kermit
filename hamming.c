#include <stdio.h>
#include <stdlib.h>
#include <hamming.h>

void printbin(unsigned char value, unsigned int nbits) {
    unsigned int i;

    printf("[%d] ", value);

    for(i = nbits; i > 0; --i) {
      printf("%c", ((value >> (i - 1)) & 0x1 ? '1' : '0'));
    }

    printf("\n");
}

unsigned char get_encoded_bits(unsigned char value) {
  unsigned char result = 0, parity = 0;
  unsigned int i;
  unsigned int data_positions[] = {7, 6, 5, 3};
  unsigned int parity_positions[] = {4, 2, 1};

  for(i = 0; i < 4; ++i) {
    if(value & (1 << i)) {
      result |= 1 << (7 - data_positions[i]);
      parity ^= data_positions[i];
    }
  }

  for(i = 0; i < 3; ++i) {
    if(parity & (1 << i)) {
      result |= 1 << (7 - parity_positions[i]);
    }
  }

  return result;
}

unsigned char get_decoded_bits(unsigned char value) {
  unsigned char result = 0, parity = 0, error = 0;
  unsigned int i;
  unsigned int data_positions[] = {7, 6, 5, 3};
  unsigned int parity_positions[] = {4, 2, 1};

  for(i = 0; i < 4; ++i) {
    if(value & (1 << (7 - data_positions[i]))) {
      result |= (1 << i);
      parity ^= data_positions[i];
    }
  }

  for(i = 0; i < 3; ++i) {
    if(value & (1 << (7 - parity_positions[i]))) {
      error |= (1 << i);
    }
  }

  error ^= parity;

  if(error != 0) {
    if(result & (1 << (error - 1))) {
      result &= ~(1 << (error - 1));
    } else {
      result |= 1 << (error - 1);
    }
  }

  return result;
}

char *hamming_encode(const char *message, unsigned int length, unsigned int *ham_length) {
    char *hamming, *msg_ptr, *ham_ptr;

    *ham_length = length * 2;
    hamming = ham_ptr = (char *) malloc(*ham_length);

    if(hamming != NULL) {
      for(msg_ptr = (char *) message; length--; ++msg_ptr) {
        *ham_ptr++ = get_encoded_bits(*msg_ptr >> 4);
        *ham_ptr++ = get_encoded_bits(*msg_ptr & 0xF);
      }
    }

    return hamming;
}

char *hamming_decode(const char *hamming, unsigned int length, unsigned int *msg_length) {
  char *message, *ham_ptr, *msg_ptr;
  unsigned char first_nibble, second_nibble;

  *msg_length = (length / 2) + 1;
  message = msg_ptr = (char *) malloc(*msg_length);

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
