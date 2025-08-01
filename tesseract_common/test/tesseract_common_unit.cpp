#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <gtest/gtest.h>
#include <type_traits>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <tinyxml2.h>
#include <sstream>
#include <thread>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include <tesseract_common/utils.h>
#include <tesseract_common/ply_io.h>
#include <tesseract_common/sfinae_utils.h>
#include <tesseract_common/resource_locator.h>
#include <tesseract_common/manipulator_info.h>
#include <tesseract_common/joint_state.h>
#include <tesseract_common/types.h>
#include <tesseract_common/any_poly.h>
#include <tesseract_common/kinematic_limits.h>
#include <tesseract_common/yaml_utils.h>
#include <tesseract_common/yaml_extensions.h>
#include <tesseract_common/collision_margin_data.h>
#include <tesseract_common/stopwatch.h>
#include <tesseract_common/timer.h>
#include <tesseract_common/profile.h>
#include <tesseract_common/profile_dictionary.h>

/** @brief Resource locator implementation using a provided function to locate file resources */
class TestResourceLocator : public tesseract_common::ResourceLocator
{
public:
  using Ptr = std::shared_ptr<TestResourceLocator>;
  using ConstPtr = std::shared_ptr<const TestResourceLocator>;

  ~TestResourceLocator() override = default;

  tesseract_common::Resource::Ptr locateResource(const std::string& url) const override final
  {
    std::string mod_url = url;
    if (url.find("package://tesseract_common") == 0)
    {
      mod_url.erase(0, strlen("package://tesseract_common"));
      size_t pos = mod_url.find('/');
      if (pos == std::string::npos)
        return nullptr;

      mod_url.erase(0, pos);

      std::filesystem::path file_path(__FILE__);
      std::string package_path = file_path.parent_path().parent_path().string();

      if (package_path.empty())
        return nullptr;

      mod_url = package_path + mod_url;
    }

    if (!std::filesystem::path(mod_url).is_absolute())
      return nullptr;

    return std::make_shared<tesseract_common::SimpleLocatedResource>(
        url, mod_url, std::make_shared<TestResourceLocator>(*this));
  }
};

class TestProfile : public tesseract_common::Profile
{
public:
  TestProfile() = default;
  ~TestProfile() override = default;
  TestProfile(std::size_t key) : Profile(key) {}
  TestProfile(const TestProfile&) = default;
  TestProfile& operator=(const TestProfile&) = default;
  TestProfile(TestProfile&&) = default;
  TestProfile& operator=(TestProfile&&) = default;

  bool operator==(const TestProfile& rhs) const { return (key_ == rhs.key_); }
  bool operator!=(const TestProfile& rhs) const { return !operator==(rhs); }
};

TEST(TesseractCommonUnit, ProfileDictionary)  // NOLINT
{
  tesseract_common::ProfileDictionary profiles;

  const std::string ns{ "test_ns" };
  const std::string profile_name{ "test_profile" };
  const std::string profile_name_2{ "test_profile_2" };
  const std::size_t profile_key{ 100 };
  auto profile = std::make_shared<const TestProfile>(profile_key);
  auto profile2 = std::make_shared<const TestProfile>(2 * profile_key);
  profiles.addProfile(ns, profile_name, profile);
  profiles.addProfile(ns, profile_name_2, profile);
  profiles.addProfile(ns, profile_name, profile2);
  profiles.addProfile(ns, profile_name_2, profile2);

  // Constructors
  auto data = profiles.getAllProfileEntries();
  tesseract_common::ProfileDictionary profiles_copy(profiles);
  EXPECT_EQ(data, profiles_copy.getAllProfileEntries());
  tesseract_common::ProfileDictionary profiles_copy_assign;
  profiles_copy_assign = profiles;
  EXPECT_EQ(data, profiles_copy_assign.getAllProfileEntries());
  tesseract_common::ProfileDictionary profiles_move(std::move(profiles_copy));
  EXPECT_EQ(data, profiles_move.getAllProfileEntries());
  tesseract_common::ProfileDictionary profiles_move_assign;
  profiles_move_assign = std::move(profiles_copy_assign);
  EXPECT_EQ(data, profiles_move_assign.getAllProfileEntries());

  EXPECT_FALSE(profiles.hasProfile(profile_key, ns, "does_not_exist"));
  EXPECT_FALSE(profiles.hasProfile(profile_key, "does_not_exist", profile_name));
  EXPECT_FALSE(profiles.hasProfile(0, ns, profile_name));
  EXPECT_TRUE(profiles.hasProfile(profile_key, ns, profile_name));
  EXPECT_TRUE(profiles.hasProfile(profile_key, ns, profile_name_2));
  EXPECT_TRUE(profiles.hasProfile(2 * profile_key, ns, profile_name));
  EXPECT_TRUE(profiles.hasProfile(2 * profile_key, ns, profile_name_2));
  EXPECT_TRUE(profile == profiles.getProfile(profile_key, ns, profile_name));
  EXPECT_TRUE(profile == profiles.getProfile(profile_key, ns, profile_name_2));
  EXPECT_TRUE(profile2 == profiles.getProfile(2 * profile_key, ns, profile_name));
  EXPECT_TRUE(profile2 == profiles.getProfile(2 * profile_key, ns, profile_name_2));
  EXPECT_TRUE(profiles.hasProfileEntry(profile_key, ns));
  EXPECT_TRUE(profiles.hasProfileEntry(2 * profile_key, ns));
  EXPECT_FALSE(profiles.hasProfileEntry(profile_key, "does_not_exist"));
  EXPECT_FALSE(profiles.hasProfileEntry(0, ns));
  EXPECT_EQ(profiles.getProfileEntry(profile_key, ns).size(), 2);
  EXPECT_EQ(profiles.getProfileEntry(2 * profile_key, ns).size(), 2);

  // Remove profile wich does not exist
  profiles.removeProfile(profile_key, ns, "does_not_exist");
  EXPECT_EQ(profiles.getProfileEntry(profile_key, ns).size(), 2);
  profiles.removeProfile(profile_key, "does_not_exist", profile_name);
  EXPECT_EQ(profiles.getProfileEntry(profile_key, ns).size(), 2);
  EXPECT_EQ(profiles.getProfileEntry(2 * profile_key, ns).size(), 2);

  // Remove profile which does exist
  profiles.removeProfile(profile_key, ns, profile_name);
  EXPECT_EQ(profiles.getProfileEntry(profile_key, ns).size(), 1);
  EXPECT_EQ(profiles.getProfileEntry(2 * profile_key, ns).size(), 2);
  profiles.removeProfile(profile_key, ns, profile_name_2);
  EXPECT_FALSE(profiles.hasProfileEntry(profile_key, ns));
  EXPECT_EQ(profiles.getProfileEntry(2 * profile_key, ns).size(), 2);

  profiles.removeProfile(2 * profile_key, ns, profile_name);
  EXPECT_FALSE(profiles.hasProfileEntry(profile_key, ns));
  EXPECT_EQ(profiles.getProfileEntry(2 * profile_key, ns).size(), 1);
  profiles.removeProfile(2 * profile_key, ns, profile_name_2);
  EXPECT_FALSE(profiles.hasProfileEntry(profile_key, ns));
  EXPECT_FALSE(profiles.hasProfileEntry(2 * profile_key, ns));
  EXPECT_TRUE(profiles.getAllProfileEntries().empty());

  // Test clear
  profiles.addProfile(ns, std::vector<std::string>{ profile_name, profile_name_2 }, profile);
  profiles.addProfile(ns, std::vector<std::string>{ profile_name, profile_name_2 }, profile2);
  EXPECT_EQ(profiles.getProfileEntry(profile_key, ns).size(), 2);
  EXPECT_EQ(profiles.getProfileEntry(2 * profile_key, ns).size(), 2);
  EXPECT_FALSE(profiles.getAllProfileEntries().empty());
  profiles.clear();
  EXPECT_TRUE(profiles.getAllProfileEntries().empty());

  profiles.addProfile(ns, std::vector<std::string>{ profile_name }, profile);
  profiles.addProfile(ns, std::vector<std::string>{ profile_name_2 }, profile);
  profiles.addProfile(ns, std::vector<std::string>{ profile_name }, profile2);
  profiles.addProfile(ns, std::vector<std::string>{ profile_name_2 }, profile2);

  EXPECT_TRUE(profiles.hasProfileEntry(profile_key, ns));
  profiles.removeProfileEntry(profile_key, "does_not_exist");
  EXPECT_TRUE(profiles.hasProfileEntry(profile_key, ns));
  profiles.removeProfileEntry(profile_key, ns);
  EXPECT_FALSE(profiles.hasProfileEntry(profile_key, ns));

  EXPECT_TRUE(profiles.hasProfileEntry(2 * profile_key, ns));
  profiles.removeProfileEntry(2 * profile_key, "does_not_exist");
  EXPECT_TRUE(profiles.hasProfileEntry(2 * profile_key, ns));
  profiles.removeProfileEntry(2 * profile_key, ns);
  EXPECT_FALSE(profiles.hasProfileEntry(2 * profile_key, ns));

  EXPECT_TRUE(profiles.getAllProfileEntries().empty());

  // Test failures
  EXPECT_ANY_THROW(profiles.addProfile(ns, std::vector<std::string>{ "" }, profile));  // NOLINT
  EXPECT_TRUE(profiles.getAllProfileEntries().empty());

  profiles.addProfile(ns, std::vector<std::string>{ profile_name }, profile);
  profiles.addProfile(ns, std::vector<std::string>{ profile_name_2 }, profile);

  EXPECT_ANY_THROW(profiles.addProfile("", profile_name, profile));                              // NOLINT
  EXPECT_ANY_THROW(profiles.addProfile(ns, "", profile));                                        // NOLINT
  EXPECT_ANY_THROW(profiles.addProfile(ns, profile_name, nullptr));                              // NOLINT
  EXPECT_ANY_THROW(profiles.addProfile("", std::vector<std::string>{ profile_name }, profile));  // NOLINT
  EXPECT_ANY_THROW(profiles.addProfile(ns, std::vector<std::string>(), profile));                // NOLINT
  EXPECT_ANY_THROW(profiles.addProfile(ns, std::vector<std::string>{ "" }, profile));            // NOLINT
  EXPECT_ANY_THROW(profiles.addProfile(ns, std::vector<std::string>{ "" }, profile2));           // NOLINT
  EXPECT_ANY_THROW(profiles.addProfile(ns, std::vector<std::string>{ profile_name }, nullptr));  // NOLINT
  EXPECT_ANY_THROW(profiles.getProfileEntry(profile_key, "does_not_exist"));                     // NOLINT
  EXPECT_ANY_THROW(profiles.getProfileEntry(0, ns));                                             // NOLINT
}

TEST(TesseractCommonUnit, isNumeric)  // NOLINT
{
  std::vector<std::string> true_test = { "1",     "1.5",  "-1",     "-1.5",  "1e-5",    "1e5",
                                         "-1e-5", "-1e5", "1.0e-5", "1.0e5", "-1.0e-5", "-1.0e5" };

  EXPECT_TRUE(tesseract_common::isNumeric(true_test));
  for (const auto& s : true_test)
  {
    EXPECT_TRUE(tesseract_common::isNumeric(s));
  }

  std::vector<std::string> false_test = { "a", "test sdfs", "1 2", "1.0 2.0", "+", "-", "=" };
  EXPECT_FALSE(tesseract_common::isNumeric(false_test));
  for (const auto& s : false_test)
  {
    EXPECT_FALSE(tesseract_common::isNumeric(s));
  }

  std::string empty_string;
  EXPECT_FALSE(tesseract_common::isNumeric(empty_string));
}

TEST(TesseractCommonUnit, toNumeric)  // NOLINT
{
  std::vector<std::string> true_test = { "1",     "1.5",  "-1",     "-1.5",  "1e-5",    "1e5",
                                         "-1e-5", "-1e5", "1.0e-5", "1.0e5", "-1.0e-5", "-1.0e5" };

  std::vector<double> true_test_value = { 1, 1.5, -1, -1.5, 1e-5, 1e5, -1e-5, -1e5, 1.0e-5, 1.0e5, -1.0e-5, -1.0e5 };

  EXPECT_TRUE(tesseract_common::isNumeric(true_test));
  for (size_t i = 0; i < true_test.size(); ++i)
  {
    double value = 0;
    EXPECT_TRUE(tesseract_common::toNumeric<double>(true_test[i], value));
    EXPECT_NEAR(value, true_test_value[i], 1e-8);
  }

  std::vector<std::string> false_test = { "a", "test sdfs", "1 2", "1.0 2.0", "+", "-", "=" };
  EXPECT_FALSE(tesseract_common::isNumeric(false_test));
  for (const auto& s : false_test)
  {
    double value = 0;
    EXPECT_FALSE(tesseract_common::toNumeric(s, value));
    EXPECT_NEAR(value, 0, 1e-8);
  }

  std::string empty_string;
  double value = 0;
  EXPECT_FALSE(tesseract_common::toNumeric(empty_string, value));
}

TEST(TesseractCommonUnit, generateRandomNumber)  // NOLINT
{
  Eigen::MatrixX2d limits(4, 2);
  limits(0, 0) = -5;
  limits(0, 1) = 5;
  limits(1, 0) = 0;
  limits(1, 1) = 10;
  limits(2, 0) = 5;
  limits(2, 1) = 15;
  limits(3, 0) = -15;
  limits(3, 1) = -5;

  Eigen::VectorXd random_numbers = tesseract_common::generateRandomNumber(limits);
  EXPECT_EQ(limits.rows(), random_numbers.rows());
  for (long i = 0; i < limits.rows(); ++i)
  {
    EXPECT_LE(random_numbers(i), limits(i, 1));
    EXPECT_GE(random_numbers(i), limits(i, 0));
  }

  Eigen::MatrixX2d empty_limits;
  Eigen::VectorXd random_numbers2 = tesseract_common::generateRandomNumber(empty_limits);
  EXPECT_EQ(empty_limits.rows(), random_numbers2.rows());

  Eigen::MatrixX2d equal_limits(4, 2);
  equal_limits(0, 0) = 5;
  equal_limits(0, 1) = 5;
  equal_limits(1, 0) = 5;
  equal_limits(1, 1) = 5;
  equal_limits(2, 0) = 5;
  equal_limits(2, 1) = 5;
  equal_limits(3, 0) = 5;
  equal_limits(3, 1) = 5;
  Eigen::VectorXd random_numbers3 = tesseract_common::generateRandomNumber(equal_limits);
  EXPECT_EQ(equal_limits.rows(), random_numbers3.rows());
  for (long i = 0; i < equal_limits.rows(); ++i)
  {
    EXPECT_NEAR(random_numbers3(i), 5, 1e-5);
  }

  Eigen::MatrixX2d wrong_limits(4, 2);
  wrong_limits(0, 0) = 5;
  wrong_limits(0, 1) = -5;
  wrong_limits(1, 0) = 5;
  wrong_limits(1, 1) = -5;
  wrong_limits(2, 0) = 5;
  wrong_limits(2, 1) = -5;
  wrong_limits(3, 0) = 5;
  wrong_limits(3, 1) = -5;
  Eigen::VectorXd random_numbers4 = tesseract_common::generateRandomNumber(wrong_limits);
  EXPECT_EQ(wrong_limits.rows(), random_numbers4.rows());
  for (long i = 0; i < limits.rows(); ++i)
  {
    EXPECT_GE(random_numbers4(i), wrong_limits(i, 1));
    EXPECT_LE(random_numbers4(i), wrong_limits(i, 0));
  }
}

TEST(TesseractCommonUnit, trim)  // NOLINT
{
  std::string check1 = "    trim";
  std::string check2 = "trim    ";
  std::string check3 = "    trim    ";
  std::string check_trimmed = "trim";

  std::string s = check1;
  tesseract_common::rtrim(s);
  EXPECT_EQ(s, check1);
  tesseract_common::ltrim(s);
  EXPECT_EQ(s, check_trimmed);

  s = check2;
  tesseract_common::ltrim(s);
  EXPECT_EQ(s, check2);
  tesseract_common::rtrim(s);
  EXPECT_EQ(s, check_trimmed);

  s = check1;
  tesseract_common::trim(s);
  EXPECT_EQ(s, check_trimmed);

  s = check2;
  tesseract_common::trim(s);
  EXPECT_EQ(s, check_trimmed);

  s = check3;
  tesseract_common::trim(s);
  EXPECT_EQ(s, check_trimmed);
}

struct TestHasMemberFunction
{
  bool update() const { return true; }    // NOLINT
  int add(int a) const { return a + 1; }  // NOLINT
};

struct TestHasMemberWithArgFunction
{
  bool update(std::shared_ptr<TestHasMemberWithArgFunction>& p) { return (p == nullptr); }  // NOLINT
  double add(double a, double b) const { return a + b; }                                    // NOLINT
};

struct TestMissingMemberFunction
{
  bool missingUpdate() const { return false; }  // NOLINT
  double add(int a) const { return a + 1; }     // NOLINT
};

CREATE_MEMBER_CHECK(update);
CREATE_MEMBER_FUNC_INVOCABLE_CHECK(update, std::shared_ptr<T>&);
CREATE_MEMBER_FUNC_INVOCABLE_CHECK(add, double, double);
CREATE_MEMBER_FUNC_RETURN_TYPE_CHECK(add, int, int);
CREATE_MEMBER_FUNC_SIGNATURE_CHECK(add, double, double, double);

TEST(TesseractCommonUnit, sfinaeHasMemberFunction)  // NOLINT
{
  bool t_true = has_member_update<TestHasMemberFunction>::value;
  bool t_false = has_member_update<TestMissingMemberFunction>::value;
  EXPECT_TRUE(t_true);
  EXPECT_FALSE(t_false);
}

TEST(TesseractCommonUnit, sfinaeHasMemberFunctionInvocable)  // NOLINT
{
  bool i_update_true = has_member_func_invocable_update<TestHasMemberWithArgFunction>::value;
  bool i_add_true = has_member_func_invocable_add<TestHasMemberWithArgFunction>::value;
  bool i_update_false = has_member_func_invocable_update<TestHasMemberFunction>::value;
  bool i_add_false = has_member_func_invocable_add<TestHasMemberFunction>::value;
  EXPECT_TRUE(i_update_true);
  EXPECT_TRUE(i_add_true);
  EXPECT_FALSE(i_update_false);
  EXPECT_FALSE(i_add_false);
}

TEST(TesseractCommonUnit, sfinaeHasMemberFunctionWithReturnType)  // NOLINT
{
  bool i_add_true = has_member_func_return_type_add<TestHasMemberFunction>::value;
  bool t_add_false = has_member_func_return_type_add<TestMissingMemberFunction>::value;
  EXPECT_TRUE(i_add_true);
  EXPECT_FALSE(t_add_false);
}

TEST(TesseractCommonUnit, sfinaeHasMemberFunctionSignature)  // NOLINT
{
  bool i_add_true = has_member_func_signature_add<TestHasMemberWithArgFunction>::value;
  bool t_add_false = has_member_func_signature_add<TestMissingMemberFunction>::value;
  EXPECT_TRUE(i_add_true);
  EXPECT_FALSE(t_add_false);
}

TEST(TesseractCommonUnit, bytesResource)  // NOLINT
{
  std::vector<uint8_t> data;
  data.reserve(8);
  for (uint8_t i = 0; i < 8; i++)
    data.push_back(i);

  std::shared_ptr<tesseract_common::BytesResource> bytes_resource =
      std::make_shared<tesseract_common::BytesResource>("package://test_package/data.bin", data);
  EXPECT_EQ(bytes_resource->getUrl(), "package://test_package/data.bin");
  EXPECT_EQ(bytes_resource->isFile(), false);
  EXPECT_EQ(bytes_resource->getFilePath(), "");
  EXPECT_EQ(bytes_resource->locateResource("test"), nullptr);
  auto data2 = bytes_resource->getResourceContents();
  ASSERT_EQ(data.size(), data2.size());
  for (size_t i = 0; i < data.size(); i++)
  {
    EXPECT_EQ(data[i], data2[i]);
  }
  auto data2_stream = bytes_resource->getResourceContentStream();
  for (unsigned char& i : data)
  {
    char data2_val{ 0 };
    data2_stream->read(&data2_val, 1);
    EXPECT_EQ(i, *reinterpret_cast<uint8_t*>(&data2_val));  // NOLINT
  }

  std::shared_ptr<tesseract_common::BytesResource> bytes_resource2 =
      std::make_shared<tesseract_common::BytesResource>("package://test_package/data.bin", data.data(), data.size());
  EXPECT_EQ(bytes_resource2->getUrl(), "package://test_package/data.bin");
  EXPECT_EQ(bytes_resource->getResourceContents().size(), data.size());
}

TEST(TesseractCommonUnit, fileToString)  // NOLINT
{
  tesseract_common::ResourceLocator::Ptr locator = std::make_shared<TestResourceLocator>();
  tesseract_common::Resource::Ptr resource = locator->locateResource("package://tesseract_common/package.xml");
  std::string data = tesseract_common::fileToString(std::filesystem::path(resource->getFilePath()));
  EXPECT_FALSE(data.empty());
}

TEST(TesseractCommonUnit, stopwatch)  // NOLINT
{
  tesseract_common::Stopwatch stopwatch;
  stopwatch.start();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  auto elapsed_ms = stopwatch.elapsedMilliseconds();
  auto elapsed_s = stopwatch.elapsedSeconds();
  EXPECT_GT(elapsed_ms, 999);
  EXPECT_GT(elapsed_s, 0.999);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  stopwatch.stop();
  elapsed_ms = stopwatch.elapsedMilliseconds();
  elapsed_s = stopwatch.elapsedSeconds();
  EXPECT_GT(elapsed_ms, 1999);
  EXPECT_GT(elapsed_s, 1.999);
}

TEST(TesseractCommonUnit, timer)  // NOLINT
{
  int counter{ 0 };
  auto callback = [&counter]() { ++counter; };
  std::chrono::steady_clock::duration interval(std::chrono::milliseconds(1));
  tesseract_common::Timer timer;
  timer.start(callback, interval);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  timer.stop();
  EXPECT_GT(counter, 900);
}

TEST(TesseractCommonUnit, ManipulatorInfo)  // NOLINT
{
  // Empty tcp
  tesseract_common::ManipulatorInfo manip_info;
  EXPECT_TRUE(manip_info.empty());
  EXPECT_TRUE(manip_info.tcp_frame.empty());
  EXPECT_TRUE(manip_info.manipulator.empty());
  EXPECT_TRUE(manip_info.manipulator_ik_solver.empty());
  EXPECT_TRUE(manip_info.working_frame.empty());

  tesseract_common::ManipulatorInfo manip_info_override("manipulator", "world", "tool0");
  manip_info_override.tcp_offset = Eigen::Isometry3d::Identity() * Eigen::Translation3d(0.0, 0.0, 0.25);
  manip_info_override.manipulator_ik_solver = "OPWInvKin";
  manip_info_override.working_frame = "base_link";

  manip_info = manip_info.getCombined(manip_info_override);
  EXPECT_FALSE(manip_info.empty());
  EXPECT_TRUE(manip_info.tcp_frame == manip_info_override.tcp_frame);
  EXPECT_EQ(manip_info.manipulator, manip_info_override.manipulator);
  EXPECT_EQ(manip_info.manipulator_ik_solver, manip_info_override.manipulator_ik_solver);
  EXPECT_EQ(manip_info.working_frame, manip_info_override.working_frame);

  // Test empty method
  {
    tesseract_common::ManipulatorInfo manip_info("manip", "world", "");
    EXPECT_TRUE(manip_info.empty());
  }

  {
    tesseract_common::ManipulatorInfo manip_info("manip", "", "tool0");
    EXPECT_TRUE(manip_info.empty());
  }

  {
    tesseract_common::ManipulatorInfo manip_info("", "world", "tool0");
    EXPECT_TRUE(manip_info.empty());
  }

  {
    tesseract_common::ManipulatorInfo manip_info("", "", "");
    manip_info.manipulator_ik_solver = "manip";
    EXPECT_TRUE(manip_info.empty());
  }
}

TEST(TesseractCommonUnit, JointStateTest)  // NOLINT
{
  std::vector<std::string> joint_names{ "joint_1", "joint_2", "joint_3" };
  Eigen::VectorXd positons = Eigen::VectorXd::Constant(3, 5);
  tesseract_common::JointState joint_state(joint_names, positons);
  EXPECT_TRUE(joint_state.joint_names == joint_names);
  EXPECT_TRUE(joint_state.position.isApprox(positons, 1e-5));
}

TEST(TesseractCommonUnit, anyUnit)  // NOLINT
{
  tesseract_common::AnyPoly any_null;
  EXPECT_TRUE(any_null.getType() == std::type_index(typeid(nullptr)));
  EXPECT_TRUE(any_null.isNull());

  tesseract_common::AnyPoly any_type;
  EXPECT_TRUE(any_type.getType() == std::type_index(typeid(nullptr)));
  EXPECT_TRUE(any_type.isNull());

  tesseract_common::AnyPoly any_double(1.5);
  EXPECT_TRUE(any_double.getType() == std::type_index(typeid(double)));
  EXPECT_FALSE(any_double.isNull());

  EXPECT_TRUE(any_null == any_type);

  tesseract_common::JointState joint_state;
  joint_state.joint_names = { "joint_1", "joint_2", "joint_3" };
  joint_state.position = Eigen::VectorXd::Constant(3, 5);
  joint_state.velocity = Eigen::VectorXd::Constant(3, 6);
  joint_state.acceleration = Eigen::VectorXd::Constant(3, 7);
  joint_state.effort = Eigen::VectorXd::Constant(3, 8);
  joint_state.time = 100;

  any_type = joint_state;
  EXPECT_TRUE(any_type.getType() == std::type_index(typeid(tesseract_common::JointState)));
  EXPECT_FALSE(any_type.isNull());
  EXPECT_TRUE(any_type.as<tesseract_common::JointState>() == joint_state);

  // Expect False
  EXPECT_FALSE(any_type == any_null);
  EXPECT_FALSE(any_type == any_double);

  tesseract_common::AnyInterface& interface = any_type.get();
  EXPECT_TRUE(interface.getType() == std::type_index(typeid(tesseract_common::JointState)));

  const tesseract_common::AnyInterface& interface_const = std::as_const(any_type).get();
  EXPECT_TRUE(interface_const.getType() == std::type_index(typeid(tesseract_common::JointState)));

  // Construct with interface
  tesseract_common::AnyPoly any_interface_copy(interface_const);
  EXPECT_TRUE(any_interface_copy.getType() == std::type_index(typeid(tesseract_common::JointState)));
  EXPECT_FALSE(any_interface_copy.isNull());
  EXPECT_TRUE(any_interface_copy.as<tesseract_common::JointState>() == joint_state);

  // Check clone
  tesseract_common::AnyPoly any_copy = any_type;
  EXPECT_TRUE(any_copy == any_type);

  tesseract_common::AnyPoly any_copy2;
  any_copy2 = any_type;
  EXPECT_TRUE(any_copy2 == any_type);

  // Check to make sure it is not making a copy during cast
  auto& any_type_ref1 = any_type.as<tesseract_common::JointState>();
  auto& any_type_ref2 = any_type.as<tesseract_common::JointState>();
  auto& any_copy_ref = any_copy.as<tesseract_common::JointState>();
  EXPECT_TRUE(&any_type_ref1 == &any_type_ref2);
  EXPECT_TRUE(&any_type_ref1 != &any_copy_ref);
  EXPECT_TRUE(&any_type_ref2 != &any_copy_ref);

  const auto& any_type_const_ref1 = any_type.as<tesseract_common::JointState>();
  const auto& any_type_const_ref2 = any_type.as<tesseract_common::JointState>();
  EXPECT_TRUE(&any_type_const_ref1 == &any_type_const_ref2);

  {
    std::ofstream os(tesseract_common::getTempPath() + "any_type_boost.xml");
    boost::archive::xml_oarchive oa(os);
    oa << BOOST_SERIALIZATION_NVP(any_type);
  }

  tesseract_common::AnyPoly nany_type;
  {
    std::ifstream ifs(tesseract_common::getTempPath() + "any_type_boost.xml");
    assert(ifs.good());
    boost::archive::xml_iarchive ia(ifs);

    // restore the schedule from the archive
    ia >> BOOST_SERIALIZATION_NVP(nany_type);
  }

  EXPECT_TRUE(nany_type.getType() == std::type_index(typeid(tesseract_common::JointState)));
  EXPECT_TRUE(nany_type.as<tesseract_common::JointState>() == joint_state);

// Test bad cast
#ifndef _MSC_VER
  // Horrible compilation errors on MSVC
  EXPECT_ANY_THROW(nany_type.as<tesseract_common::Toolpath>());  // NOLINT
#endif
}

template <typename T>
void runAnyPolyIntegralTest(T value, const std::string& type_str)
{
  tesseract_common::AnyPoly any_type{ value };
  EXPECT_TRUE(any_type.getType() == std::type_index(typeid(T)));
  EXPECT_TRUE(any_type.as<T>() == value);

  // Check clone
  tesseract_common::AnyPoly any_copy = any_type;
  EXPECT_TRUE(any_copy == any_type);
  EXPECT_TRUE(any_copy.as<T>() == value);

  const std::string filepath{ tesseract_common::getTempPath() + "any_" + type_str + "_type_boost.xml" };
  {
    std::ofstream os(filepath);
    boost::archive::xml_oarchive oa(os);
    oa << BOOST_SERIALIZATION_NVP(any_type);
  }

  tesseract_common::AnyPoly nany_type;
  {
    std::ifstream ifs(filepath);
    assert(ifs.good());
    boost::archive::xml_iarchive ia(ifs);

    // restore the schedule from the archive
    ia >> BOOST_SERIALIZATION_NVP(nany_type);
  }

  EXPECT_TRUE(nany_type.getType() == std::type_index(typeid(T)));
  EXPECT_TRUE(nany_type.as<T>() == value);

// Test bad cast
#ifndef _MSC_VER
  // Horrible compilation errors on MSVC
  EXPECT_ANY_THROW(nany_type.as<tesseract_common::Toolpath>());  // NOLINT
#endif
}

TEST(TesseractCommonUnit, anyIntegralTypesUnit)  // NOLINT
{
  runAnyPolyIntegralTest<bool>(true, "bool");
  runAnyPolyIntegralTest<int>(-10, "int");
  runAnyPolyIntegralTest<unsigned>(5, "unsigned");
  runAnyPolyIntegralTest<double>(1.2, "double");
  runAnyPolyIntegralTest<float>(-0.2F, "float");
  runAnyPolyIntegralTest<std::string>("this", "string");
  runAnyPolyIntegralTest<std::size_t>(10, "std_size_t");
}

template <typename T>
void runAnyPolyUnorderedMapIntegralTest(T value, const std::string& type_str)
{
  std::unordered_map<std::string, T> data;
  data["test"] = std::move(value);
  tesseract_common::AnyPoly any_type{ data };
  EXPECT_TRUE(any_type.getType() == std::type_index(typeid(std::unordered_map<std::string, T>)));
  bool check = any_type.as<std::unordered_map<std::string, T>>() == data;
  EXPECT_TRUE(check);

  // Check clone
  tesseract_common::AnyPoly any_copy = any_type;
  EXPECT_TRUE(any_copy == any_type);
  check = any_copy.as<std::unordered_map<std::string, T>>() == data;
  EXPECT_TRUE(check);

  const std::string filepath{ tesseract_common::getTempPath() + "any_unordered_map_string_" + type_str +
                              "_type_boost.xml" };
  {
    std::ofstream os(filepath);
    boost::archive::xml_oarchive oa(os);
    oa << BOOST_SERIALIZATION_NVP(any_type);
  }

  tesseract_common::AnyPoly nany_type;
  {
    std::ifstream ifs(filepath);
    assert(ifs.good());
    boost::archive::xml_iarchive ia(ifs);

    // restore the schedule from the archive
    ia >> BOOST_SERIALIZATION_NVP(nany_type);
  }

  EXPECT_TRUE(nany_type.getType() == std::type_index(typeid(std::unordered_map<std::string, T>)));
  check = nany_type.as<std::unordered_map<std::string COMMA T>>() == data;
  EXPECT_TRUE(check);

  // Test bad cast
#ifndef _MSC_VER
  // Horrible compilation errors on MSVC
  EXPECT_ANY_THROW(nany_type.as<tesseract_common::Toolpath>());  // NOLINT
#endif
}

TEST(TesseractCommonUnit, anyUnorderedMapIntegralTypesUnit)  // NOLINT
{
  runAnyPolyUnorderedMapIntegralTest<bool>(true, "bool");
  runAnyPolyUnorderedMapIntegralTest<int>(-10, "int");
  runAnyPolyUnorderedMapIntegralTest<unsigned>(5, "unsigned");
  runAnyPolyUnorderedMapIntegralTest<double>(1.2, "double");
  runAnyPolyUnorderedMapIntegralTest<float>(-0.2F, "float");
  runAnyPolyUnorderedMapIntegralTest<std::string>("this", "string");
  runAnyPolyUnorderedMapIntegralTest<std::size_t>(10, "std_size_t");
}

TEST(TesseractCommonUnit, anySharedPtrUnit)  // NOLINT
{
  tesseract_common::AnyPoly any_type;
  EXPECT_TRUE(any_type.getType() == std::type_index(typeid(nullptr)));

  tesseract_common::JointState joint_state;
  joint_state.joint_names = { "joint_1", "joint_2", "joint_3" };
  joint_state.position = Eigen::VectorXd::Constant(3, 5);
  joint_state.velocity = Eigen::VectorXd::Constant(3, 6);
  joint_state.acceleration = Eigen::VectorXd::Constant(3, 7);
  joint_state.effort = Eigen::VectorXd::Constant(3, 8);
  joint_state.time = 100;

  auto joint_state_ptr = std::make_shared<tesseract_common::JointState>(joint_state);
  any_type = joint_state_ptr;
  EXPECT_TRUE(any_type.getType() == std::type_index(typeid(std::shared_ptr<tesseract_common::JointState>)));
  EXPECT_TRUE(*any_type.as<std::shared_ptr<tesseract_common::JointState>>() == joint_state);
  EXPECT_TRUE(any_type.as<std::shared_ptr<tesseract_common::JointState>>() == joint_state_ptr);

  // Check clone
  tesseract_common::AnyPoly any_copy = any_type;
  EXPECT_TRUE(any_copy == any_type);

  // Check to make sure it is not making a copy during cast
  auto& any_type_ref1 = any_type.as<std::shared_ptr<tesseract_common::JointState>>();
  auto& any_type_ref2 = any_type.as<std::shared_ptr<tesseract_common::JointState>>();
  auto& any_copy_ref = any_copy.as<std::shared_ptr<tesseract_common::JointState>>();
  EXPECT_TRUE(&any_type_ref1 == &any_type_ref2);
  EXPECT_TRUE(&any_type_ref1 != &any_copy_ref);
  EXPECT_TRUE(&any_type_ref2 != &any_copy_ref);

  const auto& any_type_const_ref1 = any_type.as<std::shared_ptr<tesseract_common::JointState>>();
  const auto& any_type_const_ref2 = any_type.as<std::shared_ptr<tesseract_common::JointState>>();
  EXPECT_TRUE(&any_type_const_ref1 == &any_type_const_ref2);

  {
    std::ofstream os(tesseract_common::getTempPath() + "any_shared_ptr_type_boost.xml");
    boost::archive::xml_oarchive oa(os);
    oa << BOOST_SERIALIZATION_NVP(any_type);
  }

  tesseract_common::AnyPoly nany_type;
  {
    std::ifstream ifs(tesseract_common::getTempPath() + "any_shared_ptr_type_boost.xml");
    assert(ifs.good());
    boost::archive::xml_iarchive ia(ifs);

    // restore the schedule from the archive
    ia >> BOOST_SERIALIZATION_NVP(nany_type);
  }

  EXPECT_TRUE(nany_type.getType() == std::type_index(typeid(std::shared_ptr<tesseract_common::JointState>)));
  EXPECT_TRUE(*nany_type.as<std::shared_ptr<tesseract_common::JointState>>() == joint_state);

  // Test bad cast
  EXPECT_ANY_THROW(nany_type.as<tesseract_common::JointState>());  // NOLINT
}

TEST(TesseractCommonUnit, boundsUnit)  // NOLINT
{
  Eigen::VectorXd v = Eigen::VectorXd::Ones(6);
  v = v.array() + std::numeric_limits<float>::epsilon();
  Eigen::MatrixX2d limits(6, 2);  // NOLINT(clang-analyzer-core.uninitialized.UndefReturn)
  limits.col(0) = -Eigen::VectorXd::Ones(6);
  limits.col(1) = Eigen::VectorXd::Ones(6);

  EXPECT_FALSE(tesseract_common::isWithinLimits<double>(v, limits));
  EXPECT_TRUE(tesseract_common::satisfiesLimits<double>(v, limits, std::numeric_limits<float>::epsilon()));
  EXPECT_FALSE(tesseract_common::satisfiesLimits<double>(v, limits, std::numeric_limits<double>::epsilon()));
  tesseract_common::enforceLimits<double>(v, limits);
  EXPECT_TRUE(tesseract_common::satisfiesLimits<double>(v, limits, std::numeric_limits<double>::epsilon()));

  v = -Eigen::VectorXd::Ones(6);
  v = v.array() - std::numeric_limits<float>::epsilon();

  EXPECT_FALSE(tesseract_common::isWithinLimits<double>(v, limits));
  EXPECT_TRUE(tesseract_common::satisfiesLimits<double>(v, limits, std::numeric_limits<float>::epsilon()));
  EXPECT_FALSE(tesseract_common::satisfiesLimits<double>(v, limits, std::numeric_limits<double>::epsilon()));
  tesseract_common::enforceLimits<double>(v, limits);
  EXPECT_TRUE(tesseract_common::satisfiesLimits<double>(v, limits, std::numeric_limits<double>::epsilon()));

  // Check that clamp is done correctly on both sides
  v = Eigen::VectorXd::Constant(6, -2);
  EXPECT_FALSE(tesseract_common::isWithinLimits<double>(v, limits));
  EXPECT_FALSE(tesseract_common::satisfiesLimits<double>(v, limits, std::numeric_limits<double>::epsilon()));
  tesseract_common::enforceLimits<double>(v, limits);
  ASSERT_EQ((v - limits.col(0)).norm(), 0);

  v = Eigen::VectorXd::Constant(6, 2);
  EXPECT_FALSE(tesseract_common::isWithinLimits<double>(v, limits));
  EXPECT_FALSE(tesseract_common::satisfiesLimits<double>(v, limits, std::numeric_limits<double>::epsilon()));
  tesseract_common::enforceLimits<double>(v, limits);
  ASSERT_EQ((v - limits.col(1)).norm(), 0);

  v = Eigen::VectorXd::Ones(6);
  v = v.array() - std::numeric_limits<float>::epsilon();
  EXPECT_TRUE(tesseract_common::isWithinLimits<double>(v, limits));

  v = Eigen::VectorXd::Ones(6);
  v(3) = v(3) + std::numeric_limits<float>::epsilon();
  EXPECT_FALSE(tesseract_common::isWithinLimits<double>(v, limits));

  v = -Eigen::VectorXd::Ones(6);
  v(3) = v(3) - std::numeric_limits<float>::epsilon();
  EXPECT_FALSE(tesseract_common::isWithinLimits<double>(v, limits));
}

TEST(TesseractCommonUnit, isIdenticalUnit)  // NOLINT
{
  std::vector<std::string> v1{ "a", "b", "c" };
  std::vector<std::string> v2{ "a", "b", "c" };
  EXPECT_TRUE(tesseract_common::isIdentical(v1, v2, false));
  EXPECT_TRUE(tesseract_common::isIdentical(v1, v2, true));

  v2 = { "c", "b", "a" };
  EXPECT_TRUE(tesseract_common::isIdentical(v1, v2, false));
  EXPECT_FALSE(tesseract_common::isIdentical(v1, v2, true));

  v2 = { "a", "b", "d" };
  EXPECT_FALSE(tesseract_common::isIdentical(v1, v2, false));
  EXPECT_FALSE(tesseract_common::isIdentical(v1, v2, true));
}

TEST(TesseractCommonUnit, isIdenticalMapUnit)  // NOLINT
{
  std::map<std::string, int> v1;
  v1["1"] = 1;
  v1["2"] = 2;
  std::map<std::string, int> v2;
  bool equal = tesseract_common::isIdenticalMap<std::map<std::string, int>, int>(v1, v2);
  EXPECT_FALSE(equal);

  v2["2"] = 2;
  equal = tesseract_common::isIdenticalMap<std::map<std::string, int>, int>(v1, v2);
  EXPECT_FALSE(equal);

  v2 = v1;
  equal = tesseract_common::isIdenticalMap<std::map<std::string, int>, int>(v1, v2);
  EXPECT_TRUE(equal);

  v1.clear();
  equal = tesseract_common::isIdenticalMap<std::map<std::string, int>, int>(v1, v2);
  EXPECT_FALSE(equal);
}

TEST(TesseractCommonUnit, isIdenticalSetUnit)  // NOLINT
{
  std::set<int> v1;
  std::set<int> v2;
  bool equal = tesseract_common::isIdenticalSet<int>(v1, v2);
  EXPECT_TRUE(equal);

  v1.insert(1);
  equal = tesseract_common::isIdenticalSet<int>(v1, v2);
  EXPECT_FALSE(equal);

  v2.insert(1);
  v2.insert(2);
  equal = tesseract_common::isIdenticalSet<int>(v1, v2);
  EXPECT_FALSE(equal);

  v1.insert(2);
  equal = tesseract_common::isIdenticalSet<int>(v1, v2);
  EXPECT_TRUE(equal);
}

TEST(TesseractCommonUnit, isIdenticalArrayUnit)  // NOLINT
{
  {
    std::array<int, 4> v1 = { 1, 2, 3, 4 };
    std::array<int, 4> v2 = { 1, 2, 3, 4 };
    bool equal = tesseract_common::isIdenticalArray<int, 4>(v1, v2);
    EXPECT_TRUE(equal);
  }
  {
    std::array<int, 4> v1 = { 1, 2, 3, 4 };
    std::array<int, 4> v2 = { -1, 2, 3, 4 };
    bool equal = tesseract_common::isIdenticalArray<int, 4>(v1, v2);
    EXPECT_FALSE(equal);
  }
  {
    // Clang-tidy catches initialized arrays anyway, but check it just in case the caller isn't running clang-tidy
    std::array<int, 4> v1 = { 1, 2, 3, 6 };
    std::array<int, 4> v2;  // NOLINT
    bool equal = tesseract_common::isIdenticalArray<int, 4>(v1, v2);
    EXPECT_FALSE(equal);
  }
}

TEST(TesseractCommonUnit, pointersEqual)  // NOLINT
{
  {
    auto p1 = std::make_shared<int>(1);
    auto p2 = std::make_shared<int>(2);
    bool equal = tesseract_common::pointersEqual(p1, p2);
    EXPECT_FALSE(equal);
  }
  {
    auto p1 = std::make_shared<int>(1);
    auto p2 = nullptr;
    bool equal = tesseract_common::pointersEqual<int>(p1, p2);
    EXPECT_FALSE(equal);
  }
  {
    auto p1 = nullptr;
    auto p2 = std::make_shared<int>(2);
    bool equal = tesseract_common::pointersEqual<int>(p1, p2);
    EXPECT_FALSE(equal);
  }
  {
    auto p1 = nullptr;
    auto p2 = nullptr;
    bool equal = tesseract_common::pointersEqual<int>(p1, p2);
    EXPECT_TRUE(equal);
  }
  {
    auto p1 = std::make_shared<int>(1);
    auto p2 = std::make_shared<int>(1);
    bool equal = tesseract_common::pointersEqual<int>(p1, p2);
    EXPECT_TRUE(equal);
  }
}

TEST(TesseractCommonUnit, pointersComparison)  // NOLINT
{
  // True if p1 < p2
  {
    auto p1 = std::make_shared<int>(1);
    auto p2 = std::make_shared<int>(2);
    bool equal = tesseract_common::pointersComparison<int>(p1, p2);
    EXPECT_TRUE(equal);
  }
  {
    auto p1 = std::make_shared<int>(1);
    auto p2 = nullptr;
    bool equal = tesseract_common::pointersComparison<int>(p1, p2);
    EXPECT_FALSE(equal);
  }
  {
    auto p1 = nullptr;
    auto p2 = std::make_shared<int>(2);
    bool equal = tesseract_common::pointersComparison<int>(p1, p2);
    EXPECT_TRUE(equal);
  }
  {
    auto p1 = nullptr;
    auto p2 = nullptr;
    bool equal = tesseract_common::pointersComparison<int>(p1, p2);
    EXPECT_FALSE(equal);
  }
  {
    auto p1 = std::make_shared<int>(1);
    auto p2 = std::make_shared<int>(1);
    bool equal = tesseract_common::pointersComparison<int>(p1, p2);
    EXPECT_FALSE(equal);
  }
}

TEST(TesseractCommonUnit, getTimestampStringUnit)  // NOLINT
{
  std::string s1 = tesseract_common::getTimestampString();
  EXPECT_FALSE(s1.empty());
}

TEST(TesseractCommonUnit, reorder)  // NOLINT
{
  std::vector<std::vector<Eigen::Index>> checks;
  checks.push_back({ 5, 4, 3, 2, 1, 0 });
  checks.push_back({ 0, 1, 2, 3, 4, 5 });
  checks.push_back({ 3, 2, 4, 1, 5, 0 });
  Eigen::VectorXd v = Eigen::VectorXd::Random(6);

  for (const auto& check : checks)
  {
    Eigen::VectorXd v_copy = v;
    tesseract_common::reorder(v_copy, check);

    for (std::size_t i = 0; i < check.size(); ++i)
    {
      EXPECT_NEAR(v_copy(static_cast<Eigen::Index>(i)), v(check[i]), 1e-8);
    }
  }
}

TEST(TesseractCommonUnit, getTempPathUnit)  // NOLINT
{
  std::string s1 = tesseract_common::getTempPath();
  EXPECT_FALSE(s1.empty());
  EXPECT_TRUE(std::filesystem::exists(s1));
}

TEST(TesseractCommonUnit, QueryStringValueUnit)  // NOLINT
{
  {
    std::string str = R"(<box>Test</box>)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    std::string string_value;
    int status = tesseract_common::QueryStringValue(element, string_value);
    EXPECT_TRUE(status == tinyxml2::XML_SUCCESS);
    EXPECT_TRUE(string_value == "box");
  }
}

TEST(TesseractCommonUnit, QueryStringTextUnit)  // NOLINT
{
  {
    std::string str = R"(<box>Test</box>)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    std::string string_value;
    int status = tesseract_common::QueryStringText(element, string_value);
    EXPECT_TRUE(status == tinyxml2::XML_SUCCESS);
    EXPECT_TRUE(string_value == "Test");
  }

  {
    std::string str = R"(<box></box>)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    std::string string_value;
    int status = tesseract_common::QueryStringText(element, string_value);
    EXPECT_TRUE(status == tinyxml2::XML_NO_ATTRIBUTE);
  }
}

TEST(TesseractCommonUnit, QueryStringAttributeUnit)  // NOLINT
{
  {
    std::string str = R"(<box name="test" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    std::string string_value;
    int status = tesseract_common::QueryStringAttribute(element, "name", string_value);
    EXPECT_TRUE(status == tinyxml2::XML_SUCCESS);
    EXPECT_TRUE(string_value == "test");
  }

  {
    std::string str = R"(<box missing="test" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    std::string string_value;
    int status = tesseract_common::QueryStringAttribute(element, "name", string_value);
    EXPECT_TRUE(status == tinyxml2::XML_NO_ATTRIBUTE);
  }
}

TEST(TesseractCommonUnit, StringAttributeUnit)  // NOLINT
{
  {
    std::string str = R"(<box name="test" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    std::string string_value = tesseract_common::StringAttribute(element, "name", "default");
    EXPECT_TRUE(string_value == "test");
  }

  {
    std::string str = R"(<box name="test" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    std::string string_value = tesseract_common::StringAttribute(element, "missing", "default");
    EXPECT_TRUE(string_value == "default");
  }
}

TEST(TesseractCommonUnit, QueryStringAttributeRequiredUnit)  // NOLINT
{
  {
    std::string str = R"(<box name="test" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    std::string string_value;
    int status = tesseract_common::QueryStringAttributeRequired(element, "name", string_value);
    EXPECT_TRUE(status == tinyxml2::XML_SUCCESS);
    EXPECT_TRUE(string_value == "test");
  }

  {
    std::string str = R"(<box name="test" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    std::string string_value;
    int status = tesseract_common::QueryStringAttributeRequired(element, "missing", string_value);
    EXPECT_TRUE(status == tinyxml2::XML_NO_ATTRIBUTE);
  }
}

TEST(TesseractCommonUnit, QueryDoubleAttributeRequiredUnit)  // NOLINT
{
  {
    std::string str = R"(<box name="1.5" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    double double_value{ 0 };
    int status = tesseract_common::QueryDoubleAttributeRequired(element, "name", double_value);
    EXPECT_TRUE(status == tinyxml2::XML_SUCCESS);
    EXPECT_NEAR(double_value, 1.5, 1e-6);
  }

  {
    std::string str = R"(<box name="1.5" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    double double_value{ 0 };
    int status = tesseract_common::QueryDoubleAttributeRequired(element, "missing", double_value);
    EXPECT_TRUE(status == tinyxml2::XML_NO_ATTRIBUTE);
  }

  {
    std::string str = R"(<box name="abc" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    double double_value{ 0 };
    int status = tesseract_common::QueryDoubleAttributeRequired(element, "name", double_value);
    EXPECT_TRUE(status == tinyxml2::XML_WRONG_ATTRIBUTE_TYPE);
  }
}

TEST(TesseractCommonUnit, QueryIntAttributeRequiredUnit)  // NOLINT
{
  {
    std::string str = R"(<box name="1" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    int int_value{ 0 };
    int status = tesseract_common::QueryIntAttributeRequired(element, "name", int_value);
    EXPECT_TRUE(status == tinyxml2::XML_SUCCESS);
    EXPECT_NEAR(int_value, 1, 1e-6);
  }

  {
    std::string str = R"(<box name="1.5" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    int int_value{ 0 };
    int status = tesseract_common::QueryIntAttributeRequired(element, "missing", int_value);
    EXPECT_TRUE(status == tinyxml2::XML_NO_ATTRIBUTE);
  }

  {
    std::string str = R"(<box name="abc" />)";
    tinyxml2::XMLDocument xml_doc;
    EXPECT_TRUE(xml_doc.Parse(str.c_str()) == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* element = xml_doc.FirstChildElement("box");
    EXPECT_TRUE(element != nullptr);

    int int_value{ 0 };
    int status = tesseract_common::QueryIntAttributeRequired(element, "name", int_value);
    EXPECT_TRUE(status == tinyxml2::XML_WRONG_ATTRIBUTE_TYPE);
  }
}

// sample function that catches an exception and wraps it in a nested exception
void runThrowNestedException()
{
  try
  {
    throw std::runtime_error("failed");
  }
  catch (...)
  {
    std::throw_with_nested(std::runtime_error("runThrowNestedException() failed"));
  }
}

TEST(TesseractCommonUnit, printNestedExceptionUnit)  // NOLINT
{
  try
  {
    runThrowNestedException();
  }
  catch (const std::exception& e)
  {
    tesseract_common::printNestedException(e);
  }
}

TEST(TesseractCommonUnit, checkForUnknownKeys)  // NOLINT
{
  std::set<std::string> expected_keys{ "search_paths", "search_libraries" };

  std::string yaml_string = R"(kinematic_plugins:
                                 search_paths:
                                   - /usr/local/lib
                                 search_libraries:
                                   - tesseract_kinematics_kdl_factories)";

  YAML::Node config = tesseract_common::fromYAMLString(yaml_string);
  EXPECT_NO_THROW(tesseract_common::checkForUnknownKeys(config["kinematic_plugins"], expected_keys));  // NOLINT

  // Not a map
  EXPECT_ANY_THROW(
      tesseract_common::checkForUnknownKeys(config["kinematic_plugins"]["search_paths"], expected_keys));  // NOLINT

  expected_keys = { "search_paths" };
  EXPECT_ANY_THROW(tesseract_common::checkForUnknownKeys(config["kinematic_plugins"], expected_keys));  // NOLINT
}

TEST(TesseractCommonUnit, almostEqualRelativeAndAbsUnit)  // NOLINT
{
  double a = 1e-5;
  double b = 0;
  EXPECT_FALSE(tesseract_common::almostEqualRelativeAndAbs(a, b));

  a = 1e-7;
  EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(a, b));

  a = 100000000000000.01;
  b = 100000000000000;
  EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(a, b));

  a = 100000000000000.1;
  b = 100000000000000;
  EXPECT_FALSE(tesseract_common::almostEqualRelativeAndAbs(a, b));

  Eigen::VectorXd v1 = Eigen::VectorXd::Constant(3, 1e-5);
  Eigen::VectorXd v2 = Eigen::VectorXd::Constant(3, 0);
  EXPECT_FALSE(tesseract_common::almostEqualRelativeAndAbs(v1, v2));

  v1 = Eigen::VectorXd::Constant(3, 1e-7);
  EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(v1, v2));

  v1 = Eigen::VectorXd::Constant(3, 100000000000000.01);
  v2 = Eigen::VectorXd::Constant(3, 100000000000000);
  EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(v1, v2));

  v1 = Eigen::VectorXd::Constant(3, 100000000000000.1);
  v2 = Eigen::VectorXd::Constant(3, 100000000000000);
  EXPECT_FALSE(tesseract_common::almostEqualRelativeAndAbs(v1, v2));

  v2 = Eigen::VectorXd::Constant(1, 100000000000000);
  EXPECT_FALSE(tesseract_common::almostEqualRelativeAndAbs(v1, v2));

  EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(Eigen::VectorXd(), Eigen::VectorXd()));
}

TEST(TesseractCommonUnit, kinematicsPluginInfoUnit)  // NOLINT
{
  tesseract_common::KinematicsPluginInfo kpi;
  EXPECT_TRUE(kpi.empty());

  tesseract_common::KinematicsPluginInfo kpi_insert;
  kpi_insert.search_paths.insert("/usr/local/lib");
  kpi_insert.search_libraries.insert("tesseract_collision");

  {
    tesseract_common::PluginInfo pi;
    pi.class_name = "KDLFwdKin";
    kpi_insert.fwd_plugin_infos["manipulator"].plugins = { std::make_pair("KDLFwdKin", pi) };
    kpi_insert.fwd_plugin_infos["manipulator"].default_plugin = "KDLFwdKin";
  }

  {
    tesseract_common::PluginInfo pi;
    pi.class_name = "KDLInvKin";
    kpi_insert.inv_plugin_infos["manipulator"].plugins = { std::make_pair("KDLInvKin", pi) };
    kpi_insert.inv_plugin_infos["manipulator"].default_plugin = "KDLInvKin";
  }

  EXPECT_FALSE(kpi_insert.empty());

  EXPECT_NE(kpi, kpi_insert);
  kpi.insert(kpi_insert);
  EXPECT_FALSE(kpi.empty());
  EXPECT_EQ(kpi, kpi_insert);

  kpi.clear();
  EXPECT_TRUE(kpi.empty());
}

TEST(TesseractCommonUnit, ContactManagersPluginInfoUnit)  // NOLINT
{
  tesseract_common::ContactManagersPluginInfo cmpi;
  EXPECT_TRUE(cmpi.empty());

  tesseract_common::ContactManagersPluginInfo cmpi_insert;
  cmpi_insert.search_paths.insert("/usr/local/lib");
  cmpi_insert.search_libraries.insert("tesseract_collision");

  {
    tesseract_common::PluginInfo pi;
    pi.class_name = "DiscretePluginFactory";
    cmpi_insert.discrete_plugin_infos.plugins = { std::make_pair("DiscretePlugin", pi) };
    cmpi_insert.discrete_plugin_infos.default_plugin = "DiscretePlugin";
  }

  {
    tesseract_common::PluginInfo pi;
    pi.class_name = "ContinuousPluginFactory";
    cmpi_insert.continuous_plugin_infos.plugins = { std::make_pair("ContinuousPlugin", pi) };
    cmpi_insert.continuous_plugin_infos.default_plugin = "ContinuousPlugin";
  }

  EXPECT_FALSE(cmpi_insert.empty());

  EXPECT_NE(cmpi, cmpi_insert);
  cmpi.insert(cmpi_insert);
  EXPECT_FALSE(cmpi.empty());
  EXPECT_EQ(cmpi, cmpi_insert);

  cmpi.clear();
  EXPECT_TRUE(cmpi.empty());
}

TEST(TesseractCommonUnit, ProfilePluginInfoUnit)  // NOLINT
{
  tesseract_common::ProfilesPluginInfo kpi;
  EXPECT_TRUE(kpi.empty());

  tesseract_common::ProfilesPluginInfo kpi_insert;
  kpi_insert.search_paths.insert("/usr/local/lib");
  kpi_insert.search_libraries.insert("tesseract_collision");

  {
    tesseract_common::PluginInfo pi;
    pi.class_name = "Profile";
    kpi_insert.plugin_infos["manipulator"]["KDLFwdKin"] = pi;
  }

  {
    tesseract_common::PluginInfo pi;
    pi.class_name = "Profile";
    kpi_insert.plugin_infos["manipulator"]["KDLInvKin"] = pi;
  }

  EXPECT_FALSE(kpi_insert.empty());

  EXPECT_NE(kpi, kpi_insert);
  kpi.insert(kpi_insert);
  EXPECT_FALSE(kpi.empty());
  EXPECT_EQ(kpi, kpi_insert);

  kpi.clear();
  EXPECT_TRUE(kpi.empty());
}

TEST(TesseractCommonUnit, TaskComposerPluginInfoUnit)  // NOLINT
{
  tesseract_common::TaskComposerPluginInfo tcpi;
  EXPECT_TRUE(tcpi.empty());

  tesseract_common::TaskComposerPluginInfo tcpi_insert;
  tcpi_insert.search_paths.insert("/usr/local/lib");
  tcpi_insert.search_libraries.insert("tesseract_collision");

  {
    tesseract_common::PluginInfo pi;
    pi.class_name = "TaskComposerExecutorPluginFactory";
    tcpi_insert.executor_plugin_infos.plugins = { std::make_pair("TaskComposerExecutorPlugin", pi) };
    tcpi_insert.executor_plugin_infos.default_plugin = "TaskComposerExecutorPlugin";
  }

  {
    tesseract_common::PluginInfo pi;
    pi.class_name = "TaskComposerNodePluginFactory";
    tcpi_insert.task_plugin_infos.plugins = { std::make_pair("TaskComposerNodePlugin", pi) };
    tcpi_insert.task_plugin_infos.default_plugin = "TaskComposerNodePlugin";
  }

  EXPECT_FALSE(tcpi_insert.empty());

  EXPECT_NE(tcpi, tcpi_insert);
  tcpi.insert(tcpi_insert);
  EXPECT_FALSE(tcpi.empty());
  EXPECT_EQ(tcpi, tcpi_insert);

  tcpi.clear();
  EXPECT_TRUE(tcpi.empty());
}

TEST(TesseractPluginFactoryUnit, KinematicsPluginInfoYamlUnit)  // NOLINT
{
  std::string yaml_string = R"(kinematic_plugins:
                                 search_paths:
                                   - /usr/local/lib
                                 search_libraries:
                                   - tesseract_kinematics_kdl_factories
                                 fwd_kin_plugins:
                                   iiwa_manipulator:
                                     default: KDLFwdKinChain
                                     plugins:
                                       KDLFwdKinChain:
                                         class: KDLFwdKinChainFactory
                                         config:
                                           base_link: base_link
                                           tip_link: tool0
                                 inv_kin_plugins:
                                   iiwa_manipulator:
                                     default: KDLInvKinChainLMA
                                     plugins:
                                       KDLInvKinChainLMA:
                                         class: KDLInvKinChainLMAFactory
                                         config:
                                           base_link: base_link
                                           tip_link: tool0
                                       KDLInvKinChainNR:
                                         class: KDLInvKinChainNRFactory
                                         config:
                                           base_link: base_link
                                           tip_link: tool0)";

  {  // Success
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::KinematicsPluginInfo::CONFIG_KEY];
    auto cmpi = config.as<tesseract_common::KinematicsPluginInfo>();

    const YAML::Node& plugin_info = plugin_config["kinematic_plugins"];
    const YAML::Node& search_paths = plugin_info["search_paths"];
    const YAML::Node& search_libraries = plugin_info["search_libraries"];
    const YAML::Node& fwd_kin_default_plugin = plugin_info["fwd_kin_plugins"]["iiwa_manipulator"]["default"];
    const YAML::Node& fwd_kin_plugins = plugin_info["fwd_kin_plugins"]["iiwa_manipulator"]["plugins"];
    const YAML::Node& inv_kin_default_plugin = plugin_info["inv_kin_plugins"]["iiwa_manipulator"]["default"];
    const YAML::Node& inv_kin_plugins = plugin_info["inv_kin_plugins"]["iiwa_manipulator"]["plugins"];

    {
      std::set<std::string> sp = cmpi.search_paths;
      EXPECT_EQ(sp.size(), 1);

      for (auto it = search_paths.begin(); it != search_paths.end(); ++it)
      {
        EXPECT_TRUE(sp.find(it->as<std::string>()) != sp.end());
      }
    }

    {
      std::set<std::string> sl = cmpi.search_libraries;
      EXPECT_EQ(sl.size(), 1);

      for (auto it = search_libraries.begin(); it != search_libraries.end(); ++it)
      {
        EXPECT_TRUE(sl.find(it->as<std::string>()) != sl.end());
      }
    }

    EXPECT_EQ(fwd_kin_default_plugin.as<std::string>(), cmpi.fwd_plugin_infos["iiwa_manipulator"].default_plugin);
    EXPECT_EQ(fwd_kin_plugins.size(), cmpi.fwd_plugin_infos["iiwa_manipulator"].plugins.size());

    EXPECT_EQ(inv_kin_default_plugin.as<std::string>(), cmpi.inv_plugin_infos["iiwa_manipulator"].default_plugin);
    EXPECT_EQ(inv_kin_plugins.size(), cmpi.inv_plugin_infos["iiwa_manipulator"].plugins.size());
  }

  {  // search_paths failure
    std::string yaml_string = R"(kinematic_plugins:
                                   search_paths:
                                     failure: issue
                                   search_libraries:
                                     - tesseract_kinematics_kdl_factories
                                   fwd_kin_plugins:
                                     iiwa_manipulator:
                                       default: KDLFwdKinChain
                                       plugins:
                                         KDLFwdKinChain:
                                           class: KDLFwdKinChainFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0
                                   inv_kin_plugins:
                                     iiwa_manipulator:
                                       default: KDLInvKinChainLMA
                                       plugins:
                                         KDLInvKinChainLMA:
                                           class: KDLInvKinChainLMAFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0
                                         KDLInvKinChainNR:
                                           class: KDLInvKinChainNRFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0)";

    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::KinematicsPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::KinematicsPluginInfo>());  // NOLINT
  }

  {  // search_libraries failure
    std::string yaml_string = R"(kinematic_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     failure: issue
                                   fwd_kin_plugins:
                                     iiwa_manipulator:
                                       default: KDLFwdKinChain
                                       plugins:
                                         KDLFwdKinChain:
                                           class: KDLFwdKinChainFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0
                                   inv_kin_plugins:
                                     iiwa_manipulator:
                                       default: KDLInvKinChainLMA
                                       plugins:
                                         KDLInvKinChainLMA:
                                           class: KDLInvKinChainLMAFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0
                                         KDLInvKinChainNR:
                                           class: KDLInvKinChainNRFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0)";

    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::KinematicsPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::KinematicsPluginInfo>());  // NOLINT
  }

  {  // missing fwd plugins failure
    std::string yaml_string = R"(kinematic_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_kinematics_kdl_factories
                                   fwd_kin_plugins:
                                     iiwa_manipulator:
                                       default: KDLFwdKinChain
                                   inv_kin_plugins:
                                     iiwa_manipulator:
                                       default: KDLInvKinChainLMA
                                       plugins:
                                         KDLInvKinChainLMA:
                                           class: KDLInvKinChainLMAFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0
                                         KDLInvKinChainNR:
                                           class: KDLInvKinChainNRFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0)";

    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::KinematicsPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::KinematicsPluginInfo>());  // NOLINT
  }

  {  // fwd plugins is not map failure

    std::string yaml_string = R"(kinematic_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_kinematics_kdl_factories
                                   fwd_kin_plugins:
                                     iiwa_manipulator:
                                       - tesseract_collision_bullet_factories
                                       - tesseract_collision_fcl_factories
                                   inv_kin_plugins:
                                     iiwa_manipulator:
                                       default: KDLInvKinChainLMA
                                       plugins:
                                         KDLInvKinChainLMA:
                                           class: KDLInvKinChainLMAFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0
                                         KDLInvKinChainNR:
                                           class: KDLInvKinChainNRFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0)";

    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::KinematicsPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::KinematicsPluginInfo>());  // NOLINT
  }

  {  // missing inv plugins failure
    std::string yaml_string = R"(kinematic_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_kinematics_kdl_factories
                                   fwd_kin_plugins:
                                     iiwa_manipulator:
                                       default: KDLFwdKinChain
                                       plugins:
                                         KDLFwdKinChain:
                                           class: KDLFwdKinChainFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0
                                   inv_kin_plugins:
                                     iiwa_manipulator:
                                       default: KDLInvKinChainLMA)";

    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::KinematicsPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::KinematicsPluginInfo>());  // NOLINT
  }

  {  // inv plugins is not map failure
    std::string yaml_string = R"(kinematic_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_kinematics_kdl_factories
                                   fwd_kin_plugins:
                                     iiwa_manipulator:
                                       default: KDLFwdKinChain
                                       plugins:
                                         KDLFwdKinChain:
                                           class: KDLFwdKinChainFactory
                                           config:
                                             base_link: base_link
                                             tip_link: tool0
                                   inv_kin_plugins:
                                     iiwa_manipulator:
                                       - tesseract_collision_bullet_factories
                                       - tesseract_collision_fcl_factories)";

    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::KinematicsPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::KinematicsPluginInfo>());  // NOLINT
  }
}

TEST(TesseractPluginFactoryUnit, ContactManagersPluginInfoYamlUnit)  // NOLINT
{
  std::string yaml_string = R"(contact_manager_plugins:
                                 search_paths:
                                   - /usr/local/lib
                                 search_libraries:
                                   - tesseract_collision_bullet_factories
                                   - tesseract_collision_fcl_factories
                                 discrete_plugins:
                                   default: BulletDiscreteBVHManager
                                   plugins:
                                     BulletDiscreteBVHManager:
                                       class: BulletDiscreteBVHManagerFactory
                                     BulletDiscreteSimpleManager:
                                       class: BulletDiscreteSimpleManagerFactory
                                     FCLDiscreteBVHManager:
                                       class: FCLDiscreteBVHManagerFactory
                                 continuous_plugins:
                                   default: BulletCastBVHManager
                                   plugins:
                                     BulletCastBVHManager:
                                       class: BulletCastBVHManagerFactory
                                     BulletCastSimpleManager:
                                       class: BulletCastSimpleManagerFactory)";

  {  // Success
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ContactManagersPluginInfo::CONFIG_KEY];
    auto cmpi = config.as<tesseract_common::ContactManagersPluginInfo>();

    const YAML::Node& plugin_info = plugin_config["contact_manager_plugins"];
    const YAML::Node& search_paths = plugin_info["search_paths"];
    const YAML::Node& search_libraries = plugin_info["search_libraries"];
    const YAML::Node& discrete_default_plugin = plugin_info["discrete_plugins"]["default"];
    const YAML::Node& discrete_plugins = plugin_info["discrete_plugins"]["plugins"];
    const YAML::Node& continuous_default_plugin = plugin_info["continuous_plugins"]["default"];
    const YAML::Node& continuous_plugins = plugin_info["continuous_plugins"]["plugins"];

    {
      std::set<std::string> sp = cmpi.search_paths;
      EXPECT_EQ(sp.size(), 1);

      for (auto it = search_paths.begin(); it != search_paths.end(); ++it)
      {
        EXPECT_TRUE(sp.find(it->as<std::string>()) != sp.end());
      }
    }

    {
      std::set<std::string> sl = cmpi.search_libraries;
      EXPECT_EQ(sl.size(), 2);

      for (auto it = search_libraries.begin(); it != search_libraries.end(); ++it)
      {
        EXPECT_TRUE(sl.find(it->as<std::string>()) != sl.end());
      }
    }

    EXPECT_EQ(discrete_default_plugin.as<std::string>(), cmpi.discrete_plugin_infos.default_plugin);
    EXPECT_EQ(discrete_plugins.size(), cmpi.discrete_plugin_infos.plugins.size());

    EXPECT_EQ(continuous_default_plugin.as<std::string>(), cmpi.continuous_plugin_infos.default_plugin);
    EXPECT_EQ(continuous_plugins.size(), cmpi.continuous_plugin_infos.plugins.size());
  }

  {  // search_paths failure
    std::string yaml_string = R"(contact_manager_plugins:
                                   search_paths:
                                     failure: issue
                                   search_libraries:
                                     - tesseract_collision_bullet_factories
                                     - tesseract_collision_fcl_factories
                                   discrete_plugins:
                                     default: BulletDiscreteBVHManager
                                     plugins:
                                       BulletDiscreteBVHManager:
                                         class: BulletDiscreteBVHManagerFactory
                                       BulletDiscreteSimpleManager:
                                         class: BulletDiscreteSimpleManagerFactory
                                       FCLDiscreteBVHManager:
                                         class: FCLDiscreteBVHManagerFactory
                                   continuous_plugins:
                                     default: BulletCastBVHManager
                                     plugins:
                                       BulletCastBVHManager:
                                         class: BulletCastBVHManagerFactory
                                       BulletCastSimpleManager:
                                         class: BulletCastSimpleManagerFactory)";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ContactManagersPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::ContactManagersPluginInfo>());  // NOLINT
  }

  {  // search_libraries failure
    std::string yaml_string = R"(contact_manager_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     failure: issue
                                   discrete_plugins:
                                     default: BulletDiscreteBVHManager
                                     plugins:
                                       BulletDiscreteBVHManager:
                                         class: BulletDiscreteBVHManagerFactory
                                       BulletDiscreteSimpleManager:
                                         class: BulletDiscreteSimpleManagerFactory
                                       FCLDiscreteBVHManager:
                                         class: FCLDiscreteBVHManagerFactory
                                   continuous_plugins:
                                     default: BulletCastBVHManager
                                     plugins:
                                       BulletCastBVHManager:
                                         class: BulletCastBVHManagerFactory
                                       BulletCastSimpleManager:
                                         class: BulletCastSimpleManagerFactory)";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ContactManagersPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::ContactManagersPluginInfo>());  // NOLINT
  }

  {  // missing discrete plugins failure
    std::string yaml_string = R"(contact_manager_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_collision_bullet_factories
                                     - tesseract_collision_fcl_factories
                                   discrete_plugins:
                                     default: BulletDiscreteBVHManager
                                   continuous_plugins:
                                     default: BulletCastBVHManager
                                     plugins:
                                       BulletCastBVHManager:
                                         class: BulletCastBVHManagerFactory
                                       BulletCastSimpleManager:
                                         class: BulletCastSimpleManagerFactory)";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ContactManagersPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::ContactManagersPluginInfo>());  // NOLINT
  }

  {  // discrete plugins is not map failure
    std::string yaml_string = R"(contact_manager_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_collision_bullet_factories
                                     - tesseract_collision_fcl_factories
                                   discrete_plugins:
                                     - tesseract_collision_bullet_factories
                                     - tesseract_collision_fcl_factories
                                   continuous_plugins:
                                     default: BulletCastBVHManager
                                     plugins:
                                       BulletCastBVHManager:
                                         class: BulletCastBVHManagerFactory
                                       BulletCastSimpleManager:
                                         class: BulletCastSimpleManagerFactory)";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ContactManagersPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::ContactManagersPluginInfo>());  // NOLINT
  }

  {  // missing continuous plugins failure
    std::string yaml_string = R"(contact_manager_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_collision_bullet_factories
                                     - tesseract_collision_fcl_factories
                                   discrete_plugins:
                                     default: BulletDiscreteBVHManager
                                     plugins:
                                       BulletDiscreteBVHManager:
                                         class: BulletDiscreteBVHManagerFactory
                                       BulletDiscreteSimpleManager:
                                         class: BulletDiscreteSimpleManagerFactory
                                       FCLDiscreteBVHManager:
                                         class: FCLDiscreteBVHManagerFactory
                                   continuous_plugins:
                                     default: BulletCastBVHManager)";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ContactManagersPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::ContactManagersPluginInfo>());  // NOLINT
  }

  {  // continuous plugins is not map failure
    std::string yaml_string = R"(contact_manager_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_collision_bullet_factories
                                     - tesseract_collision_fcl_factories
                                   discrete_plugins:
                                     default: BulletDiscreteBVHManager
                                     plugins:
                                       BulletDiscreteBVHManager:
                                         class: BulletDiscreteBVHManagerFactory
                                       BulletDiscreteSimpleManager:
                                         class: BulletDiscreteSimpleManagerFactory
                                       FCLDiscreteBVHManager:
                                         class: FCLDiscreteBVHManagerFactory
                                   continuous_plugins:
                                     - tesseract_collision_bullet_factories
                                     - tesseract_collision_fcl_factories)";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ContactManagersPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::ContactManagersPluginInfo>());  // NOLINT
  }
}

TEST(TesseractPluginFactoryUnit, ProfilePluginInfoYamlUnit)  // NOLINT
{
  std::string yaml_string = R"(profile_plugins:
                                 search_paths:
                                   - /usr/local/lib
                                 search_libraries:
                                   - profile_factories
                                 profiles:
                                   TrajOptCollisionCost:
                                     DiscreteCollisionProfile:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01
                                     DiscreteCollisionProfile2:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01
                                   TrajOptCollisionConstraint:
                                     DiscreteCollisionProfile:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01
                                     DiscreteCollisionProfile2:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01)";

  {  // Success
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ProfilesPluginInfo::CONFIG_KEY];
    auto cmpi = config.as<tesseract_common::ProfilesPluginInfo>();

    const YAML::Node& plugin_info = plugin_config["profile_plugins"];
    const YAML::Node& search_paths = plugin_info["search_paths"];
    const YAML::Node& search_libraries = plugin_info["search_libraries"];
    const YAML::Node& cost_plugins = plugin_info["profiles"]["TrajOptCollisionCost"];
    const YAML::Node& constraint_plugins = plugin_info["profiles"]["TrajOptCollisionConstraint"];

    {
      std::set<std::string> sp = cmpi.search_paths;
      EXPECT_EQ(sp.size(), 1);

      for (auto it = search_paths.begin(); it != search_paths.end(); ++it)
      {
        EXPECT_TRUE(sp.find(it->as<std::string>()) != sp.end());
      }
    }

    {
      std::set<std::string> sl = cmpi.search_libraries;
      EXPECT_EQ(sl.size(), 1);

      for (auto it = search_libraries.begin(); it != search_libraries.end(); ++it)
      {
        EXPECT_TRUE(sl.find(it->as<std::string>()) != sl.end());
      }
    }

    EXPECT_EQ(cost_plugins.size(), cmpi.plugin_infos["TrajOptCollisionCost"].size());
    EXPECT_EQ(constraint_plugins.size(), cmpi.plugin_infos["TrajOptCollisionConstraint"].size());
  }

  {  // search_paths failure
    std::string yaml_string = R"(profile_plugins:
                                 search_paths:
                                   failure: issue
                                 search_libraries:
                                   - profile_factories
                                 profiles:
                                   TrajOptCollisionCost:
                                     DiscreteCollisionProfile:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01
                                     DiscreteCollisionProfile2:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01
                                   TrajOptCollisionConstraint:
                                     DiscreteCollisionProfile:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01
                                     DiscreteCollisionProfile2:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01)";

    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ProfilesPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::ProfilesPluginInfo>());  // NOLINT
  }

  {  // search_libraries failure
    std::string yaml_string = R"(profile_plugins:
                                 search_paths:
                                   - /usr/local/lib
                                 search_libraries:
                                   failure: issue
                                 profiles:
                                   TrajOptCollisionCost:
                                     DiscreteCollisionProfile:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01
                                     DiscreteCollisionProfile2:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01
                                   TrajOptCollisionConstraint:
                                     DiscreteCollisionProfile:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01
                                     DiscreteCollisionProfile2:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01)";

    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ProfilesPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::ProfilesPluginInfo>());  // NOLINT
  }

  {  // missing cost plugins failure
    std::string yaml_string = R"(profile_plugins:
                                 search_paths:
                                   - /usr/local/lib
                                 search_libraries:
                                   - profile_factories
                                 profiles:
                                   TrajOptCollisionCost:
                                   TrajOptCollisionConstraint:
                                     DiscreteCollisionProfile:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01
                                     DiscreteCollisionProfile2:
                                       class: DiscreteCollisionProfileFactory
                                       config:
                                         threshold: 0.01)";

    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ProfilesPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::ProfilesPluginInfo>());  // NOLINT
  }

  {  // profiles is not map failure
    std::string yaml_string = R"(profile_plugins:
                                 search_paths:
                                   - /usr/local/lib
                                 search_libraries:
                                   - profile_factories
                                 profiles:
                                   - TrajOptCollisionCost
                                   - TrajOptCollisionConstraint)";

    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::ProfilesPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::ProfilesPluginInfo>());  // NOLINT
  }
}

TEST(TesseractPluginFactoryUnit, TaskComposerPluginInfoYamlUnit)  // NOLINT
{
  std::string yaml_string = R"(task_composer_plugins:
                                 search_paths:
                                   - /usr/local/lib
                                 search_libraries:
                                   - tesseract_task_composer_executor_factories
                                   - tesseract_task_composer_node_factories
                                 executors:
                                   default: TaskflowTaskComposerExecutor
                                   plugins:
                                     TaskflowTaskComposerExecutor:
                                       class: TaskflowTaskComposerExecutorFactory
                                       config:
                                         threads: 5
                                     TaskflowTaskComposerExecutor2:
                                       class: TaskflowTaskComposerExecutorFactory
                                       config:
                                         threads: 10
                                     TaskflowTaskComposerExecutor3:
                                       class: TaskflowTaskComposerExecutorFactory
                                       config:
                                         threads: 15
                                 tasks:
                                   default: CartesianMotionPipeline
                                   plugins:
                                     CartesianMotionPipeline:
                                       class: CartesianMotionPipelineFactory
                                       config:
                                         input_key: "input"
                                         output_key: "output"
                                     FreespaceMotionPipeline:
                                       class: FreespaceMotionPipelineFactory
                                       config:
                                         input_key: "input"
                                         output_key: "output")";

  {  // Success
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::TaskComposerPluginInfo::CONFIG_KEY];
    auto tcpi = config.as<tesseract_common::TaskComposerPluginInfo>();

    const YAML::Node& plugin_info = plugin_config["task_composer_plugins"];
    const YAML::Node& search_paths = plugin_info["search_paths"];
    const YAML::Node& search_libraries = plugin_info["search_libraries"];
    const YAML::Node& executor_default_plugin = plugin_info["executors"]["default"];
    const YAML::Node& executor_plugins = plugin_info["executors"]["plugins"];
    const YAML::Node& task_default_plugin = plugin_info["tasks"]["default"];
    const YAML::Node& task_plugins = plugin_info["tasks"]["plugins"];

    {
      std::set<std::string> sp = tcpi.search_paths;
      EXPECT_EQ(sp.size(), 1);

      for (auto it = search_paths.begin(); it != search_paths.end(); ++it)
      {
        EXPECT_TRUE(sp.find(it->as<std::string>()) != sp.end());
      }
    }

    {
      std::set<std::string> sl = tcpi.search_libraries;
      EXPECT_EQ(sl.size(), 2);

      for (auto it = search_libraries.begin(); it != search_libraries.end(); ++it)
      {
        EXPECT_TRUE(sl.find(it->as<std::string>()) != sl.end());
      }
    }

    EXPECT_EQ(executor_default_plugin.as<std::string>(), tcpi.executor_plugin_infos.default_plugin);
    EXPECT_EQ(executor_plugins.size(), tcpi.executor_plugin_infos.plugins.size());

    EXPECT_EQ(task_default_plugin.as<std::string>(), tcpi.task_plugin_infos.default_plugin);
    EXPECT_EQ(task_plugins.size(), tcpi.task_plugin_infos.plugins.size());
  }

  {  // search_paths failure
    std::string yaml_string = R"(task_composer_plugins:
                                   search_paths:
                                     failure: issue
                                   search_libraries:
                                     - tesseract_task_composer_executor_factories
                                     - tesseract_task_composer_node_factories
                                   executors:
                                     default: TaskflowTaskComposerExecutor
                                     plugins:
                                       TaskflowTaskComposerExecutor:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 5
                                       TaskflowTaskComposerExecutor2:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 10
                                       TaskflowTaskComposerExecutor3:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 15
                                   tasks:
                                     default: CartesianMotionPipeline
                                     plugins:
                                       CartesianMotionPipeline:
                                         class: CartesianMotionPipelineFactory
                                         config:
                                           input_key: "input"
                                           output_key: "output"
                                       FreespaceMotionPipeline:
                                         class: FreespaceMotionPipelineFactory
                                         config:
                                           input_key: "input"
                                           output_key: "output")";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::TaskComposerPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::TaskComposerPluginInfo>());  // NOLINT
  }

  {  // search_libraries failure
    std::string yaml_string = R"(task_composer_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     failure: issue
                                   executors:
                                     default: TaskflowTaskComposerExecutor
                                     plugins:
                                       TaskflowTaskComposerExecutor:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 5
                                       TaskflowTaskComposerExecutor2:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 10
                                       TaskflowTaskComposerExecutor3:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 15
                                   tasks:
                                     default: CartesianMotionPipeline
                                     plugins:
                                       CartesianMotionPipeline:
                                         class: CartesianMotionPipelineFactory
                                         config:
                                           input_key: "input"
                                           output_key: "output"
                                       FreespaceMotionPipeline:
                                         class: FreespaceMotionPipelineFactory
                                         config:
                                           input_key: "input"
                                           output_key: "output")";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::TaskComposerPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::TaskComposerPluginInfo>());  // NOLINT
  }

  {  // missing executor plugins failure
    std::string yaml_string = R"(task_composer_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_task_composer_executor_factories
                                     - tesseract_task_composer_node_factories
                                   executors:
                                     default: TaskflowTaskComposerExecutor
                                   tasks:
                                     default: CartesianMotionPipeline
                                     plugins:
                                       CartesianMotionPipeline:
                                         class: CartesianMotionPipelineFactory
                                         config:
                                           input_key: "input"
                                           output_key: "output"
                                       FreespaceMotionPipeline:
                                         class: FreespaceMotionPipelineFactory
                                         config:
                                           input_key: "input"
                                           output_key: "output")";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::TaskComposerPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::TaskComposerPluginInfo>());  // NOLINT
  }

  {  // executor plugins is not map failure
    std::string yaml_string = R"(task_composer_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_task_composer_executor_factories
                                     - tesseract_task_composer_node_factories
                                   executors:
                                     - TaskflowTaskComposerExecutor:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 5
                                     - TaskflowTaskComposerExecutor2:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 10
                                     - TaskflowTaskComposerExecutor3:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 15
                                   tasks:
                                     default: CartesianMotionPipeline
                                     plugins:
                                       CartesianMotionPipeline:
                                         class: CartesianMotionPipelineFactory
                                         config:
                                           input_key: "input"
                                           output_key: "output"
                                       FreespaceMotionPipeline:
                                         class: FreespaceMotionPipelineFactory
                                         config:
                                           input_key: "input"
                                           output_key: "output")";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::TaskComposerPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::TaskComposerPluginInfo>());  // NOLINT
  }

  {  // missing node plugins failure
    std::string yaml_string = R"(task_composer_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_task_composer_executor_factories
                                     - tesseract_task_composer_node_factories
                                   executors:
                                     default: TaskflowTaskComposerExecutor
                                     plugins:
                                       TaskflowTaskComposerExecutor:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 5
                                       TaskflowTaskComposerExecutor2:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 10
                                       TaskflowTaskComposerExecutor3:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 15
                                   tasks:
                                     default: CartesianMotionPipeline)";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::TaskComposerPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::TaskComposerPluginInfo>());  // NOLINT
  }

  {  // continuous plugins is not map failure
    std::string yaml_string = R"(task_composer_plugins:
                                   search_paths:
                                     - /usr/local/lib
                                   search_libraries:
                                     - tesseract_task_composer_executor_factories
                                     - tesseract_task_composer_node_factories
                                   executors:
                                     default: TaskflowTaskComposerExecutor
                                     plugins:
                                       TaskflowTaskComposerExecutor:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 5
                                       TaskflowTaskComposerExecutor2:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 10
                                       TaskflowTaskComposerExecutor3:
                                         class: TaskflowTaskComposerExecutorFactory
                                         config:
                                           threads: 15
                                   tasks:
                                     - CartesianMotionPipeline:
                                         class: CartesianMotionPipelineFactory
                                         config:
                                           input_key: "input"
                                           output_key: "output"
                                     - FreespaceMotionPipeline:
                                         class: FreespaceMotionPipelineFactory
                                         config:
                                           input_key: "input"
                                           output_key: "output")";
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node plugin_config = tesseract_common::loadYamlString(yaml_string, locator);
    YAML::Node config = plugin_config[tesseract_common::TaskComposerPluginInfo::CONFIG_KEY];
    EXPECT_ANY_THROW(config.as<tesseract_common::TaskComposerPluginInfo>());  // NOLINT
  }
}

TEST(TesseractCommonUnit, TransformMapYamlUnit)  // NOLINT
{
  std::string yaml_string =
      R"(joints:
           joint_1:
             position:
               x: 1
               y: 2
               z: 3
             orientation:
               x: 0
               y: 0
               z: 0
               w: 1
           joint_2:
             position:
               x: 4
               y: 5
               z: 6
             orientation:
               x: 0
               y: 0
               z: 0
               w: 1)";

  {  // valid string
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node node = tesseract_common::loadYamlString(yaml_string, locator);
    auto trans_map = node["joints"].as<tesseract_common::TransformMap>();
    EXPECT_EQ(trans_map.size(), 2);
    EXPECT_FALSE(trans_map.empty());
    EXPECT_TRUE(trans_map.find("joint_1") != trans_map.end());
    EXPECT_TRUE(trans_map.find("joint_2") != trans_map.end());
  }

  std::string bad_yaml_string =
      R"(joints:
           - joint_1:
               position:
                 x: 1
                 y: 2
                 z: 3
               orientation:
                 x: 0
                 y: 0
                 z: 0
                 w: 1
           - joint_2:
               position:
                 x: 4
                 y: 5
                 z: 6
               orientation:
                 x: 0
                 y: 0
                 z: 0
                 w: 1)";
  {  // invalid string
    tesseract_common::GeneralResourceLocator locator;
    YAML::Node node = tesseract_common::loadYamlString(bad_yaml_string, locator);
    EXPECT_ANY_THROW(node["joints"].as<tesseract_common::TransformMap>());  // NOLINT
  }
}

TEST(TesseractCommonUnit, CalibrationInfoYamlUnit)  // NOLINT
{
  std::string yaml_string =
      R"(calibration:
           joints:
             joint_1:
               position:
                 x: 1
                 y: 2
                 z: 3
               orientation:
                 x: 0
                 y: 0
                 z: 0
                 w: 1
             joint_2:
               position:
                 x: 4
                 y: 5
                 z: 6
               orientation:
                 x: 0
                 y: 0
                 z: 0
                 w: 1)";

  tesseract_common::GeneralResourceLocator locator;
  YAML::Node node = tesseract_common::loadYamlString(yaml_string, locator);
  auto cal_info = node[tesseract_common::CalibrationInfo::CONFIG_KEY].as<tesseract_common::CalibrationInfo>();
  EXPECT_FALSE(cal_info.empty());
  EXPECT_TRUE(cal_info.joints.find("joint_1") != cal_info.joints.end());
  EXPECT_TRUE(cal_info.joints.find("joint_2") != cal_info.joints.end());

  tesseract_common::CalibrationInfo cal_insert;
  EXPECT_TRUE(cal_insert.empty());
  cal_insert.insert(cal_info);
  EXPECT_FALSE(cal_insert.empty());
  EXPECT_TRUE(cal_insert.joints.find("joint_1") != cal_insert.joints.end());
  EXPECT_TRUE(cal_insert.joints.find("joint_2") != cal_insert.joints.end());

  cal_info.clear();
  EXPECT_TRUE(cal_info.empty());
}

TEST(TesseractCommonUnit, linkNamesPairUnit)  // NOLINT
{
  {
    tesseract_common::LinkNamesPair p1 = tesseract_common::makeOrderedLinkPair("link_1", "link_2");
    tesseract_common::LinkNamesPair p2 = tesseract_common::makeOrderedLinkPair("link_2", "link_1");

    EXPECT_EQ(p1.first, p2.first);
    EXPECT_EQ(p1.second, p2.second);

    tesseract_common::PairHash hash;
    EXPECT_EQ(hash(p1), hash(p2));
  }

  {
    tesseract_common::LinkNamesPair p1;
    tesseract_common::makeOrderedLinkPair(p1, "link_1", "link_2");
    tesseract_common::LinkNamesPair p2;
    tesseract_common::makeOrderedLinkPair(p2, "link_2", "link_1");

    EXPECT_EQ(p1.first, p2.first);
    EXPECT_EQ(p1.second, p2.second);

    tesseract_common::PairHash hash;
    EXPECT_EQ(hash(p1), hash(p2));
  }

  {
    tesseract_common::LinkNamesPair p1 = tesseract_common::makeOrderedLinkPair("link_1", "link_2");
    tesseract_common::LinkNamesPair p2 = tesseract_common::makeOrderedLinkPair("link_2", "link_1");

    tesseract_common::LinkNamesPair mp1;
    tesseract_common::makeOrderedLinkPair(mp1, "link_1", "link_2");
    tesseract_common::LinkNamesPair mp2;
    tesseract_common::makeOrderedLinkPair(mp2, "link_2", "link_1");

    EXPECT_EQ(p1.first, mp1.first);
    EXPECT_EQ(p1.second, mp1.second);
    EXPECT_EQ(p2.first, mp2.first);
    EXPECT_EQ(p2.second, mp2.second);
  }
}

/** @brief Tests calcRotationalError which return angle between [-PI, PI]*/
TEST(TesseractCommonUnit, calcRotationalError)  // NOLINT
{
  Eigen::Isometry3d identity = Eigen::Isometry3d::Identity();
  Eigen::Isometry3d pi_rot = identity * Eigen::AngleAxisd(M_PI - 0.0001, Eigen::Vector3d::UnitZ());
  Eigen::Vector3d rot_err = tesseract_common::calcRotationalError(pi_rot.rotation());
  EXPECT_NEAR(rot_err.norm(), M_PI - 0.0001, 1e-6);
  EXPECT_TRUE(rot_err.normalized().isApprox(Eigen::Vector3d::UnitZ(), 1e-6));

  pi_rot = identity * Eigen::AngleAxisd(-M_PI + 0.0001, Eigen::Vector3d::UnitZ());
  rot_err = tesseract_common::calcRotationalError(pi_rot.rotation());
  EXPECT_NEAR(rot_err.norm(), M_PI - 0.0001, 1e-6);
  EXPECT_TRUE(rot_err.normalized().isApprox(-Eigen::Vector3d::UnitZ(), 1e-6));

  // Test greater than PI
  pi_rot = identity * Eigen::AngleAxisd(3 * M_PI_2, Eigen::Vector3d::UnitZ());
  rot_err = tesseract_common::calcRotationalError(pi_rot.rotation());
  EXPECT_NEAR(rot_err.norm(), M_PI_2, 1e-6);
  EXPECT_TRUE(rot_err.normalized().isApprox(-Eigen::Vector3d::UnitZ(), 1e-6));

  // Test less than than -PI
  pi_rot = identity * Eigen::AngleAxisd(-3 * M_PI_2, Eigen::Vector3d::UnitZ());
  rot_err = tesseract_common::calcRotationalError(pi_rot.rotation());
  EXPECT_NEAR(rot_err.norm(), M_PI_2, 1e-6);
  EXPECT_TRUE(rot_err.normalized().isApprox(Eigen::Vector3d::UnitZ(), 1e-6));

  // Test for angle between [0, PI]
  Eigen::Isometry3d pi_rot_plus = identity * Eigen::AngleAxisd(M_PI_2 + 0.001, Eigen::Vector3d::UnitZ());
  Eigen::Isometry3d pi_rot_minus = identity * Eigen::AngleAxisd(M_PI_2 - 0.001, Eigen::Vector3d::UnitZ());
  Eigen::Vector3d pi_rot_delta = tesseract_common::calcRotationalError(pi_rot_plus.rotation()) -
                                 tesseract_common::calcRotationalError(pi_rot_minus.rotation());
  EXPECT_NEAR(pi_rot_delta.norm(), 0.002, 1e-6);

  // Test for angle between [-PI, 0]
  pi_rot_plus = identity * Eigen::AngleAxisd(-M_PI_2 + 0.001, Eigen::Vector3d::UnitZ());
  pi_rot_minus = identity * Eigen::AngleAxisd(-M_PI_2 - 0.001, Eigen::Vector3d::UnitZ());
  pi_rot_delta = tesseract_common::calcRotationalError(pi_rot_plus.rotation()) -
                 tesseract_common::calcRotationalError(pi_rot_minus.rotation());
  EXPECT_NEAR(pi_rot_delta.norm(), 0.002, 1e-6);

  // Test for angle at 0
  pi_rot_plus = identity * Eigen::AngleAxisd(0.001, Eigen::Vector3d::UnitZ());
  pi_rot_minus = identity * Eigen::AngleAxisd(-0.001, Eigen::Vector3d::UnitZ());
  pi_rot_delta = tesseract_common::calcRotationalError(pi_rot_plus.rotation()) -
                 tesseract_common::calcRotationalError(pi_rot_minus.rotation());
  EXPECT_NEAR(pi_rot_delta.norm(), 0.002, 1e-6);

  // Test for angle at PI
  pi_rot_plus = identity * Eigen::AngleAxisd(M_PI + 0.001, Eigen::Vector3d::UnitZ());
  pi_rot_minus = identity * Eigen::AngleAxisd(M_PI - 0.001, Eigen::Vector3d::UnitZ());
  pi_rot_delta = tesseract_common::calcRotationalError(pi_rot_plus.rotation()) -
                 tesseract_common::calcRotationalError(pi_rot_minus.rotation());
  EXPECT_TRUE(pi_rot_delta.norm() > M_PI);  // This is because calcRotationalError breaks down at PI or -PI

  // Test for angle at -PI
  pi_rot_plus = identity * Eigen::AngleAxisd(-M_PI + 0.001, Eigen::Vector3d::UnitZ());
  pi_rot_minus = identity * Eigen::AngleAxisd(-M_PI - 0.001, Eigen::Vector3d::UnitZ());
  pi_rot_delta = tesseract_common::calcRotationalError(pi_rot_plus.rotation()) -
                 tesseract_common::calcRotationalError(pi_rot_minus.rotation());
  EXPECT_TRUE(pi_rot_delta.norm() > M_PI);  // This is because calcRotationalError breaks down at PI or -PI

  // Test random axis
  for (int i = 0; i < 100; i++)
  {
    Eigen::Vector3d axis = Eigen::Vector3d::Random().normalized();

    // Avoid M_PI angle because this breaks down
    Eigen::VectorXd angles = Eigen::VectorXd::LinSpaced(100, -M_PI + 0.005, M_PI - 0.005);
    for (Eigen::Index j = 0; j < angles.rows(); j++)
    {
      pi_rot_plus = identity * Eigen::AngleAxisd(angles(j) + 0.001, axis);
      pi_rot_minus = identity * Eigen::AngleAxisd(angles(j) - 0.001, axis);
      Eigen::Vector3d e1 = tesseract_common::calcRotationalError(pi_rot_plus.rotation());
      Eigen::Vector3d e2 = tesseract_common::calcRotationalError(pi_rot_minus.rotation());
      EXPECT_FALSE((e1.norm() < -M_PI));
      EXPECT_FALSE((e1.norm() > M_PI));
      EXPECT_FALSE((e2.norm() < -M_PI));
      EXPECT_FALSE((e2.norm() > M_PI));
      pi_rot_delta = e1 - e2;
      EXPECT_NEAR(pi_rot_delta.norm(), 0.002, 1e-6);
    }

    // Avoid M_PI angle because this breaks down
    angles = Eigen::VectorXd::LinSpaced(100, M_PI + 0.005, 2 * M_PI);
    for (Eigen::Index j = 0; j < angles.rows(); j++)
    {
      pi_rot_plus = identity * Eigen::AngleAxisd(angles(j) + 0.001, axis);
      pi_rot_minus = identity * Eigen::AngleAxisd(angles(j) - 0.001, axis);
      Eigen::Vector3d e1 = tesseract_common::calcRotationalError(pi_rot_plus.rotation());
      Eigen::Vector3d e2 = tesseract_common::calcRotationalError(pi_rot_minus.rotation());
      EXPECT_FALSE((e1.norm() < -M_PI));
      EXPECT_FALSE((e1.norm() > M_PI));
      EXPECT_FALSE((e2.norm() < -M_PI));
      EXPECT_FALSE((e2.norm() > M_PI));
      pi_rot_delta = e1 - e2;
      EXPECT_NEAR(pi_rot_delta.norm(), 0.002, 1e-6);
    }

    // Avoid M_PI angle because this breaks down
    angles = Eigen::VectorXd::LinSpaced(100, -M_PI - 0.005, -2 * M_PI);
    for (Eigen::Index j = 0; j < angles.rows(); j++)
    {
      pi_rot_plus = identity * Eigen::AngleAxisd(angles(j) + 0.001, axis);
      pi_rot_minus = identity * Eigen::AngleAxisd(angles(j) - 0.001, axis);
      Eigen::Vector3d e1 = tesseract_common::calcRotationalError(pi_rot_plus.rotation());
      Eigen::Vector3d e2 = tesseract_common::calcRotationalError(pi_rot_minus.rotation());
      EXPECT_FALSE((e1.norm() < -M_PI));
      EXPECT_FALSE((e1.norm() > M_PI));
      EXPECT_FALSE((e2.norm() < -M_PI));
      EXPECT_FALSE((e2.norm() > M_PI));
      pi_rot_delta = e1 - e2;
      EXPECT_NEAR(pi_rot_delta.norm(), 0.002, 1e-6);
    }

    // These should fail
    {
      pi_rot_plus = identity * Eigen::AngleAxisd(M_PI + 0.001, axis);
      pi_rot_minus = identity * Eigen::AngleAxisd(M_PI - 0.001, axis);
      Eigen::Vector3d e1 = tesseract_common::calcRotationalError(pi_rot_plus.rotation());
      Eigen::Vector3d e2 = tesseract_common::calcRotationalError(pi_rot_minus.rotation());
      EXPECT_FALSE((e1.norm() < -M_PI));
      EXPECT_FALSE((e1.norm() > M_PI));
      EXPECT_FALSE((e2.norm() < -M_PI));
      EXPECT_FALSE((e2.norm() > M_PI));
      pi_rot_delta = e1 - e2;
      EXPECT_TRUE(pi_rot_delta.norm() > M_PI);  // This is because calcRotationalError breaks down at PI or -PI
    }
    {
      pi_rot_plus = identity * Eigen::AngleAxisd(-M_PI + 0.001, axis);
      pi_rot_minus = identity * Eigen::AngleAxisd(-M_PI - 0.001, axis);
      Eigen::Vector3d e1 = tesseract_common::calcRotationalError(pi_rot_plus.rotation());
      Eigen::Vector3d e2 = tesseract_common::calcRotationalError(pi_rot_minus.rotation());
      EXPECT_FALSE((e1.norm() < -M_PI));
      EXPECT_FALSE((e1.norm() > M_PI));
      EXPECT_FALSE((e2.norm() < -M_PI));
      EXPECT_FALSE((e2.norm() > M_PI));
      pi_rot_delta = e1 - e2;
      EXPECT_TRUE(pi_rot_delta.norm() > M_PI);  // This is because calcRotationalError breaks down at PI or -PI
    }
  }
}

/** @brief Tests calcTransformError */
TEST(TesseractCommonUnit, calcTransformError)  // NOLINT
{
  Eigen::Isometry3d identity = Eigen::Isometry3d::Identity();

  {  // X-Axis
    Eigen::Isometry3d pi_rot = identity * Eigen::AngleAxisd(M_PI_2, Eigen::Vector3d::UnitX());
    Eigen::VectorXd err = tesseract_common::calcTransformError(identity, pi_rot);
    EXPECT_TRUE(err.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(err.tail(3).isApprox(Eigen::Vector3d(M_PI_2, 0, 0)));
  }

  {  // Y-Axis
    Eigen::Isometry3d pi_rot = identity * Eigen::AngleAxisd(M_PI_2, Eigen::Vector3d::UnitY());
    Eigen::VectorXd err = tesseract_common::calcTransformError(identity, pi_rot);
    EXPECT_TRUE(err.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(err.tail(3).isApprox(Eigen::Vector3d(0, M_PI_2, 0)));
  }

  {  // Z-Axis
    Eigen::Isometry3d pi_rot = identity * Eigen::AngleAxisd(M_PI_2, Eigen::Vector3d::UnitZ());
    Eigen::VectorXd err = tesseract_common::calcTransformError(identity, pi_rot);
    EXPECT_TRUE(err.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(err.tail(3).isApprox(Eigen::Vector3d(0, 0, M_PI_2)));
  }

  {  // Translation
    Eigen::Isometry3d pi_rot = identity * Eigen::Translation3d(1, 2, 3);
    Eigen::VectorXd err = tesseract_common::calcTransformError(identity, pi_rot);
    EXPECT_TRUE(err.head(3).isApprox(Eigen::Vector3d(1, 2, 3)));
    EXPECT_TRUE(err.tail(3).isApprox(Eigen::Vector3d::Zero()));
  }
}

void runCalcJacobianTransformErrorDiffTest(double anlge)
{
  Eigen::Isometry3d identity = Eigen::Isometry3d::Identity();
  const double eps = 0.001;
  {  // X-Axis positive
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge, Eigen::Vector3d::UnitX());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge + eps, Eigen::Vector3d::UnitX());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(eps, 0, 0)));
  }

  {  // X-Axis negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge, Eigen::Vector3d::UnitX());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge - eps, Eigen::Vector3d::UnitX());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(-eps, 0, 0)));
  }

  {  // X-Axis positive and negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge + eps, Eigen::Vector3d::UnitX());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge - eps, Eigen::Vector3d::UnitX());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(-2 * eps, 0, 0)));
  }

  {  // X-Axis translation
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge, Eigen::Vector3d::UnitX());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge - eps, Eigen::Vector3d::UnitX());
    source_tf_perturbed.translation() += Eigen::Vector3d(eps, -eps, eps);
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d(eps, -eps, eps)));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(-eps, 0, 0)));
  }

  {  // Y-Axis positive
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge, Eigen::Vector3d::UnitY());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge + eps, Eigen::Vector3d::UnitY());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, eps, 0)));
  }

  {  // Y-Axis negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge, Eigen::Vector3d::UnitY());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge - eps, Eigen::Vector3d::UnitY());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, -eps, 0)));
  }

  {  // Y-Axis positive and negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge + eps, Eigen::Vector3d::UnitY());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge - eps, Eigen::Vector3d::UnitY());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, -2 * eps, 0)));
  }

  {  // Y-Axis translation
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge, Eigen::Vector3d::UnitY());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge - eps, Eigen::Vector3d::UnitY());
    source_tf_perturbed.translation() += Eigen::Vector3d(-eps, eps, -eps);
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d(-eps, eps, -eps)));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, -eps, 0)));
  }

  {  // Z-Axis positive
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge, Eigen::Vector3d::UnitZ());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge + eps, Eigen::Vector3d::UnitZ());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, 0, eps)));
  }

  {  // Z-Axis negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge, Eigen::Vector3d::UnitZ());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge - eps, Eigen::Vector3d::UnitZ());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, 0, -eps)));
  }

  {  // Z-Axis positive and negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge + eps, Eigen::Vector3d::UnitZ());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge - eps, Eigen::Vector3d::UnitZ());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, 0, -2 * eps)));
  }

  {  // Z-Axis translation
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(anlge, Eigen::Vector3d::UnitZ());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(anlge - eps, Eigen::Vector3d::UnitZ());
    source_tf_perturbed.translation() += Eigen::Vector3d(-eps, -eps, eps);
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(target_tf, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d(-eps, -eps, eps)));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, 0, -eps)));
  }
}

void runCalcJacobianTransformErrorDiffDynamicTargetTest(double angle)
{
  Eigen::Isometry3d identity = Eigen::Isometry3d::Identity();
  const double eps = 0.001;
  {  // X-Axis positive
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed = Eigen::AngleAxisd(-eps, Eigen::Vector3d::UnitX()) * target_tf;
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitX());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle + eps, Eigen::Vector3d::UnitX());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(diff.head(3), Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(2 * eps, 0, 0)));
  }

  {  // X-Axis negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed = Eigen::AngleAxisd(eps, Eigen::Vector3d::UnitX()) * target_tf;
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitX());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle - eps, Eigen::Vector3d::UnitX());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(diff.head(3), Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(-2 * eps, 0, 0)));
  }

  {  // X-Axis positive and negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed = Eigen::AngleAxisd(eps, Eigen::Vector3d::UnitX()) * target_tf;
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle + eps, Eigen::Vector3d::UnitX());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle - eps, Eigen::Vector3d::UnitX());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(diff.head(3), Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(-3 * eps, 0, 0)));
  }

  {  // X-Axis translation
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed =
        Eigen::AngleAxisd(eps, Eigen::Vector3d::UnitX()) * target_tf * Eigen::Translation3d(eps, -eps, eps);
    ;
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitX());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle - eps, Eigen::Vector3d::UnitX());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d(-eps, eps, -eps)));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(-2 * eps, 0, 0)));
  }

  {  // Y-Axis positive
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed = Eigen::AngleAxisd(-eps, Eigen::Vector3d::UnitY()) * target_tf;
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitY());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle + eps, Eigen::Vector3d::UnitY());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(diff.head(3), Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, 2 * eps, 0)));
  }

  {  // Y-Axis negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed = Eigen::AngleAxisd(eps, Eigen::Vector3d::UnitY()) * target_tf;
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitY());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle - eps, Eigen::Vector3d::UnitY());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(diff.head(3), Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, -2 * eps, 0)));
  }

  {  // Y-Axis positive and negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed = Eigen::AngleAxisd(eps, Eigen::Vector3d::UnitY()) * target_tf;
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle + eps, Eigen::Vector3d::UnitY());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle - eps, Eigen::Vector3d::UnitY());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(diff.head(3), Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, -3 * eps, 0)));
  }

  {  // Y-Axis translation
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed =
        Eigen::AngleAxisd(eps, Eigen::Vector3d::UnitY()) * target_tf * Eigen::Translation3d(-eps, eps, -eps);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitY());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle - eps, Eigen::Vector3d::UnitY());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d(eps, -eps, eps)));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, -2 * eps, 0)));
  }

  {  // Z-Axis positive
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed = Eigen::AngleAxisd(-eps, Eigen::Vector3d::UnitZ()) * target_tf;
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitZ());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle + eps, Eigen::Vector3d::UnitZ());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(diff.head(3), Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, 0, 2 * eps)));
  }

  {  // Z-Axis negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed = Eigen::AngleAxisd(eps, Eigen::Vector3d::UnitZ()) * target_tf;
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitZ());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle - eps, Eigen::Vector3d::UnitZ());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(diff.head(3), Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, 0, -2 * eps)));
  }

  {  // Z-Axis positive and negative
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed = Eigen::AngleAxisd(eps, Eigen::Vector3d::UnitZ()) * target_tf;
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle + eps, Eigen::Vector3d::UnitZ());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle - eps, Eigen::Vector3d::UnitZ());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(tesseract_common::almostEqualRelativeAndAbs(diff.head(3), Eigen::Vector3d::Zero()));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, 0, -3 * eps)));
  }

  {  // Z-Axis translation
    Eigen::Isometry3d target_tf{ identity };
    target_tf.translation() = Eigen::Vector3d(1, 2, 3);
    Eigen::Isometry3d target_tf_perturbed =
        Eigen::AngleAxisd(eps, Eigen::Vector3d::UnitZ()) * target_tf * Eigen::Translation3d(-eps, -eps, eps);
    Eigen::Isometry3d source_tf = identity * Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitZ());
    Eigen::Isometry3d source_tf_perturbed = identity * Eigen::AngleAxisd(angle - eps, Eigen::Vector3d::UnitZ());
    Eigen::VectorXd diff = tesseract_common::calcJacobianTransformErrorDiff(
        target_tf, target_tf_perturbed, source_tf, source_tf_perturbed);
    EXPECT_TRUE(diff.head(3).isApprox(Eigen::Vector3d(eps, eps, -eps)));
    EXPECT_TRUE(diff.tail(3).isApprox(Eigen::Vector3d(0, 0, -2 * eps)));
  }
}

/** @brief Tests calcJacobianTransformErrorDiff */
TEST(TesseractCommonUnit, calcJacobianTransformErrorDiff)  // NOLINT
{
  runCalcJacobianTransformErrorDiffTest(0);
  runCalcJacobianTransformErrorDiffTest(M_PI_2);
  runCalcJacobianTransformErrorDiffTest(M_PI);
  runCalcJacobianTransformErrorDiffTest(3 * M_PI_2);
  runCalcJacobianTransformErrorDiffTest(2 * M_PI);

  runCalcJacobianTransformErrorDiffDynamicTargetTest(0);
  runCalcJacobianTransformErrorDiffDynamicTargetTest(M_PI_2);
  runCalcJacobianTransformErrorDiffDynamicTargetTest(M_PI);
  runCalcJacobianTransformErrorDiffDynamicTargetTest(3 * M_PI_2);
  runCalcJacobianTransformErrorDiffDynamicTargetTest(2 * M_PI);
}

/** @brief Tests calcTransformError */
TEST(TesseractCommonUnit, computeRandomColor)  // NOLINT
{
  Eigen::Vector4d color = tesseract_common::computeRandomColor();
  EXPECT_FALSE(color(0) < 0);
  EXPECT_FALSE(color(1) < 0);
  EXPECT_FALSE(color(2) < 0);
  EXPECT_FALSE(color(3) < 0);
  EXPECT_FALSE(color(0) > 1);
  EXPECT_FALSE(color(1) > 1);
  EXPECT_FALSE(color(2) > 1);
  EXPECT_FALSE(color(3) > 1);
}

/** @brief Tests calcTransformError */
TEST(TesseractCommonUnit, concat)  // NOLINT
{
  Eigen::Vector3d a(1, 2, 3);
  Eigen::Vector3d b(4, 5, 6);

  Eigen::VectorXd c = tesseract_common::concat(a, b);
  EXPECT_EQ(c.rows(), a.rows() + b.rows());
  EXPECT_TRUE(c.head(3).isApprox(a));
  EXPECT_TRUE(c.tail(3).isApprox(b));
}

TEST(TesseractCommonUnit, TestAllowedCollisionMatrix)  // NOLINT
{
  tesseract_common::AllowedCollisionMatrix acm;
  acm.reserveAllowedCollisionMatrix(2);

  acm.addAllowedCollision("link1", "link2", "test");
  // collision between link1 and link2 should be allowed
  EXPECT_TRUE(acm.isCollisionAllowed("link1", "link2"));
  // but now between link2 and link3
  EXPECT_FALSE(acm.isCollisionAllowed("link2", "link3"));

  tesseract_common::AllowedCollisionMatrix acm_copy(acm);
  EXPECT_TRUE(acm_copy == acm);
  // collision between link1 and link2 should be allowed
  EXPECT_TRUE(acm_copy.isCollisionAllowed("link1", "link2"));
  // but now between link2 and link3
  EXPECT_FALSE(acm_copy.isCollisionAllowed("link2", "link3"));

  tesseract_common::AllowedCollisionMatrix acm_move(std::move(acm_copy));
  EXPECT_TRUE(acm_move == acm);
  // collision between link1 and link2 should be allowed
  EXPECT_TRUE(acm_move.isCollisionAllowed("link1", "link2"));
  // but now between link2 and link3
  EXPECT_FALSE(acm_move.isCollisionAllowed("link2", "link3"));

  acm.removeAllowedCollision("link1", "link2");
  // now collision link1 and link2 is not allowed anymore
  EXPECT_FALSE(acm.isCollisionAllowed("link1", "link2"));

  acm.addAllowedCollision("link3", "link3", "test");
  EXPECT_EQ(acm.getAllAllowedCollisions().size(), 1);
  acm.clearAllowedCollisions();
  EXPECT_EQ(acm.getAllAllowedCollisions().size(), 0);

  tesseract_common::AllowedCollisionMatrix acm2;
  acm.addAllowedCollision("link1", "link2", "test");
  acm2.addAllowedCollision("link1", "link2", "test");
  acm2.addAllowedCollision("link1", "link3", "test");
  acm.insertAllowedCollisionMatrix(acm2);

  EXPECT_EQ(acm.getAllAllowedCollisions().size(), 2);
  EXPECT_TRUE(acm.isCollisionAllowed("link1", "link2"));
  EXPECT_TRUE(acm.isCollisionAllowed("link1", "link3"));
  EXPECT_FALSE(acm.isCollisionAllowed("link2", "link3"));
  EXPECT_EQ(acm.getAllAllowedCollisions().size(), 2);

  acm.addAllowedCollision("link2", "link3", "test");
  acm.removeAllowedCollision("link1");
  EXPECT_EQ(acm.getAllAllowedCollisions().size(), 1);
  EXPECT_FALSE(acm.isCollisionAllowed("link1", "link2"));
  EXPECT_FALSE(acm.isCollisionAllowed("link1", "link3"));
  EXPECT_TRUE(acm.isCollisionAllowed("link2", "link3"));
  EXPECT_EQ(acm.getAllAllowedCollisions().size(), 1);

  // ostream
  std::stringstream ss;
  EXPECT_NO_THROW(ss << acm);  // NOLINT
}

TEST(TesseractCommonUnit, TestAllowedCollisionEntriesCompare)  // NOLINT
{
  tesseract_common::AllowedCollisionMatrix acm1;
  acm1.addAllowedCollision("link1", "link2", "test");

  tesseract_common::AllowedCollisionMatrix acm2;
  acm2.addAllowedCollision("link1", "link2", "test");

  EXPECT_TRUE(acm1.getAllAllowedCollisions() == acm2.getAllAllowedCollisions());

  acm2.addAllowedCollision("link1", "link3", "test");
  EXPECT_FALSE(acm1.getAllAllowedCollisions() == acm2.getAllAllowedCollisions());

  acm2.clearAllowedCollisions();
  acm2.addAllowedCollision("link1", "link3", "test");
  EXPECT_FALSE(acm1.getAllAllowedCollisions() == acm2.getAllAllowedCollisions());

  acm2.clearAllowedCollisions();
  acm2.addAllowedCollision("link1", "link2", "do_not_match");
  EXPECT_FALSE(acm1.getAllAllowedCollisions() == acm2.getAllAllowedCollisions());
}

TEST(TesseractCommonUnit, CollisionMarginDataUnit)  // NOLINT
{
  double tol = std::numeric_limits<double>::epsilon();

  {  // Test Default Constructor
    tesseract_common::CollisionMarginData data;
    EXPECT_NEAR(data.getDefaultCollisionMargin(), 0, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), 0, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), 0, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 0);

    double increment = 0.01;
    data.incrementMargins(increment);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), increment, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), increment, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), increment, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 0);

    increment = -0.01;
    data.incrementMargins(increment);
    data.incrementMargins(increment);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), increment, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), increment, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), increment, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 0);
  }

  {  // Test construction with non zero default distance
    double default_margin = 0.0254;
    tesseract_common::CollisionMarginData data(default_margin);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), default_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 0);

    double scale{ -1 };
    data.scaleMargins(scale);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), scale * default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), scale * default_margin, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), scale * default_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 0);

    data.scaleMargins(scale);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), default_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 0);
  }

  {  // Test changing default margin
    double default_margin = 0.0254;
    tesseract_common::CollisionMarginData data;
    data.setDefaultCollisionMargin(default_margin);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), default_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 0);
  }

  {  // Test construction with default margin and pair margins
    double default_margin = 0.0;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginPairData pair_margins;
    pair_margins.setCollisionMargin("link_1", "link_2", pair_margin);
    tesseract_common::CollisionMarginData data(pair_margins);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test construction with non default margin and pair margins
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginPairData pair_margins;
    pair_margins.setCollisionMargin("link_1", "link_2", pair_margin);
    tesseract_common::CollisionMarginData data(default_margin, pair_margins);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test construction with non default margin and pair margins
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::PairsCollisionMarginData temp;
    temp[std::make_pair("link_1", "link_2")] = pair_margin;

    tesseract_common::CollisionMarginPairData pair_margins(temp);
    tesseract_common::CollisionMarginData data(default_margin, pair_margins);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test construction with default margin and pair margins
    double default_margin = 0.0;
    double pair_margin = 0.5;
    tesseract_common::PairsCollisionMarginData temp;
    temp[std::make_pair("link_1", "link_2")] = pair_margin;
    tesseract_common::CollisionMarginPairData pair_margins(temp);
    tesseract_common::CollisionMarginData data(pair_margins);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test adding pair margin larger than default
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test adding pair margin less than default
    double default_margin = 0.0254;
    double pair_margin = 0.01;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test setting default larger than the current max margin
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);

    default_margin = 2 * pair_margin;
    data.setDefaultCollisionMargin(default_margin);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test setting pair_margin larger than default and then set it lower so the max should be the default
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);
    data.setCollisionMargin("link_1", "link_2", default_margin);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), default_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test setting default larger than pair the change to lower than pair and the max should be the pair
    double default_margin = 0.05;
    double pair_margin = 0.0254;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);

    default_margin = 0.0;
    data.setDefaultCollisionMargin(default_margin);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test increment positive
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    double increment = 0.01;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);
    data.incrementMargins(increment);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin + increment, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin + increment, pair_margin + increment), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin + increment, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test increment negative
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    double increment = -0.01;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);
    data.incrementMargins(increment);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin + increment, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin + increment, pair_margin + increment), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin + increment, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test scale > 1
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    double scale = 1.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);
    data.scaleMargins(scale);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin * scale, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin * scale, pair_margin * scale), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin * scale, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test scale < 1
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    double scale = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);
    data.scaleMargins(scale);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin * scale, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin * scale, pair_margin * scale), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin * scale, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test Apply Override Default
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);

    default_margin = default_margin * 3;
    data.setDefaultCollisionMargin(default_margin);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test Apply Override Link Pair
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);

    default_margin = default_margin * 3;
    pair_margin = pair_margin * 3;
    tesseract_common::CollisionMarginPairData override_pair_margins;
    override_pair_margins.setCollisionMargin("link_1", "link_2", pair_margin);
    data.setDefaultCollisionMargin(default_margin);
    data.apply(override_pair_margins, tesseract_common::CollisionMarginPairOverrideType::MODIFY);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test Apply Override Replace
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);

    default_margin = default_margin * 3;
    pair_margin = pair_margin * 3;
    tesseract_common::CollisionMarginPairData override_pair_margins;
    override_pair_margins.setCollisionMargin("link_1", "link_2", pair_margin);
    data.setDefaultCollisionMargin(default_margin);
    data.apply(override_pair_margins, tesseract_common::CollisionMarginPairOverrideType::REPLACE);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test Apply Override None
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);

    default_margin = default_margin * 3;
    tesseract_common::CollisionMarginPairData override_pair_margins;
    override_pair_margins.setCollisionMargin("link_1", "link_2", pair_margin * 3);
    data.setDefaultCollisionMargin(default_margin);
    data.apply(override_pair_margins, tesseract_common::CollisionMarginPairOverrideType::NONE);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 1);
  }

  {  // Test Apply Override Modify
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);

    default_margin = default_margin * 3;
    tesseract_common::CollisionMarginPairData override_pair_margins;
    override_pair_margins.setCollisionMargin("link_1", "link_3", pair_margin * 3);
    data.setDefaultCollisionMargin(default_margin);
    data.apply(override_pair_margins, tesseract_common::CollisionMarginPairOverrideType::MODIFY);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin * 3), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_3"), pair_margin * 3, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 2);

    // Test clearing the pair data
    auto pair_data_copy = data.getCollisionMarginPairData();
    EXPECT_EQ(pair_data_copy.getCollisionMargins().size(), 2);
    EXPECT_FALSE(pair_data_copy.empty());
    pair_data_copy.clear();
    EXPECT_EQ(pair_data_copy.getCollisionMargins().size(), 0);
    EXPECT_TRUE(pair_data_copy.empty());
  }

  {  // Test Apply Override Modify with pair that already exists
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);

    pair_margin = pair_margin * 3;
    default_margin = default_margin * 3;
    tesseract_common::CollisionMarginPairData override_pair_margins;
    override_pair_margins.setCollisionMargin("link_1", "link_2", pair_margin);
    override_pair_margins.setCollisionMargin("link_1", "link_3", pair_margin * 3);
    data.setDefaultCollisionMargin(default_margin);
    data.apply(override_pair_margins, tesseract_common::CollisionMarginPairOverrideType::MODIFY);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin * 3), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_3"), pair_margin * 3, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 2);
  }

  {  // Test Apply Override Modify Pair
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);

    default_margin = default_margin * 3;
    tesseract_common::CollisionMarginPairData override_pair_margins;
    override_pair_margins.setCollisionMargin("link_1", "link_3", pair_margin * 3);
    data.setDefaultCollisionMargin(default_margin);
    data.apply(override_pair_margins, tesseract_common::CollisionMarginPairOverrideType::MODIFY);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin * 3), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_3"), pair_margin * 3, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 2);
  }

  {  // Test Apply Override Modify Pair that already exists
    double default_margin = 0.0254;
    double pair_margin = 0.5;
    tesseract_common::CollisionMarginData data(default_margin);
    data.setCollisionMargin("link_1", "link_2", pair_margin);

    pair_margin = pair_margin * 3;
    default_margin = default_margin * 3;
    tesseract_common::CollisionMarginPairData override_pair_margins;
    override_pair_margins.setCollisionMargin("link_1", "link_2", pair_margin);
    override_pair_margins.setCollisionMargin("link_1", "link_3", pair_margin * 3);
    data.setDefaultCollisionMargin(default_margin);
    data.apply(override_pair_margins, tesseract_common::CollisionMarginPairOverrideType::MODIFY);
    EXPECT_NEAR(data.getDefaultCollisionMargin(), default_margin, tol);
    EXPECT_NEAR(data.getMaxCollisionMargin(), std::max(default_margin, pair_margin * 3), tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_2"), pair_margin, tol);
    EXPECT_NEAR(data.getCollisionMargin("link_1", "link_3"), pair_margin * 3, tol);
    EXPECT_EQ(data.getCollisionMarginPairData().getCollisionMargins().size(), 2);
  }
}

TEST(TesseractCommonUnit, CollisionMarginDataCompare)  // NOLINT
{
  {  // EQUAL Default
    tesseract_common::CollisionMarginData margin_data1;
    tesseract_common::CollisionMarginData margin_data2;

    EXPECT_TRUE(margin_data1 == margin_data2);
  }

  {  // EQUAL with pair data
    tesseract_common::CollisionMarginData margin_data1;
    margin_data1.setCollisionMargin("link_1", "link_2", 1);
    tesseract_common::CollisionMarginData margin_data2;
    margin_data2.setCollisionMargin("link_1", "link_2", 1);

    EXPECT_TRUE(margin_data1 == margin_data2);
  }

  {  // Not EQUAL Default
    tesseract_common::CollisionMarginData margin_data1(0.1);
    tesseract_common::CollisionMarginData margin_data2(0.2);

    EXPECT_FALSE(margin_data1 == margin_data2);
  }

  {  // Not EQUAL with pair data
    tesseract_common::CollisionMarginData margin_data1;
    margin_data1.setCollisionMargin("link_1", "link_2", 1);
    tesseract_common::CollisionMarginData margin_data2;
    margin_data2.setCollisionMargin("link_1", "link_2", 1);
    margin_data2.setCollisionMargin("link_1", "link_3", 1);

    EXPECT_FALSE(margin_data1 == margin_data2);
  }

  {  // Not EQUAL with pair data
    tesseract_common::CollisionMarginData margin_data1;
    margin_data1.setCollisionMargin("link_1", "link_2", 1);
    tesseract_common::CollisionMarginData margin_data2;
    margin_data2.setCollisionMargin("link_1", "link_2", 2);

    EXPECT_FALSE(margin_data1 == margin_data2);
  }

  {  // Not EQUAL with pair data
    tesseract_common::CollisionMarginData margin_data1;
    margin_data1.setCollisionMargin("link_1", "link_2", 1);
    tesseract_common::CollisionMarginData margin_data2;
    margin_data2.setCollisionMargin("link_1", "link_3", 1);

    EXPECT_FALSE(margin_data1 == margin_data2);
  }
}

// Helper function to create temporary test files
void createTestYamlWithIncludeDirectivesFile(const std::string& filePath, const std::string& content)
{
  std::ofstream file(filePath);
  ASSERT_TRUE(file.is_open());
  file << content;
  file.close();
}

TEST(TesseractCommonUnit, YamlBasicIncludeTest)  // NOLINT
{
  std::string separator(1, std::filesystem::path::preferred_separator);
  std::string test_dir = tesseract_common::getTempPath() + "test_yaml_1" + separator;

  // Create a temporary test directory
  std::filesystem::create_directory(test_dir);

  // Resource locator
  tesseract_common::GeneralResourceLocator locator;

  // Create test files
  createTestYamlWithIncludeDirectivesFile(test_dir + "main.yaml", R"(
key1: value1
key2: !include included.yaml
)");
  createTestYamlWithIncludeDirectivesFile(test_dir + "included.yaml", R"(
included_key1: included_value1
included_key2: included_value2
)");

  // Load the main file
  YAML::Node root = loadYamlFile(test_dir + "main.yaml", locator);
  tesseract_common::writeYamlToFile(root, test_dir + "processed.yaml");

  // Validate the structure
  ASSERT_TRUE(root["key1"].IsScalar());
  ASSERT_EQ(root["key1"].as<std::string>(), "value1");

  ASSERT_TRUE(root["key2"].IsMap());
  ASSERT_EQ(root["key2"]["included_key1"].as<std::string>(), "included_value1");
  ASSERT_EQ(root["key2"]["included_key2"].as<std::string>(), "included_value2");

  // Clean up the test directory
  std::filesystem::remove_all(test_dir);
}

TEST(TesseractCommonUnit, YamlIncludeNestedIncludesTest)  // NOLINT
{
  std::string separator(1, std::filesystem::path::preferred_separator);
  std::string test_dir = tesseract_common::getTempPath() + "test_yaml_2" + separator;

  // Create a temporary test directory
  std::filesystem::create_directory(test_dir);

  // Resource locator
  tesseract_common::GeneralResourceLocator locator;

  // Create test files
  createTestYamlWithIncludeDirectivesFile(test_dir + "main.yaml", R"(
key1: value1
key2: !include included.yaml
)");
  createTestYamlWithIncludeDirectivesFile(test_dir + "included.yaml", R"(
nested_key1: !include nested.yaml
)");
  createTestYamlWithIncludeDirectivesFile(test_dir + "nested.yaml", R"(
deep_key1: deep_value1
)");

  // Load the main file
  YAML::Node root = loadYamlFile(test_dir + "main.yaml", locator);
  tesseract_common::writeYamlToFile(root, test_dir + "processed.yaml");

  // Validate the structure
  ASSERT_TRUE(root["key2"].IsMap());
  ASSERT_TRUE(root["key2"]["nested_key1"].IsMap());
  ASSERT_EQ(root["key2"]["nested_key1"]["deep_key1"].as<std::string>(), "deep_value1");

  // Clean up the test directory
  std::filesystem::remove_all(test_dir);
}

TEST(TesseractCommonUnit, YamlIncludeSequenceIncludesTest)  // NOLINT
{
  std::string separator(1, std::filesystem::path::preferred_separator);
  std::string test_dir = tesseract_common::getTempPath() + "test_yaml_3" + separator;

  // Create a temporary test directory
  std::filesystem::create_directory(test_dir);

  // Resource locator
  tesseract_common::GeneralResourceLocator locator;

  // Create test files
  createTestYamlWithIncludeDirectivesFile(test_dir + "main.yaml", R"(
key1:
  - item1
  - !include included.yaml
)");
  createTestYamlWithIncludeDirectivesFile(test_dir + "included.yaml", R"(
- included_item1
- included_item2
)");

  // Load the main file
  YAML::Node root = loadYamlFile(test_dir + "main.yaml", locator);
  tesseract_common::writeYamlToFile(root, test_dir + "processed.yaml");

  // Validate the structure
  ASSERT_TRUE(root["key1"].IsSequence());
  ASSERT_EQ(root["key1"].size(), 2);
  ASSERT_EQ(root["key1"][0].as<std::string>(), "item1");
  ASSERT_EQ(root["key1"][1][0].as<std::string>(), "included_item1");
  ASSERT_EQ(root["key1"][1][1].as<std::string>(), "included_item2");

  // Clean up the test directory
  std::filesystem::remove_all(test_dir);
}

TEST(TesseractCommonUnit, YamlIncludeSequenceIncludesMapTest)  // NOLINT
{
  std::string separator(1, std::filesystem::path::preferred_separator);
  std::string test_dir = tesseract_common::getTempPath() + "test_yaml_4" + separator;

  // Create a temporary test directory
  std::filesystem::create_directory(test_dir);

  // Resource locator
  tesseract_common::GeneralResourceLocator locator;

  // Create test files
  createTestYamlWithIncludeDirectivesFile(test_dir + "main.yaml", R"(
key1:
  - item1
  - !include included.yaml
)");
  createTestYamlWithIncludeDirectivesFile(test_dir + "included.yaml", R"(
keyA: valueA
keyB: valueB
)");

  // Load the main file
  YAML::Node root = loadYamlFile(test_dir + "main.yaml", locator);
  tesseract_common::writeYamlToFile(root, test_dir + "processed.yaml");

  // Validate the structure
  ASSERT_TRUE(root["key1"].IsSequence());
  ASSERT_EQ(root["key1"].size(), 2);
  ASSERT_EQ(root["key1"][0].as<std::string>(), "item1");

  // Check the included map
  ASSERT_TRUE(root["key1"][1].IsMap());
  ASSERT_EQ(root["key1"][1]["keyA"].as<std::string>(), "valueA");
  ASSERT_EQ(root["key1"][1]["keyB"].as<std::string>(), "valueB");

  // Clean up the test directory
  std::filesystem::remove_all(test_dir);
}

TEST(TesseractCommonUnit, YamlIncludeMissingIncludeFileTest)  // NOLINT
{
  std::string separator(1, std::filesystem::path::preferred_separator);
  std::string test_dir = tesseract_common::getTempPath() + "test_yaml_5" + separator;

  // Create a temporary test directory
  std::filesystem::create_directory(test_dir);

  // Resource locator
  tesseract_common::GeneralResourceLocator locator;

  // Create a test file
  createTestYamlWithIncludeDirectivesFile(test_dir + "main.yaml", R"(
key1: !include missing.yaml
)");

  // Attempt to load the main file and expect an exception
  EXPECT_THROW(loadYamlFile(test_dir + "main.yaml", locator), std::runtime_error);  // NOLINT

  // Clean up the test directory
  std::filesystem::remove_all(test_dir);
}

TEST(TesseractCommonUnit, YamlIncludeInvalidIncludeTagTest)  // NOLINT
{
  std::string separator(1, std::filesystem::path::preferred_separator);
  std::string test_dir = tesseract_common::getTempPath() + "test_yaml_6" + separator;

  // Create a temporary test directory
  std::filesystem::create_directory(test_dir);

  // Resource locator
  tesseract_common::GeneralResourceLocator locator;

  // Create a test file with an invalid !include tag
  createTestYamlWithIncludeDirectivesFile(test_dir + "main.yaml", R"(
key1: !include
)");

  // Attempt to load the main file and expect an exception
  EXPECT_THROW(loadYamlFile(test_dir + "main.yaml", locator), std::runtime_error);  // NOLINT

  // Clean up the test directory
  std::filesystem::remove_all(test_dir);
}

TEST(TesseractCommonUnit, YamlCollisionMarginPairOverrideType)  // NOLINT
{
  {
    YAML::Node n = YAML::Load(R"(NONE)");
    auto v = n.as<tesseract_common::CollisionMarginPairOverrideType>();
    EXPECT_EQ(v, tesseract_common::CollisionMarginPairOverrideType::NONE);
  }

  {
    YAML::Node n = YAML::Load(R"(MODIFY)");
    auto v = n.as<tesseract_common::CollisionMarginPairOverrideType>();
    EXPECT_EQ(v, tesseract_common::CollisionMarginPairOverrideType::MODIFY);
  }

  {
    YAML::Node n = YAML::Load(R"(REPLACE)");
    auto v = n.as<tesseract_common::CollisionMarginPairOverrideType>();
    EXPECT_EQ(v, tesseract_common::CollisionMarginPairOverrideType::REPLACE);
  }

  {
    YAML::Node n(tesseract_common::CollisionMarginPairOverrideType::NONE);
    auto v = n.as<tesseract_common::CollisionMarginPairOverrideType>();
    EXPECT_EQ(v, tesseract_common::CollisionMarginPairOverrideType::NONE);
  }

  {
    YAML::Node n(tesseract_common::CollisionMarginPairOverrideType::MODIFY);
    auto v = n.as<tesseract_common::CollisionMarginPairOverrideType>();
    EXPECT_EQ(v, tesseract_common::CollisionMarginPairOverrideType::MODIFY);
  }

  {
    YAML::Node n(tesseract_common::CollisionMarginPairOverrideType::REPLACE);
    auto v = n.as<tesseract_common::CollisionMarginPairOverrideType>();
    EXPECT_EQ(v, tesseract_common::CollisionMarginPairOverrideType::REPLACE);
  }

  {
    YAML::Node n = YAML::Load(R"(DOES_NOT_EXIST)");
    EXPECT_ANY_THROW(n.as<tesseract_common::CollisionMarginPairOverrideType>());  // NOLINT
  }

  {
    YAML::Node n = YAML::Load(R"(["test", "test"])");
    EXPECT_ANY_THROW(n.as<tesseract_common::CollisionMarginPairOverrideType>());  // NOLINT
  }
}

TEST(TesseractCommonUnit, YamlPairsCollisionMarginData)  // NOLINT
{
  const std::string yaml_string = R"(
  ["link1","link2"]: 0.8
  ["base","tool0"]: 1.5
)";

  tesseract_common::PairsCollisionMarginData data_original;
  data_original[std::make_pair("link1", "link2")] = 0.8;
  data_original[std::make_pair("base", "tool0")] = 1.5;

  {
    YAML::Node n(data_original);
    auto data = n.as<tesseract_common::PairsCollisionMarginData>();
    EXPECT_EQ(data.size(), 2U);
    EXPECT_DOUBLE_EQ(data.at({ "link1", "link2" }), data_original[std::make_pair("link1", "link2")]);
    EXPECT_DOUBLE_EQ(data.at({ "base", "tool0" }), data_original[std::make_pair("base", "tool0")]);
  }

  {
    YAML::Node n = YAML::Load(yaml_string);
    auto data = n.as<tesseract_common::PairsCollisionMarginData>();
    EXPECT_EQ(data.size(), 2U);
    EXPECT_DOUBLE_EQ(data.at({ "link1", "link2" }), data_original[std::make_pair("link1", "link2")]);
    EXPECT_DOUBLE_EQ(data.at({ "base", "tool0" }), data_original[std::make_pair("base", "tool0")]);
  }

  {  // Failure: Is not map
    YAML::Node n = YAML::Load(R"(["test", "test"])");
    EXPECT_ANY_THROW(n.as<tesseract_common::PairsCollisionMarginData>());  // NOLINT
  }

  {  // Failure: Invalid Key
    const std::string invalid_yaml = R"(
  ["link1","link2", "link2"]: 0.8
  ["base","tool0"]: 1.5
)";
    YAML::Node n = YAML::Load(invalid_yaml);
    EXPECT_ANY_THROW(n.as<tesseract_common::PairsCollisionMarginData>());  // NOLINT
  }
}

TEST(TesseractCommonUnit, YamlCollisionMarginPairData)  // NOLINT
{
  const std::string yaml_string = R"(
  ["link1","link2"]: 0.8
  ["base","tool0"]: 1.5
)";

  tesseract_common::CollisionMarginPairData data_original;
  {
    tesseract_common::PairsCollisionMarginData pair_data;
    pair_data[std::make_pair("link1", "link2")] = 0.8;
    pair_data[std::make_pair("base", "tool0")] = 1.5;
    data_original = tesseract_common::CollisionMarginPairData(pair_data);
  }

  {
    YAML::Node n = YAML::Load(yaml_string);
    auto data = n.as<tesseract_common::CollisionMarginPairData>();
    EXPECT_EQ(data.getCollisionMargins().size(), 2U);
    // NOLINTNEXTLINE
    EXPECT_DOUBLE_EQ(data.getCollisionMargin("link1", "link2").value(),
                     data_original.getCollisionMargin("link1", "link2").value());
    // NOLINTNEXTLINE
    EXPECT_DOUBLE_EQ(data.getCollisionMargin("base", "tool0").value(),
                     data_original.getCollisionMargin("base", "tool0").value());
  }

  {
    YAML::Node n(data_original);
    auto data = n.as<tesseract_common::CollisionMarginPairData>();
    EXPECT_EQ(data.getCollisionMargins().size(), 2U);
    // NOLINTNEXTLINE
    EXPECT_DOUBLE_EQ(data.getCollisionMargin("link1", "link2").value(),
                     data_original.getCollisionMargin("link1", "link2").value());
    // NOLINTNEXTLINE
    EXPECT_DOUBLE_EQ(data.getCollisionMargin("base", "tool0").value(),
                     data_original.getCollisionMargin("base", "tool0").value());
  }
}

TEST(TesseractCommonUnit, YamlAllowedCollisionEntries)  // NOLINT
{
  const std::string yaml_string = R"(
  ["link1","link2"]: "adjacent"
  ["base","tool0"]: "never"
)";

  tesseract_common::AllowedCollisionEntries data_original;
  data_original[std::make_pair("link1", "link2")] = "adjacent";
  data_original[std::make_pair("base", "tool0")] = "never";

  {
    YAML::Node n(data_original);
    auto data = n.as<tesseract_common::AllowedCollisionEntries>();
    EXPECT_EQ(data.size(), 2U);
    EXPECT_EQ(data.at({ "link1", "link2" }), data_original[std::make_pair("link1", "link2")]);
    EXPECT_EQ(data.at({ "base", "tool0" }), data_original[std::make_pair("base", "tool0")]);
  }

  {
    YAML::Node n = YAML::Load(yaml_string);
    auto data = n.as<tesseract_common::AllowedCollisionEntries>();
    EXPECT_EQ(data.size(), 2U);
    EXPECT_EQ(data.at({ "link1", "link2" }), data_original[std::make_pair("link1", "link2")]);
    EXPECT_EQ(data.at({ "base", "tool0" }), data_original[std::make_pair("base", "tool0")]);
  }

  {  // Failure: Is not map
    YAML::Node n = YAML::Load(R"(["test", "test"])");
    EXPECT_ANY_THROW(n.as<tesseract_common::AllowedCollisionEntries>());  // NOLINT
  }

  {  // Failure: Invalid Key
    const std::string invalid_yaml = R"(
  ["link1","link2", "link2"]: 0.8
  ["base","tool0"]: 1.5
)";
    YAML::Node n = YAML::Load(invalid_yaml);
    EXPECT_ANY_THROW(n.as<tesseract_common::AllowedCollisionEntries>());  // NOLINT
  }
}

TEST(TesseractCommonUnit, YamlAllowedCollisionMatrix)  // NOLINT
{
  const std::string yaml_string = R"(
  ["link1","link2"]: "adjacent"
  ["base","tool0"]: "never"
)";

  tesseract_common::AllowedCollisionMatrix data_original;
  {
    tesseract_common::AllowedCollisionEntries pair_data;
    pair_data[std::make_pair("link1", "link2")] = "adjacent";
    pair_data[std::make_pair("base", "tool0")] = "never";
    data_original = tesseract_common::AllowedCollisionMatrix(pair_data);
  }

  {
    YAML::Node n(data_original);
    auto data = n.as<tesseract_common::AllowedCollisionMatrix>();
    EXPECT_EQ(data.getAllAllowedCollisions().size(), 2U);
    EXPECT_EQ(data.isCollisionAllowed("link1", "link2"), data_original.isCollisionAllowed("link1", "link2"));
    EXPECT_EQ(data.isCollisionAllowed("base", "tool0"), data_original.isCollisionAllowed("base", "tool0"));
  }

  {
    YAML::Node n = YAML::Load(yaml_string);
    auto data = n.as<tesseract_common::AllowedCollisionMatrix>();
    EXPECT_EQ(data.getAllAllowedCollisions().size(), 2U);
    EXPECT_EQ(data.isCollisionAllowed("link1", "link2"), data_original.isCollisionAllowed("link1", "link2"));
    EXPECT_EQ(data.isCollisionAllowed("base", "tool0"), data_original.isCollisionAllowed("base", "tool0"));
  }
}

TEST(TesseractCommonUnit, YamlStdUnorderedMapStringBool)  // NOLINT
{
  using DataType = std::unordered_map<std::string, bool>;

  const std::string yaml_string = R"(
  link1: true
  tool0: false
)";

  DataType data_original;
  data_original["link1"] = true;
  data_original["tool0"] = false;

  {
    YAML::Node n(data_original);
    auto data = n.as<DataType>();
    EXPECT_EQ(data.size(), 2U);
    EXPECT_EQ(data["link1"], data_original["link1"]);
    EXPECT_EQ(data["tool0"], data_original["tool0"]);
  }

  {
    YAML::Node n = YAML::Load(yaml_string);
    auto data = n.as<DataType>();
    EXPECT_EQ(data.size(), 2U);
    EXPECT_EQ(data["link1"], data_original["link1"]);
    EXPECT_EQ(data["tool0"], data_original["tool0"]);
  }

  {  // Failure: Is not map
    YAML::Node n = YAML::Load(R"(["test", "test"])");
    EXPECT_ANY_THROW(n.as<DataType>());  // NOLINT
  }
}

static constexpr char const* NO_COLOR_USE_TRIANGLE_PLY = "test_no_color_use_triangle.ply";
static constexpr char const* NO_COLOR_PLY = "test_no_color.ply";
static constexpr char const* SINGLE_COLOR_PLY = "test_single_color.ply";
static constexpr char const* PER_VERTEX_COLOR_PLY = "test_per_vertex_color.ply";
static constexpr char const* MISSING_PLY = "no_such_file.ply";

TEST(TesseractCommonUnit, PlyIO_WriteAndLoadWithoutColor)  // NOLINT
{
  // Prepare a single quad (4 verts, 1 face)
  tesseract_common::VectorVector3d verts{ { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };

  // One face: quad with 4 vertex indices
  Eigen::VectorXi faces(5);
  faces << 4, 0, 1, 2, 3;
  int num_faces = 1;

  // Write PLY (no color)
  EXPECT_TRUE(
      tesseract_common::writeSimplePlyFile(tesseract_common::getTempPath() + NO_COLOR_PLY, verts, faces, num_faces));

  // Load back
  tesseract_common::VectorVector3d loaded_verts;
  Eigen::VectorXi loaded_faces;
  int ret = tesseract_common::loadSimplePlyFile(
      tesseract_common::getTempPath() + NO_COLOR_PLY, loaded_verts, loaded_faces, /*triangles_only=*/false);

  // Should get exactly one face back
  EXPECT_EQ(ret, num_faces);
  EXPECT_EQ(loaded_verts.size(), verts.size());
  // Compare vertex positions
  for (size_t i = 0; i < verts.size(); ++i)
  {
    EXPECT_DOUBLE_EQ(loaded_verts[i].x(), verts[i].x());
    EXPECT_DOUBLE_EQ(loaded_verts[i].y(), verts[i].y());
    EXPECT_DOUBLE_EQ(loaded_verts[i].z(), verts[i].z());
  }
  // Compare face buffer
  ASSERT_EQ(loaded_faces.size(), faces.size());
  for (int i = 0; i < faces.size(); ++i)
    EXPECT_EQ(loaded_faces[i], faces[i]);
}

TEST(TesseractCommonUnit, PlyIO_WriteWithSingleColor)  // NOLINT
{
  // Two vertices, single default color
  tesseract_common::VectorVector3d verts{ { 0, 0, 0 }, { 1, 0, 0 } };
  std::vector<Eigen::Vector3i> colors{ Eigen::Vector3i{ 255, 128, 64 } };

  // One edge (treated as face of 2 verts here for demonstration)
  Eigen::VectorXi faces(3);
  faces << 2, 0, 1;
  int num_faces = 1;

  // Write PLY with per-vertex default color
  EXPECT_TRUE(tesseract_common::writeSimplePlyFile(
      tesseract_common::getTempPath() + SINGLE_COLOR_PLY, verts, colors, faces, num_faces));

  // Open file and inspect header + first vertex line
  std::ifstream ifs(tesseract_common::getTempPath() + SINGLE_COLOR_PLY);
  ASSERT_TRUE(ifs.is_open());

  std::string line;
  bool saw_red_prop = false;
  std::vector<std::string> file_lines;
  while (std::getline(ifs, line))
  {
    file_lines.push_back(line);
    if (line == "property uchar red")
      saw_red_prop = true;
  }
  ifs.close();

  // Expect the color property in header
  EXPECT_TRUE(saw_red_prop);

  // Locate "end_header"
  auto it = std::find(file_lines.begin(), file_lines.end(), "end_header");
  ASSERT_NE(it, file_lines.end());
  auto header_idx = static_cast<std::size_t>(std::distance(file_lines.begin(), it));
  ASSERT_GT(file_lines.size(), header_idx + 1);

  // First vertex line should have: x y z R G B
  std::istringstream vs(file_lines[header_idx + 1]);
  std::vector<std::string> tokens;
  while (vs >> line)
    tokens.push_back(line);

  EXPECT_EQ(tokens.size(), 6);
  EXPECT_EQ(tokens[3], "255");
  EXPECT_EQ(tokens[4], "128");
  EXPECT_EQ(tokens[5], "64");
}

TEST(TesseractCommonUnit, PlyIO_WriteWithPerVertexColors)
{
  // 3 vertices, each with its own color
  tesseract_common::VectorVector3d verts{ { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
  std::vector<Eigen::Vector3i> colors{ Eigen::Vector3i{ 10, 20, 30 },
                                       Eigen::Vector3i{ 40, 50, 60 },
                                       Eigen::Vector3i{ 70, 80, 90 } };

  // Single triangular face
  Eigen::VectorXi faces(4);
  faces << 3, 0, 1, 2;
  int num_faces = 1;

  // Write PLY with per-vertex colors
  EXPECT_TRUE(tesseract_common::writeSimplePlyFile(
      tesseract_common::getTempPath() + PER_VERTEX_COLOR_PLY, verts, colors, faces, num_faces));

  // Read the entire file
  std::ifstream ifs(tesseract_common::getTempPath() + PER_VERTEX_COLOR_PLY);
  ASSERT_TRUE(ifs.is_open());
  std::string line;
  std::vector<std::string> lines;
  bool saw_color_property = false;
  while (std::getline(ifs, line))
  {
    lines.push_back(line);
    if (line == "property uchar blue")
      saw_color_property = true;
  }
  ifs.close();

  // Header must contain the RGB property lines
  EXPECT_TRUE(saw_color_property);

  // Find end_header
  auto it = std::find(lines.begin(), lines.end(), "end_header");
  ASSERT_NE(it, lines.end());
  auto header_idx = static_cast<std::size_t>(std::distance(lines.begin(), it));

  // Next 3 lines are the vertices with their colors
  for (std::size_t i = 0; i < verts.size(); ++i)
  {
    ASSERT_GT(lines.size(), header_idx + 1 + i);
    std::istringstream ss(lines[header_idx + 1 + i]);
    std::vector<std::string> tokens;
    while (ss >> line)
      tokens.push_back(line);

    // Expect: x y z R G B
    ASSERT_EQ(tokens.size(), 6U);
    EXPECT_EQ(tokens[3], std::to_string(colors[i][0]));
    EXPECT_EQ(tokens[4], std::to_string(colors[i][1]));
    EXPECT_EQ(tokens[5], std::to_string(colors[i][2]));
  }
}

TEST(TesseractCommonUnit, PlyIO_WriteAndLoadWithoutColorAndUseTriangles)  // NOLINT
{
  // Prepare a single quad (4 verts, 1 face)
  tesseract_common::VectorVector3d verts{ { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };

  // One face: quad with 4 vertex indices
  Eigen::VectorXi faces(5);
  faces << 4, 0, 1, 2, 3;
  int num_faces = 1;

  // Write PLY (no color)
  EXPECT_TRUE(tesseract_common::writeSimplePlyFile(
      tesseract_common::getTempPath() + NO_COLOR_USE_TRIANGLE_PLY, verts, faces, num_faces));

  // Load back
  tesseract_common::VectorVector3d loaded_verts;
  Eigen::VectorXi loaded_faces;
  int ret = tesseract_common::loadSimplePlyFile(
      tesseract_common::getTempPath() + NO_COLOR_USE_TRIANGLE_PLY, loaded_verts, loaded_faces, true);

  // Should get exactly one face back
  EXPECT_EQ(ret, num_faces + 1);
  EXPECT_EQ(loaded_verts.size(), verts.size());
  // Compare vertex positions
  for (size_t i = 0; i < verts.size(); ++i)
  {
    EXPECT_DOUBLE_EQ(loaded_verts[i].x(), verts[i].x());
    EXPECT_DOUBLE_EQ(loaded_verts[i].y(), verts[i].y());
    EXPECT_DOUBLE_EQ(loaded_verts[i].z(), verts[i].z());
  }
  // Compare face buffer
  Eigen::VectorXi loaded_faces_check(8);
  loaded_faces_check << 3, 0, 1, 2, 3, 0, 2, 3;
  ASSERT_EQ(loaded_faces.size(), loaded_faces_check.size());
  for (int i = 0; i < loaded_faces_check.size(); ++i)
    EXPECT_EQ(loaded_faces[i], loaded_faces_check[i]);
}

// Test failure when the "element vertex" line is malformed
TEST(TesseractCommonUnit, PlyIO_LoadSimplePlyFileFailures)
{
  {  // Test failure when the "element vertex" line is malformed
    const std::string path = tesseract_common::getTempPath() + "bad_vertex_header.ply";
    {
      std::ofstream ofs(path);
      ofs << "ply\n";
      ofs << "format ascii 1.0\n";
      ofs << "comment test\n";
      // Here we intentionally break the header: too many tokens
      ofs << "element vertex 4 extra\n";
      ofs << "property float x\nproperty float y\nproperty float z\n";
      ofs << "element face 1\nproperty list uchar int vertex_indices\n";
      ofs << "end_header\n";
      // dummy data
      ofs << "0 0 0\n0 0 0 0\n";
    }
    tesseract_common::VectorVector3d verts;
    Eigen::VectorXi faces;
    int ret = tesseract_common::loadSimplePlyFile(path, verts, faces, /*triangles_only=*/false);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(verts.empty());
    EXPECT_EQ(faces.size(), 0);
  }

  {  // Test failure when the "element face" line is malformed
    const std::string path = tesseract_common::getTempPath() + "bad_face_header.ply";
    {
      std::ofstream ofs(path);
      ofs << "ply\n";
      ofs << "format ascii 1.0\n";
      ofs << "comment test\n";
      ofs << "element vertex 1\n";
      ofs << "property float x\nproperty float y\nproperty float z\n";
      // Break the face header: missing numeric face count
      ofs << "element face faces\n";
      ofs << "property list uchar int vertex_indices\n";
      ofs << "end_header\n";
      ofs << "0 0 0\n";    // one vertex
      ofs << "3 0 0 0\n";  // dummy face
    }
    tesseract_common::VectorVector3d verts;
    Eigen::VectorXi faces;
    int ret = tesseract_common::loadSimplePlyFile(path, verts, faces, false);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(verts.empty());
    EXPECT_EQ(faces.size(), 0);
  }

  {  // Test failure when the "end_header" line is missing or wrong
    const std::string path = tesseract_common::getTempPath() + "no_end_header.ply";
    {
      std::ofstream ofs(path);
      ofs << "ply\n";
      ofs << "format ascii 1.0\n";
      ofs << "comment test\n";
      ofs << "element vertex 1\n";
      ofs << "property float x\nproperty float y\nproperty float z\n";
      ofs << "element face 1\n";
      ofs << "property list uchar int vertex_indices\n";
      // Missing "end_header" here:
      ofs << "header_over\n";
      ofs << "0 0 0\n";    // one vertex
      ofs << "3 0 0 0\n";  // one face
    }
    tesseract_common::VectorVector3d verts;
    Eigen::VectorXi faces;
    int ret = tesseract_common::loadSimplePlyFile(path, verts, faces, false);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(verts.empty());
    EXPECT_EQ(faces.size(), 0);
  }

  {  // Test failure on a malformed vertex data line (not exactly 3 floats)
    const std::string path = tesseract_common::getTempPath() + "bad_vertex_data.ply";
    {
      std::ofstream ofs(path);
      ofs << "ply\n";
      ofs << "format ascii 1.0\n";
      ofs << "comment test\n";
      ofs << "element vertex 2\n";
      ofs << "property float x\nproperty float y\nproperty float z\n";
      ofs << "element face 1\n";
      ofs << "property list uchar int vertex_indices\n";
      ofs << "end_header\n";
      ofs << "0 0   \n";   // too few values
      ofs << "1 1 1\n";    // correct line
      ofs << "3 0 1 1\n";  // face
    }
    tesseract_common::VectorVector3d verts;
    Eigen::VectorXi faces;
    int ret = tesseract_common::loadSimplePlyFile(path, verts, faces, false);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(verts.empty());
    EXPECT_EQ(faces.size(), 0);
  }

  {  // Test failure on a malformed face data line (fewer than 3 indices)
    const std::string path = tesseract_common::getTempPath() + "bad_face_data.ply";
    {
      std::ofstream ofs(path);
      ofs << "ply\n";
      ofs << "format ascii 1.0\n";
      ofs << "comment test\n";
      ofs << "element vertex 3\n";
      ofs << "property float x\nproperty float y\nproperty float z\n";
      ofs << "element face 1\n";
      ofs << "property list uchar int vertex_indices\n";
      ofs << "end_header\n";
      ofs << "0 0 0\n1 1 1\n2 2 2\n";  // three vertices
      ofs << "2 0 1\n";                // only 2 indices → error
    }
    tesseract_common::VectorVector3d verts;
    Eigen::VectorXi faces;
    int ret = tesseract_common::loadSimplePlyFile(path, verts, faces, false);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(verts.empty());
    EXPECT_EQ(faces.size(), 0);
  }
}

TEST(TesseractCommonUnit, PlyIO_LoadNonexistentFile)  // NOLINT
{
  tesseract_common::VectorVector3d verts;
  Eigen::VectorXi faces;
  // Should return 0 and print an error
  int ret = tesseract_common::loadSimplePlyFile(tesseract_common::getTempPath() + MISSING_PLY, verts, faces, false);
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(verts.empty());
  EXPECT_EQ(faces.size(), 0);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
