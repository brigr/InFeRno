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

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>
#include <mysql.h>

#include "errmsg.h"
#include "mysqld_error.h"
#include "dbcache.h"
#include "logger.h"
#include "config.h"

using namespace std;

DbCache::DbCache() {
	conn = NULL;
}

DbCache::~DbCache() {
	cleanup();
}

int DbCache::init(const InfernoConf& conf) {
	my_bool _recnct = 1;

	cleanup();

	dbConf = conf;

	// make spool directory
	if (makeSpoolQueue())
		return -1;

	// initialize libmysql
	conn = mysql_init(NULL);
	if (!conn)
		return -1;

	// enable automatic reconnection
	mysql_options(conn, MYSQL_OPT_RECONNECT, (void *)&_recnct);
	mysql_connected = false;
	return 0;
}

int DbCache::init() {
	// use the default credentials defined in "dbcachecreds.cpp"
	return init(InfernoConf());
}
InfernoConf DbCache::getInfernoConf() {
	return dbConf;
}

int DbCache::checkAndCreateDir(const char* path) {
	struct stat st;
	int stat_;
	if ((stat_ = stat(path, &st))) { // If path does not exist, create it
		if (mkdir(path, 0700)) {
			if (errno != EEXIST) {
				perror("mkdir");
				return 1;
			}
			stat_ = 1;
		}
	}

	if (stat_ > 0 && (stat_ = stat(path, &st))) {
		perror("stat");
		return 1;
	}

	if (!stat_) { // Path exists
		if (!S_ISDIR(st.st_mode)) { // If path is not a directory
			Logger::error("The spool directory path exists but is not a directory. Bailing out...");
			return 1;
		}
		if (access(path, F_OK)) { // Path is a dir. Check access rights.
			perror("access");
			return 1;
		}
	}
	return 0;
}

int DbCache::makeSpoolQueue() {
	string path = dbConf.getDirectory();

	if (checkAndCreateDir(path.c_str()))
		return 1;

	path += "/";
	// check if structure is already constructed on the file system
	for (int i = 0; i < 16; i++) {
		char suffix[2];
		snprintf(suffix, 2, "%x", i);
		string subdir = path;
		subdir.push_back('/');
		subdir.push_back(tolower(suffix[0]));

		if (checkAndCreateDir(subdir.c_str()))
			return 1;
	}

	return 0;
}

int DbCache::reconnect(int times) {
	unsigned long tid;

	if (mysql_connected) {
		tid = mysql_thread_id(conn);
		if (!mysql_ping(conn))
			return 1;
		if (tid != mysql_thread_id(conn))
			return 1;
	}

	/* try to establish connection 'times' times */
	for(int i = 0; i < times; i++)
		if(connect())
			return 1;

	return 0;
}

int DbCache::getErrorCode() {
	return mysql_errno(this->conn);
}

char *DbCache::getErrorString() {
	return const_cast<char*>(mysql_error(this->conn));
}

int DbCache::connect() {
	mysql_connected = (mysql_real_connect(conn,
				dbConf.getHostname().c_str(),
				dbConf.getUsername().c_str(),
				dbConf.getPassword().c_str(),
				dbConf.getStore().c_str(),
				0, NULL, CLIENT_REMEMBER_OPTIONS) != NULL);
	return mysql_connected;
}

/**
 * Returns: 1 for success, -1 for 'dup_unique', 0 otherwise
 */
int DbCache::insertUrlEntry(const string& url, string& hash) {
	string stmt;
	MYSQL_RES *res;
	MYSQL_ROW row;
	char *buf;

	if (url.empty())
		return 0;

	if(!reconnect())
		return 0;

	if (!(buf = new char[2 * url.length() + 1]))
		return 0;
	mysql_escape_string(buf, url.c_str(), url.length());

	stmt = "SELECT MD5('";
	stmt.append(buf);
	stmt.append("')");

	// execute MD5 query
	if(mysql_real_query(conn, stmt.c_str(), stmt.length()))
		return 0;

	if (!(res = mysql_store_result(conn)))
		return 0;

	// check if we have a result
	if (!mysql_num_rows(res)) {
		mysql_free_result(res);
		return 0;
	}

	// fetch md5 transform
	row = mysql_fetch_row(res);

	// return MD5
	hash = string(row[0]);
	mysql_free_result(res);

	/* begin creating an INSERT statement, adding the id value */
	stmt.clear();
	stmt.append("INSERT INTO " + dbConf.getTable() + "(hash, url, decision, status) VALUES ('" + hash + "', '");
	stmt.append(buf);
	stmt.append("', ");
	stmt.push_back('0' + InfernoConf::CLASS_UNDEFINED);
	stmt.push_back(',');
	stmt.push_back('0' + InfernoConf::STATUS_FETCHING);
	stmt.push_back(')');
	delete[] buf;

	if (mysql_real_query(conn, stmt.c_str(), stmt.length())) {
		int error = getErrorCode();
		if (error == ER_DUP_UNIQUE || error == ER_DUP_ENTRY)
			return -1;
		return 0;
	}
	return 1;
}

InfernoConf::Classification DbCache::lookupUrlClassification(const string& hash) {
	MYSQL_ROW row;
	MYSQL_RES *res;
	string stmt;
	int rows;

	// attempt reconnection if connection to mysql has gone down
	if(!reconnect()) {
		Logger::debug("lookupUrlClassification: connection was turned down...");
		return InfernoConf::CLASS_ERROR;
	}

	// prepare query statement
	stmt.append("SELECT decision+0 FROM " + dbConf.getTable() + " WHERE hash='" + hash + "'");

	// send and execute query on the server
	if(mysql_query(conn, stmt.c_str())) {
		Logger::debug("lookupUrlClassification: mysql_query() failed. Error report: %s", getErrorString());
		return InfernoConf::CLASS_ERROR;
	}

	// obtain the result-set and check if it is non-empty
	if (!(res = mysql_store_result(conn)) ||
			!(rows = mysql_num_rows(res))) {
		if (res)
			mysql_free_result(res);
		return InfernoConf::CLASS_ERROR;
	}

	// fetch row
	row = mysql_fetch_row(res);
	InfernoConf::Classification decision = (InfernoConf::Classification)atoi(row[0]);
	mysql_free_result(res);

	return decision;
}

InfernoConf::Status DbCache::lookupUrlStatus(const string& hash) {
	MYSQL_ROW row;
	MYSQL_RES *res;
	string stmt;
	int rows;

	// attempt reconnection if connection to mysql has gone down
	if(!reconnect()) {
		Logger::debug("lookupUrlStatus: connection was turned down...");
		return InfernoConf::STATUS_ERROR;
	}

	// prepare query statement
	stmt.append("SELECT status+0 FROM " + dbConf.getTable() + " WHERE hash='");
	stmt.append(hash);
	stmt.append("'");

	// send and execute query on the server
	if(mysql_query(conn, stmt.c_str())) {
		Logger::debug("lookupUrlStatus: mysql_query() failed. Error report: %s", getErrorString());
		return InfernoConf::STATUS_ERROR;
	}

	// obtain the result-set and check if it is non-empty
	if (!(res = mysql_store_result(conn)) ||
			!(rows = mysql_num_rows(res))) {
		if (res)
			mysql_free_result(res);
		return InfernoConf::STATUS_ERROR;
	}

	// fetch row
	row = mysql_fetch_row(res);
	InfernoConf::Status status = (InfernoConf::Status)atoi(row[0]);
	mysql_free_result(res);

	return status;
}

string DbCache::lookupUrlContentType(const string& hash) {
	MYSQL_ROW row;
	MYSQL_RES *res;
	string stmt, reply;
	int rows;

	// attempt reconnection if connection to mysql has gone down
	if(!reconnect()) {
		Logger::debug("lookupUrlClassification: connection was turned down...");
		return reply;
	}

	// prepare query statement
	stmt.append("SELECT ctype FROM " + dbConf.getTable() + " WHERE hash='" + hash + "'");

	// send and execute query on the server
	if(mysql_query(conn, stmt.c_str())) {
		Logger::debug("lookupUrlClassification: mysql_query() failed. Error report: %s", getErrorString());
		return reply;
	}

	// obtain the result-set and check if it is non-empty
	if (!(res = mysql_store_result(conn)) ||
			!(rows = mysql_num_rows(res))) {
		if (res)
			mysql_free_result(res);
		return reply;
	}

	// fetch row
	row = mysql_fetch_row(res);
	reply = row[0];
	mysql_free_result(res);

	return reply;
}

int DbCache::updateUrlClassification(const string& hash, InfernoConf::Classification classification) {
	string stmt;

	if(!reconnect())
		return 0;

	/* construct SQL statement */
	stmt.append("UPDATE " + dbConf.getTable() + " SET decision=");
	stmt.push_back('0' + classification); // XXX: Works only for <10 class types
	stmt.append(" WHERE hash='" + hash + "'");

	/* execute query */
	if(mysql_query(conn, stmt.c_str())) {
		Logger::debug("updateUrlClassification(): mysql_query() failed. Error report: %s", mysql_error(conn));
		return 0;
	}

	/* check if we have a row change */
	return (mysql_affected_rows(conn) == 1);
}

int DbCache::updateUrlStatus(const string& hash, InfernoConf::Status status) {
	string stmt;

	if(!reconnect())
		return 0;

	/* construct SQL statement */
	if (status != InfernoConf::STATUS_FAILURE) {
		stmt.append("UPDATE " + dbConf.getTable() + " SET status=");
		stmt.push_back('0' + status); // XXX: Works only for <10 statuses
	} else {
		stmt.append("DELETE FROM " + dbConf.getTable());
	}
	stmt.append(" WHERE hash='" + hash + "'");

	/* execute query */
	if(mysql_query(conn, stmt.c_str()))
		return 0;

	/* check if we have a row change */
	return (mysql_affected_rows(this->conn) == 1);
}

int DbCache::updateUrlContentType(const string& hash, const string& ctype) {
	string stmt;

	if(!reconnect())
		return 0;

	/* construct SQL statement */
	stmt.append("UPDATE " + dbConf.getTable() + " SET ctype='" + ctype + "' WHERE hash='" + hash + "'");

	/* execute query */
	if(mysql_query(conn, stmt.c_str())) {
		Logger::debug("updateUrlClassification(): mysql_query() failed. Error report: %s", mysql_error(conn));
		return 0;
	}

	/* check if we have a row change */
	return (mysql_affected_rows(conn) == 1);
}

void DbCache::cleanup() {
	if (conn)
		mysql_close(conn);
	conn = NULL;
}

int DbCache::fixCache() {
	if(!reconnect() || mysql_query(conn, "DELETE FROM cache WHERE not (status='DONE' or status='FAILURE')"))
		return 0;

	return 1;
}
