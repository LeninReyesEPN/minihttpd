/*
 *
*/


#include <stddef.h>
#include <string.h>
#include <strings.h>
#include "mime.h"

#define MIME_DEFECTO "application/octet-stream"

struct mime_entrada {
	const char *ext;
	const char *tipo;
};

static const struct mime_entrada tabla [] = {
	{".html", "text/html"},
	{".css", "text/css"},
	{".js" , "application/javascript"},
	{".png", "image/png"},
	{".jpg", "image/jpeg"},
};

const char *mime_tipo(const char *ruta) {
	const char *nombre;
	const char *punto;
	size_t i;

	nombre = strrchr(ruta, '/');
	if (nombre != NULL)
		nombre = nombre + 1;
	else
		nombre = ruta;

	punto = strrchr(nombre, '.');
	if (punto == NULL)
		return MIME_DEFECTO;

	for (i = 0; i < sizeof(tabla) / sizeof(tabla[0]); i++) {
		if (strcasecmp(punto, tabla[i].ext) == 0)
			return tabla[i].tipo;
	}

	return MIME_DEFECTO;
}
