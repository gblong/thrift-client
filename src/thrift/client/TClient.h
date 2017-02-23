#ifndef ANET_THRIFT_CLIENT_TCLIENT_H_
#define ANET_THRIFT_CLIENT_TCLIENT_H_

#include <sys/time.h>

#include <string>

#include <ev.h>
#include <thrift/TApplicationException.h>
#include <thrift/protocol/TBinaryProtocol.h>

using namespace std;
namespace net { namespace thrift { namespace client {
using apache::thrift::protocol::TProtocol;
using apache::thrift::protocol::TMessageType;

class TClient;

class TimeoutOps {
 public:
  virtual void timeout(TClient* call) = 0;
  virtual void state_timeout(TClient* call) = 0;
  virtual void wait_timeout(TClient* call) = 0;
};

struct Readable;
struct Writeable;
class TClient {
 public:
  typedef void (*done_cb_t)(void* data, const void* result);
  typedef void (*error_cb_t)(void* data, int32_t error);

  enum TClientError {
    TIMEOUT,
    NOCONNECTION,
    EXCEPTION,
    FAILED,
    UNKNOWN_RESULT
  };

  enum TClientState {
    PENDING,
    CALLOUT,
    CALLIN
  };

  TClient(const std::string &service,
          const std::string &function,
          done_cb_t done_cb,
          error_cb_t error_cb,
          void* data):
      service_(service),
      function_(function),
      done_cb_(done_cb),
      error_cb_(error_cb),
      id_(0),
      buf_(new string()),
      udata_(data) {}

  TClient(): buf_(new string()) {}

  void reset(const string &service,
             const string &function,
             done_cb_t done_cb,
             error_cb_t error_cb,
             void* data) {
    service_.assign(service);
    function_.assign(function);
    done_cb_ = done_cb;
    error_cb_ = error_cb;
    udata_ = data;
    id_ = 0;
  }

  ~TClient() {
    delete buf_;
  }

  int32_t readBody(TProtocol *iprot, const std::string &fname,
                   const TMessageType mtype, const int32_t rseqid,
                   Readable *reader);
  int32_t read(TProtocol *iprot, Readable *response);
  int32_t write(TProtocol *oprot, Writeable *request);
  void rpcDone(const void* resp);
  void rpcError(int32_t err);

  string* mutable_buf() {return buf_;}
  void set_id(uint32_t id) {id_ = id;}
  int32_t id() {return id_;}

  const string& service() {
    return service_;
  }
  const string& function() {
    return function_;
  }

  int32_t state() {
    return state_;
  }
  void set_state(int32_t state) {
    state_ = state;
  }

 private:
  friend class TClientController;
  friend class CallTimer;
  string service_;
  string function_;
  done_cb_t done_cb_;
  error_cb_t error_cb_;
  int32_t id_;                ///< call id
  ev_tstamp expiration_;
  ev_tstamp state_expiration_;
  string *buf_;       //serialization buffer
  int32_t state_;
  TimeoutOps* timeout_ops_;

  void timeout() {
    timeout_ops_->timeout(this);
  }
  void state_timeout() {
    timeout_ops_->state_timeout(this);
  }
  void wait_timeout() {
    timeout_ops_->wait_timeout(this);
  }

 public:
  void* udata_;   //user data for error_cb_ and done_cb_
  void* cdata_;   //data associating related connection
  void* tdata_;    //data associating related timers
  void (*rdata_)(void* data, uint8_t *buf, uint32_t len);
};

template <typename PARGS>
void pargs_set(PARGS &pargs) {}

template <typename PARGS, typename T1>
void pargs_set(PARGS &pargs, T1 &arg1) {
  pargs.arg1 = &arg1;
}

template <typename PARGS, typename T1, typename T2>
void pargs_set(PARGS &pargs, T1 &arg1, T2 &arg2) {
  pargs.arg2 = &arg2;
  pargs_set(pargs, arg1);
}

template <typename PARGS, typename T1, typename T2, typename T3>
void pargs_set(PARGS &pargs, T1 &arg1, T2 &arg2, T3 &arg3) {
  pargs.arg3 = &arg3;
  pargs_set(pargs, arg1, arg2);
}

struct Writeable {
  virtual ~Writeable() {}
  virtual uint32_t write(TProtocol* oprot) const = 0;
};

struct Readable {
  virtual ~Readable() {}
  virtual uint32_t read(TProtocol* iprot) = 0;
  virtual bool succeed() = 0;
  virtual void* data() = 0;
};

template <typename PARGS, typename... ARGS>
struct Writer: public Writeable {
  PARGS pargs;

  Writer() {}
  void set(ARGS&... args) {
    pargs_set(pargs, args...);
  }

  virtual uint32_t write(TProtocol* oprot) const {
    return pargs.write(oprot);
  }
};

template <typename RESPONSE, typename PRESULT>
struct Reader: public Readable {
  PRESULT result;

  Reader(RESPONSE *response) {
    result.success = response;
  }

  virtual bool succeed() {
    return result.__isset.success;
  }

  virtual uint32_t read(TProtocol* iprot) {
    return result.read(iprot);
  }

  virtual void* data() {
    return (void*)result.success;
  }
};

}}} //net::thrift::client

#endif
