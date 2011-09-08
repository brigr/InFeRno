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

#ifndef __MY_HELPERS_H__
#define __MY_HELPERS_H__

#include <string>
#include <cv.h>
#include <highgui.h>

class Sead {
	private:
		const static int PARTITIONS_X;
		const static int PARTITIONS_Y;
		const static double alpha;
		const static double beta;
		const static double c;
		const static double rho;

		std::string path;
		IplImage *img;

		struct IntensityHistogram {
			int bins[256]; /* intensity bins 0..255 */
		};

		struct IplImagePartition {
			unsigned long width; /* refernced image width in pixels */
			unsigned long height; /* referenced image height in pixels */

			unsigned long ref_x; /* top left x coordinate of reference partition */
			unsigned long ref_y; /* top left y coordinate of reference partition */

			int partitions_y; /* number of partitions over y direction */
			int partitions_x; /* number of partitions over x direction */

			int maxSplitLevel; /* maximum number of splitting iteration levels */
			int control_points[4]; /* control points of rectngular subregion */

			struct IplImagePartition **blocks; /* map of partitioned regions in the image */
			struct IntensityHistogram **intensity_histograms; /* intensity histograms for subregions in current splitting level */

			bool is_base_image; /* indicated whether structure refers to initial image */
		};

		static int cvAdaptiveGrids(IplImage *image, int partitions_x, int partitions_y, int maxSplitLevel, struct IplImagePartition *cell, CvSeq *ctrl_points, struct IplImagePartition *& partition);
		static int cvAdaptiveGridsHasFoundCells(struct IplImagePartition *partition);
		static void cvAdaptiveGridsRelease(struct IplImagePartition*& partition);
		static int _min(int, int, int);
		static int _max(int, int, int);
		static int is_skin(int r, int g, int b);
		static int doAdaptiveGammaCorrection(IplImage *img);
		static IplImage* loadGIF(const std::string& path);
		static int Psi(const IplImage *image, int x, int y);
		static int CAN(const IplImage *image, int x, int y);
		static int Edge(const IplImage *skin_binary_img, const IplImage *canny_edge_img, int x, int y);
		static void cvGetAreaPixels(
				IplImage *image,
				IplImage *blackboard,
				CvPoint init_point,
				CvScalar boundary_color,
				CvScalar fill_color,
				CvSeq *points,
				double& contour_nonskin_mean_r,
				double& contour_nonskin_mean_g,
				double& contour_nonskin_mean_b,
				double& skin_to_nonskin_ratio,
				unsigned long& contour_nonskin_total_pixels);

	public:
		struct IplImageFeature {
			int sample_class;

			/* angles u1, u2, u3 */
			double u1, u2, u3;

			/* ratio of the line segment */
			double ratio_line_segment;

			/* ratio of skin to non-skin area */
			double skin_to_non_skin_area;

			/* mean and covariance matrix */
			double mean_r, mean_g, mean_b;
			double cov_r, cov_g, cov_b;

			/* invariant Hu moments of extracted contour */
			CvHuMoments hu;
		};

		Sead();
		~Sead();
		int init(const std::string& path);
		struct IplImageFeature *process();
		int blur();
};

#endif
