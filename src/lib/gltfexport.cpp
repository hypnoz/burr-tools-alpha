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
#include "gltfexport.h"

#include "problem.h"
#include "puzzle.h"
#include "solution.h"
#include "assembly.h"
#include "disassembly.h"
#include "disasmtomoves.h"
#include "gridtype.h"
#include "voxel.h"
#include "sliding.h"
#include "stacking.h"

#include "../halfedge/polyhedron.h"
#include "../halfedge/face.h"
#include "../halfedge/vertex.h"
#include "../halfedge/halfedge.h"
#include "../halfedge/vector3.h"
#include "../halfedge/modifiers.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>

/* The scene graph is a root node that scales cells to metres, with one
 * child per placed piece. Each piece has a single mesh: its shape in the
 * orientation it has in the assembly. The animation drives the piece node
 * with the same positions the 3D view gets from disasmToMoves_c. When a
 * rotation move leaves the piece in another orientation, the node rotation
 * takes over the difference, so a tumble stays one smooth motion.
 */

namespace {

  struct vec3 { double x, y, z; };

  struct mat3 {
    double m[9];
    vec3 operator*(const vec3 & v) const {
      return { m[0]*v.x + m[1]*v.y + m[2]*v.z,
               m[3]*v.x + m[4]*v.y + m[5]*v.z,
               m[6]*v.x + m[7]*v.y + m[8]*v.z };
    }
    mat3 operator*(const mat3 & o) const {
      mat3 r;
      for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
          r.m[3*i+j] = m[3*i]*o.m[j] + m[3*i+1]*o.m[3+j] + m[3*i+2]*o.m[6+j];
      return r;
    }
    mat3 transposed(void) const {
      return {{ m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8] }};
    }
    double det(void) const {
      return m[0]*(m[4]*m[8]-m[5]*m[7]) - m[1]*(m[3]*m[8]-m[5]*m[6]) + m[2]*(m[3]*m[7]-m[4]*m[6]);
    }
    static mat3 identity(void) { return {{ 1, 0, 0, 0, 1, 0, 0, 0, 1 }}; }
  };

  vec3 operator+(const vec3 & a, const vec3 & b) { return { a.x+b.x, a.y+b.y, a.z+b.z }; }
  vec3 operator-(const vec3 & a, const vec3 & b) { return { a.x-b.x, a.y-b.y, a.z-b.z }; }

  /* rotation by deg degrees about the unit axis (ax, ay, az), as glRotatef */
  mat3 axisAngle(double deg, double ax, double ay, double az) {
    double r = deg * M_PI / 180.0;
    double c = cos(r), s = sin(r), t = 1 - c;
    return {{ t*ax*ax + c,    t*ax*ay - s*az, t*ax*az + s*ay,
              t*ax*ay + s*az, t*ay*ay + c,    t*ay*az - s*ax,
              t*ax*az - s*ay, t*ay*az + s*ax, t*az*az + c }};
  }

  /* unit quaternion (x, y, z, w) of a proper rotation matrix */
  void toQuat(const mat3 & R, float q[4]) {
    const double * m = R.m;
    double tr = m[0] + m[4] + m[8];
    double x, y, z, w;
    if (tr > 0) {
      double s = sqrt(tr + 1.0) * 2;
      w = 0.25 * s; x = (m[7] - m[5]) / s; y = (m[2] - m[6]) / s; z = (m[3] - m[1]) / s;
    } else if (m[0] > m[4] && m[0] > m[8]) {
      double s = sqrt(1.0 + m[0] - m[4] - m[8]) * 2;
      w = (m[7] - m[5]) / s; x = 0.25 * s; y = (m[1] + m[3]) / s; z = (m[2] + m[6]) / s;
    } else if (m[4] > m[8]) {
      double s = sqrt(1.0 + m[4] - m[0] - m[8]) * 2;
      w = (m[2] - m[6]) / s; x = (m[1] + m[3]) / s; y = 0.25 * s; z = (m[5] + m[7]) / s;
    } else {
      double s = sqrt(1.0 + m[8] - m[0] - m[4]) * 2;
      w = (m[3] - m[1]) / s; x = (m[2] + m[6]) / s; y = (m[5] + m[7]) / s; z = 0.25 * s;
    }
    double n = sqrt(x*x + y*y + z*z + w*w);
    q[0] = float(x / n); q[1] = float(y / n); q[2] = float(z / n); q[3] = float(w / n);
  }

  /* glTF colours are linear, the 3D view's are sRGB */
  float srgbToLinear(float c) {
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
  }

  /* one indexed triangle list per shape, flat shaded: a vertex is shared
   * only by triangles with the same normal */
  struct meshData {
    std::vector<float> pos, nrm;
    std::vector<uint32_t> idx;
    vec3 lo { 0, 0, 0 }, hi { 0, 0, 0 };
    vec3 centre(void) const { return { (lo.x+hi.x)/2, (lo.y+hi.y)/2, (lo.z+hi.z)/2 }; }
  };

  bool buildMesh(const voxel_c & v, bool bevel, meshData & out) {
    /* The bevelled mesh is what the 3D view draws. Every cell edge is a
     * strip of its own, so it is large. The flat mesh merges into a few
     * big faces per side. */
    std::unique_ptr<Polyhedron> poly(bevel ? v.getDrawingMesh() : v.getFlatMesh());
    if (!poly) return false;
    if (!bevel)
      poly.reset(mergeCoplanarFaces(*poly));

    bool first = true;
    std::map<std::array<float, 6>, uint32_t> seen;
    auto add = [&](const Vector3Df & p, const Vector3Df & n) {
      std::array<float, 6> key { p.x(), p.y(), p.z(), n.x(), n.y(), n.z() };
      auto it = seen.find(key);
      if (it != seen.end()) {
        out.idx.push_back(it->second);
        return;
      }
      uint32_t i = uint32_t(out.pos.size() / 3);
      seen[key] = i;
      out.idx.push_back(i);
      out.pos.push_back(p.x()); out.pos.push_back(p.y()); out.pos.push_back(p.z());
      out.nrm.push_back(n.x()); out.nrm.push_back(n.y()); out.nrm.push_back(n.z());
      vec3 q { p.x(), p.y(), p.z() };
      if (first) { out.lo = out.hi = q; first = false; }
      out.lo = { fmin(out.lo.x, q.x), fmin(out.lo.y, q.y), fmin(out.lo.z, q.z) };
      out.hi = { fmax(out.hi.x, q.x), fmax(out.hi.y, q.y), fmax(out.hi.z, q.z) };
    };

    for (Polyhedron::const_face_iterator it = poly->fBegin(); it != poly->fEnd(); ++it) {
      const Face * f = *it;
      if (f->hole() || (f->_flags & FF_INSIDE_FACE))
        continue;

      Vector3Df n = f->normal();
      if (std::isnan(n.x()) || std::isnan(n.y()) || std::isnan(n.z()))
        continue;

      /* the same fan the 3D view draws */
      Face::const_edge_circulator e = f->begin();
      Face::const_edge_circulator sentinel = e;
      ++e;
      Vector3Df start = (*e)->dst()->position();
      ++e;
      do {
        add(start, n);
        add((*e)->dst()->position(), n);
        ++e;
        add((*e)->dst()->position(), n);
      } while (e != sentinel);
    }

    return !out.pos.empty();
  }

  /* where a piece is at one keyframe */
  struct pose {
    float t[3] = { 0, 0, 0 };
    float r[4] = { 0, 0, 0, 1 };
    bool visible = false;
  };

  /* placed piece: its part, shape and starting orientation */
  struct pieceInfo {
    unsigned int piece = 0, part = 0, trans0 = 0;
    mat3 base0inv = mat3::identity();  // inverse of the geometric matrix of trans0
    vec3 centre0 { 0, 0, 0 };          // bounding box centre of the trans0 mesh
    std::string name;
  };

  /* the binary chunk and the accessors that index into it */
  class glbBuilder {
    public:
      std::vector<unsigned char> bin;
      std::ostringstream views, accessors;
      unsigned int nViews = 0, nAccessors = 0;

      unsigned int addFloats(const std::vector<float> & data, unsigned int comps,
                             const char * type, bool minmax, int target) {
        while (bin.size() % 4) bin.push_back(0);
        size_t offset = bin.size();
        bin.resize(offset + data.size() * 4);
        memcpy(bin.data() + offset, data.data(), data.size() * 4);

        if (nViews) views << ",";
        views << "{\"buffer\":0,\"byteOffset\":" << offset << ",\"byteLength\":" << data.size() * 4;
        if (target) views << ",\"target\":" << target;
        views << "}";

        if (nAccessors) accessors << ",";
        accessors << "{\"bufferView\":" << nViews << ",\"componentType\":5126,\"count\":"
                  << data.size() / comps << ",\"type\":\"" << type << "\"";
        if (minmax) {
          std::vector<float> lo(comps, 0), hi(comps, 0);
          for (size_t i = 0; i < data.size(); i++) {
            unsigned int c = i % comps;
            if (i < comps || data[i] < lo[c]) lo[c] = data[i];
            if (i < comps || data[i] > hi[c]) hi[c] = data[i];
          }
          accessors << ",\"min\":[";
          for (unsigned int c = 0; c < comps; c++) accessors << (c ? "," : "") << lo[c];
          accessors << "],\"max\":[";
          for (unsigned int c = 0; c < comps; c++) accessors << (c ? "," : "") << hi[c];
          accessors << "]";
        }
        accessors << "}";

        nViews++;
        return nAccessors++;
      }

      unsigned int addIndices(const std::vector<uint32_t> & data) {
        while (bin.size() % 4) bin.push_back(0);
        size_t offset = bin.size();
        bin.resize(offset + data.size() * 4);
        memcpy(bin.data() + offset, data.data(), data.size() * 4);

        if (nViews) views << ",";
        views << "{\"buffer\":0,\"byteOffset\":" << offset << ",\"byteLength\":" << data.size() * 4
              << ",\"target\":34963}";
        if (nAccessors) accessors << ",";
        accessors << "{\"bufferView\":" << nViews << ",\"componentType\":5125,\"count\":"
                  << data.size() << ",\"type\":\"SCALAR\"}";

        nViews++;
        return nAccessors++;
      }
  };

  std::string jsonString(const std::string & s) {
    std::string r = "\"";
    for (char c : s) {
      if (c == '"' || c == '\\') { r += '\\'; r += c; }
      else if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, 8, "\\u%04x", c); r += b; }
      else r += c;
    }
    return r + "\"";
  }
}

std::string gltfExport::solutionAnimation(const problem_c & pr, unsigned int solNum,
                                          const std::vector<color_c> & pieceColors,
                                          const options_c & opt,
                                          std::vector<unsigned char> & glb) {

  if (stacking::isStacking(pr))
    return "Stacking puzzles cannot be exported as an animation.";
  if (!pr.resultValid())
    return "The problem has no valid result shape.";
  if (solNum >= pr.getNumberOfSavedSolutions())
    return "There is no such solution.";

  const solution_c * sol = pr.getSavedSolution(solNum);
  const separation_c * tree = sol->getDisassembly();
  const assembly_c * assm = sol->getAssembly();
  if (!tree || !assm)
    return "This solution has no disassembly to animate.";

  const gridType_c * gt = pr.getPuzzle().getGridType();
  const unsigned int numPieces = pr.getNumberOfPieces();

  /* the same driver and settings as the solution slider */
  disasmToMoves_c dtm(tree, 2 * getResultShape(pr)->getBiggestDimension(), numPieces);
  if (sliding::isSliding(pr))
    sliding::applySlideRoutes(pr, *tree, dtm);

  float cx, cy, cz;
  getResultShape(pr)->calculateSize(&cx, &cy, &cz);
  const vec3 centre { cx * 0.5, cy * 0.5, cz * 0.5 };

  /* the placed pieces, with their mesh in assembly orientation */
  std::vector<pieceInfo> pieces;
  std::vector<meshData> meshes;
  {
    unsigned int piece = 0;
    for (unsigned int p = 0; p < pr.getNumberOfParts(); p++)
      for (unsigned int q = 0; q < pr.getPartMaximum(p); q++, piece++) {
        if (!assm->isPlaced(piece))
          continue;

        pieceInfo pi;
        pi.piece = piece;
        pi.part = p;
        pi.trans0 = assm->getTransformation(piece);

        std::unique_ptr<voxel_c> v(gt->getVoxel(pr.getPartShape(p)));
        if (!v->transform(pi.trans0))
          return "A piece could not be put into its assembly orientation.";

        meshData m;
        if (!buildMesh(*v, opt.bevel, m))
          return "A piece mesh could not be generated.";

        mat3 b = mat3::identity();
        v->getTransformMatrix(pi.trans0, b.m);
        pi.base0inv = b.transposed();
        pi.centre0 = m.centre();

        pi.name = pr.getPuzzle().getShape(pr.getShapeIdOfPart(p))->getName();
        if (pi.name.empty())
          pi.name = "S" + std::to_string(pr.getShapeIdOfPart(p) + 1);
        if (pr.getPartMaximum(p) > 1)
          pi.name += " #" + std::to_string(q + 1);

        pieces.push_back(pi);
        meshes.push_back(std::move(m));
      }
  }
  if (pieces.empty())
    return "The solution places no pieces.";

  /* bounding box centre of each piece mesh in every orientation it takes,
   * which locates the rotated base mesh */
  std::vector<std::vector<std::pair<unsigned int, vec3>>> orientCentre(pieces.size());
  auto meshCentre = [&](size_t i, unsigned int t, vec3 & c) -> bool {
    for (auto & oc : orientCentre[i])
      if (oc.first == t) { c = oc.second; return true; }
    std::unique_ptr<voxel_c> v(gt->getVoxel(pr.getPartShape(pieces[i].part)));
    meshData m;
    if (!v->transform(t) || !buildMesh(*v, opt.bevel, m))
      return false;
    c = m.centre();
    orientCentre[i].push_back({ t, c });
    return true;
  };

  /* pose of every piece at one point of the animation */
  auto evaluate = [&](float step, std::vector<pose> & out) -> bool {
    dtm.setStep(step, false, true);
    out.assign(pieces.size(), pose());

    for (size_t i = 0; i < pieces.size(); i++) {
      const pieceInfo & pi = pieces[i];
      const unsigned int p = pi.piece;

      out[i].visible = dtm.getA(p) > 0;

      unsigned int t = dtm.getTrans(p);
      if (t == (unsigned int)-1)
        t = pi.trans0;

      std::unique_ptr<voxel_c> v(gt->getVoxel(pr.getPartShape(pi.part)));
      if (!v->transform(t))
        return false;

      /* the 3D view draws the mesh of orientation t at (x - hotspot - centre),
       * see voxelFrame_c::drawVoxelSpace */
      float x = dtm.getX(p), y = dtm.getY(p), z = dtm.getZ(p);
      v->recalcSpaceCoordinates(&x, &y, &z);
      float hx = v->getHx(), hy = v->getHy(), hz = v->getHz();
      v->recalcSpaceCoordinates(&hx, &hy, &hz);
      const vec3 place { x - hx, y - hy, z - hz };

      /* mesh_t = R * mesh_0 + d */
      mat3 R = mat3::identity();
      vec3 d { 0, 0, 0 };
      if (t != pi.trans0) {
        mat3 mt = mat3::identity();
        v->getTransformMatrix(t, mt.m);
        R = mt * pi.base0inv;
        if (fabs(R.det() - 1) > 1e-6)
          return false;
        vec3 ct;
        if (!meshCentre(i, t, ct))
          return false;
        d = ct - R * pi.centre0;
      }

      /* the tumble about the pivot cell centre, local to the mesh_t frame */
      mat3 P = mat3::identity();
      vec3 pl { 0, 0, 0 };
      float ang, ax, ay, az, px, py, pz;
      if (dtm.getRotationAnim(p, &ang, &ax, &ay, &az, &px, &py, &pz)) {
        P = axisAngle(ang, ax, ay, az);
        pl = vec3 { px, py, pz } - place;
      }

      /* world = P * (R * m0 + d - pl) + pl + place - centre */
      vec3 tr = P * (d - pl) + pl + place - centre;
      out[i].t[0] = float(tr.x); out[i].t[1] = float(tr.y); out[i].t[2] = float(tr.z);
      toQuat(P * R, out[i].r);
    }
    return true;
  };

  /* the step values to sample, in playback order */
  const unsigned int steps = tree->sumSteps();
  const unsigned int K = opt.samplesPerStep ? opt.samplesPerStep : 1;
  std::vector<float> forward;
  for (unsigned int s = 0; s < steps; s++)
    for (unsigned int k = 0; k < K; k++)
      forward.push_back(s + float(k) / K);
  forward.push_back(float(steps));

  std::vector<std::vector<pose>> fwdPoses(forward.size());
  for (size_t j = 0; j < forward.size(); j++)
    if (!evaluate(forward[j], fwdPoses[j]))
      return "The animation could not be computed for this puzzle.";

  /* A hidden piece still has a pose, and LINEAR interpolation runs towards
   * it while the piece is visible. Give it the pose it has just before it
   * vanishes (or just after it appears), so it does not stall or drift. */
  const float eps = 1.0f / (64.0f * K);
  for (size_t j = 0; j < forward.size(); j++)
    for (size_t i = 0; i < pieces.size(); i++) {
      if (fwdPoses[j][i].visible)
        continue;
      std::vector<pose> near;
      if (j > 0 && fwdPoses[j-1][i].visible && evaluate(forward[j] - eps, near) && near[i].visible) {
        fwdPoses[j][i] = near[i];
      } else if (j + 1 < forward.size() && fwdPoses[j+1][i].visible && evaluate(forward[j] + eps, near) && near[i].visible) {
        fwdPoses[j][i] = near[i];
      } else {
        /* copy the nearest visible keyframe */
        for (size_t dj = 1; dj < forward.size(); dj++) {
          if (j >= dj && fwdPoses[j-dj][i].visible) { fwdPoses[j][i] = fwdPoses[j-dj][i]; break; }
          if (j + dj < forward.size() && fwdPoses[j+dj][i].visible) { fwdPoses[j][i] = fwdPoses[j+dj][i]; break; }
        }
      }
      fwdPoses[j][i].visible = false;
    }

  /* keyframe times: a hold on the assembled puzzle, the moves, a hold,
   * and optionally the moves backwards */
  const float dt = (opt.secondsPerStep > 0 ? opt.secondsPerStep : 0.5f) / K;
  const float hold = 2 * dt * K;
  std::vector<float> times;
  std::vector<size_t> keys;   // index into forward / fwdPoses

  times.push_back(0);
  keys.push_back(0);
  for (size_t j = 0; j < forward.size(); j++) {
    times.push_back(hold + j * dt);
    keys.push_back(j);
  }
  times.push_back(times.back() + hold);
  keys.push_back(forward.size() - 1);
  if (opt.reassemble) {
    float t0 = times.back();
    for (size_t j = 1; j < forward.size(); j++) {
      times.push_back(t0 + j * dt);
      keys.push_back(forward.size() - 1 - j);
    }
    times.push_back(times.back() + hold);
    keys.push_back(0);
  }

  /* keep consecutive quaternions in one hemisphere, so slerp takes the short way */
  std::vector<std::vector<pose>> track(pieces.size(), std::vector<pose>(keys.size()));
  for (size_t i = 0; i < pieces.size(); i++)
    for (size_t j = 0; j < keys.size(); j++) {
      pose ps = fwdPoses[keys[j]][i];
      if (j > 0) {
        const float * q = track[i][j-1].r;
        if (q[0]*ps.r[0] + q[1]*ps.r[1] + q[2]*ps.r[2] + q[3]*ps.r[3] < 0)
          for (float & c : ps.r) c = -c;
      }
      track[i][j] = ps;
    }

  /* --- write the glTF ---------------------------------------------------- */

  glbBuilder b;
  std::ostringstream meshesJ, materialsJ, nodesJ, channelsJ, samplersJ;

  /* Most keyframes lie on the line between their neighbours: a piece
   * that waits, or slides straight. Each channel keeps only the keyframes
   * its interpolation does not reproduce, with its own time accessor. */
  unsigned int nSamplers = 0, nChannels = 0;
  auto addChannel = [&](unsigned int node, const char * path, const std::vector<float> & data,
                        unsigned int comps, const char * type, bool step) {
    const size_t n = times.size();
    auto at = [&](size_t k) { return &data[k * comps]; };

    /* value at t of the segment from key a to key c */
    auto reproduces = [&](size_t a, size_t c, size_t k) {
      const float * va = at(a), * vc = at(c), * vk = at(k);
      if (step) {
        for (unsigned int m = 0; m < comps; m++)
          if (va[m] != vk[m]) return false;
        return true;
      }
      float f = (times[k] - times[a]) / (times[c] - times[a]);
      float w0 = 1 - f, w1 = f;
      if (comps == 4) {
        /* slerp, as the viewer will */
        double d = va[0]*vc[0] + va[1]*vc[1] + va[2]*vc[2] + va[3]*vc[3];
        if (d < 0.9999) {
          double th = acos(d < 1 ? d : 1);
          w0 = float(sin((1 - f) * th) / sin(th));
          w1 = float(sin(f * th) / sin(th));
        }
      }
      for (unsigned int m = 0; m < comps; m++)
        if (fabs(w0 * va[m] + w1 * vc[m] - vk[m]) > 1e-4f) return false;
      return true;
    };

    std::vector<size_t> keep { 0 };
    for (size_t k = 1; k + 1 < n; k++) {
      bool needed = false;
      for (size_t m = keep.back() + 1; m <= k && !needed; m++)
        needed = !reproduces(keep.back(), k + 1, m);
      if (needed)
        keep.push_back(k);
    }
    if (n > 1)
      keep.push_back(n - 1);

    std::vector<float> kt, kv;
    for (size_t k : keep) {
      kt.push_back(times[k]);
      kv.insert(kv.end(), at(k), at(k) + comps);
    }

    unsigned int input = b.addFloats(kt, 1, "SCALAR", true, 0);
    unsigned int output = b.addFloats(kv, comps, type, false, 0);
    if (nSamplers) samplersJ << ",";
    samplersJ << "{\"input\":" << input << ",\"output\":" << output
              << ",\"interpolation\":\"" << (step ? "STEP" : "LINEAR") << "\"}";
    if (nChannels) channelsJ << ",";
    channelsJ << "{\"sampler\":" << nSamplers << ",\"target\":{\"node\":" << node
              << ",\"path\":\"" << path << "\"}}";
    nSamplers++;
    nChannels++;
  };

  for (size_t i = 0; i < pieces.size(); i++) {
    const pieceInfo & pi = pieces[i];

    unsigned int posAcc = b.addFloats(meshes[i].pos, 3, "VEC3", true, 34962);
    unsigned int nrmAcc = b.addFloats(meshes[i].nrm, 3, "VEC3", false, 34962);
    unsigned int idxAcc = b.addIndices(meshes[i].idx);

    color_c c = pi.piece < pieceColors.size() ? pieceColors[pi.piece] : color_c { 0.7f, 0.7f, 0.7f };

    if (i) { meshesJ << ","; materialsJ << ","; nodesJ << ","; }
    materialsJ << "{\"name\":" << jsonString(pi.name)
               << ",\"pbrMetallicRoughness\":{\"baseColorFactor\":["
               << srgbToLinear(c.r) << "," << srgbToLinear(c.g) << "," << srgbToLinear(c.b)
               << ",1],\"metallicFactor\":0,\"roughnessFactor\":0.6},\"doubleSided\":true}";
    meshesJ << "{\"name\":" << jsonString(pi.name) << ",\"primitives\":[{\"attributes\":{\"POSITION\":"
            << posAcc << ",\"NORMAL\":" << nrmAcc << "},\"indices\":" << idxAcc << ",\"material\":" << i << ",\"mode\":4}]}";

    const pose & p0 = track[i][0];
    nodesJ << "{\"name\":" << jsonString(pi.name) << ",\"mesh\":" << i
           << ",\"translation\":[" << p0.t[0] << "," << p0.t[1] << "," << p0.t[2] << "]"
           << ",\"rotation\":[" << p0.r[0] << "," << p0.r[1] << "," << p0.r[2] << "," << p0.r[3] << "]"
           << ",\"scale\":" << (p0.visible ? "[1,1,1]" : "[0,0,0]") << "}";

    std::vector<float> tr, rot, sc;
    bool rotates = false, hides = false;
    for (const pose & ps : track[i]) {
      tr.insert(tr.end(), ps.t, ps.t + 3);
      rot.insert(rot.end(), ps.r, ps.r + 4);
      float s = ps.visible ? 1.0f : 0.0f;
      sc.insert(sc.end(), { s, s, s });
      if (fabs(ps.r[0] - p0.r[0]) + fabs(ps.r[1] - p0.r[1]) + fabs(ps.r[2] - p0.r[2]) + fabs(ps.r[3] - p0.r[3]) > 1e-6)
        rotates = true;
      if (ps.visible != p0.visible)
        hides = true;
    }

    addChannel(i + 1, "translation", tr, 3, "VEC3", false);
    if (rotates)
      addChannel(i + 1, "rotation", rot, 4, "VEC4", false);
    if (hides)
      addChannel(i + 1, "scale", sc, 3, "VEC3", true);
  }

  std::ostringstream j;
  j.precision(7);
  j << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"BurrTools\"}"
    << ",\"scene\":0,\"scenes\":[{\"name\":\"Solution " << solNum + 1 << "\",\"nodes\":[0]}]"
    << ",\"nodes\":[{\"name\":\"Puzzle\",\"scale\":[" << opt.cellSize << "," << opt.cellSize << "," << opt.cellSize << "]"
    << ",\"children\":[";
  for (size_t i = 0; i < pieces.size(); i++)
    j << (i ? "," : "") << i + 1;
  j << "]}," << nodesJ.str() << "]"
    << ",\"meshes\":[" << meshesJ.str() << "]"
    << ",\"materials\":[" << materialsJ.str() << "]"
    << ",\"animations\":[{\"name\":\"Disassembly\",\"channels\":[" << channelsJ.str()
    << "],\"samplers\":[" << samplersJ.str() << "]}]"
    << ",\"accessors\":[" << b.accessors.str() << "]"
    << ",\"bufferViews\":[" << b.views.str() << "]"
    << ",\"buffers\":[{\"byteLength\":" << b.bin.size() << "}]}";

  std::string json = j.str();
  while (json.size() % 4) json += ' ';
  while (b.bin.size() % 4) b.bin.push_back(0);

  auto put32 = [&](uint32_t v) {
    for (int k = 0; k < 4; k++) glb.push_back((unsigned char)(v >> (8 * k)));
  };

  glb.clear();
  put32(0x46546C67);                                   // "glTF"
  put32(2);
  put32(uint32_t(12 + 8 + json.size() + 8 + b.bin.size()));
  put32(uint32_t(json.size()));
  put32(0x4E4F534A);                                   // "JSON"
  glb.insert(glb.end(), json.begin(), json.end());
  put32(uint32_t(b.bin.size()));
  put32(0x004E4942);                                   // "BIN\0"
  glb.insert(glb.end(), b.bin.begin(), b.bin.end());

  return "";
}

std::string gltfExport::writeSolutionAnimation(const char * fname,
                                               const problem_c & pr, unsigned int sol,
                                               const std::vector<color_c> & pieceColors,
                                               const options_c & opt) {
  std::vector<unsigned char> glb;
  std::string err = solutionAnimation(pr, sol, pieceColors, opt, glb);
  if (!err.empty())
    return err;

  std::unique_ptr<FILE, int(*)(FILE*)> f(fopen(fname, "wb"), &fclose);
  if (!f)
    return "Could not open the file for writing.";
  if (fwrite(glb.data(), 1, glb.size(), f.get()) != glb.size())
    return "Could not write the file.";
  return "";
}
