// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <climits>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <type_traits>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/IndexType.hpp"
#include "DataStructures/Tensor/Symmetry.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/MakeWithValue.hpp"
#include "Utilities/TMPL.hpp"

namespace {
template <typename... Ts>
void assign_unique_values_to_tensor(
    gsl::not_null<Tensor<double, Ts...>*> tensor) {
  std::iota(tensor->begin(), tensor->end(), 0.0);
}

template <typename... Ts>
void assign_unique_values_to_tensor(
    gsl::not_null<Tensor<DataVector, Ts...>*> tensor) {
  double value = 0.0;
  for (auto index_it = tensor->begin(); index_it != tensor->end(); index_it++) {
    for (auto vector_it = index_it->begin(); vector_it != index_it->end();
         vector_it++) {
      *vector_it = value;
      value += 1.0;
    }
  }
}

// \brief Test the outer product and of a tensor expression and `double` is
// correctly evaluated
//
// \details
// The cases tested are:
// - \f$L_{ij} = R * S_{ij}\f$
// - \f$L_{ij} = S_{ij} * R\f$
// - \f$L_{ij} = R * S_{ij} * T\f$
//
// where \f$R\f$ and \f$T\f$ are `double`s and \f$S\f$ and \f$L\f$ are Tensors
// with data type `double` or DataVector.
//
// \tparam DataType the type of data being stored in the tensor operand of the
// products
template <typename DataType>
void test_outer_product_double(const DataType& used_for_size) {
  constexpr size_t dim = 3;
  using tensor_type =
      Tensor<DataType, Symmetry<1, 1>,
             index_list<SpatialIndex<dim, UpLo::Lo, Frame::Inertial>,
                        SpatialIndex<dim, UpLo::Lo, Frame::Inertial>>>;

  tensor_type S(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&S));

  // \f$L_{ij} = R * S_{ij}\f$
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const tensor_type Lij_from_R_Sij =
      tenex::evaluate<ti::i, ti::j>(5.6 * S(ti::i, ti::j));
  // \f$L_{ij} = S_{ij} * R\f$
  const tensor_type Lij_from_Sij_R =
      tenex::evaluate<ti::i, ti::j>(S(ti::i, ti::j) * -8.1);
  // \f$L_{ij} = R * S_{ij} * T\f$
  const tensor_type Lij_from_R_Sij_T =
      tenex::evaluate<ti::i, ti::j>(-1.7 * S(ti::i, ti::j) * 0.6);

  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < dim; j++) {
      CHECK(Lij_from_R_Sij.get(i, j) == 5.6 * S.get(i, j));
      CHECK(Lij_from_Sij_R.get(i, j) == S.get(i, j) * -8.1);
      CHECK(Lij_from_R_Sij_T.get(i, j) == -1.7 * S.get(i, j) * 0.6);
    }
  }
}

// \brief Test the outer product of a rank 0 tensor with another tensor is
// correctly evaluated
//
// \details
// The outer product cases tested are:
// - \f$L = R * R\f$
// - \f$L = R * R * R\f$
// - \f$L^{a} = R * S^{a}\f$
// - \f$L_{ai} = R * T_{ai}\f$
//
// For the last two cases, both operand orderings are tested. For the last case,
// both LHS index orderings are tested.
//
// \tparam DataType the type of data being stored in the product operands
template <typename DataType>
void test_outer_product_rank_0_operand(const DataType& used_for_size) {
  Tensor<DataType> R{{{used_for_size}}};
  if constexpr (std::is_same_v<DataType, double>) {
    // Replace tensor's value from `used_for_size` to a proper test value
    R.get() = -3.7;
  } else {
    assign_unique_values_to_tensor(make_not_null(&R));
  }

  // \f$L = R * R\f$
  CHECK(tenex::evaluate(R() * R()).get() == R.get() * R.get());
  // \f$L = R * R * R\f$
  CHECK(tenex::evaluate(R() * R() * R()).get() == R.get() * R.get() * R.get());

  Tensor<DataType, Symmetry<1>,
         index_list<SpacetimeIndex<3, UpLo::Up, Frame::Inertial>>>
      Su(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Su));

  // \f$L^{a} = R * S^{a}\f$
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const decltype(Su) LA_from_R_SA = tenex::evaluate<ti::A>(R() * Su(ti::A));
  // \f$L^{a} = S^{a} * R\f$
  const decltype(Su) LA_from_SA_R = tenex::evaluate<ti::A>(Su(ti::A) * R());

  for (size_t a = 0; a < 4; a++) {
    CHECK(LA_from_R_SA.get(a) == R.get() * Su.get(a));
    CHECK(LA_from_SA_R.get(a) == Su.get(a) * R.get());
  }

  Tensor<DataType, Symmetry<2, 1>,
         index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                    SpatialIndex<4, UpLo::Lo, Frame::Inertial>>>
      Tll(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Tll));

  // \f$L_{ai} = R * T_{ai}\f$
  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                          SpatialIndex<4, UpLo::Lo, Frame::Inertial>>>
      Lai_from_R_Tai = tenex::evaluate<ti::a, ti::i>(R() * Tll(ti::a, ti::i));
  // \f$L_{ia} = R * T_{ai}\f$
  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpatialIndex<4, UpLo::Lo, Frame::Inertial>,
                          SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>>>
      Lia_from_R_Tai = tenex::evaluate<ti::i, ti::a>(R() * Tll(ti::a, ti::i));
  // \f$L_{ai} = T_{ai} * R\f$
  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                          SpatialIndex<4, UpLo::Lo, Frame::Inertial>>>
      Lai_from_Tai_R = tenex::evaluate<ti::a, ti::i>(Tll(ti::a, ti::i) * R());
  // \f$L_{ia} = T_{ai} * R\f$
  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpatialIndex<4, UpLo::Lo, Frame::Inertial>,
                          SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>>>
      Lia_from_Tai_R = tenex::evaluate<ti::i, ti::a>(Tll(ti::a, ti::i) * R());

  for (size_t a = 0; a < 4; a++) {
    for (size_t i = 0; i < 4; i++) {
      const DataType expected_R_Tai_product = R.get() * Tll.get(a, i);
      CHECK(Lai_from_R_Tai.get(a, i) == expected_R_Tai_product);
      CHECK(Lia_from_R_Tai.get(i, a) == expected_R_Tai_product);

      const DataType expected_Tai_R_product = Tll.get(a, i) * R.get();
      CHECK(Lai_from_Tai_R.get(a, i) == expected_Tai_R_product);
      CHECK(Lia_from_Tai_R.get(i, a) == expected_Tai_R_product);
    }
  }
}

// \brief Test the outer product of rank 1 tensors with another tensor is
// correctly evaluated
//
// \details
// The outer product cases tested are:
// - \f$L^{a}{}_{i} = R_{i} * S^{a}\f$
// - \f$L^{ja}{}_{i} = R_{i} * S^{a} * T^{j}\f$
// - \f$L_{k}{}^{c}{}_{d} = S^{c} * G_{dk}\f$
// - \f$L^{c}{}_{dk} = G_{dk} * S^{c}\f$
//
// For each case, the LHS index order is different from the order in the
// operands.
//
// \tparam DataType the type of data being stored in the product operands
template <typename DataType>
void test_outer_product_rank_1_operand(const DataType& used_for_size) {
  Tensor<DataType, Symmetry<1>,
         index_list<SpatialIndex<3, UpLo::Lo, Frame::Grid>>>
      Rl(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Rl));

  Tensor<DataType, Symmetry<1>,
         index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>
      Su(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Su));

  // \f$L^{a}{}_{i} = R_{i} * S^{a}\f$
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                          SpatialIndex<3, UpLo::Lo, Frame::Grid>>>
      LAi_from_Ri_SA = tenex::evaluate<ti::A, ti::i>(Rl(ti::i) * Su(ti::A));

  for (size_t i = 0; i < 3; i++) {
    for (size_t a = 0; a < 4; a++) {
      CHECK(LAi_from_Ri_SA.get(a, i) == Rl.get(i) * Su.get(a));
    }
  }

  Tensor<DataType, Symmetry<1>,
         index_list<SpatialIndex<3, UpLo::Up, Frame::Grid>>>
      Tu(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Tu));

  // \f$L^{ja}{}_{i} = R_{i} * S^{a} * T^{j}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<SpatialIndex<3, UpLo::Up, Frame::Grid>,
                          SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                          SpatialIndex<3, UpLo::Lo, Frame::Grid>>>
      LJAi_from_Ri_SA_TJ = tenex::evaluate<ti::J, ti::A, ti::i>(
          Rl(ti::i) * Su(ti::A) * Tu(ti::J));

  for (size_t j = 0; j < 3; j++) {
    for (size_t a = 0; a < 4; a++) {
      for (size_t i = 0; i < 3; i++) {
        CHECK(LJAi_from_Ri_SA_TJ.get(j, a, i) ==
              Rl.get(i) * Su.get(a) * Tu.get(j));
      }
    }
  }

  Tensor<DataType, Symmetry<2, 1>,
         index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                    SpatialIndex<4, UpLo::Lo, Frame::Grid>>>
      Gll(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Gll));

  // \f$L_{k}{}^{c}{}_{d} = S^{c} * G_{dk}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<SpatialIndex<4, UpLo::Lo, Frame::Grid>,
                          SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                          SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>
      LkCd_from_SC_Gdk =
          tenex::evaluate<ti::k, ti::C, ti::d>(Su(ti::C) * Gll(ti::d, ti::k));
  // \f$L^{c}{}_{dk} = G_{dk} * S^{c}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                          SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                          SpatialIndex<4, UpLo::Lo, Frame::Grid>>>
      LCdk_from_Gdk_SC =
          tenex::evaluate<ti::C, ti::d, ti::k>(Gll(ti::d, ti::k) * Su(ti::C));

  for (size_t k = 0; k < 4; k++) {
    for (size_t c = 0; c < 4; c++) {
      for (size_t d = 0; d < 4; d++) {
        CHECK(LkCd_from_SC_Gdk.get(k, c, d) == Su.get(c) * Gll.get(d, k));
        CHECK(LCdk_from_Gdk_SC.get(c, d, k) == Gll.get(d, k) * Su.get(c));
      }
    }
  }
}

// \brief Test the outer product of two rank 2 tensors is correctly evaluated
//
// \details
// All LHS index orders are tested. One such example case:
// \f$L_{abc}{}^{i} = R_{ab} * S^{i}{}_{c}\f$
//
// \tparam DataType the type of data being stored in the product operands
template <typename DataType>
void test_outer_product_rank_2x2_operands(const DataType& used_for_size) {
  using R_index = SpacetimeIndex<3, UpLo::Lo, Frame::Grid>;
  using S_first_index = SpatialIndex<4, UpLo::Up, Frame::Grid>;
  using S_second_index = SpacetimeIndex<2, UpLo::Lo, Frame::Grid>;

  Tensor<DataType, Symmetry<1, 1>, index_list<R_index, R_index>> Rll(
      used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Rll));
  Tensor<DataType, Symmetry<2, 1>, index_list<S_first_index, S_second_index>>
      Sul(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Sul));

  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const Tensor<DataType, Symmetry<3, 3, 2, 1>,
               index_list<R_index, R_index, S_first_index, S_second_index>>
      L_abIc = tenex::evaluate<ti::a, ti::b, ti::I, ti::c>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 3, 2, 1>,
               index_list<R_index, R_index, S_second_index, S_first_index>>
      L_abcI = tenex::evaluate<ti::a, ti::b, ti::c, ti::I>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 3, 1>,
               index_list<R_index, S_first_index, R_index, S_second_index>>
      L_aIbc = tenex::evaluate<ti::a, ti::I, ti::b, ti::c>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 3>,
               index_list<R_index, S_first_index, S_second_index, R_index>>
      L_aIcb = tenex::evaluate<ti::a, ti::I, ti::c, ti::b>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 3, 1>,
               index_list<R_index, S_second_index, R_index, S_first_index>>
      L_acbI = tenex::evaluate<ti::a, ti::c, ti::b, ti::I>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 3>,
               index_list<R_index, S_second_index, S_first_index, R_index>>
      L_acIb = tenex::evaluate<ti::a, ti::c, ti::I, ti::b>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));

  const Tensor<DataType, Symmetry<3, 3, 2, 1>,
               index_list<R_index, R_index, S_first_index, S_second_index>>
      L_baIc = tenex::evaluate<ti::b, ti::a, ti::I, ti::c>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 3, 2, 1>,
               index_list<R_index, R_index, S_second_index, S_first_index>>
      L_bacI = tenex::evaluate<ti::b, ti::a, ti::c, ti::I>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 3, 1>,
               index_list<R_index, S_first_index, R_index, S_second_index>>
      L_bIac = tenex::evaluate<ti::b, ti::I, ti::a, ti::c>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 3>,
               index_list<R_index, S_first_index, S_second_index, R_index>>
      L_bIca = tenex::evaluate<ti::b, ti::I, ti::c, ti::a>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 3, 1>,
               index_list<R_index, S_second_index, R_index, S_first_index>>
      L_bcaI = tenex::evaluate<ti::b, ti::c, ti::a, ti::I>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 3>,
               index_list<R_index, S_second_index, S_first_index, R_index>>
      L_bcIa = tenex::evaluate<ti::b, ti::c, ti::I, ti::a>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));

  const Tensor<DataType, Symmetry<3, 2, 2, 1>,
               index_list<S_first_index, R_index, R_index, S_second_index>>
      L_Iabc = tenex::evaluate<ti::I, ti::a, ti::b, ti::c>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 2>,
               index_list<S_first_index, R_index, S_second_index, R_index>>
      L_Iacb = tenex::evaluate<ti::I, ti::a, ti::c, ti::b>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 2, 1>,
               index_list<S_first_index, R_index, R_index, S_second_index>>
      L_Ibac = tenex::evaluate<ti::I, ti::b, ti::a, ti::c>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 2>,
               index_list<S_first_index, R_index, S_second_index, R_index>>
      L_Ibca = tenex::evaluate<ti::I, ti::b, ti::c, ti::a>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 1>,
               index_list<S_first_index, S_second_index, R_index, R_index>>
      L_Icab = tenex::evaluate<ti::I, ti::c, ti::a, ti::b>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 1>,
               index_list<S_first_index, S_second_index, R_index, R_index>>
      L_Icba = tenex::evaluate<ti::I, ti::c, ti::b, ti::a>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));

  const Tensor<DataType, Symmetry<3, 2, 2, 1>,
               index_list<S_second_index, R_index, R_index, S_first_index>>
      L_cabI = tenex::evaluate<ti::c, ti::a, ti::b, ti::I>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 2>,
               index_list<S_second_index, R_index, S_first_index, R_index>>
      L_caIb = tenex::evaluate<ti::c, ti::a, ti::I, ti::b>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 2, 1>,
               index_list<S_second_index, R_index, R_index, S_first_index>>
      L_cbaI = tenex::evaluate<ti::c, ti::b, ti::a, ti::I>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 2>,
               index_list<S_second_index, R_index, S_first_index, R_index>>
      L_cbIa = tenex::evaluate<ti::c, ti::b, ti::I, ti::a>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 1>,
               index_list<S_second_index, S_first_index, R_index, R_index>>
      L_cIab = tenex::evaluate<ti::c, ti::I, ti::a, ti::b>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));
  const Tensor<DataType, Symmetry<3, 2, 1, 1>,
               index_list<S_second_index, S_first_index, R_index, R_index>>
      L_cIba = tenex::evaluate<ti::c, ti::I, ti::b, ti::a>(Rll(ti::a, ti::b) *
                                                           Sul(ti::I, ti::c));

  for (size_t a = 0; a < R_index::dim; a++) {
    for (size_t b = 0; b < R_index::dim; b++) {
      for (size_t i = 0; i < S_first_index::dim; i++) {
        for (size_t c = 0; c < S_second_index::dim; c++) {
          CHECK(L_abIc.get(a, b, i, c) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_abcI.get(a, b, c, i) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_aIbc.get(a, i, b, c) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_aIcb.get(a, i, c, b) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_acbI.get(a, c, b, i) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_acIb.get(a, c, i, b) == Rll.get(a, b) * Sul.get(i, c));

          CHECK(L_baIc.get(b, a, i, c) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_bacI.get(b, a, c, i) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_bIac.get(b, i, a, c) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_bIca.get(b, i, c, a) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_bcaI.get(b, c, a, i) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_bcIa.get(b, c, i, a) == Rll.get(a, b) * Sul.get(i, c));

          CHECK(L_Iabc.get(i, a, b, c) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_Iacb.get(i, a, c, b) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_Ibac.get(i, b, a, c) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_Ibca.get(i, b, c, a) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_Icab.get(i, c, a, b) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_Icba.get(i, c, b, a) == Rll.get(a, b) * Sul.get(i, c));

          CHECK(L_cabI.get(c, a, b, i) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_caIb.get(c, a, i, b) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_cbaI.get(c, b, a, i) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_cbIa.get(c, b, i, a) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_cIab.get(c, i, a, b) == Rll.get(a, b) * Sul.get(i, c));
          CHECK(L_cIba.get(c, i, b, a) == Rll.get(a, b) * Sul.get(i, c));
        }
      }
    }
  }
}

// \brief Test the outer product of a rank 0, rank 1, and rank 2 tensor is
// correctly evaluated
//
// \details
// The outer product cases tested are permutations of the form:
// - \f$L^{a}{}_{ib} = R * S^{a} * T_{bi}\f$
//
// Each case represents an ordering for the operands and the LHS indices.
//
// \tparam DataType the type of data being stored in the product operands
template <typename DataType>
void test_outer_product_rank_0x1x2_operands(const DataType& used_for_size) {
  Tensor<DataType> R{{{used_for_size}}};
  if constexpr (std::is_same_v<DataType, double>) {
    // Replace tensor's value from `used_for_size` to a proper test value
    R.get() = 4.5;
  } else {
    assign_unique_values_to_tensor(make_not_null(&R));
  }

  using S_index = SpacetimeIndex<3, UpLo::Up, Frame::Inertial>;
  using T_first_index = SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>;
  using T_second_index = SpatialIndex<4, UpLo::Lo, Frame::Inertial>;

  Tensor<DataType, Symmetry<1>, index_list<S_index>> Su(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Su));

  Tensor<DataType, Symmetry<2, 1>, index_list<T_first_index, T_second_index>>
      Tll(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Tll));

  // \f$R * S^{a} * T_{bi}\f$
  const auto R_SA_Tbi_expr = R() * Su(ti::A) * Tll(ti::b, ti::i);
  // \f$L^{a}{}_{bi} = R * S^{a} * T_{bi}\f$
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_first_index, T_second_index>>
      LAbi_from_R_SA_Tbi = tenex::evaluate<ti::A, ti::b, ti::i>(R_SA_Tbi_expr);
  // \f$L^{a}{}_{ib} = R * S^{a} * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_second_index, T_first_index>>
      LAib_from_R_SA_Tbi = tenex::evaluate<ti::A, ti::i, ti::b>(R_SA_Tbi_expr);
  // \f$L_{b}{}^{a}{}_{i} = R * S^{a} * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, S_index, T_second_index>>
      LbAi_from_R_SA_Tbi = tenex::evaluate<ti::b, ti::A, ti::i>(R_SA_Tbi_expr);
  // \f$L_{bi}{}^{a} = R * S^{a} * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, T_second_index, S_index>>
      LbiA_from_R_SA_Tbi = tenex::evaluate<ti::b, ti::i, ti::A>(R_SA_Tbi_expr);
  // \f$L_{i}{}^{a}{}_{b} = R * S^{a} * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, S_index, T_first_index>>
      LiAb_from_R_SA_Tbi = tenex::evaluate<ti::i, ti::A, ti::b>(R_SA_Tbi_expr);
  // \f$L_{ib}{}^{a} = R * S^{a} * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, T_first_index, S_index>>
      LibA_from_R_SA_Tbi = tenex::evaluate<ti::i, ti::b, ti::A>(R_SA_Tbi_expr);

  // \f$R * T_{bi} * S^{a}\f$
  const auto R_Tbi_SA_expr = R() * Tll(ti::b, ti::i) * Su(ti::A);
  // \f$L^{a}{}_{bi} = R * T_{bi} * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_first_index, T_second_index>>
      LAbi_from_R_Tbi_SA = tenex::evaluate<ti::A, ti::b, ti::i>(R_Tbi_SA_expr);
  // \f$L^{a}{}_{ib} = R * T_{bi} * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_second_index, T_first_index>>
      LAib_from_R_Tbi_SA = tenex::evaluate<ti::A, ti::i, ti::b>(R_Tbi_SA_expr);
  // \f$L_{b}{}^{a}{}_{i} = R * T_{bi} * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, S_index, T_second_index>>
      LbAi_from_R_Tbi_SA = tenex::evaluate<ti::b, ti::A, ti::i>(R_Tbi_SA_expr);
  // \f$L_{bi}{}^{a} = R * R * T_{bi} * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, T_second_index, S_index>>
      LbiA_from_R_Tbi_SA = tenex::evaluate<ti::b, ti::i, ti::A>(R_Tbi_SA_expr);
  // \f$L_{i}{}^{a}{}_{b} = R * T_{bi} * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, S_index, T_first_index>>
      LiAb_from_R_Tbi_SA = tenex::evaluate<ti::i, ti::A, ti::b>(R_Tbi_SA_expr);
  // \f$L_{ib}{}^{a} = R * T_{bi} * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, T_first_index, S_index>>
      LibA_from_R_Tbi_SA = tenex::evaluate<ti::i, ti::b, ti::A>(R_Tbi_SA_expr);

  // \f$S^{a} * R * T_{bi}\f$
  const auto SA_R_Tbi_expr = Su(ti::A) * R() * Tll(ti::b, ti::i);
  // \f$L^{a}{}_{bi} = S^{a} * R * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_first_index, T_second_index>>
      LAbi_from_SA_R_Tbi = tenex::evaluate<ti::A, ti::b, ti::i>(SA_R_Tbi_expr);
  // \f$L^{a}{}_{ib} = S^{a} * R * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_second_index, T_first_index>>
      LAib_from_SA_R_Tbi = tenex::evaluate<ti::A, ti::i, ti::b>(SA_R_Tbi_expr);
  // \f$L_{b}{}^{a}{}_{i} = S^{a} * R * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, S_index, T_second_index>>
      LbAi_from_SA_R_Tbi = tenex::evaluate<ti::b, ti::A, ti::i>(SA_R_Tbi_expr);
  // \f$L_{bi}{}^{a} = S^{a} * R * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, T_second_index, S_index>>
      LbiA_from_SA_R_Tbi = tenex::evaluate<ti::b, ti::i, ti::A>(SA_R_Tbi_expr);
  // \f$L_{i}{}^{a}{}_{b} = S^{a} * R * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, S_index, T_first_index>>
      LiAb_from_SA_R_Tbi = tenex::evaluate<ti::i, ti::A, ti::b>(SA_R_Tbi_expr);
  // \f$L_{ib}{}^{a} = S^{a} * R * T_{bi}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, T_first_index, S_index>>
      LibA_from_SA_R_Tbi = tenex::evaluate<ti::i, ti::b, ti::A>(SA_R_Tbi_expr);

  // \f$S^{a} * T_{bi} * R\f$
  const auto SA_Tbi_R_expr = Su(ti::A) * Tll(ti::b, ti::i) * R();
  // \f$L^{a}{}_{bi} = S^{a} * T_{bi} * R\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_first_index, T_second_index>>
      LAbi_from_SA_Tbi_R = tenex::evaluate<ti::A, ti::b, ti::i>(SA_Tbi_R_expr);
  // \f$L^{a}{}_{ib} = S^{a} * T_{bi} * R\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_second_index, T_first_index>>
      LAib_from_SA_Tbi_R = tenex::evaluate<ti::A, ti::i, ti::b>(SA_Tbi_R_expr);
  // \f$L_{b}{}^{a}{}_{i} = S^{a} * T_{bi} * R\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, S_index, T_second_index>>
      LbAi_from_SA_Tbi_R = tenex::evaluate<ti::b, ti::A, ti::i>(SA_Tbi_R_expr);
  // \f$L_{bi}{}^{a} = S^{a} * T_{bi} * R\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, T_second_index, S_index>>
      LbiA_from_SA_Tbi_R = tenex::evaluate<ti::b, ti::i, ti::A>(SA_Tbi_R_expr);
  // \f$L_{i}{}^{a}{}_{b} = S^{a} * T_{bi} * R\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, S_index, T_first_index>>
      LiAb_from_SA_Tbi_R = tenex::evaluate<ti::i, ti::A, ti::b>(SA_Tbi_R_expr);
  // \f$L_{ib}{}^{a} = S^{a} * T_{bi} * R\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, T_first_index, S_index>>
      LibA_from_SA_Tbi_R = tenex::evaluate<ti::i, ti::b, ti::A>(SA_Tbi_R_expr);

  // \f$T_{bi} * R * S^{a}\f$
  const auto Tbi_R_SA_expr = Tll(ti::b, ti::i) * R() * Su(ti::A);
  // \f$L^{a}{}_{bi} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_first_index, T_second_index>>
      LAbi_from_Tbi_R_SA = tenex::evaluate<ti::A, ti::b, ti::i>(Tbi_R_SA_expr);
  // \f$L^{a}{}_{ib} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_second_index, T_first_index>>
      LAib_from_Tbi_R_SA = tenex::evaluate<ti::A, ti::i, ti::b>(Tbi_R_SA_expr);
  // \f$L_{b}{}^{a}{}_{i} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, S_index, T_second_index>>
      LbAi_from_Tbi_R_SA = tenex::evaluate<ti::b, ti::A, ti::i>(Tbi_R_SA_expr);
  // \f$L_{bi}{}^{a} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, T_second_index, S_index>>
      LbiA_from_Tbi_R_SA = tenex::evaluate<ti::b, ti::i, ti::A>(Tbi_R_SA_expr);
  // \f$L_{i}{}^{a}{}_{b} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, S_index, T_first_index>>
      LiAb_from_Tbi_R_SA = tenex::evaluate<ti::i, ti::A, ti::b>(Tbi_R_SA_expr);
  // \f$L_{ib}{}^{a} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, T_first_index, S_index>>
      LibA_from_Tbi_R_SA = tenex::evaluate<ti::i, ti::b, ti::A>(Tbi_R_SA_expr);

  // \f$T_{bi} * S^{a} * R\f$
  const auto Tbi_SA_R_expr = Tll(ti::b, ti::i) * Su(ti::A) * R();
  // \f$L^{a}{}_{bi} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_first_index, T_second_index>>
      LAbi_from_Tbi_SA_R = tenex::evaluate<ti::A, ti::b, ti::i>(Tbi_SA_R_expr);
  // \f$L^{a}{}_{ib} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<S_index, T_second_index, T_first_index>>
      LAib_from_Tbi_SA_R = tenex::evaluate<ti::A, ti::i, ti::b>(Tbi_SA_R_expr);
  // \f$L_{b}{}^{a}{}_{i} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, S_index, T_second_index>>
      LbAi_from_Tbi_SA_R = tenex::evaluate<ti::b, ti::A, ti::i>(Tbi_SA_R_expr);
  // \f$L_{bi}{}^{a} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_first_index, T_second_index, S_index>>
      LbiA_from_Tbi_SA_R = tenex::evaluate<ti::b, ti::i, ti::A>(Tbi_SA_R_expr);
  // \f$L_{i}{}^{a}{}_{b} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, S_index, T_first_index>>
      LiAb_from_Tbi_SA_R = tenex::evaluate<ti::i, ti::A, ti::b>(Tbi_SA_R_expr);
  // \f$L_{ib}{}^{a} = T_{bi} * R * S^{a}\f$
  const Tensor<DataType, Symmetry<3, 2, 1>,
               index_list<T_second_index, T_first_index, S_index>>
      LibA_from_Tbi_SA_R = tenex::evaluate<ti::i, ti::b, ti::A>(Tbi_SA_R_expr);

  for (size_t a = 0; a < S_index::dim; a++) {
    for (size_t b = 0; b < T_first_index::dim; b++) {
      for (size_t i = 0; i < T_second_index::dim; i++) {
        const DataType expected_product = R.get() * Su.get(a) * Tll.get(b, i);

        CHECK(LAbi_from_R_SA_Tbi.get(a, b, i) == expected_product);
        CHECK(LAib_from_R_SA_Tbi.get(a, i, b) == expected_product);
        CHECK(LbAi_from_R_SA_Tbi.get(b, a, i) == expected_product);
        CHECK(LbiA_from_R_SA_Tbi.get(b, i, a) == expected_product);
        CHECK(LiAb_from_R_SA_Tbi.get(i, a, b) == expected_product);
        CHECK(LibA_from_R_SA_Tbi.get(i, b, a) == expected_product);

        CHECK(LAbi_from_R_Tbi_SA.get(a, b, i) == expected_product);
        CHECK(LAib_from_R_Tbi_SA.get(a, i, b) == expected_product);
        CHECK(LbAi_from_R_Tbi_SA.get(b, a, i) == expected_product);
        CHECK(LbiA_from_R_Tbi_SA.get(b, i, a) == expected_product);
        CHECK(LiAb_from_R_Tbi_SA.get(i, a, b) == expected_product);
        CHECK(LibA_from_R_Tbi_SA.get(i, b, a) == expected_product);

        CHECK(LAbi_from_SA_R_Tbi.get(a, b, i) == expected_product);
        CHECK(LAib_from_SA_R_Tbi.get(a, i, b) == expected_product);
        CHECK(LbAi_from_SA_R_Tbi.get(b, a, i) == expected_product);
        CHECK(LbiA_from_SA_R_Tbi.get(b, i, a) == expected_product);
        CHECK(LiAb_from_SA_R_Tbi.get(i, a, b) == expected_product);
        CHECK(LibA_from_SA_R_Tbi.get(i, b, a) == expected_product);

        CHECK(LAbi_from_SA_Tbi_R.get(a, b, i) == expected_product);
        CHECK(LAib_from_SA_Tbi_R.get(a, i, b) == expected_product);
        CHECK(LbAi_from_SA_Tbi_R.get(b, a, i) == expected_product);
        CHECK(LbiA_from_SA_Tbi_R.get(b, i, a) == expected_product);
        CHECK(LiAb_from_SA_Tbi_R.get(i, a, b) == expected_product);
        CHECK(LibA_from_SA_Tbi_R.get(i, b, a) == expected_product);

        CHECK(LAbi_from_Tbi_R_SA.get(a, b, i) == expected_product);
        CHECK(LAib_from_Tbi_R_SA.get(a, i, b) == expected_product);
        CHECK(LbAi_from_Tbi_R_SA.get(b, a, i) == expected_product);
        CHECK(LbiA_from_Tbi_R_SA.get(b, i, a) == expected_product);
        CHECK(LiAb_from_Tbi_R_SA.get(i, a, b) == expected_product);
        CHECK(LibA_from_Tbi_R_SA.get(i, b, a) == expected_product);

        CHECK(LAbi_from_Tbi_SA_R.get(a, b, i) == expected_product);
        CHECK(LAib_from_Tbi_SA_R.get(a, i, b) == expected_product);
        CHECK(LbAi_from_Tbi_SA_R.get(b, a, i) == expected_product);
        CHECK(LbiA_from_Tbi_SA_R.get(b, i, a) == expected_product);
        CHECK(LiAb_from_Tbi_SA_R.get(i, a, b) == expected_product);
        CHECK(LibA_from_Tbi_SA_R.get(i, b, a) == expected_product);
      }
    }
  }
}

// \brief Test the inner product of two rank 1 tensors is correctly evaluated
//
// \details
// The inner product cases tested are:
// - \f$L = R^{a} * S_{a}\f$
// - \f$L = S_{a} * R^{a}\f$
//
// \tparam DataType the type of data being stored in the product operands
template <typename DataType>
void test_inner_product_rank_1x1_operands(const DataType& used_for_size) {
  Tensor<DataType, Symmetry<1>,
         index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>
      Ru(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Ru));

  Tensor<DataType, Symmetry<1>,
         index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>
      Sl(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Sl));

  // \f$L = R^{a} * S_{a}\f$
  const Tensor<DataType> L_from_RA_Sa = tenex::evaluate(Ru(ti::A) * Sl(ti::a));
  // \f$L = S_{a} * R^{a}\f$
  const Tensor<DataType> L_from_Sa_RA = tenex::evaluate(Sl(ti::a) * Ru(ti::A));

  DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t a = 0; a < 4; a++) {
    expected_sum += (Ru.get(a) * Sl.get(a));
  }
  CHECK(L_from_RA_Sa.get() == expected_sum);
  CHECK(L_from_Sa_RA.get() == expected_sum);
}

// \brief Test the inner product of two rank 2 tensors is correctly evaluated
//
// \details
// All cases in this test contract both pairs of indices of the two rank 2
// tensor operands to a resulting rank 0 tensor. For each case, the two tensor
// operands have one spacetime and one spatial index. Each case is a permutation
// of the positions of contracted pairs and their valences. One such example
// case: \f$L = R_{ai} * S^{ai}\f$
//
// \tparam DataType the type of data being stored in the product operands
template <typename DataType>
void test_inner_product_rank_2x2_operands(const DataType& used_for_size) {
  using lower_spacetime_index = SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>;
  using upper_spacetime_index = SpacetimeIndex<3, UpLo::Up, Frame::Inertial>;
  using lower_spatial_index = SpatialIndex<2, UpLo::Lo, Frame::Inertial>;
  using upper_spatial_index = SpatialIndex<2, UpLo::Up, Frame::Inertial>;

  // All tensor variables starting with 'R' refer to tensors whose first index
  // is a spacetime index and whose second index is a spatial index. Conversely,
  // all tensor variables starting with 'S' refer to tensors whose first index
  // is a spatial index and whose second index is a spacetime index.
  Tensor<DataType, Symmetry<2, 1>,
         index_list<lower_spacetime_index, lower_spatial_index>>
      Rll(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Rll));
  Tensor<DataType, Symmetry<2, 1>,
         index_list<lower_spatial_index, lower_spacetime_index>>
      Sll(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Sll));
  Tensor<DataType, Symmetry<2, 1>,
         index_list<upper_spacetime_index, upper_spatial_index>>
      Ruu(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Ruu));
  Tensor<DataType, Symmetry<2, 1>,
         index_list<upper_spatial_index, upper_spacetime_index>>
      Suu(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Suu));
  Tensor<DataType, Symmetry<2, 1>,
         index_list<lower_spacetime_index, upper_spatial_index>>
      Rlu(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Rlu));
  Tensor<DataType, Symmetry<2, 1>,
         index_list<lower_spatial_index, upper_spacetime_index>>
      Slu(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Slu));
  Tensor<DataType, Symmetry<2, 1>,
         index_list<upper_spacetime_index, lower_spatial_index>>
      Rul(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Rul));
  Tensor<DataType, Symmetry<2, 1>,
         index_list<upper_spatial_index, lower_spacetime_index>>
      Sul(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Sul));

  // \f$L = Rll_{ai} * Ruu^{ai}\f$
  const Tensor<DataType> L_aiAI_product =
      tenex::evaluate(Rll(ti::a, ti::i) * Ruu(ti::A, ti::I));
  // \f$L = Rll_{ai} * Suu^{ia}\f$
  const Tensor<DataType> L_aiIA_product =
      tenex::evaluate(Rll(ti::a, ti::i) * Suu(ti::I, ti::A));
  // \f$L = Ruu^{ai} * Rll_{ai}\f$
  const Tensor<DataType> L_AIai_product =
      tenex::evaluate(Ruu(ti::A, ti::I) * Rll(ti::a, ti::i));
  // \f$L = Ruu^{ai} * Sll_{ia}\f$
  const Tensor<DataType> L_AIia_product =
      tenex::evaluate(Ruu(ti::A, ti::I) * Sll(ti::i, ti::a));
  // \f$L = Rlu_{a}{}^{i} * Rul^{a}{}_{i}\f$
  const Tensor<DataType> L_aIAi_product =
      tenex::evaluate(Rlu(ti::a, ti::I) * Rul(ti::A, ti::i));
  // \f$L = Rlu_{a}{}^{i} * Slu_{i}{}^{a}\f$
  const Tensor<DataType> L_aIiA_product =
      tenex::evaluate(Rlu(ti::a, ti::I) * Slu(ti::i, ti::A));
  // \f$L = Rul^{a}{}_{i} * Rlu_{a}{}^{i}\f$
  const Tensor<DataType> L_AiaI_product =
      tenex::evaluate(Rul(ti::A, ti::i) * Rlu(ti::a, ti::I));
  // \f$L = Rul^{a}{}_{i} * Sul^{i}{}_{a}\f$
  const Tensor<DataType> L_AiIa_product =
      tenex::evaluate(Rul(ti::A, ti::i) * Sul(ti::I, ti::a));

  DataType L_aiAI_expected_product =
      make_with_value<DataType>(used_for_size, 0.0);
  DataType L_aiIA_expected_product =
      make_with_value<DataType>(used_for_size, 0.0);
  DataType L_AIai_expected_product =
      make_with_value<DataType>(used_for_size, 0.0);
  DataType L_AIia_expected_product =
      make_with_value<DataType>(used_for_size, 0.0);
  DataType L_aIAi_expected_product =
      make_with_value<DataType>(used_for_size, 0.0);
  DataType L_aIiA_expected_product =
      make_with_value<DataType>(used_for_size, 0.0);
  DataType L_AiaI_expected_product =
      make_with_value<DataType>(used_for_size, 0.0);
  DataType L_AiIa_expected_product =
      make_with_value<DataType>(used_for_size, 0.0);

  for (size_t a = 0; a < lower_spacetime_index::dim; a++) {
    for (size_t i = 0; i < lower_spatial_index::dim; i++) {
      L_aiAI_expected_product += (Rll.get(a, i) * Ruu.get(a, i));
      L_aiIA_expected_product += (Rll.get(a, i) * Suu.get(i, a));
      L_AIai_expected_product += (Ruu.get(a, i) * Rll.get(a, i));
      L_AIia_expected_product += (Ruu.get(a, i) * Sll.get(i, a));
      L_aIAi_expected_product += (Rlu.get(a, i) * Rul.get(a, i));
      L_aIiA_expected_product += (Rlu.get(a, i) * Slu.get(i, a));
      L_AiaI_expected_product += (Rul.get(a, i) * Rlu.get(a, i));
      L_AiIa_expected_product += (Rul.get(a, i) * Sul.get(i, a));
    }
  }
  CHECK(L_aiAI_product.get() == L_aiAI_expected_product);
  CHECK(L_aiIA_product.get() == L_aiIA_expected_product);
  CHECK(L_AIai_product.get() == L_AIai_expected_product);
  CHECK(L_AIia_product.get() == L_AIia_expected_product);
  CHECK(L_aIAi_product.get() == L_aIAi_expected_product);
  CHECK(L_aIiA_product.get() == L_aIiA_expected_product);
  CHECK(L_AiaI_product.get() == L_AiaI_expected_product);
  CHECK(L_AiIa_product.get() == L_AiIa_expected_product);
}

// \brief Test the product of two tensors with one pair of indices to contract
// is correctly evaluated
//
// \details
// The product cases tested are:
// - \f$L_{b} = R_{ab} * T^{a}\f$
// - \f$L_{ac} = R_{ab} * S^{b}_{c}\f$
//
// All cases in this test contract one pair of indices of the two tensor
// operands. Each case is a permutation of the position of the contracted pair
// and the ordering of the LHS indices.
//
// \tparam DataType the type of data being stored in the product operands
template <typename DataType>
void test_two_term_inner_outer_product(const DataType& used_for_size) {
  using R_index = SpacetimeIndex<3, UpLo::Lo, Frame::Grid>;
  using T_index = SpacetimeIndex<3, UpLo::Up, Frame::Grid>;

  Tensor<DataType, Symmetry<1, 1>, index_list<R_index, R_index>> Rll(
      used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Rll));
  Tensor<DataType, Symmetry<1>, index_list<T_index>> Tu(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Tu));

  // \f$L_{b} = R_{ab} * T^{a}\f$
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  using Lb = Tensor<DataType, Symmetry<1>, index_list<R_index>>;
  const Lb Lb_from_Rab_TA =
      tenex::evaluate<ti::b>(Rll(ti::a, ti::b) * Tu(ti::A));
  // \f$L_{b} = R_{ba} * T^{a}\f$
  const Lb Lb_from_Rba_TA =
      tenex::evaluate<ti::b>(Rll(ti::b, ti::a) * Tu(ti::A));
  // \f$L_{b} = T^{a} * R_{ab}\f$
  const Lb Lb_from_TA_Rab =
      tenex::evaluate<ti::b>(Tu(ti::A) * Rll(ti::a, ti::b));
  // \f$L_{b} = T^{a} * R_{ba}\f$
  const Lb Lb_from_TA_Rba =
      tenex::evaluate<ti::b>(Tu(ti::A) * Rll(ti::b, ti::a));

  for (size_t b = 0; b < R_index::dim; b++) {
    DataType expected_product = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t a = 0; a < T_index::dim; a++) {
      expected_product += (Rll.get(a, b) * Tu.get(a));
    }
    CHECK(Lb_from_Rab_TA.get(b) == expected_product);
    CHECK(Lb_from_Rba_TA.get(b) == expected_product);
    CHECK(Lb_from_TA_Rab.get(b) == expected_product);
    CHECK(Lb_from_TA_Rba.get(b) == expected_product);
  }

  using S_lower_index = SpacetimeIndex<2, UpLo::Lo, Frame::Grid>;
  using S_upper_index = SpacetimeIndex<3, UpLo::Up, Frame::Grid>;

  Tensor<DataType, Symmetry<2, 1>, index_list<S_upper_index, S_lower_index>>
      Sul(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Sul));
  Tensor<DataType, Symmetry<2, 1>, index_list<S_lower_index, S_upper_index>>
      Slu(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Slu));

  // \f$L_{ac} = R_{ab} * S^{b}_{c}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<R_index, S_lower_index>>
      L_abBc_to_ac =
          tenex::evaluate<ti::a, ti::c>(Rll(ti::a, ti::b) * Sul(ti::B, ti::c));
  // \f$L_{ca} = R_{ab} * S^{b}_{c}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<S_lower_index, R_index>>
      L_abBc_to_ca =
          tenex::evaluate<ti::c, ti::a>(Rll(ti::a, ti::b) * Sul(ti::B, ti::c));
  // \f$L_{ac} = R_{ab} * S_{c}^{b}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<R_index, S_lower_index>>
      L_abcB_to_ac =
          tenex::evaluate<ti::a, ti::c>(Rll(ti::a, ti::b) * Slu(ti::c, ti::B));
  // \f$L_{ca} = R_{ab} * S_{c}^{b}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<S_lower_index, R_index>>
      L_abcB_to_ca =
          tenex::evaluate<ti::c, ti::a>(Rll(ti::a, ti::b) * Slu(ti::c, ti::B));
  // \f$L_{ac} = R_{ba} * S^{b}_{c}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<R_index, S_lower_index>>
      L_baBc_to_ac =
          tenex::evaluate<ti::a, ti::c>(Rll(ti::b, ti::a) * Sul(ti::B, ti::c));
  // \f$L_{ca} = R_{ba} * S^{b}_{c}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<S_lower_index, R_index>>
      L_baBc_to_ca =
          tenex::evaluate<ti::c, ti::a>(Rll(ti::b, ti::a) * Sul(ti::B, ti::c));
  // \f$L_{ac} = R_{ba} * S_{c}^{b}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<R_index, S_lower_index>>
      L_bacB_to_ac =
          tenex::evaluate<ti::a, ti::c>(Rll(ti::b, ti::a) * Slu(ti::c, ti::B));
  // \f$L_{ca} = R_{ba} * S_{c}^{b}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<S_lower_index, R_index>>
      L_bacB_to_ca =
          tenex::evaluate<ti::c, ti::a>(Rll(ti::b, ti::a) * Slu(ti::c, ti::B));

  for (size_t a = 0; a < R_index::dim; a++) {
    for (size_t c = 0; c < S_lower_index::dim; c++) {
      DataType L_abBc_expected_product =
          make_with_value<DataType>(used_for_size, 0.0);
      DataType L_abcB_expected_product =
          make_with_value<DataType>(used_for_size, 0.0);
      DataType L_baBc_expected_product =
          make_with_value<DataType>(used_for_size, 0.0);
      DataType L_bacB_expected_product =
          make_with_value<DataType>(used_for_size, 0.0);
      for (size_t b = 0; b < 4; b++) {
        L_abBc_expected_product += (Rll.get(a, b) * Sul.get(b, c));
        L_abcB_expected_product += (Rll.get(a, b) * Slu.get(c, b));
        L_baBc_expected_product += (Rll.get(b, a) * Sul.get(b, c));
        L_bacB_expected_product += (Rll.get(b, a) * Slu.get(c, b));
      }
      CHECK(L_abBc_to_ac.get(a, c) == L_abBc_expected_product);
      CHECK(L_abBc_to_ca.get(c, a) == L_abBc_expected_product);
      CHECK(L_abcB_to_ac.get(a, c) == L_abcB_expected_product);
      CHECK(L_abcB_to_ca.get(c, a) == L_abcB_expected_product);
      CHECK(L_baBc_to_ac.get(a, c) == L_baBc_expected_product);
      CHECK(L_baBc_to_ca.get(c, a) == L_baBc_expected_product);
      CHECK(L_bacB_to_ac.get(a, c) == L_bacB_expected_product);
      CHECK(L_bacB_to_ca.get(c, a) == L_bacB_expected_product);
    }
  }
}

// \brief Test the product of three tensors involving both inner and outer
// products of indices is correctly evaluated
//
// \details
// The product cases tested are:
// - \f$L_{i} = R^{j} * S_{j} * T_{i}\f$
// - \f$L_{i}{}^{k} = S_{j} * T_{i} * G^{jk}\f$
//
// For each case, multiple operand orderings are tested. For the second case,
// both LHS index orderings are also tested.
//
// \tparam DataType the type of data being stored in the product operands
template <typename DataType>
void test_three_term_inner_outer_product(const DataType& used_for_size) {
  Tensor<DataType, Symmetry<1>,
         index_list<SpatialIndex<3, UpLo::Up, Frame::Inertial>>>
      Ru(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Ru));
  Tensor<DataType, Symmetry<1>,
         index_list<SpatialIndex<3, UpLo::Lo, Frame::Inertial>>>
      Sl(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Sl));
  Tensor<DataType, Symmetry<1>,
         index_list<SpatialIndex<3, UpLo::Lo, Frame::Inertial>>>
      Tl(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Tl));

  // \f$L_{i} = R^{j} * S_{j} * T_{i}\f$
  const decltype(Tl) Li_from_Jji =
      tenex::evaluate<ti::i>(Ru(ti::J) * Sl(ti::j) * Tl(ti::i));
  // \f$L_{i} = R^{j} * T_{i} * S_{j}\f$
  const decltype(Tl) Li_from_Jij =
      tenex::evaluate<ti::i>(Ru(ti::J) * Tl(ti::i) * Sl(ti::j));
  // \f$L_{i} = T_{i} * S_{j} * R^{j}\f$
  const decltype(Tl) Li_from_ijJ =
      tenex::evaluate<ti::i>(Tl(ti::i) * Sl(ti::j) * Ru(ti::J));

  for (size_t i = 0; i < 3; i++) {
    DataType expected_product = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t j = 0; j < 3; j++) {
      expected_product += (Ru.get(j) * Sl.get(j) * Tl.get(i));
    }
    CHECK(Li_from_Jji.get(i) == expected_product);
    CHECK(Li_from_Jij.get(i) == expected_product);
    CHECK(Li_from_ijJ.get(i) == expected_product);
  }

  using T_index = tmpl::front<typename decltype(Tl)::index_list>;
  using G_index = SpatialIndex<3, UpLo::Up, Frame::Inertial>;

  Tensor<DataType, Symmetry<2, 1>, index_list<G_index, G_index>> Guu(
      used_for_size);
  assign_unique_values_to_tensor(make_not_null(&Guu));

  // \f$L_{i}{}^{k} = S_{j} * T_{i} * G^{jk}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<T_index, G_index>>
      LiK_from_Sj_Ti_GJK = tenex::evaluate<ti::i, ti::K>(Sl(ti::j) * Tl(ti::i) *
                                                         Guu(ti::J, ti::K));
  // \f$L^{k}{}_{i} = S_{j} * T_{i} * G^{jk}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<G_index, T_index>>
      LKi_from_Sj_Ti_GJK = tenex::evaluate<ti::K, ti::i>(Sl(ti::j) * Tl(ti::i) *
                                                         Guu(ti::J, ti::K));
  // \f$L_{i}{}^{k} = S_{j} *  G^{jk} * T_{i}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<T_index, G_index>>
      LiK_from_Sj_GJK_Ti = tenex::evaluate<ti::i, ti::K>(
          Sl(ti::j) * Guu(ti::J, ti::K) * Tl(ti::i));
  // \f$L^{k}{}_{i} = S_{j} *  G^{jk} * T_{i}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<G_index, T_index>>
      LKi_from_Sj_GJK_Ti = tenex::evaluate<ti::K, ti::i>(
          Sl(ti::j) * Guu(ti::J, ti::K) * Tl(ti::i));
  // \f$L_{i}{}^{k} = T_{i} * S_{j} * G^{jk}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<T_index, G_index>>
      LiK_from_Ti_Sj_GJK = tenex::evaluate<ti::i, ti::K>(Tl(ti::i) * Sl(ti::j) *
                                                         Guu(ti::J, ti::K));
  // \f$L^{k}{}_{i} = T_{i} * S_{j} * G^{jk}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<G_index, T_index>>
      LKi_from_Ti_Sj_GJK = tenex::evaluate<ti::K, ti::i>(Tl(ti::i) * Sl(ti::j) *
                                                         Guu(ti::J, ti::K));
  // \f$L_{i}{}^{k} = T_{i} * G^{jk} * S_{j}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<T_index, G_index>>
      LiK_from_Ti_GJK_Sj = tenex::evaluate<ti::i, ti::K>(
          Tl(ti::i) * Guu(ti::J, ti::K) * Sl(ti::j));
  // \f$L^{k}{}_{i} = T_{i} * G^{jk} * S_{j}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<G_index, T_index>>
      LKi_from_Ti_GJK_Sj = tenex::evaluate<ti::K, ti::i>(
          Tl(ti::i) * Guu(ti::J, ti::K) * Sl(ti::j));
  // \f$L_{i}{}^{k} = G^{jk} * S_{j} * T_{i}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<T_index, G_index>>
      LiK_from_GJK_Sj_Ti = tenex::evaluate<ti::i, ti::K>(Guu(ti::J, ti::K) *
                                                         Sl(ti::j) * Tl(ti::i));
  // \f$L^{k}{}_{i} = G^{jk} * S_{j} * T_{i}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<G_index, T_index>>
      LKi_from_GJK_Sj_Ti = tenex::evaluate<ti::K, ti::i>(Guu(ti::J, ti::K) *
                                                         Sl(ti::j) * Tl(ti::i));
  // \f$L_{i}{}^{k} = G^{jk} * T_{i} * S_{j}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<T_index, G_index>>
      LiK_from_GJK_Ti_Sj = tenex::evaluate<ti::i, ti::K>(Guu(ti::J, ti::K) *
                                                         Tl(ti::i) * Sl(ti::j));
  // \f$L^{k}{}_{i} = G^{jk} * T_{i} * S_{j}\f$
  const Tensor<DataType, Symmetry<2, 1>, index_list<G_index, T_index>>
      LKi_from_GJK_Ti_Sj = tenex::evaluate<ti::K, ti::i>(Guu(ti::J, ti::K) *
                                                         Tl(ti::i) * Sl(ti::j));

  for (size_t k = 0; k < G_index::dim; k++) {
    for (size_t i = 0; i < T_index::dim; i++) {
      DataType expected_product =
          make_with_value<DataType>(used_for_size, 0.0);
      for (size_t j = 0; j < G_index::dim; j++) {
        expected_product += (Sl.get(j) * Tl.get(i) * Guu.get(j, k));
      }
      CHECK(LiK_from_Sj_Ti_GJK.get(i, k) == expected_product);
      CHECK(LKi_from_Sj_Ti_GJK.get(k, i) == expected_product);
      CHECK(LiK_from_Sj_GJK_Ti.get(i, k) == expected_product);
      CHECK(LKi_from_Sj_GJK_Ti.get(k, i) == expected_product);
      CHECK(LiK_from_Ti_Sj_GJK.get(i, k) == expected_product);
      CHECK(LKi_from_Ti_Sj_GJK.get(k, i) == expected_product);
      CHECK(LiK_from_Ti_GJK_Sj.get(i, k) == expected_product);
      CHECK(LKi_from_Ti_GJK_Sj.get(k, i) == expected_product);
      CHECK(LiK_from_GJK_Sj_Ti.get(i, k) == expected_product);
      CHECK(LKi_from_GJK_Sj_Ti.get(k, i) == expected_product);
      CHECK(LiK_from_GJK_Ti_Sj.get(i, k) == expected_product);
      CHECK(LKi_from_GJK_Ti_Sj.get(k, i) == expected_product);
    }
  }
}

// \brief Test the products of tensors where generic spatial indices are used
// for spacetime indices
//
// \details
// The product cases tested are:
// - \f$L = R^{i} * S_{i}\f$
// - \f$L = R^{k} * T_{k}\f$
// - \f$L_{j, a, i}{}^{b} = G_{i, a} * H_{j}{}^{b}\f$
// - \f$L_{k, i} = H_{i}{}^{j} * G_{k, j}\f$
//
// where \f$R\f$'s index is spacetime, \f$S\f$' index is spatial, \f$T\f$'s
// index is spacetime, \f$G\f$'s indices are spacetime, and \f$H\f$'s first
// index is spatial and second is spacetime.
//
// \tparam DataType the type of data being stored in the tensor operand of the
// products
template <typename DataType>
void test_spatial_spacetime_index(const DataType& used_for_size) {
  std::uniform_real_distribution<> distribution(0.1, 1.0);

  Tensor<DataType, Symmetry<1>,
         index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>
      R(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&R));
  Tensor<DataType, Symmetry<1>,
         index_list<SpatialIndex<3, UpLo::Lo, Frame::Grid>>>
      S(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&S));
  Tensor<DataType, Symmetry<1>,
         index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>
      T(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&T));

  // \f$L = R^{i} * S_{i}\f$
  // (spatial) * (spacetime) inner product with generic spatial indices
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const Tensor<DataType> RS = tenex::evaluate(R(ti::I) * S(ti::i));
  // \f$L = R^{k} * T_{k}\f$
  // (spacetime) * (spacetime) inner product with generic spatial indices
  const Tensor<DataType> RT = tenex::evaluate(R(ti::K) * T(ti::k));

  DataType expected_RS_product = make_with_value<DataType>(used_for_size, 0.0);
  DataType expected_RT_product = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t i = 0; i < 3; i++) {
    expected_RS_product += R.get(i + 1) * S.get(i);
    expected_RT_product += R.get(i + 1) * T.get(i + 1);
  }
  CHECK_ITERABLE_APPROX(RS.get(), expected_RS_product);
  CHECK_ITERABLE_APPROX(RT.get(), expected_RT_product);

  Tensor<DataType, Symmetry<1, 1>,
         index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                    SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>>>
      G(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&G));
  Tensor<DataType, Symmetry<2, 1>,
         index_list<SpatialIndex<3, UpLo::Lo, Frame::Inertial>,
                    SpacetimeIndex<3, UpLo::Up, Frame::Inertial>>>
      H(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&H));

  // \f$L_{j, a, i}{}^{b} = G_{i, a} * H_{j}{}^{b}\f$
  // rank 2 x rank 2 outer product containing a spacetime index with a generic
  // spatial index
  const Tensor<DataType, Symmetry<4, 3, 2, 1>,
               index_list<SpatialIndex<3, UpLo::Lo, Frame::Inertial>,
                          SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                          SpatialIndex<3, UpLo::Lo, Frame::Inertial>,
                          SpacetimeIndex<3, UpLo::Up, Frame::Inertial>>>
      GH = tenex::evaluate<ti::j, ti::a, ti::i, ti::B>(G(ti::i, ti::a) *
                                                       H(ti::j, ti::B));
  for (size_t j = 0; j < 3; j++) {
    for (size_t a = 0; a < 4; a++) {
      for (size_t i = 0; i < 3; i++) {
        for (size_t b = 0; b < 4; b++) {
          CHECK_ITERABLE_APPROX(GH.get(j, a, i, b),
                                G.get(i + 1, a) * H.get(j, b));
        }
      }
    }
  }

  // \f$L_{k, i} = H_{i}{}^{j} * G_{k, j}\f$
  // inner and outer product in one expression using generic spatial indices
  // for spacetime indices
  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpatialIndex<3, UpLo::Lo, Frame::Inertial>,
                          SpatialIndex<3, UpLo::Lo, Frame::Inertial>>>
      HG = tenex::evaluate<ti::k, ti::i>(H(ti::i, ti::J) * G(ti::k, ti::j));
  for (size_t k = 0; k < 3; k++) {
    for (size_t i = 0; i < 3; i++) {
      DataType expected_product = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t j = 0; j < 3; j++) {
        expected_product += H.get(i, j + 1) * G.get(k + 1, j + 1);
      }
      CHECK_ITERABLE_APPROX(HG.get(k, i), expected_product);
    }
  }
}

// \brief Test the products of tensors where concrete time indices are used for
// spacetime indices
//
// \details
// The product cases tested are:
// - \f$L = R_{t} * S * R_{t}\f$
// - \f$L^{b} = G_{t}{}^{a} * H_{a}{}^{tb}\f$
// - \f$L^{T}{}_{ba} = R_{a} * R_{b}\f$
// - \f$L_^{ct} = G_{t}{}^{b} * R_{b} * H_{t}^{ta}\f$
//
// \tparam DataType the type of data being stored in the tensor operand of the
// products
template <typename DataType>
void test_time_index(const DataType& used_for_size) {
  Tensor<DataType, Symmetry<1>,
         index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>
      R(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&R));

  Tensor<DataType> S{{{used_for_size}}};
  if constexpr (std::is_same_v<DataType, double>) {
    // Replace tensor's value from `used_for_size` to a proper test value
    S.get() = -2.2;
  } else {
    assign_unique_values_to_tensor(make_not_null(&S));
  }

  // \f$L = R_{t} * S * R_{t}\f$
  const Tensor<DataType> L = tenex::evaluate(R(ti::t) * S() * R(ti::t));
  CHECK(L.get() == R.get(0) * S.get() * R.get(0));

  Tensor<DataType, Symmetry<2, 1>,
         index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                    SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>
      G(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&G));

  Tensor<DataType, Symmetry<3, 2, 1>,
         index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                    SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                    SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>
      H(used_for_size);
  assign_unique_values_to_tensor(make_not_null(&H));

  // \f$L^{b} = G_{t}{}^{a} * H_{a}{}^{tb}\f$
  const Tensor<DataType, Symmetry<1>,
               index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>
      L_B = tenex::evaluate<ti::B>(G(ti::t, ti::A) * H(ti::a, ti::T, ti::B));

  for (size_t b = 0; b < 4; b++) {
    DataType expected_product = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t a = 0; a < 4; a++) {
      expected_product += G.get(0, a) * H.get(a, 0, b);
    }
    CHECK_ITERABLE_APPROX(L_B.get(b), expected_product);
  }

  // Assign a placeholder to the LHS tensor's components before it is computed
  // so that when test expressions below only compute time components, we can
  // check that LHS spatial components haven't changed
  const double spatial_component_placeholder =
      std::numeric_limits<double>::max();
  auto L_Tba = make_with_value<
      Tensor<DataType, Symmetry<2, 1, 1>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>>(
      used_for_size, spatial_component_placeholder);
  // \f$L^{T}{}_{ba} = R_{a} * R_{b}\f$
  tenex::evaluate<ti::T, ti::b, ti::a>(make_not_null(&L_Tba),
                                       R(ti::a) * R(ti::b));

  for (size_t b = 0; b < 4; b++) {
    for (size_t a = 0; a < 4; a++) {
      CHECK_ITERABLE_APPROX(L_Tba.get(0, b, a), R.get(a) * R.get(b));
      for (size_t i = 0; i < 3; i++) {
        CHECK(L_Tba.get(i + 1, b, a) == spatial_component_placeholder);
      }
    }
  }

  auto L_CT = make_with_value<
      Tensor<DataType, Symmetry<2, 1>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>>(
      used_for_size, spatial_component_placeholder);
  // \f$L_^{ct} = G_{t}{}^{b} * R_{b} * H_{t}^{ta}\f$
  tenex::evaluate<ti::C, ti::T>(
      make_not_null(&L_CT),
      G(ti::t, ti::B) * H(ti::b, ti::A, ti::C) * G(ti::a, ti::T));

  for (size_t c = 0; c < 4; c++) {
    DataType expected_product = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t b = 0; b < 4; b++) {
      for (size_t a = 0; a < 4; a++) {
        expected_product += G.get(0, b) * H.get(b, a, c) * G.get(a, 0);
      }
    }
    CHECK_ITERABLE_APPROX(L_CT.get(c, 0), expected_product);

    for (size_t i = 0; i < 3; i++) {
      CHECK(L_CT.get(c, i + 1) == spatial_component_placeholder);
    }
  }
}

template <typename DataType>
void test_products(const DataType& used_for_size) {
  // Test evaluation of outer products
  test_outer_product_double(used_for_size);
  test_outer_product_rank_0_operand(used_for_size);
  test_outer_product_rank_1_operand(used_for_size);
  test_outer_product_rank_2x2_operands(used_for_size);
  test_outer_product_rank_0x1x2_operands(used_for_size);

  // Test evaluation of inner products
  test_inner_product_rank_1x1_operands(used_for_size);
  test_inner_product_rank_2x2_operands(used_for_size);

  // Test evaluation of expressions involving both inner and outer products
  test_two_term_inner_outer_product(used_for_size);
  test_three_term_inner_outer_product(used_for_size);

  // Test product expressions where generic spatial indices are used for
  // spacetime indices
  test_spatial_spacetime_index(used_for_size);

  // Test product expressions where time indices are used for spacetime indices
  test_time_index(used_for_size);
}
}  // namespace

SPECTRE_TEST_CASE("Unit.DataStructures.Tensor.Expression.Product",
                  "[DataStructures][Unit]") {
  test_products(std::numeric_limits<double>::signaling_NaN());
  test_products(DataVector(5, std::numeric_limits<double>::signaling_NaN()));
}
