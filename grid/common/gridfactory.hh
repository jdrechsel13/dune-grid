// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#ifndef DUNE_GRID_FACTORY_HH
#define DUNE_GRID_FACTORY_HH

/** \file
    \brief Provide a generic factory class for unstructured grids.
 */

#include <vector>

#include <dune/common/fvector.hh>
#include <dune/common/geometrytype.hh>

#include <dune/grid/common/grid.hh>
#include <dune/grid/common/boundarysegment.hh>

namespace Dune {


  /** \brief Provide a generic factory class for unstructured grids.

      This base class declares the interface.
   */
  template <class GridType>
  class GridFactoryInterface {

    /** \brief The grid world dimension */
    enum {dimworld = GridType::dimensionworld};

  protected:

    /** \brief Type used by the grid for coordinates */
    typedef typename GridType::ctype ctype;

  public:

    /** \brief Insert a vertex into the coarse grid */
    virtual void insertVertex(const FieldVector<ctype,dimworld>& pos) = 0;

    /** \brief Insert an element into the coarse grid
        \param type The GeometryType of the new element
        \param vertices The vertices of the new element, using the DUNE numbering
     */
    virtual void insertElement(const GeometryType& type,
                               const std::vector<unsigned int>& vertices) = 0;

    /** \brief Method to insert an arbitrarily shaped boundary segment into a coarse grid
        \param vertices The indices of the vertices of the segment
        \param boundarySegment Class implementing the geometry of the boundary segment.
        The grid object takes control of this object and deallocates it when destructing itself.
     */
    virtual void insertBoundarySegment(const std::vector<unsigned int> vertices,
                                       const BoundarySegment<dimworld>* boundarySegment) {
      DUNE_THROW(GridError, "This grid does not support parametrized boundary segments!");
    }

    /** \brief Finalize grid creation and hand over the grid

       The receiver takes responsibility of the memory allocated for the grid
     */
    virtual GridType* createGrid() = 0;

  };


  /** \brief Provide a generic factory class for unstructured grids.

      This is the unspecialized class, which does nothing.  All work is
      done in the specializations for the different grid types.
   */
  template <class GridType>
  class GridFactory : public GridFactoryInterface<GridType> {

    /** \brief The grid world dimension */
    enum {dimworld = GridType::dimensionworld};

    /** \brief Type used by the grid for coordinates */
    typedef typename GridType::ctype ctype;

  public:

    /** \brief Insert a vertex into the coarse grid */
    virtual void insertVertex(const FieldVector<ctype,dimworld>& pos) {
      DUNE_THROW(GridError, "There is no grid factory for this grid type!");
    }

    /** \brief Insert an element into the coarse grid
        \param type The GeometryType of the new element
        \param vertices The vertices of the new element, using the DUNE numbering
     */
    virtual void insertElement(GeometryType type,
                               const std::vector<unsigned int>& vertices) {
      DUNE_THROW(GridError, "There is no grid factory for this grid type!");
    }

    /** \brief Finalize grid creation and hand over the grid

       The receiver takes responsibility of the memory allocated for the grid
     */
    virtual GridType* createGrid() {
      DUNE_THROW(GridError, "There is no grid factory for this grid type!");
    }

  };

}

#endif
