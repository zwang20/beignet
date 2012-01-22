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

#ifndef __DEFAULT_PATH_HPP__
#define __DEFAULT_PATH_HPP__

#include <cstdlib>

namespace gbe
{
  /*! Where you may find data files and shaders */
  extern const char *defaultPath[];
  /*! Number of default paths */
  extern const size_t defaultPathNum;
} /* namespace gbe */

#endif /* __DEFAULT_PATH_HPP__ */

