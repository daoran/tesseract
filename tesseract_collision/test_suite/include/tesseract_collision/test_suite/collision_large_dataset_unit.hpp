#ifndef TESSERACT_COLLISION_COLLISION_LARGE_DATASET_UNIT_HPP
#define TESSERACT_COLLISION_COLLISION_LARGE_DATASET_UNIT_HPP

#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <console_bridge/console.h>
#include <chrono>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include <tesseract_collision/bullet/convex_hull_utils.h>
#include <tesseract_collision/core/discrete_contact_manager.h>
#include <tesseract_collision/core/common.h>
#include <tesseract_geometry/geometries.h>
#include <tesseract_common/resource_locator.h>
#include <tesseract_common/ply_io.h>

namespace tesseract_collision::test_suite
{
inline void runTest(DiscreteContactManager& checker, bool use_convex_mesh = false, int num_interations = 10)
{
  // Add Meshed Sphere to checker
  CollisionShapePtr sphere;

  if (use_convex_mesh)
  {
    auto mesh_vertices = std::make_shared<tesseract_common::VectorVector3d>();
    auto mesh_faces = std::make_shared<Eigen::VectorXi>();
    tesseract_common::GeneralResourceLocator locator;
    EXPECT_GT(tesseract_common::loadSimplePlyFile(
                  locator.locateResource("package://tesseract_support/meshes/sphere_p25m.ply")->getFilePath(),
                  *mesh_vertices,
                  *mesh_faces,
                  true),
              0);

    auto mesh = std::make_shared<tesseract_geometry::Mesh>(mesh_vertices, mesh_faces);
    sphere = makeConvexMesh(*mesh);
  }
  else
  {
    sphere = std::make_shared<tesseract_geometry::Sphere>(0.25);
  }

  double delta = 0.55;

  std::size_t t = 5;  // Because of unit test runtime this was reduced from 10 to 5.
  std::vector<std::string> link_names;
  tesseract_common::TransformMap location;
  for (std::size_t x = 0; x < t; ++x)
  {
    for (std::size_t y = 0; y < t; ++y)
    {
      for (std::size_t z = 0; z < t; ++z)
      {
        CollisionShapesConst obj3_shapes;
        tesseract_common::VectorIsometry3d obj3_poses;
        Eigen::Isometry3d sphere_pose;
        sphere_pose.setIdentity();

        obj3_shapes.push_back(CollisionShapePtr(sphere->clone()));
        obj3_poses.push_back(sphere_pose);

        link_names.push_back("sphere_link_" + std::to_string(x) + std::to_string(y) + std::to_string(z));

        location[link_names.back()] = sphere_pose;
        location[link_names.back()].translation() = Eigen::Vector3d(
            static_cast<double>(x) * delta, static_cast<double>(y) * delta, static_cast<double>(z) * delta);
        checker.addCollisionObject(link_names.back(), 0, obj3_shapes, obj3_poses);
      }
    }
  }

  // Check if they are in collision
  checker.setActiveCollisionObjects(link_names);
  std::vector<std::string> check_active_links = checker.getActiveCollisionObjects();
  EXPECT_TRUE(tesseract_common::isIdentical<std::string>(link_names, check_active_links, false));

  EXPECT_TRUE(checker.getContactAllowedValidator() == nullptr);

  checker.setCollisionMarginData(CollisionMarginData(0.1));
  EXPECT_NEAR(checker.getCollisionMarginData().getMaxCollisionMargin(), 0.1, 1e-5);
  checker.setCollisionObjectsTransform(location);

  ContactResultMap result;
  ContactResultVector result_vector;

  auto start_time = std::chrono::high_resolution_clock::now();
  num_interations = 1;
  for (auto i = 0; i < num_interations; ++i)
  {
    result.clear();
    result_vector.clear();
    checker.contactTest(result, ContactRequest(ContactTestType::ALL));
    result.flattenMoveResults(result_vector);

    if (result_vector.size() != 300)
      for (const auto& result : result_vector)
        std::cout << result.link_names[0] << "," << result.link_names[1] << "," << result.distance << "\n";

    EXPECT_EQ(result_vector.size(), 300);
  }
  auto end_time = std::chrono::high_resolution_clock::now();

  CONSOLE_BRIDGE_logInform("DT: %f ms", std::chrono::duration<double, std::milli>(end_time - start_time).count());
}
}  // namespace tesseract_collision::test_suite
#endif  // TESSERACT_COLLISION_COLLISION_LARGE_DATASET_UNIT_HPP
