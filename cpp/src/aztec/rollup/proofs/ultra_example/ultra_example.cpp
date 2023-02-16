#include "ultra_example.hpp"
#include <common/log.hpp>
#include <plonk/proof_system/commitment_scheme/kate_commitment_scheme.hpp>
#include <stdlib/primitives/uint/uint.hpp>
#include <stdlib/primitives/bool/bool.hpp>

namespace rollup {
namespace proofs {
namespace ultra_example {

using namespace plonk;

// Defining ultra-plonk-specific types for ultra example.
using Composer = waffle::UltraComposer;
using Prover = waffle::UltraProver;
using Verifier = waffle::UltraVerifier;
using bool_ct = stdlib::bool_t<Composer>;
using uint32_ct = stdlib::uint32<Composer>;
using witness_ct = stdlib::witness_t<Composer>;
using public_witness_ct = stdlib::public_witness_t<Composer>;

static std::shared_ptr<waffle::proving_key> proving_key;
static std::shared_ptr<waffle::verification_key> verification_key;

void build_circuit(Composer& composer)
{
    uint32_ct a(witness_ct(&composer, 123));
    uint32_ct b(public_witness_ct(&composer, 456));
    bool_ct r = (a + b) == 579;
    r.assert_equal(true);
}

void init_proving_key(std::unique_ptr<waffle::ReferenceStringFactory>&& crs_factory)
{
    Composer composer(std::move(crs_factory));
    build_circuit(composer);
    proving_key = composer.compute_proving_key();
}

void init_verification_key(std::unique_ptr<waffle::ReferenceStringFactory>&& crs_factory)
{
    if (!proving_key) {
        std::abort();
    }
    // Patch the 'nothing' reference string fed to init_proving_key.
    proving_key->reference_string = crs_factory->get_prover_crs(proving_key->n);
    verification_key = Composer::compute_verification_key_base(proving_key, crs_factory->get_verifier_crs());
}

Prover new_prover()
{
    Composer composer(proving_key, nullptr);
    build_circuit(composer);

    info("composer gates: ", composer.get_num_gates());

    Prover prover = composer.create_prover();

    return prover;
}

bool verify_proof(waffle::plonk_proof const& proof)
{
    Verifier verifier(verification_key, Composer::create_manifest(1));

    std::unique_ptr<waffle::KateCommitmentScheme<waffle::ultra_settings>> kate_commitment_scheme =
        std::make_unique<waffle::KateCommitmentScheme<waffle::ultra_settings>>();
    verifier.commitment_scheme = std::move(kate_commitment_scheme);

    return verifier.verify_proof(proof);
}

} // namespace ultra_example
} // namespace proofs
} // namespace rollup