// The user module: a small C API over user-generator, built into the npm
// package in package/ by scripts/build_package.py.
//
// Every call returns a JSON document, a user or {"error": "..."}; the pointer
// stays valid until the next call. String inputs are UTF-8 and an empty
// string leaves the choice to the generator.

#include "cpp23_polyfill.hpp"
#include <dasmig/usergen.hpp>
#include <emscripten/emscripten.h>
#include "support.hpp"

#include <cmath>
#include <exception>
#include <string>

namespace
{

std::string result_; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const char* reply(std::string json)
{
    result_ = std::move(json);
    return result_.c_str();
}

std::string hex(std::uint64_t value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i, value >>= 4) out[static_cast<std::size_t>(i)] = digits[value & 0xf];
    return out;
}

// The request the C API's string and number arguments describe; "" and -1
// leave a choice to the generator.
dasmig::user_request make_request(const char* country, const char* sex, int age_min, int age_max,
                                  const char* reference_date, int safe, const char* status, int device)
{
    dasmig::user_request request;
    if (*country != '\0') request.country = country;
    const std::string_view sex_text{sex};
    if (sex_text == "female") request.person_sex = dasmig::sex::female;
    else if (sex_text == "male") request.person_sex = dasmig::sex::male;
    else if (!sex_text.empty()) throw std::invalid_argument("sex must be \"male\", \"female\" or empty");
    if (age_min >= 0 || age_max >= 0)
    {
        if (age_min < 0 || age_max < 0 || age_min > 120 || age_max > 120)
            throw std::invalid_argument("ages must both be between 0 and 120");
        request.ages = dasmig::age_range{.min = static_cast<std::uint8_t>(age_min),
                                         .max = static_cast<std::uint8_t>(age_max)};
    }
    if (*reference_date != '\0') request.reference_date = parse_reference_date(reference_date);
    request.safe = safe != 0;
    if (*status != '\0') request.status = status;
    request.device = device != 0;
    return request;
}

} // namespace

extern "C"
{

/// @brief Generate a user.
/// @param country ISO 3166-1 alpha-2 code (any case), or "" to draw one of
///        the supported countries, weighted by population.
/// @param sex "male", "female", or "" to draw it.
/// @param age_min, age_max Inclusive age range in completed years (13-120),
///        or -1 for both for the default (18 to 80).
/// @param reference_date The date ages and account dates are computed at,
///        YYYY-MM-DD, or "" for birth-generator's default (2023-07-01).
///        Never the clock.
/// @param safe Non-zero for contacts that reach no one (reserved email
///        domains, fictional phone numbers where a country has a range).
/// @param status An account status key (`active`, `inactive`, `unverified`,
///        `suspended`), or "" to draw one.
/// @param device Non-zero to add the device of the last sign-in
///        (user_request::device), under `device`.
/// @param seed 1 to 16 hexadecimal digits, or "" for a random seed (the
///        user's signature has the seed that replays it).
EMSCRIPTEN_KEEPALIVE const char* dasmig_user_generate(const char* country, const char* sex, int age_min,
                                                      int age_max, const char* reference_date, int safe,
                                                      const char* status, int device, const char* seed)
{
    try
    {
        const auto request = make_request(country, sex, age_min, age_max, reference_date, safe, status, device);

        auto& generator = dasmig::usg::instance();
        return reply(*seed == '\0' ? generator.get_user(request).to_json()
                                   : generator.get_user(request, parse_seed(seed)).to_json());
    }
    catch (const std::exception& error) { return reply(error_json(error.what())); }
}

/// @brief Generate users `first` to `first + count - 1` of a batch, as JSON
///        Lines (one user per line, each line ending in a line feed).
///
/// User `i` is exactly `dasmig_user_generate` with the seed
/// `core::item_seed(batch_seed, i)`, so a caller can split a batch into ranges
/// and join the results (the npm package does this across Web Workers).
/// Arguments as for dasmig_user_generate, except:
/// @param seed The batch seed, 1 to 16 hexadecimal digits (required).
/// @param first Index of the first user (a whole number below 2^53).
/// @param count Number of users (0 or more).
EMSCRIPTEN_KEEPALIVE const char* dasmig_user_generate_batch(const char* country, const char* sex, int age_min,
                                                            int age_max, const char* reference_date, int safe,
                                                            const char* status, int device, const char* seed,
                                                            double first, int count)
{
    try
    {
        const auto request = make_request(country, sex, age_min, age_max, reference_date, safe, status, device);
        if (*seed == '\0') throw std::invalid_argument("a batch needs a seed");
        if (count < 0) throw std::invalid_argument("count must not be negative");
        if (!(first >= 0.0 && first <= 9007199254740991.0) || first != std::floor(first))
            throw std::invalid_argument("first must be a whole number from 0 to 2^53 - 1");

        std::string lines;
        const dasmig::core::batch_options options{.count = static_cast<std::uint64_t>(count),
                                                  .first = static_cast<std::uint64_t>(first),
                                                  .threads = 1};
        dasmig::usg::instance().for_each_user(request, options, parse_seed(seed),
                                              [&lines](std::uint64_t, const dasmig::user& u) {
                                                  lines += u.to_json();
                                                  lines += '\n';
                                              });
        return reply(std::move(lines));
    }
    catch (const std::exception& error) { return reply(error_json(error.what())); }
}

/// @brief What this module can generate: the supported countries, the hash
///        of the eight libraries' data (`data`, the signature of a user
///        without a device) and device-generator's (`device_data`, combined
///        into the signature of a user with one).
EMSCRIPTEN_KEEPALIVE const char* dasmig_user_info()
{
    try
    {
        dasmig::core::json_writer w;
        w.begin_object().key("countries").begin_array();
        for (const auto& code : dasmig::usg::instance().supported_countries()) w.value(code);
        w.end_array()
            .field("data", hex(dasmig::usg::instance().data_hash()))
            .field("device_data", hex(dasmig::dvg::instance().data_hash()))
            .end_object();
        return reply(w.str());
    }
    catch (const std::exception& error) { return reply(error_json(error.what())); }
}

} // extern "C"
