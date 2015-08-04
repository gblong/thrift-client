# thrift-client
thrift asynchronous and non-blocking client based on libev



#Architecture

![image](https://github.com/gblong/thrift-client/raw/master/doc/thrift_client_state.jpg)


#Example
---------
#include <thrift/client/TClientController.h>

#include "gen-cpp/Calculator.h"

using namespace std;
using namespace tutorial;

using net::thrift::client::TClientController;

void add_done_cb(void* data, const void* res) {
  const int32_t ret = *reinterpret_cast<const int32_t*>(res);
  LOG(INFO) << "ret:" << ret << endl;
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

  TClientController controller("service.conf", NULL);

  for (int i = 0; i < 10; i++) {
    controller.call<RPC_TYPE(Calculator, int32_t, add), int32_t, int32_t>(
        "Calculator","add", add_done_cb, add_error_cb, NULL, -1, 1, i);
  }

  for (int i = 0; i < 10; i++) {
    controller.call<RPC_TYPE(Calculator, string, getString)>(
        "Calculator","getString", string_done_cb, string_error_cb, NULL, -1);
  }

  int n = controller.wait(80);
  LOG(INFO) << "calls handled:" << n << endl;
}
