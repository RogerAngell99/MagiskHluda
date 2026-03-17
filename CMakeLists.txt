cmake_minimum_required(VERSION 3.16.3)
project(MagiskHluda)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(MagiskHluda
        main.cpp
        utils.cpp
)

# Set the target architecture for Visual Studio (Win64)
if (MSVC)
    set_target_properties(MagiskHluda PROPERTIES
            VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE x64
    )
endif ()
