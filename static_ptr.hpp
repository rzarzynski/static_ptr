/*
 * (C) Copyright 2016 Mirantis Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author:
 *     Radoslaw Zarzynski <rzarzynski@mirantis.com>
 */

#include <cstddef>
#include <type_traits>


template <class TypeT, size_t MaxSize>
class static_ptr {
public:
  typedef TypeT* pointer;
  typedef TypeT element_type;

private:
  struct Cloneable;
  typedef Cloneable* pointer_lcm;

  /* Virtual constructors for C++... */
  struct Cloneable {
    virtual pointer clone_obj(void *, TypeT&&) = 0;
    virtual pointer_lcm clone_lcm(void *) = 0;
  };

  unsigned char storage[MaxSize];
  unsigned char lcm_storage[sizeof(Cloneable)];
  bool is_empty = true;

  pointer_lcm get_lcm() noexcept {
    return reinterpret_cast<pointer_lcm>(lcm_storage);
  }

public:
  static_ptr() {
  }

  static_ptr(static_ptr&& rhs) {
    if (false == rhs.is_empty) {
      rhs.get_lcm()->clone_obj(storage, std::move(*rhs.get()));
      rhs.get_lcm()->clone_lcm(lcm_storage);
      is_empty = false;
    }
  }

  static_ptr& operator=(static_ptr&& rhs) {
    if (false == rhs.is_empty) {
      rhs.lcm->clone_obj(storage, std::move(*rhs.get()));
      rhs.lcm->clone_lcm(lcm_storage);
      is_empty = false;
    }
  }

  /* Let's mimic the std::unique_ptr's behaviour. It's very useful to say to
   * the world who controls the lifetime of the contained object. */
  static_ptr(const static_ptr&) = delete;
  static_ptr& operator=(const static_ptr&) = delete;


  template <class T,
            typename std::enable_if<std::is_base_of<TypeT, T>::value>::type* = nullptr >
  static_ptr(T&& t) {
    struct ClonableT : public Cloneable {
      virtual TypeT* clone_obj(void* p, TypeT&& u) override {
        return new (p) T(static_cast<T&&>(u));
      }

      virtual pointer_lcm clone_lcm(void* p) override {
        return new (p) ClonableT;
      }
    };

    new (lcm_storage) ClonableT();
    get_lcm()->clone_obj(&storage, std::move(t));
    is_empty = false;
  }

  ~static_ptr() {
    auto obj = get();
    if (obj) {
      obj->~TypeT();
    }
  }

  TypeT* operator->() {
    return get();
  }

  pointer get() noexcept {
    return is_empty ? nullptr : reinterpret_cast<pointer>(storage);
  }

};
