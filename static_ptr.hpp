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
  /* All variants of static_ptr are friends. */
  template <class Tf, size_t Sf> friend class static_ptr;
public:
  typedef TypeT* pointer;
  typedef TypeT element_type;

private:
  /* Virtual constructors for C++... */
  struct LifeCycleManager {
    virtual void clone_obj(void *, TypeT&&) const = 0;
    virtual void clone_lcm(void *) const = 0;

    virtual void delete_obj(TypeT&) const = 0;
  };

  typedef const LifeCycleManager* pointer_lcm;

  unsigned char storage[MaxSize];
  unsigned char storage_lcm[sizeof(LifeCycleManager)];
  bool is_empty = true;

  pointer_lcm get_lcm() noexcept {
    return reinterpret_cast<pointer_lcm>(storage_lcm);
  }

  template <
    class Te,
    class... Args,
    /* Dummy template parameter solely for SFINAE. */
    typename std::enable_if<
      std::is_base_of<TypeT, Te>::value>::type* = nullptr >
  void _emplace(Args&&... args) {
    /* Life cycle manager for the type Te. Due to nesting this class, each
     * instance of the emplace template will receive its own LCM with info
     * about the concrete Te deeply buried inside. That's the way how we
     * support "virtual constructors" and call a proper destructor even if
     * TypeT hasn't declared its dtor as virtual. */
    struct LCMe : public LifeCycleManager {
      virtual void clone_obj(void* p, TypeT&& u) const override {
        new (p) Te(static_cast<Te&&>(u));
      }

      virtual void clone_lcm(void* p) const override {
        new (p) LCMe;
      }

      virtual void delete_obj(TypeT& t) const override {
        static_cast<Te&>(t).~Te();
      }
    };

    /* The storage_lcm shouldn't store anything more than the VPTR. */
    new (storage_lcm) LCMe();
    new (storage) Te(std::forward<Args>(args)...);
    is_empty = false;
  }

  template <class Tf, size_t Sf>
  void _clone(static_ptr<Tf, Sf>&& rhs) {
    typename static_ptr<Tf, Sf>::pointer rhs_obj_ptr = rhs.get();

    if (rhs_obj_ptr) {
      rhs.get_lcm()->clone_obj(storage, std::move(*rhs_obj_ptr));
      rhs.get_lcm()->clone_lcm(storage_lcm);

      /* Using the already std::moved rhs_obj_ptr is fully intensional. */
      rhs.get_lcm()->delete_obj(*rhs_obj_ptr);
    }

    this->is_empty = rhs.is_empty;
  }

public:
  /* All necessary things are initialized in the definitions. NOTE: we won't
   * zeroize or touch the storage in any other way as the only thing it could
   * bring is an impact on performance. */
  static_ptr() noexcept = default;

  /* Constructor: the nullptr case. */
  static_ptr(nullptr_t) noexcept : static_ptr() {};

  /* Constructor: move from a compatible variant of static_ptr. Variants
   * are compatible only if:
   *  1) a source one is smaller or equal in terms of available storage
   *     size than a destination one AND
   *  2) a destination one encapsulates a type that stays in is_base_of
   *     relationship with type stored by a source one. */
  template <class Tf, size_t Sf>
  static_ptr(static_ptr<Tf, Sf>&& rhs) {
    static_assert(MaxSize >= Sf,
                  "constructed from too big static_ptr instance");
    static_assert(std::is_base_of<TypeT, Tf>::value,
                  "constructed from non-related static_ptr instance");

    _clone(std::move(rhs));
  }

  /* Constructor: the emplace caller. */
  template <
    class Tn,
    typename std::enable_if<
      std::is_base_of<TypeT, Tn>::value>::type* = nullptr >
  static_ptr(Tn&& t) {
    _emplace<Tn>(std::move(t));
  }

  /* Assignment: move from a compatible variant of static_ptr. For details
   * please refer to the documentation of the corresponding constructor. */
  template <class Tf, size_t Sf>
  static_ptr& operator=(static_ptr<Tf, Sf>&& rhs) {
    static_assert(MaxSize >= Sf,
                  "assigned from too big static_ptr instance");
    static_assert(std::is_base_of<TypeT, Tf>::value,
                  "assigned from non-related static_ptr instance");

    /* First, release (destroy) the currently stored object if necessary. */
    pointer this_obj_ptr = this->get();
    if (this_obj_ptr) {
      this->get_lcm()->delete_obj(*this_obj_ptr);
    }

    /* Second, MoveConstruct a new object using our own storage but basing
     * on the object hold by rhs. */
    _clone(std::move(rhs));

    return *this;
  }

  /* Let's mimic the std::unique_ptr's behaviour. It's very useful to say to
   * the world who controls the lifetime of the contained object. */
  static_ptr(const static_ptr&) = delete;
  static_ptr& operator=(const static_ptr&) = delete;


  ~static_ptr() {
    auto obj = get();
    if (obj) {
      get_lcm()->delete_obj(*obj);
    }
  }

  TypeT* operator->() {
    return get();
  }

  pointer get() noexcept {
    return is_empty ? nullptr : reinterpret_cast<pointer>(storage);
  }

  template <
    class Te,
    class... Args,
    /* Dummy template parameter solely for SFINAE. */
    typename std::enable_if<
      std::is_base_of<TypeT, Te>::value>::type* = nullptr >
  bool emplace(Args&&... args) {
    /* The public emplace method can be called on empty static_pointer only. */
    if (is_empty) {
      _emplace<Te>(std::forward<Args>(args)...);
    }
    return is_empty;
  }
};
