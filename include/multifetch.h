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

#ifndef __MY_MULTIFETCH_H__
#define __MY_MULTIFETCH_H__

#include <set>
#include <string>
#include <curl/curl.h>

#include "seadclient.h"
#include "dbcache.h"
#include "infernoconf.h"
#include "config.h"

class Multifetch {
	private:
		// porn classification statistics per web page request
		long int porn_count;
		long int bikini_count;
		long int benign_count;

		InfernoConf iConf;

		int consult_nimage_classifier(std::string hash, std::string type, SeadClient* sclient = NULL);
		CURL* setupHandle(const std::string& url, const std::string& path, FILE *& fp, char* errorBuffer = NULL);

	public:
		Multifetch() : iConf() {
			porn_count = benign_count = bikini_count = 0;
		}
		Multifetch(const InfernoConf& ic) : iConf(ic) {
			porn_count = benign_count = bikini_count = 0;
		}

		int fetch_multi_from_list(const std::set<std::string>&, DbCache *);
		InfernoConf::Classification extractlinks(const std::string& url_pt, std::string& url_pt_hash, std::string& ctype);
};
#endif
