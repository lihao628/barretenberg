#include "../gemini/gemini.hpp"
#include "../shplonk/shplonk_single.hpp"
#include "barretenberg/common/mem.hpp"
#include "barretenberg/ecc/curves/bn254/fq12.hpp"
#include "barretenberg/ecc/curves/types.hpp"
#include "barretenberg/honk/pcs/commitment_key.hpp"
#include "barretenberg/honk/pcs/commitment_key.test.hpp"
#include "barretenberg/polynomials/polynomial.hpp"
#include "barretenberg/polynomials/polynomial_arithmetic.hpp"
#include "ipa.hpp"
#include <gtest/gtest.h>
using namespace barretenberg;
namespace proof_system::honk::pcs::ipa {

class IPATest : public CommitmentTest<Params> {
  public:
    using Fr = typename Params::Fr;
    using GroupElement = typename Params::GroupElement;
    using CK = typename Params::CommitmentKey;
    using VK = typename Params::VerificationKey;
    using Polynomial = barretenberg::Polynomial<Fr>;
};

TEST_F(IPATest, CommitOnManyZeroCoeffPolyWorks)
{
    constexpr size_t n = 4;
    Polynomial p(n);
    for (size_t i = 0; i < n - 1; i++) {
        p[i] = Fr::zero();
    }
    p[3] = Fr::one();
    GroupElement commitment = this->commit(p);
    auto srs_elements = this->ck()->srs->get_monomial_points();
    GroupElement expected = srs_elements[0] * p[0];
    // The SRS stored in the commitment key is the result after applying the pippenger point table so the
    // values at odd indices contain the point {srs[i-1].x * beta, srs[i-1].y}, where beta is the endomorphism
    // G_vec_local should use only the original SRS thus we extract only the even indices.
    for (size_t i = 2; i < 2 * n; i += 2) {
        expected += srs_elements[i] * p[i >> 1];
    }
    EXPECT_EQ(expected.normalize(), commitment.normalize());
}

TEST_F(IPATest, Commit)
{
    constexpr size_t n = 128;
    auto poly = this->random_polynomial(n);
    GroupElement commitment = this->commit(poly);
    auto srs_elements = this->ck()->srs->get_monomial_points();
    GroupElement expected = srs_elements[0] * poly[0];
    // The SRS stored in the commitment key is the result after applying the pippenger point table so the
    // values at odd indices contain the point {srs[i-1].x * beta, srs[i-1].y}, where beta is the endomorphism
    // G_vec_local should use only the original SRS thus we extract only the even indices.
    for (size_t i = 2; i < 2 * n; i += 2) {
        expected += srs_elements[i] * poly[i >> 1];
    }
    EXPECT_EQ(expected.normalize(), commitment.normalize());
}

TEST_F(IPATest, Open)
{
    using IPA = IPA<Params>;
    // generate a random polynomial, degree needs to be a power of two
    size_t n = 128;
    auto poly = this->random_polynomial(n);
    auto [x, eval] = this->random_eval(poly);
    auto commitment = this->commit(poly);
    const OpeningPair<Params> opening_pair = { x, eval };
    const OpeningClaim<Params> opening_claim{ opening_pair, commitment };

    // initialize empty prover transcript
    ProverTranscript<Fr> prover_transcript;
    IPA::compute_opening_proof(this->ck(), opening_pair, poly, prover_transcript);

    // initialize verifier transcript from proof data
    VerifierTranscript<Fr> verifier_transcript{ prover_transcript.proof_data };

    auto result = IPA::verify(this->vk(), opening_claim, verifier_transcript);
    EXPECT_TRUE(result);

    EXPECT_EQ(prover_transcript.get_manifest(), verifier_transcript.get_manifest());
}

TEST_F(IPATest, GeminiShplonkIPAWithShift)
{
    using IPA = IPA<Params>;
    using Shplonk = shplonk::SingleBatchOpeningScheme<Params>;
    using Gemini = gemini::MultilinearReductionScheme<Params>;

    const size_t n = 8;
    const size_t log_n = 3;

    Fr rho = Fr::random_element();

    // Generate multilinear polynomials, their commitments (genuine and mocked) and evaluations (genuine) at a random
    // point.
    const auto mle_opening_point = this->random_evaluation_point(log_n); // sometimes denoted 'u'
    auto poly1 = this->random_polynomial(n);
    auto poly2 = this->random_polynomial(n);
    poly2[0] = Fr::zero(); // this property is required of polynomials whose shift is used

    GroupElement commitment1 = this->commit(poly1);
    GroupElement commitment2 = this->commit(poly2);

    auto eval1 = poly1.evaluate_mle(mle_opening_point);
    auto eval2 = poly2.evaluate_mle(mle_opening_point);
    auto eval2_shift = poly2.evaluate_mle(mle_opening_point, true);

    // Collect multilinear evaluations for input to prover
    std::vector<Fr> multilinear_evaluations = { eval1, eval2, eval2_shift };

    std::vector<Fr> rhos = Gemini::powers_of_rho(rho, multilinear_evaluations.size());

    // Compute batched multivariate evaluation
    Fr batched_evaluation = Fr::zero();
    for (size_t i = 0; i < rhos.size(); ++i) {
        batched_evaluation += multilinear_evaluations[i] * rhos[i];
    }

    // Compute batched polynomials
    Polynomial batched_unshifted(n);
    Polynomial batched_to_be_shifted(n);
    batched_unshifted.add_scaled(poly1, rhos[0]);
    batched_unshifted.add_scaled(poly2, rhos[1]);
    batched_to_be_shifted.add_scaled(poly2, rhos[2]);

    // Compute batched commitments
    GroupElement batched_commitment_unshifted = GroupElement::zero();
    GroupElement batched_commitment_to_be_shifted = GroupElement::zero();
    batched_commitment_unshifted = commitment1 * rhos[0] + commitment2 * rhos[1];
    batched_commitment_to_be_shifted = commitment2 * rhos[2];

    auto prover_transcript = ProverTranscript<Fr>::init_empty();

    // Run the full prover PCS protocol:

    // Compute:
    // - (d+1) opening pairs: {r, \hat{a}_0}, {-r^{2^i}, a_i}, i = 0, ..., d-1
    // - (d+1) Fold polynomials Fold_{r}^(0), Fold_{-r}^(0), and Fold^(i), i = 0, ..., d-1
    auto fold_polynomials = Gemini::compute_fold_polynomials(
        mle_opening_point, std::move(batched_unshifted), std::move(batched_to_be_shifted));

    for (size_t l = 0; l < log_n - 1; ++l) {
        std::string label = "FOLD_" + std::to_string(l + 1);
        auto commitment = this->ck()->commit(fold_polynomials[l + 2]);
        prover_transcript.send_to_verifier(label, commitment);
    }

    const Fr r_challenge = prover_transcript.get_challenge("Gemini:r");

    const auto [gemini_opening_pairs, gemini_witnesses] =
        Gemini::compute_fold_polynomial_evaluations(mle_opening_point, std::move(fold_polynomials), r_challenge);

    for (size_t l = 0; l < log_n; ++l) {
        std::string label = "Gemini:a_" + std::to_string(l);
        const auto& evaluation = gemini_opening_pairs[l + 1].evaluation;
        prover_transcript.send_to_verifier(label, evaluation);
    }

    // Shplonk prover output:
    // - opening pair: (z_challenge, 0)
    // - witness: polynomial Q - Q_z
    const Fr nu_challenge = prover_transcript.get_challenge("Shplonk:nu");
    auto batched_quotient_Q = Shplonk::compute_batched_quotient(gemini_opening_pairs, gemini_witnesses, nu_challenge);
    prover_transcript.send_to_verifier("Shplonk:Q", this->ck()->commit(batched_quotient_Q));

    const Fr z_challenge = prover_transcript.get_challenge("Shplonk:z");
    const auto [shplonk_opening_pair, shplonk_witness] = Shplonk::compute_partially_evaluated_batched_quotient(
        gemini_opening_pairs, gemini_witnesses, std::move(batched_quotient_Q), nu_challenge, z_challenge);

    // KZG prover:
    // - Adds commitment [W] to transcript
    IPA::compute_opening_proof(this->ck(), shplonk_opening_pair, shplonk_witness, prover_transcript);

    // Run the full verifier PCS protocol with genuine opening claims (genuine commitment, genuine evaluation)

    auto verifier_transcript = VerifierTranscript<Fr>::init_empty(prover_transcript);

    // Gemini verifier output:
    // - claim: d+1 commitments to Fold_{r}^(0), Fold_{-r}^(0), Fold^(l), d+1 evaluations a_0_pos, a_l, l = 0:d-1
    auto gemini_verifier_claim = Gemini::reduce_verify(mle_opening_point,
                                                       batched_evaluation,
                                                       batched_commitment_unshifted,
                                                       batched_commitment_to_be_shifted,
                                                       verifier_transcript);

    // Shplonk verifier claim: commitment [Q] - [Q_z], opening point (z_challenge, 0)
    const auto shplonk_verifier_claim = Shplonk::reduce_verify(this->vk(), gemini_verifier_claim, verifier_transcript);

    // IPA verifier:
    // aggregates inputs [Q] - [Q_z] and [W] into an 'accumulator' (can perform pairing check on result)
    bool verified = IPA::verify(this->vk(), shplonk_verifier_claim, verifier_transcript);

    // Final pairing check: e([Q] - [Q_z] + z[W], [1]_2) = e([W], [x]_2)

    EXPECT_EQ(verified, true);
}
} // namespace proof_system::honk::pcs::ipa
