#include <set>
#include <string>
#include <vector>

// Bridge from XS to C++11 code.

// Perl XS header files collide with C++11 header files, so we have minimal
// C++ code on the XS file, and full blown C++11 code in Stasherapi.C
//
// The C++11 code traps exceptions. The general API is to capture exception
// errors as strings, and the XS shim returns the error code as the first
// return value from the syscall. The Perl wrapper checks if the first
// return value is a non-empty string, and dies if it is.

void api_defaultnodes(std::string &, std::set<std::string> &s);
void api_connect(std::string &, void *&, const char *, int, int);
void api_disconnect(void *&);

int api_request_done(void *);
void api_request_wait(void *);
void api_request_free(void *&);
void api_getrequest_free(void *&);
void api_putrequest_free(void *&);
void api_dirrequest_free(void *&);

void api_get_objects(void *, std::string &,
		     void *&, void *&,
		     int, std::vector<std::string> &);

void api_get_results(void *, std::vector<std::string> &);

void api_put_objects(void *, std::string &,
		     void *&, void *&,
		     std::vector<std::string> &);
void api_put_results(void *handle, std::string &,
		     std::string &);

void api_dir_request(void *, std::string &,
		     void *&, void *&,
		     const char *);

void api_getdir_results(void *, std::vector<std::string> &);

void api_limits(void *, std::vector<std::string> &);
