# 编译器和编译选项
CXX = g++
#DEBUG_FLAGS = -O0 -g
CXXFLAGS = -std=c++11 -Wall -I./src/include -I./example

# 链接库
LIBS = -lprotobuf -lzookeeper_mt -lpthread -lmuduo_net -lmuduo_base
CXXFLAGS += $(LIBS)

# 源文件目录
SRCDIRS = src
CALLER_SRCDIRS = example/caller
CALLEE_SRCDIRS = example/callee
PROTO_SRCDIRS = example

# 二进制输出目录
BINDIR = bin
TARGET_CLIENT = $(BINDIR)/client/client
TARGET_SERVER = $(BINDIR)/server/server

# 获取所有源文件
SRCS = $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cc))
CALLER_SRCS = $(foreach dir,$(CALLER_SRCDIRS),$(wildcard $(dir)/*.cc))
CALLEE_SRCS = $(foreach dir,$(CALLEE_SRCDIRS),$(wildcard $(dir)/*.cc))
PROTO_SRCS = $(wildcard $(PROTO_SRCDIRS)/*.pb.cc)

# 目标文件
SRCS_OBJS = $(patsubst %.cc, %.o, $(SRCS))
CALLER_OBJS = $(patsubst %.cc, %.o, $(CALLER_SRCS))
CALLEE_OBJS = $(patsubst %.cc, %.o, $(CALLEE_SRCS))
PROTO_OBJS = $(patsubst %.cc, %.o, $(PROTO_SRCS))

# 默认目标
all: $(TARGET_CLIENT) $(TARGET_SERVER)

# 编译规则：将源文件转化为目标文件
%.o: %.cc
	$(CXX) -c $< $(CXXFLAGS) -o $@

# 自动生成 protobuf 文件 注意这里只是生成了user.pb.cc 和 user.pb.h
PROTOC = protoc
PROTOC_FLAGS = --cpp_out=./example
%.pb.cc %.pb.h: %.proto
	$(PROTOC) $(PROTOC_FLAGS) $<

# 链接客户端可执行文件
$(TARGET_CLIENT): $(SRCS_OBJS) $(CALLER_OBJS) $(PROTO_OBJS)
	@mkdir -p $(BINDIR)/client
	$(CXX) $^ $(CXXFLAGS) -o $@

# 链接服务端可执行文件
$(TARGET_SERVER): $(SRCS_OBJS) $(CALLEE_OBJS) $(PROTO_OBJS)
	@mkdir -p $(BINDIR)/server
	$(CXX) $^ $(CXXFLAGS) -o $@

# 清理生成文件
clean:
	@rm -rf $(SRCS_OBJS) $(CALLER_OBJS) $(CALLEE_OBJS) $(PROTO_OBJS) $(BINDIR)

# 伪目标
.PHONY: all clean
