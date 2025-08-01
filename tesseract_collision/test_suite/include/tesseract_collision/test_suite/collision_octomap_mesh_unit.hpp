#ifndef TESSERACT_COLLISION_COLLISION_OCTOMAP_MESH_UNIT_HPP
#define TESSERACT_COLLISION_COLLISION_OCTOMAP_MESH_UNIT_HPP

#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <octomap/octomap.h>
#include <console_bridge/console.h>
#include <tesseract_geometry/mesh_parser.h>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include <tesseract_collision/core/discrete_contact_manager.h>
#include <tesseract_collision/core/common.h>
#include <tesseract_geometry/geometries.h>
#include <tesseract_common/resource_locator.h>
#include <tesseract_common/ply_io.h>

namespace tesseract_collision::test_suite
{
namespace detail
{
inline void addCollisionObjects(DiscreteContactManager& checker)
{
  /////////////////////////////////////////////////////////////////
  // Add Octomap
  /////////////////////////////////////////////////////////////////
  tesseract_common::GeneralResourceLocator locator;
  std::string path = locator.locateResource("package://tesseract_support/meshes/box_2m.bt")->getFilePath();
  auto ot = std::make_shared<octomap::OcTree>(path);
  CollisionShapePtr dense_octomap =
      std::make_shared<tesseract_geometry::Octree>(ot, tesseract_geometry::OctreeSubType::BOX);
  Eigen::Isometry3d octomap_pose;
  octomap_pose.setIdentity();

  CollisionShapesConst obj1_shapes;
  tesseract_common::VectorIsometry3d obj1_poses;
  obj1_shapes.push_back(dense_octomap);
  obj1_poses.push_back(octomap_pose);

  checker.addCollisionObject("octomap_link", 0, obj1_shapes, obj1_poses);

  /////////////////////////////////////////////////////////////////
  // Add plane mesh to checker.
  /////////////////////////////////////////////////////////////////
  CollisionShapePtr sphere = tesseract_geometry::createMeshFromPath<tesseract_geometry::Mesh>(
      locator.locateResource("package://tesseract_support/meshes/plane_4m.stl")->getFilePath(),
      Eigen::Vector3d(1, 1, 1),
      true)[0];

  Eigen::Isometry3d sphere_pose;
  sphere_pose.setIdentity();

  CollisionShapesConst obj2_shapes;
  tesseract_common::VectorIsometry3d obj2_poses;
  obj2_shapes.push_back(sphere);
  obj2_poses.push_back(sphere_pose);

  checker.addCollisionObject("plane_link", 0, obj2_shapes, obj2_poses);

  EXPECT_TRUE(checker.getCollisionObjects().size() == 2);
  for (const auto& co : checker.getCollisionObjects())
  {
    EXPECT_TRUE(checker.getCollisionObjectGeometries(co).size() == 1);
    EXPECT_TRUE(checker.getCollisionObjectGeometriesTransforms(co).size() == 1);
    for (const auto& cgt : checker.getCollisionObjectGeometriesTransforms(co))
    {
      EXPECT_TRUE(cgt.isApprox(Eigen::Isometry3d::Identity(), 1e-5));
    }
  }
}
}  // namespace detail

inline void runTest(DiscreteContactManager& checker, const std::string& file_path)
{
  // Add collision object
  detail::addCollisionObjects(checker);

  // Call it again to test adding same object
  detail::addCollisionObjects(checker);

  //////////////////////////////////////
  // Test when object is in collision
  //////////////////////////////////////
  std::vector<std::string> active_links{ "octomap_link", "plane_link" };
  checker.setActiveCollisionObjects(active_links);
  std::vector<std::string> check_active_links = checker.getActiveCollisionObjects();
  EXPECT_TRUE(tesseract_common::isIdentical<std::string>(active_links, check_active_links, false));

  EXPECT_TRUE(checker.getContactAllowedValidator() == nullptr);

  checker.setCollisionMarginData(CollisionMarginData(0.5));
  EXPECT_NEAR(checker.getCollisionMarginData().getMaxCollisionMargin(), 0.5, 1e-5);

  checker.setCollisionMarginPair("octomap_link", "plane_link", 0.1);

  // Set the collision object transforms
  tesseract_common::TransformMap location;
  location["octomap_link"] = Eigen::Isometry3d::Identity();
  location["plane_link"] = Eigen::Isometry3d::Identity();
  location["plane_link"].translation() = Eigen::Vector3d(0, 0, 0);
  checker.setCollisionObjectsTransform(location);

  // Perform collision check
  ContactResultMap result;
  checker.contactTest(result, ContactRequest(ContactTestType::ALL));

  ContactResultVector result_vector;
  result.flattenMoveResults(result_vector);

  const tesseract_collision::CollisionShapesConst& geom = checker.getCollisionObjectGeometries("plane_link");
  const auto& mesh = std::static_pointer_cast<const tesseract_geometry::Mesh>(geom.at(0));
  const auto& mesh_vertices = mesh->getVertices();
  const auto& mesh_triangles = mesh->getFaces();

  // default color is green
  std::vector<Eigen::Vector3i> mesh_vertices_color(mesh_vertices->size(), Eigen::Vector3i(0, 128, 0));

  for (auto& r : result_vector)
  {
    int idx = 0;
    if (r.link_names[0] != "plane_link")
      idx = 1;

    mesh_vertices_color[static_cast<std::size_t>(
        (*mesh_triangles)[4 * r.subshape_id[static_cast<std::size_t>(idx)] + 1])] = Eigen::Vector3i(255, 0, 0);
    mesh_vertices_color[static_cast<std::size_t>(
        (*mesh_triangles)[4 * r.subshape_id[static_cast<std::size_t>(idx)] + 2])] = Eigen::Vector3i(255, 0, 0);
    mesh_vertices_color[static_cast<std::size_t>(
        (*mesh_triangles)[4 * r.subshape_id[static_cast<std::size_t>(idx)] + 3])] = Eigen::Vector3i(255, 0, 0);
  }

  tesseract_common::writeSimplePlyFile(
      file_path, *mesh_vertices, mesh_vertices_color, *mesh_triangles, mesh->getFaceCount());

  EXPECT_TRUE(!result_vector.empty());
  EXPECT_TRUE(result_vector.size() == 2712);
}
}  // namespace tesseract_collision::test_suite

#endif  // TESSERACT_COLLISION_COLLISION_OCTOMAP_MESH_UNIT_HPP
