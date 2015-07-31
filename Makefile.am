AUTOMAKE_OPTIONS=foreign

VERSION_INFO = 0:1:0

ACLOCAL_AMFLAGS= -I m4

protoc_inputs = src/thrift/client/service.proto 
protoc_outputs = \
				 src/thrift/client/service.pb.h \
				 src/thrift/client/service.pb.cc
$(protoc_outputs): $(protoc_inputs)
	$(PROTOC) -I$(srcdir) --cpp_out=$(srcdir)  $^

BUILT_SOURCES = $(protoc_outputs)

lib_LTLIBRARIES=libthrift-client.la
libthrift_client_la_LDFLAGS = -version-info $(VERSION_INFO)
nodist_libthrift_client_la_SOURCES = $(protoc_outputs)
libthrift_client_la_SOURCES  = \
							 src/thrift/client/ServiceRegistry.cpp \
							 src/thrift/client/TClient.cpp \
							 src/thrift/client/TClientController.cpp \
							 src/thrift/transport/TConnection.cpp \
							 src/thrift/transport/TConnectionPool.cpp


include_thriftdir = $(includedir)/thrift

include_clientdir = $(include_thriftdir)/client
nodist_include_client_HEADERS = src/thrift/client/service.pb.h
include_client_HEADERS = \
						 src/thrift/client/ServiceRegistry.h \
						 src/thrift/client/TClient.h \
						 src/thrift/client/TClientController.h


include_transportdir = $(include_thriftdir)/transport
include_transport_HEADERS = \
							src/thrift/transport/TStringBufferTransport.h \
							src/thrift/transport/TConnection.h \
							src/thrift/transport/TConnectionPool.h


include_commondir = $(include_thriftdir)/common
include_common_HEADERS = src/thrift/common/ObjAllocator.h


EXTRA_DIST = \
			 src/thrift/client/service.proto \
			 packages/rpm.sh packages/rpm/rpm.spec \
			 examples/Makefile \
			 examples/shared.thrift \
			 examples/tutorial.thrift \
			 examples/client.cpp \
			 examples/controller.cpp

CLEANFILES = $(protoc_outputs) proto_middleman


AM_CXXFLAGS = -std=c++0x
INCLUDES = -I$(srcdir)/src

