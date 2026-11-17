#ifndef FILES_H
#define FILES_H

#include <stddef.h>
#include <sys/types.h>

int files_abrir(const char *raiz, const char *uri, int *fd, off_t *tam, char *ruta, size_t ruta_tam);

#endif
