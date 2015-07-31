#include "TClientController.h"

namespace net { namespace thrift { namespace client {
SendTimeoutOps send_timeout_ops;
ReadTimeoutOps read_timeout_ops;
PendTimeoutOps pend_timeout_ops;

TClientController::TClientController(const string& conf, struct ev_loop *loop):
    call_id_(0),
    ev_loop_(loop),
    conn_pool_(),
    service_registry_(conf) {
      if (ev_loop_ == NULL) {
        mode_ = CTL_WAIT;
        ev_loop_ = ev_loop_new(EVBACKEND_EPOLL);
      }
      conn_pool_.set_ev_loop(ev_loop_);
      conn_pool_.set_conn_listener(this, conn_up_cb, conn_down_cb);

      service_registry_.set_listener(this, service_add_cb, service_del_cb);
      service_registry_.load();
      service_registry_.set_fstat(ev_loop_);

      pend_timer_.start(ev_loop_, service_registry_.timeout());
      read_timer_.start(ev_loop_, service_registry_.read_timeout());
      send_timer_.start(ev_loop_, service_registry_.send_timeout());

      transport_ = new TStringBufferTransport();
      protocol_ = new TBinaryProtocol(boost::shared_ptr<TStringBufferTransport>(transport_));
    }

TClientController::~TClientController() {
  delete protocol_;

  send_timer_.stop();
  read_timer_.stop();
  pend_timer_.stop();

  service_registry_.clear_fstat(ev_loop_);

  if (mode_ == CTL_WAIT) {
    ev_loop_destroy(ev_loop_);
  }
}

//wait for second
int32_t TClientController::wait(const ev_tstamp timeout) {
  if (mode_ != CTL_WAIT) {
    return -1;
  }
  if (timeout <= 0) {
    return -2;
  }
  int32_t ret = call_map_.size();
  if (ret == 0) {
    return 0;
  }

  int32_t left = 0;
  wait_timer_.data = this;
  ev_timer_init(&wait_timer_, wait_timeout_cb, timeout, 0.);
  ev_timer_start (ev_loop_, &wait_timer_);

  wait_checker_.data = this;
  ev_check_init(&wait_checker_, wait_check_cb);
  ev_check_start(ev_loop_, &wait_checker_);

  ev_run(ev_loop_, 0);

  LOG(INFO) << __func__ << ":ev_run" << endl;
  left = call_map_.size();
  if (left > 0) {
    ret -= left;
    clear_call_map();
  }

  return ret;
}

TimeoutOps* TClientController::send_timeout_ops_ = &send_timeout_ops;
TimeoutOps* TClientController::read_timeout_ops_ = &read_timeout_ops;
TimeoutOps* TClientController::pend_timeout_ops_ = &pend_timeout_ops;

}}}//namespace
