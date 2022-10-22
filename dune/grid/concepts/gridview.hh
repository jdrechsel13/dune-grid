// SPDX-FileCopyrightText: Copyright © DUNE Project contributors, see file LICENSE.md in module root
// SPDX-License-Identifier: LicenseRef-GPL-2.0-only-with-DUNE-exception
// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#ifndef DUNE_GRID_CONCEPTS_GRIDVIEW_HH
#define DUNE_GRID_CONCEPTS_GRIDVIEW_HH

#include <concepts>
#include <cstddef>
#include <utility>

#include <dune/grid/common/capabilities.hh>
#include <dune/grid/common/gridenums.hh>
#include <dune/grid/concepts/entityiterator.hh>
#include <dune/grid/concepts/geometry.hh>
#include <dune/grid/concepts/indexidset.hh>
#include <dune/grid/concepts/intersection.hh>
#include <dune/grid/concepts/intersectioniterator.hh>
#include <dune/grid/concepts/archetypes/datahandle.hh>

namespace Dune::Concept {
namespace Impl {

  template<class GV, int codim, Dune::PartitionIteratorType partition,
    class Traits = typename GV::template Codim<codim>::template Partition<partition>>
  concept GridViewPartition =
    EntityIterator<typename Traits::Iterator> &&
  requires(const GV gv)
  {
    { gv.template begin<codim,partition>() } -> std::convertible_to<typename Traits::Iterator>;
    { gv.template end<codim,partition>()   } -> std::convertible_to<typename Traits::Iterator>;
  };

  template<class GV, int codim,
    class Traits = typename GV::template Codim<codim>>
  concept GridViewCodim =
    Geometry<typename Traits::Geometry> &&
    Geometry<typename Traits::LocalGeometry> &&
    EntityIterator<typename Traits::Iterator> &&
  requires(const GV gv)
  {
    { gv.template begin<codim>() } -> std::convertible_to<typename Traits::Iterator>;
    { gv.template end<codim>()   } -> std::convertible_to<typename Traits::Iterator>;

    requires (codim != 0) || requires(const typename Traits::Entity& entity)
    {
      { gv.ibegin(entity) } -> std::convertible_to<typename GV::IntersectionIterator>;
      { gv.iend(entity)   } -> std::convertible_to<typename GV::IntersectionIterator>;
    };
  };

  template<class GV, int codim>
  concept GridViewAllPartitions = GridViewCodim<GV,codim> &&
    GridViewPartition<GV,codim,Dune::PartitionIteratorType::InteriorBorder_Partition> &&
    GridViewPartition<GV,codim,Dune::PartitionIteratorType::Overlap_Partition> &&
    GridViewPartition<GV,codim,Dune::PartitionIteratorType::OverlapFront_Partition> &&
    GridViewPartition<GV,codim,Dune::PartitionIteratorType::All_Partition> &&
    GridViewPartition<GV,codim,Dune::PartitionIteratorType::Ghost_Partition>;

  template<class GV, class Grid, int codim>
    requires Dune::Capabilities::hasEntityIterator<Grid,codim>::v
  void requireGridViewCodim()
    requires GridViewAllPartitions<GV,codim> {}

  template<class GV, class Grid, int codim>
    requires (not Dune::Capabilities::hasEntityIterator<Grid,codim>::v)
  void requireGridViewCodim() {}

  template <class GV, int dim>
  concept GridViewAllCodims = requires(std::make_integer_sequence<int,dim+1> dims)
  {
    []<int... d>(std::integer_sequence<int,d...>) requires
      requires { (requireGridViewCodim<GV,typename GV::Grid,(dim-d)>(),...); } {} (dims);
  };

} // end namespace Impl

/**
 * @brief Model of a grid view
 * @ingroup GridConcepts
 * @details Dune::GridView is a template for this model
 */
template<class GV>
concept GridView = std::copyable<GV> &&
  IndexSet<typename GV::IndexSet> &&
  Intersection<typename GV::Intersection> &&
  IntersectionIterator<typename GV::IntersectionIterator> &&
requires(const GV gv, int codim, Dune::GeometryType type)
{
  typename GV::Traits;
  typename GV::ctype;
  { GV::conforming        } -> std::convertible_to<bool>;
  { GV::dimension         } -> std::convertible_to<int>;
  { GV::dimensionworld    } -> std::convertible_to<int>;
  { gv.grid()             } -> std::convertible_to<const typename GV::Grid&>;
  { gv.indexSet()         } -> std::convertible_to<const typename GV::IndexSet&>;
  { gv.size(codim)        } -> std::convertible_to<int>;
  { gv.size(type)         } -> std::convertible_to<int>;
  { gv.comm()             } -> std::convertible_to<typename GV::CollectiveCommunication>;
  { gv.overlapSize(codim) } -> std::convertible_to<int>;
  { gv.ghostSize(codim)   } -> std::convertible_to<int>;

  requires requires(Archetypes::CommDataHandle<std::byte>& handle,
                    InterfaceType iface, CommunicationDirection dir)
  {
    gv.communicate(handle, iface, dir);
  };
} && Impl::GridViewAllCodims<GV, GV::dimension>;

}  // end namespace Dune::Concept

#endif // DUNE_GRID_CONCEPTS_GRIDVIEW_HH
