/*
 *
*/

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "server.h"
#include "http.h"
#include "files.h"
#include "mime.h"

#define QLEN	5
#define RAIZ	"www"
#define TROZO	4096
#define PUERTO	"8080"

static int enviar_todo(int sd, const char *buf, size_t len) {
	ssize_t n;

	while (len > 0) {
		n=write(sd, buf, len);
		if(n<=0)
			return -1;
		buf += n;
		len -= n;
	}
	return 0;
}

static int enviar_archivo(int sd, int fd) {
	char trozo[TROZO];
	ssize_t n;

	while ((n = read(fd, trozo, sizeof(trozo))) > 0 ) {
		if(enviar_todo(sd, trozo, n) < 0)
			return -1;
	}
	return (n<0) ? -1 : 0;
}

static void atender(int nsd) {
	char buf[HTTP_MAX_CAB];
	char cab[512], ruta[PATH_MAX];
	struct http_peticion pet;
	size_t len = 0, fin;
	ssize_t n;
	off_t tam;
	int codigo, fd, largo;

	while((fin = http_fin_peticion(buf, len)) == 0) {
		if(len == sizeof(buf)) {
			largo = http_error(cab, sizeof(cab), 400, 0);
			if(largo>0)
				enviar_todo(nsd, cab, largo);
			printf("(peticiooon demasiado grande) -> 400\n");
			return;
		}
		n = read(nsd, buf + len, sizeof(buf) - len);
		if (n <= 0)
			return;
		len += n;
	}

	codigo = http_parsear(buf, fin, &pet);
	if (codigo == 200)
		codigo = files_abrir(RAIZ, pet.uri, &fd, &tam, ruta, sizeof(ruta));

	printf("%s %s -> %d\n", pet.metodo, pet.uri, codigo);


	if (codigo != 200) {
		largo = http_error(cab, sizeof(cab), codigo, 0);
		if (largo>0)
			enviar_todo(nsd, cab, largo);
		return;
	}

	largo = http_encabezados(cab, sizeof(cab), 200, mime_tipo(ruta), tam, 0);
	if(largo>0 && enviar_todo(nsd, cab, largo) == 0)
		enviar_archivo(nsd, fd);
	close(fd);
 }


int main(int argc, char *argv[]) {

	const char *puerto = PUERTO;
	int sd, nsd;

	setvbuf(stdout, NULL, _IOLBF, 0);
	signal(SIGPIPE, SIG_IGN);

	if(argc > 1)
		puerto = argv[1];

	sd = passiveTCP(puerto, QLEN);
	if (sd <0)
		return 1;

	printf("MiniHTTPd escuchando en el puerto %s\n", puerto);

	while (1) {
		nsd = accept(sd, NULL, NULL);
		if (nsd < 0)
			continue;
		atender(nsd);
		close(nsd);
	}
}
