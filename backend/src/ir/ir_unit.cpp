/* 
 * Copyright © 2012 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Segovia <benjamin.segovia@intel.com>
 */

#include "ir_unit.hpp"
#include "ir_function.hpp"

namespace gbe
{
  Unit::Unit(void) {}
  Unit::~Unit(void) {
    for (auto it = functions.begin(); it != functions.end(); ++it)
      GBE_DELETE(it->second);
  }
  Function *Unit::getFunction(const std::string &name) const {
    auto it = functions.find(name);
    if (it == functions.end())
      return NULL;
    return it->second;
  }
  Function *Unit::newFunction(const std::string &name) {
    auto it = functions.find(name);
    if (it != functions.end())
      return NULL;
    Function *fn = GBE_NEW(Function);
    functions[name] = fn;
    return fn;
  }
  void Unit::newConstant(const char *data,
                         const std::string &name,
                         uint32 size,
                         uint32 alignment)
  {
    constantSet.append(data, name, size, alignment);
  }

} /* namespace gbe */

