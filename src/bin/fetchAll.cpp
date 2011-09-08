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
#include <iostream>
#include <set>

#include <curl/curl.h>
#include <mysql.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#include "dbcache.h"
#include "multifetch.h"
#include "logger.h"
#include "infernoconf.h"
#include "config.h"

#define MAX_LINE 10 * 1024

using namespace std;

struct UrlPacket {
	string url;
};

void *thread_function(void *arg);

int main(int argc, char **argv) {
	FILE *fp = NULL;
	char *p;
	char *buff;
	void *thread_result;

	int len;
	int res;

	set<string> url_set;
	set<string>::iterator it;

	pthread_t *t_id;

	// check if argument is given
	if(argc != 2) {
		fprintf(stderr, "Usage: %s <url_file>\n", argv[0]);
		return 1;
	}

	// preparing spool directory
	//res = DbCache::makeSpoolQueue();
	//if(!res) {
	//	BAIL("Error creating spool directory structure!");
	//	return 1;
	//}

	// open file stream
	fp = fopen(argv[1], "r");
	if(fp == NULL) {
		Logger::bail("Unable to open file based on the supplied argument");
	}

	if (!(buff = new char[MAX_LINE]))
		return 1;

	// read urls from file
	while(!feof(fp)) {
		bzero(buff, MAX_LINE);
		if(!(p = fgets(buff, MAX_LINE, fp)))
			break;

		len = strlen(p);
		p[len - 1] = '\0';

		Logger::debug("adding url %s to pool", buff);

		// add url to pool
		url_set.insert(buff);
	}
	delete[] buff;

	if(!url_set.size()) {
		Logger::bail("Input file contains no URLs");
	}

	Logger::debug("Initializing libmysqlclient");
	if(mysql_library_init(0, NULL, NULL)) {
		Logger::bail("Unable to initialize MySQL library");
	}

	Logger::debug("Allocating thread store");
	t_id = new pthread_t[url_set.size()];
	if(t_id == NULL) {
		Logger::bail("Unable to request memory for thread store");
	}
	memset(t_id, 0, url_set.size() * sizeof(pthread_t));

	// initialize libcurl
	curl_global_init(CURL_GLOBAL_DEFAULT);

	Logger::debug("Creating user-level threads");
	it = url_set.begin();
	for(int i = 0; it != url_set.end(); it++, i++) {
		// construct url packet
		struct UrlPacket *pkt = new struct UrlPacket;
		if(pkt == NULL) {
			perror("new");
			delete[] t_id;
			return 1;
		}

		pkt->url = *it;

		// create new thread for current url
		res = pthread_create(&(t_id[i]), NULL, thread_function, (void *)pkt);
		if(res != 0) {
			perror("pthread_create()");
			delete[] t_id;
			return 1;
		}
	}

	// call pthread_join for all threads
	for(int i = 0; i < (int)url_set.size(); i++) {
		res = pthread_join(t_id[i], &thread_result);
		if(!res) {
			if (thread_result == PTHREAD_CANCELED)
				fprintf(stderr, "Thread %d was canceled\n", i);
			else
				delete (struct UrlPacket*)thread_result;
		} else
			perror("pthread_join");
	}

	// clean-up memory
	delete[] t_id;
	fclose(fp);

	// XXX: commenting out since this call alone seems to take a couple
	//      of secs, all while not being required per the manual
	// mysql_library_end();
	return 0;
}

// thread function
void *thread_function(void *arg) {
	struct UrlPacket *pkt = (struct UrlPacket *)arg;

	FILE *fp;
	timeval tim;

	char buff[4096];
	double t1, t2;
	InfernoConf::Classification retval;

	Multifetch multifetch;

	fp = fopen("runtime.log", "a");
	if(fp == NULL) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	// show debug messages (check if UrlPacket was passed correctly)
	fprintf(stderr, "Thread '%lu' was assigned the url '%s'\n", pthread_self(), pkt->url.c_str());

	// calling extractlinks handler
	gettimeofday(&tim, NULL);
	t1 = tim.tv_sec + tim.tv_usec / 1000000.0;

	// call extractlinks on url
	string url_hash, ctype;
	retval = multifetch.extractlinks(pkt->url, url_hash, ctype);

	gettimeofday(&tim, NULL);
	t2 = tim.tv_sec + tim.tv_usec / 1000000.0;

	// check return value
	if(retval == InfernoConf::CLASS_ERROR) {
		fprintf(stderr, "Thread '%lu': extractlinks returned with FAILURE\n", pthread_self());
	} else {
		fprintf(stderr, "Thread '%lu': extractlinks returned with SUCCESS\n", pthread_self());
	}

	sprintf(buff, "%s\t%6.6lf sec\n", pkt->url.c_str(), (t2 - t1));
	fputs(buff, fp);

	fflush(fp);
	fclose(fp);

	return((void *)pkt);
}
