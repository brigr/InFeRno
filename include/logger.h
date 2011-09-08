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

#ifndef __MY_DEBUG_H__
#define __MY_DEBUG_H__

#include <cstdarg>
#include <string>
#include <syslog.h>

class Logger {
	private:
		const static char* IDENT;
		static bool logOpened;
		static void log(int priority, const std::string& format, va_list ap);

	public:
		static void info(const std::string& format, ...);
		static void notice(const std::string& format, ...);
		static void warn(const std::string& format, ...);
		static void debug(const std::string& format, ...);
		static void error(const std::string& format, ...);
		static void bail(const std::string& format, ...);
};

#endif
