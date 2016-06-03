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

struct Interface {
  /* NOTE: the destructor is NOT virtual. */
  virtual const char* get_name() const = 0;
};

struct ConcreteA : public Interface {
  long member[4];
  const char* get_name() const override { return "ConcreteA"; }
};

struct ConcreteB : public Interface {
  long member[4];
  const char* get_name() const override { return "ConcreteB"; }
  ~ConcreteB() { std::cout << "ConcreteB destructed" << std::endl; }
};

struct Factory {
  static static_ptr<Interface,
                    maxsizeof<ConcreteA, ConcreteB>() >
  make_instance(bool first_one) {
    if (first_one) {
      return ConcreteA();
    } else {
      return ConcreteB();
    }
  }
};


int main (void) {
  auto ptrA = Factory::make_instance(true);
  auto ptrB = Factory::make_instance(false);

  /* Result: ptrA->get_name(): ConcreteA */
  std::cout << "ptrA->get_name(): " << ptrA->get_name() << std::endl;

  /* Result:
   *  ptrB->get_name(): ConcreteB
   *  ConcreteB destructed */
  std::cout << "ptrB->get_name(): " << ptrB->get_name() << std::endl;

  return 0;
}
