# Configure the GUI application
# wrap ui files to c++ header files
qt5_wrap_ui(SophireadDisplayUI
    ui/mainwindow.ui
)

# set include directories
include_directories(
    include
    ${PROJECT_SOURCE_DIR}/SophireadLib/include
    ${CMAKE_CURRENT_BINARY_DIR}
    $ENV{CONDA_PREFIX}/include
)

link_directories(
    $ENV{CONDA_PREFIX}/lib
)

#
set(SRC_FILES
    src/sophiread_display.cpp
    include/mainwindow.h
    src/mainwindow.cpp
    ${SophireadDisplayUI}
)

# add executable
add_executable(SophireadGUI
    ${SRC_FILES}
)
set_target_properties(SophireadGUI PROPERTIES VERSION ${PROJECT_VERSION})
target_link_libraries(SophireadGUI SophireadLib Qt5::Widgets qwt hdf5 hdf5_cpp OpenMP::OpenMP_CXX)

# symlink the executable to the build directory
add_custom_command(TARGET SophireadGUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/SophireadGUI/SophireadGUI
    ${PROJECT_BINARY_DIR}/SophireadGUI.app
)

# ----------------- INSTALL ----------------- #
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Default install prefix" FORCE)
endif()
install(TARGETS SophireadGUI
        RUNTIME DESTINATION bin)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
include(CPack)
