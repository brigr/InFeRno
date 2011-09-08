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
#include <stack>
#include <stdexcept>

#define CV_NO_BACKWARD_COMPATIBILITY
#include <cv.h>
#include <highgui.h>
#include <ml.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gthread.h>

#include "sead.h"
#include "logger.h"

#ifndef M_PI
#define M_PI ((double)3.14159265358979323846)
#endif

using namespace std;

typedef struct {
	int r;
	int g;
	int b;
} Pixel_t;

const int Sead::PARTITIONS_X = 4;
const int Sead::PARTITIONS_Y = 4;
const double Sead::alpha = 0.005;
const double Sead::beta = 0.1;
const double Sead::c = 0.3;
const double Sead::rho = 0.1;

Sead::Sead() : img(NULL) {}
Sead::~Sead() { if (img) { cvReleaseImage(&img); img = NULL; } }

IplImage* Sead::loadGIF(const string& file) {
	GdkPixbuf *pb;
	GdkColorspace csp;
	GError *gerror = NULL;

	IplImage *image;

	int width;
	int height;
	int rowstride;
	int chans;
	int bps;

	int x;
	int y;

	guchar *pixels;
	guchar *p;

	// check path
	if(file.empty()) {
		errno = EINVAL;
		return NULL;
	}

	// load gdk pixbuf from file
	pb = gdk_pixbuf_new_from_file(file.c_str(), &gerror);
	if(pb == NULL) {
		Logger::debug("GDK cannot load file %s: %s", file.c_str(), gerror->message);
		return NULL;
	}

	// get necessary file information
	width		= gdk_pixbuf_get_width(pb);
	height		= gdk_pixbuf_get_height(pb);
	rowstride	= gdk_pixbuf_get_rowstride(pb);
	chans		= gdk_pixbuf_get_n_channels(pb);
	bps			= gdk_pixbuf_get_bits_per_sample(pb);

	csp			= gdk_pixbuf_get_colorspace(pb);
	pixels		= gdk_pixbuf_get_pixels(pb);


	// create opencv image structure
	image = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, chans);
	if(image == NULL) {
		Logger::debug("cvCreateImage");
		gdk_pixbuf_unref(pb);
		return NULL;
	}

	for(x = 0; x < width; x++) {
		for(y = 0; y < height; y++) {
			// read channel values from pixbuf
			p = pixels + y * rowstride + x * chans;

			// copy RGB channel values to the IplImage structure
			cvSet2D(image, y, x, CV_RGB((int)p[0], (int)p[1], (int)p[2]));
		}
	}

	// free memory associated with image
	gdk_pixbuf_unref(pb);

	// return structure
	return(image);
}

int Sead::init(const string& path) {
	if (path.empty()) return -1;
	this->path = path;
	if (img)
		cvReleaseImage(&img);
	Logger::debug("Loading client-requested resource from %s...", path.c_str());
	if (
			!(img = cvLoadImage(path.c_str(), CV_LOAD_IMAGE_COLOR)) &&
			!(img = loadGIF(path))) {
		Logger::debug("Sead could not open/locate image %s on storage media", path.c_str());
		return -1;
	}
	return 0;
}

int Sead::doAdaptiveGammaCorrection(IplImage *img) {
	static const double chiM = 128.0; /* intensity midpoint */
	uchar *data = (uchar *)img->imageData;

	// invert the image
	for(int i = 0; i < img->height; i++) {
		for(int j = 0; j < img->width; j++) {
			for(int idx = 0; idx < 3; idx++) {
				double intensity = (double)data[i * img->widthStep + 3 * j + idx];

				double f1 = alpha * cos((M_PI * intensity) / (2.0 * chiM));
				double f2 = (rho * sin((4 * M_PI * intensity) / 255.0) + beta) * cos(alpha) + intensity * sin(alpha);
				double f3 = c * fabs((intensity / chiM) - 1.0) * cos((3.0 * M_PI * intensity) / 255.0);
				double gammax = 1.0 + f1 + f2 + f3;

				if(gammax <= 0)
					return -1;

				// update gamma corrected values
				data[i * img->widthStep + 3 * j + idx] = (uchar)255 * pow(intensity / 255.0, 1.0 / gammax);
			}
		}
	}
	return 0;
}

// image: binary skin filtered image, 3 channel image!
// x, y: coordinates of point in binary image
int Sead::Psi(const IplImage *image, int x, int y) {
	if(image == NULL)
		return -1;

	/* pixel pixel values of 4-neighborhood around pixel (x,y) */
	CvScalar p[] = {
		cvGet2D(image, y + 1, x + 0),
		cvGet2D(image, y + 0, x + 1),
		cvGet2D(image, y - 1, x + 0),
		cvGet2D(image, y + 0, x - 1)
	};

	// find skin pixels around 4-neighborhood
	int ksi = 0;
	for(int i = 0; i < 4; i++)
		ksi += (is_skin((int)p[i].val[2], (int)p[i].val[1], (int)p[i].val[0])); /* check if pixel is a skin pixel (white) */

	/* decide edge operator return value */
	return (ksi != 4);
}

// image: canny filtered image
// x, y: coordinates
int Sead::CAN(const IplImage *image, int x, int y) {
	if(image == NULL)
		return -1;

	/* get intensity value at pixel (x,y) */
	CvScalar sc = cvGet2D(image, y, x);

	/* check skiness in 4-neighborhood; check if pixel is white */
	return (sc.val[0] == 255);
}

int Sead::Edge(const IplImage *skin_binary_img, const IplImage *canny_edge_img, int x, int y) {
	return (Psi(skin_binary_img, x, y) + CAN(canny_edge_img, x, y) == 2);
}

void Sead::cvGetAreaPixels(IplImage *image, IplImage *blackboard, CvPoint init_point, CvScalar boundary_color, CvScalar fill_color, CvSeq *points, double& contour_nonskin_mean_r, double& contour_nonskin_mean_g, double& contour_nonskin_mean_b, double& skin_to_nonskin_ratio, unsigned long& contour_nonskin_total_pixels) {
	unsigned long contour_skin_total_pixels = 0;
	stack<CvPoint> neighbors;
	CvPoint pt;

	// mean R, G, B values for non-skin pixels
	contour_nonskin_mean_r = contour_nonskin_mean_g = contour_nonskin_mean_b = 0;
	contour_nonskin_total_pixels = 0;

	// push mean pixel to stack
	pt.x = init_point.x;
	pt.y = init_point.y;
	neighbors.push(pt);

	while (!neighbors.empty()) {
		// pop neighboring pixel
		CvPoint n = neighbors.top();
		neighbors.pop();

		// check if we are out of the image matrix bounds
		if(n.x < 0 || n.x >= image->width || n.y < 0 || n.y >= image->height)
			continue;

		// get color vector of this pixel
		CvScalar tmp = cvGet2D(blackboard, n.y - 1, n.x - 1);

		if ((tmp.val[0] != boundary_color.val[0] && tmp.val[1] != boundary_color.val[1] && tmp.val[2] != boundary_color.val[2]) ||
				(tmp.val[0] == fill_color.val[0] || tmp.val[1] == fill_color.val[1] || tmp.val[2] == fill_color.val[2]))
			continue;

		cvSet2D(blackboard, n.y - 1, n.x - 1, CV_RGB(255, 255, 255));

		// update statistics
		tmp = cvGet2D(image, n.y, n.x);
		if(is_skin((int) tmp.val[2], (int) tmp.val[1], (int) tmp.val[0])) {
			contour_skin_total_pixels++;
		} else {
			contour_nonskin_total_pixels++;

			/* update non-skin r, g, b mean */
			contour_nonskin_mean_r += (double)tmp.val[2];
			contour_nonskin_mean_g += (double)tmp.val[1];
			contour_nonskin_mean_b += (double)tmp.val[0];

			/* add non-skin pixel color components to the list of non-skin in-contour pixels */
			Pixel_t pix;
			pix.r = (int)tmp.val[2];
			pix.g = (int)tmp.val[1];
			pix.b = (int)tmp.val[0];
			cvSeqPush(points, &pix);
		}

		// push west
		pt.x = n.x - 1;
		pt.y = n.y;
		neighbors.push(pt);

		// push east
		pt.x = n.x + 1;
		pt.y = n.y;
		neighbors.push(pt);

		// push north
		pt.x = n.x;
		pt.y = n.y - 1;
		neighbors.push(pt);

		// push south
		pt.x = n.x;
		pt.y = n.y + 1;
		neighbors.push(pt);
	}

	/* compute mean of non skin pixels in contour area */
	contour_nonskin_mean_r /= (double) contour_nonskin_total_pixels;
	contour_nonskin_mean_g /= (double) contour_nonskin_total_pixels;
	contour_nonskin_mean_b /= (double) contour_nonskin_total_pixels;
	skin_to_nonskin_ratio = contour_skin_total_pixels / (double)contour_nonskin_total_pixels;
}

struct Sead::IplImageFeature *Sead::process() {
	IplImage *splitColorImage = NULL, *featureimg = NULL, *blackboard = NULL, *hu_img = NULL;
	CvMemStorage *mem_storage = NULL, *nsp_storage = NULL;
	CvSeq *hull = NULL, *mem_seq = NULL, *nsp_seq = NULL;

	bool returnNull = false;

	struct IplImageFeature *feature = NULL;
	struct IplImagePartition *partition = NULL;
	unsigned long contour_nonskin_total_pixels;

	double mean_x, mean_y;
	CvPoint mean_pt, main_pt;

	CvMoments moments;

	if (!img) {
		Logger::debug("Called process() on an uninitialized Sead instance");
		return NULL;
	}

	if (
			!(feature = new struct IplImageFeature) || // alloc + zero-out

			!(mem_storage = cvCreateMemStorage(0)) ||
			!(nsp_storage = cvCreateMemStorage(0)) ||

			!(mem_seq = cvCreateSeq(CV_SEQ_KIND_GENERIC | CV_32SC2, sizeof(CvContour), sizeof(CvPoint), mem_storage)) ||
			!(nsp_seq = cvCreateSeq(CV_SEQ_KIND_GENERIC, sizeof(CvContour), sizeof(Pixel_t), nsp_storage)) ||

			!(featureimg = cvCloneImage(img)) ||
			!(splitColorImage = cvCloneImage(img)) ||

			!(hu_img = cvCreateImage(cvGetSize(img), img->depth, 1)) ||
			!(blackboard = cvCreateImage(cvGetSize(img), IPL_DEPTH_32F, 3))
	   ) {
		returnNull = true;
		goto dealloc_memory;
	}
	memset(feature, 0, sizeof(struct IplImageFeature));

	/* compute Canny edge detection on image */
	cvCvtColor(img, hu_img, CV_BGR2GRAY);

	/* perform adaptive Gamma correction on image */
	//doAdaptiveGammaCorrection(img);
	//doAdaptiveGammaCorrection(splitColorImage);

	// draw a black filled rectangle over the whole image
	cvRectangle(blackboard, cvPoint(0, 0), cvPoint(img->width, img->height), CV_RGB(0, 0, 0), CV_FILLED, 8, 0);

	/* recursively partition the color image and locate skin-like regions */
	if (cvAdaptiveGrids(splitColorImage, 4, 4, 3, NULL, mem_seq, partition)) {
		Logger::error("There was an error in performing the recursive splitting or no skin regions in the image!");
		returnNull = true;
		goto dealloc_memory;
	}

	if(cvAdaptiveGridsHasFoundCells(partition) == 0) {
		returnNull = true;
		Logger::info("No connected skin regions found in image: image is benign!");
		goto dealloc_memory;
	}


	/* compute convex hull of detected skin-like regions */
	hull = cvConvexHull2(mem_seq, 0, CV_CLOCKWISE, 0 );
	if(hull == NULL) {
		returnNull = true;
		Logger::error("cvConvexHull2: returned NULL");
		goto dealloc_memory;

	}

	/* render perimeter of skin-region convex hull */
	main_pt = **CV_GET_SEQ_ELEM( CvPoint*, hull, hull->total - 1 );
	for (int i = 0; i < hull->total; i++) {
		/* get adjacent point in convex hull */
		CvPoint pt = **CV_GET_SEQ_ELEM( CvPoint*, hull, i );

		/* draw contour line on feature image */
		cvLine(featureimg, main_pt, pt, CV_RGB( 0, 255, 0), 3, 8, 0);

		/* draw contour line on blackboard image */
		cvLine(blackboard, main_pt, pt, CV_RGB( 0, 255, 0), 3, 8, 0);

		/* keep last point */
		main_pt = pt;
	}

	/* COMPUTE MEAN CONTROL POINT ON THE REFINED CONVEX HULL */
	/* ...first initialize coordinates of the mean point */
	mean_x = 0;
	mean_y = 0;
	/* ...compute nominator, also dividing by the total number of points
	 * on the convex hull to get mean point */
	for (int i = 0; i < hull->total; i++) {
		CvPoint pt1 = **CV_GET_SEQ_ELEM(CvPoint *, hull, i);
		mean_x += pt1.x / (double)hull->total;
		mean_y += pt1.y / (double)hull->total;
	}
	mean_pt.x = (int)floor(mean_x);
	mean_pt.y = (int)floor(mean_y);

	/* get all internal points with respect to the focus-of-attention contour */
	cvGetAreaPixels(featureimg, blackboard, mean_pt, CV_RGB(0, 255, 0), CV_RGB(255, 255, 255), nsp_seq, feature->mean_r, feature->mean_g, feature->mean_b, feature->skin_to_non_skin_area, contour_nonskin_total_pixels);

	if(contour_nonskin_total_pixels == 0) {
		Logger::info("No nonskin pixels found!");
		returnNull = true;
		goto dealloc_memory;
	}

	cvMoments(hu_img, &moments);
	cvGetHuMoments(&moments, &feature->hu);

	/* compute variance of non-skin pixels inside the contour convex hull for R, G, B channels */
	for (int i = 0; i < nsp_seq->total; i++) {
		Pixel_t pix = *CV_GET_SEQ_ELEM(Pixel_t, nsp_seq, i);
		feature->cov_r += (pix.r - feature->mean_r) * (pix.r - feature->mean_r) / (double)nsp_seq->total;
		feature->cov_g += (pix.g - feature->mean_g) * (pix.g - feature->mean_g) / (double)nsp_seq->total;
		feature->cov_b += (pix.b - feature->mean_b) * (pix.b - feature->mean_b) / (double)nsp_seq->total;
	}

dealloc_memory:
	/* release allocated space for images */
	if (hu_img)
		cvReleaseImage(&hu_img);
	if (blackboard)
		cvReleaseImage(&blackboard);
	if (featureimg)
		cvReleaseImage(&featureimg);
	if (splitColorImage)
		cvReleaseImage(&splitColorImage);

	/* release memory of storage trunks */
	if (mem_storage)
		cvReleaseMemStorage(&mem_storage);
	if (nsp_storage)
		cvReleaseMemStorage(&nsp_storage);

	if (feature && returnNull) {
		delete feature;
		feature = NULL;
	}

	return feature;
}

int Sead::blur() {
	if (!img)
		return -1;
	string dest = path + ".jpg";
	Logger::debug("Blurring image '%s' out!", path.c_str());
	cvSmooth(img, img, CV_BLUR, 20, 20);
	if (!cvSaveImage(dest.c_str(), img)) {
		Logger::error("cvSaveImage(%s)", dest.c_str());
		return -1;
	}
	if (rename(dest.c_str(), path.c_str())) {
		Logger::error("rename");
		return -1;
	}
	return 0;
}

int Sead::_min(int a, int b, int c) {
	int tmp = (a < b) ? a : b;
	return (tmp < c) ? tmp : c;
}

int Sead::_max(int a, int b, int c) {
	int tmp = (a > b) ? a : b;
	return (tmp > c) ? tmp : c;
}

int Sead::is_skin(int r, int g, int b) {
	// P. Peer, J. Kovac, and F. Solina. Human skin colour clustering
	// for face detection. In Proc. EUROCON - International Conference
	// on Computer as a Tool (2003).
	return ((r > 95 && g > 40 && b > 20) &&
			(_max(r, g, b) - _min(r, g, b) > 15) &&
			(abs(r-g) > 15) &&
			(r > g && r > b));
}

int Sead::cvAdaptiveGridsHasFoundCells(struct IplImagePartition *partition) {
	/* scan all cells and find potentially non-split region */
	for (int i = 0; i < partition->partitions_x; i++)
		for (int j = 0; j < partition->partitions_y; j++)
			if(partition->blocks[i][j].partitions_x > 0 || partition->blocks[i][j].partitions_y > 0)
				return 1;
	return 0;
}

int Sead::cvAdaptiveGrids(IplImage *image, int partitions_x, int partitions_y, int maxSplitLevel, struct IplImagePartition *cell, CvSeq *ctrl_points, struct IplImagePartition*& partition) {
	bool returnNull = false;

	int block_width; /* width in pixels for examined cell */
	int block_height; /* height in poixels for examined cell */

	unsigned long region_total_area; /* total area of cell */

	double region_mean_intensity; /* mean intensity of examined cell */
	double region_mean_skin_intensity; /* mean skin pixel intensity of examined cell */

	double region_variance_intensity; /* variance of intensity in examined cell */
	double region_variance_skin_intensity; /* variance of skin pixel intensity in examined cell */

	double region_kurtosis_total_area; /* 4-th order kurtosis of examined cell */
	double region_kurtosis_skin_area; /* 4-th order kurtosis of skin area in examined cell */

	double zetta1; /* skin area to total area ratio; decision factor for splitting criterion */
	double zetta2; /* kurtosis of overal image to kurtosis of skin area in cell; constraint in splitting criterion */

	/* if image is uninitialized return NULL pointer to partitioning structure */
	/* maxSplitLevel must be a positive integer */
	if (!image || !maxSplitLevel)
		return 0;

	/* allocate memory for partition table */
	if (!(partition = new struct IplImagePartition)) {
		Logger::warn("cvAdaptiveGrids: not enough memory for partition table allocation!");
		return -1;
	}
	memset(partition, 0, sizeof(struct IplImagePartition));

	/* initialize structure parameters */
	if (!cell) {
		partition->is_base_image = 1;
		partition->ref_x  = partition->ref_y = 0;
		partition->height = image->height;
		partition->width  = image->width;
	} else {
		partition->is_base_image = 0;
		partition->ref_x  = cell->ref_x;
		partition->ref_y  = cell->ref_y;
		partition->height = cell->height;
		partition->width  = cell->width;
	}

	// if user wants 0 partitions over x (y), we be default split x (y)
	// in 4 partitions
	partition->partitions_x  = (partitions_x) ? partitions_x : PARTITIONS_X;
	partition->partitions_y  = (partitions_y) ? partitions_y : PARTITIONS_Y;
	// default: split level is set to 2
	partition->maxSplitLevel = (maxSplitLevel) ? maxSplitLevel : 2;

	partition->blocks = new struct IplImagePartition *[partition->partitions_x];
	if(partition->blocks == NULL) {
		Logger::warn("cvAdaptiveGrids: allocation error while trying to allocate image partition structures!");
		returnNull = true;
		goto cleanup_memory;
	}

	for (int i = 0; i < partition->partitions_x; i++) {
		partition->blocks[i] = new struct IplImagePartition[partition->partitions_y];
		if(partition->blocks[i] == NULL) {
			Logger::warn("cvAdaptiveGrids: allocation error while trying to allocate image partition structures!");
			returnNull = true;
			goto cleanup_memory;
		}
		memset(partition->blocks[i], 0, partition->partitions_y * sizeof(struct IplImagePartition));
	}

	/* allocate space for intensity histograms */
	partition->intensity_histograms = new struct IntensityHistogram *[partition->partitions_x];
	if(partition->intensity_histograms == NULL) {
		Logger::warn("cvAdaptiveGrids: allocation failed for region intensity histograms!");

		returnNull = true;
		goto cleanup_memory;
	}

	for (int i = 0; i < partition->partitions_x; i++) {
		partition->intensity_histograms[i] = new struct IntensityHistogram[partition->partitions_y];
		if(partition->intensity_histograms[i] == NULL) {
			Logger::warn("cvAdaptiveGrids: allocation failed for region intensity histograms!");
			returnNull = true;
			goto cleanup_memory;
		}
		memset(partition->intensity_histograms[i], 0, partition->partitions_y * sizeof(struct IntensityHistogram));
	}

	/* obtain width and height of local subregion */
	block_width  = (int)floor(partition->width / (double)partition->partitions_x);
	block_height = (int)floor(partition->height / (double)partition->partitions_y);

	/* compute total area based on width and height of cell */
	region_total_area = block_width * block_height;

	/* do the partitioning */
	for (int i = 0; i < partition->partitions_x; i++) {
		/* obtain x-axis top left coordinate of sub-region to scan */
		int start_x = partition->ref_x + i * block_width;

		for (int j = 0; j < partition->partitions_y; j++) {
			/* obtain y-axis top left coordinate of sub-region to scan */
			int start_y = partition->ref_y + j * block_height;
			unsigned long region_skin_area = 0;

			/* scan all pixels in region and compute metrics */
			for (int coord_x = start_x; coord_x < start_x + block_width; coord_x++) {
				for (int coord_y = start_y; coord_y < start_y + block_height; coord_y++) {
					CvScalar tmp = cvGet2D(image, coord_y, coord_x);

					/* obtain color constraints at that point */
					int r = (int) tmp.val[2];
					int g = (int) tmp.val[1];
					int b = (int) tmp.val[0];

					int intensity = (int)floor(((double)(r + g + b)) / 3.0);

					/* increment intensity count at intensity I */
					partition->intensity_histograms[i][j].bins[intensity]++;

					region_skin_area += is_skin(r, g, b);
				}
			}
			if (!region_skin_area) {
				continue;
			}
			/* compute skin proportion decision factor */
			zetta1 = region_skin_area / (double)region_total_area;

			/* compute intensity mean */
			region_mean_intensity = region_mean_skin_intensity = 0.0;
			for (int k = 0; k < 256; k++) {
				region_mean_intensity += (double)k * (partition->intensity_histograms[i][j].bins[k] / (double)region_total_area);
				region_mean_skin_intensity += (double)k * (partition->intensity_histograms[i][j].bins[k] / (double)region_skin_area);
			}

			/* compute intensity variance and kurtosis */
			region_variance_intensity = region_variance_skin_intensity = 0.0;
			region_kurtosis_total_area = region_kurtosis_skin_area = 0.0;
			for (int k = 0; k < 256; k++) {
				region_variance_intensity += (k - region_mean_intensity) * (k - region_mean_intensity) * (partition->intensity_histograms[i][j].bins[k] / (double)region_total_area);
				region_variance_skin_intensity += (double)(k - region_mean_skin_intensity) * (k - region_mean_skin_intensity) * (partition->intensity_histograms[i][j].bins[k] / (double)region_skin_area);
			}

			if (!region_variance_intensity || !region_variance_skin_intensity) {
				// Zero variance means all values are in a single cell
				// (i.e., an impulse) so set the skin region kurtosis
				// would be infinite, and thus zetta2 also in. Thus, we
				// symbolically set it to 1.6, which is the uppper bound
				// for the selection criterion.
				zetta2 = 1.6;
			} else {
				double div_var = region_variance_intensity * region_variance_intensity, div_svar = region_variance_skin_intensity * region_variance_skin_intensity;

				for (int k = 0; k < 256; k++) {
					region_kurtosis_total_area += pow(k - region_mean_intensity, 4.0) * (partition->intensity_histograms[i][j].bins[k] / (double)region_total_area) / div_var;
					region_kurtosis_skin_area += pow(k - region_mean_skin_intensity, 4.0) * (partition->intensity_histograms[i][j].bins[k] / (double)region_skin_area) / div_svar;
				}
				/* compute skin to total kurtosis decision factor */
				zetta2 = (region_kurtosis_skin_area - 3.0) / (region_kurtosis_total_area - 3.0);
			}

			/* decide if region needs to be split further */
			if (zetta1 >= 0.35 && zetta2 >= 1.00) {
				/* draw green rectange signifying ROI */
				if(cell != NULL && !cell->is_base_image && cell->maxSplitLevel == 1) {
					CvPoint pt1, pt2;

					pt1.x = start_x;
					pt1.y = start_y;
					pt2.x = start_x + block_width;
					pt2.y = start_y + block_height;
					cvRectangle(image, pt1, pt2, CV_RGB(255, 0, 0), 1, 8, 0);

					cvSeqPush(ctrl_points, &pt1);
					cvSeqPush(ctrl_points, &pt2);

					pt2.x = pt1.x + block_width;
					pt2.y = pt1.y;
					cvSeqPush(ctrl_points, &pt2);

					pt2.x = pt1.x;
					pt2.y = pt1.y + block_height;
					cvSeqPush(ctrl_points, &pt2);
				}

				/* prepare terrain for next recursive splitting */
				partition->blocks[i][j].is_base_image = 0;

				partition->blocks[i][j].maxSplitLevel = maxSplitLevel - 1;

				/* number of segments to partition image into along x and y */
				partition->blocks[i][j].partitions_x = (partitions_x) ? partitions_x : PARTITIONS_X;
				partition->blocks[i][j].partitions_y = (partitions_y) ? partitions_y : PARTITIONS_Y;

				/* partitioned cell dimensions */
				partition->blocks[i][j].width = block_width;
				partition->blocks[i][j].height = block_height;

				/* relative start point of partitioned cell */
				partition->blocks[i][j].ref_x = start_x;
				partition->blocks[i][j].ref_y = start_y;

				/* recursively split cell */
				struct IplImagePartition* innerPartition = NULL;
				if (cvAdaptiveGrids(image, 4, 4, maxSplitLevel - 1, &partition->blocks[i][j], ctrl_points, innerPartition)) {
					returnNull = true;
					goto cleanup_memory;
				}

				if (innerPartition) {
					cvAdaptiveGridsRelease(innerPartition);
				}
			}
		}
	}

cleanup_memory:
	if (returnNull && partition)
		cvAdaptiveGridsRelease(partition);

	return (returnNull) ? -1 : 0;
}

void Sead::cvAdaptiveGridsRelease(struct IplImagePartition*& partition) {
	if (!partition)
		return;
	if (partition->blocks) {
		for (int i = 0; i < partition->partitions_x && partition->blocks[i]; i++)
			cvAdaptiveGridsRelease(partition->blocks[i]);
		delete[] partition->blocks;
	}
	if (partition->intensity_histograms) {
		for (int i = 0; i < partition->partitions_x && partition->intensity_histograms[i]; i++)
			delete[] partition->intensity_histograms[i];
		delete[] partition->intensity_histograms;
	}
	delete partition;
	partition = NULL;
}
