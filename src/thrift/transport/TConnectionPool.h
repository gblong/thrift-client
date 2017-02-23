#ifndef ANET_THRIFT_ASYNC_CONNECTIONPOOL_H_
#define ANET_THRIFT_ASYNC_CONNECTIONPOOL_H_

#include <ev.h>

#include "thrift/common/ObjAllocator.h"
#include "thrift/transport/TConnection.h"

namespace net { namespace thrift { namespace transport {
using net::common::ObjAllocator;

struct ServiceEntry {
  ServiceEntry() {
    it_ = active_conn_.begin();
  }
  ~ServiceEntry() {}

  std::list<TConnection*> pending_conn_;
  std::list<TConnection*> active_conn_;
  std::list<TConnection*>::iterator it_;
};

typedef void (*conn_change_cb_t)(void* data, TConnection* conn);

struct TConnListener {
  void* data_;
  conn_change_cb_t conn_up_cb_;
  conn_change_cb_t conn_down_cb_;
};

class TConnectionPool {
 public:
  typedef std::map<std::string, ServiceEntry*> ServiceMap;

  TConnectionPool() {}
  explicit TConnectionPool(struct ev_loop* loop): ev_loop_(loop) {}
  ~TConnectionPool() {}

  ServiceEntry* find(const string& service, const string& function) {
    string key;
    key.append(service);
    key.append(function);
    ServiceMap::iterator it = service_map_.find(key);
    if (it == service_map_.end()) {
      return NULL;
    }
    return it->second;
  }

  TConnection* next(const string& service, const string& function) {
    ServiceEntry* entry = find(service, function);
    if (entry == NULL) {
      return NULL;
    }
    if (entry->it_ != entry->active_conn_.end()) {
      return *entry->it_++;
    }
    if (entry->active_conn_.size() == 0) {
      return NULL;
    }
    entry->it_ = entry->active_conn_.begin();

    return *entry->it_++;
  }

  void set_ev_loop(struct ev_loop* loop) {
    ev_loop_= loop;
  }

  void set_conn_listener(void* data,
                         conn_change_cb_t conn_up_cb,
                         conn_change_cb_t conn_down_cb) {
    conn_listener_.data_ = data;
    conn_listener_.conn_up_cb_ = conn_up_cb;
    conn_listener_.conn_down_cb_ = conn_down_cb;
  }

  int32_t open(TConnection* conn);
  void close(TConnection* conn);
  void destroy(TConnection* conn);


  void active(TConnection* conn);
  void pend(TConnection* conn);

  int32_t disable(TConnection* conn);
  int32_t enable(TConnection* conn);

  ServiceEntry* find_or_insert(const string& service, const string& function);

  TConnection* add(const string& service,
                   const string &function,
                   const string& host,
                   uint32_t port,
                   uint32_t fail_timeout);

  void del(std::list<TConnection*> *list, const string& host, uint32_t port,
           void (*del_conn_cb)(TConnectionPool*, TConnection*));
  int32_t del(const string& service,
              const string& function,
              const string& host,
              uint32_t port);


 private:
  struct ev_loop* ev_loop_;
  ServiceMap service_map_;
  TConnListener conn_listener_;
  ObjAllocator<TConnection> connection_;

  void set_fail_timer(TConnection* conn) {
    conn->pdata_ = this;
    ev_timer* timer = conn->mutable_timer();
    timer->data = conn;
    ev_timer_init(timer, reconnect_cb, conn->fail_timeout(), conn->fail_timeout());
    ev_timer_start (ev_loop_, timer);
  }

  void clear_fail_timer(TConnection* conn) {
    ev_timer_stop(ev_loop_, conn->mutable_timer());
  }

  static void del_pending_conn_cb(TConnectionPool* pool, TConnection* conn) {
    pool->clear_fail_timer(conn);
    pool->destroy(conn);
  }

  static void del_active_conn_cb(TConnectionPool* pool, TConnection* conn) {
    TConnListener& listener = pool->conn_listener_;
    listener.conn_down_cb_(listener.data_, conn);
    pool->destroy(conn);
  }

  static void reconnect_cb(struct ev_loop* loop, ev_timer *w, int revents) {
    TConnection* conn = reinterpret_cast<TConnection*>(w->data);
    TConnectionPool* conn_pool = reinterpret_cast<TConnectionPool*>(conn->pdata_);
    if (conn_pool->open(conn) > -1) {
      conn_pool->enable(conn);
    }
  }
};

}}}//net::thrift::transport

#endif
