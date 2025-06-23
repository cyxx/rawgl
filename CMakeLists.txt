cmake_minimum_required(VERSION 3.0)
project("rawgl")
add_executable(rawgl2
    aifcplayer.cpp     engine.h           graphics_soft.cpp  pak.cpp            resource_3do.h     resource_win31.cpp screenshot.h       staticres.cpp      util.cpp
    aifcplayer.h       file.cpp           intern.h           pak.h              resource_mac.cpp   resource_win31.h   script.cpp         systemstub.h       util.h
    bitmap.cpp         file.h             main.cpp           resource.cpp       resource_mac.h     scaler.cpp         script.h           systemstub_sdl.cpp video.cpp
    bitmap.h           graphics.h         mixer.cpp          resource.h         resource_nth.cpp   scaler.h           sfxplayer.cpp      unpack.cpp         video.h
    engine.cpp         graphics_gl.cpp    mixer.h            resource_3do.cpp   resource_nth.h     screenshot.cpp     sfxplayer.h        unpack.h
)

target_compile_options(rawgl2 PRIVATE -g -O -MMD -Wall -Wpedantic -DBYPASS_PROTECTION -DUSE_GL)
set_property(TARGET rawgl2 PROPERTY CXX_STANDARD 11)

include(FindPkgConfig)

pkg_search_module(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_search_module(SDL2MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)

find_library(GL OpenGL)
find_library(ZLIB z)

target_include_directories(rawgl2 PUBLIC PkgConfig::SDL2)
target_link_libraries(rawgl2 PkgConfig::SDL2 PkgConfig::SDL2MIXER ${GL} ${ZLIB})
