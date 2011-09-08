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

#ifndef __INFERNO_CONF_H__
#define __INFERNO_CONF_H__

#include <string>

class InfernoConf {
	public:
		enum FilteringMode {
			F_MODE_PAGE,
			F_MODE_IMAGE,
			F_MODE_MIXED
		};

	private:
		const static std::string CACHE_HOSTNAME;
		const static std::string CACHE_STORE;
		const static std::string CACHE_TABLE;
		const static std::string CACHE_UNAME;
		const static std::string CACHE_PASSWD;
		const static std::string CACHE_DIR;

		/**
		 * Limit on the number of redirection hops to follow, when the web
		 * server returns a 301 or 302.
		 */
		const static long REDIR_LIMIT;

		/**
		 * Minimum bytes/sec, over a period of LOW_SPEED_TIME, lower than
		 * which we will interrupt the transfer. Set to 0 to disable the
		 * minimum speed limit.
		 */
		const static long LOW_SPEED_LIMIT;

		/**
		 * Number of seconds over which to tollerate the minimum transfer
		 * speed.
		 */
		const static long LOW_SPEED_TIME;

		/**
		 * Number of seconds to wait per request for connect(2) to complete. A
		 * value of 0 means waiting for whatever the default is in libcurl.
		 */
		const static long CONNECT_TIMEOUT;

		/**
		 * Limit on the number of concurrent transfers to initiate (i.e.,
		 * transfer pool size). A value of 0 means no limit on the pool size.
		 */
		const static long MAX_CONC_XFERS;

		/**
		 * Time (in microseconds) to wait between successive polls for pending
		 * database items.
		 */
		const static long POLL_INTERVAL;

		/**
		 * Acceptance threshold of the proportion of pornographic and/or bikini
		 * images allowed to be viewed by a user
		 */
		const static long ACC_THRESH;

		/**
		 * Filtering mode of inferno: a) image-level filtering (blurring out images
		 * that are deemed as pornographic), b) page-wide
		 * filtering (blocking the whole content)
		 */
		const static FilteringMode FILTERING_MODE;


		std::string cache_host;
		std::string cache_store;
		std::string cache_table;
		std::string cache_uname;
		std::string cache_passwd;
		std::string cache_dir;

		long redir_limit;
		long conn_timeo;
		long max_xfers;
		long poll_interval;
		long low_speed_lim;
		long low_speed_time;
		long acc_thresh;
		FilteringMode f_mode;

	public:
		enum Classification {
			CLASS_ERROR,
			CLASS_PORN,
			CLASS_BENIGN,
			CLASS_BIKINI,
			CLASS_UNDEFINED,
		};

		enum Status {
			STATUS_ERROR,
			STATUS_FETCHING,
			STATUS_PROCESSING,
			STATUS_FETCHING_MORE,
			STATUS_CLASSIFYING,
			STATUS_DONE,
			STATUS_FAILURE,
		};

		/**
		 * List of known/handled image mime types.
		 */
		const static char *img_mimes[];
		const static char *img_ext[];

		InfernoConf() :
			cache_host(CACHE_HOSTNAME), cache_store(CACHE_STORE),
			cache_table(CACHE_TABLE), cache_uname(CACHE_UNAME),
			cache_passwd(CACHE_PASSWD), cache_dir(CACHE_DIR),
			redir_limit(REDIR_LIMIT), conn_timeo(CONNECT_TIMEOUT),
			max_xfers(MAX_CONC_XFERS), poll_interval(POLL_INTERVAL),
			low_speed_lim(LOW_SPEED_LIMIT), low_speed_time(LOW_SPEED_TIME),
			acc_thresh(ACC_THRESH), f_mode(FILTERING_MODE) {}

		InfernoConf(
				const std::string& ch, const std::string& cs, const
				std::string& ct, const std::string& cu, const std::string& cp,
				const std::string& cd, long rl, long cto, long mx, long pi,
				long ll, long lt, long at, FilteringMode fm) :
			cache_host(ch), cache_store(cs), cache_table(ct), cache_uname(cu),
			cache_passwd(cp), cache_dir(cd), redir_limit(rl), conn_timeo(cto),
			max_xfers(mx), poll_interval(pi), low_speed_lim(ll),
			low_speed_time(lt), acc_thresh(at), f_mode(fm) {}

		long getRedirLimit() const { return redir_limit; }
		long getConnTimeout() const { return conn_timeo; }
		long getMaxXfers() const { return max_xfers; }
		long getPollInterval() const { return poll_interval; }
		long getLowSpeedLimit() const { return low_speed_lim; }
		long getLowSpeedTime() const { return low_speed_time; }
		long getAcceptanceThreshold() const { return acc_thresh; }
		FilteringMode getFilteringMode() const { return f_mode; }
		std::string getHostname() const { return cache_host; }
		std::string getStore() const { return cache_store; }
		std::string getTable() const { return cache_table; }
		std::string getUsername() const { return cache_uname; }
		std::string getPassword() const { return cache_passwd; }
		std::string getDirectory() const { return cache_dir; }

		void setRedirLimit(long l) { redir_limit = l; }
		void setConnTimeout(long l) { conn_timeo = l; }
		void setMaxXfers(long l) { max_xfers = l; }
		void setPollInterval(long l) { poll_interval = l; }
		void setLowSpeedLimit(long l) { low_speed_lim = l; }
		void setLowSpeedTime(long l) { low_speed_time = l; }
		void setAcceptanceThreshold(long l) { acc_thresh = l; }
		void setFilteringMode(FilteringMode l) { f_mode = l; }
		void setHostname(std::string s) { cache_host = s; }
		void setStore(std::string s) { cache_store = s; }
		void setTable(std::string s) { cache_table = s; }
		void setUsername(std::string s) { cache_uname = s; }
		void setPassword(std::string s) { cache_passwd = s; }
		void setDirectory(std::string s) { cache_dir = s; }

		std::string computePathFromHash(const std::string& hash) const;
		std::string toString() const;
		static InfernoConf* parseString(const std::string&);
		static bool isImageContentType(const std::string&);
};

#endif
