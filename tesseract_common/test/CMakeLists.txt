find_gtest()
find_package(boost_plugin_loader REQUIRED)

add_executable(${PROJECT_NAME}_serialization_unit tesseract_common_serialization_unit.cpp)
target_link_libraries(${PROJECT_NAME}_serialization_unit PRIVATE GTest::GTest GTest::Main ${PROJECT_NAME})
target_compile_options(${PROJECT_NAME}_serialization_unit PRIVATE ${TESSERACT_COMPILE_OPTIONS_PRIVATE}
                                                                  ${TESSERACT_COMPILE_OPTIONS_PUBLIC})
target_compile_definitions(${PROJECT_NAME}_serialization_unit PRIVATE ${TESSERACT_COMPILE_DEFINITIONS})
target_clang_tidy(${PROJECT_NAME}_serialization_unit ENABLE ${TESSERACT_ENABLE_CLANG_TIDY})
target_cxx_version(${PROJECT_NAME}_serialization_unit PUBLIC VERSION ${TESSERACT_CXX_VERSION})
target_code_coverage(
  ${PROJECT_NAME}_serialization_unit
  PRIVATE
  ALL
  EXCLUDE ${COVERAGE_EXCLUDE}
  ENABLE ${TESSERACT_ENABLE_CODE_COVERAGE})
add_gtest_discover_tests(${PROJECT_NAME}_serialization_unit)
add_dependencies(${PROJECT_NAME}_serialization_unit ${PROJECT_NAME})
add_dependencies(run_tests ${PROJECT_NAME}_serialization_unit)

add_executable(${PROJECT_NAME}_unit tesseract_common_unit.cpp)
target_link_libraries(${PROJECT_NAME}_unit PRIVATE GTest::GTest GTest::Main ${PROJECT_NAME})
target_compile_options(${PROJECT_NAME}_unit PRIVATE ${TESSERACT_COMPILE_OPTIONS_PRIVATE}
                                                    ${TESSERACT_COMPILE_OPTIONS_PUBLIC})
target_compile_definitions(${PROJECT_NAME}_unit PRIVATE ${TESSERACT_COMPILE_DEFINITIONS})
target_clang_tidy(${PROJECT_NAME}_unit ENABLE ${TESSERACT_ENABLE_CLANG_TIDY})
target_cxx_version(${PROJECT_NAME}_unit PUBLIC VERSION ${TESSERACT_CXX_VERSION})
target_code_coverage(
  ${PROJECT_NAME}_unit
  PRIVATE
  ALL
  EXCLUDE ${COVERAGE_EXCLUDE}
  ENABLE ${TESSERACT_ENABLE_CODE_COVERAGE})
add_gtest_discover_tests(${PROJECT_NAME}_unit)
add_dependencies(${PROJECT_NAME}_unit ${PROJECT_NAME})
add_dependencies(run_tests ${PROJECT_NAME}_unit)

add_executable(${PROJECT_NAME}_clone_cache_unit clone_cache_unit.cpp)
target_link_libraries(
  ${PROJECT_NAME}_clone_cache_unit
  PRIVATE GTest::GTest
          GTest::Main
          ${PROJECT_NAME}
          console_bridge::console_bridge)
target_compile_options(${PROJECT_NAME}_clone_cache_unit PRIVATE ${TESSERACT_COMPILE_OPTIONS_PRIVATE}
                                                                ${TESSERACT_COMPILE_OPTIONS_PUBLIC})
target_compile_definitions(${PROJECT_NAME}_clone_cache_unit PRIVATE ${TESSERACT_COMPILE_DEFINITIONS})
target_clang_tidy(${PROJECT_NAME}_clone_cache_unit ENABLE ${TESSERACT_ENABLE_CLANG_TIDY})
target_cxx_version(${PROJECT_NAME}_clone_cache_unit PUBLIC VERSION ${TESSERACT_CXX_VERSION})
target_code_coverage(
  ${PROJECT_NAME}_clone_cache_unit
  PRIVATE
  ALL
  EXCLUDE ${COVERAGE_EXCLUDE}
  ENABLE ${TESSERACT_ENABLE_CODE_COVERAGE})
add_gtest_discover_tests(${PROJECT_NAME}_clone_cache_unit)
add_dependencies(${PROJECT_NAME}_clone_cache_unit ${PROJECT_NAME})
add_dependencies(run_tests ${PROJECT_NAME}_clone_cache_unit)

add_library(${PROJECT_NAME}_test_plugins test_plugin_multiply.cpp test_profile_factory.cpp)
target_link_libraries(${PROJECT_NAME}_test_plugins PUBLIC ${PROJECT_NAME} boost_plugin_loader::boost_plugin_loader)
target_compile_options(${PROJECT_NAME}_test_plugins PUBLIC ${TESSERACT_COMPILE_OPTIONS_PRIVATE}
                                                           ${TESSERACT_COMPILE_OPTIONS_PUBLIC})
target_compile_definitions(${PROJECT_NAME}_test_plugins PUBLIC ${TESSERACT_COMPILE_DEFINITIONS})
target_clang_tidy(${PROJECT_NAME}_test_plugins ENABLE ${TESSERACT_ENABLE_CLANG_TIDY})
target_cxx_version(${PROJECT_NAME}_test_plugins PUBLIC VERSION ${TESSERACT_CXX_VERSION})

add_executable(${PROJECT_NAME}_plugin_loader_unit plugin_loader_unit.cpp)
target_link_libraries(
  ${PROJECT_NAME}_plugin_loader_unit
  PRIVATE GTest::GTest
          GTest::Main
          ${PROJECT_NAME}
          console_bridge::console_bridge
          boost_plugin_loader::boost_plugin_loader)
target_compile_options(${PROJECT_NAME}_plugin_loader_unit PRIVATE ${TESSERACT_COMPILE_OPTIONS_PRIVATE}
                                                                  ${TESSERACT_COMPILE_OPTIONS_PUBLIC})
target_compile_definitions(${PROJECT_NAME}_plugin_loader_unit PRIVATE ${TESSERACT_COMPILE_DEFINITIONS})
target_compile_definitions(${PROJECT_NAME}_plugin_loader_unit PRIVATE TEST_PLUGIN_DIR="${CMAKE_CURRENT_BINARY_DIR}")
target_clang_tidy(${PROJECT_NAME}_plugin_loader_unit ENABLE ${TESSERACT_ENABLE_CLANG_TIDY})
target_cxx_version(${PROJECT_NAME}_plugin_loader_unit PUBLIC VERSION ${TESSERACT_CXX_VERSION})
target_code_coverage(
  ${PROJECT_NAME}_plugin_loader_unit
  PRIVATE
  ALL
  EXCLUDE ${COVERAGE_EXCLUDE}
  ENABLE ${TESSERACT_ENABLE_CODE_COVERAGE})
add_gtest_discover_tests(${PROJECT_NAME}_plugin_loader_unit)
add_dependencies(${PROJECT_NAME}_plugin_loader_unit ${PROJECT_NAME})
add_dependencies(run_tests ${PROJECT_NAME}_plugin_loader_unit)

add_executable(${PROJECT_NAME}_profile_plugin_factory_unit profile_plugin_factory_unit.cpp)
target_link_libraries(
  ${PROJECT_NAME}_profile_plugin_factory_unit
  PRIVATE GTest::GTest
          GTest::Main
          ${PROJECT_NAME}
          console_bridge::console_bridge
          boost_plugin_loader::boost_plugin_loader)
target_compile_options(${PROJECT_NAME}_profile_plugin_factory_unit PRIVATE ${TESSERACT_COMPILE_OPTIONS_PRIVATE}
                                                                           ${TESSERACT_COMPILE_OPTIONS_PUBLIC})
target_compile_definitions(${PROJECT_NAME}_profile_plugin_factory_unit PRIVATE ${TESSERACT_COMPILE_DEFINITIONS})
target_compile_definitions(${PROJECT_NAME}_profile_plugin_factory_unit
                           PRIVATE TEST_PLUGIN_DIR="${CMAKE_CURRENT_BINARY_DIR}")
target_clang_tidy(${PROJECT_NAME}_profile_plugin_factory_unit ENABLE ${TESSERACT_ENABLE_CLANG_TIDY})
target_cxx_version(${PROJECT_NAME}_profile_plugin_factory_unit PUBLIC VERSION ${TESSERACT_CXX_VERSION})
target_code_coverage(
  ${PROJECT_NAME}_profile_plugin_factory_unit
  PRIVATE
  ALL
  EXCLUDE ${COVERAGE_EXCLUDE}
  ENABLE ${TESSERACT_ENABLE_CODE_COVERAGE})
add_gtest_discover_tests(${PROJECT_NAME}_profile_plugin_factory_unit)
add_dependencies(${PROJECT_NAME}_profile_plugin_factory_unit ${PROJECT_NAME})
add_dependencies(run_tests ${PROJECT_NAME}_profile_plugin_factory_unit)

add_executable(${PROJECT_NAME}_resource_unit resource_locator_unit.cpp)
target_link_libraries(${PROJECT_NAME}_resource_unit PRIVATE GTest::GTest GTest::Main ${PROJECT_NAME})
target_compile_options(${PROJECT_NAME}_resource_unit PRIVATE ${TESSERACT_COMPILE_OPTIONS_PRIVATE}
                                                             ${TESSERACT_COMPILE_OPTIONS_PUBLIC})
target_compile_definitions(${PROJECT_NAME}_resource_unit PRIVATE ${TESSERACT_COMPILE_DEFINITIONS})
target_clang_tidy(${PROJECT_NAME}_resource_unit ENABLE ${TESSERACT_ENABLE_CLANG_TIDY})
target_cxx_version(${PROJECT_NAME}_resource_unit PRIVATE VERSION ${TESSERACT_CXX_VERSION})
target_code_coverage(
  ${PROJECT_NAME}_resource_unit
  PRIVATE
  ALL
  EXCLUDE ${COVERAGE_EXCLUDE}
  ENABLE ${TESSERACT_ENABLE_CODE_COVERAGE})
add_gtest_discover_tests(${PROJECT_NAME}_resource_unit)
add_dependencies(${PROJECT_NAME}_resource_unit ${PROJECT_NAME})
add_dependencies(run_tests ${PROJECT_NAME}_resource_unit)

add_executable(${PROJECT_NAME}_utils_unit utils_unit.cpp)
target_link_libraries(${PROJECT_NAME}_utils_unit PRIVATE GTest::GTest GTest::Main ${PROJECT_NAME})
target_compile_options(${PROJECT_NAME}_utils_unit PRIVATE ${TESSERACT_COMPILE_OPTIONS_PRIVATE}
                                                          ${TESSERACT_COMPILE_OPTIONS_PUBLIC})
target_clang_tidy(${PROJECT_NAME}_utils_unit ENABLE ${TESSERACT_ENABLE_CLANG_TIDY})
target_cxx_version(${PROJECT_NAME}_utils_unit PRIVATE VERSION ${TESSERACT_CXX_VERSION})
target_code_coverage(
  ${PROJECT_NAME}_utils_unit
  PRIVATE
  ALL
  EXCLUDE ${COVERAGE_EXCLUDE}
  ENABLE ${TESSERACT_ENABLE_CODE_COVERAGE})
add_gtest_discover_tests(${PROJECT_NAME}_utils_unit)
add_dependencies(${PROJECT_NAME}_utils_unit ${PROJECT_NAME})
add_dependencies(run_tests ${PROJECT_NAME}_utils_unit)
