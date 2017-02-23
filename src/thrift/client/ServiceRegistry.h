#ifndef ANET_THRIFT_CLIENT_SERVICEREGISTRY_H_
#define ANET_THRIFT_CLIENT_SERVICEREGISTRY_H_


#include <fcntl.h>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>

#include <ev.h>
#include <glog/logging.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "service.pb.h"

using namespace std;

namespace net { namespace thrift { namespace client {

typedef void (*add_cb_t)(void* data, Service* service, Server* server);
typedef void (*del_cb_t)(void* data, Service* service, Server* server);

struct ServiceListener {
  void* data_;
  add_cb_t add_cb_;
  del_cb_t del_cb_;
};

class ServiceRegistry {
 public:

  ServiceRegistry(const string& file): filename_(file) {
    new_list_ = &list_[0];
    old_list_ = &list_[1];
  }
  ~ServiceRegistry() {}


  virtual int32_t preload();
  virtual void reload();

  void postload() {
    old_list_->Clear();
  }

  void load() {
    if (preload() < 0) {
      LOG(WARNING) << __FUNCTION__ << ": conf load error!" << endl;
      return;
    }
    reload();
    postload();
  }

  int32_t findService(ServiceList* list, Service* service);
  int32_t findServer(Service* service, Server* server);

  void set_fstat(struct ev_loop* loop) {
    fstat_.data = this;
    ev_stat_init(&fstat_, fstat_cb, filename_.c_str(), 0.);
    ev_stat_start(loop, &fstat_);
  }
  void clear_fstat(struct ev_loop* loop) {
    ev_stat_stop(loop, &fstat_);
  }

  void set_listener(void* data,
                    add_cb_t add_cb,
                    del_cb_t del_cb) {
    listener_.data_ = data;
    listener_.add_cb_ = add_cb;
    listener_.del_cb_ = del_cb;
  }

  uint32_t read_timeout() {
    return new_list_->read_timeout();
  }
  uint32_t send_timeout() {
    return new_list_->send_timeout();
  }
  uint32_t timeout() {
    return new_list_->timeout();
  }

 private:
  string filename_;
  ServiceList list_[2];
  ServiceList* new_list_;
  ServiceList* old_list_;

  ServiceListener listener_;
  ev_stat  fstat_;

  static void fstat_cb(struct ev_loop* loop, ev_stat *w, int revents) {
    ServiceRegistry* registry = reinterpret_cast<ServiceRegistry*>(w->data);
    registry->load();
  }
};

}}}//namespace
#endif
