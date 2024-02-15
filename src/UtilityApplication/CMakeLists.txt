project(Utility CXX)

set(application_core_name "${PROJECT_NAME}Core")
set(application_name "${PROJECT_NAME}Application")
set(module_name Kratos${application_name})

# Define core C++ library
file(GLOB sources "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(${application_core_name} SHARED ${sources})
target_include_directories(${application_core_name} PUBLIC
                           "${KRATOS_SOURCE_DIR}/kratos"
                           "${CMAKE_CURRENT_SOURCE_DIR}/include"
                           "${KRATOS_SOURCE_DIR}/applications/HDF5Application"
                           "${KRATOS_SOURCE_DIR}/applications/MedApplication"
                           "${KRATOS_SOURCE_DIR}/applications/LinearSolversApplication"
                           "${KRATOS_SOURCE_DIR}/applications/StructuralMechanicsApplication")
target_link_libraries(${application_core_name} PUBLIC
                      KratosCore
                      KratosMedCore
                      KratosHDF5Core
                      KratosLinearSolversCore
                      KratosStructuralMechanicsCore)

set_target_properties(${application_core_name} PROPERTIES COMPILE_DEFINITIONS "UTILITY_APPLICATION=EXPORT,API")
install(TARGETS ${application_core_name} DESTINATION libs)

# Define python bindings
file(GLOB bindings "${CMAKE_CURRENT_SOURCE_DIR}/bindings/*.cpp")
pybind11_add_module(${module_name} MODULE THIN_LTO ${bindings})
target_link_libraries(${module_name} PRIVATE ${application_core_name})
target_include_directories(${module_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(${module_name} PROPERTIES PREFIX "")

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set_target_properties(${module_name} PROPERTIES SUFFIX .pyd) # Suffix on Windows
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(${module_name} PROPERTIES SUFFIX .so) # Suffix on MacOS
endif()

kratos_python_install_directory(${INSTALL_PYTHON_USING_LINKS}
                                "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
                                KratosMultiphysics/${application_name})

if (${INSTALL_TESTING_FILES} MATCHES ON)
    kratos_python_install_directory(${INSTALL_PYTHON_USING_LINKS}
                                    "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                                    KratosMultiphysics/${application_name}/tests)
endif()

install(TARGETS ${module_name} DESTINATION libs)

# Define executables
file(GLOB drivers "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.cpp")
foreach(driver ${drivers})
    get_filename_component(driver_name "${driver}" NAME_WE)
    add_executable(${driver_name} "${driver}")
    target_link_libraries(${driver_name} PRIVATE ${application_core_name})
endforeach()
