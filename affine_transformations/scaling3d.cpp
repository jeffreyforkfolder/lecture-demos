//Illustration of 3-D scaling
// Andreas Unterweger, 2017-2018
//This code is licensed under the 3-Clause BSD License. See LICENSE file for details.

#include <iostream>

#include <opencv2/viz.hpp>

#include "conf_viz.hpp"

using namespace std;

using namespace cv;
using namespace viz;

using namespace vizutils;

static constexpr auto transformed_object_name = "Transformed object";
static constexpr auto cone_length = 0.2;
static constexpr auto cone_radius = cone_length / 2;

static void AddCoordinateSystem(ConfigurableVisualization &visualization)
{
  WCoordinateSystem coordinate_system(cone_radius); //TODO: Use adaptive scaling
  visualization.objects.insert(make_pair("Coordinate system", coordinate_system));
}

static void Add3DObjects(ConfigurableVisualization &visualization, const char * const model_filename)
{
  if (model_filename)
  {
    Mesh mesh = Mesh::load(model_filename);
    WMesh original_mesh(mesh);
    original_mesh.setColor(Color::gray());
    original_mesh.setRenderingProperty(OPACITY, 0.5);
    WMesh transformed_mesh(mesh);
    visualization.objects.insert(make_pair("Original object", original_mesh));
    visualization.objects.insert(make_pair(transformed_object_name, transformed_mesh));
  }
  else
  {
    constexpr auto cone_resolution = 100;
    WCone original_cone(cone_length, cone_radius, cone_resolution, Color::gray());
    original_cone.setRenderingProperty(OPACITY, 0.5);
    WCone transformed_cone(cone_length, cone_radius, cone_resolution);
    visualization.objects.insert(make_pair("Original object", original_cone));
    visualization.objects.insert(make_pair(transformed_object_name, transformed_cone));
  }
}

static void AddObjects(ConfigurableVisualization &visualization, const char * const model_filename)
{ 
  AddCoordinateSystem(visualization);
  Add3DObjects(visualization, model_filename);
}

static constexpr auto x_zoom_trackbar_name = "X zoom [%]";
static constexpr auto y_zoom_trackbar_name = "Y zoom [%]";
static constexpr auto z_zoom_trackbar_name = "Z zoom [%]";

static constexpr decltype(x_zoom_trackbar_name) trackbar_names[] { x_zoom_trackbar_name, y_zoom_trackbar_name, z_zoom_trackbar_name };

static void ApplyTransformations(ConfigurableVisualization &visualization)
{
  const auto x_zoom_percent = visualization.GetTrackbarValue(x_zoom_trackbar_name);
  const auto y_zoom_percent = visualization.GetTrackbarValue(y_zoom_trackbar_name);
  const auto z_zoom_percent = visualization.GetTrackbarValue(z_zoom_trackbar_name);
  const Vec3d zoom(x_zoom_percent / 100.0, y_zoom_percent / 100.0, z_zoom_percent / 100.0);
  const Affine3d transformation = Affine3d(Affine3d::Mat3::diag(zoom));
  auto &transformed_object = visualization.objects[transformed_object_name];
  transformed_object.setPose(transformation);
}

static void AddControls(ConfigurableVisualization &visualization)
{
  for (const auto &trackbar_name : trackbar_names)
    visualization.AddTrackbar(trackbar_name, ApplyTransformations, 200, 0, 100);
}

int main(const int argc, const char * const argv[])
{
  if (argc > 2)
  {
    cout << "Illustrates scaling in three dimensions." << endl;
    cout << "Usage: " << argv[0] << " [3-D model (PLY) file name]" << endl;
    return 1;
  }
  const auto model_filename = (argc == 2) ? argv[1] : NULL;
  constexpr auto visualization_window_name = "3-D scaling";
  constexpr auto control_window_name = "3-D scaling parameters";
  ConfigurableVisualization visualization(visualization_window_name, control_window_name);
  AddObjects(visualization, model_filename);
  AddControls(visualization);
  visualization.ShowWindows([&visualization](const Affine3d &pose)
                                            {
                                              const auto old_camera = visualization.GetCamera();
                                              const auto focal_length = old_camera.getFocalLength() / 2; //Reduce focal length so that object is not clipped
                                              const Camera camera(focal_length[0], focal_length[1], old_camera.getPrincipalPoint()[0], old_camera.getPrincipalPoint()[1], old_camera.getWindowSize());
                                              visualization.SetCamera(camera);
                                              return pose;
                                            });
  return 0;
}
