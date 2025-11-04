cmake_minimum_required(VERSION 3.13)
# set(CMAKE_TOOLCHAIN_FILE "D:/Applications/vcpkg-vcc/scripts/buildsystems/vcpkg.cmake")

project(sdrv1_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})


if (MSVC)
    # Lib path
    target_link_directories(sdrv1_source PRIVATE "C:/Program Files/IIO/lib/")
    target_include_directories(sdrv1_source PRIVATE "C:/Program Files/IIO/include/")

    target_link_libraries(sdrv1_source PRIVATE libiio)
elseif (ANDROID)
    target_include_directories(sdrv1_source PUBLIC
        /sdr-kit/${ANDROID_ABI}/include
    )

    target_link_libraries(sdrv1_source PUBLIC
        /sdr-kit/${ANDROID_ABI}/lib/libxml2.so
        /sdr-kit/${ANDROID_ABI}/lib/libiio.so
    )
else (MSVC)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_include_directories(sdrv1_source PRIVATE "/Library/Frameworks/iio.framework/Headers")
        target_link_libraries(sdrv1_source PRIVATE "/Library/Frameworks/iio.framework/iio")
    else()
        find_package(PkgConfig)

        pkg_check_modules(LIBIIO REQUIRED libiio)

        target_include_directories(sdrv1_source PRIVATE ${LIBIIO_INCLUDE_DIRS})
        target_link_directories(sdrv1_source PRIVATE ${LIBIIO_LIBRARY_DIRS})
        target_link_libraries(sdrv1_source PRIVATE ${LIBIIO_LIBRARIES})
    endif()
endif ()