// This file is part of Hermes2D.
//
// Hermes2D is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Hermes2D is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Hermes2D.  If not, see <http://www.gnu.org/licenses/>.

#include "hermes2d_common_defs.h"
#include "shapeset.h"
#include "matrix.h"

using namespace Hermes::Algebra::DenseMatrixOperations;

namespace Hermes
{
  namespace Hermes2D
  {
    double* Shapeset::calculate_constrained_edge_combination(int order, int part, int ori)
    {
      /*
      "ebias" is the order of the first edge function, this has to be subtracted
      from the order to get a reasonable numbering of the edge functions, starting
      from 0. For H1 ebias is 2 and for Hcurl it is 0.
      */
      assert((order - ebias) >= 0);
      assert(part >= 0);

      int i, j, n;

      // determine the interval of the edge
      for (n = 2; n <= part; n <<= 1)
        part -= n;

      double n2 = 2.0 / n;
      double hi = -((double) part * n2 - 1.0);
      double lo = -((double) (part + 1) * n2 - 1.0);

      int idx[16];
      ori = ori ? 0 : 1;
      for (i = 0; i <= order; i++)
        idx[i] = get_edge_index(0, ori, i);

      // function values at the endpoints (for subtracting of the linear part in H1)
      double c = 1.0;
      double f_lo = 0.0, f_hi = 0.0;
      if (ebias == 2)
      {
        f_lo = get_value(0, idx[order], lo, -1.0, 0);
        f_hi = get_value(0, idx[order], hi, -1.0, 0);
      }
      else
      {
        // this is for H(curl), where we need to multiply the constrained function
        // so that its vectors are not shortened and fit the large constraining fn.
        c = (hi - lo) / 2.0;
      }

      // fill the matrix of the linear system
      n = order + 1 - ebias;
      int space_type = this->get_space_type();
      int component = (space_type == HERMES_HDIV_SPACE)? 1 : 0;
      double** a = new_matrix<double>(n, n);
      double* b = new double[n];
      for (i = 0; i < n; i++)
      {
        // chebyshev point
        int o = (ebias == 0) ? order + 1 : order;
        double p = cos((i + 1) * M_PI / o);
        double r = (p + 1.0) * 0.5;
        double s = 1.0 - r;

        // matrix row
        for (j = 0; j < n; j++)
          a[i][j] = get_value(0, idx[j + ebias], p, -1.0, component);

        // rhs
        b[i] = c * get_value(0, idx[order], lo*s + hi*r, -1.0, component) - f_lo*s - f_hi*r;
      }

      // solve the system
      double d;
      int* iperm = new int[n];
      ludcmp(a, n, iperm, &d);
      lubksb(a, n, iperm, b);

      // cleanup
      delete [] iperm;
      delete [] a;

      return b;
    }

    double* Shapeset::get_constrained_edge_combination(int order, int part, int ori, int& nitems)
    {
      int index = 2*((max_order + 1 - ebias)*part + (order - ebias)) + ori;

      // allocate/reallocate the array if necessary
      if (comb_table == NULL)
      {
        table_size = 1024;
        while (table_size <= index) table_size *= 2;
        comb_table = (double**) malloc(table_size * sizeof(double*));
        memset(comb_table, 0, table_size * sizeof(double*));
      }
      else if (index >= table_size)
      {
        // adjust table_size to accommodate the required depth
        int old_size = table_size;
        while (index >= table_size) table_size *= 2;

        // reallocate the table
        verbose("Shapeset::get_constrained_edge_combination(): realloc to table_size = %d", table_size);
        comb_table = (double**) realloc(comb_table, table_size * sizeof(double*));
        memset(comb_table + old_size, 0, (table_size - old_size) * sizeof(double*));
      }

      // do we have the required linear combination yet?
      if (comb_table[index] == NULL)
      {
        // no, calculate it
        comb_table[index] = calculate_constrained_edge_combination(order, part, ori);
      }

      nitems = order + 1 - ebias;
      return comb_table[index];
    }

    void Shapeset::free_constrained_edge_combinations()
    {
      if (comb_table != NULL)
      {
        for (int i = 0; i < table_size; i++)
          if (comb_table[i] != NULL)
            delete [] comb_table[i];

        free(comb_table);
        comb_table = NULL;
      }
    }

    double Shapeset::get_constrained_value(int n, int index, double x, double y, int component)
    {
      index = -1 - index;

      int part = (unsigned) index >> 7;
      int order = (index >> 3) & 15;
      int edge = (index >> 1) & 3;
      int ori = index & 1;

      int i, nc;
      double sum, *comb = get_constrained_edge_combination(order, part, ori, nc);

      sum = 0.0;
      shape_fn_t* table = shape_table[n][mode][component];
      for (i = 0; i < nc; i++)
        sum += comb[i] * table[get_edge_index(edge, ori, i + ebias)](x, y);

      return sum;
    }

    Shapeset::~Shapeset() { free_constrained_edge_combinations(); }

    /// Selects HERMES_MODE_TRIANGLE or HERMES_MODE_QUAD.
    void Shapeset::set_mode(int mode)
    {
      assert(mode == HERMES_MODE_TRIANGLE || mode == HERMES_MODE_QUAD);
      this->mode = mode;
      nvert = (mode == HERMES_MODE_TRIANGLE) ? 3 : 4;
    }

    /// Returns the current mode.
    int Shapeset::get_mode() const { return mode; }

    /// Returns the maximum poly degree for all shape functions.
    int Shapeset::get_max_order() const { return max_order; }

    /// Returns the highest shape function index.
    int Shapeset::get_max_index() const { return max_index[mode]; }

    /// Returns 2 if this is a vector shapeset, 1 otherwise.
    int Shapeset::get_num_components() const { return num_components; }

    /// Returns the index of a vertex shape function associated with the specified vertex.
    int Shapeset::get_vertex_index(int vertex) const
    {
      assert(vertex >= 0 && vertex < nvert);
      return vertex_indices[mode][vertex];
    }

    /// Returns the index of an edge function associated with the specified edge and of the
    /// requested order. 'ori' can be 0 or 1 and determines edge orientation (this is for
    /// shapesets with non-symmetric edge functions).
    int Shapeset::get_edge_index(int edge, int ori, int order) const
    {
      assert(edge >= 0 && edge < nvert);
      assert(order >= 0 && order <= max_order);
      assert(ori == 0 || ori == 1);
      return edge_indices[mode][edge][2*order + ori];
    }

    /// Returns a complete set of indices of bubble functions for an element of the given order.
    int* Shapeset::get_bubble_indices(int order) const
    {
      assert(H2D_GET_H_ORDER(order) >= 0 && H2D_GET_H_ORDER(order) <= max_order);
      assert(H2D_GET_V_ORDER(order) >= 0 && H2D_GET_V_ORDER(order) <= max_order);
      int index = order;
      if (mode == HERMES_MODE_QUAD) //tables of bubble indices are transposed
        index = H2D_MAKE_QUAD_ORDER(H2D_GET_V_ORDER(order), H2D_GET_H_ORDER(order));
      return bubble_indices[mode][index];
    }

    /// Returns the number of bubble functions for an element of the given order.
    int Shapeset::get_num_bubbles(int order) const
    {
      assert(H2D_GET_H_ORDER(order) >= 0 && H2D_GET_H_ORDER(order) <= max_order);
      assert(H2D_GET_V_ORDER(order) >= 0 && H2D_GET_V_ORDER(order) <= max_order);
      return bubble_count[mode][order];
    }

    /// Returns the index of a constrained edge function. 'part' is 0 or 1 for edge
    /// halves, 2, 3, 4, 5 for edge quarters, etc. See shapeset.cpp.
    int Shapeset::get_constrained_edge_index(int edge, int order, int ori, int part) const
    {
      assert(edge >= 0 && edge < nvert);
      assert(order >= 0 && order <= max_order);
      assert(part >= 0);
      assert(order <= H2D_ORDER_MASK);
      return -1 - ((part << 7) + (order << 3) + (edge << 1) + ori);
    }

    /// Returns the polynomial degree of the specified shape function.
    /// If on quads, it returns encoded orders. The orders has to be decoded through macros
    /// H2D_GET_H_ORDER and H2D_GET_V_ORDER.
    int Shapeset::get_order(int index) const
    {
      if (index >= 0) {
        assert(index >= 0 && index <= max_index[mode]);
        return index_to_order[mode][index];
      }
      else return ((-1 - index) >> 3) & 15;
    }
    int Shapeset::get_order(int index, int mode_) const
    {
      if (index >= 0) {
        assert(index >= 0 && index <= max_index[mode_]);
        return index_to_order[mode_][index];
      }
      else return ((-1 - index) >> 3) & 15;
    }

    /// Obtains the value of the given shape function. (x,y) is a coordinate in the reference
    /// domain, component is 0 for Scalar shapesets and 0 or 1 for vector shapesets.
    double Shapeset::get_value(int n, int index, double x, double y, int component)
    {
      if (index >= 0)
      {
        assert(index >= 0 && index <= max_index[mode]); assert(component >= 0 && component < num_components);
        Shapeset::shape_fn_t** shape_expansion = shape_table[n][mode];
        if (shape_expansion == NULL) { // requested exansion (f, df/dx, df/dy, ddf/dxdx, ...) is not defined
          static int warned_mode = -1, warned_index = -1, warned_n = 1; //just to keep the number of warnings low: warn just once about a given combinations of n, mode, and index.
          warn_if(warned_mode != mode || warned_index != index || warned_n != n, "Requested undefined expansion %d (mode: %d) of a shape %d, returning 0", n, mode, index);
          warned_mode = mode; warned_index = index; warned_n = n;
          return 0;
        }
        else
          return shape_expansion[component][index](x, y);
      }
      else
        return get_constrained_value(n, index, x, y, component);
    }

    double Shapeset::get_fn_value (int index, double x, double y, int component)  { return get_value(0, index, x, y, component); }
    double Shapeset::get_dx_value (int index, double x, double y, int component)  { return get_value(1, index, x, y, component); }
    double Shapeset::get_dy_value (int index, double x, double y, int component)  { return get_value(2, index, x, y, component); }
    double Shapeset::get_dxx_value(int index, double x, double y, int component)  { return get_value(3, index, x, y, component); }
    double Shapeset::get_dyy_value(int index, double x, double y, int component)  { return get_value(4, index, x, y, component); }
    double Shapeset::get_dxy_value(int index, double x, double y, int component) { return get_value(5, index, x, y, component); }

    /// Returns the coordinates of the reference domain vertices.
    double2* Shapeset::get_ref_vertex(int vertex)
    {
      return &ref_vert[mode][vertex];
    }
  }
}