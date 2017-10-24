// -*- tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#ifndef DUNE_PYTHON_GRID_FUNCTION_HH
#define DUNE_PYTHON_GRID_FUNCTION_HH

#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <dune/common/ftraits.hh>
#include <dune/common/visibility.hh>

#include <dune/python/common/dimrange.hh>
#include <dune/python/common/typeregistry.hh>
#include <dune/python/common/vector.hh>
#include <dune/python/function/simplegridfunction.hh>
#include <dune/python/grid/localview.hh>
#include <dune/python/grid/entity.hh>
#include <dune/python/grid/numpy.hh>
#include <dune/python/grid/object.hh>
#include <dune/python/grid/vtk.hh>

#include <dune/python/pybind11/numpy.h>
#include <dune/python/pybind11/pybind11.h>

namespace Dune
{

  namespace Python
  {

    // GridFunctionTraits
    // ------------------

    template< class GridFunction >
    struct GridFunctionTraits
      : public GridObjectTraits< GridFunction >
    {
      typedef typename GridObjectTraits< GridFunction >::LocalCoordinate LocalCoordinate;

      typedef std::decay_t< decltype( localFunction( std::declval< const GridFunction & >() ) ) > LocalFunction;
      typedef std::decay_t< decltype( std::declval< LocalFunction & >()( std::declval< const LocalCoordinate & >() ) ) > Range;
      static const unsigned int dimRange = GridFunction::dimRange;
    };



    namespace detail
    {

      template< class LocalCoordinate, class LocalFunction, class X >
      inline static auto callLocalFunction ( LocalFunction &&f, const X &x, PriorityTag< 1 > )
        -> decltype( pybind11::cast( f( x ) ) )
      {
        return pybind11::cast( f( x ) );
      }

      template< class LocalCoordinate, class LocalFunction >
      inline static pybind11::object callLocalFunction ( LocalFunction &&f, pybind11::array_t< typename FieldTraits< LocalCoordinate >::field_type > x, PriorityTag< 0 > )
      {
        return vectorize( [ &f ] ( const LocalCoordinate &x ) { return f( x ); }, x );
      }

      template< class LocalCoordinate, class LocalFunction, class Element >
      inline static pybind11::object callLocalFunction ( LocalFunction &&f, const CoordinateWrapper< Element > &x, PriorityTag< 0 > )
      {
        f.bind( x.entity() );
        pybind11::object result = vectorize( [ &f ] ( const LocalCoordinate &x ) { return f( x ); }, x.localPosition() );
        f.unbind();
        return result;
      }

      template< class LocalCoordinate, class LocalFunction, class Element >
      inline static pybind11::object callLocalFunction ( const LocalFunction &&f, const FVCoordinateWrapper< Element > &x, PriorityTag< 0 > )
      {
        f.bind( x.entity() );
        auto result = f( x.localPosition() );
        f.unbind();
        return pybind11::cast( result );
      }

      template< class LocalCoordinate, class LocalFunction, class X >
      inline static auto callLocalFunction ( LocalFunction &&f, const X &x )
        -> std::enable_if_t< !std::is_const< LocalFunction >::value, pybind11::object >
      {
        return callLocalFunction< LocalCoordinate >( std::forward< LocalFunction >( f ), x, PriorityTag< 42 >() );
      }

    } // namespace detail



    // registerGridFunction
    // --------------------

    template< class GridFunction, class... options >
    void registerGridFunction ( pybind11::handle scope, pybind11::class_< GridFunction, options... > cls )
    {
      using pybind11::operator""_a;

      typedef typename GridFunctionTraits< GridFunction >::Element Element;
      typedef typename GridFunctionTraits< GridFunction >::LocalCoordinate LocalCoordinate;
      typedef typename GridFunctionTraits< GridFunction >::LocalFunction LocalFunction;
      typedef typename GridFunctionTraits< GridFunction >::Range Range;

      typedef pybind11::array_t< typename FieldTraits< LocalCoordinate >::field_type > Array;

      // TODO subclassing from a non registered traits class not covered by TypeRegistry
      pybind11::class_< LocalFunction > clsLocalFunction( cls, "LocalFunction" );
      registerLocalView< Element >( clsLocalFunction );
      clsLocalFunction.def( "__call__", [] ( const LocalFunction &self, const LocalCoordinate &x ) {
          return detail::callLocalFunction< LocalCoordinate >( self, x );
        }, "x"_a );
      clsLocalFunction.def( "__call__", [] ( const LocalFunction &self, Array x ) {
          return detail::callLocalFunction< LocalCoordinate >( self, x );
        }, "x"_a );
      clsLocalFunction.def_property_readonly( "dimRange", [] ( pybind11::object self ) { return pybind11::int_( DimRange< Range >::value ); } );

      cls.def_property_readonly( "grid", [] ( const GridFunction &self ) { return gridView( self ); } );
      cls.def_property_readonly( "dimRange", [] ( pybind11::object self ) { return pybind11::int_( DimRange< Range >::value ); } );

      cls.def( "localFunction", [] ( const GridFunction &self ) { return localFunction( self ); }, pybind11::keep_alive< 0, 1 >() );

      cls.def( "addToVTKWriter", &addToVTKWriter< GridFunction >, pybind11::keep_alive< 3, 1 >(), "name"_a, "writer"_a, "dataType"_a );

      cls.def( "cellData", [] ( const GridFunction &self, int level ) { return cellData( self, level ); }, "level"_a = 0 );
      cls.def( "pointData", [] ( const GridFunction &self, int level ) { return pointData( self, level ); }, "level"_a = 0 );

      cls.def( "__call__", [] ( const GridFunction &self, const FVCoordinateWrapper< Element > &x ) {
          return detail::callLocalFunction< LocalCoordinate >( localFunction( self ), x );
        }, "x"_a );
      cls.def( "__call__", [] ( const GridFunction &self, const CoordinateWrapper< Element > &x ) {
          return detail::callLocalFunction< LocalCoordinate >( localFunction( self ), x );
        }, "x"_a );
    }



    namespace detail
    {

      // PyGridFunctionEvaluator
      // -----------------------

      template <class GridView, int dimR>
      struct DUNE_PRIVATE PyGridFunctionEvaluator
      {
        static const unsigned int dimRange = (dimR ==0 ? 1 : dimR);

        typedef typename GridView::template Codim< 0 >::Entity Entity;
        typedef typename Entity::Geometry::LocalCoordinate Coordinate;

        typedef typename std::conditional< dimR == 0, double, Dune::FieldVector< double, dimRange > >::type Value;

        explicit PyGridFunctionEvaluator ( pybind11::function evaluate ) : evaluate_( evaluate ) {}

        Value operator() ( const Entity &entity, const Coordinate &x ) const
        {
          pybind11::gil_scoped_acquire acq;
          return pybind11::cast< Value >( evaluate_( FVCoordinateWrapper< Entity >( entity, x ) ) );
        }

        pybind11::array_t< double > operator() ( const Entity &entity, const CoordinateWrapper< Entity > &x ) const
        {
          pybind11::gil_scoped_acquire acq;
          return pybind11::cast< pybind11::array_t< double > >( evaluate_( x ) );
        }

      private:
        pybind11::function evaluate_;
      };

      // registerPyGridFunction
      // ----------------------

      template< class GridView, int dimRange >
      auto registerPyGridFunction ( pybind11::handle scope, const std::string &name, std::integral_constant< int, dimRange > )
      {
        typedef PyGridFunctionEvaluator<GridView,dimRange> Evaluator;
        typedef SimpleGridFunction< GridView, Evaluator > GridFunction;
        addToTypeRegistry<Evaluator>(GenerateTypeName("Dune::Python::detail::PyGridFunctionEvaluator",
                                            MetaType<GridView>(),dimRange),
                         IncludeFiles{"dune/python/grid/function.hh"});

        std::string clsName = name + std::to_string( dimRange );
        auto gf = insertClass< GridFunction >( scope, clsName,
                  GenerateTypeName("Dune::Python::SimpleGridFunction",
                                      MetaType<GridView>(), Dune::MetaType<Evaluator>()),
            IncludeFiles{"dune/python/grid/function.hh"}).first;
        registerGridFunction( scope, gf );
        return gf;
      }



      // pyGlobalGridFunction
      // --------------------

      template< class GridView, int dimRange >
      pybind11::object pyGridFunction ( const GridView &gridView, pybind11::function evaluate, pybind11::object parent )
      {
        auto gridFunction = simpleGridFunction( gridView, PyGridFunctionEvaluator< GridView, dimRange >( std::move( evaluate ) ) );
        return pybind11::cast( std::move( gridFunction ), pybind11::return_value_policy::move, parent );
      }

    } // namespace detail



    // defGridFunction
    // ---------------

    template< class GridView, int... dimRange >
    auto defGridFunction ( pybind11::handle scope, std::string name, std::integer_sequence< int, dimRange... > )
    {
      std::ignore = std::make_tuple( detail::registerPyGridFunction< GridView >( scope, name, std::integral_constant< int, dimRange >() )... );

      typedef std::function< pybind11::object( const GridView &, pybind11::function, pybind11::object ) > Dispatch;
      std::array< Dispatch, sizeof...( dimRange ) > dispatch = {{ Dispatch( detail::pyGridFunction< GridView, dimRange > )... }};

      return [ dispatch ] ( pybind11::object gp, pybind11::function evaluate ) {
          const GridView &gridView = gp.cast< const GridView & >();
          int dimR = -1;
          if( gridView.template begin< 0 >() != gridView.template end< 0 >() )
          {
            typedef typename GridView::template Codim< 0 >::Entity Entity;
            typename Entity::Geometry::LocalCoordinate x( 0 );
            pybind11::gil_scoped_acquire acq;
            pybind11::object v( evaluate( FVCoordinateWrapper< Entity >( *gridView.template begin< 0 >(), x ) ) );
            try
            {
              dimR = len( v );
            }
            catch( std::runtime_error )
            {
              pybind11::error_already_set(); // .clear(); //????
              // we have to assume it's a double so we test for that
              (void) v.template cast<double>();
              dimR = 0;
            }
          }
          dimR = gridView.comm().max( dimR );
          if( dimR < 0 )
            DUNE_THROW( InvalidStateException, "Cannot create local grid function on empty grid" );
          if( static_cast< std::size_t >( dimR ) >= dispatch.size() )
            DUNE_THROW( NotImplemented, "gridFunction not implemented for dimRange = " + std::to_string( dimR ) );
          return dispatch[ static_cast< std::size_t >( dimR ) ]( gridView, std::move( evaluate ), std::move( gp ) );
        };
    }

  } // namespace Python

} // namespace Dune

#endif // #ifndef DUNE_PYTHON_GRID_FUNCTION_HH
