find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
add_executable(ocr_dialog_test
  ocr_dialog_test.cpp
  ../../src/tools/ocr/ocrdialog.cpp
  ../../src/tools/ocr/ocrdialog.h)
set_target_properties(ocr_dialog_test PROPERTIES AUTOMOC ON)
target_include_directories(ocr_dialog_test PRIVATE ../../src)
target_compile_definitions(ocr_dialog_test PRIVATE OCR_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ocr_dialog_test PRIVATE Qt6::Test Qt6::Widgets)
add_test(NAME ocr_dialog COMMAND ocr_dialog_test)
set_tests_properties(ocr_dialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
