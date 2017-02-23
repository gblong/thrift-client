#ifndef ANET_THRIFT_CLIENT_TCLIENTCONTROLLER_H_
#define ANET_THRIFT_CLIENT_TCLIENTCONTROLLER_H_

#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>

#include <list>
#include <map>
#include <string>

#include <ev.h>
#include <glog/logging.h>
#include <thrift/protocol/TBinaryProtocol.h>

#include "ServiceRegistry.h"
#include "thrift/client/TClient.h"
#include "thrift/transport/TStringBufferTransport.h"
#include "thrift/transport/TConnectionPool.h"
#include "thrift/common/ObjAllocator.h"

/// @param R RPC response type
/// @param S service_name
/// @param F function_name
/// @param RESPONSE RPC response type
/// @param PARGS a class that thrift generates, class name format: $service_$function_pargs
/// @param PRESULT a class that thrift generates, class name format: $service_$function_presult
//#define RPC_TYPE(S,R,F) #S, R, S##_##F##_pargs, S##_##F##_presult
#define RPC_TYPE(S,R,F) R, S##_##F##_pargs, S##_##F##_presult

using namespace std;
using net::thrift::transport::TStringBufferTransport;
using net::thrift::transport::TConnectionPool;
using net::thrift::transport::read_done_cb_t;
using net::thrift::client::TClient;
using net::thrift::client::TimeoutOps;
using net::common::ObjAllocator;
using apache::thrift::protocol::TProtocol;
using apache::thrift::protocol::TBinaryProtocol;

namespace net { namespace thrift { namespace client {
class TClientController;
class CallTimer {
 public:
  CallTimer() {}
  ~CallTimer() {}

  void start(struct ev_loop* loop, double timeout) {
    loop_ = loop;
    timer_.data = this;
    timeout_ = timeout;

    ev_timer_init(&timer_, timeout_cb, timeout, timeout);
    ev_timer_start(loop_, &timer_);
  }

  void stop() {
    ev_timer_stop(loop_, &timer_);
  }

  void add(TClient* call) {
    call->state_expiration_ = ev_now(loop_) + timeout_;
    list_.push_back(call);
  }

  void del(TClient* call) {
    list_.remove(call);
  }

 private:
  struct ev_loop* loop_;
  ev_timer timer_;
  ev_tstamp timeout_;
  std::list<TClient*> list_;

  static void timeout_cb (struct ev_loop* loop, ev_timer *w, int revents) {
    std::list<TClient*> list = reinterpret_cast<CallTimer*>(w->data)->list_;
    std::list<TClient*>::iterator it;
    TClient* call;
    ev_tstamp now = ev_now(loop);
    for (it = list.begin(); it != list.end();) {
      call = *it;
      if (now >= call->expiration_) {
        LOG(INFO) << __func__ << ":call timeout!" << endl;
        list.erase(it++);
        call->timeout();
      } else if (now >= call->state_expiration_) {
        LOG(INFO) << __func__ << ":call state:" << call->state_ << " timeout!" << endl;
        list.erase(it++);
        call->state_timeout();
      } else {
        break;
      }
    }
  }
};


using net::thrift::transport::TConnection;
//class TConnection;
class TClientController {
 public:
  typedef std::list<TClient*> TClientList;

  enum {CTL_WAIT, CTL_NOWAIT};

  TClientController(const string& conf, struct ev_loop *loop);
  virtual ~TClientController();

  template <typename RESPONSE, typename PARGS, typename PRESULT, typename... ARGS>
  int32_t call(const std::string& service,
               const std::string& function,
               TClient::done_cb_t done_cb,
               TClient::error_cb_t error_cb,
               void* data,
               double timeout,    //-1 means it is decides by system
               ARGS... args) {
    TClient *call = call_allocator_.get();
    call->reset(service, function, done_cb, error_cb, data);

    uint32_t id = ++call_id_;
    map<uint32_t, TClient*>::iterator iter = call_map_.find(id);
    if (iter != call_map_.end()) {
      this->timeout(iter->second);
    }
    call->id_ = id;
    call_map_[id] = call;

    transport_->setWriteBuffer(call->mutable_buf());
    Writer<PARGS, ARGS...> param;
    param.set(args...);
    call->write(protocol_, &param);

    if (timeout > 0) {
      call->expiration_ = ev_now(ev_loop_) + timeout;
    } else {
      call->expiration_ = ev_now(ev_loop_) + service_registry_.timeout();
    }

    call->tdata_ = this;
    call->rdata_ =  conn_read_done_cb<RESPONSE, PRESULT>;

    TConnection *conn = getConnection(service, function);
    if (conn == NULL) {
      LOG(WARNING) << __FUNCTION__ <<  ": no connection for service: "
          << service << "-" << function << endl;
      pend(call);
      return -1;
    }

    conn->set_io_listener(this,
                          conn_read_done_cb<RESPONSE, PRESULT>,
                          conn_send_done_cb,
                          conn_error_cb);

    call->state_ = TClient::CALLOUT;
    call->timeout_ops_ = send_timeout_ops_;

    send_timer_.add(call);
    conn->write(call);

    return 0;
  }

  int32_t recall(TClient* call) {
    TConnection *conn = getConnection(call->service_, call->function_);
    if (conn == NULL) {
      pend(call);
      return -1;
    }
    conn->set_io_listener(this,
                          call->rdata_,
                          conn_send_done_cb,
                          conn_error_cb);
    call->state_ = TClient::CALLOUT;
    call->timeout_ops_ = send_timeout_ops_;
    send_timer_.add(call);
    conn->write(call);

    return 0;
  }

  int32_t pend(TClient* call) {
    call->state_ = TClient::PENDING;
    call->state_expiration_ = call->expiration_;
    call->timeout_ops_ = pend_timeout_ops_;

    string key;
    key += call->service();
    key += call->function();
    TClientList* list;
    std::map<string, TClientList*>::iterator it;
    it = pending_call_.find(key);
    if (it == pending_call_.end()) {
      list = new TClientList();
      pending_call_[key] = list;
    } else {
      list = it->second;
    }
    list->push_back(call);

    pend_timer_.add(call);

    return 0;
  }

  void clear(TClient* call) {
    call->buf_->clear();
    call_allocator_.put(call);
  }

  void remove(TClient* call) {
    call_map_.erase(call->id());
    clear(call);
  }

  void timeout(TClient* call) {
    call->rpcError(TClient::TIMEOUT);
    remove(call);
  }

  void wait_timeout(TClient*call) {
    call->rpcError(TClient::TIMEOUT);
    clear(call);
  }

  void read_timeout(TClient* call) {
    recall(call);
  }

  void send_timeout(TClient* call) {
    TConnection* conn = reinterpret_cast<TConnection*>(call->cdata_);
    conn->remove(call);
    recall(call);
  }

  void pend_timeout(TClient*call) {
      TClientList* list = pending_call(call->service_, call->function_);
      if (list != NULL) {
        list->remove(call);
      }
      timeout(call);
  }


  //io complete interfaces
  template <typename RESPONSE, typename PRESULT>
  void read_done_cb(uint8_t *buf, uint32_t len) {
    int32_t callid = 0;
    string fname;
    apache::thrift::protocol::TMessageType mtype;
    TClient *call = NULL;

    transport_->setReadBuffer(buf, len);
    try {
      protocol_->readMessageBegin(fname, mtype, callid);
    } catch (apache::thrift::TException &e) {
      LOG(ERROR) << __FUNCTION__ <<  ": EXCEPTION: " << e.what() << endl;
      mtype = apache::thrift::protocol::T_EXCEPTION;
    }

    map<uint32_t, TClient*>::iterator iter;
    iter = call_map_.find(callid);
    if (iter != call_map_.end()) {
      call = iter->second;
      RESPONSE response;
      Reader<RESPONSE, PRESULT> reader(&response);
      call->readBody(protocol_, fname, mtype, callid, &reader);
      read_timer_.del(call);
      remove(call);
    } else {
      //timed out
    }
  }

  void send_done_cb(TClient* call) {
    call->state_ = TClient::CALLIN;
    call->timeout_ops_ = read_timeout_ops_;
    send_timer_.del(call);
    read_timer_.add(call);
  }

  void error_cb(TConnection* conn, int error) {
    LOG(WARNING) << __FUNCTION__ << ": connection error!" << endl;
    TClient* call = NULL;
    conn_pool_.disable(conn);
    std::list<TClient*> *oqueue = conn->mutable_oqueue();
    while (!oqueue->empty()) {
      call = oqueue->front();
      oqueue->pop_front();
      recall(call);
    }
  }

  int32_t wait(const ev_tstamp timeout);//wait for timeout second

  TConnection *getConnection(const string& service, const string& function) {
    return conn_pool_.next(service, function);
  }

  TClientList* pending_call(const string& service, const string& function) {
    string key;
    key += service;
    key += function;

    std::map<string, TClientList*>::iterator it;
    it = pending_call_.find(key);
    if (it == pending_call_.end()) {
      return NULL;
    }
    return it->second;
  }

  TConnectionPool* mutable_conn_pool() {
    return &conn_pool_;
  }

 private:
  friend class SendTimeoutOps;
  friend class ReadTimeoutOps;
  friend class PendTimeoutOps;

  int mode_;
  int32_t call_id_;      ///call id

  TConnectionPool conn_pool_;

  ObjAllocator<TClient> call_allocator_;
  map<uint32_t, TClient*> call_map_;  //id -> call
  std::map<string, TClientList*> pending_call_;

  struct ev_loop *ev_loop_;
  ev_timer wait_timer_;
  ev_check wait_checker_;

  CallTimer pend_timer_;
  CallTimer read_timer_;
  CallTimer send_timer_;

  TProtocol *protocol_;
  TStringBufferTransport* transport_;

  ServiceRegistry service_registry_;

  static TimeoutOps *send_timeout_ops_;
  static TimeoutOps *read_timeout_ops_;
  static TimeoutOps *pend_timeout_ops_;

  void clear_call_map() {
    TClient* call;
    std::map<uint32_t, TClient*>::iterator it;
    for (it = call_map_.begin(); it != call_map_.end();) {
      call = it->second;
      call_map_.erase(it++);
      call->wait_timeout();
    }
  }

  //io complete listener callbacks
  template <typename response, typename presult>
  static void conn_read_done_cb(void* data, uint8_t* buf, uint32_t len) {
    reinterpret_cast<TClientController*>(data)->read_done_cb<response, presult>(buf, len);
  }
  static void conn_send_done_cb(void* data, TClient* call) {
    reinterpret_cast<TClientController*>(data)->send_done_cb(call);
  }
  static void conn_error_cb(void* data, TConnection* conn, int error) {
    reinterpret_cast<TClientController*>(data)->error_cb(conn, error);
  }

  static void wait_timeout_cb (struct ev_loop* loop, ev_timer *w, int revents) {
    TClientController* controller = reinterpret_cast<TClientController*>(w->data);
    ev_check_stop(controller->ev_loop_, &controller->wait_checker_);
    ev_break (controller->ev_loop_, EVBREAK_ALL);
  }

  static void wait_check_cb (struct ev_loop *loop, ev_check *w, int revents) {
    TClientController* controller = reinterpret_cast<TClientController*>(w->data);
    if (controller->call_map_.empty()) {
      ev_check_stop(controller->ev_loop_, &controller->wait_checker_);
      ev_timer_stop(controller->ev_loop_, &controller->wait_timer_);
      ev_break (controller->ev_loop_, EVBREAK_ALL);
    }
  }

  //connection from pending to active or new connection created
  static void conn_up_cb(void* data, TConnection* conn) {
    TClientController* controller = reinterpret_cast<TClientController*>(data);
    TClientList* list = controller->pending_call(conn->service(), conn->function());
    if (list == NULL) {
      return;
    }
    TClient* call;
    TClientList::iterator it;
    for (it = list->begin(); it != list->end();) {
      call = *it;
      list->erase(it++);
      controller->pend_timer_.del(call);
      if (controller->recall(call) < 0) {
        break;
      }
    }
  }
  static void conn_down_cb(void* data, TConnection* conn) {
    TClientController* controller = reinterpret_cast<TClientController*>(data);
    std::list<TClient*> *oqueue = conn->mutable_oqueue();
    while (!oqueue->empty()) {
      TClient* call = oqueue->front();
      oqueue->pop_front();
      controller->recall(call);
    }
  }

  static void service_add_cb(void* data, Service* service, Server* server) {
    TClientController* controller = reinterpret_cast<TClientController*>(data);
    controller->mutable_conn_pool()->add(service->name(),
                                         service->function(),
                                         server->host(),
                                         server->port(),
                                         server->fail_timeout());
  }
  static void service_del_cb(void* data, Service* service, Server* server) {
    TClientController* controller = reinterpret_cast<TClientController*>(data);
    controller->mutable_conn_pool()->del(service->name(),
                                         service->function(),
                                         server->host(),
                                         server->port());
  }

};

//call timeout operations
class DefaultTimeoutOps: public TimeoutOps {
  virtual void timeout(TClient* call) {
    reinterpret_cast<TClientController*>(call->tdata_)->timeout(call);
  }
};

class ReadTimeoutOps: public DefaultTimeoutOps {
  virtual void state_timeout(TClient* call) {
    reinterpret_cast<TClientController*>(call->tdata_)->read_timeout(call);
  }

  virtual void wait_timeout(TClient* call) {
    TClientController* controller =
        reinterpret_cast<TClientController*>(call->tdata_);
    controller->read_timer_.del(call);
    controller->wait_timeout(call);
  }
};

class SendTimeoutOps: public DefaultTimeoutOps {
  virtual void state_timeout(TClient* call) {
    reinterpret_cast<TClientController*>(call->tdata_)->send_timeout(call);
  }

  virtual void wait_timeout(TClient* call) {
    TClientController* controller =
        reinterpret_cast<TClientController*>(call->tdata_);
    controller->send_timer_.del(call);
    controller->wait_timeout(call);
  }
};

class PendTimeoutOps: public DefaultTimeoutOps {
  virtual void timeout(TClient* call) {
    state_timeout(call);
  }

  virtual void state_timeout(TClient* call) {
    reinterpret_cast<TClientController*>(call->tdata_)->pend_timeout(call);
  }

  virtual void wait_timeout(TClient* call) {
    TClientController* controller =
        reinterpret_cast<TClientController*>(call->tdata_);
    controller->pend_timer_.del(call);
    controller->wait_timeout(call);
  }
};

}}} //namespace

#endif
