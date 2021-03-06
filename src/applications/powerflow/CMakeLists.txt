#
#     Copyright (c) 2013 Battelle Memorial Institute
#     Licensed under modified BSD License. A copy of this license can be
#     found
#     in the LICENSE file in the top level directory of this distribution.
#
# -*- mode: cmake -*-
# -------------------------------------------------------------
# file: CMakeLists.txt
# -------------------------------------------------------------
# -------------------------------------------------------------
# Created May  6, 2013 by William A. Perkins
# Last Change: 2014-01-28 13:13:59 d3g096
# -------------------------------------------------------------

set(target_libraries
    gridpack_components
    gridpack_factory
    gridpack_partition
    gridpack_parallel
    gridpack_math
    gridpack_configuration
    gridpack_timer
    gridpack_pfmatrix_components
    gridpack_ymatrix_components
    ${PARMETIS_LIBRARY} ${METIS_LIBRARY} 
    ${Boost_LIBRARIES}
    ${GA_LIBRARIES}
    ${MPI_CXX_LIBRARIES}
    ${PETSC_LIBRARIES})

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
if (GA_FOUND)
  include_directories(AFTER ${GA_INCLUDE_DIRS})
endif()
if (PARMETIS_FOUND)
  include_directories(BEFORE ${PARMETIS_INCLUDE_DIR})
endif()

add_library(powerflow_app
  pf_factory.cpp
)

add_executable(powerflow.x
   pf_app.cpp
   pf_factory.cpp
   pf_main.cpp
)

target_link_libraries(powerflow.x ${target_libraries})

add_executable(powerflow2
  pf_app2.cpp
  pf_factory.cpp
)
target_link_libraries(powerflow2 ${target_libraries})

# Put files necessary to run powerflow.x in binary directory.
# gridpack.petscrc is temporary -- it will be incorporated into
# input.xml

add_custom_target(powerflow.x.input
 
  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/input.xml
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14.raw
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/gridpack.petscrc
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/118.xml
  ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND ${CMAKE_COMMAND} -E copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/118.raw
  ${CMAKE_CURRENT_BINARY_DIR}

  DEPENDS 
  ${CMAKE_CURRENT_SOURCE_DIR}/input.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14.raw
  ${CMAKE_CURRENT_SOURCE_DIR}/gridpack.petscrc
  ${CMAKE_CURRENT_SOURCE_DIR}/118.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/118.raw

)
add_dependencies(powerflow.x powerflow.x.input)
add_dependencies(powerflow2 powerflow.x.input)

# -------------------------------------------------------------
# TEST: pf_network_test
# -------------------------------------------------------------
add_executable(pf_network_test test/pf_network_test.cpp)
target_link_libraries(pf_network_test powerflow_app ${target_libraries})
gridpack_add_unit_test(pf_network_test pf_network_test)

# -------------------------------------------------------------
# install as a sample application
# -------------------------------------------------------------

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.install.in
  ${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt @ONLY)

install(FILES 
  ${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/pf_app.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/pf_factory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/pf_main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/pf_app2.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/pf_app2.hpp  
  ${CMAKE_CURRENT_SOURCE_DIR}/pf_app.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/pf_factory.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/input.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/IEEE14.raw
  ${CMAKE_CURRENT_SOURCE_DIR}/gridpack.petscrc
  ${CMAKE_CURRENT_SOURCE_DIR}/118.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/118.raw
  ${CMAKE_CURRENT_SOURCE_DIR}/pti08.xml
  DESTINATION doc/gridpack/example/powerflow
)
