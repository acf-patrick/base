file(GLOB_RECURSE SOURCES "*.cpp" "*.h")

add_library(entix STATIC ${SOURCES})

target_include_directories(entix PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/tileson/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/box2d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/yaml-cpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/termcolor
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
    ${SDL2_GFX_INCLUDE_DIRS}
)

target_link_libraries(entix PUBLIC
    yaml-cpp
    SDL2_gfx
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
    SDL2_mixer::SDL2_mixer
)

if(USE_PHYSIC_SYSTEM)
    target_link_libraries(entix PUBLIC box2d)
    target_compile_definitions(entix PUBLIC USE_PHYSIC_SYSTEM=1)
endif()