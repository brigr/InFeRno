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

#include <cerrno>
#include <cstring>
#include <iostream>

#include "seadclient.h"
#include "multifetch.h"
#include "htmlparse.h"
#include "dbcache.h"
#include "logger.h"
#include "config.h"

using namespace std;

int Multifetch::consult_nimage_classifier(std::string hash, std::string type, SeadClient* sclient) {
	std::string path;
	SeadClient *sc = sclient;
	int ret;

	if (!sc) {
		sc = new SeadClient();
		if (!sc || sc->init() == SeadClient::SCL_FAILURE) {
			Logger::debug("Could not connect to image classifier network server!");
			return -1;
		}
	}

	// send classification request per uri
	ret = sc->classifyUri(hash, type, iConf);

	// return server's result
	if (!sclient)
		delete sc;
	return ret;
}

CURL* Multifetch::setupHandle(const string& url, const string& hash, FILE *& fp, char *errorBuffer) {
	// add new curl_easy
	CURL *handle;
	string path = iConf.computePathFromHash(hash);

	if (path.empty() || url.empty() || hash.empty())
		return NULL;

	if (!(fp = fopen(path.c_str(), "wb"))) {
		perror("fopen");
		return NULL;
	}
	setbuf(fp, NULL);

	if (!(handle = curl_easy_init())) {
		perror("curl_easy_init");
		return NULL;
	}

	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers,"Accept-Encoding: gzip, deflate");
	headers = curl_slist_append(headers,"User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/534.30 (KHTML, like Gecko) Chrome/12.0.742.112 Safari/534.30");

	if (errorBuffer && curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errorBuffer) != CURLE_OK)
		Logger::debug("Failed to set error buffer");

	// set the options (I left out a few, you'll get the point anyway)
	if (curl_easy_setopt(handle, CURLOPT_NOSIGNAL, (long)1) != CURLE_OK ||
			curl_easy_setopt(handle, CURLOPT_URL, url.c_str()) != CURLE_OK ||
			curl_easy_setopt(handle, CURLOPT_WRITEDATA, fp) != CURLE_OK ||
			curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, NULL) != CURLE_OK ||
			curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, true) != CURLE_OK ||
			(iConf.getRedirLimit() && curl_easy_setopt(handle, CURLOPT_MAXREDIRS, iConf.getRedirLimit()) != CURLE_OK) ||
			(iConf.getConnTimeout() && curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, iConf.getConnTimeout()) != CURLE_OK) ||
			(iConf.getLowSpeedLimit() && curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, iConf.getLowSpeedLimit()) != CURLE_OK) || 
			(iConf.getLowSpeedTime() && curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, iConf.getLowSpeedTime()) != CURLE_OK) ||
			curl_easy_setopt(handle, CURLOPT_ENCODING, "") != CURLE_OK ||
			curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers) != CURLE_OK) {
		Logger::debug("Failed setting options: %s", errorBuffer);
		curl_easy_cleanup(handle);
		return NULL;
	}

	return handle;
}

int Multifetch::fetch_multi_from_list(const set<string>& url_set, DbCache *cache) {
	int still_running = 1; /* keep number of running handles */
	int cur_idx, k, size, concur;
	int known_flag = 0;

	// non-cached urls
	set<pair<string, string> > indices;
	set<string> waitfor;

	// curl-specific handles
	CURLcode code;
	CURLMsg *msg; /* for picking up messages with the transfer status */

	int msgs_left; /* how many messages are left */

	// get size of the url pool
	size = url_set.size();
	if(!size) {
		// return to the caller; nothing to process in pool
		return 0;
	}

	set<string>::iterator uit = url_set.begin();
	for(int i = 0; i < size; i++, uit++) {
		// cache url if there is no caching entry for this url already
		string cur_hash;
		int err = cache->insertUrlEntry(*uit, cur_hash);
		switch (err) {
			case 1:
				indices.insert(pair<string, string>(*uit, cur_hash));
				break;
			case -1:
				Logger::info("URL %s already in cache. Skipping...", uit->c_str());
				waitfor.insert(cur_hash);
				break;
			case 0:
			default:
				Logger::warn("Error inserting new URL entry on cache (Error report: %s). A request for this url may be pending for some other user...", cache->getErrorString());
				continue;
		}
	}

	// get pool size of newly-cached urls
	size = indices.size();
	concur = ((iConf.getMaxXfers() > 0) ? ((size > iConf.getMaxXfers()) ? iConf.getMaxXfers() : size) : size);

	// constructing network I/O handlers for each newly-inserted image url in the image pool
	CURL **handles = NULL;
	string *handleURLs = NULL;
	DbCache **caches = NULL;
	FILE **fps = NULL;
	CURLM *multi_handle = NULL;

	if (!(handles = new CURL*[concur]) ||
			!(handleURLs = new string[concur]) ||
			!(caches = new DbCache *[concur]) ||
			!(fps = new FILE *[concur]) ||
			!(multi_handle = curl_multi_init())) {
		Logger::error("new");

		for(set<pair<string, string> >::iterator uit = indices.begin(); uit != indices.end(); uit++)
			cache->updateUrlStatus(uit->second, InfernoConf::STATUS_FAILURE);

		if (handles)
			delete[] handles;
		if (handleURLs)
			delete[] handleURLs;
		if (caches)
			delete[] caches;
		if (fps)
			delete[] fps;
		if (multi_handle)
			curl_multi_cleanup(multi_handle);
		return 0;
	}

	set<pair<string, string> >::iterator it = indices.begin();
	for(cur_idx = 0; cur_idx < concur; cur_idx++, it++) {
		// associate a new cache server connection with handle
		// XXX: examine pooling or sharing to avoid a new connection per thread
		caches[cur_idx] = new DbCache();
		if (!caches[cur_idx] || caches[cur_idx]->init(iConf)) {
			Logger::error("Unable to initialize database client");
			pthread_exit(NULL);
		}

		// add new curl_easy
		handles[cur_idx] = setupHandle(it->first, it->second, fps[cur_idx]);
		handleURLs[cur_idx] = it->second;
		curl_multi_add_handle(multi_handle, handles[cur_idx]);
	}

	Logger::debug("Initiating concurrent downloads");
	do {
		struct timeval timeout;
		int rc; // select() return code

		fd_set fdread;
		fd_set fdwrite;
		fd_set fdexcep;

		while (still_running) {

			int maxfd = -1;
			long curl_timeo = -1;

			FD_ZERO(&fdread);
			FD_ZERO(&fdwrite);
			FD_ZERO(&fdexcep);

			for (CURLMcode retCode = CURLM_CALL_MULTI_PERFORM;
					retCode == CURLM_CALL_MULTI_PERFORM;
					retCode = curl_multi_perform(multi_handle, &still_running)) {}

			// set a suitable timeout to play around with
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;

			curl_multi_timeout(multi_handle, &curl_timeo);

			if(curl_timeo >= 0) {
				timeout.tv_sec = curl_timeo / 1000;
				timeout.tv_usec = (curl_timeo % 1000) * 1000;
			}

			/* get file descriptors from the transfers */
			curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

			/* In a real-world program you OF COURSE check the return code of the
			   function calls.  On success, the value of maxfd is guaranteed to be
			   greater or equal than -1.  We call select(maxfd + 1, ...), specially in
			   case of (maxfd == -1), we call select(0, ...), which is basically equal
			   to sleep. */

			rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
			switch(rc) {
				case -1:
					/* select error */
					if (errno != EINTR) {
						perror("select");
						for(int x = 0; x < concur; x++)
							if (caches[x])
								delete caches[x];
						delete[] caches;
						for(int x = 0; x < concur; x++)
							if (fps[x])
								fclose(fps[x]);
						delete[] fps;
						delete[] handles;
						delete[] handleURLs;
						return 0;
					}
					break;
				case 0: /* timeout */
				default: /* action */
					curl_multi_perform(multi_handle, &still_running);
					break;
			}

			// XXX: Do this in-line to avoid waiting for all the transfers to complete before starting the classification chores
			while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
				if (msg->msg == CURLMSG_DONE) { // XXX: no other msg types defined at this time per curl_multi_info_read(3)
					CURL *cur_handle = msg->easy_handle;
					int idx;

					// get index of current handle in the handles store
					for (idx = 0; idx < concur && cur_handle != handles[idx]; idx++) {}
					const char* cur_url = handleURLs[idx].c_str();

					Logger::debug("HTTP transfer for %s completed with status %d", (cur_url ? cur_url : "[N/A]"), msg->data.result);

					if (msg->data.result == CURLE_OK) {
						double cl; // content length buffer
						char *ct = NULL;
						long http_code = 0;

						// get content length header field that the server sent to us
						code = curl_easy_getinfo(cur_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);

						// get content type field that the server sent to us
						code = curl_easy_getinfo(cur_handle, CURLINFO_CONTENT_TYPE, &ct);
						if (ct)
							cache->updateUrlContentType(cur_url, ct);

						// get HTTP response code
						code = curl_easy_getinfo(cur_handle, CURLINFO_RESPONSE_CODE, &http_code);

						// see if we had a successful transfer
						//XXX: successful transfers are always indicated by a HTTP 200 response code
						if(http_code == 200 && code != CURLE_ABORTED_BY_CALLBACK) {
							//Logger::debug("URL '%s' fetched with a HTTP 200 response code. SUCCESS!", cur_url.c_str());

							//XXX: or see if HTTP headers are missing
						} else if(ct == NULL && http_code != 200) {
							Logger::warn("URL %s without headers (Content-type: null, HTTP code: %d)! Probing next image...", cur_url, http_code);
							Logger::debug("Updating image status for %s to 'FAILURE'", cur_url);
							if(!caches[idx]->updateUrlStatus(cur_url, InfernoConf::STATUS_FAILURE)) {
								Logger::error("Error updating image URL status to 'FAILURE'. Error report: %s", caches[idx]->getErrorString());
							}

							goto cleanup_curl_handle;
						}

						/* detrmine mime-type of fetched object */
						for(k = known_flag = 0; (InfernoConf::img_mimes[k] != NULL) && (known_flag == 0); k++) {
							if(!strncasecmp(ct, InfernoConf::img_mimes[k], strlen(InfernoConf::img_mimes[k]))) {
								known_flag = 1;
							}
						}

						if(!known_flag) {
							Logger::warn("The remote web server included an unknown image MIME-type (%s) for this object. Ignoring item", ct);
							Logger::debug("Updating image status for '%s' to 'FAILURE'", cur_url);
							if(!caches[idx]->updateUrlStatus(cur_url, InfernoConf::STATUS_FAILURE)) {
								Logger::error("Error updating image URL status to 'FAILURE'. Error report: %s", caches[idx]->getErrorString());
							}

							goto cleanup_curl_handle;
						}

						Logger::debug("Updating image status for '%s' to 'PROCESSING'", cur_url);

						if(!caches[idx]->updateUrlStatus(cur_url, InfernoConf::STATUS_PROCESSING)) {
							Logger::error("Error updating image status to PROCESSING. Error report: %s", caches[idx]->getErrorString());
							caches[idx]->updateUrlStatus(cur_url, InfernoConf::STATUS_FAILURE);
							goto cleanup_curl_handle;
						}


						Logger::debug("Updating image classification status for '%s' to 'CLASSIFYING'", cur_url);
						if(!caches[idx]->updateUrlStatus(cur_url, InfernoConf::STATUS_CLASSIFYING)) {
							Logger::error("Error updating status of image url. Error report: %s", caches[idx]->getErrorString());
							caches[idx]->updateUrlStatus(cur_url, InfernoConf::STATUS_FAILURE);
							goto cleanup_curl_handle;
						}

						if (fclose(fps[idx])) {
							Logger::error("fclose");
						}
						fps[idx] = NULL;
						//XXX: invoke classifier here
						Logger::debug("Invoking classifier for this image...");

						// request the network image classifier to
						// classify image per path
						if (consult_nimage_classifier(cur_url, InfernoConf::img_ext[k-1], NULL)) {
							Logger::debug("Updating image status for '%s' to 'FAILURE'", cur_url);
							if(!caches[idx]->updateUrlStatus(cur_url, InfernoConf::STATUS_FAILURE)) {
								Logger::error("Error updating image URL status to 'FAILURE'. Error report: %s", caches[idx]->getErrorString());
								goto cleanup_curl_handle;
							}
						} else
							waitfor.insert(cur_url);
					} else {
						Logger::debug("Updating image status for '%s' to 'FAILURE'", cur_url);
						if(!caches[idx]->updateUrlStatus(cur_url, InfernoConf::STATUS_FAILURE)) {
							Logger::error("Error updating image URL status to 'FAILURE'. Error report: %s", caches[idx]->getErrorString());
						}
					}

cleanup_curl_handle:
					// remove multi handle and clean-up current i/o handler

					if (it != indices.end()) {
						delete caches[idx];
						caches[idx] = new DbCache();
						if (!caches[idx] || caches[idx]->init(iConf)) {
							Logger::error("Unable to initialize database client");
							pthread_exit(NULL);
						}
						if (fps[idx])
							fclose(fps[idx]);
						handles[idx] = setupHandle(it->first, it->second, fps[idx]);
						handleURLs[idx] = it->second;
						curl_multi_add_handle(multi_handle, handles[idx]);
						cur_idx++;
						it++;
					}

					curl_multi_remove_handle(multi_handle, cur_handle);
					curl_easy_cleanup(cur_handle);
				}
			}
		}
	} while (still_running);
	// cleaning up multi handler
	curl_multi_cleanup(multi_handle);

	/* cleaning up cache connections */
	for (int i = 0; i < concur; i++)
		if (caches[i])
			delete caches[i];
	delete[] caches;
	for(int x = 0; x < concur; x++)
		if (fps[x]) {
			fclose(fps[x]);
		}
	delete[] fps;
	delete[] handles;
	delete[] handleURLs;

	Logger::debug("Exiting multithreaded image downloader routine with %d out of %d handles done", cur_idx, size);

	while (waitfor.size()) {
		for (set<string>::iterator it = waitfor.begin(); it != waitfor.end(); ) {
			InfernoConf::Status status = cache->lookupUrlStatus(*it);
			InfernoConf::Classification cres;
			switch (status) {
				case InfernoConf::STATUS_DONE:
					cres = cache->lookupUrlClassification(*it);
					switch (cres) {
						case InfernoConf::CLASS_PORN:
							porn_count++;
							break;
						case InfernoConf::CLASS_BENIGN:
							benign_count++;
							break;
						case InfernoConf::CLASS_BIKINI:
							bikini_count++;
							break;
						default:
							;
					}
				case InfernoConf::STATUS_FAILURE:
				case InfernoConf::STATUS_ERROR:
					Logger::debug("Removing %s request for " + *it + " from wait list", ((status == InfernoConf::STATUS_DONE) ? "successfull" : "failed"));
					waitfor.erase(it++);
					break;
				default:
					it++;
			}
		}
		if (waitfor.size())
			usleep(iConf.getPollInterval());
	}

	return 1;
}

InfernoConf::Classification Multifetch::extractlinks(const string& url_pt, string& url_pt_hash, string& ctype) {
	// local variables
	CURL *conn = NULL;
	CURLcode code;
	DbCache *cache;
	InfernoConf::Classification ret = InfernoConf::CLASS_ERROR;
	int status;
	FILE *htmlFile;
	double p_ratio = 0;

	if (url_pt.empty())
		return InfernoConf::CLASS_ERROR;

	ctype = "";
	//  libcurl variables for error strings and returned data
	char *errorBuffer = new char[CURL_ERROR_SIZE];
	string buffer;

	// create new cache client
	Logger::debug("Establishing connection to caching server");
	cache = new DbCache();
	if(cache->init(iConf) || !cache->connect()) {
		delete cache;
		delete[] errorBuffer;
		Logger::error("Could not connect to caching server. Error report: %s", cache->getErrorString());
		return InfernoConf::CLASS_ERROR;
	}

	if((status = cache->insertUrlEntry(url_pt, url_pt_hash)) == -1) {
		Logger::debug("An entry already exists in cache for the entered URL. Delegating content to the user according to previous classification");

		InfernoConf::Status dbstatus;
		while ((dbstatus = cache->lookupUrlStatus(url_pt_hash)) != InfernoConf::STATUS_DONE && dbstatus != InfernoConf::STATUS_FAILURE && dbstatus != InfernoConf::STATUS_ERROR)
			usleep(iConf.getPollInterval());

		// see what are previous classification was about this url
		InfernoConf::Classification ret = cache->lookupUrlClassification(url_pt_hash);
		ctype = cache->lookupUrlContentType(url_pt_hash);

		switch(ret) {
			case InfernoConf::CLASS_PORN:
			case InfernoConf::CLASS_BIKINI:
				Logger::debug("URL was deemed as either pornographic or as containing soft porn...");
				//TODO: implement c-icap I/O here
				break;
			case InfernoConf::CLASS_BENIGN:
				Logger::debug("URL was deemed as BENIGN; nothing to block here. Forwarding content to the user.");
				break;
			case InfernoConf::CLASS_UNDEFINED:
				Logger::debug("Classification not yet complete.");
				break;
			case InfernoConf::CLASS_ERROR:
			default:
				Logger::debug("Error occured in classification.");
		}

		// terminate caching server connection
		delete cache;
		delete[] errorBuffer;
		return ret;
	} else if (status == 0) {
		delete cache;
		delete[] errorBuffer;
		Logger::error("Error inserting fresh URL entry on cache. Error report: %s", cache->getErrorString());
		return InfernoConf::CLASS_ERROR;
	}

	// initialize connection to the remote web server
	if (!(conn = setupHandle(url_pt, url_pt_hash, htmlFile, errorBuffer))) {
		Logger::debug("initConnection: connection initialization failed");
		delete cache;
		delete[] errorBuffer;
		return InfernoConf::CLASS_ERROR;
	}

	Logger::debug("Caching new URL entry, and updating URL's entry status to 'FETCHING'");
	// fetch content from the remote web server pointed to by the input URL
	code = curl_easy_perform(conn);
	fclose(htmlFile);
	if(code != CURLE_OK) {
		Logger::error("curl_easy_perform: failed to fetch contents of '%s' [error: '%s']", url_pt.c_str(), errorBuffer);
		cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_FAILURE);
		delete cache;
		delete[] errorBuffer;
		return InfernoConf::CLASS_ERROR;
	}

	double contentLength;
	code = curl_easy_getinfo(conn, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
	if (code != CURLE_OK) {
		Logger::error("curl_easy_getinfo: failed to fetch content length of '%s' [error: '%s']", url_pt.c_str(), errorBuffer);
		cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_FAILURE);
		delete cache;
		delete[] errorBuffer;
		return InfernoConf::CLASS_ERROR;
	}

	/* at this point we need to check if the remote object is an HTML page or an image, or something else... */
	char *ct;
	int i;

	code = curl_easy_getinfo(conn, CURLINFO_CONTENT_TYPE, &ct);
	if((code == CURLE_OK) && ct) {
		Logger::warn("Received response included the content-type header value: '%s'", ct);
		cache->updateUrlContentType(url_pt_hash, ct);
		ctype = ct;

		/* let us see if we can match the server content-type with any image related MIME type */
		for(i = 0; InfernoConf::img_mimes[i] != NULL; i++) {
			if(!strncasecmp(ct, InfernoConf::img_mimes[i], strlen(InfernoConf::img_mimes[i]))) {
				// updating image url's status to 'classifying'
				Logger::debug("The remote object seems to be an image. Updating image url's status to 'CLASSIFYING'");

				if(!cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_CLASSIFYING)) {
					Logger::error("Error updating HTML page's status to CLASSIFYING. Error report: %s", cache->getErrorString());
					cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_FAILURE);
				}

				//TODO: invoke classifier routine here
				Logger::debug("Classifying image...");

				// given that we have the image classified, update cache with the score
				InfernoConf::Classification cres;
				if (consult_nimage_classifier(url_pt_hash, InfernoConf::img_ext[i])) {
					Logger::debug("Updating image status to 'FAILURE'");
					if(!cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_FAILURE)) {
						Logger::error("Error updating url's status to 'FAILURE'");
					}
					cres = InfernoConf::CLASS_ERROR;
				} else {
					bool waiting = true;
					while (waiting) {
						InfernoConf::Status status = cache->lookupUrlStatus(url_pt_hash);
						switch (status) {
							case InfernoConf::STATUS_DONE:
								cres = cache->lookupUrlClassification(url_pt_hash);
								switch (cres) {
									case InfernoConf::CLASS_PORN:
										porn_count++;
										break;
									case InfernoConf::CLASS_BENIGN:
										benign_count++;
										break;
									case InfernoConf::CLASS_BIKINI:
										bikini_count++;
										break;
									default:
										;
								}
							case InfernoConf::STATUS_FAILURE:
							case InfernoConf::STATUS_ERROR:
								waiting = false;
								break;
							default:
								;
						}
						usleep(iConf.getPollInterval());
					}
				}

				Logger::debug("Delegating image to the user based on the classification (%d)", cres);

				delete cache;
				delete[] errorBuffer;
				return cres;
			}
		}
	}

	if(ct && strncasecmp(ct, "text/html", 9)) {
		// update object's url status to classifying
		Logger::debug("Updating url status of the object to CLASSIFYING");
		if(!cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_CLASSIFYING)) {
			Logger::error("Error updating HTML page's status to CLASSIFYING. Error report: %s", cache->getErrorString());
			cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_FAILURE);
			ret = InfernoConf::CLASS_ERROR;
		} else {
			//TODO: since the object is not a html page or image, we forward it to the user anyway
			Logger::debug("Updating object's URL score as 'BENIGN' on cache, anyway!");
			ret = InfernoConf::CLASS_BENIGN;
			if(!cache->updateUrlClassification(url_pt_hash, ret)) {
				Logger::error("Error updating score to BENIGN. Error report: %s", cache->getErrorString());
				ret = InfernoConf::CLASS_ERROR;
			} else {
				// update status of the object's url to done
				Logger::debug("Updating url's status to 'DONE'");
				if(!cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_DONE)) {
					Logger::error("Error updating url's status. Error report: %s", cache->getErrorString());
					cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_FAILURE);
					ret = InfernoConf::CLASS_ERROR;
				}
			}
		}

		//TODO: invoke c-icap I/O here
		Logger::debug("Forwarding content to the user anyway!");

		//TODO: terminate user session in terms of c-icap calls here
		delete cache;
		delete[] errorBuffer;
		return ret;
	}

	set<string> url_list;

	// invoke parser
	HTMLParser::parseHtml(iConf.computePathFromHash(url_pt_hash), url_pt, url_list);

	Logger::info("Determined URL pool size is = %d", (int)url_list.size());
	Logger::info("Dumping URLs in image pool:");

	// iterate all references image urls in the dom model of the HTML page
	for(set<string>::iterator it = url_list.begin(); it != url_list.end(); it++)
		Logger::info("\t%s", it->c_str());

	// fetch all image URLs based on the SRC attribute of any IMG tag
	if(url_list.size() == 0) {
		// update url status to classifying
		Logger::debug("Updating page's URL status to CLASSIFYING");
		if(!cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_CLASSIFYING)) {
			Logger::error("Error updating HTML page's status to CLASSIFYING. Error report: %s", cache->getErrorString());
			ret = InfernoConf::CLASS_ERROR;
		} else {
			// update web page score
			Logger::debug("HTML page contains no images... Setting URL as benign!");
			ret = InfernoConf::CLASS_BENIGN;
			if(!cache->updateUrlClassification(url_pt_hash, ret)) {
				Logger::error("Error updating URL's fusion to 'BENIGN'. Error report: %s", cache->getErrorString());
				ret = InfernoConf::CLASS_ERROR;
			} else {
				// update url status to DONE
				Logger::debug("Updating web page's URL status to DONE");
				if(!cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_DONE)) {
					Logger::error("Error updating URL's status to 'DONE'. Error report: %s", cache->getErrorString());
					ret = InfernoConf::CLASS_ERROR;
				}
			}
		}

		//TODO: invoke c-icap I/O in order to send content back to the user
		Logger::debug("Forwarding page to the user...");

		//TODO: terminate current user session
		Logger::debug("Cleaning up session...");
		delete cache;
		delete[] errorBuffer;
		return ret;
	}

	// set web page status to processing
	Logger::debug("Updating HTML page's url status to FETCHING_MORE");
	if(!cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_FETCHING_MORE)) {
		Logger::error("Error updating web page's status to FECHING_MORE. Error report: %s", cache->getErrorString());
		cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_FAILURE);
		goto terminate_session;
	}


	// call multithreaded image download manager
	Logger::debug("Invoking multithreaded image download manager for these images...");
	fetch_multi_from_list(url_list, cache);

	// fuse web page
	Logger::debug("Fusing scores of images into a page-wide classification.");

	// set web page status to 'classifying'
	Logger::debug("Updating HTML page's url status to CLASSIFYING");
	if(!cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_CLASSIFYING)) {
		Logger::error("Error updating web page's status to CLASSIFYING. Error report: %s", cache->getErrorString());
		cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_FAILURE);
		goto terminate_session;
	}

	// update web page score based on the decision obtained from fusion
	Logger::debug("Updating url score to some category based on our fusion step (porn: %ld, benign: %ld, bikini: %ld)", porn_count, benign_count, bikini_count);

	p_ratio = ((double)porn_count + (double)bikini_count) / ((double)porn_count + (double)benign_count + (double)bikini_count);

	Logger::debug("Nudity acceptance threshold is set to %d", iConf.getAcceptanceThreshold());

	if(p_ratio > (double)iConf.getAcceptanceThreshold() / 100.0) {
		Logger::debug("Going to classify requested web page as PORN");
		ret = InfernoConf::CLASS_PORN;
	} else {
		Logger::debug("Going to classify requested web page as BENIGN");
		ret = InfernoConf::CLASS_BENIGN;
	}

	if(!cache->updateUrlClassification(url_pt_hash, ret)) {
		Logger::error("Error updating web page's classification score. Error report: %s", cache->getErrorString());
		cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_FAILURE);
	} else {
		// update web page's status
		Logger::debug("Updating web page's status to DONE");
		if(!cache->updateUrlStatus(url_pt_hash, InfernoConf::STATUS_DONE)) {
			Logger::error("Error updating web page's status to DONE. Error report: %s", cache->getErrorString());
		}
	}

terminate_session:
	//TODO: terminate current session via c-icap
	Logger::debug("Terminating session...");
	delete cache;
	delete[] errorBuffer;

	// clean-up curl
	curl_easy_cleanup(conn);

	// exit broker
	return ret;
}
