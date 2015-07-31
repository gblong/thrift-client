#ifndef IQIYI_THRIFT_TRANSPORT_TSTRINGBUFFERTRANSPORTS_H_
#define IQIYI_THRIFT_TRANSPORT_TSTRINGBUFFERTRANSPORTS_H_

#include <cstring>

#include <glog/logging.h>
#include <thrift/transport/TTransport.h>
#include <thrift/transport/TVirtualTransport.h>

#ifdef __GNUC__
#define TDB_LIKELY(val) (__builtin_expect((val), 1))
#define TDB_UNLIKELY(val) (__builtin_expect((val), 0))
#else
#define TDB_LIKELY(val) (val)
#define TDB_UNLIKELY(val) (val)
#endif

namespace net { namespace thrift { namespace transport {
/**
 * TStringBufferTransport simply reads from and writes to an
 * in memory buffer. Anytime you call write on it, the data is simply placed
 * into a buffer, and anytime you call read, data is read from that buffer.
 */
using apache::thrift::transport::TVirtualTransport;
class TStringBufferTransport: public TVirtualTransport<TStringBufferTransport> {
 public:
  TStringBufferTransport(): rBase_(NULL), rBound_(NULL), strBuf_(NULL) {}
  virtual ~TStringBufferTransport() {}

  uint32_t read(uint8_t* buf, uint32_t len) {
    return readAll(buf, len);
  }

  uint32_t readAll(uint8_t* buf, uint32_t len) {
    uint32_t have = static_cast<uint32_t>(rBound_ - rBase_);
    uint32_t give = (std::min)(len, have);
    std::memcpy(buf, rBase_, give);
    rBase_ += give;

    return give;
  }

  void setWriteBuffer(string *str) {
    str->assign(4, '\0');
    strBuf_ = str;
  }

  void write(const uint8_t* buf, uint32_t len) {
    strBuf_->append((const char*)buf, len);
  }

  const uint8_t* borrow(uint8_t* buf, uint32_t* len) {
    if (TDB_LIKELY(static_cast<ptrdiff_t>(*len) <= rBound_ - rBase_)) {
      // With strict aliasing, writing to len shouldn't force us to
      // refetch rBase_ from memory.  TODO(dreiss): Verify this.
      *len = static_cast<uint32_t>(rBound_ - rBase_);
      return rBase_;
    }
    return NULL;
  }

  void consume(uint32_t len) {
    using apache::thrift::transport::TTransportException;
    if (TDB_LIKELY(static_cast<ptrdiff_t>(len) <= rBound_ - rBase_)) {
      rBase_ += len;
    } else {
      throw TTransportException(TTransportException::BAD_ARGS,
                                "consume did not follow a borrow.");
    }
  }

  void setReadBuffer(uint8_t* buf, uint32_t len) {
    rBase_ = buf;
    rBound_ = buf+len;
  }

  void open() {}

  bool isOpen() { return true; }

  void close() {}

  void flush() {
    uint32_t nblen = htonl(strBuf_->length() - sizeof(int));
    strBuf_->replace(0, sizeof(int), (const char*)&nblen, sizeof(nblen));
  }

  uint32_t readEnd() {return 0;}

  uint32_t writeEnd() {
    return strBuf_->length();
  }

 private:
  /// Reads begin here.
  uint8_t* rBase_;
  /// Reads may extend to just before here.
  uint8_t* rBound_;
  //buffer for write
  string *strBuf_;
};

}}} // net::thrift::transport

#endif
