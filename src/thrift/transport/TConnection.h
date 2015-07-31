#ifndef IQIYI_THRIFT_TRANSPORT_CONNECTION_H_
#define IQIYI_THRIFT_TRANSPORT_CONNECTION_H_

#include <string>

#include <ev.h>
#include <glog/logging.h>
#include <thrift/transport/TTransport.h>
#include <thrift/transport/TVirtualTransport.h>
#include <thrift/transport/TSocket.h>

#include "thrift/client/TClient.h"

using namespace std;
namespace net { namespace thrift { namespace transport {
using apache::thrift::transport::TSocket;
using net::thrift::client::TClient;
class TClient;
class TConnection;

struct ReadActionState {
  virtual ~ReadActionState() {}
  virtual int32_t action(TConnection* conn) = 0;
};
struct ReadHeaderState: public ReadActionState {
  virtual int32_t action(TConnection* conn);
};
struct ReadBodyState: public ReadActionState {
  virtual int32_t action(TConnection* conn);
};

typedef void (*read_done_cb_t)(void* data, uint8_t *buf, uint32_t len);
typedef void (*send_done_cb_t)(void* data, TClient* client);
typedef void (*error_cb_t)(void* data, TConnection* conn, int32_t error);

struct IOListener {
  void *data_;
  read_done_cb_t read_done_cb_;
  send_done_cb_t send_done_cb_;
  error_cb_t     error_cb_;
};

class TConnection: public TSocket {
 public:
  enum READ_STATE {
    HEAD_STATE = 0,
    BODY_STATE,
    END_STATE
  };

  TConnection(string host, int port): TSocket(host, port) {
    setWriteBuffer(NULL, 0);
    initReadAction();
  }

  TConnection(){
    setWriteBuffer(NULL, 0);
    initReadAction();
  }

  ~TConnection(){}

  void setWriteBuffer(const uint8_t* buf, uint32_t len) {
    wBuf_ = buf;
    wBufSize_ = len;
    wSize_ = 0;
  }

  void initReadAction() {
    bodySize_ = 0;
    rBufSize_ = 0;
    rBuf_ = NULL;
    rSize_ = 0;
    state_ = &headState_;
  }

  void set_io(struct ev_loop* loop, uint32_t event) {
    io_.data = this;
    ev_io_init(&io_, ev_io_cb, socket_, event);
    ev_io_start(loop, &io_);
  }
  void clear_io(struct ev_loop* loop) {
    ev_io_stop(loop, &io_);
  }


  void set_state(enum READ_STATE st) {
    rSize_ = 0;
    state_ = states[st];
  }

  void set_service(const string &service) {
    service_ = service;
  }
  const string& service() {
    return service_;
  }

  void set_function(const string &function) {
    function_ = function;
  }
  const string& function() {
    return function_;
  }

  void set_fail_timeout(uint32_t fail_timeout) {
    fail_timeout_ = fail_timeout;
  }
  const uint32_t fail_timeout() {
    return fail_timeout_;
  }

  ev_timer* mutable_timer() {
    return &timer_;
  }

  std::list<TClient*>* mutable_oqueue() {
    return &oqueue_;
  }


  void write(TClient* call) {
    call->cdata_ = this;
    oqueue_.push_back(call);
    write();
  }

  void remove(TClient* call) {
    oqueue_.remove(call);
  }

  void action(int revents) {
    //  LOG(INFO) << __FUNCTION__ << ":receive event:" << revents << endl;
    if (revents & EV_READ) {
      // LOG(INFO) << __FUNCTION__ << ":receive EV_READ:" << EV_READ << endl;
      read();
    }
    if (revents & EV_WRITE) {
      //LOG(INFO) << __FUNCTION__ << ":receive EV_WRITE:" << EV_WRITE << endl;
      write();
    }
  }

  void read();
  void write();
  int32_t try_write();

  void set_io_listener(void* data,
                       read_done_cb_t read_done_cb,
                       send_done_cb_t send_done_cb,
                       error_cb_t error_cb) {
    io_listener_.data_ = data;
    io_listener_.read_done_cb_ = read_done_cb;
    io_listener_.send_done_cb_ = send_done_cb;
    io_listener_.error_cb_ = error_cb;
  }

 private:
  friend class ReadHeaderState;
  friend class ReadBodyState;
  string service_;
  string function_;
  uint32_t fail_timeout_;
  IOListener io_listener_;
  struct ev_io io_;
  ev_timer timer_;

  //read action data
  uint8_t* rBuf_;
  uint32_t rBufSize_;
  uint32_t rSize_;
  int32_t bodySize_;
  ReadActionState *state_;

  //write action data
  const uint8_t* wBuf_;
  uint32_t wBufSize_;
  uint32_t wSize_;
  std::list<TClient*> oqueue_;

  static void ev_io_cb(struct ev_loop* loop, struct ev_io *w, int revents) {
    TConnection* conn = reinterpret_cast<TConnection*>(w->data);
    conn->action(revents);
  }

  static ReadActionState *getState(enum READ_STATE st) {
    return states[st];
  }
  static ReadHeaderState headState_;
  static ReadBodyState bodyState_;
  static ReadActionState *states[END_STATE];

 public:
  void* pdata_;   //connection pool data
};


}}}//net::thrift::transport

#endif //IQIYI_THRIFT_TRANSPORT_CONNECTION_H_
