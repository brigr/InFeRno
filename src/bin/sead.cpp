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
#include <clocale>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <queue>
#include <unistd.h>

#include <libgen.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <libsvm/svm.h>
#include <highgui.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gthread.h>

#include "sead.h"
#include "logger.h"
#include "dbcache.h"
#include "mynetlib.h"
#include "multifetch.h"

using namespace std;

#define MODEL_FILE_PORN     ((char *)"porn.model")
#define MODEL_FILE_BIKINI   ((char *)"bikini.model")
#define MODEL_FILE_BENIGN   ((char *)"benign.model")

static struct svm_model* porn_model = NULL;
static struct svm_model* bikini_model = NULL;
static struct svm_model* benign_model = NULL;

static unsigned POOLSIZE = 1;
static const int BUFSIZE = 1024;

typedef struct {
	string path; // resource URI
	string hash; // resource hash
	string type; // type of resource
	InfernoConf::FilteringMode f_mode;
} Resource;

typedef struct {
	ClientEndpoint* endpoint; // client endpoint associated with job
	Resource resrc; // resource handle of current job
	InfernoConf* iConf;
} WorkerJob;

static string basedir;

// inter-thread conditional variable
sem_t mutex;
sem_t fillCount;

queue<WorkerJob *> reqs; // shared job queue
pthread_t tid; // producer thread identifier
pthread_t *workers = NULL; // producer thread store
ServerEndpoint* srv; // server endpoint

using namespace std;

void sig_int(int sig) {
	int ret;

	(void)sig;

	Logger::debug("Received signal. Going to kill daemon!");

	// kill worker threads
	Logger::debug("Stopping worker threads");

	for(unsigned i = 0; i < POOLSIZE; i++) {
		ret = pthread_kill(workers[i], SIG_BLOCK);
		if(ret != 0) {
			Logger::debug("pthread_kill() failed");
			exit(EXIT_FAILURE);
		}
	}

	// kill producer detached thread
	Logger::debug("Killing producer thread");
	ret = pthread_kill(tid, SIG_BLOCK);
	if(ret != 0) {
		Logger::debug("Shooting attempt against producer thread failed!");
		exit(EXIT_FAILURE);
	}

	Logger::debug("All threads are killed!");

	// close server endpoint
	Logger::debug("Closing server endpoint...");
	delete srv;

	Logger::debug("Destroying shared queue resource");
	bool cacheCleaned = false;
	while (!reqs.empty()) {
		DbCache cache;
		WorkerJob* front = reqs.front();
		reqs.pop();
		// XXX: Assuming that state for all requests is stored in the same database
		if (!cacheCleaned && !cache.init(*front->iConf)) {
			cache.fixCache();
			cacheCleaned = true;
		}
		delete front;
	}

	sem_destroy(&mutex);
	sem_destroy(&fillCount);

	if (porn_model)
#if defined(LIBSVM_VERSION) && (LIBSVM_VERSION >= 310)
		svm_free_and_destroy_model(&porn_model);
#else
		svm_destroy_model(porn_model);
#endif

	if (bikini_model)
#if defined(LIBSVM_VERSION) && (LIBSVM_VERSION >= 310)
		svm_free_and_destroy_model(&bikini_model);
#else
		svm_destroy_model(bikini_model);
#endif

	if (benign_model)
#if defined(LIBSVM_VERSION) && (LIBSVM_VERSION >= 310)
		svm_free_and_destroy_model(&benign_model);
#else
		svm_destroy_model(benign_model);
#endif
	exit(EXIT_SUCCESS);
}

// parse user request and try to infer a known request type and extract
// request information from it; return a 'resource' handle associated
// with the actual request
int frisk_user_request_str(const string& req, Resource& res, InfernoConf*& iConf) {
	stringstream reqstream(req);
	string classify, oftype, rest;
	int pos;

	reqstream >> classify >> res.hash >> oftype >> res.type;
	if (classify != "CLASSIFY" || oftype != "OFTYPE" || res.hash.empty() || res.type.empty())
		return -1;
	pos = reqstream.tellg();
	rest = reqstream.str().substr(pos + 1);
	iConf = InfernoConf::parseString(rest);
	if (!iConf)
		return -1;
	res.path = iConf->computePathFromHash(res.hash);
	res.f_mode = iConf->getFilteringMode();

	return 0;
}

double svm_predict_str(double *feature, struct svm_model* model) {
	static const int num_pairs = 14;
	double result, prob_estimates[2];
	struct svm_node *x;
	int nr_class;
	int labels[2];

	if (!model) {
		Logger::error("Empty model");
		return -1;
	}

	nr_class = svm_get_nr_class(model);
	assert(nr_class == 2);
	svm_get_labels(model, labels);
	if (!(x = new struct svm_node[num_pairs + 1])) {
		Logger::error("Out of memory");
		return -1;
	}

	for (int i = 0; i < num_pairs; i++) {
		x[i].index = i + 1;
		x[i].value = feature[i];
	}

	x[num_pairs].index = -1;
	x[num_pairs].value = 0;

	int ret = (int)svm_predict_probability(model, x, prob_estimates);
	Logger::debug("svm_predict_probability returned %d, [%d = %lf, %d = %lf]", ret, labels[0], prob_estimates[0], labels[1], prob_estimates[1]);
	result = prob_estimates[(labels[0] == 1) ? 0 : 1];
	if (errno == ERANGE && isfinite(result))
		errno = 0;

	delete[] x;
	return result;
}

int serve_request(WorkerJob& job) {
	InfernoConf::Classification response = InfernoConf::CLASS_ERROR;
	Sead::IplImageFeature *feature = NULL;
	int max_c;
	bool seadInitFailed = false;

	string input_feat;
	stringstream ifstr(input_feat);
	Sead sead;

	DbCache cache;
	if (cache.init(*job.iConf)) {
		Logger::error("Unable to initialize database client");
		return -1;
	}

	// check if image loading succeeded
	if(sead.init(job.resrc.path)) {
		response = InfernoConf::CLASS_ERROR;
		seadInitFailed = true;
	} else {
		Logger::debug("Computing feature vector of requested image resource");
		feature = sead.process();
	}

	// consult svm model
	if (feature) {
		double _sample[] = {
			(double)feature->cov_b,
			(double)feature->cov_g,
			(double)feature->cov_r,
			(double)feature->mean_r,
			(double)feature->mean_g,
			(double)feature->mean_b,
			(double)feature->skin_to_non_skin_area,
			(double)feature->hu.hu1,
			(double)feature->hu.hu2,
			(double)feature->hu.hu3,
			(double)feature->hu.hu4,
			(double)feature->hu.hu5,
			(double)feature->hu.hu6,
			(double)feature->hu.hu7
		};

		static const double min[14] = {
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			0.000776554,
			1.27068e-10,
			2.96111e-15,
			6.46305e-16,
			-1.31129e-18,
			-2.05754e-11,
			-1.91655e-16
		};
		static const double max[14] = {
			16384,
			16384,
			16384,
			255,
			255,
			255,
			93.4684,
			0.00474654,
			1.45815e-05,
			6.10956e-09,
			2.36551e-08,
			2.52408e-17,
			2.03423e-11,
			3.76691e-17
		};

		// normalize feature vector
		for(int jk = 0; jk < 14; jk++)
			_sample[jk] = -1.0 + 2.0 * (_sample[jk] - min[jk]) / (max[jk] - min[jk]);

		double ret_svm[3]; // porn, benign, bikini
		ret_svm[0] = svm_predict_str(_sample, porn_model);
		ret_svm[1] = svm_predict_str(_sample, bikini_model);
		ret_svm[2] = svm_predict_str(_sample, benign_model);
		Logger::info("Probabilities: benign=%.20lf, bikini=%.20lf, porn=%.20lf", ret_svm[2], ret_svm[1], ret_svm[0]);

		if (ret_svm[0] == ret_svm[1] && ret_svm[1] == ret_svm[2])
			max_c = 1; // Classify as benign by default
		else {
			max_c = 0;
			for (int i = 1; i < 3; i++)
				if (ret_svm[i] > ret_svm[max_c])
					max_c = i;
		}

		switch(max_c) {
			case 0:
				Logger::debug("Classifier asserted that the image is PORN");
				// construct porn image response
				response = InfernoConf::CLASS_PORN;
				break;
			case 1:
				Logger::debug("Classifier asserted that the image is BIKINI");
				// construct bikini image response
				response = InfernoConf::CLASS_BIKINI;
				break;
			case 2:
				Logger::debug("Classifier asserted that the image is BENIGN");
				// construct benign image response
				response = InfernoConf::CLASS_BENIGN;
				break;
		}

		if(feature)
			delete feature;
	} else if (!seadInitFailed) { // for some reason the image processing module returned a null feature-vector pointer
		Logger::debug("classifying as benign!");
		response = InfernoConf::CLASS_BENIGN;
	}

	switch (response) {
		case InfernoConf::CLASS_PORN:
			Logger::debug("Image '%s' is deemed as PORN...", job.resrc.hash.c_str());
			break;

		case InfernoConf::CLASS_BENIGN:
			Logger::debug("Image '%s' is deemed as BENIGN...", job.resrc.hash.c_str());
			break;

		case InfernoConf::CLASS_BIKINI:
			Logger::debug("Image '%s' is deemed as BIKINI...", job.resrc.hash.c_str());
			break;

		default:
			Logger::debug("Classifier returned with an error status '%s' for image '%s'...", response, job.resrc.hash.c_str());
	}

	// update obtained url classification
	Logger::debug("Updating image classification score indice for '%s' in cache...", job.resrc.hash.c_str());
	if(response != InfernoConf::CLASS_ERROR) {
		if((job.resrc.f_mode == InfernoConf::F_MODE_IMAGE || job.resrc.f_mode == InfernoConf::F_MODE_MIXED) && (response == InfernoConf::CLASS_PORN || response == InfernoConf::CLASS_BIKINI)) {
			if (sead.blur())
				Logger::error("Sead::blur()");
			if (cache.lookupUrlContentType(job.resrc.hash) != "image/jpeg" && !cache.updateUrlContentType(job.resrc.hash, "image/jpeg")) {
				Logger::error("Error updating blurred image content type. Error report: %s", cache.getErrorString());
				response = InfernoConf::CLASS_ERROR;
			}
		}

		if(response != InfernoConf::CLASS_ERROR && !cache.updateUrlClassification(job.resrc.hash, response)) {
			Logger::error("Error updating classification score of image in cache. Error report: %s", cache.getErrorString());
			response = InfernoConf::CLASS_ERROR;
		}

		Logger::debug("Updating image status for '%s' to 'DONE'", job.resrc.hash.c_str());
		if(response != InfernoConf::CLASS_ERROR && !cache.updateUrlStatus(job.resrc.hash, InfernoConf::STATUS_DONE)) {
			Logger::error("Error updating image URL status to 'DONE'. Error report: %s", cache.getErrorString());
			response = InfernoConf::CLASS_ERROR;
		}
	}
	if (response == InfernoConf::CLASS_ERROR) {
		Logger::debug("Updating image status for '%s' to 'FAILURE'", job.resrc.hash.c_str());
		if(!cache.updateUrlStatus(job.resrc.hash, InfernoConf::STATUS_FAILURE)) {
			Logger::error("Error updating image URL status to 'FAILURE'. Error report: %s", cache.getErrorString());
		}
		return -1;
	}

	return 0;
}

void *producer(void *arg) {
	int response;
	ServerEndpoint *srv = (ServerEndpoint*)arg;
	WorkerJob *job = NULL;
	int result, ret, pos;
	char *buf = NULL;
	string request;
	bool erred = false;

	// detatch this producer-thread
	result = pthread_detach(pthread_self());
	if(result != 0) {
		perror("pthread_detach");
		exit(EXIT_FAILURE);
	}

	if (!(buf = new char[BUFSIZE + 1]))
		return((void *)NULL);

	/* main accept loop */
	Logger::debug("Entering main loop...");
	while(1) {
		Logger::debug("Waiting to accept client...");
		if (erred && job) {
			if (job->endpoint)
				delete job->endpoint;
			delete job;
			job = NULL;
		}
		request.clear();
		erred = false;
		// allocate space for a new worker-thread job descriptor
		if (!(job = new WorkerJob)) {
			Logger::debug("malloc failed on line %d", __LINE__);
			delete[] buf;
			return((void*)NULL);
		}

		// accept new server connection
		if(srv->getNextClientFromEndpoint(job->endpoint) == -1) {
			erred = true;
			continue; // ignore this
		}

		// identify client request
		Logger::debug("Waiting to receive client request...");
		while (1) {
			memset(buf, 0, BUFSIZE + 1);

			if ((ret = job->endpoint->recvDataFromEndpoint(buf, BUFSIZE)) <= 0) {
				erred = true;
				break;
			}

			request.append(buf);
			if ((pos = request.find("\r\n")) != string::npos) {
				request = request.substr(0, pos);
				break;
			}
		}
		if (erred) {
			response = -1;
			job->endpoint->sendDataToEndpoint(&response, sizeof(response));
			continue;
		}

		/* parse client request header part and extract request bits */
		if(frisk_user_request_str(request, job->resrc, job->iConf) < 0) {
			Logger::debug("Server could not infer a known request type!");
			response = -1;
			job->endpoint->sendDataToEndpoint(&response, sizeof(response));
			continue; // wait next
		}
		response = 0;
		job->endpoint->sendDataToEndpoint(&response, sizeof(response));
		delete job->endpoint;
		job->endpoint = NULL;

		sem_wait(&mutex);
		reqs.push(job);
		Logger::debug("Added new job to queue. Queue now contains %d elements.", reqs.size(), POOLSIZE);
		sem_post(&mutex);
		sem_post(&fillCount);
	}
	sem_post(&fillCount);

	// return nothing as a status message
	return((void *)NULL);
}

void *worker_main(void *arg) {
	WorkerJob *job;

	(void)arg; // suppress irritating compiler warning

	// retrieve client file descriptor
	while(1) {
		// access shared job-queue
		sem_wait(&fillCount);
		sem_wait(&mutex);
		// at least one item is queued in order to be processed; eat it!
		if (!(job = reqs.front())) {
			sem_post(&mutex);
			Logger::error("QUDequeue failed");
			continue;
		}
		reqs.pop();
		Logger::debug("Removed job from queue. Queue now contains %d elements.", reqs.size(), POOLSIZE);
		sem_post(&mutex);

		// process request and close remote endpoint
		if(serve_request(*job) == -1)
			Logger::error("Error serving client request");

		// free job space
		if (job->endpoint)
			delete job->endpoint;
		delete job;
		job = NULL;
	}

	// return nothing
	return((void *)NULL);
}

int main(int argc, char **argv) {
	int ret, c;
	char *port = (char*)"1345",  *ip_addr = (char*)""; // default (LISTEN on all ifaces)

	// check user-provided options
	while ((c = getopt(argc, argv, "p:i:")) != -1) {
		switch(c) {
			case 'p': // port
				port = strdup(optarg);
				break;
			case 'i': // iface
				ip_addr = strdup(optarg);
				break;
			default:
				abort();
		}
	}
	errno = 0;

	int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
	POOLSIZE = (numCPU > 0) ? (int)ceil(1.5 * (double)numCPU) : 1;
	Logger::info("Using %u threads for classification", POOLSIZE);

	if (sem_init(&mutex, 0, 1)) {
		perror("sem_init");
		goto cleanup;
	}

	if (sem_init(&fillCount, 0, 0)) {
		perror("sem_init");
		goto cleanup;
	}

	basedir = dirname(argv[0]);
	if(mysql_library_init(0, NULL, NULL)) {
		Logger::error("mysql_library_init");
		goto cleanup;
	}


	// initialize threading support
	g_thread_init(NULL);

	// initialize glib
	g_type_init();

	if (!(porn_model = svm_load_model((basedir + "/" + MODEL_FILE_PORN).c_str()))) {
		Logger::error("Can't open model file %s", MODEL_FILE_PORN);
		goto cleanup;
	}
	if (!svm_check_probability_model(porn_model)) {
		Logger::error("%s is not a probability model", MODEL_FILE_PORN);
		goto cleanup;
	}

	if (!(bikini_model = svm_load_model((basedir + "/" + MODEL_FILE_BIKINI).c_str()))) {
		Logger::error("Can't open model file %s", MODEL_FILE_BIKINI);
		goto cleanup;
	}
	if (!svm_check_probability_model(bikini_model)) {
		Logger::error("%s is not a probability model", MODEL_FILE_BIKINI);
		goto cleanup;
	}

	if (!(benign_model = svm_load_model((basedir + "/" + MODEL_FILE_BENIGN).c_str()))) {
		Logger::error("Can't open model file %s", MODEL_FILE_BENIGN);
		goto cleanup;
	}
	if (!svm_check_probability_model(benign_model)) {
		Logger::error("%s is not a probability model", MODEL_FILE_BENIGN);
		goto cleanup;
	}

	// allocate memory for worker store
	workers = new pthread_t[POOLSIZE];
	if(workers == NULL) {
		perror("new");
		goto cleanup;
	}

	srv = new ServerEndpoint(ip_addr, port, 10);
	// create server
	if(srv->init() == -1) {
		Logger::error("failed to create server on port '%s'", port);
		goto cleanup;
	}

	// register signal handlers for the server process
	Logger::debug("Installing signal handlers...");
	struct sigaction sact;
	memset(&sact, 0, sizeof(sact));
	sact.sa_handler = sig_int;
	if (sigaction(SIGTERM, &sact, NULL))
		perror("sigaction");
	if (sigaction(SIGINT, &sact, NULL))
		perror("sigaction");

	// create producer thread
	Logger::debug("creating producer thread...");
	ret = pthread_create(&tid, NULL, producer, srv);
	if(ret != 0) {
		perror("pthread_create");
		goto cleanup;
	}
	Logger::debug("producer thread created!");

	// create worker threads
	Logger::debug("creating worker threads...");
	for(unsigned i = 0; i < POOLSIZE; i++) {
		ret = pthread_create(&workers[i], NULL, worker_main, NULL);
		if(ret != 0) {
			perror("pthread_create");
			goto cleanup;
		}
		Logger::debug("[==>] created worker thread '%d'", i + 1);
	}
	Logger::debug("All workers created successfully");

	// wait for incoming connections
	Logger::debug("Entering main server loop...");
	for(;;)
		pause();

	// return peacefully
	return EXIT_SUCCESS;

cleanup:
	sem_destroy(&fillCount);
	sem_destroy(&mutex);
	if (srv)
		delete srv;
	if (workers)
		delete[] workers;
}
