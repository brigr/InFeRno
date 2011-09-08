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

#ifndef __MY_NET_LIB_H__
#define __MY_NET_LIB_H__

#include <string>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

class BaseEndpoint {
	protected:
		std::string hostname;
		std::string servname;
		int *sd;     // table of socket descriptors
		int sdlen;   // # elements in sd
		struct addrinfo *addr;   // info for the above descriptors

		void deallocInfo();

	public:
		BaseEndpoint(const std::string& host, const std::string& service) :
			hostname(host), servname(service), sd(NULL), sdlen(0), addr(NULL) {}
		BaseEndpoint() :
			hostname(), servname(), sd(NULL), sdlen(0), addr(NULL) {}
		~BaseEndpoint();
		virtual int init() = 0;

		int sendDataToEndpoint(const void *data, size_t datalen);
		int recvDataFromEndpoint(void *data, size_t datalen);
};

class ClientEndpoint : public BaseEndpoint {
	public:
		ClientEndpoint(const std::string& host, const std::string& service) : BaseEndpoint(host, service) {} ;
		int init();
		friend class ServerEndpoint;
};

class ServerEndpoint : public BaseEndpoint {
	private:
		const static int DEFAULT_BACKLOG;
		int backlog; // for TCP server sockets

	public:
		ServerEndpoint(const std::string& host, const std::string& service, int backlog = DEFAULT_BACKLOG);
		int init();
		int getNextClientFromEndpoint(ClientEndpoint*& client);
};

#endif
