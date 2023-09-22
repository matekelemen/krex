project(UtilityApplication CXX)

file(GLOB sources "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(UtilityCore SHARED ${sources})
target_include_directories(UtilityCore PUBLIC
                           "${KRATOS_SOURCE_DIR}/kratos"
                           "${CMAKE_CURRENT_SOURCE_DIR}/include"
                           "${KRATOS_SOURCE_DIR}/applications/HDF5Application"
                           "${KRATOS_SOURCE_DIR}/applications/MedApplication"
                           "${KRATOS_SOURCE_DIR}/applications/StructuralMechanicsApplication")
target_link_libraries(UtilityCore PUBLIC
                      KratosCore
                      KratosMedCore
                      KratosHDF5Core
                      KratosStructuralMechanicsCore)

file(GLOB drivers "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.cpp")
foreach(driver ${drivers})
    get_filename_component(driver_name "${driver}" NAME_WE)
    add_executable(${driver_name} "${driver}")
    target_link_libraries(${driver_name} PRIVATE UtilityCore)
endforeach()
