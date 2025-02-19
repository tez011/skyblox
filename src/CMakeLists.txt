cmake_minimum_required(VERSION 3.21)

################ Current Module ################

add_executable(game
    main.c
    ingame/constructor.c
    ingame/input_events.c
    ingame/logic.c
    render/init.c
    render/light.c
    render/nuklear.c
    render/render.c
    util/hashtable.c
    util/physfs.c
    util/queue.c
    util/rbtree.c
    util/shader.c
    util/threadpool.c
    world/generate.c
    world/render.c
    world/resources.c
    world/storage.c)
target_include_directories(game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(game
    cglm
    cjson
    gl3w
    nuklear
    PhysFS::PhysFS
    SDL2::SDL2
    SDL2::SDL2main
    stb
    tinycthread)

if(MSVC)
    target_compile_options(game PUBLIC "/EHsc" "/GR-")
else(MSVC)
    target_compile_options(game PUBLIC -Wall)
    target_compile_options(game PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
endif(MSVC)