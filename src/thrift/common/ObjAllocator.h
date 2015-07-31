#ifndef IQIYI_COMMON_OBJALLOCATOR_H_
#define IQIYI_COMMON_OBJALLOCATOR_H_

#include <list>

namespace net { namespace common {
template <typename T>
class ObjAllocator {
 public:
  ObjAllocator() {}
  ~ObjAllocator() {
    clear(free_list_);
    clear(used_list_);
  }

  T *get() {
    T *obj = NULL;

    if (free_list_.empty()) {
      obj = new T();
    } else {
      obj = free_list_.front();
      free_list_.pop_front();
    }
    used_list_.push_back(obj);

    return obj;
  }

  void put(T *obj) {
    used_list_.remove(obj);
    free_list_.push_back(obj);
  }

  void clear(std::list<T*>& list) {
    while(!list.empty()) {
      T* obj = list.front();
      list.pop_front();
      delete obj;
    }
  }


 private:
  std::list<T*> used_list_;
  std::list<T*> free_list_;
};

}}

#endif
