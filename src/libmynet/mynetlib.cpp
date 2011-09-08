/*
 * This file is part of InFeRno.
 *
 * Copyright (C) 2011:
 *    Nikos Ntarmos <ntarmos@cs.uoi.gr>,
 *    Sotirios Karavarsamis <s.karavarsamis@gmail.com>
 *
 * InFeRno is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * InFeRno is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with InFeRno. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "mynetlib.h"
#include "logger.h"

using namespace std;

const int ServerEndpoint::DEFAULT_BACKLOG = 5;

void BaseEndpoint::deallocInfo() {
	if (addr)
		freeaddrinfo(addr);
	addr = NULL;
	if (sd)
		delete[] sd;
	sd = NULL;
	sdlen = 0;
}

ServerEndpoint::ServerEndpoint(const string& hostname, const string& servname, int backlog) : BaseEndpoint(hostname, servname) {
	this->backlog = backlog;
}

int ServerEndpoint::init() {
	int tmp, err, index;
	struct addrinfo hints, *cur = NULL;

	if (servname.empty()) {
		Logger::error("createServerEndpoint: servname cannot be NULL");
		return -1;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_ADDRCONFIG | (hostname.empty() ? AI_PASSIVE : 0);

	if ((tmp = getaddrinfo(hostname.empty() ? NULL : hostname.c_str(), servname.c_str(), &hints, &addr))) {
		Logger::error("createServerEndpoint: getaddrinfo: %s", gai_strerror(tmp));
		return -1;
	}

	for (index = 0, cur = addr; cur; cur = cur->ai_next, index++) {}

	if ((sd = new int[index]) == NULL) {
		Logger::error("createServerEndpoint: calloc");
		deallocInfo();
		return -1;
	}
	sdlen = index;

	for (tmp = 0, err = 0, index = 0, cur = addr; cur; cur = cur->ai_next, index++, err = 0) {
		int on = 1;
		if ((sd[index] = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol)) < 0) {
			Logger::debug("socket");
			continue;
		}

		if (setsockopt(sd[index], SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
			Logger::error("setsockopt");

		if (bind(sd[index], cur->ai_addr, cur->ai_addrlen)) {
			Logger::debug("bind");
			close(sd[index]);
			sd[index] = -1;
			continue;
		}

		if (listen(sd[index], backlog)) {
			Logger::debug("listen");
			close(sd[index]);
			sd[index] = -1;
			continue;
		}

		tmp++;
	}
	if (!tmp)
		deallocInfo();
	return (tmp ? 0 : -1);
}

int ClientEndpoint::init() {
	int tmp, err;
	struct addrinfo hints, *cur = NULL;

	if (hostname.empty() || servname.empty()) {
		Logger::error("createClientEndpoint: %s cannot be NULL",
				(hostname.empty()) ? "hostname" : "servname");
		return -1;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if ((tmp = getaddrinfo(hostname.c_str(), servname.c_str(), &hints, &addr))) {
		Logger::error("createClientEndpoint: %s", gai_strerror(tmp));
		return -1;
	}

	if ((sd = new int[1]) == NULL) {
		Logger::error("createClientEndpoint: calloc");
		deallocInfo();
		return -1;
	}
	sdlen = 1;

	for (tmp = 0, err = 0, cur = addr; !tmp && cur; cur = cur->ai_next, err = 0) {
		if ((sd[0] = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol)) < 0) {
			Logger::debug("socket");
			continue;
		}

		if (connect(sd[0], cur->ai_addr, cur->ai_addrlen)) {
			Logger::debug("connect");
			close(sd[0]);
			sd[0] = -1;
			continue;
		}

		tmp ++;
	}

	if (!tmp)
		deallocInfo();
	return (tmp ? 0 : -1);
}

BaseEndpoint::~BaseEndpoint() {
	int index, err = 0;

	for (err = 0, index = 0; index < sdlen; index++) {
		if (sd[index] >= 0) {
			if ((shutdown(sd[index], SHUT_RDWR) && errno != EOPNOTSUPP)) {
				Logger::error("shutdown");
				err = 1;
			}
			if (close(sd[index])) {
				Logger::error("close");
				err = 1;
			}
		}
	}
	deallocInfo();
}

int ServerEndpoint::getNextClientFromEndpoint(ClientEndpoint*& client) {
	int index, maxsd;
	fd_set fds;
	client = new ClientEndpoint(hostname, servname);
	if (!client)
		return -1;

	if (!(client->sd = new int[1]) ||
			!(client->addr = (struct addrinfo*)malloc(sizeof(struct addrinfo))) ||
			!(client->addr->ai_addr = (struct sockaddr*)new struct sockaddr_storage)) {
		Logger::error("calloc");
		if (client->addr) {
			free(client->addr);
			client->addr = NULL;
		}
		delete client;
		client = NULL;
		return -1;
	}
	client->sdlen = 1;
	client->addr->ai_next = NULL;
	client->addr->ai_canonname = NULL;
	memset(client->addr->ai_addr, 0, sizeof(struct sockaddr_storage));

	do {
		FD_ZERO(&fds);
		for (index = 0, maxsd = -1; index < sdlen; index++) {
			if (sd[index] >= 0) {
				FD_SET(sd[index], &fds);
				if (maxsd < sd[index])
					maxsd = sd[index];
			}
		}

		if (select(maxsd + 1, &fds, NULL, NULL, NULL) <= 0) {
			if (errno != EINTR) {
				Logger::error("select");
				return -1;
				delete client;
				client = NULL;
			}
			continue;
		}

		for (index = 0; index < sdlen; index++) {
			if (sd[index] >= 0 && FD_ISSET(sd[index], &fds)) {
				do {
					client->addr->ai_addrlen = sizeof(struct sockaddr_storage);
					if ((client->sd[0] = accept(sd[index],
									client->addr->ai_addr,
									&client->addr->ai_addrlen)) >= 0)
						return 0;

					Logger::debug("accept");
				} while (errno == EINTR);
			}
		}
	} while (errno == ECONNRESET || errno == ECONNABORTED);
	delete client;
	client = NULL;
	return -1;
}

int BaseEndpoint::sendDataToEndpoint(const void *buf, size_t buflen) {
	size_t nleft;
	ssize_t nwritten = 0;
	char *ptr;

	if (!buf || !addr)
		return -1;

	ptr = (char*)buf;
	nleft = buflen;
	while (nleft > 0) {
		nwritten = send(sd[0], ptr, nleft, 0);
		if (nwritten <= 0) {
			if (errno == EINTR)
				nwritten = 0;
			else
				return (buflen ? -1 : 0);
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	return buflen;
}

int BaseEndpoint::recvDataFromEndpoint(void *buf, size_t buflen) {
	ssize_t nread = -1;

	if (!buf || !addr)
		return -1;

	while (nread < 0) {
		nread = recv(sd[0], buf, buflen, 0);
		if (nread < 0 && errno != EINTR)
			return -1;
	}
	return nread;
}
