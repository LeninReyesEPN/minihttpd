/*
 *
*/

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "http.h"

static int copiar(char *dst, size_t tam, const char *origen, size_t largo) {
	if (largo == 0 || largo >= tam)
		return -1;
	memcpy(dst, origen, largo);
	dst[largo] = '\0';
	return 0;
}

size_t http_fin_peticion(const char *buf, size_t len){
	size_t i;
	for (i=0; i + 3 < len; i++) {
		if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n')
			return i + 4;
	}
	return 0;
}

int http_parsear(const char *buf, size_t len, struct http_peticion *p) {
	char cab[HTTP_MAX_CAB + 1];
	char *linea, *fin, *esp1, *esp2, *dos, *valor, *q;

	memset(p, 0, sizeof(*p));

	if (len > HTTP_MAX_CAB)
		return 400;
	memcpy(cab, buf, len);
	cab[len] = '\0';

	fin = strstr(cab, "\r\n");
	if(fin == NULL)
		return 400;
	*fin = '\0';

	esp1 = strchr(cab, ' ');
	if(esp1 == NULL)
		return 400;
	esp2 = strchr(esp1 + 1, ' ');
	if(esp2 == NULL)
		return 400;
	if(strchr(esp2 + 1, ' ') != NULL)
		return 400;

	if (copiar(p->metodo, sizeof(p->metodo), cab, esp1 - cab) < 0)
		return 400;
	if (copiar(p->uri, sizeof(p->uri), esp1 + 1, esp2 - (esp1 + 1)) < 0)
		return 400;
	if(copiar(p->version, sizeof(p->version), esp2 + 1, strlen(esp2 + 1)) < 0)
		return 400;

	if (strcmp(p->version, "HTTP/1.1") != 0 && strcmp(p->version, "HTTP/1.0") != 0)
		return 400;
	if(p->uri[0] != '/')
		return 400;

	q = strchr(p->uri, '?');
	if(q != NULL)
		*q = '\0';

	linea = fin + 2;
	while ((fin = strstr(linea, "\r\n")) != NULL && fin != linea) {
		*fin = '\0';
		dos = strchr(linea, ':');
		if(dos == NULL)
			return 400;
		*dos = '\0';
		valor = dos + 1;
		while (*valor == ' ' || *valor == '\t')
			valor++;

		if(strcasecmp(linea, "Host") == 0) {
			if(copiar(p->host, sizeof(p->host), valor, strlen(valor)) < 0)
				return 400;
		} else if (strcasecmp(linea, "Connection") == 0) {
			if(copiar(p->connection, sizeof(p->connection), valor, strlen(valor)) < 0)
                                return 400;
		} else if (strcasecmp(linea, "User-Agent") == 0) {
			if(copiar(p->user_agent, sizeof(p->user_agent), valor, strlen(valor)) < 0)
				return 400;
		}
		linea = fin + 2;
	}
	if (fin == NULL)
		return 400;

	if (strcmp(p->version, "HTTP/1.1") == 0 &&
	    (p->host[0] == '\0' || p->connection[0] == '\0' || p->user_agent[0] == '\0'))
		return 400;

	if (strcmp(p->version, "HTTP/1.1") == 0)
		p->keep_alive = (strcasecmp(p->connection, "close") != 0);
	else
		p->keep_alive = (strcasecmp(p->connection, "keep-alive") == 0);

	if(strcmp(p->metodo, "GET") != 0 )
		return 405;

	return 200;
}

const char *http_frase(int codigo) {
	switch (codigo) {
		case 200: return "OK";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		default: return "Internal Server Error";
	}
}

int http_encabezados (char *dst, size_t tam, int codigo, const char *tipo, off_t largo, int keep_alive) {
	int n;
	n = snprintf(dst, tam,
	    "HTTP/1.1 %d %s\r\n"
	    "Server: MiniHTTPd\r\n"
	    "Content-Type: %s\r\n"
   	    "Content-Length: %lld\r\n"
	    "Connection: %s\r\n"
	    "%s"
	    "\r\n",
  	    codigo, http_frase(codigo), tipo,
     	    (long long) largo,
	    keep_alive ? "keep-alive" : "close",
	    codigo == 405 ? "Allow: GET\r\n" : "");
	if (n<0 || (size_t) n >= tam)
		return -1;
	return n; 
}


int http_error(char *dst, size_t tam, int codigo, int keep_alive) {
	char cuerpo[256];
	int nc, ne;

	nc = snprintf(cuerpo, sizeof(cuerpo), "<html><body><h1>%d %s</h1></body></html>\n", codigo, http_frase(codigo));
	if (nc < 0 || nc >= (int) sizeof(cuerpo))
                return -1;

	ne = http_encabezados(dst, tam, codigo, "text/html", nc, keep_alive);
	if (ne < 0 || (size_t) (ne + nc) >= tam)
                return -1;

	memcpy(dst +ne, cuerpo, nc + 1);
	return ne + nc;
}

