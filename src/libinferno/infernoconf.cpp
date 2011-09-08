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

#include <cstring>
#include <sstream>

#include "infernoconf.h"

using namespace std;

const string InfernoConf::CACHE_HOSTNAME     = "localhost";
const string InfernoConf::CACHE_STORE        = "inferno";
const string InfernoConf::CACHE_TABLE        = "cache";
const string InfernoConf::CACHE_UNAME        = "usr_inferno";
const string InfernoConf::CACHE_PASSWD       = "<passwd>";
const string InfernoConf::CACHE_DIR          = "/tmp/inferno";
const long InfernoConf::REDIR_LIMIT     = 10L;
const long InfernoConf::CONNECT_TIMEOUT = 10L;
const long InfernoConf::MAX_CONC_XFERS  = 40L;
const long InfernoConf::POLL_INTERVAL   = 200000L;
const long InfernoConf::LOW_SPEED_LIMIT = 100L;
const long InfernoConf::LOW_SPEED_TIME  = 5L;
const long InfernoConf::ACC_THRESH	   = 40;
const InfernoConf::FilteringMode InfernoConf::FILTERING_MODE  = InfernoConf::F_MODE_PAGE;

const char* InfernoConf::img_ext[] = {
	(char *)"bmp",
	(char *)"bmp",
	(char *)"gif",
	(char *)"jpg",
	(char *)"jpg",
	(char *)"png",
	(char *)"ico",
	(char *)"png",
	(char *)"bmp",
	(char *)"tiff",
	(char *)"pbm",
	(char *)"pbm",
	(char *)"pgm",
	(char *)"ppm",
	(char *)"xbm",
	(char *)"xpm",
	(char *)NULL
};

// array should be null-terminated. No intermediate NULL items are allowed!
const char* InfernoConf::img_mimes[] = {
	(char *)"image/x-bmp",
	(char *)"image/bmp",
	(char *)"image/gif",
	(char *)"image/jpeg",
	(char *)"image/pjpeg",
	(char *)"image/png",
	(char *)"image/x-icon",
	(char *)"image/x-png",
	(char *)"image/x-ms-bmp",
	(char *)"image/tiff",
	(char *)"image/x-portable-anymap",
	(char *)"image/x-portable-bitmap",
	(char *)"image/x-portable-graymap",
	(char *)"image/x-portable-pixmap",
	(char *)"image/x-xbitmap",
	(char *)"image/x-xpixmap",
	(char *)NULL
};


string InfernoConf::computePathFromHash(const string& hash) const {
	return cache_dir + "/" + hash.substr(0, 1) + "/" + hash;
}

string InfernoConf::toString() const {
	stringstream ss;

	ss << redir_limit << " " << conn_timeo << " " << max_xfers << " " << poll_interval << " " << low_speed_lim << " " << low_speed_time << " " << acc_thresh << " " << f_mode << "\n" <<
		cache_host << "\n" << cache_store << "\n" << cache_table << "\n" << cache_uname << "\n" << cache_passwd << "\n" << cache_dir << "\n";
	return ss.str();
}

InfernoConf* InfernoConf::parseString(const string& str) {
	stringstream ss(str);
	InfernoConf *ret;
	string cache_host;
	string cache_store;
	string cache_table;
	string cache_uname;
	string cache_passwd;
	string cache_dir;
	long redir_limit;
	long conn_timeo;
	long max_xfers;
	long poll_interval;
	long low_speed_lim;
	long low_speed_time;
	long acc_thresh;
	long f_mode_int;
	FilteringMode f_mode;

	ss >> redir_limit >> conn_timeo  >> max_xfers >> poll_interval >> low_speed_lim >> low_speed_time >> acc_thresh >> f_mode_int;
	switch (f_mode_int) {
		case F_MODE_PAGE:
		case F_MODE_IMAGE:
		case F_MODE_MIXED:
			f_mode = (InfernoConf::FilteringMode)f_mode_int;
			break;
		default:
			return NULL;
	}
	getline(ss, cache_host); // Eat the left-over "\n" from operator>>()
	getline(ss, cache_host);
	getline(ss, cache_store);
	getline(ss, cache_table);
	getline(ss, cache_uname);
	getline(ss, cache_passwd);
	getline(ss, cache_dir);
	if (ss.fail())
		return NULL;
	ret = new InfernoConf(cache_host, cache_store, cache_table, cache_uname, cache_passwd, cache_dir, redir_limit, conn_timeo, max_xfers, poll_interval, low_speed_lim, low_speed_time, acc_thresh, f_mode);
	return ret;
}

bool InfernoConf::isImageContentType(const std::string& ct) {
	char *ptr = const_cast<char*>(img_mimes[0]);
	const char* ctptr = ct.c_str();
	for (int i = 0; ptr; ptr = const_cast<char*>(img_mimes[++i]))
		if (!strncasecmp(ctptr, ptr, strlen(img_mimes[i])))
			return true;
	return false;
}
