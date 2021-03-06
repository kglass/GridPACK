/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */
// -------------------------------------------------------------
/**
 * @file   pf_components.cpp
 * @author Bruce Palmer
 * @date   2014-02-13 07:35:47 d3g096
 * 
 * @brief  
 * 
 * 
 */
// -------------------------------------------------------------

#include <vector>
#include <iostream>
#include <cstring>
#include <stdio.h>

#include "boost/smart_ptr/shared_ptr.hpp"
#include "gridpack/utilities/complex.hpp"
#include "gridpack/component/base_component.hpp"
#include "gridpack/component/data_collection.hpp"
#include "pf_components.hpp"
#include "gridpack/parser/dictionary.hpp"

//#define LARGE_MATRIX

/**
 *  Simple constructor
 */
gridpack::powerflow::PFBus::PFBus(void)
{
  p_shunt_gs = 0.0;
  p_shunt_bs = 0.0;
  p_v = 0.0;
  p_a = 0.0;
  p_theta = 0.0;
  p_angle = 0.0;
  p_voltage = 0.0;
  p_pl = 0.0;
  p_ql = 0.0;
  p_sbase = 0.0;
  p_mode = YBus;
  setReferenceBus(false);
}

/**
 *  Simple destructor
 */
gridpack::powerflow::PFBus::~PFBus(void)
{
}

/**
 *  Return size of matrix block contributed by the component
 *  @param isize, jsize: number of rows and columns of matrix block
 *  @return: false if network component does not contribute matrix element
 */
bool gridpack::powerflow::PFBus::matrixDiagSize(int *isize, int *jsize) const
{
  if (p_mode == Jacobian) {
    if (!isIsolated()) {
#ifdef LARGE_MATRIX
      *isize = 2;
      *jsize = 2;
      return true;
#else
      if (getReferenceBus()) {
        return false;
      } else if (p_isPV) {
        *isize = 1;
        *jsize = 1;
        return true;
      } else {
        *isize = 2;
        *jsize = 2;
        return true;
      }
#endif
    } else {
      return false;
    }
  } else if (p_mode == YBus) {
    return YMBus::matrixDiagSize(isize,jsize);
  }
  return true;
}

/**
 * Return the values of the matrix block. The values are
 * returned in row-major order.
 * @param values: pointer to matrix block values
 * @return: false if network component does not contribute matrix element
 */
bool gridpack::powerflow::PFBus::matrixDiagValues(ComplexType *values)
{
  if (p_mode == YBus) {
    return YMBus::matrixDiagValues(values);
  } else if (p_mode == Jacobian) {
    if (!isIsolated()) {
#ifdef LARGE_MATRIX
      if (!getReferenceBus()) {
        values[0] = -p_Qinj - p_ybusi * p_v *p_v; 
        values[1] = p_Pinj - p_ybusr * p_v *p_v; 
        values[2] = p_Pinj / p_v + p_ybusr * p_v; 
        values[3] = p_Qinj / p_v - p_ybusi * p_v; 
        // Fix up matrix elements if bus is PV bus
        if (p_isPV) {
          values[1] = 0.0;
          values[2] = 0.0;
          values[3] = 1.0;
        }
        return true;
      } else {
        values[0] = 1.0;
        values[1] = 0.0;
        values[2] = 0.0;
        values[3] = 1.0;
        return true;
      }
#else
      if (!getReferenceBus() && !p_isPV) {
        values[0] = -p_Qinj - p_ybusi * p_v *p_v; 
        values[1] = p_Pinj - p_ybusr * p_v *p_v; 
        values[2] = p_Pinj / p_v + p_ybusr * p_v; 
        values[3] = p_Qinj / p_v - p_ybusi * p_v; 
        // Fix up matrix elements if bus is PV bus
        return true;
      } else if (!getReferenceBus() && p_isPV) {
        values[0] = -p_Qinj - p_ybusi * p_v *p_v; 
        return true;
      } else {
        return false;
      }
#endif
    } else {
      return false;
    }
  }
}

/**
 * Return the size of the block that this component contributes to the
 * vector
 * @param size: size of vector block
 * @return: false if component does not contribute to vector
 */
bool gridpack::powerflow::PFBus::vectorSize(int *size) const
{
  if (p_mode == RHS || p_mode == State) {
    if (!isIsolated()) {
#ifdef LARGE_MATRIX
      *size = 2;
      return true;
#else
      if (getReferenceBus()) {
        return false;
      } else if (p_isPV) {
        *size = 1;
      } else {
        *size = 2;
      }
      return true;
#endif
    } else {
      return false;
    }
  } else if (p_mode == S_Cal){
    *size = 1;
  } else {
    *size = 2;
  }
  return true;
}

/**
 * Return the values of the vector block
 * @param values: pointer to vector values
 * @return: false if network component does not contribute
 *        vector element
 */
bool gridpack::powerflow::PFBus::vectorValues(ComplexType *values)
{
  if (p_mode == S_Cal)  {
    double retr = p_v * cos(p_a);
    double reti = p_v * sin(p_a);
    gridpack::ComplexType ret(retr, reti);
    values[0] = ret;
    return true;
  }
  if (p_mode == State) {
    values[0] = p_v;
    values[1] = p_a;
    return true;
  }
  if (p_mode == RHS) {
    if (!isIsolated()) {
      if (!getReferenceBus()) {
        std::vector<boost::shared_ptr<BaseComponent> > branches;
        getNeighborBranches(branches);
        int size = branches.size();
        int i;
        double P, Q, p, q;
        P = 0.0;
        Q = 0.0;
        for (i=0; i<size; i++) {
          gridpack::powerflow::PFBranch *branch
            = dynamic_cast<gridpack::powerflow::PFBranch*>(branches[i].get());
          branch->getPQ(this, &p, &q);
          P += p;
          Q += q;
        }
        // Also add bus i's own Pi, Qi
        P += p_v*p_v*p_ybusr;
        Q += p_v*p_v*(-p_ybusi);
        p_Pinj = P;
        p_Qinj = Q;
        P -= p_P0;
        Q -= p_Q0;
        values[0] = P;
#ifdef LARGE_MATRIX
        if (!p_isPV) {
          values[1] = Q;
        } else {
          values[1] = 0.0;
        }
#else
        if (!p_isPV) {
          values[1] = Q;
        }
#endif
        return true;
      } else {
#ifdef LARGE_MATRIX
        values[0] = 0.0;
        values[1] = 0.0;
        return true;
#else
        return false;
#endif
      }
    } else {
      return false;
    }
  }
 }


/**
 * Check QLIM
 * @return false: violations exist
 * @return true:  no violations
 * 
 */
bool gridpack::powerflow::PFBus::chkQlim(void)
{
//    printf("p_isPV = %d Gen %d exceeds the QMAX limit %f \n", p_isPV, getOriginalIndex(),(p_Qinj-p_Q0)*p_sbase);
    if (p_isPV) {
      double qmax, qmin;
      qmax = 0.0;
      qmin = 0.0;
      for (int i=0; i<p_gstatus.size(); i++) {
        if (p_gstatus[i] == 1) {
          qmax += p_qmax[i];
          qmin += p_qmin[i];
        }
      }
      printf(" PV Check: Gen %d =, p_ql = %f, QMAX = %f\n", getOriginalIndex(),p_ql, qmax);
      std::vector<boost::shared_ptr<BaseComponent> > branches;
      getNeighborBranches(branches);
      int size = branches.size();
      int i;
      double P, Q, p, q;
      P = 0.0;
      Q = 0.0;
      for (i=0; i<size; i++) {
        gridpack::powerflow::PFBranch *branch
          = dynamic_cast<gridpack::powerflow::PFBranch*>(branches[i].get());
        branch->getPQ(this, &p, &q);
        P += p;
        Q += q;
      }

      printf("Gen %d: Q = %f, p_QL = %f, Q+p_Q0 = %f, QMAX = %f \n", getOriginalIndex(),-Q,p_ql,-Q+p_ql, qmax);  
//      if (-Q > qmax ) { 
      if (-Q+p_ql > qmax ) { 
        printf("Gen %d exceeds the QMAX limit %f vs %f\n", getOriginalIndex(),-Q+p_ql, qmax);  
        p_ql = p_ql+qmax;
        p_isPV = 0;
        for (int i=0; i<p_gstatus.size(); i++) {
          p_gstatus[i] = 0;
        }
        return true;
      //} else if (-Q < qmin) {
      } else if (-Q+p_ql < qmin) {
        printf("Gen %d exceeds the QMIN limit %f vs %f\n", getOriginalIndex(),-Q+p_ql, qmin);  
        p_ql = p_ql+qmin;
        p_isPV = 0;
        for (int i=0; i<p_gstatus.size(); i++) {
          p_gstatus[i] = 0;
        }
        return true;
      } else {
        return false;
      }
    } else {
      printf(" PQ Check: bus: %d, p_ql = %f\n", getOriginalIndex(),p_ql);
      return false;
    }
}

/**
 * Set the internal values of the voltage magnitude and phase angle. Need this
 * function to push values from vectors back onto buses 
 * @param values array containing voltage magnitude and angle
 */
void gridpack::powerflow::PFBus::setValues(gridpack::ComplexType *values)
{
  double vt = p_v;
  double at = p_a;
  p_a -= real(values[0]);
#ifdef LARGE_MATRIX
  p_v -= real(values[1]);
#else
  if (!p_isPV) {
    p_v -= real(values[1]);
  }
#endif
  *p_vAng_ptr = p_a;
  *p_vMag_ptr = p_v;
}

/**
 * Return the size of the buffer used in data exchanges on the network.
 * For this problem, the voltage magnitude and phase angle need to be exchanged
 * @return size of buffer
 */
int gridpack::powerflow::PFBus::getXCBufSize(void)
{
  return 2*sizeof(double);
}

/**
 * Assign pointers for voltage magnitude and phase angle
 */
void gridpack::powerflow::PFBus::setXCBuf(void *buf)
{
  p_vAng_ptr = static_cast<double*>(buf);
  p_vMag_ptr = p_vAng_ptr+1;
  // Note: we are assuming that the load function has been called BEFORE
  // the factory setExchange method, so p_a and p_v are set with their initial
  // values.
  *p_vAng_ptr = p_a;
  *p_vMag_ptr = p_v;
}

/**
 * Load values stored in DataCollection object into PFBus object. The
 * DataCollection object will have been filled when the network was created
 * from an external configuration file
 * @param data: DataCollection object contain parameters relevant to this
 *       bus that were read in when network was initialized
 */
void gridpack::powerflow::PFBus::load(
    const boost::shared_ptr<gridpack::component::DataCollection> &data)
{
  YMBus::load(data);

  bool ok = data->getValue(CASE_SBASE, &p_sbase);
  data->getValue(BUS_VOLTAGE_ANG, &p_angle);
  data->getValue(BUS_VOLTAGE_MAG, &p_voltage); 
  p_v = p_voltage;
  double pi = 4.0*atan(1.0);
  p_angle = p_angle*pi/180.0;
  p_a = p_angle;
  int itype;
  data->getValue(BUS_TYPE, &itype);
  if (itype == 3) {
    setReferenceBus(true);
  }

  // if BUS_TYPE = 2, and gstatus is 1, then bus is a PV bus
  p_isPV = false;

  // added p_pg,p_qg,p_pl,p_ql,p_sbase;
  p_load = true;
  p_load = p_load && data->getValue(LOAD_PL, &p_pl);
  p_load = p_load && data->getValue(LOAD_QL, &p_ql);
  bool lgen;
  int i, ngen, gstatus;
  double pg, qg, vs,qmax,qmin;
  ngen = 0;
  if (data->getValue(GENERATOR_NUMBER, &ngen)) {
    for (i=0; i<ngen; i++) {
      lgen = true;
      lgen = lgen && data->getValue(GENERATOR_PG, &pg,i);
      lgen = lgen && data->getValue(GENERATOR_QG, &qg,i);
      lgen = lgen && data->getValue(GENERATOR_VS, &vs,i);
      lgen = lgen && data->getValue(GENERATOR_STAT, &gstatus,i);
      lgen = lgen && data->getValue(GENERATOR_QMAX, &qmax,i);
      lgen = lgen && data->getValue(GENERATOR_QMIN, &qmin,i);
      if (lgen) {
        p_pg.push_back(pg);
        p_qg.push_back(qg);
        p_gstatus.push_back(gstatus);
        p_qmax.push_back(qmax);
        p_qmin.push_back(qmin);
        if (gstatus == 1) {
          p_v = vs; //reset initial PV voltage to set voltage
          if (itype == 2) p_isPV = true;
        }
        std::string id("-1");
        data->getValue(GENERATOR_ID,&id,i);
        p_gid.push_back(id);
      }
    }
  }
  p_saveisPV = p_isPV;
}

/**
 * Set values of YBus matrix. These can then be used in subsequent
 * calculations
 */
void gridpack::powerflow::PFBus::setYBus(void)
{
  YMBus::setYBus();
  gridpack::ComplexType ret;
  ret = YMBus::getYBus();
  p_ybusr = real(ret);
  p_ybusi = imag(ret);
}

/**
 * Get values of YBus matrix. These can then be used in subsequent
 * calculations
 */
gridpack::ComplexType gridpack::powerflow::PFBus::getYBus(void)
{
  return YMBus::getYBus();
}

/**
 * Set the mode to control what matrices and vectors are built when using
 * the mapper
 * @param mode: enumerated constant for different modes
 */
void gridpack::powerflow::PFBus::setMode(int mode)
{
  if (mode == YBus) {
    YMBus::setMode(gridpack::ymatrix::YBus);
  }
  p_mode = mode;
}

/**
 * Reset voltage and phase angle to initial values
 */
void gridpack::powerflow::PFBus::resetVoltage(void)
{
  p_v = p_voltage;
  p_a = p_angle;
  *p_vMag_ptr = p_v;
  *p_vAng_ptr = p_a;
}

/**
 * Return the value of the voltage magnitude on this bus
 * @return: voltage magnitude
 */
double gridpack::powerflow::PFBus::getVoltage()
{
  return *p_vMag_ptr;
}

/**
 * Return whether or not the bus is a PV bus (V held fixed in powerflow
 * equations)
 * @return true if bus is PV bus
 */
bool gridpack::powerflow::PFBus::isPV(void)
{
  return p_isPV;
}

/**
 * Return the value of the phase angle on this bus
 * @return: phase angle
 */
double gridpack::powerflow::PFBus::getPhase()
{
  return *p_vAng_ptr;
}

/**
 * Get generator status
 * @return vector of generator statuses
 */
std::vector<int> gridpack::powerflow::PFBus::getGenStatus()
{
  return p_gstatus;
}

/**
 * Get list of generator IDs
 * @return vector of generator IDs
 */
std::vector<std::string> gridpack::powerflow::PFBus::getGenerators()
{
  return p_gid;
}

/**
 * Set generator status
 * @param gen_id generator ID
 * @param status generator status
 */
void gridpack::powerflow::PFBus::setGenStatus(std::string gen_id, int status)
{
  int i;
  for (i=0; i<p_gstatus.size(); i++) {
    if (gen_id == p_gid[i]) {
      p_gstatus[i] = status;
      break;
    }
  }
}

/**
 * Set isPV status
 * @param status isPV status
 */
void gridpack::powerflow::PFBus::setIsPV(int status)
{
  p_saveisPV = p_isPV;
  p_isPV = status;
  p_v = p_voltage;
}

/**
 * Reset isPV status
 */
void gridpack::powerflow::PFBus::resetIsPV()
{
  p_isPV = p_saveisPV;
}

/**
 * setSBus
 * BUS = (CG*(GEN(ON,PG) + J*GEN(ON,QG)-(PD+J*QD))/BASEMVA
 */
void gridpack::powerflow::PFBus::setSBus(void)
{
  int i;
  double pg, qg;
  pg = 0.0;
  qg = 0.0;
  bool usegen = false;
  for (i=0; i<p_gstatus.size(); i++) {
    if (p_gstatus[i] == 1) {
      pg += p_pg[i];
      qg += p_qg[i];
      usegen = true;
    }
  }
  if (p_gstatus.size() > 0 && usegen) {
    gridpack::ComplexType sBus((pg - p_pl) / p_sbase, (qg - p_ql) / p_sbase);
    p_P0 = real(sBus);
    p_Q0 = imag(sBus);
  } else {
    gridpack::ComplexType sBus((- p_pl) / p_sbase, (- p_ql) / p_sbase);
    p_P0 = real(sBus);
    p_Q0 = imag(sBus);
  } 
}

/**
 * Write output from buses to standard out
 * @param string (output) string with information to be printed out
 * @param bufsize size of string buffer in bytes
 * @param signal an optional character string to signal to this
 * routine what about kind of information to write
 * @return true if bus is contributing string to output, false otherwise
 */
bool gridpack::powerflow::PFBus::serialWrite(char *string, const int bufsize,
                                             const char *signal)
{
  if (signal == NULL) {
    double pi = 4.0*atan(1.0);
    double angle = p_a*180.0/pi;
    sprintf(string, "     %6d      %12.6f         %12.6f\n",
        getOriginalIndex(),angle,p_v);
  } else if (!strcmp(signal,"pq")) {
    gridpack::ComplexType v[2];
    vectorValues(v);
    std::vector<boost::shared_ptr<BaseComponent> > branches;
    getNeighborBranches(branches);
    sprintf(string, "     %6d      %12.6f      %12.6f      %2d\n",
        getOriginalIndex(),real(v[0]),real(v[1]),branches.size());
  }
  return true;
}

/**
 * Return the complex voltage on this bus
 * @return the complex voltage
 */
gridpack::ComplexType gridpack::powerflow::PFBus::getComplexVoltage(void)
{
  p_a = *p_vAng_ptr;
  p_v =  *p_vMag_ptr;
  gridpack::ComplexType ret(cos(p_a),sin(p_a));
  ret = ret*p_v;
  return ret;
}

/**
 *  Simple constructor
 */
gridpack::powerflow::PFBranch::PFBranch(void)
{
  p_reactance.clear();
  p_resistance.clear();
  p_tap_ratio.clear();
  p_phase_shift.clear();
  p_charging.clear();
  p_shunt_admt_g1.clear();
  p_shunt_admt_b1.clear();
  p_shunt_admt_g2.clear();
  p_shunt_admt_b2.clear();
  p_xform.clear();
  p_shunt.clear();
  p_elems = 0;
  p_theta = 0.0;
  p_sbase = 0.0;
  p_mode = YBus;
}

/**
 *  Simple destructor
 */
gridpack::powerflow::PFBranch::~PFBranch(void)
{
}

/**
 * Return size of off-diagonal matrix block contributed by the component
 * for the forward/reverse directions
 * @param isize, jsize: number of rows and columns of matrix block
 * @return: false if network component does not contribute matrix element
 */
bool gridpack::powerflow::PFBranch::matrixForwardSize(int *isize, int *jsize) const
{
  if (p_mode == Jacobian) {
    gridpack::powerflow::PFBus *bus1
      = dynamic_cast<gridpack::powerflow::PFBus*>(getBus1().get());
    gridpack::powerflow::PFBus *bus2
      = dynamic_cast<gridpack::powerflow::PFBus*>(getBus2().get());
    bool ok = !bus1->getReferenceBus();
    ok = ok && !bus2->getReferenceBus();
    ok = ok && !bus1->isIsolated();
    ok = ok && !bus2->isIsolated();
    ok = ok && (p_active);
    if (ok) {
#ifdef LARGE_MATRIX
      *isize = 2;
      *jsize = 2;
      return true;
#else
      bool bus1PV = bus1->isPV();
      bool bus2PV = bus2->isPV();
      if (bus1PV && bus2PV) {
        *isize = 1;
        *jsize = 1;
        return true;
      } else if (bus1PV) {
        *isize = 1;
        *jsize = 2;
        return true;
      } else if (bus2PV) {
        *isize = 2;
        *jsize = 1;
        return true;
      } else {
        *isize = 2;
        *jsize = 2;
        return true;
      }
#endif
    } else {
      return false;
    }
  } else if (p_mode == YBus) {
    return YMBranch::matrixForwardSize(isize,jsize);
  }
}
bool gridpack::powerflow::PFBranch::matrixReverseSize(int *isize, int *jsize) const
{
  if (p_mode == Jacobian) {
    gridpack::powerflow::PFBus *bus1
      = dynamic_cast<gridpack::powerflow::PFBus*>(getBus1().get());
    gridpack::powerflow::PFBus *bus2
      = dynamic_cast<gridpack::powerflow::PFBus*>(getBus2().get());
    bool ok = !bus1->getReferenceBus();
    ok = ok && !bus2->getReferenceBus();
    ok = ok && !bus1->isIsolated();
    ok = ok && !bus2->isIsolated();
    ok = ok && (p_active);
    if (ok) {
#ifdef LARGE_MATRIX
      *isize = 2;
      *jsize = 2;
      return true;
#else
      bool bus1PV = bus1->isPV();
      bool bus2PV = bus2->isPV();
      if (bus1PV && bus2PV) {
        *isize = 1;
        *jsize = 1;
        return true;
      } else if (bus1PV) {
        *isize = 2;
        *jsize = 1;
        return true;
      } else if (bus2PV) {
        *isize = 1;
        *jsize = 2;
        return true;
      } else {
        *isize = 2;
        *jsize = 2;
        return true;
      }
#endif
    } else {
      return false;
    }
  } else if (p_mode == YBus) {
    return YMBranch::matrixReverseSize(isize,jsize);
  }
}

/**
 * Return the values of the off-diagonal matrix block. The values are
 * returned in row-major order
 * @param values: pointer to matrix block values
 * @return: false if network component does not contribute matrix element
 */
bool gridpack::powerflow::PFBranch::matrixForwardValues(ComplexType *values)
{
  if (p_mode == Jacobian) {
    gridpack::powerflow::PFBus *bus1
      = dynamic_cast<gridpack::powerflow::PFBus*>(getBus1().get());
    gridpack::powerflow::PFBus *bus2
      = dynamic_cast<gridpack::powerflow::PFBus*>(getBus2().get());
    bool ok = !bus1->getReferenceBus();
    ok = ok && !bus2->getReferenceBus();
    ok = ok && !bus1->isIsolated();
    ok = ok && !bus2->isIsolated();
    ok = ok && (p_active);
    if (ok) {
      double t11, t12, t21, t22;
      double cs = cos(p_theta);
      double sn = sin(p_theta);
      bool bus1PV = bus1->isPV();
      bool bus2PV = bus2->isPV();
#ifdef LARGE_MATRIX
      values[0] = (p_ybusr_frwd*sn - p_ybusi_frwd*cs);
      values[1] = (p_ybusr_frwd*cs + p_ybusi_frwd*sn);
      values[2] = (p_ybusr_frwd*cs + p_ybusi_frwd*sn);
      values[3] = (p_ybusr_frwd*sn - p_ybusi_frwd*cs);
      values[0] *= ((bus1->getVoltage())*(bus2->getVoltage()));
      values[1] *= -((bus1->getVoltage())*(bus2->getVoltage()));
      values[2] *= bus1->getVoltage();
      values[3] *= bus1->getVoltage();
      // fix up matrix if one or both buses at the end of the branch is a PV bus
      if (bus1PV && bus2PV) {
        values[1] = 0.0;
        values[2] = 0.0;
        values[3] = 0.0;
      } else if (bus1PV) {
        values[1] = 0.0;
        values[3] = 0.0;
      } else if (bus2PV) {
        values[2] = 0.0;
        values[3] = 0.0;
      }
#else
      if (bus1PV && bus2PV) {
        values[0] = (p_ybusr_frwd*sn - p_ybusi_frwd*cs);
        values[0] *= ((bus1->getVoltage())*(bus2->getVoltage()));
      } else if (bus1PV) {
        values[0] = (p_ybusr_frwd*sn - p_ybusi_frwd*cs);
        values[1] = (p_ybusr_frwd*cs + p_ybusi_frwd*sn);
        values[0] *= ((bus1->getVoltage())*(bus2->getVoltage()));
        values[1] *= bus1->getVoltage();
      } else if (bus2PV) {
        values[0] = (p_ybusr_frwd*sn - p_ybusi_frwd*cs);
        values[1] = (p_ybusr_frwd*cs + p_ybusi_frwd*sn);
        values[0] *= ((bus1->getVoltage())*(bus2->getVoltage()));
        values[1] *= -((bus1->getVoltage())*(bus2->getVoltage()));
      } else {
        values[0] = (p_ybusr_frwd*sn - p_ybusi_frwd*cs);
        values[1] = (p_ybusr_frwd*cs + p_ybusi_frwd*sn);
        values[2] = (p_ybusr_frwd*cs + p_ybusi_frwd*sn);
        values[3] = (p_ybusr_frwd*sn - p_ybusi_frwd*cs);
        values[0] *= ((bus1->getVoltage())*(bus2->getVoltage()));
        values[1] *= -((bus1->getVoltage())*(bus2->getVoltage()));
        values[2] *= bus1->getVoltage();
        values[3] *= bus1->getVoltage();
      }  
#endif
      return true;
    } else {
      return false;
    }
  } else if (p_mode == YBus) {
    return YMBranch::matrixForwardValues(values);
  }
}

bool gridpack::powerflow::PFBranch::matrixReverseValues(ComplexType *values)
{
  if (p_mode == Jacobian) {
    gridpack::powerflow::PFBus *bus1
      = dynamic_cast<gridpack::powerflow::PFBus*>(getBus1().get());
    gridpack::powerflow::PFBus *bus2
      = dynamic_cast<gridpack::powerflow::PFBus*>(getBus2().get());
    bool ok = !bus1->getReferenceBus();
    ok = ok && !bus2->getReferenceBus();
    ok = ok && !bus1->isIsolated();
    ok = ok && !bus2->isIsolated();
    ok = ok && (p_active);
    if (ok) {
      double t11, t12, t21, t22;
      double cs = cos(-p_theta);
      double sn = sin(-p_theta);
      bool bus1PV = bus1->isPV();
      bool bus2PV = bus2->isPV();
#ifdef LARGE_MATRIX
      values[0] = (p_ybusr_rvrs*sn - p_ybusi_rvrs*cs);
      values[1] = (p_ybusr_rvrs*cs + p_ybusi_rvrs*sn);
      values[2] = (p_ybusr_rvrs*cs + p_ybusi_rvrs*sn);
      values[3] = (p_ybusr_rvrs*sn - p_ybusi_rvrs*cs);
      values[0] *= ((bus1->getVoltage())*(bus2->getVoltage()));
      values[1] *= -((bus1->getVoltage())*(bus2->getVoltage()));
      values[2] *= bus2->getVoltage();
      values[3] *= bus2->getVoltage();
      // fix up matrix if one or both buses at the end of the branch is a PV bus
      if (bus1PV && bus2PV) {
        values[1] = 0.0;
        values[2] = 0.0;
        values[3] = 0.0;
      } else if (bus1PV) {
        values[2] = 0.0;
        values[3] = 0.0;
      } else if (bus2PV) {
        values[1] = 0.0;
        values[3] = 0.0;
      }
#else
      if (bus1PV && bus2PV) {
        values[0] = (p_ybusr_rvrs*sn - p_ybusi_rvrs*cs);
        values[0] *= ((bus1->getVoltage())*(bus2->getVoltage()));
      } else if (bus1PV) {
        values[0] = (p_ybusr_rvrs*sn - p_ybusi_rvrs*cs);
        values[1] = (p_ybusr_rvrs*cs + p_ybusi_rvrs*sn);
        values[0] *= ((bus1->getVoltage())*(bus2->getVoltage()));
        values[1] *= -((bus1->getVoltage())*(bus2->getVoltage()));
      } else if (bus2PV) {
        values[0] = (p_ybusr_rvrs*sn - p_ybusi_rvrs*cs);
        values[1] = (p_ybusr_rvrs*cs + p_ybusi_rvrs*sn);
        values[0] *= ((bus1->getVoltage())*(bus2->getVoltage()));
        values[1] *= bus2->getVoltage();
      } else {
        values[0] = (p_ybusr_rvrs*sn - p_ybusi_rvrs*cs);
        values[1] = (p_ybusr_rvrs*cs + p_ybusi_rvrs*sn);
        values[2] = (p_ybusr_rvrs*cs + p_ybusi_rvrs*sn);
        values[3] = (p_ybusr_rvrs*sn - p_ybusi_rvrs*cs);
        values[0] *= ((bus1->getVoltage())*(bus2->getVoltage()));
        values[1] *= -((bus1->getVoltage())*(bus2->getVoltage()));
        values[2] *= bus2->getVoltage();
        values[3] *= bus2->getVoltage();
      } 
#endif
      return true;
    } else {
      return false;
    }
  } else if (p_mode == YBus) {
    return YMBranch::matrixForwardValues(values);
  }
}

// Calculate contributions to the admittance matrix from the branches
void gridpack::powerflow::PFBranch::setYBus(void)
{
  YMBranch::setYBus();
  gridpack::ComplexType ret;
  ret = YMBranch::getForwardYBus();
  p_ybusr_frwd = real(ret);
  p_ybusi_frwd = imag(ret);
  ret = YMBranch::getReverseYBus();
  p_ybusr_rvrs = real(ret);
  p_ybusi_rvrs = imag(ret);
  // Not really a contribution to the admittance matrix but might as well
  // calculate phase angle difference between buses at each end of branch
  gridpack::powerflow::PFBus *bus1 =
    dynamic_cast<gridpack::powerflow::PFBus*>(getBus1().get());
  gridpack::powerflow::PFBus *bus2 =
    dynamic_cast<gridpack::powerflow::PFBus*>(getBus2().get());
  double pi = 4.0*atan(1.0);
  p_theta = (bus1->getPhase() - bus2->getPhase());
}

/**
 * Load values stored in DataCollection object into PFBranch object. The
 * DataCollection object will have been filled when the network was created
 * from an external configuration file
 * @param data: DataCollection object contain parameters relevant to this
 *       branch that were read in when network was initialized
 */
void gridpack::powerflow::PFBranch::load(
    const boost::shared_ptr<gridpack::component::DataCollection> &data)
{
  YMBranch::load(data);

  bool ok = true;
  data->getValue(BRANCH_NUM_ELEMENTS, &p_elems);
  double rvar;
  int ivar;
  double pi = 4.0*atan(1.0);
  p_active = false;
  ok = data->getValue(CASE_SBASE, &p_sbase);
  int idx;
  for (idx = 0; idx<p_elems; idx++) {
    bool xform = true;
    xform = xform && data->getValue(BRANCH_X, &rvar, idx);
    p_reactance.push_back(rvar);
    xform = xform && data->getValue(BRANCH_R, &rvar, idx);
    p_resistance.push_back(rvar);
    ok = ok && data->getValue(BRANCH_SHIFT, &rvar, idx);
    rvar = -rvar*pi/180.0; 
    p_phase_shift.push_back(rvar);
    ok = ok && data->getValue(BRANCH_TAP, &rvar, idx);
    p_tap_ratio.push_back(rvar); 
    if (rvar != 0.0) {
      p_xform.push_back(xform);
    } else {
      p_xform.push_back(false);
    }
    ivar = 1;
    data->getValue(BRANCH_STATUS, &ivar, idx);
    p_branch_status.push_back(static_cast<bool>(ivar));
    if (ivar == 1) p_active = true;
    bool shunt = true;
    shunt = shunt && data->getValue(BRANCH_B, &rvar, idx);
    p_charging.push_back(rvar);
    shunt = shunt && data->getValue(BRANCH_SHUNT_ADMTTNC_G1, &rvar, idx);
    p_shunt_admt_g1.push_back(rvar);
    shunt = shunt && data->getValue(BRANCH_SHUNT_ADMTTNC_B1, &rvar, idx);
    p_shunt_admt_b1.push_back(rvar);
    shunt = shunt && data->getValue(BRANCH_SHUNT_ADMTTNC_G2, &rvar, idx);
    p_shunt_admt_g2.push_back(rvar);
    shunt = shunt && data->getValue(BRANCH_SHUNT_ADMTTNC_B2, &rvar, idx);
    p_shunt_admt_b2.push_back(rvar);
    p_shunt.push_back(shunt);
    bool rate = true;
    rate = rate && data->getValue(BRANCH_RATING_A,&rvar,idx);
    p_rateA.push_back(rvar);
  }
}

/**
 * Set the mode to control what matrices and vectors are built when using
 * the mapper
 * @param mode: enumerated constant for different modes
 */
void gridpack::powerflow::PFBranch::setMode(int mode)
{
  if (mode == YBus) {
    YMBranch::setMode(gridpack::ymatrix::YBus);
  }
  p_mode = mode;
}

/**
 * Return the contribution to the Jacobian for the powerflow equations from
 * a branch
 * @param bus: pointer to the bus making the call
 * @param values: an array of 4 doubles that holds return metrix elements
 */
void gridpack::powerflow::PFBranch::getJacobian(gridpack::powerflow::PFBus *bus, double *values)
{
  double v;
  double cs, sn;
  double ybusr, ybusi;
  if (bus == getBus1().get()) {
    gridpack::powerflow::PFBus *bus2 =
      dynamic_cast<gridpack::powerflow::PFBus*>(getBus2().get());
    v = bus2->getVoltage();
    cs = cos(p_theta);
    sn = sin(p_theta);
    ybusr = p_ybusr_frwd;
    ybusi = p_ybusi_frwd;
  } else if (bus == getBus2().get()) {
    gridpack::powerflow::PFBus *bus1 =
      dynamic_cast<gridpack::powerflow::PFBus*>(getBus1().get());
    v = bus1->getVoltage();
    cs = cos(-p_theta);
    sn = sin(-p_theta);
    ybusr = p_ybusr_rvrs;
    ybusi = p_ybusi_rvrs;
  } else {
    // TODO: Some kind of error
  }
  values[0] = v*(ybusr*sn - ybusi*cs);
  values[1] = -v*(ybusr*cs + ybusi*sn);
  values[2] = (ybusr*cs + ybusi*sn);
  values[3] = (ybusr*sn - ybusi*cs);
}

/**
 * Return contribution to constraints
 * @param p: real part of constraint
 * @param q: imaginary part of constraint
 */
void gridpack::powerflow::PFBranch::getPQ(gridpack::powerflow::PFBus *bus, double *p, double *q)
{
  gridpack::powerflow::PFBus *bus1 = 
    dynamic_cast<gridpack::powerflow::PFBus*>(getBus1().get());
  double v1 = bus1->getVoltage();
  gridpack::powerflow::PFBus *bus2 =
    dynamic_cast<gridpack::powerflow::PFBus*>(getBus2().get());
  double v2 = bus2->getVoltage();
  double cs, sn;
  double ybusr, ybusi;
  p_theta = bus1->getPhase() - bus2->getPhase();
  if (bus == bus1) {
    cs = cos(p_theta);
    sn = sin(p_theta);
    ybusr = p_ybusr_frwd;
    ybusi = p_ybusi_frwd;
  } else if (bus == bus2) {
    cs = cos(-p_theta);
    sn = sin(-p_theta);
    ybusr = p_ybusr_rvrs;
    ybusi = p_ybusi_rvrs;
  } else {
    // TODO: Some kind of error
  }
  *p = v1*v2*(ybusr*cs+ybusi*sn);
  *q = v1*v2*(ybusr*sn-ybusi*cs);
}

/**
 * Return complex power for line element
 * @param tag describing line element on branch
 * @return complex power
 */
gridpack::ComplexType gridpack::powerflow::PFBranch::getComplexPower(
        std::string tag)
{
  gridpack::ComplexType vi, vj, Yii, Yij, s;
  s = ComplexType(0.0,0.0);
  gridpack::powerflow::PFBus *bus1 = 
    dynamic_cast<gridpack::powerflow::PFBus*>(getBus1().get());
  vi = bus1->getComplexVoltage();
  gridpack::powerflow::PFBus *bus2 =
    dynamic_cast<gridpack::powerflow::PFBus*>(getBus2().get());
  vj = bus2->getComplexVoltage();
  getLineElements(tag,&Yii,&Yij);
  s = vi*conj(Yii*vi+Yij*vj)*p_sbase;
  return s;
}

/**
 * Write output from branches to standard out
 * @param string (output) string with information to be printed out
 * @param bufsize size of string buffer in bytes
 * @param signal an optional character string to signal to this
 * routine what about kind of information to write
 * @return true if branch is contributing string to output, false otherwise
 */
bool gridpack::powerflow::PFBranch::serialWrite(char *string, const int bufsize,
                                                const char *signal)
{
  gridpack::powerflow::PFBus *bus1 = 
    dynamic_cast<gridpack::powerflow::PFBus*>(getBus1().get());
  gridpack::powerflow::PFBus *bus2 =
    dynamic_cast<gridpack::powerflow::PFBus*>(getBus2().get());
  char buf[128];
  if (signal != NULL && !strcmp(signal,"flow")) {
    gridpack::ComplexType s;
    std::vector<std::string> tags = getLineTags();
    int i;
    bool found = false;
    int ilen = 0;
    for (i=0; i<p_elems; i++) {
      s = getComplexPower(tags[i]);
      double p = real(s);
      double q = imag(s);
      if (!p_branch_status[i]) p = 0.0;
      if (!p_branch_status[i]) q = 0.0;
      double S = sqrt(p*p+q*q);
      if (S > p_rateA[i] && p_rateA[i] != 0.0){
        sprintf(buf, "     %6d      %6d        %s  %12.6f         %12.6f     %8.2f     %8.2f%s\n",
    	  bus1->getOriginalIndex(),bus2->getOriginalIndex(),tags[i].c_str(),
          p,q,p_rateA[i],S/p_rateA[i]*100,"%");
        ilen += strlen(buf);
        if (ilen<bufsize) sprintf(string,"%s",buf);
        string += strlen(buf);
        found = true;
      }
    }
    return found;
  } else {
    gridpack::ComplexType s;
    std::vector<std::string> tags = getLineTags();
    int i;
    int ilen = 0;
    for (i=0; i<p_elems; i++) {
      s = getComplexPower(tags[i]);
      double p = real(s);
      double q = imag(s);
      if (!p_branch_status[i]) p = 0.0;
      if (!p_branch_status[i]) q = 0.0;
      sprintf(buf, "     %6d      %6d     %s   %12.6f         %12.6f\n",
          bus1->getOriginalIndex(),bus2->getOriginalIndex(),tags[i].c_str(),p,q);
      ilen += strlen(buf);
      if (ilen<bufsize) sprintf(string,"%s",buf);
      string += strlen(buf);
    } 
    return true;
  }
}
