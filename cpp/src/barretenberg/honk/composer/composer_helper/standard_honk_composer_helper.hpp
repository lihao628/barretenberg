#pragma once

#include <utility>
#include "barretenberg/srs/reference_string/file_reference_string.hpp"
#include "barretenberg/honk/proof_system/prover.hpp"
#include "barretenberg/honk/proof_system/verifier.hpp"
#include "barretenberg/proof_system/circuit_constructors/standard_circuit_constructor.hpp"
#include "barretenberg/plonk/proof_system/verifier/verifier.hpp"
#include "barretenberg/proof_system/composer/composer_helper_lib.hpp"
#include "barretenberg/proof_system/composer/permutation_helper.hpp"

#include "barretenberg/honk/flavor/standard.hpp"

namespace proof_system::honk {
template <StandardFlavor Flavor> class StandardHonkComposerHelper_ {
  public:
    using PCSParams = typename Flavor::PCSParams;
    using CircuitConstructor = typename Flavor::CircuitConstructor;
    using ProvingKey = typename Flavor::ProvingKey;
    using VerificationKey = typename Flavor::VerificationKey;

    static constexpr size_t NUM_RESERVED_GATES = 2; // equal to the number of multilinear evaluations leaked
    static constexpr size_t NUM_WIRES = CircuitConstructor::NUM_WIRES;
    std::shared_ptr<ProvingKey> proving_key;
    std::shared_ptr<VerificationKey> verification_key;
    // TODO(#218)(kesha): we need to put this into the commitment key, so that the composer doesn't have to handle srs
    // at all
    std::shared_ptr<ReferenceStringFactory> crs_factory_;
    bool computed_witness = false;
    // TODO(Luke): use make_shared
    StandardHonkComposerHelper_()
        : StandardHonkComposerHelper_(std::shared_ptr<ReferenceStringFactory>(
              new proof_system::FileReferenceStringFactory("../srs_db/ignition")))
    {}
    StandardHonkComposerHelper_(std::shared_ptr<ReferenceStringFactory> crs_factory)
        : crs_factory_(std::move(crs_factory))
    {}

    StandardHonkComposerHelper_(std::unique_ptr<ReferenceStringFactory>&& crs_factory)
        : crs_factory_(std::move(crs_factory))
    {}
    StandardHonkComposerHelper_(std::shared_ptr<ProvingKey> p_key, std::shared_ptr<VerificationKey> v_key)
        : proving_key(std::move(p_key))
        , verification_key(std::move(v_key))
    {}
    StandardHonkComposerHelper_(StandardHonkComposerHelper_&& other) noexcept = default;
    StandardHonkComposerHelper_(const StandardHonkComposerHelper_& other) = delete;
    StandardHonkComposerHelper_& operator=(StandardHonkComposerHelper_&& other) noexcept = default;
    StandardHonkComposerHelper_& operator=(const StandardHonkComposerHelper_& other) = delete;
    ~StandardHonkComposerHelper_() = default;

    std::shared_ptr<ProvingKey> compute_proving_key(const CircuitConstructor& circuit_constructor);
    std::shared_ptr<VerificationKey> compute_verification_key(const CircuitConstructor& circuit_constructor);

    StandardVerifier_<Flavor> create_verifier(const CircuitConstructor& circuit_constructor);

    StandardProver_<Flavor> create_prover(const CircuitConstructor& circuit_constructor);

    // TODO(#216)(Adrian): Seems error prone to provide the number of randomized gates
    std::shared_ptr<ProvingKey> compute_proving_key_base(const CircuitConstructor& circuit_constructor,
                                                         const size_t minimum_circuit_size = 0,
                                                         const size_t num_randomized_gates = NUM_RESERVED_GATES);
    // This needs to be static as it may be used only to compute the selector commitments.

    static std::shared_ptr<VerificationKey> compute_verification_key_base(
        std::shared_ptr<ProvingKey> const& proving_key);

    void compute_witness(const CircuitConstructor& circuit_constructor, const size_t minimum_circuit_size = 0);
};
extern template class StandardHonkComposerHelper_<honk::flavor::Standard>;
using StandardHonkComposerHelper = StandardHonkComposerHelper_<honk::flavor::Standard>;

} // namespace proof_system::honk
