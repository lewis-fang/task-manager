cmake_minimum_required(VERSION 3.16)
project(KSAT VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Sql)

set(SOURCES
    src/main.cpp
    src/database.cpp
    src/dialogs.cpp
    src/taskcard.cpp
    src/mainwindow.cpp
    resources.qrc
)

set(HEADERS
    src/models.h
    src/database.h
    src/dialogs.h
    src/taskcard.h
    src/mainwindow.h
)

add_executable(KSAT ${SOURCES} ${HEADERS})
target_link_libraries(KSAT PRIVATE Qt5::Widgets Qt5::Sql)

# ---- install (Linux) -------------------------------------------------------
install(TARGETS KSAT RUNTIME DESTINATION bin)

# ---- tests ----------------------------------------------------------------
option(KSAT_BUILD_TESTS "Build data-layer tests" ON)
if(KSAT_BUILD_TESTS)
    enable_testing()
    add_executable(test_db tests/test_db.cpp src/database.cpp)
    target_include_directories(test_db PRIVATE src)
    target_link_libraries(test_db PRIVATE Qt5::Sql Qt5::Core Qt5::Gui)
    add_test(NAME test_db COMMAND test_db)

    add_executable(ui_screenshot tests/ui_screenshot.cpp
                   src/database.cpp src/dialogs.cpp src/taskcard.cpp src/mainwindow.cpp)
    target_include_directories(ui_screenshot PRIVATE src)
    target_link_libraries(ui_screenshot PRIVATE Qt5::Widgets Qt5::Sql)

    add_executable(test_ui tests/test_ui.cpp
                   src/database.cpp src/dialogs.cpp src/taskcard.cpp src/mainwindow.cpp)
    target_include_directories(test_ui PRIVATE src)
    target_link_libraries(test_ui PRIVATE Qt5::Widgets Qt5::Sql)

    add_executable(test_login tests/test_login.cpp src/dialogs.cpp src/database.cpp)

    add_executable(test_gate tests/test_gate.cpp src/dialogs.cpp src/database.cpp)
    target_include_directories(test_gate PRIVATE src)
    target_link_libraries(test_gate PRIVATE Qt5::Widgets Qt5::Sql)
    target_include_directories(test_login PRIVATE src)
    target_link_libraries(test_login PRIVATE Qt5::Widgets Qt5::Sql)
endif()
