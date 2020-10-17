############################################################################
# toolchain-raspberry.cmake
# Copyright (C) 2014  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

# Based on:
# https://github.com/Pro/raspi-toolchain/blob/master/Toolchain-rpi.cmake

# TODO build on WSL:
# rsync -vR --progress -rl --delete-after --safe-links pi@192.168.1.PI:/{lib,usr,opt/vc/lib} $HOME/rpi/sysroot (optional sysroot sync)
# export PATH=/opt/cross-pi-gcc/bin:$PATH
# cmake -G Ninja
#   -DRPI_SYSROOT_PATH=$RPI_SYSROOT
#   -DCMAKE_TOOLCHAIN_FILE=$GBA_PATH/cmake/RPi3CrossCompile.cmake ..

set(TOOLCHAIN_HOST "arm-linux-gnueabihf")
set(BUILDING_FOR_RPI3 TRUE)

message(STATUS "Using sysroot path: ${RPI_SYSROOT_PATH}")

set(TOOLCHAIN_CC "${TOOLCHAIN_HOST}-gcc")
set(TOOLCHAIN_CXX "${TOOLCHAIN_HOST}-g++")
set(TOOLCHAIN_LD "${TOOLCHAIN_HOST}-ld")
set(TOOLCHAIN_AR "${TOOLCHAIN_HOST}-ar")
set(TOOLCHAIN_RANLIB "${TOOLCHAIN_HOST}-ranlib")
set(TOOLCHAIN_STRIP "${TOOLCHAIN_HOST}-strip")
set(TOOLCHAIN_NM "${TOOLCHAIN_HOST}-nm")

set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SYSROOT "${RPI_SYSROOT_PATH}")

# Define name of the target system
set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SYSTEM_PROCESSOR "armv7")

# Define the compiler
set(CMAKE_C_COMPILER ${TOOLCHAIN_CC})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_CXX})

# List of library dirs where LD has to look. Pass them directly through gcc. LD_LIBRARY_PATH is not evaluated by arm-*-ld
set(LIB_DIRS
        "/opt/cross-pi-gcc/arm-linux-gnueabihf/lib"
        "/opt/cross-pi-gcc/lib"
        "${RPI_SYSROOT_PATH}/opt/vc/lib"
        "${RPI_SYSROOT_PATH}/lib/${TOOLCHAIN_HOST}"
        "${RPI_SYSROOT_PATH}/usr/local/lib"
        "${RPI_SYSROOT_PATH}/usr/lib/${TOOLCHAIN_HOST}"
        "${RPI_SYSROOT_PATH}/usr/lib"
        "${RPI_SYSROOT_PATH}/usr/lib/${TOOLCHAIN_HOST}/blas"
        "${RPI_SYSROOT_PATH}/usr/lib/${TOOLCHAIN_HOST}/lapack"
        "${RPI_SYSROOT_PATH}/usr/lib/${TOOLCHAIN_HOST}/pulseaudio")

# You can additionally check the linker paths if you add the flags ' -Xlinker --verbose'
set(COMMON_FLAGS "-I${RPI_SYSROOT_PATH}/usr/include")
foreach(LIB ${LIB_DIRS})
    set(COMMON_FLAGS "${COMMON_FLAGS} -L${LIB} -Wl,-rpath-link,${LIB}")
endforeach()

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${RPI_SYSROOT_PATH}/usr/lib/${TOOLCHAIN_HOST}")

set(CMAKE_C_FLAGS "-mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard ${COMMON_FLAGS}" CACHE STRING "Flags for RPi3")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags for RPi3")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_INSTALL_PREFIX};${CMAKE_PREFIX_PATH};${CMAKE_SYSROOT}")

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
