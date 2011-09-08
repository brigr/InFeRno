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
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <pthread.h>

#include "logger.h"
#include "config.h"

using namespace std;

const char* Logger::IDENT = "InFeRno";
bool Logger::logOpened = false;

void Logger::log(int priority, const string& format, va_list ap) {
	string fmt = format;

	if (!logOpened) {
		openlog(Logger::IDENT, LOG_CONS | LOG_PERROR | LOG_PID, LOG_LOCAL1);
		logOpened = true;
	}

	if (errno) {
		fmt.append(": ");
		fmt.append(strerror(errno));
		errno = 0;
	}

	string loglevel;
	switch (priority) {
		case LOG_INFO: loglevel = "INFO"; break;
		case LOG_NOTICE: loglevel = "NOTICE"; break;
		case LOG_WARNING: loglevel = "WARN"; break;
		case LOG_DEBUG: loglevel = "DEBUG"; break;
		case LOG_ERR: loglevel = "ERROR"; break;
		case LOG_CRIT: loglevel = "CRITICAL"; break;
		default: loglevel = "UNKNOWN";
	}
	fmt = "[" + loglevel + "] " + fmt + "\n";
#ifdef DEBUG
	vfprintf(stderr, fmt.c_str(), ap);
#else
	if (priority == LOG_CRIT || priority == LOG_ERR)
		vsyslog(priority, fmt.c_str(), ap);
#endif
}

void Logger::info(const string& format, ...) {
	va_list ap;
	va_start(ap, format);
	Logger::log(LOG_INFO, format, ap);
	va_end(ap);
}

void Logger::notice(const string& format, ...) {
	va_list ap;
	va_start(ap, format);
	Logger::log(LOG_NOTICE, format, ap);
	va_end(ap);
}

void Logger::warn(const string& format, ...) {
	va_list ap;
	va_start(ap, format);
	Logger::log(LOG_WARNING, format, ap);
	va_end(ap);
}

void Logger::debug(const string& format, ...) {
	va_list ap;
	va_start(ap, format);
	Logger::log(LOG_DEBUG, format, ap);
	va_end(ap);
}

void Logger::error(const string& format, ...) {
	va_list ap;
	va_start(ap, format);
	Logger::log(LOG_ERR, format, ap);
	va_end(ap);
}

void Logger::bail(const string& format, ...) {
	va_list ap;
	va_start(ap, format);
	Logger::log(LOG_CRIT, format, ap);
	va_end(ap);
	throw new runtime_error("see syslog");
}

