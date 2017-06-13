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
  Base2() {
    std::cout << "Base2 cted" << std::endl;
  }
  Base2(const Base2&) {
    std::cout << "Base2 copy cted" << std::endl;
  }

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

#if 1
  // cannot use get_max_size here due to a language quirk
  static static_ptr<Interface, max_size> make_instance(bool first_one) {
    if (first_one) {
      return make_static<Base1>();
    } else {
      return make_static<Base2>();
    }
  }
#endif
};


struct duda;
struct duda {
  static_ptr<duda, 100> prev;
  static_ptr<duda, 100> next;
} pisodlug;

int main (void) {
  std::cout << "maxsizeof: " << maxsizeof<Factory>() << std::endl;
  std::cout << "maxsizeof: " << maxsizeof<Base1, Base2>() << std::endl;

  std::cout << "max size: " << Factory::get_max_size() << std::endl;

  static_ptr<Interface, Factory::get_max_size()> ptr1 = Base1();
  static_ptr<Interface, Factory::get_max_size()> ptr2 = Base2();
  static_ptr<Interface, Factory::get_max_size()> ptr11 = Base1();

  
  static_ptr<static_ptr<Interface, Factory::get_max_size()>, 200> ptrXX(
    static_ptr<Interface, Factory::get_max_size()>(Base1()));
  //ptrXX->print_name();
  //(*ptrXX)->print_name();
  //(*ptrXX.get())->print_name();

  static_ptr<duda, 100> dudus;
  dudus = std::move(pisodlug.next);

  ptr1->print_name();
  ptr2->print_name();
  ptr2->print_name();
  ptr2->print_name();
  ptr2->print_name();
  ptr2->print_name();
  ptr2->print_name();
  ptr2->print_name();
  ptr2->print_name();

  std::cout << "testing move ctor of static_ptr" << std::endl;
  auto ptrf = Factory::make_instance(false);
  ptrf->print_name();

  auto ptr_copy = std::move(ptrf);
  static_ptr<Interface, 2 * Factory::get_max_size()> big_ptr = Base2();
  std::cout << "testing move ctor of static_ptr<, * 3>" << std::endl;
  static_ptr<Interface, 3 * Factory::get_max_size()> big_ptr_m = std::move(big_ptr);
  //static_ptr<Interface, 1 * Factory::get_max_size()> small_ptr_m = std::move(big_ptr);

  return 0;
}
