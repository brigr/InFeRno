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

#ifndef __MY_HTMLPARSE_H__
#define __MY_HTMLPARSE_H__

#include <set>
#include <string>

#include <libxml/HTMLparser.h>

#include "config.h"

class HTMLParser {
	private:
		const static char *ximg_cts[];
		const static char *ximg_cts_ext[];
		const static htmlSAXHandler saxHandler;

		class Context {
			public:
				int x;
				std::set<std::string>* url_list;
				std::string global_base_prefix;
		};

		static void StartElement(void *voidContext, const xmlChar *name, const xmlChar **attributes);
		static void EndElement(void *voidContext, const xmlChar *name);

	public:
		static std::string recompose_url(const std::string& baseUrl, const std::string& relativeUrl);
		static void parseHtml(const std::string&, const std::string&, std::set<std::string>&);
};

#endif
