#include "TConnectionPool.h"

#include <glog/logging.h>

namespace net { namespace thrift { namespace transport {
int32_t TConnectionPool::open(TConnection* conn) {
  try {
    conn->open();
  } catch(apache::thrift::transport::TTransportException e) {
    return -1;
  }
  conn->set_io(ev_loop_, EV_READ | EV_WRITE);
  return 0;
}

void TConnectionPool::close(TConnection* conn) {
  conn->clear_io(ev_loop_);
  conn->close();
}

void TConnectionPool::destroy(TConnection* conn) {
  close(conn);
  connection_.put(conn);
}

ServiceEntry* TConnectionPool::find_or_insert(const string& service, const string& function) {
  string key;
  key.append(service);
  key.append(function);
  ServiceMap::iterator it = service_map_.find(key);
  ServiceEntry* entry;
  if (it == service_map_.end()) {
    entry = new ServiceEntry();
    service_map_[key] = entry;
  } else {
    entry = it->second;
  }

  return entry;
}

int32_t TConnectionPool::enable(TConnection* conn) {
  ServiceEntry* entry = find(conn->service(), conn->function());
  if (entry == NULL) {
    return -1;
  }
  entry->pending_conn_.remove(conn);
  clear_fail_timer(conn);

  entry->active_conn_.push_back(conn);
  entry->it_ = entry->active_conn_.begin();
  conn_listener_.conn_up_cb_(conn_listener_.data_, conn);

  return 0;
}

int32_t TConnectionPool::disable(TConnection* conn) {
  close(conn);
  ServiceEntry* entry = find(conn->service(), conn->function());
  if (entry == NULL) {
    return -1;
  }
  entry->active_conn_.remove(conn);
  entry->it_ = entry->active_conn_.begin();

  entry->pending_conn_.push_back(conn);
  set_fail_timer(conn);

  return 0;
}

void TConnectionPool::pend(TConnection* conn) {
  ServiceEntry* entry = find_or_insert(conn->service(), conn->function());
  entry->pending_conn_.push_back(conn);
  set_fail_timer(conn);
}

void TConnectionPool::active(TConnection* conn) {
  ServiceEntry* entry = find_or_insert(conn->service(), conn->function());
  entry->active_conn_.push_back(conn);
  entry->it_ = entry->active_conn_.begin();
  conn_listener_.conn_up_cb_(conn_listener_.data_, conn);
}

TConnection* TConnectionPool::add(const string& service,
                                  const string& function,
                                  const string& host,
                                  uint32_t port,
                                  uint32_t fail_timeout) {
  TConnection* conn = connection_.get();
  conn->setHost(host);
  conn->setPort(port);
  conn->set_service(service);
  conn->set_function(function);
  conn->set_fail_timeout(fail_timeout);
  if (open(conn) < 0) {
    pend(conn);
    return NULL;
  }
  active(conn);

  return conn;
}

int32_t TConnectionPool::del(const string& service,
                             const string& function,
                             const string& host,
                             uint32_t port) {
    string key;
    key.append(service);
    key.append(function);
    ServiceMap::iterator it = service_map_.find(key);
    if (it == service_map_.end()) {
      return -1;
    }
    ServiceEntry* entry = it->second;

    del(&entry->pending_conn_, host, port, del_pending_conn_cb);
    del(&entry->active_conn_, host, port, del_active_conn_cb);

    if (entry->pending_conn_.empty() && entry->active_conn_.empty()) {
      service_map_.erase(it);
      delete entry;
    } else {
      entry->it_ = entry->active_conn_.begin();
    }

    return 0;
}

void TConnectionPool::del(std::list<TConnection*> *list, const string& host, uint32_t port,
                          void (*del_conn_cb)(TConnectionPool*, TConnection*)) {
  TConnection* conn;
  std::list<TConnection*>::iterator itor;
  for (itor = list->begin(); itor != list->end();) {
    conn = *itor;
    if (conn->getHost() == host && conn->getPort() == port) {
      list->erase(itor++);
      if (del_conn_cb) {
        del_conn_cb(this, conn);
      }
    } else {
      ++itor;
    }
  }
}

}}}//net::thrift::transport
