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


template <class TypeT, size_t MaxSize>
class static_ptr {
  TypeT* ptr = nullptr;
  unsigned char storage[MaxSize];

public:
  static_ptr() = default;

  template <class T>
  static_ptr(const T& t) {
    ptr = new (storage) T(t);
  }

  ~static_ptr() {
    if (ptr) {
      ptr->~TypeT();
    }
  }

  TypeT* operator->() {
    return ptr;
  }
};
