/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "rotationrules.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <set>

namespace {

/* The debug switches, read once: this runs for every rotation tried, and
 * getenv scans the whole environment each time. */
bool rotDebug(void) {
  static const bool on = getenv("BT_ROT_DEBUG") != nullptr;
  return on;
}

/* BT_ROT_DUMP=x,y,z,axis,sense: dump the cells of that one rotation. */
struct rotDump_c {
  bool on = false;
  int px = 0, py = 0, pz = 0, a = 0, s = 0;
};

const rotDump_c & rotDump(void) {
  static const rotDump_c d = [] {
    rotDump_c r;
    const char * spec = getenv("BT_ROT_DUMP");
    r.on = spec && sscanf(spec, "%d,%d,%d,%d,%d", &r.px, &r.py, &r.pz, &r.a, &r.s) == 5;
    return r;
  }();
  return d;
}

}

namespace {

struct cellLess {
  bool operator()(const rotationRules_c::cell_t & a, const rotationRules_c::cell_t & b) const {
    if (a.x != b.x) return a.x < b.x;
    if (a.y != b.y) return a.y < b.y;
    return a.z < b.z;
  }
};

typedef std::set<rotationRules_c::cell_t, cellLess> cellSet;

/* A set of cells for the rules: a dense grid over their bounding box for
 * lookups, and the cells, each once, for scans. Built for every rotation
 * tried, so it must be cheap: a std::set allocated a node per cell. The
 * rules only ask yes/no questions, so the scan order does not matter. */
class occupancy_c {
public:
  explicit occupancy_c(const std::vector<rotationRules_c::cell_t> & in) {
    if (in.empty())
      return;
    int x1 = in[0].x, y1 = in[0].y, z1 = in[0].z;
    x0 = x1; y0 = y1; z0 = z1;
    for (const rotationRules_c::cell_t & c : in) {
      x0 = std::min(x0, c.x); x1 = std::max(x1, c.x);
      y0 = std::min(y0, c.y); y1 = std::max(y1, c.y);
      z0 = std::min(z0, c.z); z1 = std::max(z1, c.z);
    }
    w = x1 - x0 + 1;
    h = y1 - y0 + 1;
    d = z1 - z0 + 1;
    grid.assign((size_t)w * h * d, 0);
    cells.reserve(in.size());
    for (const rotationRules_c::cell_t & c : in) {
      unsigned char & g = grid[index(c.x, c.y, c.z)];
      if (!g) {
        g = 1;
        cells.push_back(c);
      }
    }
  }

  bool has(int x, int y, int z) const {
    if (x < x0 || y < y0 || z < z0 || x >= x0 + w || y >= y0 + h || z >= z0 + d)
      return false;
    return grid[index(x, y, z)] != 0;
  }
  bool has(const rotationRules_c::cell_t & c) const { return has(c.x, c.y, c.z); }
  size_t size(void) const { return cells.size(); }

  std::vector<rotationRules_c::cell_t> cells;

private:
  size_t index(int x, int y, int z) const {
    return ((size_t)(z - z0) * h + (size_t)(y - y0)) * w + (size_t)(x - x0);
  }

  int x0 = 0, y0 = 0, z0 = 0;
  int w = 0, h = 0, d = 0;
  std::vector<unsigned char> grid;
};

/* (u, v, a) as a cell, for a rotation about axis: u and v in the plane,
 * a along the axis (see inPlaneU, inPlaneV, axialCoord). */
static rotationRules_c::cell_t cellAt(unsigned int axis, int u, int v, int a) {
  if (axis == 0) return rotationRules_c::cell_t(a, u, v);
  if (axis == 1) return rotationRules_c::cell_t(u, a, v);
  return rotationRules_c::cell_t(u, v, a);
}

/* Mid-path samples along a 90° turn, matching the Fortran NrotSteps default. */
static const int ARC_STEPS = 24;
static const double BEVEL_REMOVE = 0.0405;
static const double HALFTHICK = 0.5 - BEVEL_REMOVE;

static double pivotWorld(int doubled) {
  return doubled * 0.5 + 0.5;
}

/* Voxel (i,j,k) occupies [i,i+1]×[j,j+1]×[k,k+1]. A unit cube centred at C
 * overlaps lattice voxels in each axis from floor(C-0.5) to floor(C+0.5-eps). */
static void addOverlappedCells(double cx, double cy, double cz, cellSet & out) {
  const double eps = 1e-9;
  int x0 = (int)floor(cx - 0.5 + eps);
  int x1 = (int)floor(cx + 0.5 - eps);
  int y0 = (int)floor(cy - 0.5 + eps);
  int y1 = (int)floor(cy + 0.5 - eps);
  int z0 = (int)floor(cz - 0.5 + eps);
  int z1 = (int)floor(cz + 0.5 - eps);

  for (int x = x0; x <= x1; x++)
    for (int y = y0; y <= y1; y++)
      for (int z = z0; z <= z1; z++)
        out.insert(rotationRules_c::cell_t(x, y, z));
}

/* Rotate in-plane coords (u,v) by angle θ toward a ±90° turn.
 * sense 0 = +90°, sense 1 = -90°. */
static void rotateInPlane(double u, double v, double theta, unsigned int sense,
                          double * uOut, double * vOut) {
  double c = cos(theta);
  double s = sin(theta);
  if (sense == 0) {
    *uOut = u * c - v * s;
    *vOut = u * s + v * c;
  } else {
    *uOut = u * c + v * s;
    *vOut = -u * s + v * c;
  }
}

/* Continuous centre of a voxel after rotating by theta about pivot centre. */
static void rotatedCenter(double dx, double dy, double dz,
                          unsigned int axis, unsigned int sense, double theta,
                          double * ox, double * oy, double * oz) {
  if (axis == 0) {
    double ny, nz;
    rotateInPlane(dy, dz, theta, sense, &ny, &nz);
    *ox = dx; *oy = ny; *oz = nz;
  } else if (axis == 1) {
    if (sense == 0) {
      *ox = dx * cos(theta) + dz * sin(theta);
      *oy = dy;
      *oz = -dx * sin(theta) + dz * cos(theta);
    } else {
      *ox = dx * cos(theta) - dz * sin(theta);
      *oy = dy;
      *oz = dx * sin(theta) + dz * cos(theta);
    }
  } else {
    double nx, ny;
    rotateInPlane(dx, dy, theta, sense, &nx, &ny);
    *ox = nx; *oy = ny; *oz = dz;
  }
}

/* The four normals of the separating-axis test at one angle: both squares'
 * edges (axis-aligned, and turned by the angle). Worked out once per step;
 * the same calls as before, so exactly the same values. */
struct satAxes_c {
  double c[4], s[4];
  explicit satAxes_c(double theta) {
    const double nrm[4] = { 0.0, 1.5707963267948966, theta, theta + 1.5707963267948966 };
    for (int r = 0; r < 4; r++) {
      c[r] = cos(nrm[r]);
      s[r] = sin(nrm[r]);
    }
  }
};

/**
 * 2D SAT: true if the moving beveled square overlaps the static one.
 * Normals are axis-aligned plus the current rotation angle (Fortran rr=1..4).
 */
static bool bevelSquaresOverlap(const double mc[4][2], const double sc[4][2],
                                const satAxes_c & axes) {
  for (int r = 0; r < 4; r++) {
    const double c = axes.c[r];
    const double s = axes.s[r];
    double min1 = 1e100, max1 = -1e100, min2 = 1e100, max2 = -1e100;
    for (int i = 0; i < 4; i++) {
      double d1 = mc[i][0] * c + mc[i][1] * s;
      double d2 = sc[i][0] * c + sc[i][1] * s;
      if (d1 < min1) min1 = d1;
      if (d1 > max1) max1 = d1;
      if (d2 < min2) min2 = d2;
      if (d2 > max2) max2 = d2;
    }
    if (max1 < min2 || max2 < min1)
      return false;
  }
  return true;
}

static void inPlaneFromWorld(double x, double y, double z, unsigned int axis,
                             double * u, double * v) {
  if (axis == 0) { *u = y; *v = z; }
  else if (axis == 1) { *u = x; *v = z; }
  else { *u = x; *v = y; }
}

static void bevelCornersWorld(const rotationRules_c::cell_t & cell, unsigned int axis,
                              double ht, double out[4][3]) {
  const double cx = cell.x + 0.5;
  const double cy = cell.y + 0.5;
  const double cz = cell.z + 0.5;
  const double su[4] = { -ht, -ht, ht, ht };
  const double sv[4] = { -ht, ht, -ht, ht };
  for (int i = 0; i < 4; i++) {
    if (axis == 0) {
      out[i][0] = cx; out[i][1] = cy + su[i]; out[i][2] = cz + sv[i];
    } else if (axis == 1) {
      out[i][0] = cx + su[i]; out[i][1] = cy; out[i][2] = cz + sv[i];
    } else {
      out[i][0] = cx + su[i]; out[i][1] = cy + sv[i]; out[i][2] = cz;
    }
  }
}

/* One static cell as the arc sweep sees it: its layer along the axis, and
 * its beveled square in the rotation plane, centre and corners. */
struct staticSquare_c {
  int layer;
  double cu, cv;
  double sc[4][2];
};

/* Every static cell's square, sorted by layer, for one rotation axis. */
static std::vector<staticSquare_c> staticSquares(const occupancy_c & occ, unsigned int axis) {
  std::vector<staticSquare_c> out(occ.size());
  for (size_t i = 0; i < occ.size(); i++) {
    const rotationRules_c::cell_t & o = occ.cells[i];
    staticSquare_c & q = out[i];
    q.layer = (axis == 0) ? o.x : ((axis == 1) ? o.y : o.z);
    double scorners[4][3];
    bevelCornersWorld(o, axis, HALFTHICK, scorners);
    for (int k = 0; k < 4; k++)
      inPlaneFromWorld(scorners[k][0], scorners[k][1], scorners[k][2], axis,
                       &q.sc[k][0], &q.sc[k][1]);
    inPlaneFromWorld(o.x + 0.5, o.y + 0.5, o.z + 0.5, axis, &q.cu, &q.cv);
  }
  std::sort(out.begin(), out.end(),
            [](const staticSquare_c & a, const staticSquare_c & b) { return a.layer < b.layer; });
  return out;
}

/* What the arc sweep needs of each angle it samples, worked out once: the
 * same calls on the same angles as before, so exactly the same values. */
struct arcStep_c {
  double c, s, sin2;
  satAxes_c axes[2];  // by sense
  explicit arcStep_c(double theta)
      : c(cos(theta)), s(sin(theta)), sin2(sin(2.0 * theta)),
        axes{satAxes_c(theta), satAxes_c(-theta)} {}
};

static const std::vector<arcStep_c> & arcSteps(void) {
  static const std::vector<arcStep_c> steps = [] {
    const double halfPi = 1.5707963267948966;
    std::vector<arcStep_c> v;
    /* Entry 0 is not used: the sweep samples steps 1 .. ARC_STEPS - 1. */
    for (int s = 0; s < ARC_STEPS; s++)
      v.push_back(arcStep_c(halfPi * (double)s / (double)ARC_STEPS));
    return v;
  }();
  return steps;
}

/* rotatedCenter with the angle's cosine and sine at hand. */
static void rotatedCenterCS(double dx, double dy, double dz,
                            unsigned int axis, unsigned int sense, double c, double s,
                            double * ox, double * oy, double * oz) {
  if (axis == 0) {
    *ox = dx;
    if (sense == 0) { *oy = dy * c - dz * s; *oz = dy * s + dz * c; }
    else            { *oy = dy * c + dz * s; *oz = -dy * s + dz * c; }
  } else if (axis == 1) {
    *oy = dy;
    if (sense == 0) { *ox = dx * c + dz * s; *oz = -dx * s + dz * c; }
    else            { *ox = dx * c - dz * s; *oz = dx * s + dz * c; }
  } else {
    *oz = dz;
    if (sense == 0) { *ox = dx * c - dy * s; *oy = dx * s + dy * c; }
    else            { *ox = dx * c + dy * s; *oy = -dx * s + dy * c; }
  }
}

/* The largest mid-turn wiggle: the third amount along a diagonal. */
static const double WIGGLE_MAX = 0.378680 * 1.4142135623730951 + 0.001;
/* Squares further apart than this, centre to centre, cannot touch: each
 * reaches HALFTHICK * sqrt(2) (about 0.65) from its centre. With room to
 * spare, so skipping them never changes an answer. */
static const double NEAR = 1.36;

/* One moving cell in the arc sweep: where its beveled square's corners and
 * centre sit from the pivot, and the static squares it can come near at all. */
struct arcMover_c {
  double corner[4][3];
  double centre[3];
  size_t firstNear, endNear;  // into arcSweep_c::nearSquares
};

/* The moving cells of one sweep, laid out once for the plain turn and every
 * wiggled one. Kept per thread so a sweep allocates nothing. */
struct arcSweep_c {
  std::vector<arcMover_c> movers;
  std::vector<const staticSquare_c *> nearSquares;
  /* Which of an angle's two sets of normals fits this turn (see arcClear). */
  int axesSet = 0;
  /* Where the last turn tried was stopped: the angle, the moving cell and
   * the static square. The next wiggle most often stops there too, so it
   * looks there first. */
  int hitStep = 0;
  size_t hitMover = 0;
  const staticSquare_c * hitSquare = nullptr;
};

/**
 * Sample the 90° path with Fortran beveled squares. Optional in-plane wiggle
 * (dshift * sin(2θ) along one of 8 directions) matches SimpleRot.
 */
static bool bevelArcClearOne(arcSweep_c & sweep,
                             const rotationRules_c::pivot_t & pivot,
                             unsigned int axis, unsigned int sense,
                             double dshift, int shifti, int shiftj) {

  const double pcx = pivotWorld(pivot.hx);
  const double pcy = pivotWorld(pivot.hy);
  const double pcz = pivotWorld(pivot.hz);
  const std::vector<arcStep_c> & steps = arcSteps();

  /* The moving cell's corners at one angle of this turn. */
  auto cornersAt = [&](const arcMover_c & m, const arcStep_c & step, double mc[4][2]) {
    const double w = dshift * step.sin2;
    const double wu = w * (double)shifti;
    const double wv = w * (double)shiftj;
    double wx = 0, wy = 0, wz = 0;
    if (axis == 0) { wy = wu; wz = wv; }
    else if (axis == 1) { wx = wu; wz = wv; }
    else { wx = wu; wy = wv; }
    for (int k = 0; k < 4; k++) {
      double ox, oy, oz;
      rotatedCenterCS(m.corner[k][0], m.corner[k][1], m.corner[k][2], axis, sense,
                      step.c, step.s, &ox, &oy, &oz);
      inPlaneFromWorld(pcx + ox + wx, pcy + oy + wy, pcz + oz + wz, axis,
                       &mc[k][0], &mc[k][1]);
    }
  };

  /* Any overlap at any angle stops the turn, so the order they are looked
   * for in does not matter: start where the last turn was stopped. */
  if (sweep.hitSquare) {
    const arcStep_c & step = steps[(size_t)sweep.hitStep];
    double mc[4][2];
    cornersAt(sweep.movers[sweep.hitMover], step, mc);
    if (bevelSquaresOverlap(mc, sweep.hitSquare->sc, step.axes[sweep.axesSet]))
      return false;
  }

  for (int s = 1; s < ARC_STEPS; s++) {
    const arcStep_c & step = steps[(size_t)s];
    const satAxes_c & axes = step.axes[sweep.axesSet];
    const double w = dshift * step.sin2;
    const double wu = w * (double)shifti;
    const double wv = w * (double)shiftj;

    for (size_t mi = 0; mi < sweep.movers.size(); mi++) {
      const arcMover_c & m = sweep.movers[mi];
      double ox, oy, oz, mu, mv;
      rotatedCenterCS(m.centre[0], m.centre[1], m.centre[2], axis, sense, step.c, step.s,
                      &ox, &oy, &oz);
      inPlaneFromWorld(pcx + ox, pcy + oy, pcz + oz, axis, &mu, &mv);
      mu += wu;
      mv += wv;

      /* The corners at this angle, worked out for the first square near. */
      double mc[4][2];
      bool haveCorners = false;

      for (size_t qi = m.firstNear; qi < m.endNear; qi++) {
        const staticSquare_c * q = sweep.nearSquares[qi];
        const double du = q->cu - mu;
        const double dv = q->cv - mv;
        if (du * du + dv * dv > NEAR * NEAR)
          continue;
        if (!haveCorners) {
          cornersAt(m, step, mc);
          haveCorners = true;
        }
        if (bevelSquaresOverlap(mc, q->sc, axes)) {
          sweep.hitStep = s;
          sweep.hitMover = mi;
          sweep.hitSquare = q;
          return false;
        }
      }
    }
  }

  return true;
}

static bool arcClear(const std::vector<staticSquare_c> & squares,
                     const std::vector<rotationRules_c::cell_t> & startCells,
                     const rotationRules_c::pivot_t & pivot,
                     unsigned int axis,
                     unsigned int sense) {

  static thread_local arcSweep_c sweep;
  sweep.movers.clear();
  sweep.nearSquares.clear();
  sweep.hitSquare = nullptr;
  /* The overlap test wants the moving square's edge normals: the angle
   * turned so far, signed by the sense. Seen in the plane's (u, v), a turn
   * about Y runs the other way round from one about X or Z (its plane is
   * (x, z), and +90° takes z to x), so its sign is the other one. With the
   * same sign for all three, as this once had, Y's normals are the mirror
   * image of the square's edges and turns about Y are refused that the same
   * pieces, lying the other way, may make about X or Z. */
  const bool plus = (sense == 0) != (axis == 1);
  sweep.axesSet = plus ? 0 : 1;

  const double pcx = pivotWorld(pivot.hx);
  const double pcy = pivotWorld(pivot.hy);
  const double pcz = pivotWorld(pivot.hz);
  double pu, pv;
  inPlaneFromWorld(pcx, pcy, pcz, axis, &pu, &pv);

  for (const rotationRules_c::cell_t & cell : startCells) {
    const int ck = (axis == 0) ? cell.x : ((axis == 1) ? cell.y : cell.z);
    auto lo = std::lower_bound(squares.begin(), squares.end(), ck,
                               [](const staticSquare_c & q, int l) { return q.layer < l; });
    if (lo == squares.end() || lo->layer != ck)
      continue;

    /* The cell's centre turns on a circle about the pivot, wiggle aside: a
     * static square can only come near when it lies about as far out. */
    double cu, cv;
    inPlaneFromWorld(cell.x + 0.5, cell.y + 0.5, cell.z + 0.5, axis, &cu, &cv);
    const double radius = sqrt((cu - pu) * (cu - pu) + (cv - pv) * (cv - pv));
    const double reach = NEAR + WIGGLE_MAX;
    const double outerSq = (radius + reach) * (radius + reach);
    const double innerSq = radius > reach ? (radius - reach) * (radius - reach) : 0.0;

    arcMover_c m;
    m.firstNear = sweep.nearSquares.size();
    for (auto q = lo; q != squares.end() && q->layer == ck; ++q) {
      const double outSq = (q->cu - pu) * (q->cu - pu) + (q->cv - pv) * (q->cv - pv);
      if (outSq <= outerSq && outSq >= innerSq)
        sweep.nearSquares.push_back(&*q);
    }
    m.endNear = sweep.nearSquares.size();
    if (m.endNear == m.firstNear)
      continue;

    double corners[4][3];
    bevelCornersWorld(cell, axis, HALFTHICK, corners);
    for (int k = 0; k < 4; k++) {
      m.corner[k][0] = corners[k][0] - pcx;
      m.corner[k][1] = corners[k][1] - pcy;
      m.corner[k][2] = corners[k][2] - pcz;
    }
    m.centre[0] = cell.x + 0.5 - pcx;
    m.centre[1] = cell.y + 0.5 - pcy;
    m.centre[2] = cell.z + 0.5 - pcz;
    sweep.movers.push_back(m);
  }

  if (bevelArcClearOne(sweep, pivot, axis, sense, 0.0, 0, 0))
    return true;

  const double amounts[3] = {
    sqrt(2.0 * HALFTHICK * HALFTHICK) - 0.5,
    (sqrt(2.0) - 1.0) * 0.5 + 0.025,
    0.378680
  };
  static const int dirs[8][2] = {
    { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
    { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 }
  };

  for (int a = 0; a < 3; a++)
    for (int d = 0; d < 8; d++)
      if (bevelArcClearOne(sweep, pivot, axis, sense,
                           amounts[a], dirs[d][0], dirs[d][1]))
        return true;

  return false;
}

static bool occupiedAt(const occupancy_c & occ, int x, int y, int z) {
  return occ.has(x, y, z);
}

static int axialCoord(const rotationRules_c::cell_t & c, unsigned int axis) {
  if (axis == 0) return c.x;
  if (axis == 1) return c.y;
  return c.z;
}

/* In-plane U,V for a rotation axis: X→(Y,Z), Y→(X,Z), Z→(X,Y). */
static void inPlaneUV(unsigned int axis,
                      int * ux, int * uy, int * uz,
                      int * vx, int * vy, int * vz) {
  if (axis == 0) {
    *ux = 0; *uy = 1; *uz = 0;
    *vx = 0; *vy = 0; *vz = 1;
  } else if (axis == 1) {
    *ux = 1; *uy = 0; *uz = 0;
    *vx = 0; *vy = 0; *vz = 1;
  } else {
    *ux = 1; *uy = 0; *uz = 0;
    *vx = 0; *vy = 1; *vz = 0;
  }
}

static int inPlaneU(const rotationRules_c::cell_t & c, unsigned int axis) {
  if (axis == 0) return c.y;
  return c.x;
}

static int inPlaneV(const rotationRules_c::cell_t & c, unsigned int axis) {
  if (axis == 2) return c.y;
  return c.z;
}

/* Moving voxel lies on the pivot column (same in-plane coords as pivot). */
static bool onPivotColumn(const rotationRules_c::cell_t & c,
                          const rotationRules_c::pivot_t & pivot,
                          unsigned int axis) {
  if (axis == 0)
    return c.y * 2 == pivot.hy && c.z * 2 == pivot.hz;
  if (axis == 1)
    return c.x * 2 == pivot.hx && c.z * 2 == pivot.hz;
  return c.x * 2 == pivot.hx && c.y * 2 == pivot.hy;
}

/* True if any moving voxel on layer `a` is face-adjacent to an other-piece voxel. */
static bool layerTouchesOther(const occupancy_c & occ,
                              const std::vector<rotationRules_c::cell_t> & startCells,
                              unsigned int axis,
                              int a) {
  static const int d[6][3] = {
    { -1, 0, 0 }, { 1, 0, 0 },
    { 0, -1, 0 }, { 0, 1, 0 },
    { 0, 0, -1 }, { 0, 0, 1 }
  };

  for (unsigned int i = 0; i < startCells.size(); i++) {
    const rotationRules_c::cell_t & c = startCells[i];
    if (axialCoord(c, axis) != a)
      continue;
    for (int k = 0; k < 6; k++) {
      if (occupiedAt(occ, c.x + d[k][0], c.y + d[k][1], c.z + d[k][2]))
        return true;
    }
  }
  return false;
}

/* Record ±U/±V face neighbours of one moving cell for axis-cross (in-plane only). */
static void axisCrossScanCell(const rotationRules_c::cell_t & c,
                              unsigned int axis,
                              const occupancy_c & occ,
                              const occupancy_c & start,
                              bool * negU, bool * posU, bool * negV, bool * posV,
                              cellSet * slotOccupied,
                              cellSet * restricted) {

  rotationRules_c::cell_t nu, pu, nv, pv;

  if (axis == 0) {
    nu = rotationRules_c::cell_t(c.x, c.y - 1, c.z);
    pu = rotationRules_c::cell_t(c.x, c.y + 1, c.z);
    nv = rotationRules_c::cell_t(c.x, c.y, c.z - 1);
    pv = rotationRules_c::cell_t(c.x, c.y, c.z + 1);
  } else if (axis == 1) {
    nu = rotationRules_c::cell_t(c.x - 1, c.y, c.z);
    pu = rotationRules_c::cell_t(c.x + 1, c.y, c.z);
    nv = rotationRules_c::cell_t(c.x, c.y, c.z - 1);
    pv = rotationRules_c::cell_t(c.x, c.y, c.z + 1);
  } else {
    nu = rotationRules_c::cell_t(c.x - 1, c.y, c.z);
    pu = rotationRules_c::cell_t(c.x + 1, c.y, c.z);
    nv = rotationRules_c::cell_t(c.x, c.y - 1, c.z);
    pv = rotationRules_c::cell_t(c.x, c.y + 1, c.z);
  }

  const rotationRules_c::cell_t slots[4] = { nu, pu, nv, pv };
  bool * flags[4] = { negU, posU, negV, posV };

  for (int i = 0; i < 4; i++) {
    if (start.has(slots[i]))
      continue;
    if (occ.has(slots[i])) {
      *flags[i] = true;
      if (slotOccupied)
        slotOccupied->insert(slots[i]);
    } else if (restricted) {
      restricted->insert(slots[i]);
    }
  }
}

/**
 * A moving voxel must not have static face-neighbours on both opposite
 * in-plane sides on the same axial layer (Fortran SimpleRot in-plane test).
 * Own-piece voxels are not in `occ`, so they do not count as a pinch.
 */
static bool inPlanePinchClear(const occupancy_c & occ,
                              const std::vector<rotationRules_c::cell_t> & cells,
                              unsigned int axis,
                              cellSet * blocking) {

  int ux, uy, uz, vx, vy, vz;
  inPlaneUV(axis, &ux, &uy, &uz, &vx, &vy, &vz);

  bool pinched = false;

  for (unsigned int i = 0; i < cells.size(); i++) {
    const rotationRules_c::cell_t & c = cells[i];
    rotationRules_c::cell_t nu(c.x - ux, c.y - uy, c.z - uz);
    rotationRules_c::cell_t pu(c.x + ux, c.y + uy, c.z + uz);
    rotationRules_c::cell_t nv(c.x - vx, c.y - vy, c.z - vz);
    rotationRules_c::cell_t pv(c.x + vx, c.y + vy, c.z + vz);

    bool negU = occ.has(nu);
    bool posU = occ.has(pu);
    bool negV = occ.has(nv);
    bool posV = occ.has(pv);

    if (negU && posU) {
      pinched = true;
      if (blocking) {
        blocking->insert(nu);
        blocking->insert(pu);
      } else {
        return false;
      }
    }
    if (negV && posV) {
      pinched = true;
      if (blocking) {
        blocking->insert(nv);
        blocking->insert(pv);
      } else {
        return false;
      }
    }
  }

  return !pinched;
}

/**
 * Perpendicular-plane capture (Fortran PerPplane 1 and 2).
 * On a slice of constant U (resp. V) that contains at least two moving and
 * two static voxels, opposite static walls in V (resp. U) at the same axial
 * coordinate reject the axis.
 */
static bool perpPlaneClear(const occupancy_c & occ,
                           const std::vector<rotationRules_c::cell_t> & startCells,
                           unsigned int axis,
                           cellSet * blocking) {

  if (startCells.size() < 2 || occ.size() < 2)
    return true;

  int uMin = inPlaneU(startCells[0], axis);
  int uMax = uMin;
  int vMin = inPlaneV(startCells[0], axis);
  int vMax = vMin;
  for (unsigned int i = 1; i < startCells.size(); i++) {
    int u = inPlaneU(startCells[i], axis);
    int v = inPlaneV(startCells[i], axis);
    if (u < uMin) uMin = u;
    if (u > uMax) uMax = u;
    if (v < vMin) vMin = v;
    if (v > vMax) vMax = v;
  }

  /* Moving and static voxels on each slice, counted in one pass each. */
  std::vector<unsigned int> moveU(uMax - uMin + 1, 0), statU(uMax - uMin + 1, 0);
  std::vector<unsigned int> moveV(vMax - vMin + 1, 0), statV(vMax - vMin + 1, 0);
  for (const rotationRules_c::cell_t & c : startCells) {
    moveU[inPlaneU(c, axis) - uMin]++;
    moveV[inPlaneV(c, axis) - vMin]++;
  }
  for (const rotationRules_c::cell_t & o : occ.cells) {
    const int u = inPlaneU(o, axis);
    const int v = inPlaneV(o, axis);
    if (u >= uMin && u <= uMax) statU[u - uMin]++;
    if (v >= vMin && v <= vMax) statV[v - vMin]++;
  }

  bool captured = false;

  /* PerPplane 1: slices of constant U, opposite walls in V; PerPplane 2:
   * slices of constant V, opposite walls in U. A wall is a static voxel on
   * the slice and layer of a moving one, next to it across the slice. */
  for (int plane = 0; plane < 2; plane++) {
    const bool alongU = plane == 0;
    const int lo = alongU ? uMin : vMin;
    const int hi = alongU ? uMax : vMax;
    for (int k = lo; k <= hi; k++) {
      const unsigned int nMove = alongU ? moveU[k - uMin] : moveV[k - vMin];
      const unsigned int nStat = alongU ? statU[k - uMin] : statV[k - vMin];
      if (nMove < 2 || nStat < 2)
        continue;

      bool neg = false, pos = false;
      cellSet hits;
      for (const rotationRules_c::cell_t & c : startCells) {
        if ((alongU ? inPlaneU(c, axis) : inPlaneV(c, axis)) != k)
          continue;
        const int ck = axialCoord(c, axis);
        const int across = alongU ? inPlaneV(c, axis) : inPlaneU(c, axis);
        for (int side = -1; side <= 1; side += 2) {
          const rotationRules_c::cell_t wall = alongU ? cellAt(axis, k, across + side, ck)
                                                      : cellAt(axis, across + side, k, ck);
          if (!occ.has(wall))
            continue;
          (side < 0 ? neg : pos) = true;
          if (blocking) hits.insert(wall);
        }
      }
      if (neg && pos) {
        captured = true;
        if (blocking) {
          for (cellSet::const_iterator it = hits.begin(); it != hits.end(); ++it)
            blocking->insert(*it);
        } else {
          return false;
        }
      }
    }
  }

  return !captured;
}

/**
 * Static ±U/±V neighbours face-adjacent to pivot-column moving voxels,
 * aggregated across layers along the rotation axis that face-touch the
 * other piece. Same-layer opposites are handled by inPlanePinchClear;
 * opposition across two different touching layers is rejected here.
 */
static bool axisCrossClear(const occupancy_c & occ,
                           const std::vector<rotationRules_c::cell_t> & startCells,
                           const occupancy_c & start,
                           const rotationRules_c::pivot_t & pivot,
                           unsigned int axis) {

  if (startCells.empty())
    return true;

  int aMin = axialCoord(startCells[0], axis);
  int aMax = aMin;
  for (unsigned int i = 1; i < startCells.size(); i++) {
    int a = axialCoord(startCells[i], axis);
    if (a < aMin) aMin = a;
    if (a > aMax) aMax = a;
  }

  bool seenNegU = false, seenPosU = false;
  bool seenNegV = false, seenPosV = false;

  for (int a = aMin; a <= aMax; a++) {
    if (!layerTouchesOther(occ, startCells, axis, a))
      continue;

    bool layerNegU = false, layerPosU = false;
    bool layerNegV = false, layerPosV = false;

    for (unsigned int i = 0; i < startCells.size(); i++) {
      const rotationRules_c::cell_t & c = startCells[i];
      if (axialCoord(c, axis) != a)
        continue;
      if (!onPivotColumn(c, pivot, axis))
        continue;
      axisCrossScanCell(c, axis, occ, start,
                        &layerNegU, &layerPosU, &layerNegV, &layerPosV, 0, 0);
    }

    if ((layerPosU && seenNegU) || (layerNegU && seenPosU) ||
        (layerPosV && seenNegV) || (layerNegV && seenPosV)) {
      if (rotDebug()) {
        const char * uName = (axis == 0) ? "Y" : "X";
        const char * vName = (axis == 0) ? "Z" : ((axis == 1) ? "Z" : "Y");
        fprintf(stderr,
                "ROT_AXIS_CROSS detail pivot=(%.1f,%.1f,%.1f) axis=%u "
                "touching-layer=%d cross-layer sides: -%s=%d +%s=%d -%s=%d +%s=%d "
                "(layer -%s=%d +%s=%d -%s=%d +%s=%d)\n",
                pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, a,
                uName, (int)seenNegU, uName, (int)seenPosU,
                vName, (int)seenNegV, vName, (int)seenPosV,
                uName, (int)layerNegU, uName, (int)layerPosU,
                vName, (int)layerNegV, vName, (int)layerPosV);
      }
      return false;
    }

    seenNegU |= layerNegU;
    seenPosU |= layerPosU;
    seenNegV |= layerNegV;
    seenPosV |= layerPosV;
  }

  return true;
}

}

struct rotationRules_c::occupancyCache_c {
  std::vector<cell_t> input;
  occupancy_c occ{std::vector<cell_t>()};
};

rotationRules_c::rotationRules_c(void) {}
rotationRules_c::~rotationRules_c(void) {}

namespace {

/* The occupancy of cells, from the cache when they are the ones it holds. */
const occupancy_c & cachedOccupancy(std::unique_ptr<rotationRules_c::occupancyCache_c> & cache,
                                    const std::vector<rotationRules_c::cell_t> & cells) {
  auto same = [](const rotationRules_c::cell_t & a, const rotationRules_c::cell_t & b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  };
  if (!cache)
    cache = std::make_unique<rotationRules_c::occupancyCache_c>();
  if (cache->input.size() != cells.size() ||
      !std::equal(cells.begin(), cells.end(), cache->input.begin(), same)) {
    cache->input = cells;
    cache->occ = occupancy_c(cells);
  }
  return cache->occ;
}

}

bool rotationRules_c::allowRotation(const std::vector<cell_t> & occupied,
                                    const std::vector<cell_t> & startCells,
                                    const std::vector<cell_t> & endCells,
                                    const pivot_t & pivot,
                                    unsigned int axis,
                                    unsigned int sense) const {

  const occupancy_c & occ = cachedOccupancy(occCache, occupied);

  {
    const rotDump_c & d = rotDump();
    if (d.on && pivot.hx == d.px * 2 && pivot.hy == d.py * 2 && pivot.hz == d.pz * 2 &&
        axis == (unsigned)d.a && sense == (unsigned)d.s) {
      static bool dumped = false;
      if (!dumped) {
        dumped = true;
        fprintf(stderr, "ROT_DUMP moving cells (%zu):\n", startCells.size());
        for (unsigned int i = 0; i < startCells.size(); i++)
          fprintf(stderr, "  M %d %d %d\n", startCells[i].x, startCells[i].y, startCells[i].z);
        fprintf(stderr, "ROT_DUMP static/other cells (%zu):\n", occupied.size());
        for (unsigned int i = 0; i < occupied.size(); i++)
          fprintf(stderr, "  S %d %d %d\n", occupied[i].x, occupied[i].y, occupied[i].z);
      }
    }
  }

  /* End-position: final voxels must not overlap other pieces */
  for (unsigned int i = 0; i < endCells.size(); i++)
    if (occ.has(endCells[i])) {
      if (rotDebug())
        fprintf(stderr,
                "ROT_REJECT end-overlap pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u end=(%d,%d,%d)\n",
                pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense,
                endCells[i].x, endCells[i].y, endCells[i].z);
      return false;
    }

  /* Same-layer sandwich at start (pinched voxel cannot turn in-plane) */
  if (!inPlanePinchClear(occ, startCells, axis, 0)) {
    if (rotDebug())
      fprintf(stderr,
              "ROT_REJECT sandwich-start pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u "
              "(moving voxel has static on both opposite in-plane sides)\n",
              pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);
    return false;
  }

  /* Same-layer sandwich at the 90° pose */
  if (!inPlanePinchClear(occ, endCells, axis, 0)) {
    if (rotDebug())
      fprintf(stderr,
              "ROT_REJECT sandwich-end pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u "
              "(rotated voxel would have static on both opposite in-plane sides)\n",
              pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);
    return false;
  }

  /* Opposite static walls in planes perpendicular to the rotation plane */
  if (!perpPlaneClear(occ, startCells, axis, 0)) {
    if (rotDebug())
      fprintf(stderr,
              "ROT_REJECT perp-plane pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u "
              "(opposite static walls on a U or V slice with 2+ moving and 2+ static voxels)\n",
              pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);
    return false;
  }

  /* Opposite static neighbours forbidden across layers that touch the other piece */
  if (!axisCrossClear(occ, startCells, occupancy_c(startCells), pivot, axis)) {
    if (rotDebug())
      fprintf(stderr,
              "ROT_REJECT axis-cross pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u "
              "(opposite ±in-plane static neighbours face-adjacent to moving piece on layers touching other piece)\n",
              pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);
    return false;
  }

  /* Continuous path must not clip other pieces mid-turn */
  if (!arcClear(staticSquares(occ, axis), startCells, pivot, axis, sense)) {
    if (rotDebug())
      fprintf(stderr,
              "ROT_REJECT arc-sweep pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u "
              "(mid-path beveled square overlaps other piece)\n",
              pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);
    return false;
  }

  if (rotDebug())
    fprintf(stderr,
            "ROT_ALLOW pivot=(%.1f,%.1f,%.1f) axis=%u sense=%u\n",
            pivot.hx * 0.5, pivot.hy * 0.5, pivot.hz * 0.5, axis, sense);

  return true;
}

bool rotationRules_c::axisBlocked(const std::vector<cell_t> & occupied,
                                  const std::vector<cell_t> & startCells,
                                  unsigned int axis) const {

  const occupancy_c & occ = cachedOccupancy(occCache, occupied);

  if (!inPlanePinchClear(occ, startCells, axis, 0))
    return true;
  if (!perpPlaneClear(occ, startCells, axis, 0))
    return true;
  return false;
}

/* What setBodies keeps for the rotations tried next. */
struct rotationRules_c::prepared_c {
  std::vector<cell_t> occupied;
  std::vector<cell_t> start;
  occupancy_c occ{std::vector<cell_t>()};
  occupancy_c startOcc{std::vector<cell_t>()};
  /* The others' squares for the arc sweep, per axis, made when first asked for. */
  std::vector<staticSquare_c> squares[3];
  bool haveSquares[3] = { false, false, false };
  std::vector<cell_t> end;
};

bool rotationRules_c::rotateCell(const cell_t & cell, const pivot_t & pivot,
                                 unsigned int axis, unsigned int sense, cell_t & out) {
  const int dx = cell.x * 2 - pivot.hx;
  const int dy = cell.y * 2 - pivot.hy;
  const int dz = cell.z * 2 - pivot.hz;
  int rx = dx, ry = dy, rz = dz;
  if (axis == 0) {
    if (sense == 0) { ry = -dz; rz = dy; }
    else            { ry = dz;  rz = -dy; }
  } else if (axis == 1) {
    if (sense == 0) { rx = dz;  rz = -dx; }
    else            { rx = -dz; rz = dx; }
  } else {
    if (sense == 0) { rx = -dy; ry = dx; }
    else            { rx = dy;  ry = -dx; }
  }
  const int nx = pivot.hx + rx;
  const int ny = pivot.hy + ry;
  const int nz = pivot.hz + rz;
  if ((nx | ny | nz) & 1)
    return false;
  out = cell_t(nx / 2, ny / 2, nz / 2);
  return true;
}

void rotationRules_c::setBodies(const std::vector<cell_t> & occupied,
                                const std::vector<cell_t> & startCells) {
  if (!prep)
    prep = std::make_unique<prepared_c>();
  prep->occupied = occupied;
  prep->start = startCells;
  prep->occ = occupancy_c(occupied);
  prep->startOcc = occupancy_c(startCells);
  for (bool & have : prep->haveSquares)
    have = false;
}

bool rotationRules_c::preparedAxisBlocked(unsigned int axis) const {
  return !inPlanePinchClear(prep->occ, prep->start, axis, 0) ||
         !perpPlaneClear(prep->occ, prep->start, axis, 0);
}

bool rotationRules_c::allowPrepared(const pivot_t & pivot, unsigned int axis,
                                    unsigned int sense, bool axisClear) const {
  prepared_c & p = *prep;
  /* The debug output is allowRotation's: let it speak. */
  const bool talk = rotDebug() || rotDump().on;

  /* The end cells, stopping at the first that lands on another piece. */
  p.end.clear();
  for (const cell_t & c : p.start) {
    cell_t e;
    if (!rotateCell(c, pivot, axis, sense, e))
      return false;
    if (!talk && p.occ.has(e))
      return false;
    p.end.push_back(e);
  }

  if (talk)
    return allowRotation(p.occupied, p.start, p.end, pivot, axis, sense);

  if (!axisClear && preparedAxisBlocked(axis))
    return false;
  if (!inPlanePinchClear(p.occ, p.end, axis, 0))
    return false;
  if (!axisCrossClear(p.occ, p.start, p.startOcc, pivot, axis))
    return false;
  if (!p.haveSquares[axis]) {
    p.squares[axis] = staticSquares(p.occ, axis);
    p.haveSquares[axis] = true;
  }
  return arcClear(p.squares[axis], p.start, pivot, axis, sense);
}

void rotationRules_c::collectDebugConflictCells(
    const std::vector<cell_t> & occupied,
    const std::vector<cell_t> & startCells,
    const pivot_t & pivot,
    unsigned int axis,
    unsigned int sense,
    std::vector<cell_t> & outBlocking,
    std::vector<cell_t> & outClearance,
    std::vector<cell_t> & outRestricted) const {

  outBlocking.clear();
  outClearance.clear();
  outRestricted.clear();

  const occupancy_c occ(occupied);

  const occupancy_c start(startCells);

  cellSet blocking;
  cellSet clearance;
  cellSet restricted;

  /* Arc-sweep samples: every overlapped lattice cell that is not a start
   * voxel is a clearance cell; those also in occ are hard blockers. */
  {
    const double pcx = pivotWorld(pivot.hx);
    const double pcy = pivotWorld(pivot.hy);
    const double pcz = pivotWorld(pivot.hz);
    const double halfPi = 1.5707963267948966;

    for (unsigned int ci = 0; ci < startCells.size(); ci++) {
      const cell_t & cell = startCells[ci];
      double dx = (cell.x + 0.5) - pcx;
      double dy = (cell.y + 0.5) - pcy;
      double dz = (cell.z + 0.5) - pcz;

      if (axis == 0 && dy == 0 && dz == 0) continue;
      if (axis == 1 && dx == 0 && dz == 0) continue;
      if (axis == 2 && dx == 0 && dy == 0) continue;

      for (int s = 1; s < ARC_STEPS; s++) {
        double theta = halfPi * (double)s / (double)ARC_STEPS;
        double ox, oy, oz;
        rotatedCenter(dx, dy, dz, axis, sense, theta, &ox, &oy, &oz);

        cellSet hit;
        addOverlappedCells(pcx + ox, pcy + oy, pcz + oz, hit);
        for (cellSet::const_iterator it = hit.begin(); it != hit.end(); ++it) {
          if (start.has(*it))
            continue;
          clearance.insert(*it);
          if (occ.has(*it))
            blocking.insert(*it);
        }
      }
    }
  }

  /* Same-layer sandwich and perpendicular-plane capture: static cells that
   * currently pinch or wall-in the moving piece. */
  inPlanePinchClear(occ, startCells, axis, &blocking);
  perpPlaneClear(occ, startCells, axis, &blocking);

  /* Axis-cross: ±in-plane slots face-adjacent to pivot-column moving voxels on
   * touching layers. Cross-layer opposition marks hard blockers. */
  if (!startCells.empty()) {
    int aMin = axialCoord(startCells[0], axis);
    int aMax = aMin;
    for (unsigned int i = 1; i < startCells.size(); i++) {
      int a = axialCoord(startCells[i], axis);
      if (a < aMin) aMin = a;
      if (a > aMax) aMax = a;
    }

    bool seenNegU = false, seenPosU = false;
    bool seenNegV = false, seenPosV = false;
    cellSet slotOccupied;

    for (int a = aMin; a <= aMax; a++) {
      if (!layerTouchesOther(occ, startCells, axis, a))
        continue;

      bool layerNegU = false, layerPosU = false;
      bool layerNegV = false, layerPosV = false;

      for (unsigned int i = 0; i < startCells.size(); i++) {
        const cell_t & c = startCells[i];
        if (axialCoord(c, axis) != a)
          continue;
        if (!onPivotColumn(c, pivot, axis))
          continue;
        axisCrossScanCell(c, axis, occ, start,
                          &layerNegU, &layerPosU, &layerNegV, &layerPosV,
                          &slotOccupied, &restricted);
      }

      bool cross = (layerPosU && seenNegU) || (layerNegU && seenPosU) ||
                   (layerPosV && seenNegV) || (layerNegV && seenPosV);
      if (cross) {
        for (cellSet::const_iterator it = slotOccupied.begin(); it != slotOccupied.end(); ++it)
          blocking.insert(*it);
      }

      seenNegU |= layerNegU;
      seenPosU |= layerPosU;
      seenNegV |= layerNegV;
      seenPosV |= layerPosV;
    }

    for (cellSet::const_iterator it = slotOccupied.begin(); it != slotOccupied.end(); ++it)
      clearance.insert(*it);
  }

  for (cellSet::const_iterator it = blocking.begin(); it != blocking.end(); ++it)
    outBlocking.push_back(*it);
  for (cellSet::const_iterator it = clearance.begin(); it != clearance.end(); ++it)
    if (blocking.find(*it) == blocking.end())
      outClearance.push_back(*it);
  for (cellSet::const_iterator it = restricted.begin(); it != restricted.end(); ++it)
    if (blocking.find(*it) == blocking.end() && clearance.find(*it) == clearance.end())
      outRestricted.push_back(*it);
}
