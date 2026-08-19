add_executable(TimeOverlaySmokeTest smoke_test.cpp)

target_link_libraries(TimeOverlaySmokeTest PRIVATE timeoverlay_core)

# Application console : sans cela, MinGW produirait un binaire sans sortie
# standard visible sous Windows a cause de CMAKE_WIN32_EXECUTABLE.
set_target_properties(TimeOverlaySmokeTest PROPERTIES WIN32_EXECUTABLE FALSE)

add_test(NAME smoke COMMAND TimeOverlaySmokeTest)
