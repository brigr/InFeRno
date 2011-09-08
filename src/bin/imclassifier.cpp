#include <cerrno>
#include <fstream>
#include <iostream>
#include "sead.h"
#include "logger.h"

using namespace std;

void _usage(char *prog) {
	cerr << "Usage: " << prog << " [-q label] [-c img_file] <-l log_file>" << endl;
	exit(1);
}

#define usage() (_usage(argv[0]))

int main(int argc, char **argv) {
	Sead sead;
	Sead::IplImageFeature *feature = NULL;
	char *label = NULL, *img_file = NULL;
	ofstream *log_file = NULL;
	bool needsClose = false;
	int c;

	while ((c = getopt(argc, argv, "c:l:q:h")) != -1) {
		switch(c) {
			case 'q':
				if (!(label = strdup(optarg)))
					Logger::bail("Unable to allocate memory");
				break;
			case 'l':
				log_file = new ofstream((char*)optarg, ios_base::app);
				if (!log_file || log_file->fail())
					Logger::bail("Unable to open file %s", optarg);
				errno = 0;
				needsClose = true;
				break;
			case 'c':
				if (!(img_file = strdup(optarg)))
					Logger::bail("Unable to allocate memory");
				break;
			default:
				usage();
		}
	}
	if (!log_file)
		log_file = (ofstream*)&cout;
	if (!img_file || !label || !log_file)
		usage();

	errno = 0;
	if (!sead.init(img_file)) {
		feature = sead.process();
		if (!feature) {
			Logger::error("Got NULL feature");
		} else {
			(*log_file) << feature->cov_b << "," << feature->cov_g << "," << feature->cov_r << "," << feature->mean_r << "," << feature->mean_g << "," << feature->mean_b << "," << feature->skin_to_non_skin_area << "," << feature->hu.hu1 << "," << feature->hu.hu2 << "," << feature->hu.hu3 << "," << feature->hu.hu4 << "," << feature->hu.hu5 << "," << feature->hu.hu6 << "," << feature->hu.hu7 << "," << (label ? label : "0") << endl;
			delete feature;
		}
	}

	if (needsClose)
		delete log_file;

	free(label);
	free(img_file);

	return 0;
}
