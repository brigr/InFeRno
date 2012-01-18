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
#include <string>

#include <sys/stat.h>
#include <unistd.h>

extern "C"
{
#include <c-icap.h>
#include <simple_api.h>
#include <debug.h>
}

#include "multifetch.h"
#include "htmlparse.h"
#include "dbcache.h"
#include "logger.h"
#include "config.h"

using namespace std;

int   inferno_init_service(ci_service_xdata_t * srv_xdata, struct ci_server_conf *server_conf);
void  inferno_close_service();
void* inferno_init_request_data(ci_request_t * req);
void  inferno_release_data(void *data);
int   inferno_check_preview(char *preview_data, int preview_data_len, ci_request_t *);
int   inferno_process(ci_request_t *);
int   inferno_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof, ci_request_t * req);

static InfernoConf iConf;

int cfg_get_filtering_mode(char *directive, char **argv, void *setdata);
int cfg_get_acceptance_threshold(char *directive, char **argv, void *setdata);
int cfg_get_redir_limit(char *directive, char **argv, void *setdata);
int cfg_get_conn_timeo(char *directive, char **argv, void *setdata);
int cfg_get_speed_lim(char *directive, char **argv, void *setdata);
int cfg_get_max_xfers(char *directive, char **argv, void *setdata);
int cfg_get_poll_ival(char *directive, char **argv, void *setdata);
int cfg_get_cachedir(char *directive, char **argv, void *setdata);
int cfg_get_cache_db(char *directive, char **argv, void *setdata);

const char *protos[] = {"", "http", "https", "ftp", NULL};
enum proto {UNKNOWN=0, HTTP, HTTPS, FTP};

const static struct ci_conf_entry configuration[] = {
	{(char*)"FilteringMode", &iConf, cfg_get_filtering_mode, NULL},
	{(char*)"AcceptanceThreshold", &iConf, cfg_get_acceptance_threshold, NULL},
	{(char*)"RedirectionLimit", &iConf, cfg_get_redir_limit, NULL},
	{(char*)"ConnectTimeout", &iConf, cfg_get_conn_timeo, NULL},
	{(char*)"LowSpeedLimit", &iConf, cfg_get_speed_lim, NULL},
	{(char*)"MaxConcurrentTransfers", &iConf, cfg_get_max_xfers, NULL},
	{(char*)"PollInterval", &iConf, cfg_get_poll_ival, NULL},
	{(char*)"CacheDir", &iConf, cfg_get_cachedir, NULL},
	{(char*)"CacheDB", &iConf, cfg_get_cache_db, NULL},
	{NULL, NULL, NULL, NULL}
};

CI_DECLARE_MOD_DATA ci_service_module_t service = {
	(char *)"inferno",
	(char *)"InFeRno porn filtering service",
	ICAP_REQMOD,
	inferno_init_service,       /* init_service			 */
	NULL,                       /* post_init_service	 */
	inferno_close_service,      /* close_Service		 */
	inferno_init_request_data,  /* init_request_data	 */
	inferno_release_data,       /* release request data  */
	inferno_check_preview,
	inferno_process,
	inferno_io,
	const_cast<ci_conf_entry *>(configuration),
	NULL
};

struct inferno_req_data {
	ci_cached_file_t *body;
	int denied;
	int eof;
};

enum http_methods { HTTP_UNKNOWN = 0, HTTP_GET, HTTP_POST };

int cfg_get_filtering_mode(char *directive, char **argv, void *setdata) {
	(void)directive;
	if(!argv || !argv[0] || !setdata)
		return 0;

	long val = atol(argv[0]);
	if (val == InfernoConf::F_MODE_PAGE ||
		val == InfernoConf::F_MODE_IMAGE ||
		val == InfernoConf::F_MODE_MIXED) {
		((InfernoConf *)setdata)->setFilteringMode((InfernoConf::FilteringMode)val);
		return 1;
	}
	return 0;
}

int cfg_get_acceptance_threshold(char *directive, char **argv, void *setdata) {
	(void)directive;
	if(!argv || !argv[0] || !setdata)
		return 0;
	((InfernoConf *)setdata)->setAcceptanceThreshold(atol(argv[0]));
	return 1;
}

int cfg_get_redir_limit(char *directive, char **argv, void *setdata) {
	(void)directive;
	if (!argv || !argv[0] || !setdata)
		return 0;
	((InfernoConf *)setdata)->setRedirLimit(atol(argv[0]));
	return 1;
}

int cfg_get_conn_timeo(char *directive, char **argv, void *setdata) {
	(void)directive;
	if (!argv || !argv[0] || !setdata)
		return 0;
	((InfernoConf *)setdata)->setConnTimeout(atol(argv[0]));
	return 1;
}

int cfg_get_speed_lim(char *directive, char **argv, void *setdata) {
	(void)directive;
	if (!argv || !argv[0] || !argv[1])
		return 0;
	((InfernoConf *)setdata)->setLowSpeedLimit(atol(argv[0]));
	((InfernoConf *)setdata)->setLowSpeedTime(atol(argv[1]));
	return 1;
}

int cfg_get_max_xfers(char *directive, char **argv, void *setdata) {
	(void)directive;
	if (!argv || !argv[0] || !setdata)
		return 0;
	((InfernoConf *)setdata)->setMaxXfers(atol(argv[0]));
	return 1;
}

int cfg_get_poll_ival(char *directive, char **argv, void *setdata) {
	(void)directive;
	if (!argv || !argv[0] || !setdata)
		return 0;
	((InfernoConf *)setdata)->setPollInterval(atol(argv[0]));
	return 1;
}

int cfg_get_cachedir(char *directive, char **argv, void *setdata) {
	(void)directive;
	if (!argv || !argv[0] || !setdata)
		return 0;
	((InfernoConf *)setdata)->setDirectory(argv[0]);
	return 1;
}

int cfg_get_cache_db(char *directive, char **argv, void *setdata) {
	(void)directive;
	if (!argv || !argv[0] || !argv[1] || !argv[2] || !argv[3] || !argv[4])
		return 0;
	((InfernoConf *)setdata)->setHostname(argv[0]);
	((InfernoConf *)setdata)->setStore(argv[1]);
	((InfernoConf *)setdata)->setTable(argv[2]);
	((InfernoConf *)setdata)->setUsername(argv[3]);
	((InfernoConf *)setdata)->setPassword(argv[4]);
	return 1;
}

void inferno_close_service() {
	Logger::info("Releasing InFeRno module...");
	DbCache cache;
	if (!cache.init(iConf))
		cache.fixCache();
}

int inferno_init_service(ci_service_xdata_t * srv_xdata, struct ci_server_conf *server_conf) {
	uint64_t xops;
	(void)server_conf;

	Logger::info("Initializing InFeRno...");
	curl_global_init(CURL_GLOBAL_DEFAULT);
	mysql_library_init(0, NULL, NULL);

	ci_service_set_preview(srv_xdata, 1024);
	ci_service_enable_204(srv_xdata);

	xops = CI_XCLIENTIP | CI_XSERVERIP;
	xops |= CI_XAUTHENTICATEDUSER | CI_XAUTHENTICATEDGROUPS;
	ci_service_set_xopts(srv_xdata, xops);

	ci_service_set_transfer_preview(srv_xdata, (char*)"*");

	Logger::info("Inferno initialized...");

	return CI_OK;
}

void * inferno_init_request_data(ci_request_t * req) {
	struct inferno_req_data *uc = new struct inferno_req_data;
	(void) req;

	uc->body = ci_cached_file_new(0);
	uc->denied = 0;
	uc->eof = 0;

	return uc; /*Get from a pool of pre-allocated structs better...... */
}

void inferno_release_data(void *data) {
	struct inferno_req_data *uc = (struct inferno_req_data *)data;
	if(uc) {
		if (uc->body)
			ci_cached_file_destroy(uc->body);
		delete uc;
	}
}

/**
 * Returns 0 on success, -1 on unknown HTTP method or empty url
 */
int get_http_url(ci_headers_list_t * req_header, string& site, string& url) {
	char *str;

	// Now get the site name
	str = const_cast<char*>(ci_headers_value(req_header, (char*)"Host"));
	if (str)
		site = str;

	str = req_header->headers[0];
	if (!strncasecmp(str, "GET ", 4))
		str += 4;
	else if (!strncasecmp(str, "POST ", 5))
		str += 5;
	else {
		Logger::info("Unknown HTTP method %s", str);
		return -1;
	}

	while (*str == ' ')
		str++;

	/* copy page to the struct. */
	for (url.clear(); *str != ' ' && *str != '\0'; str++)
		url.push_back(*str);

	if (url.empty()) {
		Logger::info("Unable to infer URL from request %s", str);
		return -1;
	}
	return 0;
}

const static char *blocked_message = (char*)"<html><head><title>InFeRno pornography elimination system</title>"
"</head><body><h1>Permission to access site has been denied!</h1><br>"
"<h2>InFeRno has blocked access to this site due to a high indication "
"of it being pornographic!</h2></body></html>";

const static char denied_img[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00,
	0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89,
	0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xae, 0xce,
	0x1c, 0xe9, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x08,
	0xd7, 0x63, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, 0x00, 0x05, 0x00,
	0x01, 0x5e, 0xf3, 0x2a, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
	0x4e, 0x44, 0xae, 0x42, 0x60, 0x82, 0x00
};


int inferno_check_preview(char *preview_data, int preview_data_len, ci_request_t * req) {
	struct inferno_req_data *uc = (struct inferno_req_data *)ci_service_data(req);
	ci_headers_list_t *req_header;
	InfernoConf::Classification cval;
	string cur_site, cur_uri, cur_uri_hash, ctype;
	bool gotError = false;
	InfernoConf::FilteringMode f_mode = iConf.getFilteringMode();
	(void)preview_data_len;
	(void)preview_data;

	Logger::debug("Filtering mode: %d", f_mode);
	Multifetch multifetch(iConf);

	if ((req_header = ci_http_request_headers(req)) == NULL ||
			get_http_url(req_header, cur_site, cur_uri)) {
		ci_req_unlock_data(req);
		goto allow_request;
	}

	Logger::info("URL host: '%s', URL page: '%s'", cur_site.c_str(), cur_uri.c_str());
	ci_req_unlock_data(req);
	uc->denied = 0;

	// classify web page
	Logger::info("fetching and classifying web object");
	cval = multifetch.extractlinks(cur_uri, cur_uri_hash, ctype);
	if (cval == InfernoConf::CLASS_ERROR) {
		gotError = true;
		goto allow_request;
	}

	if ((cval == InfernoConf::CLASS_PORN || cval == InfernoConf::CLASS_BIKINI) &&
		(
		 (f_mode == InfernoConf::F_MODE_MIXED) || 
		 (f_mode == InfernoConf::F_MODE_PAGE && !InfernoConf::isImageContentType(ctype)) || 
		 (f_mode == InfernoConf::F_MODE_IMAGE && InfernoConf::isImageContentType(ctype))
		)) {
		size_t reply_len = 0;
		char *reply = NULL;
		bool needsFree = false;

		uc->denied = 1;
		if (uc->body) {
			ci_cached_file_destroy(uc->body);
			uc->body = NULL;
		}

		ci_http_response_create(req, 1, 1);
		Logger::info("Adding headers");
		ci_http_response_add_header(req, (char*)"HTTP/1.0 200 OK");
		ci_http_response_add_header(req, (char*)"Server: C-ICAP");
		ci_http_response_add_header(req, (char*)"Connection: close");

		if (iConf.getFilteringMode() != InfernoConf::F_MODE_PAGE && InfernoConf::isImageContentType(ctype)) {
			Logger::info("image was classified as BIKINI/PORN. Forwarding blurred version to the user");
			struct stat st;
			string path = iConf.computePathFromHash(cur_uri_hash);
			if (!stat(path.c_str(), &st)) {
				FILE* fd = fopen(path.c_str(), "rb");
				if (fd) {
					if ((reply = new char[st.st_size])) {
						if (fread(reply, 1, st.st_size, fd) == (size_t)st.st_size) {
							needsFree = true;
							reply_len = st.st_size;
							ci_http_response_add_header(req, (char*)"Content-Type: image/jpeg");
						} else {
							delete[] reply;
							reply = NULL;
						}
					}
					fclose(fd);
				}
			}
			if (!reply) {
				Logger::debug("Using empty image for reply");
				reply = (char*)denied_img;
				reply_len = sizeof(denied_img);
				ci_http_response_add_header(req, (char*)"Content-Type: image/png");
			}
			uc->body = ci_cached_file_new(reply_len);
		}

		if (!reply && iConf.getFilteringMode() != InfernoConf::F_MODE_IMAGE) {
			// Build the responce headers
			ci_http_response_add_header(req, (char*)"Content-Type: text/html");
			ci_http_response_add_header(req, (char*)"Content-Language: en");
			Logger::info("page was classified as BIKINI/PORN. Forwarding a denial page to the user");
			reply = const_cast<char *>(blocked_message);
			reply_len = strlen(blocked_message) + 1;
			uc->body = ci_cached_file_new(reply_len);
		}

		// send reply
		if (reply) {
			Logger::info("Sending data to the user (%d bytes)", reply_len);
			ci_cached_file_write(uc->body, reply, reply_len, 1);
			if (needsFree)
				delete[] reply;
		}
		return CI_MOD_CONTINUE;
	}

allow_request:
	uc->denied = 0;

	if (gotError)
		return CI_MOD_ERROR;

	if(!preview_data_len)
		return CI_MOD_CONTINUE;

	Logger::info("Returning a 204 response to the user anyway");
	return CI_MOD_ALLOW204;
}

int inferno_process(ci_request_t * req) {
	struct inferno_req_data *uc = (struct inferno_req_data *)ci_service_data(req);
	(void)req;

	uc->eof = 1;

	return CI_MOD_DONE;
}

int inferno_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof, ci_request_t * req) {
	struct inferno_req_data *uc = (struct inferno_req_data *)ci_service_data(req);
	int ret = CI_OK;

	if (uc->denied == 0) {
		if (rbuf && rlen) {
			if ((*rlen = ci_cached_file_write(uc->body, rbuf, *rlen, iseof)) == CI_ERROR)
				ret = CI_ERROR;
		} else if (iseof) {
			ci_cached_file_write(uc->body, NULL, 0, iseof);
		}
	}

	if (wbuf && wlen && (*wlen = ci_cached_file_read(uc->body, wbuf, *wlen)) == CI_ERROR)
		ret = CI_ERROR;

	if(*wlen == 0 && uc->eof == 1)
		*wlen = CI_EOF;

	return ret;
}
