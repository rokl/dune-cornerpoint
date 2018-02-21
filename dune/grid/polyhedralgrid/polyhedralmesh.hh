#ifndef DUNE_POLYHEDRALGRID_MESH_HH
#define DUNE_POLYHEDRALGRID_MESH_HH

#include <dune/common/fvector.hh>

namespace Dune
{
  template <int dim, int dimworld, class T = double, class I = int>
  class PolyhedralMesh
  {
    static constexpr int volumeData   = 0;
    static constexpr int centroidData = 1;
    static constexpr int normalData = centroidData + dimworld;

    static constexpr int elementData = centroidData + dimworld;
    static constexpr int faceData = normalData + dimworld;
  public:
    typedef T  ctype;
    typedef I  Index;

    typedef Dune::FieldVector< ctype, dimworld > GlobalCoordinate ;

    PolyhedralMesh ()
      : coordinates_(),
        centroids_( 2 ),
        faceNormals_(),
        subEntity_( 1 ),
        subEntityPos_( 1, std::vector<Index>(1, Index(0)) ),
        topology_( dim+1 ),
        topologyPos_( dim+1, std::vector<Index>(1, Index(0)) ),
        volumes_( dim-1 ),
        geomTypes_()
    {}

    template <class UnstructuredGrid>
    PolyhedralMesh (const UnstructuredGrid& ug)
      : coordinates_(),
        centroids_( 2 ),
        faceNormals_(),
        subEntity_( 1 ),
        subEntityPos_( 1, std::vector<Index>(1, Index(0)) ),
        topology_( dim+1 ),
        topologyPos_( dim+1, std::vector<Index>(1, Index(0)) ),
        volumes_( dim-1 ),
        geomTypes_()
    {
      coordinates_.resize( ug.number_of_nodes );
      centroids_[ 0 ].resize( ug.number_of_cells );
      volumes_[ 0 ].resize( ug.number_of_cells );

      centroids_[ 1 ].resize( ug.number_of_faces );
      faceNormals_.resize( ug.number_of_faces );
      volumes_[ 1 ].resize( ug.number_of_faces );
      GeometryType tmp;
      tmp.makeNone( dim );
      geomTypes_.resize( ug.number_of_cells, tmp );

      int id = 0;
      for( int i=0; i<ug.number_of_nodes; ++i )
      {
        GlobalCoordinate& coord = coordinates_[ i ];
        for(int d=0; d<dimworld; ++d, ++id )
          coord[ d ] = ug.node_coordinates[ id ];
      }

      id = 0;
      for( int i=0; i<ug.number_of_cells; ++i )
      {
        volumes_[ 0 ][ i ] = ug.cell_volumes[ i ];
        GlobalCoordinate& center = centroids_[ 0 ][ i ];
        for(int d=0; d<dimworld; ++d, ++id )
          center[ d ] = ug.cell_centroids[ id ];
      }

      id = 0;
      for( int i=0; i<ug.number_of_faces; ++i )
      {
        volumes_[ 1 ][ i ] = ug.face_areas[ i ];
        GlobalCoordinate& normal = faceNormals_[ i ];
        for(int d=0; d<dimworld; ++d, ++id )
          normal[ d ] = ug.face_normals[ id ];
      }

      topology_[ 1 ].resize( ug.face_nodepos[ug.number_of_faces] );
      std::copy_n( ug.face_nodes, ug.face_nodepos[ug.number_of_faces], topology_[ 1 ].begin() );

      subEntity_[ 0 ].resize( ug.cell_facepos[ug.number_of_cells ] );
      std::copy_n( ug.cell_faces, ug.cell_facepos[ug.number_of_cells], subEntity_[0].begin() );

      faceNeighbors_.resize( 2*ug.number_of_faces );
      std::copy_n( ug.face_cells, 2*ug.number_of_faces, faceNeighbors_.begin() );
    }

    /** \brief return number of cells in the mesh */
    Index size( const int codim ) const { return topology_[ codim ].size(); }

    GlobalCoordinate& coordinate( const Index idx ) { return coordinates_[ idx ]; }
    const GlobalCoordinate& coordinate( const Index idx ) const { return coordinates_[ idx ]; }

    std::pair< Index, Index* > entity( const Index en, const int codim )
    {
      return std::make_pair( topologyPos_[ codim ][ en+1 ] - topologyPos_[ codim ][ en ], topology_[ codim ].data() + topologyPos_[ codim ][ en ] );
    }

    std::pair< Index, Index* > subEntity( const Index en, const int i, const int codim )
    {
      // TODO:
      return std::make_pair( topologyPos_[ codim ][ en+1 ] - topologyPos_[ codim ][ en ], topology_[ codim ].data() + topologyPos_[ codim ][ en ] );
    }

    std::pair< Index, Index* > element( const Index en )
    {
      return entity( en, 0 );
    }

    ctype volume( const Index& entity, const int codim ) const
    {
      return volumes_[ codim ][ entity ];
    }

    GlobalCoordinate& center( const Index& entity, const int codim )
    {
      return centroids_[ codim ][ entity ];
    }

    const GlobalCoordinate& center( const Index& entity, const int codim ) const
    {
      return centroids_[ codim ][ entity ];
    }

    GlobalCoordinate& faceNormal( const Index& face)
    {
      return faceNormals_[ face ];
    }

    const GlobalCoordinate& faceNormal( const Index& face) const
    {
      return faceNormals_[ face ];
    }

    void insertVertex( const GlobalCoordinate& vertex )
    {
      coordinates_.push_back( vertex );
    }

    void insertElement( const GeometryType& type, const std::vector<unsigned int>& items )
    {
      // could be face or element
      const int codim = dim - type.dim();
      if( codim == 1 ) // face
      {
        insertItems( items, topology_[ codim ], topologyPos_[ codim ] );
      }
      else if( codim == 0 )
      {
        // store geometry type of element
        geomTypes_.push_back( type );
        // for polyhedral cells items are the faces
        if( type.isNone() )
        {
          insertItems( items, subEntity_[ codim ], subEntityPos_[ codim ] );
        }
        else
        {
          // otherwise items are vertices
          insertItems( items, topology_[ codim ], topologyPos_[ codim ] );
        }
      }

      // TODO: compute volume and normals etc.
    }

  protected:
    void insertItems( const std::vector<unsigned int>& items,
                      std::vector< Index >& entities,
                      std::vector< Index >& entityPos )
    {
        const int iSize = items.size();
        entities.reserve( entities.size() + items.size() );
        assert( entityPos.back() == items.size() );
        for( int i=0; i<iSize; ++i )
        {
          entities.push_back( Index(items[ i ]) );
        }

        // store position
        entityPos.push_back( entities.size() );
    }

    std::vector< GlobalCoordinate > coordinates_;
    std::vector< std::vector< GlobalCoordinate > > centroids_;
    std::vector< GlobalCoordinate > faceNormals_;

    std::vector< std::vector< Index > > subEntity_;
    std::vector< std::vector< Index > > subEntityPos_;

    std::vector< std::vector< Index > > topology_;
    std::vector< std::vector< Index > > topologyPos_;

    std::vector< Index > faceNeighbors_;

    std::vector< std::vector< ctype > > volumes_; // mostly codim 0 and 1
    std::vector< GeometryType >         geomTypes_;
  };

}// end namespace Dune

#endif
