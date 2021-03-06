/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */
/* *****************************************************************************
 * bus_vector_map.hpp
 * gridpack
 * kglass, bjpalmer
 * Jul 22, 2013
 *
 * Documentation status
 * Revision status
 * File contents
 **************************************************************************** */

#ifndef BUSVECTORMAP_HPP_
#define BUSVECTORMAP_HPP_

#include <boost/smart_ptr/shared_ptr.hpp>
#include <ga.h>
#include "gridpack/parallel/parallel.hpp"
#include <gridpack/parallel/distributed.hpp>
#include <gridpack/component/base_component.hpp>
#include <gridpack/network/base_network.hpp>
#include <gridpack/math/vector.hpp>

//#define DBG_CHECK

namespace gridpack {
namespace mapper {

template <class _network>
class BusVectorMap {
  public: 
/**
 * Initialize mapper for the given network and the current mode. Create global
 * arrays that contain offsets that will be used to create vector from the
 * network component objects
 * @param network network that will generate vector
 */
BusVectorMap(boost::shared_ptr<_network> network)
  : p_network(network)
{
  p_Offsets                        = NULL;
  int                     iSize    = 0;

  p_timer = NULL;
  // p_timer = gridpack::utility::CoarseTimer::instance();

  p_GAgrp = network->communicator().getGroup();
  p_me = GA_Pgroup_nodeid(p_GAgrp);
  p_nNodes = GA_Pgroup_nnodes(p_GAgrp);


  p_nBuses = p_network->numBuses();

  p_activeBuses         = getActiveBuses();

  setupGlobalArrays(p_activeBuses);  // allocate globalIndex arrays

  setupIndexingArrays();

  setupOffsetArrays();

  contributions();
  setBusOffsets();
}

~BusVectorMap()
{
  if (p_Offsets != NULL) delete [] p_Offsets;
  GA_Destroy(gaVecBlksI);
  GA_Destroy(gaOffsetI);
}

/**
 * Create a vector from the current bus state
 * @return return pointer to new vector
 */
boost::shared_ptr<gridpack::math::Vector> mapToVector(void)
{
  gridpack::parallel::Communicator comm = p_network->communicator();
  int t_new(0), t_bus(0), t_set(0);
  if (p_timer) t_new = p_timer->createCategory("Vector Map: New Vector");
  if (p_timer) p_timer->start(t_new);
  boost::shared_ptr<gridpack::math::Vector>
             Ret(new gridpack::math::Vector(comm, p_rowBlockSize));
  if (p_timer) p_timer->stop(t_new);
  if (p_timer) t_bus = p_timer->createCategory("Vector Map: Load Bus Data");
  if (p_timer) p_timer->start(t_bus);
  loadBusData(*Ret,true);
  if (p_timer) p_timer->stop(t_bus);
  if (p_timer) t_set = p_timer->createCategory("Vector Map: Set Vector");
  if (p_timer) p_timer->start(t_set);
  GA_Pgroup_sync(p_GAgrp);
  Ret->ready();
  if (p_timer) p_timer->stop(t_set);
  return Ret;
}

/**
 * Create a vector from the current bus state and return a conventional pointer
 * to it. Used for Fortran interface
 * @return return pointer to new vector
 */
gridpack::math::Vector* intMapToVector(void)
{
  gridpack::parallel::Communicator comm = p_network->communicator();
  int t_new, t_bus, t_set;
  if (p_timer) t_new = p_timer->createCategory("Vector Map: New Vector");
  if (p_timer) p_timer->start(t_new);
  gridpack::math::Vector*
     Ret(new gridpack::math::Vector(comm, p_rowBlockSize));
  if (p_timer) p_timer->stop(t_new);
  if (p_timer) t_bus = p_timer->createCategory("Vector Map: Load Bus Data");
  if (p_timer) p_timer->start(t_bus);
  loadBusData(*Ret,true);
  if (p_timer) p_timer->stop(t_bus);
  if (p_timer) t_set = p_timer->createCategory("Vector Map: Set Vector");
  if (p_timer) p_timer->start(t_set);
  GA_Pgroup_sync(p_GAgrp);
  Ret->ready();
  if (p_timer) p_timer->stop(t_set);
  return Ret;
}

/**
 * Reset a vector from the current bus state (vector should be created with same
 * mapper)
 * @param vector existing vector that should be reset
 */
void mapToVector(gridpack::math::Vector &vector)
{
  int t_bus(0), t_set(0);
  if (p_timer) t_set = p_timer->createCategory("Vector Map: Set Vector");
  if (p_timer) p_timer->start(t_set);
  vector.zero();
  if (p_timer) p_timer->stop(t_set);
  if (p_timer) t_bus = p_timer->createCategory("Vector Map: Load Bus Data");
  if (p_timer) p_timer->start(t_bus);
  loadBusData(vector,false);
  if (p_timer) p_timer->stop(t_bus);
  if (p_timer) p_timer->start(t_set);
  GA_Pgroup_sync(p_GAgrp);
  vector.ready();
  if (p_timer) p_timer->stop(t_set);
}

/**
 * Reset a vector from the current bus state (vector should be created with same
 * mapper). 
 * @param vector existing vector that should be reset
 */
void mapToVector(boost::shared_ptr<gridpack::math::Vector> &vector)
{
  mapToVector(*vector);
}

/**
 * Push data from vector onto buses. Vector must be created with the
 * mapToVector method using the same BusVectorMap
 * @param vector vector containing data to be pushed to buses
 */
void mapToBus(const gridpack::math::Vector &vector)
{
  // Assume that row partitioning is working correctly
  int nRows = p_maxRowIndex - p_minRowIndex + 1;
  int *sizes = new int[nRows];
  int *offsets = new int[nRows];
  int one = 1;
  NGA_Get(gaVecBlksI,&p_minRowIndex,&p_maxRowIndex,sizes,&one);
  NGA_Get(gaOffsetI,&p_minRowIndex,&p_maxRowIndex,offsets,&one);
  ComplexType *values = new ComplexType[p_maxIBlock];
  int i, j, idx, size, offset;
  for (i=0; i<p_nBuses; i++) {
    if (p_network->getActiveBus(i)) {
      if (p_network->getBus(i)->vectorSize(&size)) {
        p_network->getBus(i)->getMatVecIndex(&idx);
        idx = idx - p_minRowIndex;
        size = sizes[idx];
        offset = offsets[idx];
        for (j=0; j<size; j++) {
          vector.getElement(offset+j,values[j]); 
        }
        p_network->getBus(i)->setValues(values);
      }
    }
  }
  delete [] sizes;
  delete [] offsets;
  delete [] values;
  GA_Pgroup_sync(p_GAgrp);
}

/**
 * Push data from vector onto buses. Vector must be created with the
 * mapToVector method using the same BusVectorMap
 * @param vector vector containing data to be pushed to buses
 */
void mapToBus(boost::shared_ptr<gridpack::math::Vector> &vector)
{
  mapToBus(*vector);
}

private:
/**
 * Return the number of active buses on this process
 * @return number of active buses
 */
int getActiveBuses(void)
{
  int nActiveBuses = 0;
  for (int i = 0; i<p_nBuses; i++) {
    if (p_network->getActiveBus(i)) {
      nActiveBuses++;
    }
  }
  return nActiveBuses;
}

/**
 * Allocate the gaVecBlksI global array
 * @param nActiveBuses the number of active buses on this process
 */
void setupGlobalArrays(int nActiveBuses)
{
  int one = 1;

  p_totalBuses = nActiveBuses;

  GA_Pgroup_igop(p_GAgrp,&p_totalBuses,one,"+");

  // the gaVecBlksI array contains the vector blocks sizes for
  // individual block contributions
  createIndexGA(&gaVecBlksI, p_totalBuses);
}

/**
 * Create a global array of integers
 * @param size size of global array
 */
void createIndexGA(int * handle, int size)
{
  int one = 1;
  *handle = GA_Create_handle();
  GA_Set_data(*handle, one, &size, C_INT);
  GA_Set_pgroup(*handle,p_GAgrp);
  if (!GA_Allocate(*handle)) {
    // TODO: some kind of error
  }
  GA_Zero(*handle);
}

/**
 * Set up global arrays that contain all vector block sizes.
 * These will then be used to create offset arrays.
 */
void setupIndexingArrays()
{
  int                    * iSizeArray     = NULL;
  int                   ** iIndexArray    = NULL;
  int                      count          = 0;

  // set up bus indexing
  allocateIndexArray(p_nBuses, &iSizeArray, &iIndexArray);

  loadBusArrays(iSizeArray, iIndexArray, &count);
  scatterIndexingArrays(iSizeArray, iIndexArray, count);
  deleteIndexArrays(p_nBuses, iSizeArray, iIndexArray);
  GA_Pgroup_sync(p_GAgrp);
}

/**
 * Allocate arrays that hold sizes and approximate indices of vector elements
 * @param n number of elements in array
 * @param iSizeArray array containing size of vector block
 * @param iIndexArray array containing index of vector block
 */
void allocateIndexArray(int n, int ** iSizeArray, int *** iIndexArray)
{
  *iSizeArray         = new int[n];
  *iIndexArray        = new int*[n];

  for(int i = 0; i < n; i++) {
    (*iIndexArray)[i]  = new int;
  }
}

/**
 * Load arrays containing vector block sizes and indices
 * These come from buses
 * @param iSizeArray array containing size of vector block along i axis
 * @param iIndexArray array containing i index of vector block
 * @param count return total number of non-zero blocks
 */
void loadBusArrays(int * iSizeArray, int ** iIndexArray, int *count)
{
  int                      index          = 0;
  int                      iSize          = 0;
  bool                     status         = true;

  *count = 0;
  for (int i = 0; i < p_nBuses; i++) {
    status = p_network->getBus(i)->vectorSize(&iSize);
    if (status) {
      p_network->getBus(i)->getMatVecIndex(&index);
      iSizeArray[*count]     = iSize;
      *(iIndexArray[*count])  = index;
      (*count)++;
    }
  }
}

/**
 *  Clean up index arrays
 *  @param n array size
 *  @param iSizeArray array containing size of vector block along i axis
 *  @param iIndexArray array containing i index of vector block
 */
void deleteIndexArrays(int n, int * iSizeArray, int ** iIndexArray)
{
  for(int i = 0; i < n; i++) {
    delete iIndexArray[i];
  }
  delete [] iIndexArray;
  delete [] iSizeArray;
}

/**
 * Scatter elements into global arrays
 * @param iSizeArray array containing size of vector block along i axis
 * @param iIndexArray array containing i index of vector block
 * @param count number of elements to be scattered
 */
void scatterIndexingArrays(int * iSizeArray, int ** iIndexArray, int count)
{
  if (count > 0) NGA_Scatter(gaVecBlksI, iSizeArray, iIndexArray, count);
}

/**
 * Set up the offset arrays that will be used to find the exact location of
 * each vector block in the vector produced by the mapper
 */
void setupOffsetArrays()
{
  int *itmp = new int[p_nNodes];

  int *rptr;
  int i, idx;
  int one = 1;

  // We need to decompose this vector so that each processor has a contiguous
  // set of elements. The MatVec indices are set up so that the active
  // buses on each processor are indexed consecutively. This index can be used
  // to set up this partition.
  p_minRowIndex = p_totalBuses;
  p_maxRowIndex = 0;
  for (i=0; i<p_nBuses; i++) {
    if (p_network->getActiveBus(i)) {
      p_network->getBus(i)->getMatVecIndex(&idx);
      if (idx > p_maxRowIndex) p_maxRowIndex = idx;
      if (idx < p_minRowIndex) p_minRowIndex = idx;
    }
  }
  // Create array to hold information about desired elements
  int nRows = p_maxRowIndex-p_minRowIndex+1;
  int *iSizes = new int[nRows]; 
  GA_Pgroup_sync(p_GAgrp);
  NGA_Get(gaVecBlksI,&p_minRowIndex,&p_maxRowIndex,iSizes,&one);

  // Calculate total number of elements associated with row block and column
  // block associated with this processor
  int iSize = 0;
  p_maxIBlock = 0;
  for (i=0; i<nRows; i++) {
    if (p_maxIBlock < iSizes[i]) p_maxIBlock = iSizes[i];
    if (iSizes[i] > 0) iSize += iSizes[i];
  }
  p_rowBlockSize = iSize;
  GA_Pgroup_igop(p_GAgrp,&p_maxIBlock,one,"max");

  for (i = 0; i<p_nNodes; i++) {
    itmp[i] = 0;
  }
  itmp[p_me] = iSize;

  GA_Pgroup_igop(p_GAgrp,itmp, p_nNodes, "+");

  int offsetArrayISize = 0;
  for (i = 0; i < p_me; i++) {
    offsetArrayISize += itmp[i];
  }

  // Calculate vector length
  int p_iDim = 0;
  for (i=0; i<p_nNodes; i++) {
    p_iDim += itmp[i];
  }

  // Create map array so that offset arrays can be created with a specified
  // distribution
  int *offset = new int[p_nNodes];
  for (i=0; i<p_nNodes; i++) {
    offset[i] = 0;
  }
  offset[p_me] = p_activeBuses;
  GA_Pgroup_igop(p_GAgrp,offset,p_nNodes,"+");

  int *mapc = new int[p_nNodes];
  mapc[0]=0;
  for (i=1; i<p_nNodes; i++) {
    mapc[i] = mapc[i-1] + offset[i-1];
  }

  delete [] offset;
  delete [] itmp;

  // Create arrays that will hold final offsets
  gaOffsetI = GA_Create_handle();
  GA_Set_data(gaOffsetI, one, &p_totalBuses, C_INT);
  GA_Set_irreg_distr(gaOffsetI, mapc, &p_nNodes);
  GA_Set_pgroup(gaOffsetI,p_GAgrp);
  if (!GA_Allocate(gaOffsetI)) {
    // TODO: some kind of error
  }
  GA_Zero(gaOffsetI);

  // Evaluate offsets for this processor
  int *iOffsets = new int[nRows]; 
  iOffsets[0] = offsetArrayISize;
  for (i=1; i<nRows; i++) {
    iOffsets[i] = iOffsets[i-1] + iSizes[i-1];
  }

  // Put offsets into global arrays
  if (nRows > 0) {
    NGA_Put(gaOffsetI,&p_minRowIndex,&p_maxRowIndex,iOffsets,&one);
  }
  GA_Pgroup_sync(p_GAgrp);

  delete [] mapc;
  delete [] iSizes;
  delete [] iOffsets;
}

/**
 * Set up offset arrays that are used to evaluate index locations of individual
 * vector elements generated by buses.
 */
void setBusOffsets(void)
{
  int i,idx,isize,icnt;
  int **indices = new int*[p_busContribution];
  int *data = new int[p_busContribution];
  int *ptr = data;
  icnt = 0;
  int t_idx(0), t_gat(0);
  if (p_timer) t_idx = p_timer->createCategory("setBusOffsets: Set Index Arrays");
  if (p_timer) p_timer->start(t_idx);
  boost::shared_ptr<gridpack::component::BaseBusComponent> bus;
  for (i=0; i<p_nBuses; i++) {
    if (p_network->getActiveBus(i)) {
      bus = p_network->getBus(i);
      if (bus->vectorSize(&isize)) {
        indices[icnt] = ptr;
        bus->getMatVecIndex(&idx);
        *(indices[icnt]) = idx;
        ptr++;
        icnt++;
      }
    }
  }
  if (icnt != p_busContribution) {
    // TODO: some kind of error
    printf("p[%d] Mismatch icnt: %d busContribution: %d\n",
        GA_Nodeid(),icnt,p_busContribution);
  }
  if (p_timer) p_timer->stop(t_idx);

  // Gather vector offsets
  if (p_timer) t_gat = p_timer->createCategory("setBusOffsets: Gather Offsets");
  if (p_timer) p_timer->start(t_gat);
  p_Offsets = new int[p_busContribution];
  if (p_busContribution > 0) {
    NGA_Gather(gaOffsetI,p_Offsets,indices,p_busContribution);
  }
  if (p_timer) p_timer->stop(t_gat);
  // Clean up arrays
  if (p_busContribution > 0) {
    delete [] indices;
    delete [] data;
  }
}

/**
 * Add block contributions from buses to vector
 * @param vector vector to which contributions are added
 * @param flag flag to distinguish new vector (true) from existing vector * (false)
 */
void loadBusData(gridpack::math::Vector &vector, bool flag)
{
  int i,idx,isize,icnt;
  // Add vector elements
  boost::shared_ptr<gridpack::component::BaseBusComponent> bus;
  ComplexType *values = new ComplexType[p_maxIBlock];
  int t_bus(0);
  if (p_timer) t_bus = p_timer->createCategory("loadBusData: Add Vector Elements");
  if (p_timer) p_timer->start(t_bus);
  int j;
  int jcnt = 0;
  for (i=0; i<p_nBuses; i++) {
    if (p_network->getActiveBus(i)) {
      bus = p_network->getBus(i);
      if (bus->vectorSize(&isize)) {
#ifdef DGB_CHECK
        for (j=0; j<isize; j++) values[j] = 0.0;
#endif
        bus->vectorValues(values);
        icnt = 0;
        for (j=0; j<isize; j++) {
          idx = p_Offsets[jcnt] + j;
//          if (flag) {
            vector.addElement(idx, values[icnt]);
//          } else {
//            vector.setElement(idx, values[icnt]);
//          } 
          icnt++;
        }
        jcnt++;
      }
    }
  }
  if (p_timer) p_timer->stop(t_bus);

  // Clean up arrays
  delete [] values;
}

/**
 * Add block contributions from buses to vector
 * @param vector vector to which contributions are added
 * @param flag flag to distinguish new vector (true) from existing vector * (false)
 */
void loadBusData(boost::shared_ptr<gridpack::math::Vector> &vector, bool flag)
{
  loadBusData(*vector, flag);
}

/**
 * Calculate how many buses contribute to vector
 */
void contributions(void)
{
  int i;
  // Get number of contributions from buses
  int isize, jsize;
  p_busContribution = 0;
  for (i=0; i<p_nBuses; i++) {
    if (p_network->getActiveBus(i)) {
      if (p_network->getBus(i)->vectorSize(&isize)) p_busContribution++;
    }
  }
}

    // GA information
int                         p_me;
int                         p_nNodes;

    // network information
boost::shared_ptr<_network> p_network;
int                         p_nBuses;
int                         p_totalBuses;
int                         p_activeBuses;

    // vector information
int                         p_iDim;
int                         p_minRowIndex;
int                         p_maxRowIndex;
int                         p_rowBlockSize;
int                         p_busContribution;
int                         p_maxIBlock;

int*                        p_Offsets;

    // global vector block size array
int                         gaVecBlksI; // g_idx
int                         gaOffsetI; // g_ioff
int                         p_GAgrp; // GA group

    // pointer to timer
gridpack::utility::CoarseTimer *p_timer;

};

} /* namespace mapper */
} /* namespace gridpack */

#endif //BUSVECTORMAP_HPP_
