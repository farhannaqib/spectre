# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY XctsPointwiseFunctions)

add_spectre_library(${LIBRARY})

spectre_target_sources(
  ${LIBRARY}
  PRIVATE
  AdmLinearMomentum.cpp
  AdmMass.cpp
  CenterOfMass.cpp
  ExtrinsicCurvature.cpp
  LongitudinalOperator.cpp
  SpacetimeQuantities.cpp
  )

spectre_target_headers(
  ${LIBRARY}
  INCLUDE_DIRECTORY ${CMAKE_SOURCE_DIR}/src
  HEADERS
  AdmLinearMomentum.hpp
  AdmMass.hpp
  CenterOfMass.hpp
  ExtrinsicCurvature.hpp
  LongitudinalOperator.hpp
  SpacetimeQuantities.hpp
  )

target_link_libraries(
  ${LIBRARY}
  PUBLIC
  DataStructures
  GeneralRelativity
  LinearOperators
  Serialization
  Spectral
  Utilities
  INTERFACE
  Domain
  PRIVATE
  ElasticityPointwiseFunctions
  )
