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

#include <iostream>

#include "static_ptr.hpp"

class Interface {
public:
//  virtual ~Interface() {};
  virtual void print_name() const noexcept = 0;
};

class Base1 : public Interface {
  long member[2];
public:
  virtual void print_name() const noexcept override {
    std::cout << "Base1" << std::endl;
  }
};

class Base2 : public Interface {
  long member[4];
public:
  virtual ~Base2() {
    std::cout << "Base2 dted" << std::endl;
  }

  virtual void print_name() const noexcept override {
    std::cout << "Base2" << std::endl;
  }
};


class Factory {
  static constexpr size_t max_size = sizeof(Base1) > sizeof(Base2) ? sizeof(Base1) : sizeof(Base2);
public:
  static constexpr size_t get_max_size() {
    return max_size;
  }

  // cannot use get_max_size here due to a language quirk
  static static_ptr<Interface, max_size> make_instance(bool first_one) {
    if (first_one) {
      return Base1();
    } else {
      return Base2();
    }
  }
};


int main (void) {
  std::cout << "max size: " << Factory::get_max_size() << std::endl;

  static_ptr<Interface, Factory::get_max_size()> ptr = Base2();
  ptr->print_name();

  return 0;
}
