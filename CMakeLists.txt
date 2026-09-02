cmake_minimum_required(VERSION 3.16)
project(velora VERSION 0.6.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.8 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)
add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC glfw)
find_package(OpenGL REQUIRED)
target_link_libraries(imgui_lib PUBLIC OpenGL::GL)
add_executable(velora
  src/main.cpp
  src/core/Kernel.cpp
  src/core/Access.cpp
  src/fs/VelFS.cpp
  src/gfx/Graphics.cpp
  src/system/Sandbox.cpp
  src/system/Session.cpp
  src/system/Notifications.cpp
  src/system/PythonRuntime.cpp
  src/apps/ClorEngine.cpp
  src/apps/EmbeddedBrowser.cpp
  src/apps/Lattice.cpp
  src/apps/Nucleus.cpp
  src/ui/Assets.cpp
  src/ui/Package.cpp
  src/ui/IconCache.cpp
  src/ui/Shell.cpp
  src/ui/Cursor.cpp
  src/ui/WindowManager.cpp
)
target_include_directories(velora PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(velora PRIVATE imgui_lib glfw OpenGL::GL)
if(WIN32)
  target_link_libraries(velora PRIVATE gdi32 user32 shell32 opengl32 urlmon)
endif()
add_custom_command(TARGET velora POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:velora>/media
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:velora>/apps
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:velora>/ui
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/media $<TARGET_FILE_DIR:velora>/media
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/apps $<TARGET_FILE_DIR:velora>/apps
)
message(STATUS "VeloraOS ${PROJECT_VERSION} — OpenGL + Vulkan detect, TaskMgr, Clor")


# Optional WebView2 for in-OS Clor browser (Edge Chromium)
set(WEBVIEW2_SDK "" CACHE PATH "Path to Microsoft.Web.WebView2 package root")
if(WEBVIEW2_SDK AND EXISTS "${WEBVIEW2_SDK}/build/native/include/WebView2.h")
  message(STATUS "WebView2 enabled: ${WEBVIEW2_SDK}")
  target_compile_definitions(velora PRIVATE VELORA_HAS_WEBVIEW2=1)
  target_include_directories(velora PRIVATE "${WEBVIEW2_SDK}/build/native/include")
  if(EXISTS "${WEBVIEW2_SDK}/build/native/x64/WebView2Loader.dll.lib")
    target_link_libraries(velora PRIVATE "${WEBVIEW2_SDK}/build/native/x64/WebView2Loader.dll.lib")
  elseif(EXISTS "${WEBVIEW2_SDK}/build/native/x64/WebView2LoaderStatic.lib")
    target_link_libraries(velora PRIVATE "${WEBVIEW2_SDK}/build/native/x64/WebView2LoaderStatic.lib")
  endif()
  if(WIN32)
    target_link_libraries(velora PRIVATE ole32 oleaut32 uuid)
  endif()
  # Copy loader DLL next to exe
  if(EXISTS "${WEBVIEW2_SDK}/build/native/x64/WebView2Loader.dll")
    add_custom_command(TARGET velora POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WEBVIEW2_SDK}/build/native/x64/WebView2Loader.dll"
        $<TARGET_FILE_DIR:velora>)
  endif()
else()
  message(STATUS "WebView2 OFF — pass -DWEBVIEW2_SDK=... for in-window Clor browser")
endif()
