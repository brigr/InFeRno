#include <string.h>
#include <string>
#include <vector>

#include <uriparser/UriBase.h>
#include <uriparser/UriDefsAnsi.h>
#include <uriparser/UriDefsConfig.h>
#include <uriparser/UriDefsUnicode.h>
#include <uriparser/Uri.h>
#include <uriparser/UriIp4.h>

#include <libcroco/libcroco.h>

#include "sac-parser.h"

using namespace std;

CSSParser::CSSParser() {
}

string CSSParser::recompose_url(const string& baseUrl, const string& relativeUrl) {
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
		//Logger::error("new");
	}

	if(uriToStringA(uriString, &uriAbsolute, charsRequired, NULL) != URI_SUCCESS) {
		//Logger::error("uriToStringA");
	}

	uriFreeUriMembersA(&uriAbsolute);
	uriFreeUriMembersA(&uriRelative);
	uriFreeUriMembersA(&uriBase);
	
	retStr = uriString;
	if(uriString) delete[] uriString;

	return retStr;
}

void CSSParser::property_selector_cb(CRDocHandler *a_this, CRString *a_name, CRTerm *a_expression, gboolean a_is_important) {
	(void)a_this;
	(void)a_is_important;
	// find a 'background-image' directive in the current ruleset
	if(!strcmp((char *)cr_string_dup2(a_name), "background-image")) {
		printf("image reference '%s' :", (char *)cr_string_dup2(a_name));
		
		// extract url from url('') function
		char *u = (char *)malloc(1024*sizeof(char)); //FIXME: max url size??
		char *c;
		
		// zero buffer
		memset(u, 0, 1024);
		
		// skip the first 4 bytes, and append all the rest to the buffer
		strcat(u, (char *)cr_term_to_string(a_expression) + 4);
		
		// remove trailing right parenthesis
		c = u;
		while(*c++) {
		   if(*c == ')') // strip trailing parenthesis
		      *c = '\0';
		}
		
		// output url parameter
		printf(" '%s'\n", CSSParser::recompose_url("http://www.dummyurl.gr/", u).c_str());
		
		// free mem block
		if(u != NULL)
			free(u);
	}
}

void CSSParser::start_selector_cb (CRDocHandler *a_handler, CRSelector *a_selector) {
	/*  in new rule set; do nothing here */
	(void)a_handler;
	(void)a_selector;
}

void CSSParser::end_selector_cb (CRDocHandler *a_handler, CRSelector *a_selector) {
	/* in end of rule set; do nothing here also */
	(void)a_handler;
	(void)a_selector;
}

int CSSParser::parse_css_file(string baseUrl, string filePath) {
	(void)baseUrl;
	CRParser * parser = NULL ;
	CRDocHandler *sac_handler = NULL ;

	FILE *fp;
	char *buff;
	int len;
	
	fp = fopen((char *)filePath.c_str(), "r");
	if(fp == NULL) {
	   fprintf(stderr, "could not open file!\n");

	   return 1;
	}
	
	buff = (char *)malloc(4096 * sizeof(char));
	len = fread(buff, 1, 4096, fp);

	// initialize libcroco handler
	
	// FIXME: directly open CSS file?
	//parser = cr_parser_new_from_file(file_path, CR_ASCII);
	parser = cr_parser_new_from_buf((guchar *)buff, (gulong)len, CR_ASCII, (gboolean)TRUE);
	if (!parser) {
	   return -1;
	}
	
	// create new SAC document handler
	sac_handler = cr_doc_handler_new () ;
	if (!sac_handler) {
       cr_parser_destroy (parser) ;
	   return -1;
	}

	// set callback handlers
	sac_handler->start_selector = start_selector_cb ;
	sac_handler->end_selector = end_selector_cb ;
	sac_handler->property = property_selector_cb;
	
	// connect callback handlers and do the parsing
	cr_parser_set_sac_handler (parser, sac_handler) ;
	cr_parser_parse (parser) ;
	
	// free allocated resources
	cr_parser_destroy (parser) ;
	cr_doc_handler_unref (sac_handler) ;

	return 0 ;
}

int main(int argc, char **argv) {
	if(argc < 2) {
		fprintf(stderr, "%s <path/to/css/file.css>\n", argv[0]);
		return -1;
	}
	
	CSSParser *parser = new CSSParser();
	parser->parse_css_file(string("http://www.google.com/"), string(argv[1]));
	
	return 1;
}
