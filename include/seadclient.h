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

#ifndef __SEADCLIENT_H__
#define __SEADCLIENT_H__

#include <iostream>

#include "mynetlib.h"
#include "infernoconf.h"
#include "config.h"

class SeadClient {
	private:
		ClientEndpoint *nio;
		
	public:
		enum SCResult {
			SCL_OK,
			SCL_FAILURE
		};

		// constructors
		SeadClient();
		SeadClient(std::string host, std::string port);
		~SeadClient();
		SeadClient::SCResult init();
		
		// network classifier request/response methods
		int classifyUri(const std::string& path, const std::string& type, const InfernoConf& iConf);
};
#endif
