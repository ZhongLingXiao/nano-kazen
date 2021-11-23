cmake_minimum_required(VERSION 3.20)


## Set project name ##
project(kazen)


## Find packages under rez-env ##
find_package(Boost REQUIRED COMPONENTS filesystem system)
find_package(TBB REQUIRED)
find_package(OpenImageIO REQUIRED)
find_package(embree 3.13.0 REQUIRED) # Here should point a spesific version
find_package(fmt 7.1.3 REQUIRED) # Here fmt should match oiio's fmt version
find_package(pugixml 1.11 REQUIRED)


## Set platform-specific flags ##
if (WIN32)
  add_definitions(-D__WINDOWS__)
elseif(UNIX)
  if(APPLE)
    add_definitions(-D__OSX__)
  else()
    add_definitions(-D__LINUX__)
  endif()
endif()


## C++17 ##
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -Wextra")
include(CheckCXXCompilerFlag)
if (CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|Emscripten|Intel)$")
  CHECK_CXX_COMPILER_FLAG("-std=c++17" HAS_CPP17_FLAG)

  # set(CMAKE_CXX_FLAGS_DEBUG "-g")
  # set(CMAKE_CXX_FLAGS_RELEASE "-O1") # O1 is fastest in this case

  if (HAS_CPP17_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
  else()
    CHECK_CXX_COMPILER_FLAG("-std=c++1z" HAS_CPP1Z_FLAG)
    if (HAS_CPP1Z_FLAG)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")
    else()
      message(FATAL_ERROR "Unsupported compiler -- kazen requires C++17 support!")
    endif()
  endif()
elseif(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
endif()


## Add executable ##
add_executable(kazen 
    # headers
    include/kazen/accel.h
    include/kazen/bbox.h
    include/kazen/bitmap.h
    include/kazen/block.h
    include/kazen/bsdf.h
    include/kazen/camera.h
    include/kazen/color.h
    include/kazen/common.h
    include/kazen/define.h
    include/kazen/dpdf.h
    include/kazen/frame.h
    include/kazen/integrator.h
    include/kazen/light.h
    include/kazen/mesh.h
    include/kazen/object.h
    include/kazen/parser.h
    include/kazen/pcg32.h
    include/kazen/progress.h
    include/kazen/proplist.h
    include/kazen/ray.h
    include/kazen/renderer.h
    include/kazen/rfilter.h
    include/kazen/sampler.h
    include/kazen/scene.h
    include/kazen/timer.h
    include/kazen/transform.h
    include/kazen/vector.h
    include/kazen/warp.h

    # source code
    src/kazen/accel.cpp
    src/kazen/bitmap.cpp
    src/kazen/block.cpp
    src/kazen/bsdf.cpp
    src/kazen/camera.cpp
    src/kazen/common.cpp
    src/kazen/integrator.cpp
    src/kazen/light.cpp
    src/kazen/mesh.cpp
    src/kazen/object.cpp
    src/kazen/parser.cpp
    src/kazen/progress.cpp
    src/kazen/proplist.cpp
    src/kazen/renderer.cpp
    src/kazen/rfilter.cpp
    src/kazen/sampler.cpp
    src/kazen/scene.cpp
    src/kazen/warp.cpp

    # experimental code

    # tests

    # main.cpp
    src/kazen/main.cpp
)


## Header only ext ##
target_include_directories(kazen PUBLIC
    ## kazen include files ##
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    
    ## eigen3 ##
    $ENV{REZ_EIGEN_ROOT}
    
    ## tinyobjloader ##
    $ENV{REZ_TINYOBJLOADER_ROOT}    

    ## tbb ##
    $ENV{REZ_TBB_ROOT}/include

    ## filesystem ##
    $ENV{REZ_FILESYSTEM_ROOT}

)


## Link against target ##
target_link_libraries(kazen PUBLIC
    # Boost::filesystem
    TBB::tbb
    OpenImageIO::OpenImageIO
    embree
    fmt::fmt
    pugixml::pugixml
)


## Compile features ##
target_compile_features(kazen PUBLIC cxx_std_17)