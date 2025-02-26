//Helper class for plotting points and lines (template implementation)
// Andreas Unterweger, 2017-2018
//This code is licensed under the 3-Clause BSD License. See LICENSE file for details.

#include <algorithm>

//#include "plot.hpp"

namespace imgutils
{
  using namespace std;

  using namespace cv;

  template<typename T>
  PointSet::PointSet(const vector<T> &y_coordinates, const double x_scale, const Vec3b &point_color, const bool interconnect_points, const bool draw_samples, const bool draw_sample_bars)
   : point_color(point_color), interconnect_points(interconnect_points), draw_samples(draw_samples), draw_sample_bars(draw_sample_bars),
     line_width(1)
  {
    size_t i = 0;
    points.resize(y_coordinates.size());
    transform(y_coordinates.begin(), y_coordinates.end(), points.begin(), [&i, &x_scale](const T y_coordinate)
                                                                                        {
                                                                                          return Point2d(x_scale * i++, y_coordinate);
                                                                                        });
  }
}
