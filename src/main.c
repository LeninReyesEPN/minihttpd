/*
 *
*/

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include "server.h"
#include "http.h"
#include "files.h"
#include "mime.h"

#define QLEN	5
#define RAIZ	"www"
#define TROZO	4096
#define PUERTO	"8080"
#define MAX_CONEX 1024
#define MAX_EVENTOS 64

struct conexion {
	int activa;
	char ent[HTTP_MAX_CAB];
	size_t ent_len;
	char sal[TROZO];
	size_t sal_len;
	size_t sal_env;
	int fd_archivo;
	int keep_alive;
	int cerrando;
};

static struct conexion conex[MAX_CONEX];

static int ep;

static void revisar(int fd);

static int no_bloqueante(int fd) {
	int f = fcntl(fd, F_GETFL, 0);

	if (f<0)
		return -1;
	return fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

static void vigilar(int fd, unsigned int eventos, int op) {
	struct epoll_event ev;

	ev.events=eventos;
	ev.data.fd = fd;
	epoll_ctl(ep, op, fd, &ev);
}

static void cerrar(int fd) {
	struct conexion *c = &conex[fd];

	if(c->fd_archivo >= 0)
		close(c->fd_archivo);
	epoll_ctl(ep, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	c->activa = 0;
}

static void aceptar(int sd) {
	struct conexion *c;
	int nsd;

	while ((nsd = accept(sd, NULL, NULL)) >= 0 ) {
		if (nsd >= MAX_CONEX || no_bloqueante(nsd) < 0 ) {
			close(nsd);
			continue;
		}
		c = &conex[nsd];
		c->activa = 1;
		c->ent_len = 0;
		c->sal_len = 0;
		c->sal_env = 0;
		c->fd_archivo = -1;
		c->keep_alive = 0;
		c->cerrando = 0;
		vigilar(nsd, EPOLLIN, EPOLL_CTL_ADD);
	}
}

static void preparar(int fd, size_t fin)
{
	struct conexion *c = &conex[fd];
	struct http_peticion pet;
	char ruta[PATH_MAX];
	off_t tam;
	int codigo, n, fda = -1;

	codigo = http_parsear(c->ent, fin, &pet);
	if (codigo == 200)
		codigo = files_abrir(RAIZ, pet.uri, &fda, &tam, ruta, sizeof(ruta));

	printf("[%d] %s %s -> %d\n", fd, pet.metodo, pet.uri, codigo);

	c->keep_alive = (codigo == 400) ? 0 : pet.keep_alive;

	if (codigo == 200) {
		n = http_encabezados(c->sal, sizeof(c->sal), 200, mime_tipo(ruta), tam, c->keep_alive);
		c->fd_archivo = fda;
	} else {
		n = http_error(c->sal, sizeof(c->sal), codigo, c->keep_alive);
	}
	if (n < 0) {			/* no cupo (no deberia pasar): cerrar */
		n = 0;
		c->keep_alive = 0;
		if (c->fd_archivo >= 0) {
			close(c->fd_archivo);
			c->fd_archivo = -1;
		}
	}
	c->sal_len = n;
	c->sal_env = 0;

	memmove(c->ent, c->ent + fin, c->ent_len - fin);
	c->ent_len -= fin;

	vigilar(fd, EPOLLOUT, EPOLL_CTL_MOD);	/* ahora toca escribir */
}

/* ¿Hay una peticion completa en el bufer de este cliente? */
static void revisar(int fd)
{
	struct conexion *c = &conex[fd];
	size_t fin;
	int n;

	fin = http_fin_peticion(c->ent, c->ent_len);
	if (fin > 0) {
		preparar(fd, fin);
	} else if (c->ent_len == sizeof(c->ent)) {	/* lleno y sin linea vacia */
		printf("[%d] (peticion demasiado grande) -> 400\n", fd);
		n = http_error(c->sal, sizeof(c->sal), 400, 0);
		c->sal_len = (n > 0) ? n : 0;
		c->sal_env = 0;
		c->keep_alive = 0;
		c->ent_len = 0;
		vigilar(fd, EPOLLOUT, EPOLL_CTL_MOD);
	}
}

/* El cliente tiene datos para nosotros */
static void leer(int fd)
{
	struct conexion *c = &conex[fd];
	ssize_t n;

	if (c->cerrando)		/* lo que llegue ya no importa: se descarta */
		c->ent_len = 0;

	n = read(fd, c->ent + c->ent_len, sizeof(c->ent) - c->ent_len);
	if (n == 0) {				/* el cliente cerro */
		cerrar(fd);
		return;
	}
	if (n < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			cerrar(fd);
		return;				/* EAGAIN: no habia nada, otra vez sera */
	}
	if (c->cerrando)
		return;
	c->ent_len += n;
	revisar(fd);
}

/* Podemos enviarle datos al cliente: enviar todo lo que se pueda sin esperar */
static void escribir(int fd)
{
	struct conexion *c = &conex[fd];
	ssize_t n;

	while (1) {
		/* 1. ¿Queda algo en sal? enviarlo */
		if (c->sal_env < c->sal_len) {
			n = write(fd, c->sal + c->sal_env, c->sal_len - c->sal_env);
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					return;		/* lleno: seguimos en el proximo EPOLLOUT */
				cerrar(fd);
				return;
			}
			c->sal_env += n;
			continue;
		}

		/* 2. sal vacio: ¿queda archivo? leer el siguiente trozo */
		if (c->fd_archivo >= 0) {
			n = read(c->fd_archivo, c->sal, sizeof(c->sal));
			if (n > 0) {
				c->sal_len = n;
				c->sal_env = 0;
				continue;
			}
			close(c->fd_archivo);
			c->fd_archivo = -1;
			if (n < 0) {
				cerrar(fd);
				return;
			}
		}

		/* 3. Respuesta terminada */
		if (!c->keep_alive) {
			/* cierre "educado": avisar que ya no enviamos nada mas (FIN)
			 * y esperar a que el cliente cierre, descartando lo que mande */
			shutdown(fd, SHUT_WR);
			c->cerrando = 1;
			vigilar(fd, EPOLLIN, EPOLL_CTL_MOD);
			return;
		}
		c->sal_len = 0;
		c->sal_env = 0;
		vigilar(fd, EPOLLIN, EPOLL_CTL_MOD);	/* volver a esperar peticiones */
		revisar(fd);		/* por si ya llego otra peticion detras de esta */
		return;
	}
}

int main(int argc, char *argv[])
{
	struct epoll_event eventos[MAX_EVENTOS];
	const char *puerto = PUERTO;
	int sd, n, i, fd;

	setvbuf(stdout, NULL, _IOLBF, 0);
	signal(SIGPIPE, SIG_IGN);

	if (argc > 1)
		puerto = argv[1];

	sd = passiveTCP(puerto, QLEN);
	if (sd < 0)
		return 1;
	if (no_bloqueante(sd) < 0) {
		perror("fcntl");
		return 1;
	}

	ep = epoll_create1(0);
	if (ep < 0) {
		perror("epoll_create1");
		return 1;
	}
	vigilar(sd, EPOLLIN, EPOLL_CTL_ADD);

	printf("MiniHTTPd (epoll) escuchando en el puerto %s\n", puerto);

	while (1) {
		n = epoll_wait(ep, eventos, MAX_EVENTOS, -1);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			perror("epoll_wait");
			return 1;
		}
		for (i = 0; i < n; i++) {
			fd = eventos[i].data.fd;
			if (fd == sd)
				aceptar(sd);
			else if (!conex[fd].activa)
				continue;
			else if (eventos[i].events & (EPOLLERR | EPOLLHUP))
				cerrar(fd);
			else if (eventos[i].events & EPOLLIN)
				leer(fd);
			else if (eventos[i].events & EPOLLOUT)
				escribir(fd);
		}
	}
}
