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

#include <iostream>

#include "seadclient.h"
#include "logger.h"
#include "infernoconf.h"
#include "config.h"

using namespace std;

int main(int argc, char** argv) {
	int cres;
	SeadClient::SCResult res;
	InfernoConf iConf;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <path to image>\n", argv[0]);
		return 1;
	}

	fprintf(stderr, "[i] attempting connection on classification server!\n");

	SeadClient *sc = new SeadClient("127.0.0.1", "1345");
	if((res = sc->init()) == SeadClient::SCL_FAILURE) {
		fprintf(stderr, "[i] failed to connect to classification server!\n");
		return 1;
	}

	fprintf(stderr, "[i] connection to classification server is established!\n");

	// send classification request
	fprintf(stderr, "[=>] sending classification request...\n");

	cres = sc->classifyUri(argv[1], "jpg", iConf);

	// get server response
	fprintf(stderr, "[<=] Server responded with code: %d\n", cres);

	/*
	switch(cres) {
		case InfernoConf::CLASS_PORN:
			fprintf(stderr, "[=>] Image is PORN\n");
			break;

		case InfernoConf::CLASS_BENIGN:
			fprintf(stderr, "[=>] Image is BENIGN\n");
			break;

		case InfernoConf::CLASS_BIKINI:
			fprintf(stderr, "[=>] Image is BIKINI\n");
			break;

		case InfernoConf::CLASS_ERROR:
		case InfernoConf::CLASS_UNDEFINED:
			fprintf(stderr, "[=>] ERROR: Server could not serve request for some reason!\n");
			break;
	}
	*/

	fprintf(stderr, "[i] closing client connection.\n");
	//sc->disconnect();

	return 0;
}
