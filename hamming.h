/* Codifica uma mensagem em Hamming 4:7 */
char *hamming_encode(const char *message, unsigned int length, unsigned int *ham_length);

/* Decodifica uma mensagem em Hamming 4:7 */
char *hamming_decode(const char *hamming, unsigned int length, unsigned int *msg_length);
