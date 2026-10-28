// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Diego Dasso Migotto

#ifndef DASMIG_USERGEN_HPP
#define DASMIG_USERGEN_HPP

#include "core.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <dasmig/addressgen.hpp>
#include <dasmig/birthgen.hpp>
#include <dasmig/countrygen.hpp>
#include <dasmig/devicegen.hpp>
#include <dasmig/emailgen.hpp>
#include <dasmig/namegen.hpp>
#include <dasmig/nicknamegen.hpp>
#include <dasmig/phonegen.hpp>

#ifdef DASMIG_USER_EMBED
#include "usergen_data.hpp"
#endif

/// @file usergen.hpp
/// @brief User Generator library — synthetic user records for apps and test data, composed from the other dasmig generators for C++23.
/// @author Diego Dasso Migotto (diegomigotto at hotmail dot com)
/// @see See doc/usage.md for the narrative tutorial.

DASMIG_CORE_REQUIRE(1, 2);

namespace dasmig
{

/// @brief How a password is drawn.
enum class password_style : std::uint8_t
{
    random, ///< Characters drawn alike from the allowed classes.
    human   ///< What people choose: a common word with digits, a year or a symbol, a name, a keyboard walk.
};

/// @brief The rules a generated password follows.
///
/// The defaults are a 12 to 16 character password with a lower case letter, an
/// upper case letter, a digit and a symbol in it. The `human` style builds the
/// password the way people do (see doc/data.md) and then bends it to these
/// rules, as a sign-up form would: it drops a character the rules do not allow,
/// adds the digit or the symbol that is missing and pads a short one, so with
/// the default rules few passwords stay as plain as `dragon123`. Loosen the
/// rules (for example `{.min_length = 6, .max_length = 20, .require_each =
/// false}`) for a mix closer to the common-password lists.
struct password_policy
{
    std::size_t min_length{12}; ///< Shortest password, in characters.
    std::size_t max_length{16}; ///< Longest password; the length is drawn between the two.
    bool lowercase{true};       ///< Allow `a`-`z`.
    bool uppercase{true};       ///< Allow `A`-`Z`.
    bool digits{true};          ///< Allow `0`-`9`.
    bool symbols{true};         ///< Allow punctuation (`!@#$%^&*-_=+?`).
    bool require_each{true};    ///< Include at least one character of every allowed class.
    bool avoid_ambiguous{false}; ///< Leave out `0 O 1 l I |`, which are easy to misread.
    password_style style{password_style::random}; ///< How the password is drawn; `random` keeps the seeds of earlier releases.
};

/// @brief What a user call asks for.
///
/// Every generation call takes one; `user_request{}` asks for the defaults.
/// Choices are never stored on the generator, so one call cannot change the
/// next. Every field is optional: what is left out is drawn, and what is given
/// conditions the draws that depend on it (the country sets the name, the
/// address, the phone, the email and the account's locale; the birth sets the
/// age).
struct user_request
{
    /// @brief ISO 3166-1 alpha-2 code (any case). Unset draws one of the
    ///        countries every part has data for, weighted by population.
    std::optional<std::string> country{}; // NOLINT(readability-redundant-member-init)

    /// @brief Biological sex; unset draws it from the birth.
    std::optional<sex> person_sex{}; // NOLINT(readability-redundant-member-init)

    /// @brief Inclusive age range in completed years. Users are adults by
    ///        default; the lowest age must be at least `sign_up_age`.
    age_range ages{.min = 18, .max = 80};

    /// @brief The date ages and account dates are computed at. The default is
    ///        birth-generator's (2023-07-01); the library never reads the clock.
    std::chrono::year_month_day reference_date{birth_request::default_reference_date};

    /// @brief Make the contacts unable to reach anyone: reserved email domains
    ///        and, where a country has one, a phone number from the range
    ///        regulators set aside for fiction.
    bool safe{false};

    /// @brief The password rules.
    password_policy password{};

    /// @brief The youngest age at which a person signs up, in completed years.
    ///        An account is created after the person reaches it.
    std::uint8_t sign_up_age{13};

    /// @brief The earliest date an account can be created, for example the
    ///        launch of the service. Unset is ten years before the reference date.
    std::optional<std::chrono::year_month_day> service_start{}; // NOLINT(readability-redundant-member-init)

    /// @brief Force an account status by its key (`active`, `inactive`,
    ///        `unverified`, `suspended` in the shipped data). Unset draws one.
    std::optional<std::string> status{}; // NOLINT(readability-redundant-member-init)

    /// @brief Add the device the user last signed in from (user::client),
    ///        from device-generator: drawn with the account's country,
    ///        platform and locale, at the last login (the sign-up time for an
    ///        account that never logged in), with a documentation-range IP
    ///        address when `safe`. Off by default. Turning it on changes
    ///        nothing else in a user, except that a drawn country is one
    ///        device-generator has too.
    bool device{false};
};

/// @brief The online side of a user: the login, the dates, the device and the
///        regional settings.
struct user_account
{
    std::string username;       ///< From nickname-generator's `username` style, in the person's name and birth year.
    std::string password;       ///< Plain text, to the request's policy; test data, never a real credential.
    std::string created;        ///< Sign-up time, ISO 8601 UTC (`2021-03-14T09:26:53Z`).
    std::string last_login;     ///< Last login, ISO 8601 UTC; empty when the account has never logged in.
    std::string status;         ///< Status key (`active`, `inactive`, ...).
    std::string status_label;   ///< Display label of the status.
    std::string platform;       ///< Platform key (`ios-phone`, ...).
    std::string platform_label; ///< Display label of the platform.
    std::string device;         ///< `phone`, `desktop`, `tablet` or `other`.
    std::string os;             ///< Operating system name.
    std::string locale;         ///< BCP 47 tag (`pt-BR`).
    std::string timezone;       ///< IANA name (`America/Sao_Paulo`), or a UTC offset (`UTC+03:00`) where no zone is listed.
    std::string currency;       ///< ISO 4217 code of the country (`BRL`); empty where country-generator has none.
};

/// @brief Return type for user generation.
///
/// Holds the typed result of every library in the pipeline. Supports
/// implicit conversion to std::string (a one-line summary), streaming via
/// operator<< and JSON output via to_json().
class user
{
  public:
    std::string name;          ///< Full name (UTF-8), from the country's culture and the sex.
    std::string given;         ///< The given name.
    std::string family;        ///< The family name; empty when the name has one part.
    std::string country_code;  ///< ISO 3166-1 alpha-2.
    std::string country_name;  ///< Common name of the country.
    dasmig::birth birthday;    ///< Birth date, completed-year age and sex.
    dasmig::address location;  ///< The postal address, in the country.
    dasmig::phone mobile;      ///< A phone number of the country.
    dasmig::email mail;        ///< An email address, in the person's name.
    user_account account;      ///< The login, dates, device and regional settings.
    std::optional<dasmig::device> client; ///< The device of the last sign-in; only with user_request::device.

    /// @brief Retrieve the random seed used to generate this user.
    /// @return The call seed; pass it to get_user(request, seed) with
    ///         the same request and data to replay this user.
    [[nodiscard]] std::uint64_t seed() const { return _signature.seed; }

    /// @brief The seed and the data this user was generated from.
    /// @return The replay signature; `data` combines the data hashes of the
    ///         libraries and this one's (and device-generator's, for a user
    ///         with a device).
    [[nodiscard]] const core::seed_signature& signature() const
    {
        return _signature;
    }

    /// @brief Biological sex of the user.
    /// @return The sex drawn with the birth.
    [[nodiscard]] sex person_sex() const { return birthday.bio_sex; }

    /// @brief Age in completed years at the request's reference date.
    /// @return The age.
    [[nodiscard]] unsigned age() const { return birthday.age; }

    /// @brief One-line summary: name, username, email and country.
    /// @return For example `Ana Silva (ana.silva92) <ana.silva92@gmail.com>, Brazil`.
    [[nodiscard]] std::string to_string() const
    {
        return name + " (" + account.username + ") <" + mail.address() + ">, " + country_name;
    }

    /// @brief Write this user as a JSON object.
    ///
    /// Each library's result is written as that library writes it (`birth`,
    /// `address`, `phone`, `email`), next to `name`, `given`, `family`, `sex`,
    /// `age`, `country`, `account` and `signature`. `account` has `last_login`
    /// as `null` for an account that never logged in. With
    /// user_request::device, `device` follows `account`, as device-generator
    /// writes it; without, the key is left out.
    /// @param w Writer positioned where a value is allowed.
    void to_json(core::json_writer& w) const
    {
        w.begin_object()
            .field("name", name)
            .field("given", given)
            .field("family", family)
            .field("sex", core::sex_name(person_sex()))
            .field("age", age());
        w.key("country").begin_object().field("code", country_code).field("name", country_name).end_object();
        w.key("birth");
        birthday.to_json(w);
        w.key("address");
        location.to_json(w);
        w.key("phone");
        mobile.to_json(w);
        w.key("email");
        mail.to_json(w);
        w.key("account")
            .begin_object()
            .field("username", account.username)
            .field("password", account.password)
            .field("created", account.created);
        w.key("last_login");
        if (account.last_login.empty()) w.value(nullptr);
        else w.value(account.last_login);
        w.field("status", account.status)
            .field("platform", account.platform)
            .field("device", account.device)
            .field("os", account.os)
            .field("locale", account.locale)
            .field("timezone", account.timezone)
            .field("currency", account.currency)
            .end_object();
        if (client)
        {
            w.key("device");
            client->to_json(w);
        }
        w.key("signature");
        _signature.to_json(w);
        w.end_object();
    }

    /// @brief This user as a JSON document.
    /// @return Compact JSON.
    [[nodiscard]] std::string to_json() const
    {
        core::json_writer w;
        to_json(w);
        return w.str();
    }

    /// @brief Implicit conversion to std::string.
    /// @return Summary string.
    operator std::string() const // NOLINT(hicpp-explicit-conversions)
    {
        return to_string();
    }

    /// @brief Stream the summary to an output stream.
    /// @param os Output stream.
    /// @param v The user to stream.
    /// @return The stream.
    friend std::ostream& operator<<(std::ostream& os, const user& v)
    {
        os << v.to_string();
        return os;
    }

  private:
    core::seed_signature _signature;
    friend class usg;
};

/// @brief The tables of the user data, for extending it.
enum class user_table : std::uint8_t
{
    platforms,       ///< `platforms.tsv`: the devices and their global shares.
    platform_shares, ///< `platform_shares.tsv`: per-country shares, in percent, that replace the global weights.
    statuses,        ///< `statuses.tsv`: the account statuses, their shares and how they log in.
    languages,       ///< `languages.tsv`: language names to codes, for countries without a locale row.
    locales,         ///< `locales.tsv`: the locale tags of each country.
    timezones,       ///< `timezones.tsv`: the time zones of each country.
    password_words,  ///< `password_words.tsv`: the words and whole passwords of the `human` password style.
    password_patterns ///< `password_patterns.tsv`: how the `human` style puts a password together.
};

/// @brief User Generator: composes synthetic user records.
///
/// A user is the identity core (a country, a birth, a name) and an online
/// identity on top of it: a username, an email, a phone number, an address and
/// an account with a password, sign-up and last-login times, a status, a
/// platform and regional settings. The country is drawn once and passed to
/// every part, so they agree. Each part gets its own call seed, split from the
/// user's seed by name (`"country"`, `"birth"`, `"name"`, `"username"`,
/// `"email"`, `"phone"`, `"address"`, `"password"`, `"dates"`, `"status"`,
/// `"platform"`, `"locale"`, `"timezone"`, `"device"`), so the user's seed replays all of
/// it, and forcing one choice (a country, a status) does not shift the draws
/// of the others. Output is identical on every compiler and platform
/// (dasmig-core).
///
/// The generator composes the light libraries directly and does not use
/// persona-generator: a user carries no body, traits, interests, job or city.
/// It holds the data of the account only (platforms, statuses, locales and
/// time zones); the rest comes from the instances of the seven libraries it
/// was built on (the singletons by default), so their data, their layers and
/// their `lite` or `full` tiers are the user's. A device, when a request asks
/// for one, comes the same way from device-generator.
///
/// @par Thread safety
/// get_user(request, seed) is const and only calls const members of the
/// libraries, so it may run concurrently. get_user(request) draws from the
/// generator's engine and needs external synchronisation on the same instance;
/// so do load() and extend().
class usg
{
  public:
    /// @brief The library instances a user is drawn from.
    struct sources
    {
        cntg* country{&cntg::instance()};     ///< Countries: names, currency, languages and time zones.
        bthg* birth{&bthg::instance()};       ///< Birth dates and ages.
        ng* name{&ng::instance()};            ///< Names.
        nng* nickname{&nng::instance()};      ///< Usernames.
        adg* address{&adg::instance()};       ///< Postal addresses.
        phg* phone{&phg::instance()};         ///< Phone numbers.
        emg* email{&emg::instance()};         ///< Email addresses.
        dvg* device{nullptr}; ///< Devices, for user_request::device; null uses dvg::instance(), loaded when a user first asks for a device.
    };

    /// @brief Default constructor — creates a generator with no account data,
    ///        drawing from the singletons of the seven libraries.
    usg() = default;

    /// @brief A generator with no account data that draws from the given
    ///        library instances (which must outlive it).
    /// @param from The instances to use.
    explicit usg(sources from) : _from{from} {}

    usg(const usg&) = delete;            ///< Not copyable.
    usg& operator=(const usg&) = delete; ///< Not copyable.
    usg(usg&&) noexcept = default;       ///< Movable.
    usg& operator=(usg&&) noexcept = default; ///< Movable.
    ~usg() = default;                    ///< Destructor.

    /// @brief Access the global singleton instance.
    ///
    /// Loads the `lite` tier (or `full` if `lite` is missing) of the account
    /// data on first access, from compiled-in data or an auto-probed
    /// `resources/`.
    /// @return The singleton.
    static usg& instance()
    {
        static usg inst{auto_probe_tag{}};
        return inst;
    }

    // -- Generation -------------------------------------------------------

    /// @brief Generate a user with a new random seed.
    /// @param request What to ask for; `{}` for the defaults.
    /// @return The user; seed() replays it.
    /// @throws std::runtime_error If this generator or a library has no data.
    /// @throws std::invalid_argument If the request is invalid, or a country
    ///         asked for is missing from a library.
    [[nodiscard]] user get_user(const user_request& request)
    {
        return get_user(request, draw_seed_());
    }

    /// @brief Generate the user for a call seed. Deterministic; does not
    ///        touch the generator's engine.
    /// @param request What to ask for.
    /// @param call_seed Any 64-bit seed.
    /// @return The same user for the same seed, request and data.
    /// @throws std::runtime_error If this generator or a library has no data.
    /// @throws std::invalid_argument If the request is invalid, or a country
    ///         asked for is missing from a library.
    [[nodiscard]] user get_user(const user_request& request, std::uint64_t call_seed) const
    {
        require_data_();
        check_request_(request);
        if (request.password.style == password_style::human && !_model.passwords.pattern_pick)
            throw std::runtime_error("user-generator: the human password style needs password_patterns.tsv");
        const core::rng call{call_seed};

        if (request.country)
        {
            return build_(request, upper_(*request.country), call, call_seed);
        }

        // Draw countries by population until one has data in every part. The
        // contact libraries are checked first, which is cheap; the name and the
        // birth can still refuse a country (a culture without names for one sex).
        auto draws = call.split("country");
        std::string last = "user-generator: no country has data in every library";
        const auto device_countries = request.device ? device_source_().countries() : std::vector<std::string>{};
        for (int i = 0; i < max_country_attempts_; ++i)
        {
            const auto c = _from.country->get_country({}, draws());
            if (!in_contact_libraries_(c.cca2)) continue;
            if (request.device && !std::ranges::binary_search(device_countries, c.cca2)) continue;
            try
            {
                return build_(request, c.cca2, call, call_seed);
            }
            catch (const std::invalid_argument& e)
            {
                last = e.what();
            }
        }
        throw std::invalid_argument(last);
    }

    // -- Batch generation -------------------------------------------------

    /// @brief Generate a batch of users from a seed drawn like get_user()
    ///        draws its own.
    ///
    /// Item `i` is `get_user(request, core::item_seed(batch_seed, options.first + i))`
    /// for the drawn batch seed; see the seeded overload.
    ///
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @return The results, in index order.
    [[nodiscard]] std::vector<user> get_user_batch(const user_request& request,
                                                  const core::batch_options& options)
    {
        return get_user_batch(request, options, draw_seed_());
    }

    /// @brief Generate a batch of users, deterministically.
    ///
    /// Item `i` is exactly `get_user(request, core::item_seed(batch_seed,
    /// options.first + i))`, so an item replays on its own, a range can be
    /// generated directly, two batches that follow each other equal one, and
    /// the output does not depend on `options.threads`.
    ///
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness (see
    ///        core::unique_rule; the rule must be made for `user`, for example
    ///        by `mail.address()`).
    /// @param batch_seed Seed of the whole batch.
    /// @return The results, in index order.
    /// @throws std::invalid_argument If the request or options are invalid.
    /// @throws std::runtime_error If no data has been loaded, or a unique
    ///         batch runs out of attempts.
    [[nodiscard]] std::vector<user> get_user_batch(const user_request& request,
                                                  const core::batch_options& options,
                                                  std::uint64_t batch_seed) const
    {
        return core::run_batch(options, batch_seed, [&](std::uint64_t item_seed)
                               { return get_user(request, item_seed); });
    }

    /// @brief Stream a batch of users to a sink without holding the whole
    ///        batch.
    ///
    /// Generates the same items as get_user_batch() and calls
    /// `sink(index, item)` in index order on the calling thread; `index`
    /// includes `options.first`.
    ///
    /// @tparam Sink Callable taking `(std::uint64_t, user&)`; for example
    ///         core::jsonl_writer or core::csv_writer.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @param batch_seed Seed of the whole batch.
    /// @param sink Receives the items.
    template <typename Sink>
    void for_each_user(const user_request& request, const core::batch_options& options,
                       std::uint64_t batch_seed, Sink&& sink) const
    {
        core::for_each_batch(options, batch_seed,
                             [&](std::uint64_t item_seed) { return get_user(request, item_seed); },
                             std::forward<Sink>(sink));
    }

    /// @brief Stream a batch of users with a batch seed drawn like
    ///        get_user() draws its own.
    /// @tparam Sink Callable taking `(std::uint64_t, user&)`.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @param sink Receives the items.
    template <typename Sink>
    void for_each_user(const user_request& request, const core::batch_options& options,
                       Sink&& sink)
    {
        for_each_user(request, options, draw_seed_(), std::forward<Sink>(sink));
    }

    // -- Data management --------------------------------------------------

    /// @brief The countries a random user is drawn from.
    ///
    /// The countries for which the address, phone and email libraries and
    /// country-generator have data and name-generator has a culture. A country
    /// can be asked for by `user_request::country` outside this list only if the
    /// libraries do have it; one without names for a sex, or without a birth
    /// table, is refused when asked for and skipped when drawn.
    /// @return ISO 3166-1 alpha-2 codes, sorted.
    [[nodiscard]] std::vector<std::string> supported_countries() const
    {
        std::vector<std::string> out;
        for (const auto& code : _from.address->countries())
        {
            if (in_contact_libraries_(code) && has_name_culture_(code) &&
                !_from.country->find_country(code).cca2.empty())
            {
                out.push_back(code);
            }
        }
        std::ranges::sort(out);
        return out;
    }

    /// @brief Check whether the account data and every library has data.
    /// @return True when this generator can generate.
    [[nodiscard]] bool has_data() const
    {
        return has_account_data_() && _from.country->has_data() && _from.birth->has_data() &&
               _from.name->has_resources() && _from.address->has_data() && _from.phone->has_data() &&
               _from.email->has_data();
    }

    /// @brief Whether the account tables (platforms and statuses) are loaded.
    /// @return True when both are non-empty.
    [[nodiscard]] bool has_account_data() const { return has_account_data_(); }

    /// @brief Load the account data from a resource directory, replacing all
    ///        account data and layers loaded before.
    ///
    /// The directory must contain `platforms.tsv` and `statuses.tsv`; the other
    /// tables are read when present. A path without them is ignored. The
    /// libraries this one draws from are loaded on their own.
    ///
    /// @param dir Directory such as `resources/lite`.
    /// @throws core::tsv_error If a file is malformed; the data is then
    ///         unchanged.
    void load(const std::filesystem::path& dir)
    {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec) ||
            !std::filesystem::is_regular_file(dir / file_name_(user_table::platforms), ec) ||
            !std::filesystem::is_regular_file(dir / file_name_(user_table::statuses), ec))
        {
            return;
        }
        apply_(read_directory_(dir), core::layer_mode::replace, true);
    }

    /// @brief Load a dataset tier of the account data, replacing all account
    ///        data and layers loaded before.
    ///
    /// Uses the compiled-in copy when the library is built with
    /// `DASMIG_USER_EMBED`, otherwise searches `resources/`, `../resources`,
    /// `user-generator/resources` and `../user-generator/resources`. The
    /// libraries this one draws from are loaded on their own.
    ///
    /// @param tier The tier.
    /// @return False when the tier was not found.
    /// @throws core::tsv_error If a file is malformed.
    [[nodiscard]] bool load(dataset tier)
    {
        const std::string tier_dir{core::tier_name(tier)};
        const auto marker = tier_dir + "/" + std::string{file_name_(user_table::platforms)};
        const auto root = core::find_resources(library_name_, marker);
        documents docs;
        for (const auto t : all_tables_)
        {
            docs.at(index_(t)) = core::load_resource(tier_dir + "/" + std::string{file_name_(t)},
                                                     embedded_files_(), root);
        }
        if (!docs.at(index_(user_table::platforms)) || !docs.at(index_(user_table::statuses))) return false;
        apply_(std::move(docs), core::layer_mode::replace, true);
        return true;
    }

    /// @brief Add data layers from a directory or a file.
    ///
    /// A directory may hold any of the table files (see user_table); all are
    /// applied together or, if any fails, none. A file must have one of those
    /// names.
    ///
    /// @param path Directory or file.
    /// @param mode How the layers combine with the data loaded before.
    /// @throws core::tsv_error If a file cannot be read or a row is
    ///         invalid; the data is then unchanged.
    /// @throws std::invalid_argument If @p path is a file with another name.
    void extend(const std::filesystem::path& path, core::layer_mode mode = core::layer_mode::override)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec))
        {
            apply_(read_directory_(path), mode, false);
            return;
        }
        for (const auto t : all_tables_)
        {
            if (path.filename() == file_name_(t))
            {
                (void)extend(t, core::tsv_document::load(path), mode);
                return;
            }
        }
        throw std::invalid_argument("extend: expected one of the user tables (platforms.tsv, "
                                    "platform_shares.tsv, statuses.tsv, languages.tsv, locales.tsv, "
                                    "timezones.tsv, password_words.tsv, password_patterns.tsv), got " +
                                    path.filename().string());
    }

    /// @brief Add a data layer to one table.
    ///
    /// Rows are keyed by the table's key columns (see doc/data.md). In the
    /// default override mode a row replaces the loaded row with the same key
    /// and new keys are added; a row with `disabled` set to 1 removes its key.
    ///
    /// @param which The table.
    /// @param doc TSV with that table's columns.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did (rows added, replaced, removed, warnings).
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend(user_table which, core::tsv_document doc,
                                   core::layer_mode mode = core::layer_mode::override)
    {
        documents docs;
        docs.at(index_(which)).emplace(std::move(doc));
        apply_(std::move(docs), mode, false);
        return _tables.at(index_(which)).layers().back();
    }

    /// @brief Add a data layer to one table from TSV text (for example from a
    ///        WebAssembly host).
    /// @param which The table.
    /// @param text TSV with that table's columns.
    /// @param source Name used in errors.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did.
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend_from_string(user_table which, std::string_view text, std::string source,
                                               core::layer_mode mode = core::layer_mode::override)
    {
        return extend(which, core::tsv_document::parse(text, std::move(source)), mode);
    }

    /// @brief The layers in effect for one table, in order.
    /// @param which The table.
    /// @return The layer records.
    [[nodiscard]] const std::vector<core::layer_info>& layers(user_table which) const
    {
        return _tables.at(index_(which)).layers();
    }

    /// @brief Hash of the loaded data: this generator's tables and the data of
    ///        the seven libraries. Recorded in each result's signature(); a
    ///        user with a device combines it with device-generator's.
    /// @return A hash that changes when any of them does; 0 when this
    ///         generator's tables are empty.
    [[nodiscard]] std::uint64_t data_hash() const
    {
        std::uint64_t own = 0;
        bool any = false;
        for (const auto& t : _tables)
        {
            any = any || !t.layers().empty();
            own = core::hash_combine(own, t.data_hash());
        }
        if (!any) return 0;
        std::uint64_t h = own;
        for (const auto part : {_from.country->data_hash(), _from.birth->data_hash(), _from.name->data_hash(),
                                _from.nickname->data_hash(), _from.address->data_hash(),
                                _from.phone->data_hash(), _from.email->data_hash()})
        {
            h = core::hash_combine(h, part);
        }
        return h;
    }

  private:
    using table = user_table;

    // -- Tables -------------------------------------------------------------

    static constexpr std::string_view library_name_ = "user-generator";
    static constexpr std::size_t table_count_ = 8;
    static constexpr std::array<table, table_count_> all_tables_{
        table::platforms, table::platform_shares, table::statuses,       table::languages,
        table::locales,   table::timezones,       table::password_words, table::password_patterns};

    using documents = std::array<std::optional<core::tsv_document>, table_count_>;
    using tables = std::array<core::layered_table, table_count_>;

    static constexpr std::size_t index_(table t) { return static_cast<std::size_t>(t); }

    static constexpr std::string_view file_name_(table t)
    {
        switch (t)
        {
        case table::platforms: return "platforms.tsv";
        case table::platform_shares: return "platform_shares.tsv";
        case table::statuses: return "statuses.tsv";
        case table::languages: return "languages.tsv";
        case table::locales: return "locales.tsv";
        case table::timezones: return "timezones.tsv";
        case table::password_words: return "password_words.tsv";
        case table::password_patterns: return "password_patterns.tsv";
        }
        return "";
    }

    static core::table_schema schema_(table t)
    {
        switch (t)
        {
        case table::platforms:
            return {.key = {"key"}, .required = {"label", "device", "os", "weight"}, .optional = {}};
        case table::platform_shares:
            return {.key = {"country", "key"}, .required = {"weight"}, .optional = {}};
        case table::statuses: return {.key = {"key"}, .required = {"label", "login", "weight"}, .optional = {}};
        case table::languages: return {.key = {"language"}, .required = {"code"}, .optional = {}};
        case table::locales: return {.key = {"country", "locale"}, .required = {"weight"}, .optional = {}};
        case table::timezones: return {.key = {"country", "timezone"}, .required = {"weight"}, .optional = {}};
        case table::password_words: return {.key = {"word", "kind"}, .required = {"weight"}, .optional = {}};
        case table::password_patterns: return {.key = {"pattern"}, .required = {"weight"}, .optional = {}};
        }
        return {};
    }

    static tables make_tables_()
    {
        return {core::layered_table{schema_(table::platforms)},
                core::layered_table{schema_(table::platform_shares)},
                core::layered_table{schema_(table::statuses)}, core::layered_table{schema_(table::languages)},
                core::layered_table{schema_(table::locales)}, core::layered_table{schema_(table::timezones)},
                core::layered_table{schema_(table::password_words)},
                core::layered_table{schema_(table::password_patterns)}};
    }

    // -- Parsed rows --------------------------------------------------------

    /// How a status logs in: within the last 90 days, over a year ago, or never.
    enum class login_kind : std::uint8_t { recent, stale, never };

    struct platform_row
    {
        std::string key;
        std::string label;
        std::string device;
        std::string os;
        double weight{0.0};
    };

    struct status_row
    {
        std::string key;
        std::string label;
        login_kind login{login_kind::recent};
        double weight{0.0};
    };

    struct weighted_text
    {
        std::string text;
        double weight{0.0};
    };

    /// The words and patterns of the `human` password style.
    struct password_model
    {
        std::vector<std::string> words;
        std::optional<core::discrete> word_pick; // over words
        std::vector<std::string> exact;
        std::optional<core::discrete> exact_pick; // over exact
        std::vector<std::string> patterns;
        std::optional<core::discrete> pattern_pick; // over patterns
    };

    struct model
    {
        password_model passwords;
        std::vector<platform_row> platforms;
        std::map<std::string, std::vector<std::pair<std::string, double>>, std::less<>> shares; // country -> key, weight
        std::vector<status_row> statuses;
        std::optional<core::discrete> status_pick; // over statuses
        std::map<std::string, std::string, std::less<>> languages;
        std::map<std::string, std::vector<weighted_text>, std::less<>> locales;
        std::map<std::string, std::vector<weighted_text>, std::less<>> timezones;
    };

    static double weight_(const core::tsv_row& row)
    {
        const auto w = row.get<double>("weight");
        if (!(w >= 0.0) || !std::isfinite(w)) throw row.error("weight", "must be a finite number, not negative");
        return w;
    }

    static std::string country_(const core::tsv_row& row)
    {
        auto c = row.get<std::string>("country");
        if (!core::is_alpha2(c)) throw row.error("country", "must be an upper case ISO 3166-1 alpha-2 code");
        return c;
    }

    static login_kind login_(const core::tsv_row& row)
    {
        const auto text = row.get<std::string>("login");
        if (text == "recent") return login_kind::recent;
        if (text == "stale") return login_kind::stale;
        if (text == "never") return login_kind::never;
        throw row.error("login", "must be recent, stale or never");
    }

    static bool known_token_(std::string_view t)
    {
        static constexpr std::array<std::string_view, 13> tokens{"word", "Word", "WORD", "leet", "Leet",
                                                                  "exact", "name", "Name", "user", "year",
                                                                  "yy", "num", "sym"};
        return std::ranges::find(tokens, t) != tokens.end();
    }

    // A pattern is text and {tokens}; a brace that is not part of a known token is an error.
    static void check_pattern_(const core::tsv_row& row)
    {
        const auto pattern = row.get<std::string>("pattern");
        if (pattern.empty()) throw row.error("pattern", "must not be empty");
        for (std::size_t i = 0; i < pattern.size(); ++i)
        {
            if (pattern[i] == '}') throw row.error("pattern", "has a } without a {");
            if (pattern[i] != '{') continue;
            const auto close = pattern.find('}', i);
            if (close == std::string::npos) throw row.error("pattern", "has a { without a }");
            if (!known_token_(std::string_view{pattern}.substr(i + 1, close - i - 1)))
                throw row.error("pattern", "has an unknown token " + pattern.substr(i, close - i + 1));
            i = close;
        }
    }

    static void validate_(table t, const core::tsv_row& row)
    {
        switch (t)
        {
        case table::password_words:
        {
            const auto word = row.get<std::string>("word");
            if (word.empty() || word.find_first_of(" \t{}") != std::string::npos)
                throw row.error("word", "must not be empty or hold spaces or braces");
            const auto kind = row.get<std::string>("kind");
            if (kind != "word" && kind != "exact") throw row.error("kind", "must be word or exact");
            (void)weight_(row);
            break;
        }
        case table::password_patterns:
            check_pattern_(row);
            (void)weight_(row);
            break;
        case table::platforms:
            for (const char* column : {"key", "label", "device", "os"})
            {
                if (row.get<std::string>(column).empty()) throw row.error(column, "must not be empty");
            }
            (void)weight_(row);
            break;
        case table::platform_shares:
            (void)country_(row);
            (void)weight_(row);
            break;
        case table::statuses:
            if (row.get<std::string>("label").empty()) throw row.error("label", "must not be empty");
            (void)login_(row);
            (void)weight_(row);
            break;
        case table::languages:
            if (row.get<std::string>("code").empty()) throw row.error("code", "must not be empty");
            break;
        case table::locales:
        case table::timezones:
            (void)country_(row);
            (void)weight_(row);
            break;
        }
    }

    static model build_model_(const tables& tbl)
    {
        model m;
        for (const auto& row : tbl.at(index_(table::platforms)).rows())
        {
            m.platforms.push_back({.key = row.get<std::string>("key"), .label = row.get<std::string>("label"),
                                   .device = row.get<std::string>("device"), .os = row.get<std::string>("os"),
                                   .weight = weight_(row)});
        }
        for (const auto& row : tbl.at(index_(table::platform_shares)).rows())
        {
            m.shares[country_(row)].emplace_back(row.get<std::string>("key"), weight_(row));
        }
        std::vector<double> status_weights;
        for (const auto& row : tbl.at(index_(table::statuses)).rows())
        {
            m.statuses.push_back({.key = row.get<std::string>("key"), .label = row.get<std::string>("label"),
                                  .login = login_(row), .weight = weight_(row)});
            status_weights.push_back(m.statuses.back().weight);
        }
        if (!status_weights.empty()) m.status_pick.emplace(status_weights);
        for (const auto& row : tbl.at(index_(table::languages)).rows())
        {
            m.languages[row.get<std::string>("language")] = row.get<std::string>("code");
        }
        for (const auto& row : tbl.at(index_(table::locales)).rows())
        {
            m.locales[country_(row)].push_back({.text = row.get<std::string>("locale"), .weight = weight_(row)});
        }
        for (const auto& row : tbl.at(index_(table::timezones)).rows())
        {
            m.timezones[country_(row)].push_back({.text = row.get<std::string>("timezone"), .weight = weight_(row)});
        }

        auto& pw = m.passwords;
        std::vector<double> word_weights;
        std::vector<double> exact_weights;
        for (const auto& row : tbl.at(index_(table::password_words)).rows())
        {
            const auto word = row.get<std::string>("word");
            if (row.get<std::string>("kind") == "exact")
            {
                pw.exact.push_back(word);
                exact_weights.push_back(weight_(row));
            }
            else
            {
                pw.words.push_back(word);
                word_weights.push_back(weight_(row));
            }
        }
        std::vector<double> pattern_weights;
        for (const auto& row : tbl.at(index_(table::password_patterns)).rows())
        {
            pw.patterns.push_back(row.get<std::string>("pattern"));
            pattern_weights.push_back(weight_(row));
        }
        const auto pick = [](const std::vector<double>& weights) -> std::optional<core::discrete> {
            if (std::ranges::none_of(weights, [](double w) { return w > 0.0; })) return std::nullopt;
            std::optional<core::discrete> d;
            d.emplace(weights);
            return d;
        };
        pw.word_pick = pick(word_weights);
        pw.exact_pick = pick(exact_weights);
        pw.pattern_pick = pick(pattern_weights);
        return m;
    }

    static documents read_directory_(const std::filesystem::path& dir)
    {
        documents docs;
        for (const auto t : all_tables_)
        {
            const auto file = dir / file_name_(t);
            std::error_code ec;
            if (std::filesystem::is_regular_file(file, ec))
            {
                docs.at(index_(t)) = core::tsv_document::load(file);
            }
        }
        return docs;
    }

    void apply_(documents docs, core::layer_mode mode, bool reset)
    {
        tables next = _tables;
        for (const auto t : all_tables_)
        {
            auto& doc = docs.at(index_(t));
            auto& tbl = next.at(index_(t));
            if (!doc)
            {
                if (reset) tbl.clear();
                continue;
            }
            (void)tbl.extend(std::move(*doc), mode, [t](const core::tsv_row& row) { validate_(t, row); });
        }
        auto built = build_model_(next);
        _tables = std::move(next);
        _model = std::move(built);
    }

    // -- State ----------------------------------------------------------------

    static constexpr int max_country_attempts_ = 1000;
    static constexpr std::string_view symbols_{"!@#$%^&*-_=+?"};
    static constexpr std::string_view ambiguous_{"0O1lI|"};

    sources _from{};
    tables _tables{make_tables_()};
    model _model;
    core::rng _engine{core::rng::from_entropy()};

    struct auto_probe_tag {};

    explicit usg(auto_probe_tag /*tag*/)
    {
        if (!load(dataset::lite)) (void)load(dataset::full);
    }

    static std::span<const core::embedded_file> embedded_files_()
    {
#ifdef DASMIG_USER_EMBED
        return embedded::usergen::files;
#else
        return {};
#endif
    }

    std::uint64_t draw_seed_() { return _engine(); }

    [[nodiscard]] bool has_account_data_() const { return !_model.platforms.empty() && !_model.statuses.empty(); }

    void require_data_() const
    {
        if (!has_data()) throw std::runtime_error("user-generator: this generator or a library has no data loaded");
    }

    // -- Helpers --------------------------------------------------------------

    static std::string upper_(std::string_view text)
    {
        std::string out{text};
        std::ranges::transform(out, out.begin(), [](char c) {
            return static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        });
        return out;
    }

    static std::string lower_(std::string_view text)
    {
        std::string out{text};
        std::ranges::transform(out, out.begin(), [](char c) {
            return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        });
        return out;
    }

    [[nodiscard]] bool in_contact_libraries_(const std::string& cca2) const
    {
        const auto has = [&](const std::vector<std::string>& codes) { return std::ranges::find(codes, cca2) != codes.end(); };
        return has(_from.address->countries()) && has(_from.phone->countries()) && has(_from.email->countries());
    }

    [[nodiscard]] static bool has_name_culture_(const std::string& cca2)
    {
        return ng::to_culture(core::from_utf8(lower_(cca2))) != culture::any;
    }

    // A username without its trailing digits and separators ("holst.liisa_1983"
    // gives "holst.liisa"): the email adds digits of its own, and a username
    // that kept its own would double them. A username that is only digits stays.
    static std::string username_stem_(const std::string& username)
    {
        auto end = username.size();
        while (end > 0 && ((username[end - 1] >= '0' && username[end - 1] <= '9') || username[end - 1] == '.' ||
                           username[end - 1] == '_'))
        {
            --end;
        }
        return end == 0 ? username : username.substr(0, end);
    }

    static void check_request_(const user_request& r)
    {
        if (r.ages.min > r.ages.max) throw std::invalid_argument("user_request: ages.min must be <= ages.max");
        if (r.ages.min < r.sign_up_age)
            throw std::invalid_argument("user_request: ages.min must be at least sign_up_age");
        if (!r.reference_date.ok()) throw std::invalid_argument("user_request: reference_date is not a date");
        if (r.service_start && !r.service_start->ok())
            throw std::invalid_argument("user_request: service_start is not a date");
        const auto& p = r.password;
        if (p.min_length == 0 || p.min_length > p.max_length)
            throw std::invalid_argument("user_request: password needs 1 <= min_length <= max_length");
        if (p.max_length > 256) throw std::invalid_argument("user_request: password.max_length must be at most 256");
        const auto classes = (p.lowercase ? 1U : 0U) + (p.uppercase ? 1U : 0U) + (p.digits ? 1U : 0U) +
                             (p.symbols ? 1U : 0U);
        if (classes == 0) throw std::invalid_argument("user_request: password allows no character class");
        if (p.require_each && p.min_length < classes)
            throw std::invalid_argument("user_request: password.min_length is too short to hold every class");
    }

    // -- The pipeline -----------------------------------------------------------

    [[nodiscard]] user build_(const user_request& request, const std::string& cca2, const core::rng& call,
                              std::uint64_t call_seed) const
    {
        user u;
        u._signature = {.seed = call_seed, .data = data_hash()};

        // Identity core: country, birth, name.
        const auto country = _from.country->find_country(cca2);
        if (country.cca2.empty()) throw core::unknown_country_error(cca2);
        u.country_code = cca2;
        u.country_name = country.name_common;

        birth_request br;
        br.country_code = cca2;
        br.bio_sex = request.person_sex;
        br.ages = request.ages;
        br.reference_date = request.reference_date;
        u.birthday = _from.birth->get_birth(br, call.split("birth")());

        const auto name_culture = ng::to_culture(core::from_utf8(lower_(cca2)));
        if (name_culture == culture::any)
            throw std::invalid_argument("user-generator: name-generator has no names for country " + cca2);
        const auto n = _from.name->get_full_name(
            {.gender = u.birthday.bio_sex == sex::female ? gender::f : gender::m, .culture = name_culture},
            call.split("name")());
        u.name = core::to_utf8(static_cast<std::wstring>(n));
        u.given = core::to_utf8(n.given());
        u.family = core::to_utf8(n.family());

        // Online identity: username, email, phone, address.
        const auto nick = _from.nickname->get_nickname(
            {.name = static_cast<std::wstring>(n), .style = nickname_style::username,
             .birth_year = static_cast<int>(u.birthday.year)},
            call.split("username")());
        u.account.username = core::to_utf8(static_cast<std::wstring>(nick));

        u.mail = _from.email->get_email(
            {.country = cca2, .given = u.given, .family = u.family, .nickname = username_stem_(u.account.username),
             .birth_year = static_cast<int>(u.birthday.year), .safe = request.safe},
            call.split("email")());
        u.mobile = _from.phone->get_phone({.country = cca2, .safe = request.safe}, call.split("phone")());
        u.location = _from.address->get_address({.country = cca2}, call.split("address")());

        // The account.
        auto pw = call.split("password");
        if (request.password.style == password_style::human)
        {
            u.account.password = human_password_(request.password,
                                                 {.given = ascii_letters_(u.given),
                                                  .stem = ascii_only_(lower_(username_stem_(u.account.username))),
                                                  .year = static_cast<int>(u.birthday.year)},
                                                 pw);
        }
        else
        {
            u.account.password = password_(request.password, pw);
        }
        const auto line = account_dates_(request, u, call);
        const auto& status = pick_status_(request, u, call);
        const auto seen = place_login_(line, status.login, u);
        pick_platform_(u, call);
        u.account.locale = locale_(country, call);
        u.account.timezone = timezone_(country, call);
        u.account.currency = country.currency_code;
        if (request.device) add_device_(request, seen, u, call);
        return u;
    }

    // -- Password -----------------------------------------------------------------

    static std::string password_(const password_policy& p, core::rng& r)
    {
        std::vector<std::string> classes;
        const auto add = [&](bool on, std::string_view chars) {
            if (!on) return;
            std::string kept;
            for (const char c : chars)
            {
                if (p.avoid_ambiguous && ambiguous_.contains(c)) continue;
                kept.push_back(c);
            }
            classes.push_back(std::move(kept));
        };
        add(p.lowercase, "abcdefghijklmnopqrstuvwxyz");
        add(p.uppercase, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        add(p.digits, "0123456789");
        add(p.symbols, symbols_);

        std::string all;
        for (const auto& c : classes) all += c;
        const auto length = core::uniform_int<std::size_t>(r, p.min_length, p.max_length);
        std::string out;
        if (p.require_each)
        {
            for (const auto& c : classes) out.push_back(c[core::uniform_int<std::size_t>(r, 0, c.size() - 1)]);
        }
        while (out.size() < length) out.push_back(all[core::uniform_int<std::size_t>(r, 0, all.size() - 1)]);
        // Shuffle, so the guaranteed characters are not always first.
        for (std::size_t i = out.size(); i > 1; --i)
        {
            std::swap(out[i - 1], out[core::uniform_int<std::size_t>(r, 0, i - 1)]);
        }
        return out;
    }

    // -- Human passwords ---------------------------------------------------------

    /// What a human password draws on from the person.
    struct password_context
    {
        std::string given; // the given name in lower case ASCII letters; empty when it is not Latin
        std::string stem;  // the username without its trailing digits, lower case ASCII
        int year{0};       // the birth year
    };

    struct char_sets
    {
        std::string lower, upper, digit, symbol; // a class the rules leave out is empty
    };

    static char_sets sets_(const password_policy& p)
    {
        const auto keep = [&](bool on, std::string_view chars) {
            std::string kept;
            if (!on) return kept;
            for (const char c : chars)
            {
                if (!(p.avoid_ambiguous && ambiguous_.contains(c))) kept.push_back(c);
            }
            return kept;
        };
        return {.lower = keep(p.lowercase, "abcdefghijklmnopqrstuvwxyz"),
                .upper = keep(p.uppercase, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
                .digit = keep(p.digits, "0123456789"),
                .symbol = keep(p.symbols, symbols_)};
    }

    // The letters of a name, or nothing if any byte is outside ASCII (an accent
    // dropped from the middle of a name would read as another name).
    static std::string ascii_letters_(std::string_view text)
    {
        std::string out;
        for (const char c : lower_(text))
        {
            if (static_cast<unsigned char>(c) >= 0x80) return {};
            if (c >= 'a' && c <= 'z') out.push_back(c);
        }
        return out;
    }

    static std::string ascii_only_(std::string_view text)
    {
        std::string out;
        for (const char c : text)
        {
            if (static_cast<unsigned char>(c) < 0x80) out.push_back(c);
        }
        return out;
    }

    static std::string capital_(std::string text)
    {
        if (!text.empty() && text.front() >= 'a' && text.front() <= 'z') text.front() = static_cast<char>(text.front() - 'a' + 'A');
        return text;
    }

    // Look-alike digits and symbols in place of letters, each with some chance.
    static std::string leet_(std::string word, core::rng& r)
    {
        for (char& ch : word)
        {
            char to = 0;
            switch (ch)
            {
            case 'a': to = '@'; break;
            case 'e': to = '3'; break;
            case 'i': to = '1'; break;
            case 'o': to = '0'; break;
            case 's': to = '5'; break;
            default: break;
            }
            if (to != 0 && core::uniform01(r) < 0.7) ch = to;
        }
        return word;
    }

    // One of the rows, by weight.
    template <std::size_t N>
    static std::string_view weighted_(const std::array<std::pair<std::string_view, double>, N>& rows, core::rng& r)
    {
        double total = 0.0;
        for (const auto& row : rows) total += row.second;
        double at = core::uniform01(r) * total;
        for (const auto& row : rows)
        {
            if (at < row.second) return row.first;
            at -= row.second;
        }
        return rows.back().first;
    }

    // A number people like: 1, 123, 1234, 69, or two or three digits at random.
    static std::string number_(core::rng& r)
    {
        static constexpr std::array<std::pair<std::string_view, double>, 14> common{{
            {"1", 22.0}, {"123", 22.0}, {"12", 8.0}, {"1234", 9.0}, {"2", 4.0}, {"11", 4.0}, {"69", 3.0},
            {"007", 2.0}, {"99", 3.0}, {"01", 3.0}, {"13", 3.0}, {"21", 3.0}, {"23", 3.0}, {"", 16.0}}};
        const auto text = weighted_(common, r);
        return text.empty() ? std::to_string(core::uniform_int<int>(r, 10, 999)) : std::string{text};
    }

    static std::string symbol_(core::rng& r)
    {
        static constexpr std::array<std::pair<std::string_view, double>, 8> common{{
            {"!", 60.0}, {"@", 8.0}, {"#", 6.0}, {"$", 4.0}, {"*", 6.0}, {"_", 4.0}, {"-", 3.0}, {"?", 3.0}}};
        return std::string{weighted_(common, r)};
    }

    [[nodiscard]] std::string token_(std::string_view token, const password_context& c, core::rng& r) const
    {
        const auto& pw = _model.passwords;
        const auto draw = [&](const std::vector<std::string>& list, const std::optional<core::discrete>& pick) {
            return pick ? list[(*pick)(r)] : std::string{};
        };
        const auto word = [&] { return draw(pw.words, pw.word_pick); };
        // The given name; a name that is not Latin gives the first part of the username, then a word.
        const auto given = [&] {
            if (!c.given.empty()) return c.given;
            const auto first = c.stem.substr(0, c.stem.find_first_not_of("abcdefghijklmnopqrstuvwxyz"));
            return first.empty() ? word() : first;
        };

        if (token == "word") return word();
        if (token == "Word") return capital_(word());
        if (token == "WORD") return upper_(word());
        if (token == "leet") return leet_(word(), r);
        if (token == "Leet") return capital_(leet_(word(), r));
        if (token == "exact") return draw(pw.exact, pw.exact_pick);
        if (token == "name") return given();
        if (token == "Name") return capital_(given());
        if (token == "user") return c.stem;
        if (token == "year") return std::to_string(c.year);
        if (token == "yy") return std::format("{:02}", c.year % 100);
        if (token == "num") return number_(r);
        return symbol_(r); // "sym": the patterns are checked when they are loaded
    }

    // Bend a candidate to the rules the way a sign-up form does: drop what is not
    // allowed, cut it to the longest length, add the digit or symbol that is
    // missing, pad it to the shortest length. Empty when that did not work out.
    static std::string conform_(const std::string& candidate, const password_policy& p, core::rng& r)
    {
        const auto set = sets_(p);
        const auto has = [](const std::string& chars, char c) { return chars.find(c) != std::string::npos; };

        std::string s;
        for (const char c : candidate)
        {
            if (has(set.lower, c) || has(set.upper, c) || has(set.digit, c) || has(set.symbol, c))
            {
                s.push_back(c);
                continue;
            }
            // A letter in a class the rules leave out changes case; anything else is dropped.
            const char other = c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A')
                               : c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a')
                                                      : '\0';
            if (other != '\0' && (has(set.lower, other) || has(set.upper, other))) s.push_back(other);
        }
        if (s.size() > p.max_length) s.resize(p.max_length);

        const auto present = [&](const std::string& chars) { return s.find_first_of(chars) != std::string::npos; };
        const auto random_of = [&](const std::string& chars) {
            return chars[core::uniform_int<std::size_t>(r, 0, chars.size() - 1)];
        };
        const auto add = [&](char c) {
            if (s.size() < p.max_length) s.push_back(c);
            else s.back() = c;
        };
        if (p.require_each)
        {
            if (!set.upper.empty() && !present(set.upper))
            {
                const auto at = s.find_first_of(set.lower);
                const char up = at == std::string::npos ? '\0' : static_cast<char>(s[at] - 'a' + 'A');
                if (up != '\0' && has(set.upper, up)) s[at] = up;
                else add(random_of(set.upper));
            }
            if (!set.lower.empty() && !present(set.lower)) add(random_of(set.lower));
            if (!set.digit.empty() && !present(set.digit)) add(random_of(set.digit));
            if (!set.symbol.empty() && !present(set.symbol))
                add(has(set.symbol, '!') && core::uniform01(r) < 0.7 ? '!' : random_of(set.symbol));
        }
        const auto filler = set.digit.empty() ? set.lower + set.upper + set.symbol : set.digit;
        while (s.size() < p.min_length) s.push_back(random_of(filler));

        const bool ok = s.size() >= p.min_length && s.size() <= p.max_length &&
                        (!p.require_each || ((set.lower.empty() || present(set.lower)) &&
                                             (set.upper.empty() || present(set.upper)) &&
                                             (set.digit.empty() || present(set.digit)) &&
                                             (set.symbol.empty() || present(set.symbol))));
        return ok ? s : std::string{};
    }

    // A password the way people choose one: a pattern from the data filled in
    // with words, the person's name, the birth year, numbers and symbols, then
    // bent to the policy. A random password if that fails.
    [[nodiscard]] std::string human_password_(const password_policy& p, const password_context& c,
                                              core::rng& r) const
    {
        const auto& pattern = _model.passwords.patterns[(*_model.passwords.pattern_pick)(r)];
        std::string candidate;
        for (std::size_t i = 0; i < pattern.size(); ++i)
        {
            if (pattern[i] != '{')
            {
                candidate.push_back(pattern[i]);
                continue;
            }
            const auto close = pattern.find('}', i);
            candidate += token_(std::string_view{pattern}.substr(i + 1, close - i - 1), c, r);
            i = close;
        }
        auto out = conform_(candidate, p, r);
        return out.empty() ? password_(p, r) : out;
    }

    // -- Dates ----------------------------------------------------------------------

    using seconds_t = std::int64_t;

    static seconds_t epoch_seconds_(std::chrono::year_month_day d)
    {
        return static_cast<seconds_t>(std::chrono::sys_days{d}.time_since_epoch().count()) * 86400;
    }

    // A date plus whole years, with 29 February becoming 28 in a common year.
    static std::chrono::year_month_day plus_years_(std::chrono::year_month_day d, int years)
    {
        const std::chrono::year_month_day out{d.year() + std::chrono::years{years}, d.month(), d.day()};
        return out.ok() ? out : std::chrono::year_month_day{out.year(), d.month(), std::chrono::day{28}};
    }

    static std::string iso_(seconds_t s)
    {
        const auto days = (s >= 0 ? s : s - 86399) / 86400;
        const std::chrono::year_month_day d{std::chrono::sys_days{std::chrono::days{days}}};
        const auto rest = s - days * 86400;
        return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z", static_cast<int>(d.year()),
                           static_cast<unsigned>(d.month()), static_cast<unsigned>(d.day()), rest / 3600,
                           rest / 60 % 60, rest % 60);
    }

    // The instants an account's times are placed between.
    struct timeline
    {
        seconds_t created{0}; // sign-up time
        seconds_t latest{0};  // the last instant a login can be at
        seconds_t ref{0};     // the reference date
        double draw{0.0};     // a uniform draw for the login
    };

    // The sign-up time grows towards the reference date (a service gains users
    // faster as it grows), kept after the person reaches the sign-up age and
    // after the service started. The login is placed by place_login_().
    [[nodiscard]] static timeline account_dates_(const user_request& request, user& u, const core::rng& call)
    {
        const seconds_t ref = epoch_seconds_(request.reference_date);
        const std::chrono::year_month_day born{std::chrono::year{u.birthday.year},
                                               std::chrono::month{u.birthday.month},
                                               std::chrono::day{u.birthday.day}};
        const auto service = request.service_start ? *request.service_start : plus_years_(request.reference_date, -10);
        const seconds_t earliest = std::max(epoch_seconds_(service),
                                            epoch_seconds_(plus_years_(born, request.sign_up_age)));
        const seconds_t latest = std::max(earliest, ref - 1);

        auto r = call.split("dates");
        const seconds_t created = earliest + static_cast<seconds_t>(static_cast<double>(latest - earliest) *
                                                                      std::sqrt(core::uniform01(r)));
        u.account.created = iso_(created);
        return {.created = created, .latest = latest, .ref = ref, .draw = core::uniform01(r)};
    }

    // The status: the one asked for, or one drawn by weight.
    [[nodiscard]] const status_row& pick_status_(const user_request& request, user& u, const core::rng& call) const
    {
        auto r = call.split("status");
        const status_row* chosen = nullptr;
        if (request.status)
        {
            for (const auto& s : _model.statuses)
            {
                if (s.key == *request.status) chosen = &s;
            }
            if (chosen == nullptr)
                throw std::invalid_argument("user_request: unknown status " + *request.status);
        }
        else
        {
            chosen = &_model.statuses[(*_model.status_pick)(r)];
        }
        u.account.status = chosen->key;
        u.account.status_label = chosen->label;
        return *chosen;
    }

    // The last login by what the status says: within 90 days and towards the
    // reference date, over a year ago, or never. Returns the last time the
    // account was used: the login, or the sign-up for one that never logged in.
    static seconds_t place_login_(const timeline& line, login_kind kind, user& u)
    {
        const seconds_t created = line.created;
        const seconds_t latest = line.latest;
        const seconds_t ref = line.ref;
        const double x = line.draw;
        constexpr seconds_t day = 86400;
        switch (kind)
        {
        case login_kind::never: u.account.last_login.clear(); return created;
        case login_kind::recent:
        {
            const seconds_t from = std::max(created, ref - 90 * day);
            const seconds_t span = std::max<seconds_t>(latest - from, 0);
            const seconds_t at = latest - static_cast<seconds_t>(static_cast<double>(span) * x * x);
            u.account.last_login = iso_(at);
            return at;
        }
        case login_kind::stale:
        {
            const seconds_t to = ref - 365 * day;
            const seconds_t at =
                to <= created ? created : created + static_cast<seconds_t>(static_cast<double>(to - created) * x);
            u.account.last_login = iso_(at);
            return at;
        }
        }
        return created;
    }

    // -- Platform, locale, time zone -------------------------------------------------

    // The platforms of a country: the shares it overrides, in percent, and the
    // rest of the 100 split among the others in proportion to their global weights.
    void pick_platform_(user& u, const core::rng& call) const
    {
        const auto it = _model.shares.find(u.country_code);
        const auto override_of = [&](const platform_row& p) -> const double* {
            if (it == _model.shares.end()) return nullptr;
            for (const auto& [key, share] : it->second)
            {
                if (key == p.key) return &share;
            }
            return nullptr;
        };
        double overridden = 0.0;
        double others = 0.0;
        for (const auto& p : _model.platforms)
        {
            if (const auto* share = override_of(p)) overridden += *share;
            else others += p.weight;
        }
        const double left = std::max(0.0, 100.0 - overridden);
        std::vector<double> weights;
        weights.reserve(_model.platforms.size());
        for (const auto& p : _model.platforms)
        {
            const auto* share = override_of(p);
            if (share != nullptr) weights.push_back(*share);
            else weights.push_back(others > 0.0 ? p.weight / others * left : 0.0);
        }
        auto r = call.split("platform");
        const auto& p = _model.platforms[core::discrete{weights}(r)];
        u.account.platform = p.key;
        u.account.platform_label = p.label;
        u.account.device = p.device;
        u.account.os = p.os;
    }

    static const weighted_text& pick_text_(const std::vector<weighted_text>& rows, core::rng& r)
    {
        std::vector<double> weights;
        weights.reserve(rows.size());
        for (const auto& row : rows) weights.push_back(row.weight);
        return rows[core::discrete{weights}(r)];
    }

    [[nodiscard]] std::string locale_(const dasmig::country& country, const core::rng& call) const
    {
        auto r = call.split("locale");
        const auto it = _model.locales.find(country.cca2);
        if (it != _model.locales.end() && !it->second.empty()) return pick_text_(it->second, r).text;
        // The country's first language, written <language>-<country>.
        const auto end = country.languages.find(';');
        const auto first = country.languages.substr(0, end);
        const auto lang = _model.languages.find(first);
        return (lang == _model.languages.end() ? std::string{"und"} : lang->second) + "-" + country.cca2;
    }

    // -- Device ---------------------------------------------------------------------

    [[nodiscard]] const dvg& device_source_() const
    {
        return _from.device != nullptr ? *_from.device : dvg::instance();
    }

    // The device of the last sign-in, seen on the day of that sign-in. A day
    // before the earliest device-generator covers (2021-01-01) is seen as that day.
    void add_device_(const user_request& request, seconds_t seen, user& u, const core::rng& call) const
    {
        const auto& source = device_source_();
        const std::chrono::sys_days day{std::chrono::days{(seen >= 0 ? seen : seen - 86399) / 86400}};
        const std::chrono::sys_days earliest{device_request::earliest_reference_date};
        device_request dr;
        dr.country = u.country_code;
        dr.platform = u.account.platform;
        dr.locale = u.account.locale;
        dr.reference_date = std::chrono::year_month_day{std::max(day, earliest)};
        dr.safe = request.safe;
        u.client = source.get_device(dr, call.split("device")());
        u._signature.data = core::hash_combine(u._signature.data, source.data_hash());
    }

    [[nodiscard]] std::string timezone_(const dasmig::country& country, const core::rng& call) const
    {
        auto r = call.split("timezone");
        const auto it = _model.timezones.find(country.cca2);
        if (it != _model.timezones.end() && !it->second.empty()) return pick_text_(it->second, r).text;
        const auto first = country.timezones.substr(0, country.timezones.find(';'));
        return first.empty() ? std::string{"UTC"} : first;
    }
};

} // namespace dasmig

#endif // DASMIG_USERGEN_HPP
