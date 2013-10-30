// Emacs Mode Line: -*- Mode:c++;-*-
// -------------------------------------------------------------
/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */
// -------------------------------------------------------------
/**
 * @file   math.hpp
 * @author William A. Perkins
 * @date   2013-10-09 12:23:41 d3g096
 * 
 * @brief  
 * 
 * 
 */
// -------------------------------------------------------------
#ifndef _math_hpp_
#define _math_hpp_

namespace gridpack {
namespace math {

// Do whatever is necessary to initialize the math library
extern void Initialize(void);

/// Is the math library initialized?
extern bool Initialized(void);

/// Do whatever is necessary to shut down the math library
extern void Finalize(void);

} // namespace math
} // namespace gridpack


#endif
