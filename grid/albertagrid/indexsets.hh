// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#ifndef DUNE_ALBERTAGRIDINDEXSETS_HH
#define DUNE_ALBERTAGRIDINDEXSETS_HH

#if HAVE_ALBERTA

#include <dune/common/stdstreams.hh>

#include <dune/grid/common/grid.hh>
#include <dune/grid/common/indexidset.hh>

#include <dune/grid/albertagrid/albertaheader.hh>
#include <dune/grid/albertagrid/misc.hh>
#include <dune/grid/albertagrid/referencetopo.hh>
#include <dune/grid/albertagrid/dofadmin.hh>
#include <dune/grid/albertagrid/dofvector.hh>
#include <dune/grid/albertagrid/elementinfo.hh>

namespace Dune
{

  // External Forward Declarations
  // -----------------------------

  template< int dim, int dimworld >
  class AlbertaGrid;

  template< int codim, int dim, class GridImp >
  class AlbertaGridEntity;

  template< class GridType, int dim >
  struct MarkEdges;



  //! HierarchicIndexSet uses LeafIterator types for all codims and partition types
  template <class GridImp>
  struct AlbertaGridHierarchicIteratorTypes
  {
    //! The types of the iterator
    template<int cd>
    struct Codim
    {
      template<PartitionIteratorType pitype>
      struct Partition
      {
        /*
           We use the remove_const to extract the Type from the mutable class,
           because the const class is not instantiated yet.
         */
        typedef typename remove_const<GridImp>::type::Traits::template Codim<cd>::template Partition<pitype>::LeafIterator Iterator;
      };
    };
  };



  // AlbertaGridHierarchicIndexSet
  // -----------------------------

  template< int dim, int dimworld >
  class AlbertaGridHierarchicIndexSet
    : public IndexSetDefaultImplementation
      < AlbertaGrid< dim, dimworld >,
          AlbertaGridHierarchicIndexSet< dim,dimworld >,
          AlbertaGridHierarchicIteratorTypes< AlbertaGrid< dim, dimworld > > >
  {
    typedef AlbertaGridHierarchicIndexSet< dim, dimworld > This;

    typedef AlbertaGrid< dim, dimworld > Grid;

    typedef typename Grid::Traits Traits;

    static const int dimension = Grid::dimension;

    enum { numVecs  = AlbertHelp::numOfElNumVec };

    friend class AlbertaGrid< dim, dimworld >;
    friend class MarkEdges< Grid, 3 >;
    friend class MarkEdges< const Grid, 3 >;

    explicit AlbertaGridHierarchicIndexSet ( const Grid &grid );

    template< int codim >
    class DofAccess;

    class InitEntityNumber;

    template< int codim >
    class RefineNumbering;

    template< int codim >
    class CoarsenNumbering;

  public:
    enum { ncodim = dimension + 1 };

    //! return true if entity is contained in set
    template< class Entity >
    bool contains ( const Entity & ) const
    {
      return true;
    }

    //! return index of entity
    template< class Entity >
    int index ( const Entity &entity ) const
    {
      const int codim = Entity::codimension;
      const AlbertaGridEntity< codim, dim, const Grid > &entityImp
        = Grid::getRealImplementation( entity );

      Int2Type< codim > codimVariable;
      return subIndex( codimVariable, entityImp.elementInfo().el(), entityImp.getFEVnum() );
    }

    //! return subIndex of given enitiy's sub entity
    template< int codim >
    int subIndex ( const typename Traits::template Codim< 0 >::Entity &entity, int i ) const
    {
      const AlbertaGridEntity< 0, dim, const Grid > &entityImp
        = Grid::getRealImplementation( entity );

      Int2Type< codim > codimVariable;
      return subIndex( codimVariable, entityImp.elementInfo().el(), i );
    }

    //! return size of set for given GeometryType
    int size ( GeometryType type ) const
    {
      return (type.isSimplex() ? size( dimension - type.dim() ) : 0);
    }

    //! return size of set
    int size ( int codim ) const
    {
      assert( (codim >= 0) && (codim <= dimension) );
      if( codim == dimension )
        return grid_.getMesh()->n_vertices;
      else
        return indexStack_[ codim ].size();
    }

    //! return geometry types this set has indices for
    const std::vector< GeometryType > &geomTypes( int codim ) const
    {
      assert( (codim >= 0) && (codim <= dimension) );
      return geomTypes_[ codim ];
    }

#ifdef INDEXSET_HAS_ITERATORS
    /** @brief Iterator to one past the last entity of given codim for partition type
     */
    template<int cd, PartitionIteratorType pitype>
    typename AlbertaGridHierarchicIteratorTypes<Grid>::template Codim<cd>::
    template Partition<pitype>::Iterator end () const
    {
      return grid_.template leafend<cd,pitype> ();
    }

    /** @brief Iterator to first entity of given codimension and partition type.
     */
    template<int cd, PartitionIteratorType pitype>
    typename AlbertaGridHierarchicIteratorTypes<Grid>::template Codim<cd>::
    template Partition<pitype>::Iterator begin () const
    {
      return grid_.template leafbegin<cd,pitype> ();
    }
#endif

    template< int codim >
    int subIndex ( Int2Type< codim > codimVariable,
                   const Alberta::Element *element, int i ) const
    {
      if( (codim < dimension) && (codim == 2) )
        i = refTopo_.dune2albertaEdge( i );

      int dof = dofAccess_[ codimVariable ]( element, i );
      if( codim < dimension )
      {
        int *array = (int *)entityNumbers_[ codim ];
        return array[ dof ];
      }
      else
        return dof;
    }

    void create ( const Alberta::HierarchyDofNumbering< dimension > &dofNumbering )
    {
      createEntityNumbers< 0 >( dofNumbering );
      createEntityNumbers< 1 >( dofNumbering );
      if( dimension == 3 )
        createEntityNumbers< 2 >( dofNumbering );

      Int2Type< dimension > dimVariable;
      dofAccess_[ dimVariable ] = DofAccess< dimension >( dofNumbering.dofSpace( dimension ) );
    }

    void read ( const std::string &filename,
                const Alberta::MeshPointer< dimension > &mesh )
    {
      for( int i = 0; i < dimension; ++i )
      {
        std::ostringstream s;
        s << filename << ".cd" << i;
        entityNumbers_[ i ].read( s.str(), mesh );

        const int maxIdx = AlbertHelp::calcMaxIndex( entityNumbers_[ i ] );
        indexStack_[ i ].setMaxIndex( maxIdx );
      }

      DUNE_THROW( AlbertaIOError, "We need to set dofAccess_ here, too" );
    }

    bool write ( const std::string &filename ) const
    {
      bool success = true;
      for( int i = 0; i < dimension; ++i )
      {
        std::ostringstream s;
        s << filename << ".cd" << i;
        success &= entityNumbers_[ i ].write( s.str() );
      }
      return success;
    }

    void release ()
    {
      for( int i = 0; i < dimension; ++i )
        entityNumbers_[ i ].release();
    }

  private:
    template< int codim >
    void createEntityNumbers ( const Alberta::HierarchyDofNumbering< dimension > &dofNumbering )
    {
      assert( (codim >= 0) && (codim < AlbertHelp::numOfElNumVec) );
      Int2Type< codim > codimVariable;

      const Alberta::DofSpace *dofSpace = dofNumbering.dofSpace( codim );
      dofAccess_[ codimVariable ] = DofAccess< codim >( dofSpace );

      std::ostringstream s;
      s << "Numbering for codimension " << codim;
      Alberta::DofVectorPointer< int > &entityNumbers = entityNumbers_[ codim ];
      entityNumbers.create( dofSpace, s.str() );

      InitEntityNumber init( indexStack_[ codim ] );
      entityNumbers.forEach( init );

      entityNumbers.template setupInterpolation< RefineNumbering< codim > >();
      entityNumbers.template setupRestriction< CoarsenNumbering< codim > >();
    }

  private:
    // the grid this index set belongs to
    const Grid &grid_;

    // index stacks providing new numbers during adaptation
    IndexManagerType indexStack_[ AlbertHelp::numOfElNumVec ];

    // dof vectors storing the (persistent) numbering
    Alberta::DofVectorPointer< int > entityNumbers_[ AlbertHelp::numOfElNumVec ];

    // access to the dof vectors
    Alberta::CodimTable< DofAccess, dim > dofAccess_;

    // constains the mapping from dune to alberta numbers
    const ALBERTA AlbertHelp :: AlbertaGridReferenceTopology<dim> refTopo_;

    // all geometry types contained in the grid
    std::vector< GeometryType > geomTypes_[ dimension+1 ];
  };



  template< int dim, int dimworld >
  inline AlbertaGridHierarchicIndexSet< dim, dimworld >
  ::AlbertaGridHierarchicIndexSet ( const Grid &grid )
    : grid_( grid )
  {
    for( int codim = 0; codim <= dimension; ++codim )
    {
      const GeometryType type( GeometryType::simplex, dimension - codim );
      geomTypes_[ codim ].push_back( type );
    }
  }



  // AlbertaGridHierarchicIndexSet::DofAccess
  // ----------------------------------------

  template< int dim, int dimworld >
  template< int codim >
  class AlbertaGridHierarchicIndexSet< dim, dimworld >::DofAccess
    : public Alberta::DofAccess< dim, codim >
  {
    typedef Alberta::DofAccess< dim, codim > Base;

  public:
    DofAccess ()
    {}

    explicit DofAccess ( const Alberta::DofSpace *dofSpace )
      : Base( dofSpace )
    {}
  };



  // AlbertaGridHierarchicIndexSet::InitEntityNumber
  // -----------------------------------------------

  template< int dim, int dimworld >
  class AlbertaGridHierarchicIndexSet< dim, dimworld >::InitEntityNumber
  {
    IndexManagerType &indexStack_;

  public:
    InitEntityNumber ( IndexManagerType &indexStack )
      : indexStack_( indexStack )
    {}

    void operator() ( int &dof )
    {
      dof = indexStack_.getIndex();
    }
  };



  // AlbertaGridHierarchicIndexSet::RefineNumbering
  // ----------------------------------------------

  template< int dim, int dimworld >
  template< int codim >
  struct AlbertaGridHierarchicIndexSet< dim, dimworld >::RefineNumbering
  {
    static const int dimension = dim;
    static const int codimension = codim;

  private:
    typedef Alberta::DofVectorPointer< int > DofVectorPointer;
    typedef Alberta::DofAccess< dimension, codimension > DofAccess;

    IndexManagerType &indexStack_;
    DofVectorPointer dofVector_;
    DofAccess dofAccess_;

    RefineNumbering ( IndexManagerType &indexStack,
                      const DofVectorPointer &dofVector )
      : indexStack_( indexStack ),
        dofVector_( dofVector ),
        dofAccess_( dofVector.dofSpace() )
    {}

  public:
    void operator() ( const Alberta::Element *child, int subEntity )
    {
      int *const array = (int *)dofVector_;
      const int dof = dofAccess_( child, subEntity );
      array[ dof ] = indexStack_.getIndex();
    }

    static void interpolateVector ( const DofVectorPointer &dofVector,
                                    const Alberta::Patch &patch )
    {
      IndexManagerType *indexStackPtr = AlbertHelp::tmpIndexStack[ codim ];
      assert( indexStackPtr != 0 );
      RefineNumbering refineNumbering( *indexStackPtr, dofVector );
      patch.forEachInternalSubChild( refineNumbering );
    }
  };



  // AlbertaGridHierarchicIndexSet::CoarsenNumbering
  // -----------------------------------------------

  template< int dim, int dimworld >
  template< int codim >
  struct AlbertaGridHierarchicIndexSet< dim, dimworld >::CoarsenNumbering
  {
    static const int dimension = dim;
    static const int codimension = codim;

  private:
    typedef Alberta::DofVectorPointer< int > DofVectorPointer;
    typedef Alberta::DofAccess< dimension, codimension > DofAccess;

    IndexManagerType &indexStack_;
    DofVectorPointer dofVector_;
    DofAccess dofAccess_;

    CoarsenNumbering ( IndexManagerType &indexStack,
                       const DofVectorPointer &dofVector )
      : indexStack_( indexStack ),
        dofVector_( dofVector ),
        dofAccess_( dofVector.dofSpace() )
    {}

  public:
    void operator() ( const Alberta::Element *child, int subEntity )
    {
      int *const array = (int *)dofVector_;
      const int dof = dofAccess_( child, subEntity );
      indexStack_.freeIndex( array[ dof ] );
#ifndef NDEBUG
      array[ dof ] = -1;
#endif
    }

    static void restrictVector ( const DofVectorPointer &dofVector,
                                 const Alberta::Patch &patch )
    {
      IndexManagerType *indexStackPtr = AlbertHelp::tmpIndexStack[ codim ];
      assert( indexStackPtr != 0 );
      CoarsenNumbering coarsenNumbering( *indexStackPtr, dofVector );
      patch.forEachInternalSubChild( coarsenNumbering );
    }
  };



  // AlbertaGridIdSet
  // ----------------

  //! hierarchic index set of AlbertaGrid
  template< int dim, int dimworld >
  class AlbertaGridIdSet
    : public IdSetDefaultImplementation
      < AlbertaGrid< dim, dimworld >, AlbertaGridIdSet< dim, dimworld >, unsigned int >
  {
    typedef AlbertaGridIdSet< dim, dimworld > This;
    typedef AlbertaGrid< dim, dimworld > Grid;
    typedef IdSetDefaultImplementation< Grid, This, unsigned int > Base;

    friend class AlbertaGrid< dim, dimworld >;

    static const int codimShift = 30;
    static const int maxCodimSize = (1 << codimShift);

    typedef typename Grid::HierarchicIndexSet HierarchicIndexSet;

    const HierarchicIndexSet &hset_;

    //! create id set, only allowed for AlbertaGrid
    AlbertaGridIdSet ( const Grid &grid )
      : hset_( grid.hierarchicIndexSet() )
    {}

  public:
    //! export type of id
    typedef typename Base::IdType IdType;

    /** \copydoc IdSet::id(const EntityType &e) const */
    template< class Entity >
    IdType id ( const Entity &e ) const
    {
      const int codim = Entity::codimension;
      return id< codim >( e );
    }

    /** \copydoc IdSet::id(const typename remove_const<GridImp>::type::Traits::template Codim<cc>::Entity &e) const */
    template< int codim >
    IdType id ( const typename Grid::template Codim< codim >::Entity &e ) const
    {
      assert( hset_.size( codim ) < maxCodimSize );
      const IdType index = hset_.index( e );
      return ((IdType)codim << codimShift) + index;
    }

    /** \copydoc IdSet::subId(const typename remove_const<GridImp>::type::Traits::template Codim<0>::Entity &e,int i) const */
    template< int codim >
    IdType subId ( const typename Grid::template Codim< 0 >::Entity &e, int i ) const
    {
      assert( hset_.size( codim ) < maxCodimSize );
      const IdType index = hset_.template subIndex< codim >( e, i );
      return ((IdType)codim << codimShift) + index;
    }
  };

} // namespace Dune

#endif // HAVE_ALBERTA

#endif
