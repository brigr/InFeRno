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

#ifndef __MY_DBCACHE_H__
#define __MY_DBCACHE_H__

#include <iostream>
#include <mysql.h>

#include "infernoconf.h"
#include "config.h"

class DbCache {
	private:
		/* MySQL-related data structures */
		MYSQL *conn;
		bool mysql_connected;
		InfernoConf dbConf;

		int checkAndCreateDir(const char *);
		int makeSpoolQueue();

	public:
		DbCache();
		~DbCache();

		int init(const InfernoConf&);
		int init();

		/* connection status checking */
		int isConnectionAlive();
		int reconnect(int times=1);

		/* error handling */
		int getErrorCode();
		char* getErrorString();

		/* general convenience functions */
		std::string* makeHashByurl(const std::string& url);
		InfernoConf getInfernoConf();
		
		/* cache I/O functions */
		int updateUrlStatus(const std::string& hash, InfernoConf::Status status);
		int updateUrlClassification(const std::string& hash, InfernoConf::Classification classification);
		int updateUrlContentType(const std::string& hash, const std::string& ctype);
		int insertUrlEntry(const std::string& url, std::string& hash);

		InfernoConf::Classification lookupUrlClassification(const std::string& hash);
		InfernoConf::Status lookupUrlStatus(const std::string& hash);
		std::string lookupUrlContentType(const std::string& hash);
		
		int fixCache();

		/* basic operations  */
		int connect();
		void cleanup();
};

#endif
