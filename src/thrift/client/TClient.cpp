#include "TClient.h"

namespace net { namespace thrift { namespace client {
using apache::thrift::protocol::TProtocol;
using apache::thrift::protocol::TMessageType;
using apache::thrift::protocol::T_EXCEPTION;

int32_t TClient::readBody(TProtocol *iprot,
                          const std::string &fname,
                          const TMessageType mtype,
                          const int32_t rseqid, Readable *reader) {
  if (mtype == T_EXCEPTION) {
    ::apache::thrift::TApplicationException x;
    x.read(iprot);
    iprot->readMessageEnd();
    iprot->getTransport()->readEnd();
    rpcError(EXCEPTION);

    return -1;
  }
  if (mtype != ::apache::thrift::protocol::T_REPLY) {
    iprot->skip(::apache::thrift::protocol::T_STRUCT);
    iprot->readMessageEnd();
    iprot->getTransport()->readEnd();
  }
  if (fname.compare(function_) != 0) {
    iprot->skip(::apache::thrift::protocol::T_STRUCT);
    iprot->readMessageEnd();
    iprot->getTransport()->readEnd();
  }

  reader->read(iprot);
  iprot->readMessageEnd();
  iprot->getTransport()->readEnd();
  if (reader->succeed()) {
    rpcDone(reader->data());
    return 0;
  }

  rpcError(UNKNOWN_RESULT);

  return -1;
}

int32_t TClient::read(TProtocol *iprot, Readable *response) {
  int32_t rseqid = 0;
  std::string fname;
  TMessageType mtype;
  iprot->readMessageBegin(fname, mtype, rseqid);

  return readBody(iprot, fname, mtype, rseqid, response);
}

int32_t TClient::write(TProtocol *oprot, Writeable *request) {
  oprot->writeMessageBegin(function_, ::apache::thrift::protocol::T_CALL, id_);
  request->write(oprot);
  oprot->writeMessageEnd();
  oprot->getTransport()->writeEnd();
  oprot->getTransport()->flush();

  return 0;
}

void TClient::rpcDone(const void* resp) {
  if (done_cb_) {
    done_cb_(udata_, resp);
  }
}

void TClient::rpcError(int32_t err) {
  if (error_cb_) {
    error_cb_(udata_, err);
  }
}

}}} //net::thrift::client
