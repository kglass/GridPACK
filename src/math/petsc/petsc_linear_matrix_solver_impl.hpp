// Emacs Mode Line: -*- Mode:c++;-*-
// -------------------------------------------------------------
/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */
// -------------------------------------------------------------
// -------------------------------------------------------------
/**
 * @file   petsc_linear_matrx_solver_impl.hpp
 * @author William A. Perkins
 * @date   2014-11-26 11:13:21 d3g096
 * 
 * @brief  
 * 
 * 
 */
// -------------------------------------------------------------

#ifndef _petsc_linear_matrx_solver_impl_hpp_
#define _petsc_linear_matrx_solver_impl_hpp_

#include <petscmat.h>
#include "linear_matrix_solver_implementation.hpp"
#include "petsc_configurable.hpp"

namespace gridpack {
namespace math {

// -------------------------------------------------------------
//  class PetscLinearMatrixSolverImplementation
// -------------------------------------------------------------
class PetscLinearMatrixSolverImplementation 
  : public LinearMatrixSolverImplementation,
    private PETScConfigurable
{
public:

  /// Default constructor.
  PetscLinearMatrixSolverImplementation(const Matrix& A);

  /// Destructor
  ~PetscLinearMatrixSolverImplementation(void);

protected:

  /// The underlying PETSc factored coefficient matrix
  mutable Mat p_Fmat;

  /// Is p_Fmat ready?
  mutable bool p_factored;

  /// List of supported matrix ordering
  static MatOrderingType p_supportedOrderingType[];

  /// PETSc matrix ordering type
  MatOrderingType p_orderingType;

  /// List of supported solver packages
  static MatSolverPackage p_supportedSolverPackage[];

  /// PETSC solver package for factorization
  MatSolverPackage p_solverPackage;

  /// PETSc factorization method to use
  MatFactorType p_factorType;

  /// Fill levels to use in decomposition
  int p_fill;

  /// Flag to enable pivoting
  bool p_pivot;

  /// Do what is necessary to build this instance
  void p_build(const std::string& option_prefix);

  /// Specialized way to configure from property tree
  void p_configure(utility::Configuration::CursorPtr props);

  /// Factor the coefficient matrix
  void p_factor(void) const;

  /// Solve w/ the specified RHS Matrix (specialized)
  Matrix *p_solve(const Matrix& B) const;

};


} // namespace math
} // namespace gridpack


#endif
