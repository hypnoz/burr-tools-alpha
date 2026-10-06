#!/bin/sh
# Remakes the example pictures of the grid selector from example puzzles,
# with the GUI's --render-example. Run from the top of the source tree after
# `just build`.
set -e
B=./build/burrtools
D=src/gui/images/examples
$B --render-example examples/CubeInCage.xmpuzzle $D/brick.png
$B --render-example examples/Prisgon.xmpuzzle $D/prism.png
$B --render-example examples/BallRoom.xmpuzzle $D/spheres.png
$B --render-example examples/DiagonalCube.xmpuzzle $D/rhombic.png
$B --render-example examples/FourPieceTetrahedron.xmpuzzle $D/tetraocta.png 1
# sliding.png and stacking.png (Panex Level 8) are pictures supplied for
# them, not rendered here.
