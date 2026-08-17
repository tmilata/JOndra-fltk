cmake_minimum_required(VERSION 2.8.2)

project(JOndra C CXX)

# Volitelny plne staticky Linux build (urceny hlavne pro Alpine/musl).
option(JONDRA_STATIC "Build fully static Linux executable" OFF)
set(JONDRA_STATIC_PREFIX "/opt/jondra-static" CACHE PATH
    "Prefix containing static FLTK and ALSA")

if(WIN32)
    message(FATAL_ERROR
        "Tento CMake build je urcen pro Linux. Pro Windows 98/Visual C++ 6 "
        "pouzij Jondra.dsw/Jondra.dsp a FLTK 1.1.10.")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# JOndra je zamerne psana pro stary C++ (VC6 / GCC 4.x).
# CMAKE_CXX_STANDARD ve starem CMake 2.8.2 neexistuje, proto nastavujeme
# prepinac kompilatoru klasicky.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++98")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # V Debug buildu nechame varovani zapnuta pro vyvoj a hledani problemu.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -Wall -Wextra")
else()
    # Release build ma byt cisty i na starem GCC 4.4 z Debianu Squeeze.
    # -w potlaci pouze diagnostiku typu warning; chyby kompilace zustavaji.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -w")
    add_definitions(-DNDEBUG)
endif()

# fltk-config funguje jak s FLTK 1.1.x (Debian Squeeze), tak s novejsim FLTK.
# Pro staticky build pouzijeme FLTK sestavene zvlast v JONDRA_STATIC_PREFIX.
if(JONDRA_STATIC)
    set(FLTK_CONFIG_EXECUTABLE "${JONDRA_STATIC_PREFIX}/bin/fltk-config")
    if(NOT EXISTS "${FLTK_CONFIG_EXECUTABLE}")
        message(FATAL_ERROR "Nenalezen staticky FLTK: ${FLTK_CONFIG_EXECUTABLE}")
    endif()
else()
    find_program(FLTK_CONFIG_EXECUTABLE NAMES fltk-config)
    if(NOT FLTK_CONFIG_EXECUTABLE)
        message(FATAL_ERROR "Nenalezen fltk-config. Nainstaluj vyvojovy balicek FLTK (na Squeeze libfltk1.1-dev).")
    endif()
endif()

execute_process(
    COMMAND ${FLTK_CONFIG_EXECUTABLE} --use-images --cxxflags
    RESULT_VARIABLE FLTK_CXX_RESULT
    OUTPUT_VARIABLE FLTK_CXXFLAGS_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT FLTK_CXX_RESULT EQUAL 0)
    message(FATAL_ERROR "fltk-config --use-images --cxxflags selhal")
endif()

if(JONDRA_STATIC)
    set(FLTK_LINK_SWITCH --ldstaticflags)
else()
    set(FLTK_LINK_SWITCH --ldflags)
endif()

execute_process(
    COMMAND ${FLTK_CONFIG_EXECUTABLE} --use-images ${FLTK_LINK_SWITCH}
    RESULT_VARIABLE FLTK_LD_RESULT
    OUTPUT_VARIABLE FLTK_LDFLAGS_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT FLTK_LD_RESULT EQUAL 0)
    message(FATAL_ERROR "fltk-config --use-images ${FLTK_LINK_SWITCH} selhal")
endif()

# CMake 2.8.2 nema moderni target_compile_options().
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLTK_CXXFLAGS_RAW}")

# Stara jednoparametrova forma separate_arguments() je kompatibilni s CMake 2.8.2.
set(FLTK_LDFLAGS ${FLTK_LDFLAGS_RAW})
separate_arguments(FLTK_LDFLAGS)

if(JONDRA_STATIC)
    set(ALSA_INCLUDE_DIRS "${JONDRA_STATIC_PREFIX}/include")
    set(ALSA_LIBRARIES "${JONDRA_STATIC_PREFIX}/lib/libasound.a")
    if(NOT EXISTS "${ALSA_LIBRARIES}")
        message(FATAL_ERROR "Nenalezena staticka ALSA: ${ALSA_LIBRARIES}")
    endif()

    # fltk-config vypise prime zavislosti, ale pri statickem linkovani je
    # potreba i jejich tranzitivni zavislost (xcb, Xau, Xdmcp, freetype,
    # expat, zlib, bzip2...). Tu nechej zjistit pkg-config z konkretniho
    # Alpine systemu, na kterem se binarka sestavuje.
    find_program(PKG_CONFIG_EXECUTABLE NAMES pkg-config)
    if(NOT PKG_CONFIG_EXECUTABLE)
        message(FATAL_ERROR "Pro staticky build je potreba pkg-config")
    endif()
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --static --libs
                xrender xfixes xext xft fontconfig xinerama x11
        RESULT_VARIABLE STATIC_DEPS_RESULT
        OUTPUT_VARIABLE STATIC_DEPS_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT STATIC_DEPS_RESULT EQUAL 0)
        message(FATAL_ERROR "pkg-config --static pro X11/Fontconfig zavislosti selhal")
    endif()
    set(STATIC_DEPS_LDFLAGS ${STATIC_DEPS_RAW})
    separate_arguments(STATIC_DEPS_LDFLAGS)
else()
    find_package(ALSA REQUIRED)
endif()

find_package(Threads REQUIRED)

# Stary FindALSA mohl poskytovat ALSA_INCLUDE_DIR namisto ALSA_INCLUDE_DIRS.
include_directories(${ALSA_INCLUDE_DIRS} ${ALSA_INCLUDE_DIR})

# -----------------------------------------------------------------------------
# Embedded resources
# -----------------------------------------------------------------------------
# Na Windows se stejny ResPacker.cpp sestavuje jako samostatny projekt ve VC6.
# Na Linuxu jej CMake sestavi jako prvni a preda mu vstupni a vystupni cestu
# explicitne, takze samotny packer nezavisi na rozlozeni adresaru platformy.
add_executable(ResPacker ResPacker.cpp)
set_target_properties(ResPacker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(JONDRA_ASSET_DIR "${CMAKE_SOURCE_DIR}/Debug")
else()
    set(JONDRA_ASSET_DIR "${CMAKE_SOURCE_DIR}/Release")
endif()

foreach(asset_dir images roms sound)
    if(NOT IS_DIRECTORY "${JONDRA_ASSET_DIR}/${asset_dir}")
        message(FATAL_ERROR "Chybi zdrojovy resource adresar: ${JONDRA_ASSET_DIR}/${asset_dir}")
    endif()
endforeach()
if(NOT EXISTS "${JONDRA_ASSET_DIR}/Jondra.config")
    message(FATAL_ERROR "Chybi konfigurace: ${JONDRA_ASSET_DIR}/Jondra.config")
endif()

file(GLOB_RECURSE JONDRA_RESOURCE_INPUTS
    "${JONDRA_ASSET_DIR}/images/*"
    "${JONDRA_ASSET_DIR}/roms/*"
    "${JONDRA_ASSET_DIR}/sound/*"
)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
set(EMBEDDED_RESOURCES_DATA
    "${CMAKE_BINARY_DIR}/generated/EmbeddedResourcesData.cpp")
set_source_files_properties(${EMBEDDED_RESOURCES_DATA} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${EMBEDDED_RESOURCES_DATA}
    COMMAND "${CMAKE_BINARY_DIR}/tools/ResPacker"
            "${JONDRA_ASSET_DIR}"
            "${EMBEDDED_RESOURCES_DATA}"
    DEPENDS ResPacker ${JONDRA_RESOURCE_INPUTS} "${CMAKE_SOURCE_DIR}/ResPacker.cpp"
    COMMENT "Generuji vestavene resources JOndry"
)

# EmbeddedResourcesData.cpp lezi v build adresari a includuje hlavicku ze source.
include_directories("${CMAKE_SOURCE_DIR}")

set(JONDRA_SOURCES
    BinOpen.cpp
    Clock.cpp
    Config.cpp
    Debug.cpp
    DebuggerWindow.cpp
    DebuggerWindow.h
    DirtyTiles.cpp
    EmbeddedResources.cpp
    EmbeddedResources.h
    ${EMBEDDED_RESOURCES_DATA}
    FlatButton.cpp
    HexInput.cpp
    Jondra.cpp
    Keyboard.cpp
    MTimer.cpp
    Memory.cpp
    MemoryTimeline.cpp
    MemoryTimeline.h
    Melodik.cpp
    Melodik.h
    Ondra.cpp
    Screen.cpp
    Settings.cpp
    Sound.cpp
    SoundBuffer.cpp
    SoundSample.cpp
    TapFile.cpp
    Tape.cpp
    TapeSignalProc.cpp
    cpuintf.cpp
    z80disassembler.cpp
    z80emu.cpp
)

add_executable(Jondra ${JONDRA_SOURCES})
add_dependencies(Jondra ResPacker)
set_target_properties(Jondra PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(JONDRA_STATIC)
    # -static se tyka jen finalni JOndry. ResPacker muze zustat dynamicky.
    set_target_properties(Jondra PROPERTIES LINK_FLAGS "-static")

    # U statickych archivu zalezi na poradi. --start-group/--end-group
    # dovoli GNU linkeru projit archivy opakovane a vyresit kruhove zavislosti.
    target_link_libraries(Jondra
        "-Wl,--start-group"
        ${FLTK_LDFLAGS}
        ${ALSA_LIBRARIES}
        ${STATIC_DEPS_LDFLAGS}
        m pthread rt
        "-Wl,--end-group"
    )
else()
    target_link_libraries(Jondra
        ${FLTK_LDFLAGS}
        ${ALSA_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
    )
endif()

# Images/ROM/sound jsou od teto chvile soucasti executable. Vedle Jondra
# zustava pouze uzivatelsky editovatelny konfiguracni soubor.
# Nepouzivame $<TARGET_FILE_DIR:...>, aby soubor fungoval i s CMake 2.8.2.
add_custom_command(TARGET Jondra POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${JONDRA_ASSET_DIR}/Jondra.config"
            "${CMAKE_BINARY_DIR}/bin/Jondra.config"
    COMMENT "Kopiruji Jondra.config"
)

install(TARGETS Jondra RUNTIME DESTINATION .)
install(FILES "${JONDRA_ASSET_DIR}/Jondra.config" DESTINATION .)

message(STATUS "JOndra build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "JOndra static build: ${JONDRA_STATIC}")
message(STATUS "JOndra resource source: ${JONDRA_ASSET_DIR}")
message(STATUS "JOndra embedded data: ${EMBEDDED_RESOURCES_DATA}")
message(STATUS "FLTK config: ${FLTK_CONFIG_EXECUTABLE}")
message(STATUS "FLTK cxxflags: ${FLTK_CXXFLAGS_RAW}")
message(STATUS "FLTK ldflags: ${FLTK_LDFLAGS_RAW}")
