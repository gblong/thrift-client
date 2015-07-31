#include "TConnection.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include <errno.h>

#include <glog/logging.h>


using namespace std;

namespace net { namespace thrift { namespace transport {
ReadHeaderState TConnection::headState_;
ReadBodyState TConnection::bodyState_;
ReadActionState *TConnection::states[TConnection::END_STATE] = {&headState_, &bodyState_};

int32_t ReadHeaderState::action(TConnection* conn) {
  uint8_t* szp = NULL;
  int32_t len = 0;

  while (conn->rSize_ < sizeof(conn->bodySize_)) {
    szp = reinterpret_cast<uint8_t*>(&conn->bodySize_) + conn->rSize_;
    len = recv(conn->socket_, szp, sizeof(conn->bodySize_) - conn->rSize_, MSG_DONTWAIT);
    if (len < 0) {
      //LOG(WARNING) << __FUNCTION__ << ": recv error: " << strerror(errno) << endl;
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return -1;
      }
      if (errno == EINTR) {
        continue;
      }

      conn->io_listener_.error_cb_(conn->io_listener_.data_, conn, errno);
      return -1;
    }
    if (len == 0) {//the peer has performed an orderly shutdown.
      conn->io_listener_.error_cb_(conn->io_listener_.data_, conn, errno);
      return -1;
    }
    conn->rSize_ += len;
  }
  conn->bodySize_ = ntohl(conn->bodySize_);

  //LOG(INFO) << __FUNCTION__ << " : body size : " << conn->bodySize_ << endl;

  if (conn->bodySize_ < 0) {
    conn->io_listener_.error_cb_(conn->io_listener_.data_, conn, 0);
    return -1;//Frame size has negative value
  }
  if (conn->bodySize_ > conn->rBufSize_) {
    delete[] conn->rBuf_;
    conn->rBuf_ = new uint8_t[conn->bodySize_];
    conn->rBufSize_ = conn->bodySize_;
  }

  conn->set_state(TConnection::BODY_STATE);

  return 0;
}


int32_t ReadBodyState::action(TConnection* conn) {
  int len;
  uint32_t left = conn->bodySize_ - conn->rSize_;
  while (left >0) {
    len = recv(conn->socket_, conn->rBuf_ + conn->rSize_, left, MSG_DONTWAIT);
    if (len < 0) {
      //LOG(WARNING) << __FUNCTION__ << ": recv error: " << strerror(errno) << endl;
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return -1;
      }
      if (errno == EINTR) {
        continue;
      }
      conn->io_listener_.error_cb_(conn->io_listener_.data_, conn, errno);
      return -1;
    }
    if (len == 0) {//the peer has performed an orderly shutdown.
      LOG(WARNING) << __FUNCTION__ << ":the peer has shutdown." << endl;
      conn->io_listener_.error_cb_(conn->io_listener_.data_, conn, errno);
      return -1;
    }

    conn->rSize_ += len;
    left = conn->bodySize_ - conn->rSize_;
  }
  conn->io_listener_.read_done_cb_(conn->io_listener_.data_, conn->rBuf_, conn->bodySize_);
  conn->set_state(TConnection::HEAD_STATE);

  return 0;
}

void TConnection::read() {
  if (!isOpen()) {
    return;
  }
  while (1) {
    if (state_->action(this) < 0) {
      break;
    }
  }
}

void TConnection::write() {
  while (1) {
    if (try_write() < 0) {
      break;
    }
  }
}

int32_t TConnection::try_write() {
  int len;
  uint32_t left;
  if (wBuf_ == NULL) {
    if (!oqueue_.empty()) {
      TClient* call = oqueue_.front();
      string *str = call->mutable_buf();
      setWriteBuffer(reinterpret_cast<const uint8_t*>(str->data()), str->length());
    } else {
      return -1;
    }
  }

  left = wBufSize_ - wSize_;
  while (left > 0) {
    len = ::send(socket_, wBuf_ + wSize_, left, MSG_DONTWAIT |  MSG_NOSIGNAL);
    if (len < 0) {
      //LOG(WARNING) << __FUNCTION__ << ": write error: " << strerror(errno) << endl;
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return -1;
      }
      if (errno == EINTR) {
        continue;
      }
      //EPIPE, ECONNRESET, ENOTCONN
      setWriteBuffer(NULL, 0);
      io_listener_.error_cb_(io_listener_.data_, this, errno);
      return -1;
    }

    wSize_ += len;
    left = wBufSize_ - wSize_;
  }

  io_listener_.send_done_cb_(io_listener_.data_, oqueue_.front());
  oqueue_.pop_front();
  setWriteBuffer(NULL, 0);

  return 0;
}

}}}//net::thrift::transport
