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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include "seadclient.h"
#include "infernoconf.h"
#include "logger.h"
#include "config.h"

using namespace std;

SeadClient::SeadClient() {
	// use default connection settings
	nio = new ClientEndpoint("127.0.0.1", "1345");
}

SeadClient::SeadClient(std::string host, std::string port) {
	// set user-specific connection settings
	nio = new ClientEndpoint(host, port);
}

SeadClient::~SeadClient() {
	if (nio)
		delete nio;
	nio = NULL;
}

SeadClient::SCResult SeadClient::init() {
	// connect to server
	if (nio->init() == -1) {
	   // return failure
	   return SCL_FAILURE;
	}
	
	// return success
	return SCL_OK;
}

int SeadClient::classifyUri(const string& path, const string& type, const InfernoConf& iConf) {
	int ret;
	// construct request string
	string req_buf = "CLASSIFY " + path + " OFTYPE " + type + "\n";
	req_buf.append(iConf.toString());
	req_buf.append("\r\n");
	
	// forward request to classification server
	if (nio->sendDataToEndpoint(req_buf.c_str(), req_buf.length() + 1) == -1)
		return -1;

	// get response from classification server
	if (nio->recvDataFromEndpoint(&ret, sizeof(ret)) == -1)
		return -1;

	return ret;
}
