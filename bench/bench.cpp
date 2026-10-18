// Benchmarks for user-generator; see bench.hpp for the options.
//
// The composed libraries load into their shared instances, so "load" times
// loading all of them; the lite tier is compiled in (see the Makefile). A
// library without the tier keeps what it has: this one ships only lite
// account data, so the full tier is every other library's full data with lite
// platforms, statuses and regional settings.

#include "../dasmig/usergen.hpp"
#include "bench.hpp"

namespace
{
// Load a tier into the account data and into the composed libraries.
bool load_all(dasmig::usg& gen, dasmig::dataset tier)
{
    (void)dasmig::cntg::instance().load(tier);
    (void)dasmig::bthg::instance().load(tier);
    (void)dasmig::ng::instance().load(tier);
    (void)dasmig::nng::instance().load(tier);
    (void)dasmig::adg::instance().load(tier);
    (void)dasmig::phg::instance().load(tier);
    (void)dasmig::emg::instance().load(tier);
    return gen.load(tier) || gen.load(dasmig::dataset::lite);
}
} // namespace

int main(int argc, char** argv)
{
    dasmig::bench::runner run{"user-generator", argc, argv};

    run.load("load", [&] {
        dasmig::usg gen;
        return load_all(gen, run.tier()) && gen.has_data();
    });

    dasmig::usg gen;
    if (!load_all(gen, run.tier()) || !gen.has_data()) return run.missing_tier();

    run.calls("get_user", [&](std::uint64_t seed) { return gen.get_user({}, seed); });
    run.calls("get_user BR", [&](std::uint64_t seed) { return gen.get_user({.country = "BR"}, seed); });
    run.calls("get_user to_json", [&](std::uint64_t seed) { return gen.get_user({}, seed).to_json(); });

    run.batches("get_user_batch", [&](std::size_t count, std::size_t threads, std::uint64_t seed) {
        return gen.get_user_batch({}, {.count = count, .threads = threads}, seed);
    });

    return run.finish();
}
