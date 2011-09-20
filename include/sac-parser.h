#include <string>

using namespace std;

class CSSParser {
	private:
		struct CSSParserCtx {
			public:
			   int open;
		       int close;
			   vector<char*> url;
			   std::string base_url;
		};
		
		// convenience functions
		static std::string recompose_url(const string& baseUrl, const string& relativeUrl);
		
		// handlers
		static void property_selector_cb(CRDocHandler *a_this, CRString *a_name, CRTerm *a_expression, gboolean a_is_important);
		static void start_selector_cb (CRDocHandler *a_handler, CRSelector *a_selector);
		static void end_selector_cb (CRDocHandler *a_handler, CRSelector *a_selector);

	public:
		CSSParser();
		int parse_css_file(string base_url, string file_path);
};
