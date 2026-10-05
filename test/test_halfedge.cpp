#include <catch2/catch_test_macros.hpp>

#include "halfedge/face.h"
#include "halfedge/halfedge.h"
#include "halfedge/polyhedron.h"
#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <memory>

/* A crash in the 3D view (seen after deleting a voxel). Two cubes that touch
 * only along an edge put four halfedges on that edge; every other edge has
 * the usual two. finalize() looked the halfedges of an edge up with
 * multimap::find() and counted forward from what it got, but find() may
 * return any element with the key and newer libc++ does, so an ordinary edge
 * could be counted as having one halfedge, be left without a twin and trip
 * the assertion in closeSurface(). Fix taken from upstream burr-tools
 * (Derek Bosch).
 */
TEST_CASE("polyhedron: finalize gives every halfedge of a voxel mesh a twin", "[halfedge]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v(gt.getVoxel(6, 6, 6, voxel_c::VX_EMPTY));
  v->setState(2, 2, 0, voxel_c::VX_FILLED);
  v->setState(3, 3, 0, voxel_c::VX_FILLED);

  std::unique_ptr<Polyhedron> meshes[] = {
    std::unique_ptr<Polyhedron>(v->getFlatMesh()),
    std::unique_ptr<Polyhedron>(v->getWireframeMesh()),
    std::unique_ptr<Polyhedron>(v->getSTLMesh()),
  };
  const char * names[] = { "flat", "wireframe", "STL" };

  for (int i = 0; i < 3; i++) {
    INFO(names[i]);
    REQUIRE(meshes[i]);
    for (Polyhedron::const_edge_iterator it = meshes[i]->eBegin(); it != meshes[i]->eEnd(); ++it) {
      const HalfEdge * he = *it;
      REQUIRE(he->twin() != nullptr);
      REQUIRE(he->twin() != he);
      REQUIRE(he->twin()->twin() == he);
    }
  }

  /* the flat mesh is closed: closeSurface() had nothing to add */
  for (Polyhedron::const_face_iterator it = meshes[0]->fBegin(); it != meshes[0]->fEnd(); ++it)
    REQUIRE_FALSE((*it)->hole());
}
