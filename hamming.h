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

/* Codifica uma mensagem em Hamming 4:7 */
char *hamming_encode(const char *message, unsigned int length, unsigned int *ham_length);

/* Decodifica uma mensagem em Hamming 4:7 */
char *hamming_decode(const char *hamming, unsigned int length, unsigned int *msg_length);
