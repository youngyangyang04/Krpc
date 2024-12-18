# 除了可以直接使用CMAKE_CURRENT_SOURCE_DIR的相对路径，也可以使用get_filename_component来获取当前目录的绝对路径
#这样就可以保证CMake 的某些操作可能需要使用绝对路径，尤其是跨目录的构建场景。绝对路径可以避免相对路径带来的混淆和潜在问题。
#动态调整目录结构：如果项目目录层次较深，或者使用子模块或外部依赖，绝对路径可以方便地定位正确的文件，而不用担心路径拼接或依赖 CMake 的当前工作目录。

#示例：
#get_filename_component(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
#file(GLOB_RECURSE SRC_FILES ${SRC_DIR}/*.cc)
#获取当前目录下的所有源文件
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

# 获取 protobuf 的生成文件
file(GLOB PROTO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.pb.cc)

#创建静态库或共享库
add_library(krpc_core STATIC ${SRC_FILES} ${PROTO_SRCS})

#链接protobuf库
target_link_libraries(krpc_core PUBLIC protobuf)

#设置头文件的路径
target_include_directories(krpc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 添加编译选项
target_compile_options(krpc_core PRIVATE -std=c++11 -Wall)