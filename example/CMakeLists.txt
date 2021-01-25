project(zmqBufExamples)
find_package(cppzmq)

# Find Protobuf installation
# Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
set(CMAKE_CXX_STANDARD 17)
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
if(CMAKE_CROSSCOMPILING)
  find_program(_PROTOBUF_PROTOC protoc)
else()
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()

# Proto file
get_filename_component(hw_proto "./msg.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

# Generated sources
set(proto_output "${CMAKE_CURRENT_SOURCE_DIR}/proto/")
set(hw_proto_srcs "${proto_output}/msg.pb.cc")
set(hw_proto_hdrs "${proto_output}/msg.pb.h")
add_custom_command(
    OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --cpp_out "${proto_output}"
      -I "${hw_proto_path}"
      "${hw_proto}"
    DEPENDS "${hw_proto}")


add_executable(responder "responder.cpp"  ${hw_proto_srcs})
target_link_libraries(responder ${_PROTOBUF_LIBPROTOBUF} cppzmq)

add_executable(requester "requester.cpp"  ${hw_proto_srcs})
target_link_libraries(requester ${_PROTOBUF_LIBPROTOBUF} cppzmq)

add_executable(publisher "publisher.cpp"  ${hw_proto_srcs})
target_link_libraries(publisher ${_PROTOBUF_LIBPROTOBUF} cppzmq)

add_executable(subscriber "subscriber.cpp"  ${hw_proto_srcs})
target_link_libraries(subscriber ${_PROTOBUF_LIBPROTOBUF} cppzmq)
