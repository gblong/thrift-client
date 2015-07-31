#include <unistd.h>
#include <sys/time.h>

#include <stdio.h>
#include <iostream>

#include <thrift/client/TClient.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TTransport.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/transport/TConnection.h>
#include <thrift/TApplicationException.h>

#include "gen-cpp/Calculator.h"

using namespace std;
using namespace tutorial;

using apache::thrift::protocol::TProtocol;
using apache::thrift::protocol::TBinaryProtocol;
using apache::thrift::transport::TSocket;
using apache::thrift::transport::TTransport;
using apache::thrift::transport::TFramedTransport;
using apache::thrift::TException;

using net::thrift::client::TClient;
using net::thrift::client::Writer;
using net::thrift::client::Reader;

class Foo {
 public:
  Foo():sum(0) {}
  ~Foo(){}

  int add(int a) {
    sum += a;
    return sum;
  }

 private:
  int sum;
};

void add_done_cb(void* data, const void* res) {
  Foo* foo = reinterpret_cast<Foo*>(data);
  int sum = foo->add(*reinterpret_cast<const int*>(res));
  LOG(INFO) << "sum:" << sum << endl;
}
void add_error_cb(void* data, int32_t error) {
  LOG(INFO) << "error:" << error  << endl;
}

void string_done_cb(void* data, const void* res) {
  const string *str = reinterpret_cast<const string*>(res);
  LOG(INFO) << "ret:" << str->c_str() << endl;
}

void string_error_cb(void* data, int32_t error) {
  LOG(INFO) << "error:" << error  << endl;
}


int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);

  boost::shared_ptr<TTransport> socket(new TSocket("localhost", 9090));
  boost::shared_ptr<TTransport> transport(new TFramedTransport(socket));
  boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

  try {
    transport->open();

    //string getString()
    {
      TClient call("Calculator", "getString", string_done_cb, string_error_cb, NULL);
      Writer<Calculator_getString_pargs> param;
      call.write(protocol.get(), &param);
      string res;
      Reader<string, Calculator_getString_presult> reader(&res);
      call.read(protocol.get(), &reader);
    }

    //int32_t add(int32_t a, int32_t b)
    {
      Foo foo;
      TClient call("Calculator", "add", add_done_cb, add_error_cb, &foo);
      Writer<Calculator_add_pargs, const int32_t, const int32_t> param;
      param.set(1, 2);
      call.write(protocol.get(), &param);

      int32_t res;
      Reader<int32_t, Calculator_add_presult> reader(&res);
      call.read(protocol.get(), &reader);
    }

    transport->close();
  } catch (TException &tx) {
    printf("ERROR: %s\n", tx.what());
  }
}
