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

#include <algorithm>
#include <iostream>

#include <uriparser/UriBase.h>
#include <uriparser/UriDefsAnsi.h>
#include <uriparser/UriDefsConfig.h>
#include <uriparser/UriDefsUnicode.h>
#include <uriparser/Uri.h>
#include <uriparser/UriIp4.h>

#include "htmlparse.h"
#include "logger.h"
#include "config.h"

using namespace std;

//
//  libxml start element callback function
//
void HTMLParser::StartElement(void *voidContext, const xmlChar *name, const xmlChar **attributes) {
	Context *ctx = (Context*)voidContext;

	if (!strcasecmp((char *)name, "IMG")) {
		string buff;
		for(int i = 0; attributes[i] != NULL; i++) {
			if(!strcasecmp((char *)attributes[i], "SRC")) {
				buff.append(recompose_url(ctx->global_base_prefix, (char *)attributes[i + 1]));
			}
		}

		if (!buff.empty())
			ctx->url_list->insert(buff);
	}
}

//
//  libxml end element callback function
//
void HTMLParser::EndElement(void *voidContext, const xmlChar *name) {
	(void)voidContext;
	(void)name;
}

const htmlSAXHandler HTMLParser::saxHandler = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	StartElement,
	EndElement,
	NULL,
	NULL, // was: Characters but that is an empty function
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // was: cdata but that is an empty function
	NULL,
	0,
	NULL,
	NULL,
	NULL,
	NULL
};

//
//  Parse given (assumed to be) HTML text and return the title
//
void HTMLParser::parseHtml(const string& htmlPath, const string& global_url_, set<string>& url_list) {
	htmlParserCtxtPtr ctxt;
	Context ctx;
	int parserErrors;
	char *buf;
	size_t nread;
	FILE *fp;
	static const size_t bufSize = 4096;

	ctx.global_base_prefix = global_url_;
	ctx.url_list = &url_list;

	if (!(buf = new char[bufSize])) {
		Logger::error("new");
		return;
	}

	Logger::warn("Base URL is: '%s'", ctx.global_base_prefix.c_str());
	if (!(fp = fopen(htmlPath.c_str(), "rb"))) {
		Logger::error("fopen");
		delete[] buf;
		return;
	}

	// parse HTML
	ctxt = htmlCreatePushParserCtxt((xmlSAXHandler*)&saxHandler, &ctx, NULL, 0, NULL, XML_CHAR_ENCODING_NONE);

	do {
		nread = fread(buf, 1, bufSize, fp);
		if ((parserErrors = htmlParseChunk(ctxt, buf, nread, nread != bufSize)))
			Logger::info("Parser error: %d", parserErrors);
	} while (nread == bufSize);

	if (ctxt->myDoc) {
		free(ctxt->myDoc);
		ctxt->myDoc = NULL;
	}
	htmlFreeParserCtxt(ctxt);
	url_list.erase(global_url_);
	fclose(fp);
	delete[] buf;
}

string HTMLParser::recompose_url(const string& baseUrl, const string& relativeUrl) {
	UriParserStateA state;

	UriUriA uriBase;
	UriUriA uriRelative;
	UriUriA uriAbsolute;

	char *uriString;
	int charsRequired;
	string retStr;
	string relUrl = relativeUrl;
	size_t pos;

	while ((pos = relUrl.find_first_of('\\')) != string::npos)
		relUrl.at(pos) = '/';

	// construct internal uriparser representation
	state.uri = &uriRelative;
	if(uriParseUriA(&state, relativeUrl.c_str()) != URI_SUCCESS) {
		uriFreeUriMembersA(&uriRelative);
	}

	state.uri = &uriBase;
	if(uriParseUriA(&state, baseUrl.c_str()) != URI_SUCCESS) {
		uriFreeUriMembersA(&uriRelative);
		uriFreeUriMembersA(&uriBase);
	}

	if(uriAddBaseUriA(&uriAbsolute, &uriRelative, &uriBase) != URI_SUCCESS) {
		uriFreeUriMembersA(&uriRelative);
		uriFreeUriMembersA(&uriBase);
	}

	if(uriToStringCharsRequiredA(&uriAbsolute, &charsRequired) != URI_SUCCESS) {
		uriFreeUriMembersA(&uriAbsolute);
	}
	charsRequired++;

	uriString = new char[charsRequired];
	if(uriString == NULL) {
		Logger::error("new");
	}

	if(uriToStringA(uriString, &uriAbsolute, charsRequired, NULL) != URI_SUCCESS) {
		Logger::error("uriToStringA");
	}

	uriFreeUriMembersA(&uriAbsolute);
	uriFreeUriMembersA(&uriRelative);
	uriFreeUriMembersA(&uriBase);
	
	retStr = uriString;
	if(uriString) delete[] uriString;

	return retStr;
}
