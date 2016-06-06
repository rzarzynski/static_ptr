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
#include <tuple>


template <class TypeT, size_t MaxSize>
class static_ptr {
  /* All variants of static_ptr are friends. */
  template <class Tf, size_t Sf> friend class static_ptr;
public:
  /* Public typedefs and constants. */
  typedef TypeT* pointer;
  typedef TypeT element_type;
  static constexpr size_t element_max_size = MaxSize;

private:
  /* The life cycle manager interface for creation and deletion of objects that
   * static_ptr stores. Although the deletion part allows us to not enforce
   * element_type to have its destructor virtual (unlike the std::unique_ptr),
   * the main motivation here are virtual constructors. The language doesn't
   * offer them, but they are essential for the move-construct and move-assign
   * operations.
   * In contrast to std::unique_ptr and std::shared_ptr, static_ptr's lifetime
   * is tied to lifetime of an object it has. In consequence, it's not enough to
   * copy a pointer nor even memcpy the whole storage (this would be sufficient
   * for PODs only). We need to properly move-construct a new instance each time
   * someone does:
   *
   *     ptrA = std::move(ptrB);
   *     // or
   *     ptrC(std::move(ptrD);
   *
   * Moreover, static_ptr doesn't directly have information about exact types
   * of the std::moved objects - they can be anything derived from element_type.
   * We address this by constructing LCM for each concrete type (see _emplace)
   * and storing it internally (usually very cheap as LCM needs a VPTR only).
   *
   * TODO(rzarzynski): add support for externally-provided LCM implementation.
   * This would allow for custom deleter. */
  struct LifeCycleManager {
    virtual void clone_obj(void *, element_type&&) const = 0;
    virtual void clone_lcm(void *) const = 0;

    virtual void delete_obj(element_type&) const = 0;
    /* TODO(rzarzynski): take care of LCM cleaning. */
  };

  mutable typename std::aligned_storage<element_max_size>::type storage_obj;
  typename std::aligned_storage<sizeof(LifeCycleManager)>::type storage_lcm;
  bool is_empty = true;

  const LifeCycleManager& get_lcm() noexcept {
    return reinterpret_cast<const LifeCycleManager&>(storage_lcm);
  }

  /* In-place construct a new object of the Te type as well as a life cycle
   * manager dedicated to this particular type. Forward all arguments to Te's
   * constructor. Te must be compatible with the element_type. */
  template <
    class Te,
    class... Args,
    /* Dummy template parameter solely for SFINAE. */
    typename std::enable_if<
      std::is_base_of<element_type, Te>::value>::type* = nullptr >
  void _emplace(Args&&... args) {
    /* Life cycle manager for the type Te. Due to nesting this class, each
     * instance of the emplace template will receive its own LCM with info
     * about the concrete Te deeply buried inside. That's the way how we
     * support "virtual constructors" and call a proper destructor even if
     * element_type hasn't declared its dtor as virtual. */
    struct LCMe : public LifeCycleManager {
      virtual void clone_obj(void* p, element_type&& u) const override {
        new (p) Te(static_cast<Te&&>(u));
      }

      virtual void clone_lcm(void* p) const override {
        new (p) LCMe;
      }

      virtual void delete_obj(element_type& t) const override {
        static_cast<Te&>(t).~Te();
      }
    };

    /* The storage_lcm shouldn't store anything more than the VPTR. */
    new (&storage_lcm) LCMe();
    new (&storage_obj) Te(std::forward<Args>(args)...);
    is_empty = false;
  }

  /* Helpers for make_obj. We need them only because std::apply will come in
   * C++17. I know that they seem to be a black magic's spell. However, the idea
   * is awfully simple: take a std::tuple<> and apply its elements as arguments
   * to a function. It resembles Python's func(*[arg1, arg2, ...]). */
  template<int ...> struct seq { };

  template<int N, int ...S>
  struct gens : gens<N-1, N-1, S...> { };

  template<int ...S>
  struct gens<1, S...> {
    typedef seq<S...> type;
  };

  template<class Type, class Tuple, int ...S>
  void _make_obj(Tuple&& tup, seq<S...>) {
    _emplace<Type>(std::get<S>(tup) ...);
  }

  template <class Tf, size_t Sf>
  void _transfer_obj(static_ptr<Tf, Sf>&& rhs) {
    typename static_ptr<Tf, Sf>::pointer rhs_obj_ptr = rhs.get();

    if (rhs_obj_ptr) {
      rhs.get_lcm().clone_obj(&storage_obj, std::move(*rhs_obj_ptr));
      rhs.get_lcm().clone_lcm(&storage_lcm);

      /* Using the already std::moved rhs_obj_ptr is fully intensional. */
      rhs.is_empty = true;
      rhs.get_lcm().delete_obj(*rhs_obj_ptr);

      this->is_empty = false;
    }
  }

public:
  /* All necessary things are initialized in the definitions. NOTE: we won't
   * zeroize or touch the storage in any other way as the only thing it could
   * bring is an impact on performance. */
  static_ptr() noexcept = default;

  /* Constructor: the nullptr case. */
  static_ptr(nullptr_t) noexcept : static_ptr() {};

  /* Constructor: move from another instance of absolutely the same variant of
   * static_ptr. In other words, rhs must be an instance
   * of static_ptr<element_type, element_max_size). The constructor is present
   * because of the Return Value Optimization.  */
  static_ptr(static_ptr&& rhs) {
    _transfer_obj(std::move(rhs));
  }

  /* Constructor: move from a compatible variant of static_ptr. Variants
   * are compatible only if:
   *  1) a source one is smaller or equal in terms of available storage
   *     size than a destination one AND
   *  2) a destination one encapsulates a type that stays in is_base_of
   *     relationship with type stored by a source one. */
  template <class Tf, size_t Sf>
  static_ptr(static_ptr<Tf, Sf>&& rhs) {
    static_assert(element_max_size >= Sf,
                  "constructed from too big static_ptr instance");
    static_assert(std::is_base_of<element_type, Tf>::value,
                  "constructed from non-related static_ptr instance");

    _transfer_obj(std::move(rhs));
  }

  /* Constructor: the from-make_static case. */
  template <class T>
  static_ptr(T&& tup) {
    using TypePtr = typename std::tuple_element<0, T>::type;
    using Type    = typename std::remove_pointer<TypePtr>::type;

    constexpr size_t tup_size = std::tuple_size<T>::value;

    _make_obj<Type>(std::move(tup), typename gens<tup_size>::type());
  }

  /* Assignment: move from a compatible variant of static_ptr. For details
   * please refer to the documentation of the corresponding constructor. */
  template <class Tf, size_t Sf>
  static_ptr& operator=(static_ptr<Tf, Sf>&& rhs) {
    static_assert(element_max_size >= Sf,
                  "assigned from too big static_ptr instance");
    static_assert(std::is_base_of<element_type, Tf>::value,
                  "assigned from non-related static_ptr instance");

    /* First, release (destroy) the currently stored object if necessary. */
    pointer this_obj_ptr = this->get();
    if (this_obj_ptr) {
      this->is_empty = true;
      this->get_lcm().delete_obj(*this_obj_ptr);
    }

    /* Second, MoveConstruct a new object using our own storage but basing
     * on the object hold by rhs. */
    _transfer_obj(std::move(rhs));

    return *this;
  }

  /* Let's mimic the std::unique_ptr's behaviour. It's very useful to say to
   * the world who controls the lifetime of the contained object. */
  static_ptr(const static_ptr&) = delete;
  static_ptr& operator=(const static_ptr&) = delete;

  ~static_ptr() {
    auto obj = get();
    if (obj) {
      get_lcm().delete_obj(*obj);
    }
  }

  pointer operator->() const {
    return get();
  }

  element_type& operator*() const {
    return *get();
  }

  pointer get() const noexcept {
    return is_empty ? nullptr : reinterpret_cast<pointer>(&storage_obj);
  }

  template <
    class Te,
    class... Args,
    /* Dummy template parameter solely for SFINAE. */
    typename std::enable_if<
      std::is_base_of<element_type, Te>::value>::type* = nullptr >
  bool emplace(Args&&... args) {
    /* The public emplace method can be called on empty static_pointer only. */
    if (is_empty) {
      _emplace<Te>(std::forward<Args>(args)...);
    }
    return is_empty;
  }
};


/* maxsizeof is a helper function to deduce maximum size of all its parameters
 * at compile-time. It makes defining factories returning static_ptr easier and
 * more compact. */
template <class First>
constexpr size_t maxsizeof() {
  return sizeof(First);
}

template <class First, class Second, class... Tail>
constexpr size_t maxsizeof() {
  return maxsizeof<First>() > maxsizeof<Second, Tail...>()
                            ? maxsizeof<First>()
                            : maxsizeof<Second, Tail...>();
}


/* C++ doesn't allow to explicitly specify parameters for template constructor
 * of a class template. They can be deduced only. Because of the restriction we
 * need a helper function to construct an instance of a concrete type directly
 * in the memory under static_ptr::storage_obj - without spawning any temporary
 * of static_ptr::element_type-or-derived along the way.
 *
 * The idea is to prepare a std::tuple containing all information necessary to
 * create an object in its final destination and pass it to specific template
 * constructor of static_ptr.
 * The std::tuple object should be optimized out thanks to the RVO.
 *
 * Another benefit is the similarity to std::make_shared and std::make_unique
 * functions. */
template <class T, class... Args>
std::tuple<T*, Args...> make_static(Args&&... args)
{
  /* Let's forward the information about the concrete type to in-place create
   * in static_ptr as a fake null pointer. Its type will be burried in the tuple. */
  return std::forward_as_tuple(static_cast<T*>(nullptr),
                               std::forward<Args>(args)...);
}
