#include "catch_amalgamated.hpp"
#include "../dasmig/usergen.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static dasmig::usg make_loaded_generator()
{
    dasmig::usg gen;
    if (!gen.load(dasmig::dataset::lite))
    {
        for (const auto* candidate : {"resources/lite", "../resources/lite"})
        {
            if (std::filesystem::is_directory(candidate))
            {
                gen.load(std::filesystem::path{candidate});
                break;
            }
        }
    }
    REQUIRE(gen.has_data());
    return gen;
}

static bool has_any(const std::string& text, std::string_view chars)
{
    return text.find_first_of(chars) != std::string::npos;
}

// Seconds since the epoch of an ISO 8601 UTC time ("2021-03-14T09:26:53Z").
static long long to_seconds(const std::string& iso)
{
    REQUIRE(iso.size() == 20);
    const auto num = [&](std::size_t at, std::size_t n) { return std::stoi(iso.substr(at, n)); };
    const std::chrono::year_month_day d{std::chrono::year{num(0, 4)},
                                        std::chrono::month{static_cast<unsigned>(num(5, 2))},
                                        std::chrono::day{static_cast<unsigned>(num(8, 2))}};
    REQUIRE(d.ok());
    return std::chrono::sys_days{d}.time_since_epoch().count() * 86400LL + num(11, 2) * 3600LL +
           num(14, 2) * 60LL + num(17, 2);
}

static long long date_seconds(std::chrono::year_month_day d)
{
    return std::chrono::sys_days{d}.time_since_epoch().count() * 86400LL;
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

TEST_CASE("load(dataset::lite) loads the account tables", "[loading]")
{
    const auto gen = make_loaded_generator();
    REQUIRE(gen.has_account_data());
    for (const auto t : {dasmig::user_table::platforms, dasmig::user_table::platform_shares,
                         dasmig::user_table::statuses, dasmig::user_table::password_words, dasmig::user_table::password_patterns})
    {
        REQUIRE(gen.layers(t).size() == 1);
    }
}

TEST_CASE("load - non-existent path is a no-op", "[loading]")
{
    dasmig::usg gen;
    gen.load(std::filesystem::path{"no/such/directory"});
    REQUIRE_FALSE(gen.has_account_data());
    REQUIRE(gen.data_hash() == 0);
}

#ifndef DASMIG_USER_EMBED // compiled-in data is never missing
TEST_CASE("load(dataset) returns false for a missing tier", "[loading]")
{
    const auto old_cwd = std::filesystem::current_path();
    const auto tmp = std::filesystem::temp_directory_path() / "usg_empty_dir";
    std::filesystem::create_directories(tmp);
    std::filesystem::current_path(tmp);
    dasmig::usg gen;
    const bool loaded = gen.load(dasmig::dataset::full);
    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(tmp);
    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(gen.has_account_data());
}
#endif

TEST_CASE("the singleton loads data on first use", "[loading]")
{
    REQUIRE(dasmig::usg::instance().has_data());
}

TEST_CASE("supported_countries lists the countries every part has", "[loading]")
{
    const auto gen = make_loaded_generator();
    const auto countries = gen.supported_countries();
    REQUIRE(countries.size() > 50);
    REQUIRE(std::ranges::is_sorted(countries));
    for (const auto* code : {"BR", "US", "DE", "JP", "IN", "FR", "GB", "MX"})
    {
        REQUIRE(std::ranges::find(countries, std::string{code}) != countries.end());
    }
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

TEST_CASE("get_user throws when no data is loaded", "[generation]")
{
    dasmig::usg gen;
    REQUIRE_THROWS_AS(gen.get_user(dasmig::user_request{}), std::runtime_error);
    REQUIRE_THROWS_AS(gen.get_user(dasmig::user_request{}, std::uint64_t{1}), std::runtime_error);
}

TEST_CASE("get_user with a seed is deterministic", "[generation][seed]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 50; ++seed)
    {
        REQUIRE(gen.get_user({}, seed).to_json() == gen.get_user({}, seed).to_json());
    }
    REQUIRE(gen.get_user({}, 1).to_json() != gen.get_user({}, 2).to_json());
}

TEST_CASE("a result replays from its seed", "[generation][seed]")
{
    auto gen = make_loaded_generator();
    const auto first = gen.get_user({});
    REQUIRE(gen.get_user({}, first.seed()).to_json() == first.to_json());
    REQUIRE(first.signature().seed == first.seed());
    REQUIRE(first.signature().data == gen.data_hash());
}

TEST_CASE("explicit per-call seeds replay across generator instances", "[seed]")
{
    const auto a = make_loaded_generator();
    const auto b = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 20; ++seed)
    {
        REQUIRE(a.get_user({}, seed).to_json() == b.get_user({}, seed).to_json());
    }
}

TEST_CASE("the parts of a user agree on the country", "[generation]")
{
    const auto gen = make_loaded_generator();
    for (const auto* code : {"BR", "US", "DE", "JP", "IN", "FR", "MX", "ES"})
    {
        for (std::uint64_t seed = 0; seed < 20; ++seed)
        {
            const auto u = gen.get_user({.country = code}, seed);
            REQUIRE(u.country_code == code);
            REQUIRE_FALSE(u.country_name.empty());
            REQUIRE(u.birthday.country_code == code);
            REQUIRE(u.location.country_code == code);
            REQUIRE(u.mobile.country_code == code);
            REQUIRE(u.mail.country_code == code);
            REQUIRE_FALSE(u.name.empty());
            REQUIRE_FALSE(u.given.empty());
            REQUIRE(u.name.find(u.given) != std::string::npos);
            if (!u.family.empty()) REQUIRE(u.name.find(u.family) != std::string::npos);
        }
    }
}

TEST_CASE("the country is accepted in any case and a missing one is refused", "[generation][errors]")
{
    const auto gen = make_loaded_generator();
    REQUIRE(gen.get_user({.country = "br"}, 3).to_json() == gen.get_user({.country = "BR"}, 3).to_json());
    REQUIRE_THROWS_AS(gen.get_user({.country = "ZZ"}, 3), std::invalid_argument);
    REQUIRE_THROWS_AS(gen.get_user({.country = "BRA"}, 3), std::invalid_argument);
}

TEST_CASE("sex and age follow the request", "[generation]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        const auto f = gen.get_user({.person_sex = dasmig::sex::female, .ages = {.min = 30, .max = 40}}, seed);
        REQUIRE(f.person_sex() == dasmig::sex::female);
        REQUIRE(f.age() >= 30);
        REQUIRE(f.age() <= 40);
        const auto m = gen.get_user({.person_sex = dasmig::sex::male}, seed);
        REQUIRE(m.person_sex() == dasmig::sex::male);
        REQUIRE(m.age() >= 18);
        REQUIRE(m.age() <= 80);
    }
}

TEST_CASE("a username has the shape of nickname-generator's username style", "[generation][account]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        const auto u = gen.get_user({}, seed);
        const auto& name = u.account.username;
        REQUIRE_FALSE(name.empty());
        REQUIRE(name.size() <= 24);
        for (const char c : name)
        {
            REQUIRE(((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_'));
        }
    }
}

TEST_CASE("the email does not repeat the username's digits", "[generation][account]")
{
    const auto gen = make_loaded_generator();
    std::size_t doubled = 0;
    for (std::uint64_t seed = 0; seed < 500; ++seed)
    {
        const auto u = gen.get_user({.country = "US"}, seed);
        // A local part that is the username plus more digits would end in the
        // username's digits followed by others.
        const auto& local = u.mail.local;
        const auto& name = u.account.username;
        if (local.size() > name.size() && local.starts_with(name) &&
            std::isdigit(static_cast<unsigned char>(name.back())))
        {
            ++doubled;
        }
    }
    REQUIRE(doubled == 0);
}

TEST_CASE("safe users reach no one", "[generation][safe]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        const auto u = gen.get_user({.country = "US", .safe = true}, seed);
        REQUIRE(u.mail.reserved);
        REQUIRE(u.mail.domain.find("example.") == 0);
        REQUIRE(u.mobile.reserved);
    }
    // And a user that is not safe has an ordinary provider.
    std::size_t reserved = 0;
    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        reserved += gen.get_user({.country = "US"}, seed).mail.reserved ? 1 : 0;
    }
    REQUIRE(reserved == 0);
}

// ---------------------------------------------------------------------------
// The account
// ---------------------------------------------------------------------------

TEST_CASE("passwords follow the policy", "[account][password]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 300; ++seed)
    {
        const auto u = gen.get_user({}, seed);
        const auto& p = u.account.password;
        REQUIRE(p.size() >= 12);
        REQUIRE(p.size() <= 16);
        REQUIRE(has_any(p, "abcdefghijklmnopqrstuvwxyz"));
        REQUIRE(has_any(p, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
        REQUIRE(has_any(p, "0123456789"));
        REQUIRE(has_any(p, "!@#$%^&*-_=+?"));
    }

    dasmig::user_request pin;
    pin.password = {.min_length = 6, .max_length = 6, .lowercase = false, .uppercase = false, .symbols = false};
    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        const auto p = gen.get_user(pin, seed).account.password;
        REQUIRE(p.size() == 6);
        REQUIRE(p.find_first_not_of("0123456789") == std::string::npos);
    }

    dasmig::user_request clear;
    clear.password = {.min_length = 40, .max_length = 40, .avoid_ambiguous = true};
    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        REQUIRE_FALSE(has_any(gen.get_user(clear, seed).account.password, "0O1lI|"));
    }

    dasmig::user_request loose;
    loose.password = {.min_length = 3, .max_length = 3, .require_each = false};
    std::set<std::size_t> lengths;
    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        lengths.insert(gen.get_user(loose, seed).account.password.size());
    }
    REQUIRE(lengths == std::set<std::size_t>{3});
}

// Whether a password keeps to a policy.
static bool follows(const std::string& p, const dasmig::password_policy& policy)
{
    const auto count = [&](std::string_view chars) {
        return static_cast<std::size_t>(std::ranges::count_if(p, [&](char c) { return chars.find(c) != std::string_view::npos; }));
    };
    const auto lower = count("abcdefghijklmnopqrstuvwxyz");
    const auto upper = count("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    const auto digit = count("0123456789");
    const auto symbol = count("!@#$%^&*-_=+?");
    if (p.size() < policy.min_length || p.size() > policy.max_length) return false;
    if (lower + upper + digit + symbol != p.size()) return false;
    if ((!policy.lowercase && lower > 0) || (!policy.uppercase && upper > 0) || (!policy.digits && digit > 0) ||
        (!policy.symbols && symbol > 0))
        return false;
    if (policy.avoid_ambiguous && has_any(p, "0O1lI|")) return false;
    return !policy.require_each || ((!policy.lowercase || lower > 0) && (!policy.uppercase || upper > 0) &&
                                    (!policy.digits || digit > 0) && (!policy.symbols || symbol > 0));
}

TEST_CASE("human passwords follow the policy", "[account][password][human]")
{
    const auto gen = make_loaded_generator();
    const std::vector<dasmig::password_policy> policies{
        {},
        {.min_length = 6, .max_length = 20, .require_each = false},
        {.min_length = 8, .max_length = 8},
        {.min_length = 4, .max_length = 4, .lowercase = false, .uppercase = false, .symbols = false},
        {.min_length = 6, .max_length = 10, .uppercase = false, .symbols = false},
        {.min_length = 10, .max_length = 12, .avoid_ambiguous = true},
        {.min_length = 5, .max_length = 40, .lowercase = false, .digits = false, .require_each = false},
    };
    for (auto policy : policies)
    {
        policy.style = dasmig::password_style::human;
        dasmig::user_request request;
        request.password = policy;
        for (std::uint64_t seed = 0; seed < 400; ++seed)
        {
            const auto p = gen.get_user(request, seed).account.password;
            INFO(p);
            REQUIRE(follows(p, policy));
        }
    }
}

TEST_CASE("human passwords look like what people choose", "[account][password][human]")
{
    const auto gen = make_loaded_generator();
    dasmig::user_request request;
    request.password = {.min_length = 4, .max_length = 30, .require_each = false,
                        .style = dasmig::password_style::human};
    std::set<std::string> distinct;
    int with_word = 0;
    int with_year = 0;
    int ends_in_digit = 0;
    int whole = 0;
    constexpr int n = 2000;
    for (std::uint64_t seed = 0; seed < n; ++seed)
    {
        const auto u = gen.get_user(request, seed);
        const auto& p = u.account.password;
        distinct.insert(p);
        with_word += p.find("password") != std::string::npos || p.find("Password") != std::string::npos;
        with_year += p.find(std::to_string(u.birthday.year)) != std::string::npos;
        ends_in_digit += p.back() >= '0' && p.back() <= '9';
        whole += p == "123456" || p == "password" || p == "qwerty";
    }
    REQUIRE(distinct.size() > n / 2);   // not a short list
    REQUIRE(with_word > 15);            // the commonest word comes up
    REQUIRE(with_year > 50);            // birth years are used
    REQUIRE(ends_in_digit > n / 3);     // most end in digits
    REQUIRE(whole > 10);                // the plainest passwords come up

    // The same seed gives the same password, and the style changes nothing else.
    const auto plain = gen.get_user({}, 7);
    const auto human = gen.get_user({.password = {.style = dasmig::password_style::human}}, 7);
    REQUIRE(human.account.username == plain.account.username);
    REQUIRE(human.mail.address() == plain.mail.address());
    REQUIRE(human.account.created == plain.account.created);
    REQUIRE(human.account.password != plain.account.password);
    REQUIRE(gen.get_user({.password = {.style = dasmig::password_style::human}}, 7).account.password ==
            human.account.password);
}

TEST_CASE("human passwords come from the pattern and word tables", "[account][password][human][extend]")
{
    auto gen = make_loaded_generator();
    gen.extend_from_string(dasmig::user_table::password_patterns, "pattern\tweight\n{Word}-{year}\t1\n", "one.tsv",
                           dasmig::core::layer_mode::replace);
    gen.extend_from_string(dasmig::user_table::password_words, "word\tkind\tweight\nzebra\tword\t1\n", "z.tsv",
                           dasmig::core::layer_mode::replace);
    dasmig::user_request request;
    request.password = {.min_length = 1, .max_length = 40, .require_each = false,
                        .style = dasmig::password_style::human};
    for (std::uint64_t seed = 0; seed < 50; ++seed)
    {
        const auto u = gen.get_user(request, seed);
        REQUIRE(u.account.password == "Zebra-" + std::to_string(u.birthday.year));
    }

    // The name of the person.
    gen.extend_from_string(dasmig::user_table::password_patterns, "pattern\tweight\n{name}\t1\n", "name.tsv",
                           dasmig::core::layer_mode::replace);
    const auto u = gen.get_user({.country = "US", .password = request.password}, 3);
    std::string given;
    for (const char c : u.given) given.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
    REQUIRE(u.account.password == given);

    // Without patterns the style cannot be used; the random style still can.
    gen.extend_from_string(dasmig::user_table::password_patterns, "pattern\tweight\n", "none.tsv",
                           dasmig::core::layer_mode::replace);
    REQUIRE_THROWS_AS(gen.get_user(request, 1), std::runtime_error);
    REQUIRE_NOTHROW(gen.get_user({}, 1));

    // Bad rows are refused.
    REQUIRE_THROWS_AS(gen.extend_from_string(dasmig::user_table::password_patterns,
                                             "pattern\tweight\n{pet}\t1\n", "bad.tsv"),
                      dasmig::core::tsv_error); // not a token
    REQUIRE_THROWS_AS(gen.extend_from_string(dasmig::user_table::password_patterns,
                                             "pattern\tweight\n{word\t1\n", "bad.tsv"),
                      dasmig::core::tsv_error); // unclosed
    REQUIRE_THROWS_AS(gen.extend_from_string(dasmig::user_table::password_words,
                                             "word\tkind\tweight\nzebra\tanimal\t1\n", "bad.tsv"),
                      dasmig::core::tsv_error); // kind
}

TEST_CASE("an invalid request is rejected", "[generation][errors]")
{
    const auto gen = make_loaded_generator();
    REQUIRE_THROWS_AS(gen.get_user({.ages = {.min = 50, .max = 40}}, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(gen.get_user({.ages = {.min = 10, .max = 40}}, 1), std::invalid_argument); // under sign_up_age
    REQUIRE_NOTHROW(gen.get_user({.ages = {.min = 10, .max = 40}, .sign_up_age = 10}, 1));
    REQUIRE_THROWS_AS(gen.get_user({.status = "no-such-status"}, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(gen.get_user({.reference_date = {std::chrono::year{2023}, std::chrono::month{2}, std::chrono::day{30}}}, 1),
                      std::invalid_argument);

    dasmig::user_request bad;
    bad.password = {.min_length = 0};
    REQUIRE_THROWS_AS(gen.get_user(bad, 1), std::invalid_argument);
    bad.password = {.min_length = 9, .max_length = 8};
    REQUIRE_THROWS_AS(gen.get_user(bad, 1), std::invalid_argument);
    bad.password = {.lowercase = false, .uppercase = false, .digits = false, .symbols = false};
    REQUIRE_THROWS_AS(gen.get_user(bad, 1), std::invalid_argument);
    bad.password = {.min_length = 3, .max_length = 8};
    REQUIRE_THROWS_AS(gen.get_user(bad, 1), std::invalid_argument); // four classes in three characters
}

TEST_CASE("account times are ordered and inside the allowed window", "[account][dates]")
{
    const auto gen = make_loaded_generator();
    const dasmig::user_request request{};
    const auto ref = date_seconds(request.reference_date);
    const auto start = date_seconds({std::chrono::year{2013}, std::chrono::July, std::chrono::day{1}});
    for (std::uint64_t seed = 0; seed < 500; ++seed)
    {
        const auto u = gen.get_user(request, seed);
        const auto created = to_seconds(u.account.created);
        REQUIRE(created >= start);
        REQUIRE(created < ref);
        const auto born = date_seconds({std::chrono::year{u.birthday.year},
                                        std::chrono::month{u.birthday.month},
                                        std::chrono::day{u.birthday.day}});
        REQUIRE(created >= born + 13LL * 365 * 86400); // at least thirteen years old, allowing leap days
        if (!u.account.last_login.empty())
        {
            const auto last = to_seconds(u.account.last_login);
            REQUIRE(last >= created);
            REQUIRE(last < ref);
        }
    }
}

TEST_CASE("the service start and the sign-up age limit the sign-up time", "[account][dates]")
{
    const auto gen = make_loaded_generator();
    const dasmig::user_request request{.service_start = std::chrono::year_month_day{
                                           std::chrono::year{2022}, std::chrono::December, std::chrono::day{1}}};
    const auto from = date_seconds(*request.service_start);
    const auto ref = date_seconds(request.reference_date);
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        const auto created = to_seconds(gen.get_user(request, seed).account.created);
        REQUIRE(created >= from);
        REQUIRE(created < ref);
    }
}

TEST_CASE("the last login follows the status", "[account][dates]")
{
    const auto gen = make_loaded_generator();
    const dasmig::user_request base{};
    const auto ref = date_seconds(base.reference_date);
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        auto request = base;
        request.status = "unverified";
        const auto never = gen.get_user(request, seed);
        REQUIRE(never.account.status == "unverified");
        REQUIRE(never.account.last_login.empty());
        REQUIRE(never.to_json().find("\"last_login\":null") != std::string::npos);

        request.status = "active";
        const auto active = gen.get_user(request, seed);
        REQUIRE(active.account.status == "active");
        REQUIRE_FALSE(active.account.last_login.empty());
        // Recent, or the sign-up if that is later than 90 days ago.
        const auto last = to_seconds(active.account.last_login);
        REQUIRE(last >= std::max(to_seconds(active.account.created), ref - 90LL * 86400));

        request.status = "inactive";
        const auto stale = gen.get_user(request, seed);
        const auto stale_last = to_seconds(stale.account.last_login);
        const auto stale_created = to_seconds(stale.account.created);
        // A year ago or more, unless the account is younger than that.
        REQUIRE((stale_last <= ref - 365LL * 86400 || stale_last == stale_created));
    }
}

TEST_CASE("forcing the status changes nothing else", "[generation][seed]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 50; ++seed)
    {
        const auto a = gen.get_user({.status = "active"}, seed);
        const auto b = gen.get_user({.status = "suspended"}, seed);
        REQUIRE(a.name == b.name);
        REQUIRE(a.account.username == b.account.username);
        REQUIRE(a.account.password == b.account.password);
        REQUIRE(a.account.created == b.account.created);
        REQUIRE(a.account.platform == b.account.platform);
        REQUIRE(a.mail.address() == b.mail.address());
        REQUIRE(a.mobile.e164() == b.mobile.e164());
        REQUIRE(a.location.formatted() == b.location.formatted());
        REQUIRE(a.account.status != b.account.status);
    }
}

TEST_CASE("changing the password policy changes nothing else", "[generation][seed]")
{
    const auto gen = make_loaded_generator();
    dasmig::user_request pin;
    pin.password = {.min_length = 4, .max_length = 4, .lowercase = false, .uppercase = false, .symbols = false};
    for (std::uint64_t seed = 0; seed < 50; ++seed)
    {
        const auto a = gen.get_user({}, seed);
        const auto b = gen.get_user(pin, seed);
        REQUIRE(a.name == b.name);
        REQUIRE(a.account.username == b.account.username);
        REQUIRE(a.account.created == b.account.created);
        REQUIRE(a.account.last_login == b.account.last_login);
        REQUIRE(a.account.platform == b.account.platform);
        REQUIRE(a.mail.address() == b.mail.address());
    }
}

TEST_CASE("locale, time zone and currency come from country-generator", "[account][regional]")
{
    const auto gen = make_loaded_generator();
    const auto& countries = dasmig::cntg::instance();
    std::set<std::string> ca_locales;
    std::set<std::string> br_zones;
    for (std::uint64_t seed = 0; seed < 400; ++seed)
    {
        for (const char* code : {"BR", "CA", "JP", "CH"})
        {
            const auto u = gen.get_user({.country = code}, seed);
            const dasmig::core::rng call{seed};
            const auto locale = countries.get_locale({.country = code}, call.split("locale")());
            REQUIRE(u.account.locale == locale.tag);
            REQUIRE(u.account.currency == locale.currency_code);
            REQUIRE(u.account.currency_digits == locale.currency_digits);
            REQUIRE(u.account.timezone == countries.get_time_zone({.country = code}, call.split("timezone")()).name);
        }
        ca_locales.insert(gen.get_user({.country = "CA"}, seed).account.locale);
        br_zones.insert(gen.get_user({.country = "BR"}, seed).account.timezone);
        REQUIRE(gen.get_user({.country = "JP"}, seed).account.timezone == "Asia/Tokyo");
        REQUIRE(gen.get_user({.country = "JP"}, seed).account.currency == "JPY");
        REQUIRE(gen.get_user({.country = "JP"}, seed).account.currency_digits == 0);
        REQUIRE(gen.get_user({.country = "BR"}, seed).account.currency_digits == 2);
    }
    REQUIRE(ca_locales == std::set<std::string>{"en-CA", "fr-CA"});
    REQUIRE(br_zones.contains("America/Sao_Paulo"));
    REQUIRE(br_zones.contains("America/Manaus"));
}

TEST_CASE("every supported country has a locale, a zone and an IANA name for it", "[account][regional]")
{
    const auto gen = make_loaded_generator();
    for (const auto& code : gen.supported_countries())
    {
        if (code == "KR") continue; // the lite tier has no male Korean names: not a locale question
        const auto u = gen.get_user({.country = code}, 11);
        REQUIRE_FALSE(u.account.locale.empty());
        REQUIRE(u.account.timezone.find('/') != std::string::npos);
        REQUIRE_FALSE(u.account.currency.empty());
    }
}

TEST_CASE("a country instance without the locale tables is no data", "[account][regional][loading]")
{
    dasmig::cntg bare;
    bare.load(std::filesystem::path{"no/such/directory"});
    dasmig::usg gen{dasmig::usg::sources{.country = &bare}};
    REQUIRE(gen.load(dasmig::dataset::lite));
    REQUIRE_FALSE(gen.has_data());
    REQUIRE_THROWS_AS(gen.get_user({.country = "BR"}, 1), std::runtime_error);
}

TEST_CASE("the data hash covers the locale tables", "[account][regional][loading]")
{
    auto& countries = dasmig::cntg::instance();
    auto gen = make_loaded_generator();
    const auto before = gen.data_hash();
    dasmig::cntg own;
    REQUIRE(own.load(dasmig::dataset::lite));
    dasmig::usg same{dasmig::usg::sources{.country = &own}};
    REQUIRE(same.load(dasmig::dataset::lite));
    REQUIRE(same.data_hash() == before);
    own.extend_from_string(dasmig::country_table::currencies, "code\tnumeric\tdigits\tcash_digits\nXTS\t963\t2\t\n", "currencies.tsv");
    REQUIRE(own.locale_data_hash() != countries.locale_data_hash());
    REQUIRE(same.data_hash() != before);
    REQUIRE(same.get_user({.country = "FR"}, 1).signature().data == same.data_hash());
}

TEST_CASE("platform shares follow the country", "[account][platform]")
{
    const auto gen = make_loaded_generator();
    const auto share = [&](const char* country, const char* key) {
        int hits = 0;
        constexpr int n = 1500;
        for (std::uint64_t seed = 0; seed < n; ++seed)
        {
            hits += gen.get_user({.country = country}, seed).account.platform == key ? 1 : 0;
        }
        return static_cast<double>(hits) / n;
    };
    REQUIRE(share("US", "ios-phone") == Catch::Approx(0.50).margin(0.05)); // an override
    REQUIRE(share("IN", "ios-phone") == Catch::Approx(0.05).margin(0.03));
    REQUIRE(share("IN", "android-phone") == Catch::Approx(0.85).margin(0.04));
    REQUIRE(share("PL", "android-phone") == Catch::Approx(0.44).margin(0.05)); // no override: the global share
}

TEST_CASE("status shares follow the data", "[account][stats]")
{
    const auto gen = make_loaded_generator();
    std::map<std::string, int> counts;
    constexpr int n = 3000;
    for (std::uint64_t seed = 0; seed < n; ++seed)
    {
        ++counts[gen.get_user({}, seed).account.status];
    }
    REQUIRE(static_cast<double>(counts["active"]) / n == Catch::Approx(0.88).margin(0.03));
    REQUIRE(static_cast<double>(counts["inactive"]) / n == Catch::Approx(0.07).margin(0.02));
    REQUIRE(counts["unverified"] > 0);
    REQUIRE(counts["suspended"] > 0);
}

// ---------------------------------------------------------------------------
// The device
// ---------------------------------------------------------------------------

// The day an account was last used: the last login, or the sign-up for one
// that never logged in, no earlier than the first day device-generator covers.
static std::chrono::year_month_day last_seen(const dasmig::user& u)
{
    const auto& at = u.account.last_login.empty() ? u.account.created : u.account.last_login;
    const std::chrono::sys_days day{std::chrono::days{to_seconds(at) / 86400}};
    return std::chrono::year_month_day{
        std::max(day, std::chrono::sys_days{dasmig::device_request::earliest_reference_date})};
}

TEST_CASE("no device unless the request asks for one", "[device]")
{
    const auto gen = make_loaded_generator();
    const auto u = gen.get_user({.country = "BR"}, 3);
    REQUIRE_FALSE(u.client.has_value());
    REQUIRE(u.to_json().find("\"device\":{") == std::string::npos);
    REQUIRE(u.signature().data == gen.data_hash());
}

TEST_CASE("the device is device-generator's for the account", "[device]")
{
    const auto gen = make_loaded_generator();
    const auto& devices = dasmig::dvg::instance();
    for (std::uint64_t seed = 0; seed < 300; ++seed)
    {
        const auto u = gen.get_user({.device = true}, seed);
        REQUIRE(u.client.has_value());
        const auto& d = *u.client;
        REQUIRE(d.country_code == u.country_code);
        REQUIRE(d.platform == u.account.platform);
        REQUIRE(d.kind == u.account.device);
        REQUIRE(d.os == u.account.os);
        REQUIRE(d.accept_language.starts_with(u.account.locale));
        // The same device as a call of its own, from the user's "device" stream.
        const auto alone = devices.get_device({.country = u.country_code,
                                               .platform = u.account.platform,
                                               .locale = u.account.locale,
                                               .reference_date = last_seen(u)},
                                              dasmig::core::rng{seed}.split("device")());
        REQUIRE(d.to_json() == alone.to_json());
        REQUIRE(u.signature().data == dasmig::core::hash_combine(gen.data_hash(), devices.data_hash()));
    }
}

TEST_CASE("asking for the device changes nothing else", "[device][seed]")
{
    const auto gen = make_loaded_generator();
    const auto countries = dasmig::dvg::instance().countries();
    int same_country = 0;
    for (std::uint64_t seed = 0; seed < 300; ++seed)
    {
        // A fixed country and, where the drawn country has devices, a drawn one.
        for (const bool drawn : {false, true})
        {
            const dasmig::user_request off{.country = drawn ? std::nullopt : std::optional<std::string>{"JP"}};
            auto on = off;
            on.device = true;
            const auto a = gen.get_user(off, seed);
            if (drawn && !std::ranges::binary_search(countries, a.country_code)) continue;
            same_country += drawn ? 1 : 0;
            auto b = gen.get_user(on, seed);
            REQUIRE(b.client.has_value());
            b.client.reset();
            // Without the device and the signature's data hash, the JSON is the same.
            const auto strip = [](std::string json) { return json.substr(0, json.find("\"signature\":")); };
            REQUIRE(strip(a.to_json()) == strip(b.to_json()));
            REQUIRE(a.seed() == b.seed());
        }
    }
    REQUIRE(same_country > 250); // device-generator has nearly every country users are drawn from
}

TEST_CASE("a drawn country with a device is one device-generator has", "[device]")
{
    const auto gen = make_loaded_generator();
    const auto countries = dasmig::dvg::instance().countries();
    for (std::uint64_t seed = 0; seed < 1000; ++seed)
    {
        const auto u = gen.get_user({.device = true}, seed);
        REQUIRE(std::ranges::binary_search(countries, u.country_code));
    }
}

TEST_CASE("a safe user's device has a documentation address", "[device][safe]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        REQUIRE(gen.get_user({.safe = true, .device = true}, seed).client->reserved);
        REQUIRE_FALSE(gen.get_user({.device = true}, seed).client->reserved);
    }
}

TEST_CASE("the device is seen at the last sign-in", "[device][dates]")
{
    const auto gen = make_loaded_generator();
    // Never logged in: the sign-up day.
    const auto fresh = gen.get_user({.country = "US", .status = "unverified", .device = true}, 5);
    REQUIRE(fresh.account.last_login.empty());
    const auto alone = dasmig::dvg::instance().get_device(
        {.country = "US", .platform = fresh.account.platform, .locale = fresh.account.locale,
         .reference_date = last_seen(fresh)},
        dasmig::core::rng{5}.split("device")());
    REQUIRE(fresh.client->to_json() == alone.to_json());

    // A login before 2021 is seen at 2021-01-01, the earliest date device-generator covers.
    const auto old = gen.get_user({.country = "US",
                                   .reference_date = std::chrono::year{2016} / 6 / 1,
                                   .device = true},
                                  8);
    REQUIRE(to_seconds(old.account.last_login.empty() ? old.account.created : old.account.last_login) <
            date_seconds(dasmig::device_request::earliest_reference_date));
    const auto at_earliest = dasmig::dvg::instance().get_device(
        {.country = "US", .platform = old.account.platform, .locale = old.account.locale,
         .reference_date = dasmig::device_request::earliest_reference_date},
        dasmig::core::rng{8}.split("device")());
    REQUIRE(old.client->to_json() == at_earliest.to_json());
}

TEST_CASE("the device comes from the generator's own device instance", "[device][loading]")
{
    dasmig::dvg empty;
    auto gen = make_loaded_generator();
    dasmig::usg own{dasmig::usg::sources{.device = &empty}};
    REQUIRE(own.load(dasmig::dataset::lite));
    REQUIRE(own.get_user({.country = "FR"}, 1).to_json() == gen.get_user({.country = "FR"}, 1).to_json());
    REQUIRE_THROWS_AS(own.get_user({.country = "FR", .device = true}, 1), std::runtime_error);
}

TEST_CASE("the device in JSON", "[device][json]")
{
    const auto gen = make_loaded_generator();
    const auto u = gen.get_user({.country = "FR", .device = true}, 7);
    const auto json = u.to_json();
    const auto device_at = json.find("\"device\":{\"country_code\":\"FR\"");
    REQUIRE(device_at != std::string::npos);
    REQUIRE(device_at > json.find("\"account\":{"));
    REQUIRE(device_at < json.rfind("\"signature\":"));
    REQUIRE(json.find("\"user_agent\":") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Papers and wallet
// ---------------------------------------------------------------------------

namespace
{
std::chrono::year_month_day born_on(const dasmig::user& u)
{
    return std::chrono::year{u.birthday.year} / std::chrono::month{u.birthday.month} /
           std::chrono::day{u.birthday.day};
}

dasmig::identity_request with_documents()
{
    dasmig::identity_request r;
    r.documents = {dasmig::id_document_kind::passport, dasmig::id_document_kind::id_card};
    return r;
}

dasmig::finance_request with_card()
{
    dasmig::finance_request r;
    r.cards = {dasmig::card_kind::debit};
    r.card_numbers = dasmig::card_source::test_number;
    return r;
}
} // namespace

TEST_CASE("no papers or wallet unless the request asks", "[identity][finance]")
{
    const auto gen = make_loaded_generator();
    const auto u = gen.get_user({.country = "BR"}, 3);
    REQUIRE_FALSE(u.papers.has_value());
    REQUIRE_FALSE(u.wallet.has_value());
    REQUIRE(u.to_json().find("\"identity\":") == std::string::npos);
    REQUIRE(u.to_json().find("\"finance\":") == std::string::npos);
    REQUIRE(u.signature().data == gen.data_hash());
}

TEST_CASE("the papers are id-generator's for this person", "[identity]")
{
    const auto gen = make_loaded_generator();
    const auto& ids = dasmig::idg::instance();
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        const auto u = gen.get_user({.country = "IT", .identity = with_documents()}, seed);
        REQUIRE(u.papers.has_value());
        REQUIRE(u.papers->country_code == "IT");
        REQUIRE(u.papers->birth_date == born_on(u));
        REQUIRE(u.papers->bio_sex == u.person_sex());

        auto own = with_documents();
        own.country = "IT";
        own.birth_date = born_on(u);
        own.sex = u.person_sex();
        own.given_names = u.given;
        own.surname = u.family;
        own.residence = dasmig::id_place{.country_code = "IT",
                                         .admin1_code = u.location.admin1_code,
                                         .geonameid = u.location.geonameid};
        const auto alone = ids.get_identity(own, dasmig::core::rng{seed}.split("identity")());
        REQUIRE(u.papers->to_json() == alone.to_json());

        for (const auto& n : u.papers->numbers) REQUIRE(ids.is_valid(n.scheme, n.value));
        REQUIRE(u.signature().data == dasmig::core::hash_combine(gen.data_hash(), ids.data_hash()));
    }
}

TEST_CASE("the wallet is finance-generator's for this person", "[finance]")
{
    const auto gen = make_loaded_generator();
    const auto& banks = dasmig::fng::instance();
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        const auto u = gen.get_user({.country = "DE", .finance = with_card()}, seed);
        REQUIRE(u.wallet.has_value());
        REQUIRE(u.wallet->country_code == "DE");
        REQUIRE(u.wallet->birth_date == born_on(u));

        auto own = with_card();
        own.country = "DE";
        own.birth_date = born_on(u);
        own.given_names = u.given;
        own.surname = u.family;
        const auto alone = banks.get_finance(own, dasmig::core::rng{seed}.split("finance")());
        REQUIRE(u.wallet->to_json() == alone.to_json());

        for (const auto& a : u.wallet->accounts) REQUIRE(banks.is_valid(a.scheme, a.iban));
        REQUIRE(u.signature().data == dasmig::core::hash_combine(gen.data_hash(), banks.data_hash()));
    }
}

TEST_CASE("asking for papers and a wallet changes nothing else", "[identity][finance][seed]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 300; ++seed)
    {
        for (const bool drawn : {false, true})
        {
            dasmig::user_request plain;
            if (!drawn) plain.country = "BR";
            const auto u = gen.get_user(plain, seed);

            auto with = plain;
            with.identity = with_documents();
            with.finance = with_card();
            const auto v = gen.get_user(with, seed);

            // The user's JSON, without the parts asked for and without the signature.
            auto rest = v;
            rest.papers.reset();
            rest.wallet.reset();
            const auto strip = [](const std::string& json) { return json.substr(0, json.rfind(",\"signature\":")); };
            REQUIRE(strip(rest.to_json()) == strip(u.to_json()));
        }
    }
}

TEST_CASE("the signature combines device, identity and finance in that order", "[identity][finance][device]")
{
    const auto gen = make_loaded_generator();
    const auto u = gen.get_user({.country = "FR", .device = true, .identity = with_documents(), .finance = with_card()}, 5);
    auto expected = dasmig::core::hash_combine(gen.data_hash(), dasmig::dvg::instance().data_hash());
    expected = dasmig::core::hash_combine(expected, dasmig::idg::instance().data_hash());
    expected = dasmig::core::hash_combine(expected, dasmig::fng::instance().data_hash());
    REQUIRE(u.signature().data == expected);
}

TEST_CASE("a place code follows the address", "[identity]")
{
    const auto gen = make_loaded_generator();
    // Italy and Serbia code the city, Mexico and Brazil the region; Brazil's
    // address writes an abbreviation, and the code is still the region's.
    int seen = 0;
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        const auto mx = gen.get_user({.country = "MX", .identity = dasmig::identity_request{}}, seed);
        const auto again = gen.get_user({.country = "MX", .identity = dasmig::identity_request{}}, seed + 1000);
        if (!mx.papers->numbers.empty() && !again.papers->numbers.empty() &&
            mx.location.admin1_code == again.location.admin1_code)
        {
            REQUIRE(mx.papers->numbers[0].place_code == again.papers->numbers[0].place_code);
            ++seen;
        }
    }
    REQUIRE(seen > 0);
    const auto br = [&](std::uint64_t seed) { return gen.get_user({.country = "BR", .identity = dasmig::identity_request{}}, seed); };
    std::map<std::string, std::set<std::string>> digits; // by state
    for (std::uint64_t seed = 0; seed < 600; ++seed)
    {
        const auto u = br(seed);
        if (!u.papers->numbers.empty()) digits[u.location.admin1_code].insert(u.papers->numbers[0].place_code);
    }
    REQUIRE(digits.size() > 5);
    for (const auto& [state, codes] : digits) REQUIRE(codes.size() == 1);
}

TEST_CASE("a double surname reaches the document as two surnames", "[identity]")
{
    const auto gen = make_loaded_generator();
    int two = 0;
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        const auto u = gen.get_user({.country = "MX", .identity = with_documents()}, seed);
        const auto space = u.family.find(' ');
        if (space == std::string::npos || u.family.find(' ', space + 1) != std::string::npos) continue;
        if (u.papers->numbers.empty() || u.papers->documents.empty()) continue;
        const auto second = u.family.substr(space + 1);
        if (second[0] < 'A' || second[0] > 'Z') continue;
        ++two;

        // The CURP's third letter is the second surname's first (a surname of its
        // own, not a letter inside "Gamez Arroyo").
        REQUIRE(u.papers->numbers[0].value[2] == second[0]);

        // The MRZ writes both surnames before the given names.
        const auto& line = u.papers->documents[0].mrz.front();
        const auto surnames = line.substr(5, line.find("<<") - 5);
        REQUIRE(surnames.find('<') != std::string::npos);
    }
    REQUIRE(two > 50);
}

TEST_CASE("a patronymic stands in for a missing family name", "[identity][finance]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        const auto u = gen.get_user({.country = "IS", .identity = with_documents(), .finance = with_card()}, seed);
        REQUIRE(u.family.empty());
        const auto& passport = u.papers->documents.front();
        REQUIRE_FALSE(passport.mrz.empty());
        REQUIRE(passport.mrz.front().find("<<") != std::string::npos);
        // The card has the whole name, not only the given one.
        REQUIRE(u.wallet->cards.front().holder.find(' ') != std::string::npos);
    }
}

TEST_CASE("safe reaches the papers and the wallet", "[identity][finance][safe]")
{
    const auto gen = make_loaded_generator();
    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        dasmig::finance_request money;
        money.cards = {dasmig::card_kind::credit};
        const auto u = gen.get_user({.country = "FR", .safe = true, .identity = with_documents(), .finance = money}, seed);
        for (const auto& n : u.papers->numbers) REQUIRE(n.reserved);
        for (const auto& d : u.papers->documents) REQUIRE(d.reserved);
        for (const auto& a : u.wallet->accounts) REQUIRE(a.reserved);
        for (const auto& c : u.wallet->cards) REQUIRE(c.reserved);
    }
    // An explicit real range under safe is finance-generator's error, and reaches the caller.
    dasmig::finance_request bad;
    bad.cards = {dasmig::card_kind::credit};
    bad.card_numbers = dasmig::card_source::network_range;
    REQUIRE_THROWS_AS(gen.get_user({.country = "FR", .safe = true, .finance = bad}, 1), std::invalid_argument);
}

TEST_CASE("the user's values replace the same fields of the request", "[identity][finance]")
{
    const auto gen = make_loaded_generator();
    auto papers = with_documents();
    papers.country = "JP";
    papers.surname = "Someone Else";
    papers.reference_date = std::chrono::year{2000} / 1 / 1;
    dasmig::finance_request money;
    money.country = "JP";
    money.birth_date = std::chrono::year{2000} / 1 / 1;
    const auto u = gen.get_user({.country = "FR", .identity = papers, .finance = money}, 9);
    REQUIRE(u.papers->country_code == "FR");
    REQUIRE(u.wallet->country_code == "FR");
    REQUIRE(u.wallet->birth_date == born_on(u));
    REQUIRE(u.papers->documents.front().mrz.front().find("SOMEONE") == std::string::npos);
}

TEST_CASE("what the request leaves to the library is used as given", "[identity][finance]")
{
    const auto gen = make_loaded_generator();
    dasmig::identity_request papers;
    papers.schemes = std::vector<std::string>{};
    papers.documents = {dasmig::id_document_kind::passport};
    dasmig::finance_request money;
    money.accounts = {dasmig::account_kind::current, dasmig::account_kind::savings};
    money.network = "visa";
    money.card_numbers = dasmig::card_source::test_number;
    money.cards = {dasmig::card_kind::debit};
    const auto u = gen.get_user({.country = "US", .identity = papers, .finance = money}, 2);
    REQUIRE(u.papers->numbers.empty());
    REQUIRE(u.papers->documents.size() == 1);
    REQUIRE(u.wallet->accounts.size() == 2);
    REQUIRE(u.wallet->cards.front().network == "visa");
}

TEST_CASE("the parts come from the generator's own instances", "[identity][finance][loading]")
{
    dasmig::idg no_ids;
    dasmig::fng no_banks;
    const auto gen = make_loaded_generator();
    dasmig::usg own{dasmig::usg::sources{.identity = &no_ids, .finance = &no_banks}};
    REQUIRE(own.load(dasmig::dataset::lite));
    REQUIRE(own.get_user({.country = "FR"}, 1).to_json() == gen.get_user({.country = "FR"}, 1).to_json());
    REQUIRE_THROWS(own.get_user({.country = "FR", .identity = dasmig::identity_request{}}, 1));
    REQUIRE_THROWS(own.get_user({.country = "FR", .finance = dasmig::finance_request{}}, 1));
}

TEST_CASE("the parts in JSON", "[identity][finance][json]")
{
    const auto gen = make_loaded_generator();
    const auto json = gen.get_user({.country = "FR", .device = true, .identity = with_documents(), .finance = with_card()}, 7).to_json();
    const auto device_at = json.find("\"device\":{\"country_code\":\"FR\"");
    const auto identity_at = json.find("\"identity\":{");
    const auto finance_at = json.find("\"finance\":{");
    REQUIRE(device_at != std::string::npos);
    REQUIRE(identity_at != std::string::npos);
    REQUIRE(finance_at != std::string::npos);
    REQUIRE(device_at < identity_at);
    REQUIRE(identity_at < finance_at);
    REQUIRE(finance_at < json.rfind("\"signature\":"));
}

TEST_CASE("every supported country can have papers and a wallet", "[identity][finance]")
{
    const auto gen = make_loaded_generator();
    for (const auto& code : gen.supported_countries())
    {
        if (code == "KR") continue; // the lite tier has no male Korean names
        const auto u = gen.get_user({.country = code, .device = true, .identity = with_documents(), .finance = with_card()}, 21);
        REQUIRE(u.papers.has_value());
        REQUIRE_FALSE(u.papers->documents.empty());
        REQUIRE_FALSE(u.wallet->accounts.empty());
    }
}

// ---------------------------------------------------------------------------
// Conversion and JSON
// ---------------------------------------------------------------------------

TEST_CASE("string conversion and streaming give the summary", "[conversion]")
{
    const auto gen = make_loaded_generator();
    const auto u = gen.get_user({.country = "BR"}, 11);
    const std::string s = u;
    REQUIRE(s == u.to_string());
    REQUIRE(s.find(u.name) == 0);
    REQUIRE(s.find(u.account.username) != std::string::npos);
    REQUIRE(s.find(u.mail.address()) != std::string::npos);
    REQUIRE(s.ends_with("Brazil"));
    std::ostringstream os;
    os << u;
    REQUIRE(os.str() == s);
}

TEST_CASE("to_json holds every part and the signature", "[json]")
{
    const auto gen = make_loaded_generator();
    const auto u = gen.get_user({.country = "FR"}, 7);
    const auto json = u.to_json();
    for (const auto* key : {"\"name\":", "\"given\":", "\"family\":", "\"sex\":", "\"age\":", "\"country\":{\"code\":\"FR\"",
                            "\"birth\":{", "\"address\":{", "\"phone\":{", "\"email\":{", "\"account\":{\"username\":",
                            "\"password\":", "\"created\":", "\"last_login\":", "\"status\":", "\"platform\":",
                            "\"locale\":", "\"timezone\":", "\"currency\":\"EUR\"", "\"currency_digits\":2", "\"signature\":{\"seed\":"})
    {
        INFO(key);
        REQUIRE(json.find(key) != std::string::npos);
    }
    REQUIRE(json.front() == '{');
    REQUIRE(json.back() == '}');
    REQUIRE(json.find(u.signature().to_string().substr(0, 16)) != std::string::npos);
}

// ---------------------------------------------------------------------------
// Known answers (rule 2): the same seed and data give these exact results on
// every compiler and platform. A change here is a change of output and needs
// a major version.
// ---------------------------------------------------------------------------

TEST_CASE("known answers for fixed seeds", "[kat]")
{
    const auto gen = make_loaded_generator();
    // Fixed countries; the account data is this library's, the contacts and
    // names those of the pinned library releases.
    const auto u = gen.get_user({.country = "BR", .person_sex = dasmig::sex::female}, 42);
    REQUIRE(u.name == "Yara Carvalho Souza");
    REQUIRE(u.birthday.age == 26);
    REQUIRE(u.account.username == "iam_ycarvalho63");
    REQUIRE(u.mail.address() == "iam_ycarvalho@gmail.com");
    REQUIRE(u.mobile.e164() == "+5542943681103");
    REQUIRE(u.location.postcode == "10175-667");
    REQUIRE(u.account.password == "684*+!ZsY=zc43");
    REQUIRE(u.account.created == "2019-02-05T07:32:31Z");
    REQUIRE(u.account.last_login == "2023-05-22T04:07:35Z");
    REQUIRE(u.account.status == "active");
    REQUIRE(u.account.platform == "android-phone");
    REQUIRE(u.account.locale == "pt-BR");
    REQUIRE(u.account.timezone == "America/Fortaleza");
    REQUIRE(u.account.currency == "BRL");
    REQUIRE(u.account.currency_digits == 2);

    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint64_t s = 0; s < 2000; ++s)
    {
        h = dasmig::core::hash_combine(h, dasmig::core::fnv1a64(gen.get_user({}, s).to_json()));
    }
    INFO(std::format("{:016x}", h));
    REQUIRE(h == 0x226634c64ddd1b16ULL);
}

TEST_CASE("known answers with a device", "[kat][device]")
{
    const auto gen = make_loaded_generator();
    // The same user as above, and the device of its last login (2023-05-22),
    // from the pinned device-generator release.
    const auto u = gen.get_user({.country = "BR", .person_sex = dasmig::sex::female, .device = true}, 42);
    REQUIRE(u.name == "Yara Carvalho Souza");
    REQUIRE(u.account.last_login == "2023-05-22T04:07:35Z");
    REQUIRE(u.client->to_string() == "Chrome 113 on Android 13 (Samsung Galaxy A52), 2801:9c:db5a:14cb:9650:dec6:5843:7cca");
    REQUIRE(u.client->user_agent ==
            "Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/113.0.0.0 Mobile Safari/537.36");
    REQUIRE(u.client->accept_language == "pt-BR,pt;q=0.9,en;q=0.8");

    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint64_t s = 0; s < 500; ++s)
    {
        h = dasmig::core::hash_combine(h, dasmig::core::fnv1a64(gen.get_user({.device = true}, s).to_json()));
    }
    INFO(std::format("{:016x}", h));
    REQUIRE(h == 0x84119dc1e0c8ad2bULL);
}

TEST_CASE("known answers with papers and a wallet", "[kat][identity][finance]")
{
    const auto gen = make_loaded_generator();
    // The same user as above, with the numbers and documents of the pinned
    // id-generator release and the account and card of finance-generator's.
    dasmig::identity_request papers;
    papers.documents = {dasmig::id_document_kind::passport};
    dasmig::finance_request money;
    money.cards = {dasmig::card_kind::debit};
    money.card_numbers = dasmig::card_source::test_number;
    const auto u = gen.get_user({.country = "BR", .person_sex = dasmig::sex::female, .identity = papers, .finance = money}, 42);
    REQUIRE(u.name == "Yara Carvalho Souza");
    REQUIRE(u.papers->numbers.at(0).formatted == "300.274.655-00");
    REQUIRE(u.papers->documents.at(0).number == "LI206064");
    REQUIRE(u.papers->documents.at(0).mrz.at(0) == "P<BRACARVALHO<SOUZA<<YARA<<<<<<<<<<<<<<<<<<<");
    REQUIRE(u.wallet->accounts.at(0).iban == "BR0800556603000500011351977C1");
    REQUIRE(u.wallet->cards.at(0).holder == "YARA CARVALHO SOUZA");

    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint64_t s = 0; s < 500; ++s)
    {
        h = dasmig::core::hash_combine(h, dasmig::core::fnv1a64(gen.get_user({.identity = papers, .finance = money}, s).to_json()));
    }
    INFO(std::format("{:016x}", h));
    REQUIRE(h == 0xbe65b3ef0353fc43ULL);
}

// ---------------------------------------------------------------------------
// Extending the data
// ---------------------------------------------------------------------------

TEST_CASE("extend overrides, adds and removes rows", "[extend]")
{
    auto gen = make_loaded_generator();
    const auto hash = gen.data_hash();

    // A layer that makes every account suspended.
    gen.extend_from_string(dasmig::user_table::statuses,
                           "key\tlabel\tlogin\tweight\n"
                           "active\tActive\trecent\t0\n"
                           "inactive\tInactive\tstale\t0\n"
                           "unverified\tUnverified\tnever\t0\n"
                           "suspended\tSuspended\trecent\t1\n",
                           "all-suspended.tsv");
    REQUIRE(gen.data_hash() != hash);
    REQUIRE(gen.layers(dasmig::user_table::statuses).size() == 2);
    for (std::uint64_t seed = 0; seed < 50; ++seed)
    {
        REQUIRE(gen.get_user({}, seed).account.status == "suspended");
    }

    // A new status and a new platform.
    gen.extend_from_string(dasmig::user_table::statuses, "key\tlabel\tlogin\tweight\npending\tPending\tnever\t100000\n", "p.tsv");
    gen.extend_from_string(dasmig::user_table::platforms,
                           "key\tlabel\tdevice\tos\tweight\nwatch\tSmartwatch\twearable\twatchOS\t100000\n", "w.tsv");
    const auto u = gen.get_user({.country = "PL"}, 3); // a country with no platform overrides
    REQUIRE(u.account.status == "pending");
    REQUIRE(u.account.platform == "watch");
    REQUIRE(u.account.device == "wearable");
    REQUIRE(u.account.last_login.empty());

    // Removing a row.
    gen.extend_from_string(dasmig::user_table::platforms, "key\tlabel\tdevice\tos\tweight\tdisabled\nwatch\tx\tx\tx\t1\t1\n", "r.tsv");
    for (std::uint64_t seed = 0; seed < 50; ++seed)
    {
        REQUIRE(gen.get_user({.country = "PL"}, seed).account.platform != "watch");
    }
}

TEST_CASE("a bad layer is rejected and leaves the data unchanged", "[extend][errors]")
{
    auto gen = make_loaded_generator();
    const auto hash = gen.data_hash();
    const auto layers = gen.layers(dasmig::user_table::statuses).size();

    REQUIRE_THROWS_AS(gen.extend_from_string(dasmig::user_table::statuses,
                                             "key\tlabel\tlogin\tweight\nx\tX\tsometimes\t1\n", "bad.tsv"),
                      dasmig::core::tsv_error); // login is not recent, stale or never
    REQUIRE_THROWS_AS(gen.extend_from_string(dasmig::user_table::statuses,
                                             "key\tlabel\tlogin\tweight\nx\tX\trecent\t-1\n", "bad.tsv"),
                      dasmig::core::tsv_error); // negative weight
    REQUIRE_THROWS_AS(gen.extend_from_string(dasmig::user_table::platform_shares,
                                             "country\tkey\tweight\nbr\tios-phone\t1\n", "bad.tsv"),
                      dasmig::core::tsv_error); // not an upper case country code
    REQUIRE_THROWS_AS(gen.extend_from_string(dasmig::user_table::platforms,
                                             "key\tlabel\tdevice\tos\tweight\nx\t\tphone\tos\t1\n", "bad.tsv"),
                      dasmig::core::tsv_error); // empty label
    REQUIRE_THROWS_AS(gen.extend(std::filesystem::path{"not-a-user-table.tsv"}), std::invalid_argument);

    REQUIRE(gen.data_hash() == hash);
    REQUIRE(gen.layers(dasmig::user_table::statuses).size() == layers);
}

#ifdef DASMIG_USER_EMBED
TEST_CASE("compiled-in data needs no files and matches them", "[embed]")
{
    const auto old_cwd = std::filesystem::current_path();
    const auto tmp = std::filesystem::temp_directory_path() / "usg_embed_dir";
    std::filesystem::create_directories(tmp);
    std::filesystem::current_path(tmp);
    dasmig::usg gen;
    const bool loaded = gen.load(dasmig::dataset::lite);
    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(tmp);

    REQUIRE(loaded);
    REQUIRE(gen.layers(dasmig::user_table::platforms).front().source == "embedded:lite/platforms.tsv");

    dasmig::usg from_files;
    from_files.load(std::filesystem::path{"resources/lite"});
    if (!from_files.has_account_data()) from_files.load(std::filesystem::path{"../resources/lite"});
    REQUIRE(gen.data_hash() == from_files.data_hash());
    REQUIRE(gen.get_user({}, 1).to_json() == from_files.get_user({}, 1).to_json());
}
#endif

// ---------------------------------------------------------------------------
// Batch generation
// ---------------------------------------------------------------------------

TEST_CASE("user batch: items equal the single calls for any thread count", "[batch]")
{
    auto gen = make_loaded_generator();
    const auto request = dasmig::user_request{};
    const auto batch = gen.get_user_batch(request, {.count = 100, .threads = 1}, 77);
    REQUIRE(batch.size() == 100);
    for (std::uint64_t i = 0; i < batch.size(); ++i)
    {
        REQUIRE(batch[i].to_json() == gen.get_user(request, dasmig::core::item_seed(77, i)).to_json());
    }

    // A range is computed directly, on any number of threads.
    const auto tail = gen.get_user_batch(request, {.count = 50, .first = 50, .threads = 4}, 77);
    REQUIRE(tail.size() == 50);
    for (std::size_t i = 0; i < tail.size(); ++i)
    {
        REQUIRE(tail[i].to_json() == batch[50 + i].to_json());
    }
    for (const std::size_t threads : {2U, 7U, 0U})
    {
        const auto other = gen.get_user_batch(request, {.count = 100, .threads = threads}, 77);
        REQUIRE(other.size() == batch.size());
        for (std::size_t i = 0; i < other.size(); ++i)
        {
            REQUIRE(other[i].to_json() == batch[i].to_json());
        }
    }

    // Without a seed the batch still has the requested size.
    REQUIRE(gen.get_user_batch(request, {.count = 5}).size() == 5);
}

TEST_CASE("user batch: streaming gives the same items in order", "[batch]")
{
    auto gen = make_loaded_generator();
    const auto request = dasmig::user_request{};
    const auto batch = gen.get_user_batch(request, {.count = 150, .first = 10, .threads = 3}, 5);
    std::uint64_t next = 10;
    std::size_t seen = 0;
    gen.for_each_user(request, {.count = 150, .first = 10, .threads = 3}, 5,
                      [&](std::uint64_t index, const dasmig::user& item)
                      {
                          REQUIRE(index == next);
                          REQUIRE(item.to_json() == batch[seen].to_json());
                          ++next;
                          ++seen;
                      });
    REQUIRE(seen == batch.size());
}

TEST_CASE("user batch: unique emails", "[batch]")
{
    auto gen = make_loaded_generator();
    const auto rule = dasmig::core::unique_rule::by<dasmig::user>([](const dasmig::user& u) { return u.mail.address(); });
    const auto batch = gen.get_user_batch({.country = "US"}, {.count = 500, .unique = rule}, 9);
    std::set<std::string> seen;
    for (const auto& u : batch) seen.insert(u.mail.address());
    REQUIRE(seen.size() == batch.size());
}

TEST_CASE("user batch: known answer of 1,000 items", "[batch][kat]")
{
    auto gen = make_loaded_generator();
    const auto request = dasmig::user_request{};
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    gen.for_each_user(request, {.count = 1000, .threads = 4}, 1,
                      [&](std::uint64_t, const dasmig::user& item)
                      { hash = dasmig::core::hash_combine(hash, dasmig::core::fnv1a64(item.to_json())); });
    INFO(std::format("{:016x}", hash));
    REQUIRE(hash == 0x1d92d9c5e11ecd1aULL);
}
