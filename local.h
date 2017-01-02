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

/* Realiza um parse das opções do ls (-a e -l) */
void parse_list_options(const char *params, int *all, int *list);

/* Inicializa diretórios atual e anterior */
void init_directory();

/* Obtêm o diretório atual */
char *get_current_directory();

/* Altera o diretório corrente */
int change_directory(const char *path, int verbose);

/* Obtêm a lista de arquivos e diretórios no diretório corrente */
char *get_current_directory_list(int all, int list, unsigned int *length, char *error);

/* Abre um arquivo a partir do diretório corrente */
FILE *fopen_current_dir(const char *path, const char *mode);

/* Verifica se há determinado espaço no diretório corrente */
int check_avaiable_space(unsigned int size);
