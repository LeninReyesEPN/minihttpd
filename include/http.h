#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <sys/types.h>

#define HTTP_MAX_CAB 8192

struct http_peticion {
	char metodo[16];
	char uri[2048];
	char version[16];
	char host[256];
	char connection[64];
	char user_agent[512];
	int keep_alive;
};

size_t http_fin_peticion(const char *buf, size_t len);

int http_parsear(const char *buf, size_t len, struct http_peticion *p);

const char *http_frase(int codigo);

int http_encabezados(char *dst, size_t tam, int codigo, const char *tipo, off_t largo, int keep_alive);

int http_error(char *dst, size_t tam, int codigo, int keep_alive);

#endif
