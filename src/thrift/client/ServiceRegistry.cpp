#include "ServiceRegistry.h"

using namespace std;

namespace net { namespace thrift { namespace client {
int32_t ServiceRegistry::findService(ServiceList* list, Service* service) {
  for (int32_t i = 0; i < list->service_size(); i++) {
    Service* svc = list->mutable_service(i);
    if (svc->name() == service->name()) {
      return i;
    }
  }
  return -1;
}

int32_t ServiceRegistry::findServer(Service* service, Server* server) {
  for (int32_t i = 0; i < service->server_size(); i++) {
    Server *svr = service->mutable_server(i);
    if (svr->host() == server->host() &&
        svr->port() == server->port()) {
      return i;
    }
  }
  return -1;
}

int32_t ServiceRegistry::preload() {
  int fd = open(filename_.c_str(), O_RDONLY);
  if( fd < 0 ){
    return -1; 
  }   

  std::swap(old_list_, new_list_);
  google::protobuf::io::FileInputStream fileInput(fd);
  fileInput.SetCloseOnDelete(true);
  if (!google::protobuf::TextFormat::Parse(&fileInput, new_list_)){
    std::swap(old_list_, new_list_);
    return -2; 
  }

  return 0;
}

void ServiceRegistry::reload() {
  Service* newService = NULL;
  Server* newServer = NULL;

  for(int32_t i = 0; i < new_list_->service_size(); i++) {
    newService = new_list_->mutable_service(i);
    int32_t serviceIndex = findService(old_list_, newService);
    if (serviceIndex > -1) {
      Service* oldService = old_list_->mutable_service(serviceIndex);
      for(int32_t j = 0; j < newService->server_size(); j++) {
        newServer = newService->mutable_server(j);
        int32_t  serverIndex = findServer(oldService, newServer);
        if (serverIndex > -1) {//find server
          oldService->mutable_server()->DeleteSubrange(serverIndex, 1);
        } else {
          listener_.add_cb_(listener_.data_, newService, newServer);
        }
      }
    } else {//new added service
      for (int32_t m = 0; m < newService->server_size(); m++) {
        newServer =  newService->mutable_server(m);
        LOG(INFO) << "new service:" << newService->name()
            << " function" << newService->function() << endl;
        listener_.add_cb_(listener_.data_, newService, newServer);
      }
    }
  }

  //the deleted server
  for (int32_t i = 0; i < old_list_->service_size(); i++) {
    Service* oldService = old_list_->mutable_service(i);
    LOG(INFO) << "delete old service:" << oldService->name() << endl;
    for(int32_t j = 0; j < oldService->server_size(); j++) {
      Server* oldServer = oldService->mutable_server(j);
      listener_.del_cb_(listener_.data_, oldService, oldServer);
    }
  }
}

}}}
