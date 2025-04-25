add_library(zmbridge SHARED
        zm_api_bridge.cpp
)

target_include_directories(zmbridge PRIVATE
        ${PROJECT_SOURCE_DIR}/src          # for core ZM headers
)

target_link_libraries(zmbridge PRIVATE zm)  # link against existing libzm.a

install(TARGETS zmbridge
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES zm_api_bridge.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zoneminder)
