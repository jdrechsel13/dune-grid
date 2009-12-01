// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#include <config.h>

#define DISABLE_DEPRECATED_METHOD_CHECK 1
#define NEW_SUBENTITY_NUMBERING 1

#include <dune/grid/test/gridcheck.cc>
#include "../dgfgridtype.hh"

#if HAVE_GRAPE
#include <dune/grid/io/visual/grapegriddisplay.hh>
#include <dune/grid/io/visual/grapedatadisplay.hh>
#endif
#include <dune/grid/io/file/vtk/vtkwriter.hh>
#include <dune/grid/io/file/vtk/subsamplingvtkwriter.hh>

namespace Dune
{

  template< int dim, int dimworld >
  class AlbertaGrid;

}

using namespace Dune;

template< int dim, int dimworld >
struct EnableLevelIntersectionIteratorCheck< AlbertaGrid< dim, dimworld > >
{
  static const bool v = false;
};

template< class GridView >
void display ( const std::string &name,
               const GridView &view,
               std::vector< double > &elDat, int nofElParams,
               std::vector< double > &vtxDat, int nofVtxParams )
{
#if HAVE_GRAPE
  if( nofElParams + nofVtxParams > 0 )
  {
    GrapeDataDisplay< typename GridView::Grid > disp( view );
    disp.addVector( "el. Paramters", elDat, view.indexSet(), 0.0, 0, nofElParams, false );
    disp.addVector( "vtx. Paramters", vtxDat, view.indexSet(), 0.0, 1, nofVtxParams, true );
    disp.display();
  }
  else
  {
    GrapeGridDisplay< typename GridView::Grid > disp( view.grid() );
    disp.display();
  }
#endif // #if HAVE_GRAPE
  VTKWriter<GridView> vtkWriter(view);
  // SubsamplingVTKWriter<GridView> vtkWriter(view,6);
  if( nofElParams + nofVtxParams > 0 )
  {
    vtkWriter.addCellData( elDat, "el. Parameters", nofElParams );
    vtkWriter.addVertexData( vtxDat, "vtx. Parameters", nofVtxParams );
  }
  vtkWriter.write( name );
}

template< class GridView >
void test ( const GridView &view )
{
  gridcheck( const_cast< typename GridView::Grid & >( view.grid() ) );
}

int main(int argc, char ** argv, char ** envp)
try {
  // this method calls MPI_Init, if MPI is enabled
  MPIHelper & mpiHelper = MPIHelper::instance(argc,argv);
  int myrank = mpiHelper.rank();

  if (argc<2)
  {
    std::cerr << "supply grid file as parameter!" << std::endl;
    return 1;
  }

  std::cout << "tester: start grid reading" << std::endl;

  typedef GridType::LeafGridView GridView;
  typedef GridView::IndexSet IndexSetType;

  // create Grid from DGF parser
  GridType *grid;
  size_t nofElParams( 0 ), nofVtxParams( 0 );
  std::vector< double > eldat( 0 ), vtxdat( 0 );
  {
    GridPtr< GridType > gridPtr( argv[1], mpiHelper.getCommunicator() );

    GridView gridView = gridPtr->leafView();
    const IndexSetType &indexSet = gridView.indexSet();
    nofElParams = gridPtr.nofParameters( 0 );
    if( nofElParams > 0 )
    {
      std::cout << "Reading Element Parameters:" << std::endl;
      eldat.resize( indexSet.size(0) * nofElParams );
      typedef GridView::Codim< 0 >::Iterator IteratorType;
      const IteratorType endit = gridView.end< 0 >();
      for( IteratorType iter = gridView.begin< 0 >(); iter != endit; ++iter )
      {
        const std::vector< double > &param = gridPtr.parameters( *iter );
        assert( param.size() == nofElParams );
        for( size_t i = 0; i < nofElParams; ++i )
        {
          //std::cout << param[i] << " ";
          eldat[ indexSet.index(*iter) * nofElParams + i ] = param[ i ];
        }
        //std::cout << std::endl;
      }
    }

    nofVtxParams = gridPtr.nofParameters( GridType::dimension );
    if( nofVtxParams > 0 )
    {
      std::cout << "Reading Vertex Parameters:" << std::endl;
      vtxdat.resize( indexSet.size( GridType::dimension ) * nofVtxParams );
      typedef GridView::Codim< GridType::dimension >::Iterator IteratorType;
      IteratorType endit = gridView.end< GridType::dimension >();
      for( IteratorType iter = gridView.begin< GridType::dimension >(); iter != endit; ++iter )
      {
        const std::vector< double > &param = gridPtr.parameters( *iter );
        assert( param.size() == nofVtxParams );
        // std::cout << (*iter).geometry()[0] << " -\t ";
        for( size_t i = 0; i < nofVtxParams; ++i )
        {
          // std::cout << param[i] << " ";
          vtxdat[ indexSet.index(*iter) * nofVtxParams + i ] = param[ i ];
        }
        // std::cout << std::endl;
      }
    }

    grid = gridPtr.release();
  }

  GridView gridView = grid->leafView();
  // display
  if( myrank <= 0 )
    display( argv[1] , gridView, eldat, nofElParams, vtxdat, nofVtxParams );
  // refine
  std::cout << "tester: refine grid" << std::endl;
  grid->globalRefine(Dune::DGFGridInfo<GridType>::refineStepsForHalf());
  test(gridView);

  delete grid;
  return 0;
}
catch( const Dune::Exception &e )
{
  std::cerr << e << std::endl;
  return 1;
}
catch (...)
{
  std::cerr << "Generic exception!" << std::endl;
  return 1;
}
