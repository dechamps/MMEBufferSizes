cmake_minimum_required(VERSION 3.20)
project(MMEBufferSizes)

add_compile_options(/WX /W4 /permissive- /analyze /wd6553)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(MMEBufferSizes "MMEBufferSizes.c")
target_link_libraries(MMEBufferSizes
	PRIVATE winmm
)

install(TARGETS MMEBufferSizes RUNTIME)
