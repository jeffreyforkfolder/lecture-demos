//Illustration of motion estimation and motion compensation
// Andreas Unterweger, 2016-2018
//This code is licensed under the 3-Clause BSD License. See LICENSE file for details.

#include <iostream>
#include <limits>
#include <atomic>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "combine.hpp"
#include "imgmath.hpp"
#include "format.hpp"
#include "colors.hpp"

using namespace std;

using namespace cv;

using namespace imgutils;

typedef struct ME_data
{
  const Mat reference_image;
  const Mat image;
  const Rect search_area;
  const Rect reference_block;
  Point relative_search_position;
  atomic_bool running;
  
  const string me_window_name;
  const string mc_window_name;
  
  ME_data(const Mat &reference_image, const Mat &image, const Rect &search_area, const Rect &reference_block, const string &me_window_name, const string &mc_window_name)
   : reference_image(reference_image), image(image), search_area(search_area), reference_block(reference_block),
     running(false),
     me_window_name(me_window_name), mc_window_name(mc_window_name) {}
} ME_data;

static constexpr unsigned int search_radius = 16;
static constexpr unsigned int block_size = 8;
static_assert(search_radius >= block_size, "Search radius must be larger than block size");

static constexpr unsigned int border_size = 1;
static_assert(border_size < (block_size + 1) / 2, "Border size must be smaller than half the block size");

static Rect ExtendRect(const Rect &rect, const unsigned int border)
{
  return Rect(rect.x - border, rect.y - border, rect.width + 2 * border, rect.height + 2 * border);
}

static void HighlightBlock(Mat &image, const Rect &block, const Scalar &color)
{
  const Rect block_with_border = ExtendRect(block, border_size);
  rectangle(image, block_with_border, color, border_size);
}

static Mat GetAnnotatedReferenceImage(const ME_data &data, Rect &searched_block)
{
  Mat annotated_reference_image;
  cvtColor(data.reference_image, annotated_reference_image, COLOR_GRAY2BGR);
  HighlightBlock(annotated_reference_image, data.search_area, Green);
  HighlightBlock(annotated_reference_image, data.reference_block, Blue);
  searched_block = Rect(data.reference_block.tl() + data.relative_search_position, data.reference_block.size());
  HighlightBlock(annotated_reference_image, searched_block, Red); //Searched position
  return annotated_reference_image;
}

static Mat GetAnnotatedImage(const ME_data &data)
{
  Mat annotated_image;
  cvtColor(data.image, annotated_image, COLOR_GRAY2BGR);
  HighlightBlock(annotated_image, data.reference_block, Blue);
  return annotated_image;
}

static Rect UpdateMotionEstimationImage(const ME_data &data)
{
  const string status_text = "Motion vector: (" + to_string(data.relative_search_position.x) + ", " + to_string(data.relative_search_position.y) + ")";
  displayOverlay(data.me_window_name, status_text, 1000);
  displayStatusBar(data.me_window_name, status_text);
  Rect searched_block;
  const Mat annotated_reference_image = GetAnnotatedReferenceImage(data, searched_block);
  const Mat annotated_image = GetAnnotatedImage(data);
  const Mat combined_image = CombineImages({annotated_reference_image, annotated_image}, Horizontal);
  imshow(data.me_window_name, combined_image);
  return searched_block;
}

static string GetDifferenceMetrics(const Mat &difference, double &representative_metric_value)
{
  const double YSAD = SAD(difference);
  const double YSSD = SSD(difference);
  representative_metric_value = YSSD;
  const double YMSE = YSSD / (block_size * block_size);
  const double YPSNR = PSNR(YMSE);
  return "SAD: " + FormatValue(YSAD) + ", SSD: " + FormatValue(YSSD) + ", MSE: " + FormatValue(YMSE) + ", Y-PSNR: " + FormatLevel(YPSNR);
}

static double UpdateMotionCompensationImage(const ME_data &data, const Rect &searched_block)
{
  const Mat searched_block_pixels = data.reference_image(searched_block);
  const Mat block_pixels = data.image(data.reference_block);
  const Mat compensated_block_pixels_16 = SubtractImages(searched_block_pixels, block_pixels);
  double difference_value;
  const string status_text = GetDifferenceMetrics(compensated_block_pixels_16, difference_value);
  displayStatusBar(data.mc_window_name, status_text);
  const Mat difference_image = ConvertDifferenceImage(compensated_block_pixels_16);
  const Mat combined_image = CombineImages({searched_block_pixels, block_pixels, difference_image}, Horizontal, 1);
  imshow(data.mc_window_name, combined_image);
  return difference_value;
}

static double UpdateImages(ME_data &data)
{
  const Rect searched_block = UpdateMotionEstimationImage(data);
  return UpdateMotionCompensationImage(data, searched_block); //TODO: Image of search range with illustration of SSD values
}

static Rect LimitRect(const Rect &rect, const unsigned int distance)
{
  return Rect(rect.tl(), rect.size() - Size(distance - 1, distance -1)); //Shrink rectangle, but leave one pixel on the right and the bottom for the collision check to work (it considers the bottom-right pixels to be outside the rectangle)
}

static double SetMotionVector(ME_data &data, const Point &MV)
{
  data.relative_search_position = MV;
  return UpdateImages(data);
}

static Rect ExtendRect(const Point &center, const unsigned int border)
{
  return Rect(center.x - border, center.y - border, 2 * border, 2 * border);
}

static void PerformMotionEstimation(ME_data &data)
{
  constexpr auto ME_step_delay = 10; //Animation delay in ms
  const int search_limit = (int)search_radius - block_size / 2;
  double lowest_cost = numeric_limits<double>::max();
  Point best_MV;
  for (int y = -search_limit; y <= search_limit; y++)
  {
    for (int x = -search_limit; x <= search_limit; x++)
    {
      if (!data.running) //Skip the rest when the user aborts
        return;
      const Point current_MV(x, y);
      const double current_cost = SetMotionVector(data, current_MV);
      waitKey(ME_step_delay);
      lowest_cost = min(lowest_cost, current_cost);
      if (current_cost == lowest_cost) //Save the best MV
        best_MV = current_MV;
    }
  }
  SetMotionVector(data, best_MV);
}

static void ShowImages(const Mat &reference_image, const Mat &image, const Point &block_center)
{
  assert(reference_image.size() == image.size());
  constexpr auto me_window_name = "Motion estimation";
  namedWindow(me_window_name);
  moveWindow(me_window_name, 0, 0);
  constexpr auto mc_window_name = "Original block vs. found block vs. motion compensation";
  namedWindow(mc_window_name, WINDOW_NORMAL); //Disable auto-size to enable zooming
  resizeWindow(mc_window_name, (3 * block_size + 2) * 30, block_size * 30); //>30x zoom to illustrate values (MC image shows 3 blocks and two border pixels) //TODO: Why is x30 not sufficient?
  moveWindow(mc_window_name, 2 * image.cols + 3 + 3, 0); //Move MC window right beside the ME window (2 images plus 3 border pixels plus additional distance)
  static ME_data data(reference_image, image, ExtendRect(block_center, search_radius), ExtendRect(block_center, block_size / 2), me_window_name, mc_window_name); //Make variable global so that it is not destroyed after the function returns (for the variable is needed later)
  setMouseCallback(me_window_name, [](const int event, const int x, const int y, const int, void * const userdata)
                                     {
                                       auto &data = *((ME_data * const)userdata);
                                       if (!data.running && event == EVENT_LBUTTONUP) //Only react when the left mouse button is being pressed while no motion estimation is running
                                       {
                                         const Point mouse_point(x + border_size, y + border_size); //Assume mouse position is in the top-left of the search block (on the outside border which is border_size pixels in width)
                                         if (LimitRect(data.search_area, block_size).contains(mouse_point)) //If the mouse is within the search area (minus the positions on the bottom which would lead to the search block exceeding the borders)...
                                           SetMotionVector(data, mouse_point - data.reference_block.tl()); //... set the search position according to the mouse position
                                       }
                                     }, (void*)&data);
  constexpr auto perform_button_name = "Perform ME";
  createButton(perform_button_name, [](const int, void * const user_data)
                                      {
                                        auto &data = *((ME_data * const)user_data);
                                        if (!data.running)
                                        {
                                          data.running = true;
                                          PerformMotionEstimation(data);
                                          data.running = false;
                                        }
                                      }, (void*)&data, QT_PUSH_BUTTON);
  constexpr auto stop_button_name = "Stop ME";
  createButton(stop_button_name, [](const int, void * const user_data)
                                   {
                                     auto &data = *((ME_data * const)user_data);
                                     data.running = false;
                                   }, (void*)&data, QT_PUSH_BUTTON);
  SetMotionVector(data, Point()); //Set MV to (0, 0) (implies imshow)
}

static int CheckParameters(const Mat &reference_image, const Mat &image, const Point &block_origin)
{
  if (reference_image.size() != image.size())
  {
    cerr << "Both images must have the same size" << endl;
    return 10;
  }
  if (static_cast<unsigned int>(reference_image.rows) < 2 * search_radius)
  {
    cerr << "The images must be larger than " << to_string(2 * search_radius) << " pixels in each dimension" << endl;
    return 11;
  }
  if (static_cast<unsigned int>(block_origin.x) < search_radius || static_cast<unsigned int>(block_origin.x) > reference_image.cols - search_radius - 1)
  {
    cerr << "Block center X coordinate must be between " << search_radius << " and " << (reference_image.cols - search_radius - 1) << endl;
    return 12;
  }
  if (static_cast<unsigned int>(block_origin.y) < search_radius || static_cast<unsigned int>(block_origin.y) > reference_image.rows - search_radius - 1)
  {
    cerr << "Block center Y coordinate must be between " << search_radius << " and " << (reference_image.rows - search_radius - 1) << endl;
    return 13;
  }
  return 0;
}

int main(const int argc, const char * const argv[])
{
  if (argc != 5)
  {
    cout << "Illustrates motion estimation and motion compensation." << endl;
    cout << "Usage: " << argv[0] << " <reference image> <input image> <block center X coordinate> <block center Y coordinate>" << endl;
    return 1;
  }
  const auto reference_image_filename = argv[1];
  const Mat reference_image = imread(reference_image_filename, IMREAD_GRAYSCALE);
  if (reference_image.empty())
  {
    cerr << "Could not read reference image '" << reference_image_filename << "'" << endl;
    return 2;
  }
  const auto image_filename = argv[2];
  const Mat image = imread(image_filename, IMREAD_GRAYSCALE);
  if (image.empty())
  {
    cerr << "Could not read input image '" << reference_image_filename << "'" << endl;
    return 3;
  }
  Point block_origin;
  const auto x_coordinate = argv[3];
  block_origin.x = atoi(x_coordinate);
  const auto y_coordinate = argv[4];
  block_origin.y = atoi(y_coordinate);
  int ret;
  if ((ret = CheckParameters(reference_image, image, block_origin)) != 0)
    return ret;
  ShowImages(reference_image, image, block_origin);
  waitKey(0);
  return 0;
}
