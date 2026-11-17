/*
 *
*/

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "files.h"

#define INDICE "index.html"

static int resolver(const char *raiz_real, const char *pedida, char *real) {
	size_t n;
	if (realpath(pedida, real) == NULL ) {
		if(errno == ENOENT || errno == ENOTDIR)
			return 404;
		if(errno == EACCES)
			return 403;
		return 500;
	}

	n = strlen(raiz_real);
	if (strncmp(real, raiz_real, n) != 0)
		return 403;

	if (real[n] != '/' && real[n] != '\0')
		return 403;

	return 200;

}

int files_abrir(const char *raiz, const char *uri, int *fd, off_t *tam, char *ruta, size_t ruta_tam) {
	char raiz_real[PATH_MAX], pedida[PATH_MAX], real[PATH_MAX];
	struct stat info;
	int codigo, n;

	if (uri[0] != '/')
		return 403;

	if(realpath(raiz, raiz_real) == NULL)
		return 500;

	n = snprintf(pedida, sizeof(pedida), "%s%s", raiz_real, uri);
	if (n < 0 || n>= (int) sizeof(pedida))
		return 400;
	codigo = resolver(raiz_real, pedida, real);
	if (codigo != 200)
		return codigo;
	if (stat(real, &info) < 0 )
		return 500;

	if (S_ISDIR(info.st_mode)) {
		n = snprintf(pedida, sizeof(pedida), "%s/%s", real, INDICE);
		if (n < 0 || n >= (int) sizeof(pedida))
			return 400;
		codigo = resolver(raiz_real, pedida, real);
		if (codigo != 200)
			return codigo;
	}

	*fd = open(real, O_RDONLY);
	if (*fd < 0) {
		if (errno == EACCES)
			return 403;
		return 500;
	}

	if (fstat(*fd, &info) < 0) {
		close(*fd);
		return 500;
	}
	if (!S_ISREG(info.st_mode)) {
		close(*fd);
		return 403;
	}

	*tam = info.st_size;
	n = snprintf(ruta, ruta_tam, "%s", real);
	if (n < 0 || (size_t) n >= ruta_tam) {
		close(*fd);
		return 500;
	}
	return 200;
}









