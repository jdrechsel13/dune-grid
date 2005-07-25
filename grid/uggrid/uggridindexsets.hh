// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#ifndef DUNE_UGGRID_INDEXSETS_HH
#define DUNE_UGGRID_INDEXSETS_HH

/** \file
    \brief The index and id sets for the UGGrid class
 */

#include <vector>

namespace Dune {

  template<class GridImp>
  class UGGridLevelIndexSet
  {
  public:
    /** \brief Default constructor

       Unfortunately we can't force the user to init grid_ and level_, because
       UGGridLevelIndexSets are meant to be stored in an array.
     */
    UGGridLevelIndexSet () {}

    //! get index of an entity
    template<int cd>
    int index (const typename GridImp::Traits::template Codim<cd>::Entity& e) const
    {
      return grid_->template getRealEntity<cd>(e).levelIndex();
    }

    //! get index of subentity of a codim 0 entity
    template<int cc>
    int subindex (const typename GridImp::Traits::template Codim<0>::Entity& e, int i) const
    {
      return grid_->template getRealEntity<0>(e).template subIndex<cc>(i);
    }

    //! get number of entities of given codim, type and on this level
    int size (int codim, GeometryType type) const
    {
      return grid_->size(level_,codim, type);
    }

    /** \brief Deliver all geometry types used in this grid */
    const std::vector<GeometryType>& geomtypes () const
    {
      return myTypes_;
    }

    /** \todo Should be private */
    void update(const GridImp& grid, int level) {

      grid_ = &grid;
      level_ = level;

      const int dim = GridImp::dimension;

      typename GridImp::Traits::template Codim<0>::LevelIterator eIt    = grid_->template lbegin<0>(level_);
      typename GridImp::Traits::template Codim<0>::LevelIterator eEndIt = grid_->template lend<0>(level_);

      int id = 0;
      for (; eIt!=eEndIt; ++eIt)
        UG_NS<dim>::levelIndex(grid_->template getRealEntity<0>(*eIt).target_) = id++;

      typename GridImp::Traits::template Codim<dim>::LevelIterator vIt    = grid_->template lbegin<dim>(level_);
      typename GridImp::Traits::template Codim<dim>::LevelIterator vEndIt = grid_->template lend<dim>(level_);

      id = 0;
      for (; vIt!=vEndIt; ++vIt)
        UG_NS<dim>::levelIndex(grid_->template getRealEntity<dim>(*vIt).target_) = id++;
    }

  private:
    const GridImp* grid_;
    int level_;
    std::vector<GeometryType> myTypes_;
  };

  template<class GridImp>
  class UGGridLeafIndexSet
  {
  public:
    //! constructor stores reference to a grid and level
    UGGridLeafIndexSet (const GridImp& g) : grid_(g)
    {}

    //! get index of an entity
    template<int cd>
    int index (const typename GridImp::Traits::template Codim<cd>::Entity& e) const
    {
      return grid_.template getRealEntity<cd>(e).leafIndex();
    }

    //! get index of subentity of a codim 0 entity
    template<int cc>
    int subindex (const typename GridImp::Traits::template Codim<0>::Entity& e, int i) const
    {
      DUNE_THROW(NotImplemented, "UGGridLeafIndexSet::subindex()");
      //return grid.template getRealEntity<0>(e).template subCompressedIndex<cc>(i);
    }

    //! get number of entities of given codim, type and level (the level is known to the object)
    int size (int codim, GeometryType type) const
    {
      return grid_.size(codim, type);
    }

    /** deliver all geometry types used in this grid */
    const std::vector<GeometryType>& geomtypes () const
    {
      return mytypes;
    }

    /** \todo Should be private */
    void update() {

      // update the list of element types present
    }

  private:

    const GridImp& grid_;
    std::vector<GeometryType> mytypes;
  };


  template<class GridImp>
  class UGGridGlobalIdSet
  {
  public:
    //! define the type used for persistent indices
    typedef unsigned int GlobalIdType;

    //! constructor stores reference to a grid
    UGGridGlobalIdSet (const GridImp& g) : grid_(g) {}

    //! get id of an entity
    template<int cd>
    GlobalIdType id (const typename GridImp::Traits::template Codim<cd>::Entity& e) const
    {
      return grid_.template getRealEntity<cd>(e).globalId();
    }

    //! get id of subentity
    template<int cc>
    GlobalIdType subid (const typename GridImp::Traits::template Codim<0>::Entity& e, int i) const
    {
      DUNE_THROW(NotImplemented, "UGGridGlobalIdSet::subid");
      //return grid.template getRealEntity<0>(e).template subPersistentIndex<cc>(i);
    }

    /** \todo Should be private */
    void update() {}

  private:

    const GridImp& grid_;
  };


  template<class GridImp>
  class UGGridLocalIdSet
  {
  public:
    //! define the type used for persistent local ids
    typedef uint LocalIdType;

    //! constructor stores reference to a grid
    UGGridLocalIdSet (const GridImp& g) : grid_(g) {}

    //! get id of an entity
    template<int cd>
    LocalIdType id (const typename GridImp::Traits::template Codim<cd>::Entity& e) const
    {
      return grid_.template getRealEntity<cd>(e).localId();
    }

    //! get id of subentity
    template<int cc>
    LocalIdType subid (const typename GridImp::Traits::template Codim<0>::Entity& e, int i) const
    {
      DUNE_THROW(NotImplemented, "UGGridLocalIdSet::subid");
      //return grid.template getRealEntity<0>(e).template subPersistentIndex<cc>(i);
    }

    /** \todo Should be private */
    void update() {}

  private:

    const GridImp& grid_;
  };


}  // namespace Dune


#endif
