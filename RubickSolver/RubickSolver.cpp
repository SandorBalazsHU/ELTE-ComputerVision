#include "pch.h"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

using namespace cv;
using namespace std;

Mat src, detected_squares, detected_edges;
int cannyThreshold = 45;
int const houghLinesPThreshold = 0;
int const HoughLinesPa = 0;
int const HoughLinesPb = 0;
int const barMaximum = 100;
int const ratio = 3;
int const kernel_size = 3;
int const thickness = 2;
const char* edgeWindoWname = "Élek";
const char* squaresFinder = "Camera";
int squaresFinderTrashhold = 50, squaresFinderIteration = 1;
vector<vector<Point>> squares;

// helper function:
// finds a cosine of angle between vectors
// from pt0->pt1 and from pt0->pt2
static double angle(Point pt1, Point pt2, Point pt0)
{
	double dx1 = pt1.x - pt0.x;
	double dy1 = pt1.y - pt0.y;
	double dx2 = pt2.x - pt0.x;
	double dy2 = pt2.y - pt0.y;
	return (dx1*dx2 + dy1 * dy2) / sqrt((dx1*dx1 + dy1 * dy1)*(dx2*dx2 + dy2 * dy2) + 1e-10);
}

// returns sequence of squares detected on the image.
static void findSquares(const Mat& image, vector<vector<Point> >& squares)
{
	squares.clear();

	Mat pyr, timg, gray0(image.size(), CV_8U), gray;

	// down-scale and upscale the image to filter out the noise
	pyrDown(image, pyr, Size(image.cols / 2, image.rows / 2));
	pyrUp(pyr, timg, image.size());
	vector<vector<Point> > contours;

	// find squares in every color plane of the image
	for (int c = 0; c < 3; c++)
	{
		int ch[] = { c, 0 };
		mixChannels(&timg, 1, &gray0, 1, ch, 1);

		// try several threshold levels
		for (int l = 0; l < squaresFinderIteration; l++)
		{
			// hack: use Canny instead of zero threshold level.
			// Canny helps to catch squares with gradient shading
			if (l == 0)
			{
				// apply Canny. Take the upper threshold from slider
				// and set the lower to 0 (which forces edges merging)
				Canny(gray0, gray, 0, squaresFinderTrashhold, 5);
				// dilate canny output to remove potential
				// holes between edge segments
				dilate(gray, gray, Mat(), Point(-1, -1));
			}
			else
			{
				// apply threshold if l!=0:
				//     tgray(x,y) = gray(x,y) < (l+1)*255/N ? 255 : 0
				gray = gray0 >= (l + 1) * 255 / squaresFinderIteration;
			}

			// find contours and store them all as a list
			findContours(gray, contours, RETR_LIST, CHAIN_APPROX_SIMPLE);

			vector<Point> approx;

			// test each contour
			for (size_t i = 0; i < contours.size(); i++)
			{
				// approximate contour with accuracy proportional
				// to the contour perimeter
				approxPolyDP(contours[i], approx, arcLength(contours[i], true)*0.02, true);

				// square contours should have 4 vertices after approximation
				// relatively large area (to filter out noisy contours)
				// and be convex.
				// Note: absolute value of an area is used because
				// area may be positive or negative - in accordance with the
				// contour orientation
				if (approx.size() == 4 &&
					fabs(contourArea(approx)) > 1000 &&
					isContourConvex(approx))
				{
					double maxCosine = 0;

					for (int j = 2; j < 5; j++)
					{
						// find the maximum cosine of the angle between joint edges
						double cosine = fabs(angle(approx[j % 4], approx[j - 2], approx[j - 1]));
						maxCosine = MAX(maxCosine, cosine);
					}

					// if cosines of all angles are small
					// (all angles are ~90 degree) then write quandrange
					// vertices to resultant sequence
					if (maxCosine < 0.3)
						squares.push_back(approx);
				}
			}
		}
	}
}

// the function draws all the squares in the image
static void drawSquares(Mat& image, Mat& originalImage, const vector<vector<Point>>& squares)
{
	vector<vector<Point>> squaresCopy = squares;
	for (size_t i = 0; i < squares.size(); i++)
	{
		const Point* p = &squares[i][0];
		int n = (int)squares[i].size();

		int r = 0, g = 0, b = 0;
		int const padding = 5;
		int const sample = 25;
		/*Vec3b color0 = originalImage.at<Vec3b>(Point(squares[i][0].x - padding, squares[i][0].y + padding));
		r += color0[0]; g += color0[1]; b += color0[2];
		Vec3b color1 = originalImage.at<Vec3b>(Point(squares[i][1].x - padding, squares[i][1].y - padding));
		r += color1[0]; g += color1[1]; b += color1[2];
		Vec3b color2 = originalImage.at<Vec3b>(Point(squares[i][2].x + padding, squares[i][2].y - padding));
		r += color2[0]; g += color2[1]; b += color2[2];*/
		Vec3b color3 = originalImage.at<Vec3b>(Point(squares[i][0].x + sample, squares[i][0].y + sample));
		r += color3[0]; g += color3[1]; b += color3[2];
		circle(image, Point(squares[i][3].x + 25, squares[i][3].y + 25), 18, Scalar(r, g, b), -1, LINE_AA);

		polylines(image, &p, &n, 1, true, Scalar(0, 255, 0), 1, LINE_AA);
	}
	imshow(squaresFinder, image);
}

static void compute(int, void*) {
	cvtColor(src, detected_edges, COLOR_BGR2GRAY);
	GaussianBlur(detected_edges, detected_edges, Size(3, 3), 0);
	adaptiveThreshold(detected_edges, detected_edges, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 25, 10);
	cv::bitwise_not(detected_edges, detected_edges);

	Canny(detected_edges, detected_edges, cannyThreshold, cannyThreshold*ratio, kernel_size);

	vector<Vec4i> lines;
	HoughLinesP(detected_edges, lines, 1, CV_PI / 180, houghLinesPThreshold, HoughLinesPa, HoughLinesPb);
	cvtColor(detected_edges, detected_edges, COLOR_GRAY2BGR);
	for (size_t i = 0; i < lines.size(); i++) {
		Vec4i l = lines[i];
		line(detected_edges, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(255, 255, 255), thickness, LINE_AA);
	}

	findSquares(detected_edges, squares);
	src.copyTo(detected_squares);
	drawSquares(detected_squares, src, squares);
	imshow(edgeWindoWname, detected_edges);
}

int main(int, char** argv) {
	namedWindow(edgeWindoWname, WINDOW_AUTOSIZE);
	namedWindow(squaresFinder, WINDOW_AUTOSIZE);
	createTrackbar("EdgeFinderCanny:", edgeWindoWname, &cannyThreshold, barMaximum, compute);
	createTrackbar("squaresFinderTrashhold:", squaresFinder, &squaresFinderTrashhold, barMaximum, compute);
	createTrackbar("squaresFinderIteration:", squaresFinder, &squaresFinderIteration, barMaximum, compute);

	VideoCapture stream1(0);
	if (!stream1.isOpened()) cout << "cannot open camera";
	while (true) {
		stream1.read(src);
		compute(0, 0);
		if (waitKey(30) >= 0)
			break;
	}
	return 0;
}