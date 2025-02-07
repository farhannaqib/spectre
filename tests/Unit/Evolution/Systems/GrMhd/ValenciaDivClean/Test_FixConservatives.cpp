// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <cmath>
#include <cstddef>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/FixConservatives.hpp"
#include "Framework/TestCreation.hpp"
#include "Framework/TestHelpers.hpp"
#include "PointwiseFunctions/Hydro/MagneticFieldTreatment.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/MakeWithValue.hpp"

namespace {

void test_variable_fixer(
    const grmhd::ValenciaDivClean::FixConservatives& variable_fixer,
    const bool enable) {
  // Call variable fixer at five points
  // [0]:  tilde_d is too small, should be raised to limit
  // [1]:  tilde_ye is too small, should be raised to limit
  // [2]:  tilde_tau is too small, raise to level of needed, which also
  //       causes tilde_s to be zeroed
  // [3]:  tilde_S is too big, so it is lowered
  // [4]:  all values are good, no changes

  Scalar<DataVector> tilde_d{DataVector{2.e-12, 1.0, 1.0, 1.0, 1.0}};
  // We assume that ye = 0.1
  Scalar<DataVector> tilde_ye{DataVector{2.e-13, 2.0e-10, 1.0, 1.0, 1.0}};
  Scalar<DataVector> tilde_tau{DataVector{4.5, 4.5, 1.5, 4.5, 4.5}};
  auto tilde_s =
      make_with_value<tnsr::i<DataVector, 3, Frame::Inertial>>(tilde_d, 0.0);
  auto tilde_b =
      make_with_value<tnsr::I<DataVector, 3, Frame::Inertial>>(tilde_d, 0.0);
  for (size_t d = 0; d < 3; ++d) {
    tilde_s.get(0) = DataVector{3.0, 3.0, 0.0, 6.0, 5.0};
    tilde_b.get(1) = DataVector{2.0, 2.0, 2.0, 2.0, 2.0};
  }

  auto expected_tilde_d = tilde_d;
  if (enable) {
    get(expected_tilde_d)[0] = 1.e-12;
  }

  auto expected_tilde_ye = tilde_ye;
  if (enable) {
    get(expected_tilde_ye)[0] = 1.e-13;  // since Y_e = 0.1
    get(expected_tilde_ye)[1] = 1.e-10;
  }

  auto expected_tilde_tau = tilde_tau;
  if (enable) {
    get(expected_tilde_tau)[2] = 2.0;
  }

  auto expected_tilde_s = tilde_s;
  if (enable) {
    expected_tilde_s.get(0)[3] = sqrt(27.0);
  }

  auto spatial_metric =
      make_with_value<tnsr::ii<DataVector, 3, Frame::Inertial>>(tilde_d, 0.0);
  auto inv_spatial_metric =
      make_with_value<tnsr::II<DataVector, 3, Frame::Inertial>>(tilde_d, 0.0);
  auto sqrt_det_spatial_metric =
      make_with_value<Scalar<DataVector>>(tilde_d, 1.0);
  for (size_t d = 0; d < 3; ++d) {
    spatial_metric.get(d, d) = get(sqrt_det_spatial_metric);
    inv_spatial_metric.get(d, d) = get(sqrt_det_spatial_metric);
  }

  CHECK(enable == variable_fixer(&tilde_d, &tilde_ye, &tilde_tau, &tilde_s,
                                 tilde_b, spatial_metric, inv_spatial_metric,
                                 sqrt_det_spatial_metric));

  CHECK_ITERABLE_APPROX(tilde_d, expected_tilde_d);
  CHECK_ITERABLE_APPROX(tilde_ye, expected_tilde_ye);
  CHECK_ITERABLE_APPROX(tilde_tau, expected_tilde_tau);
  CHECK_ITERABLE_APPROX(tilde_s, expected_tilde_s);
}

void run_benchmark(const bool enable) {
  if (not enable) {
    return;
  }
  const size_t num_points = 512;
  Scalar<DataVector> tilde_d{DataVector(num_points)};
  Scalar<DataVector> tilde_ye{DataVector(num_points)};
  Scalar<DataVector> tilde_tau{DataVector(num_points)};
  auto tilde_s =
      make_with_value<tnsr::i<DataVector, 3, Frame::Inertial>>(tilde_d, 0.0);
  auto tilde_b =
      make_with_value<tnsr::I<DataVector, 3, Frame::Inertial>>(tilde_d, 0.0);
  for (size_t i = 0; i < num_points; ++i) {
    if (i % 2 == 0) {
      // tilde_d is too small, should be raised to limit
      get(tilde_d)[i] = 2.e-12;
      get(tilde_ye)[i] = 2.e-13;
      get(tilde_tau)[i] = 4.5;
      tilde_s.get(0)[i] = 3.0;
      tilde_b.get(0)[i] = 2.0e-6;
    } else {
      // tilde_tau is too small, raise to level of needed, which also
      // causes tilde_s to be zeroed
      get(tilde_d)[i] = 1.0;
      get(tilde_ye)[i] = 1.0;
      get(tilde_tau)[i] = 1.5;
      tilde_s.get(0)[i] = 0.0;
      tilde_b.get(0)[i] = 2.0;
    }
  }
  auto spatial_metric =
      make_with_value<tnsr::ii<DataVector, 3, Frame::Inertial>>(tilde_d, 0.0);
  auto inv_spatial_metric =
      make_with_value<tnsr::II<DataVector, 3, Frame::Inertial>>(tilde_d, 0.0);
  auto sqrt_det_spatial_metric =
      make_with_value<Scalar<DataVector>>(tilde_d, 1.0);
  for (size_t d = 0; d < 3; ++d) {
    spatial_metric.get(d, d) = get(sqrt_det_spatial_metric);
    inv_spatial_metric.get(d, d) = get(sqrt_det_spatial_metric);
  }

  const grmhd::ValenciaDivClean::FixConservatives variable_fixer{
      1.e-12,  1.0e-11,
      1.0e-10, 1.0e-9,
      0.0,     0.0,
      1.0e-8,  0.0001,
      true,    hydro::MagneticFieldTreatment::AssumeNonZero};

  BENCHMARK("FixConservatives") {
    // Note: Benchmarking and perf indicates that software prefetching is likely
    // the next big gain for conservative variable fixing.
    variable_fixer(&tilde_d, &tilde_ye, &tilde_tau, &tilde_s, tilde_b,
                   spatial_metric, inv_spatial_metric, sqrt_det_spatial_metric);
  };
}
}  // namespace

SPECTRE_TEST_CASE("Unit.Evolution.GrMhd.ValenciaDivClean.FixConservatives",
                  "[VariableFixing][Unit]") {
  for (const bool enable : {true, false}) {
    grmhd::ValenciaDivClean::FixConservatives variable_fixer{
        1.e-12,  1.0e-11,
        1.0e-10, 1.0e-9,
        0.0,     0.0,
        1.e-12,  0.0,
        enable,  hydro::MagneticFieldTreatment::AssumeNonZero};
    test_variable_fixer(serialize_and_deserialize(variable_fixer), enable);
    test_serialization(variable_fixer);
  }

  const auto fixer_from_options =
      TestHelpers::test_creation<grmhd::ValenciaDivClean::FixConservatives>(
          "MinimumValueOfD: 1.0e-12\n"
          "CutoffD: 1.0e-11\n"
          "MinimumValueOfYe: 1.0e-10\n"
          "CutoffYe: 1.0e-9\n"
          "SafetyFactorForB: 0.0\n"
          "SafetyFactorForS: 0.0\n"
          "SafetyFactorForSCutoffD: 1.0e-12\n"
          "SafetyFactorForSSlope: 0.0\n"
          "Enable: true\n"
          "MagneticField: AssumeNonZero\n");
  test_variable_fixer(fixer_from_options, true);

  run_benchmark(false);
}
