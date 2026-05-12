cmake_minimum_required(VERSION 3.16)
project(QuakeCore C)

set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(QuakeCore
        src/cl_demo.c
        src/cl_input.c
        src/cl_main.c
        src/cl_parse.c
        src/cl_tent.c
        src/cmd.c
        src/common.c
        src/console.c
        src/crc.c
        src/cvar.c
        src/host.c
        src/host_cmd.c
        src/keys.c
        src/mathlib.c
        src/menu.c
        src/net_bsd.c
        src/net_dgrm.c
        src/net_loop.c
        src/net_main.c
        src/net_udp.c
        src/net_vcr.c
        src/pr_cmds.c
        src/pr_edict.c
        src/pr_exec.c
        src/r_part.c
        src/sbar.c
        src/sv_main.c
        src/sv_move.c
        src/sv_phys.c
        src/sv_user.c
        src/view.c
        src/wad.c
        src/world.c
        src/zone.c
        src/cd_linux.c
        src/chase.c
        src/snd_dma.c
        src/snd_mem.c
        src/snd_mix.c
        src/snd_sdl.c
        src/gl_draw.c
        src/gl_mesh.c
        src/gl_model.c
        src/gl_refrag.c
        src/gl_rlight.c
        src/gl_rmain.c
        src/gl_rmisc.c
        src/gl_rsurf.c
        src/gl_screen.c
        src/gl_warp.c
        src/gl_vidsdl.c
        src/sys_linux.c)

target_compile_definitions(QuakeCore PRIVATE
    GLQUAKE
    _GNU_SOURCE
)

target_compile_options(QuakeCore PRIVATE
    -Wno-implicit-function-declaration
    -Wno-implicit-int
    -Wno-int-conversion
    -Wno-incompatible-pointer-types
)

set_source_files_properties(net_udp.c PROPERTIES
    COMPILE_FLAGS "-w"
)

target_include_directories(QuakeCore PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

target_link_libraries(QuakeCore PRIVATE
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    m
    dl
)

#set_target_properties(QuakeCore PROPERTIES
 #   RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
#)
