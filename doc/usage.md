# Usage Guide

This guide covers every feature of the user-generator library. For a quick overview, see the [README](../README.md). For the full API reference, run `doxygen Doxyfile` from the repository root and open `doc/api/html/index.html`.

[TOC]

<!-- GitHub-rendered TOC (Doxygen uses [TOC] above instead) -->
<ul>
<li><a href="#quick-start">Quick Start</a></li>
<li><a href="#installation">Installation</a></li>
<li><a href="#loading-resources">Loading Resources</a></li>
<li><a href="#generating">Generating</a></li>
<li><a href="#asking-for-what-you-need">Asking for What You Need</a></li>
<li><a href="#the-user">The User</a></li>
<li><a href="#passwords">Passwords</a></li>
<li><a href="#dates-and-status">Dates and Status</a></li>
<li><a href="#safe-users">Safe Users</a></li>
<li><a href="#seeding-and-deterministic-generation">Seeding and Deterministic Generation</a></li>
<li><a href="#extending-data">Extending Data</a></li>
<li><a href="#replay-and-json">Replay and JSON</a></li>
<li><a href="#compiled-in-data">Compiled-in Data</a></li>
<li><a href="#thread-safety">Thread Safety</a></li>
<li><a href="#batch-generation">Batch Generation</a></li>
<li><a href="#error-reference">Error Reference</a></li>
</ul>

## Quick Start

```cpp
#include <dasmig/usergen.hpp>
#include <iostream>

int main()
{
    auto& gen = dasmig::usg::instance();
    std::cout << gen.get_user(dasmig::user_request{}) << '\n';
    // Yara Carvalho Souza (iam_ycarvalho63) <iam_ycarvalho@gmail.com>, Brazil
}
```

## Installation

Copy `dasmig/usergen.hpp`, `dasmig/core.hpp` and the `dasmig/core/` directory into your include path, keeping them together. The libraries it composes must be on the include path too: address, birth, country, email, name, nickname and phone, each with its own data, and device, whose data is needed only for users with a device (`user_request::device`). Supply `resources/` next to the program, or define `DASMIG_USER_EMBED` to compile the account tables in; the other libraries have their own `DASMIG_<NAME>_EMBED` macros for their `lite` tiers.

user-generator does not need persona-generator, and a user carries no body, traits, interests, job or city.

## Loading Resources

`instance()` loads the account tables' `lite` tier on first use (or `full` if there is no `lite`), from the compiled-in copy or from the first of `resources/`, `../resources`, `user-generator/resources` and `../user-generator/resources` that has it. The libraries it composes load their own the same way, through their singletons.

```cpp
dasmig::usg gen;                       // an independent instance, with no account data
bool ok = gen.load(dasmig::dataset::lite); // false when the tier is not found
gen.load(std::filesystem::path{"path/to/resources/lite"}); // a directory
gen.has_data();          // the account tables are loaded and so is every library
gen.has_account_data();  // the account tables only
gen.supported_countries(); // the countries every part has data for
```

`load` loads **the account tables only**; it replaces them and every layer loaded before. To load a tier into a library it composes, call that library's own `load`:

```cpp
dasmig::nng::instance().load(dasmig::dataset::full);
```

A `usg` draws from the singletons of the seven libraries by default, and from device-generator's for a user with a device (`sources::device`, null by default, means `dvg::instance()`, which loads its data on the first such user). Construct one over your own instances to use different data side by side:

```cpp
dasmig::adg my_addresses;
my_addresses.load("my/addresses");
dasmig::usg::sources from;          // the singletons...
from.address = &my_addresses;       // ...except the addresses
dasmig::usg gen{from};              // the instances must outlive gen
(void)gen.load(dasmig::dataset::lite);
```

## Generating

```cpp
auto u = gen.get_user(dasmig::user_request{});         // draws a seed from the instance engine
auto w = gen.get_user(dasmig::user_request{}, 1234);   // deterministic; does not touch the engine
```

## Asking for What You Need

Every field of a `user_request` is optional, and what is left out is drawn:

| Field | Meaning | Default |
|-------|---------|---------|
| `country` | ISO 3166-1 alpha-2 code, any case | one of `supported_countries()`, by population |
| `person_sex` | `dasmig::sex::male` or `female` | drawn with the birth |
| `ages` | `age_range{.min, .max}` in completed years | 18 to 80 |
| `reference_date` | the date ages and account times are computed at | birth-generator's, 2023-07-01 (the clock is never read) |
| `safe` | contacts that reach no one (see below) | false |
| `password` | a `password_policy` | 12 to 16 characters, all four classes |
| `sign_up_age` | the youngest age at which a person signs up | 13 |
| `service_start` | the earliest date an account can be created | ten years before the reference date |
| `status` | an account status key | drawn |
| `device` | add the device of the last sign-in (see below) | false |

```cpp
dasmig::user_request request;
request.country = "br";
request.person_sex = dasmig::sex::female;
request.ages = dasmig::age_range{.min = 25, .max = 35};
auto u = gen.get_user(request);
```

`ages.min` must be at least `sign_up_age`; lower both for a service for teenagers.

## The User

```cpp
u.name;         // "Yara Carvalho Souza" (UTF-8)
u.given;        // "Yara"
u.family;       // "Carvalho Souza" (both surnames of a double surname); empty when the name has none
u.person_sex(); // dasmig::sex::female
u.age();        // 26, completed years at the reference date
u.country_code; // "BR"
u.country_name; // "Brazil"
u.birthday;     // dasmig::birth: date, age, sex, cohort...
u.location;     // dasmig::address: u.location.formatted(), .lines(), .postcode...
u.mobile;       // dasmig::phone: u.mobile.e164(), .international(), .national()
u.mail;         // dasmig::email: u.mail.address(), .local, .domain
u.account;      // user_account, below
u.client;       // std::optional<dasmig::device>: the device of the last sign-in, with request.device
```

The fields of `u.account`:

| Field | Value |
|-------|-------|
| `username` | nickname-generator's `username` style, from the name and the birth year (`iam_ycarvalho63`) |
| `password` | to the request's policy, plain text |
| `created` | sign-up time, ISO 8601 UTC (`2019-02-05T07:32:31Z`) |
| `last_login` | ISO 8601 UTC; empty if the account never logged in |
| `status`, `status_label` | `active`, `inactive`, `unverified` or `suspended` in the shipped data |
| `platform`, `platform_label`, `device`, `os` | for example `ios-phone`, `iPhone`, `phone`, `iOS` |
| `locale` | BCP 47 (`pt-BR`) |
| `timezone` | IANA name (`America/Sao_Paulo`), or a UTC offset (`UTC+03:00`) where no zone is listed |
| `currency` | ISO 4217 code of the country (`BRL`) |

Each part follows the country: the name's culture, the address's layout, the phone's numbering plan, the email's provider mix and style, the locale, the time zone and the currency. The username and the email are built from the name and the birth year, so they resemble each other.

## Passwords

`password_policy` sets the length and the character classes:

```cpp
dasmig::user_request request;
request.password = {.min_length = 8, .max_length = 12};   // all four classes, at least one of each

request.password = {.min_length = 4, .max_length = 4,
                    .lowercase = false, .uppercase = false, .symbols = false}; // a four-digit PIN

request.password = {.min_length = 20, .max_length = 20,
                    .avoid_ambiguous = true};             // no 0 O 1 l I |
```

The length is drawn uniformly between the bounds. With `require_each` (the default) the password has at least one character of every allowed class, so `min_length` must be at least the number of classes; without it the characters are drawn from all allowed classes alike. The symbols are ``!@#$%^&*-_=+?``. Passwords are plain text and test data. By default they are random strings, which look nothing like what people choose.

`style = password_style::human` builds them the way people do instead: a pattern from `password_patterns.tsv` filled in with common words, the person's given name, their birth year, numbers such as `123` or `69`, and a symbol (mostly `!`), so you get `Dragon123`, `Isabel1987!`, `sunshine`, `p@ssw0rd`, `qwerty123` or `Monkey2009`. The word and pattern shares are in `password_words.tsv` and `password_patterns.tsv` (see [Data and Method](data.md)); extend them like the other tables. The result is then bent to the policy, as a sign-up form would: characters the rules do not allow are dropped (a letter changes case if only one case is allowed), the digit or the symbol that is missing is added, it is cut to `max_length` and padded to `min_length`. With the default rules (12 to 16 characters, all four classes) most passwords end up with padding, so for a mix closer to real lists ask for looser ones:

```cpp
request.password = {.min_length = 6, .max_length = 20, .require_each = false,
                    .style = dasmig::password_style::human};
```

The `random` style is the default and does not change with the data, so the seeds of earlier releases give the same passwords. The style only draws the password: every other part of the user stays the same. A generator whose `password_patterns` table is empty throws `std::runtime_error` for the `human` style.

## Dates and Status

An account is created after the person reaches `sign_up_age` and after `service_start`, and before the reference date. Sign-ups grow towards the reference date, so most accounts are recent. The status then places the last login:

| `login` of the status | Last login |
|-----------------------|------------|
| `recent` | within the 90 days before the reference date (or after the sign-up, if later), most very recent |
| `stale` | a year or more before the reference date, after the sign-up (the sign-up itself for an account younger than a year) |
| `never` | none: `last_login` is empty, and `null` in the JSON |

```cpp
request.status = "unverified";   // an account that never logged in
request.service_start = std::chrono::year_month_day{std::chrono::year{2020}, std::chrono::January, std::chrono::day{1}};
```

Forcing the status changes nothing else about the user: every part draws from its own stream.

## Safe Users

With `safe`, the email uses a reserved domain (`example.com`, `.net`, `.org`, RFC 2606) and the phone number a number from the range regulators set aside for fiction, where the country has one (the US and Canada 555-01xx, the UK 07700 900xxx, and a few more); `mail.reserved` and `mobile.reserved` say so. In a country with no such range the phone number is outside the allocated ranges instead, and `reserved` is false.

The address is not changed by `safe`: its street is a real name with a composed number, which is not a person's address, but it can match one by chance.

```cpp
dasmig::user_request request;
request.safe = true;
auto u = gen.get_user(request);
u.mail.address();   // ...@example.com
```

## The Device

With `request.device`, a user also has `client`, the device of the last sign-in, from [device-generator](https://github.com/dasmig/device-generator): a model, OS and browser version that fit together at a date, the `User-Agent` header and client hints that combination sends, an `Accept-Language` header, a screen and an IP address in the country.

```cpp
dasmig::user_request request;
request.country = "BR";
request.person_sex = dasmig::sex::female;
request.device = true;
auto u = gen.get_user(request, 42);
u.client->to_string();     // Chrome 113 on Android 13 (Samsung Galaxy A52), 2801:9c:db5a:14cb:9650:dec6:5843:7cca
u.client->user_agent;      // Mozilla/5.0 (Linux; Android 10; K) ... Chrome/113.0.0.0 Mobile Safari/537.36
u.client->accept_language; // pt-BR,pt;q=0.9,en;q=0.8
```

The device is device-generator's `get_device` for the user's country, the account's `platform` (so `client->kind` and `client->os` are the account's `device` and `os`) and `locale`, at the day of the last login, or of the sign-up for an account that never logged in. device-generator covers dates from 2021-01-01, so an earlier login is seen at that date. With `safe`, the IP address is from a documentation range (RFC 5737, RFC 3849) and `client->reserved` is true. It draws from the user's `"device"` stream, so it is `get_device(that request, core::rng{seed}.split("device")())`, and turning it on changes nothing else in the user. A drawn country is one device-generator has data for; every country of the shipped data has.

The device is off by default, so users without it, their JSON and their signatures are those of releases before it. device-generator's header must be on the include path either way (`usergen.hpp` includes it); its data is loaded only by the first user with a device.

## Seeding and Deterministic Generation

- `get_user(request, seed)` gives the same result for the same request, seed and data, on every platform, without advancing the instance engine.
- `get_user(request)` draws a fresh seed from the instance engine, initially seeded from system entropy. There are no generator-level `seed()` or `unseed()` controls; for deterministic sequences, keep a `dasmig::core::rng` in the caller and pass its next value as the seed.
- `u.seed()` is the seed a result was generated from: `get_user(request, u.seed())` replays it with the same request and data.
- Each part (`country`, `birth`, `name`, `username`, `email`, `phone`, `address`, `password`, `dates`, `status`, `platform`, `locale`, `timezone`) draws from a stream split from the seed by that name, so forcing one choice, or changing the password policy, does not shift the others. The one exception is the country: when it is drawn, the same seed can end on a different country if the data of a library changes.

## Extending Data

The shipped account tables are base layers. Files with the same columns can be loaded on top of them to add rows, change them or remove them, without editing the shipped files (see [Data and Method](data.md) for the columns). A table is named by `dasmig::user_table`: `platforms`, `platform_shares`, `statuses`, `languages`, `locales`, `timezones`, `password_words` and `password_patterns`.

```cpp
using dasmig::user_table;

// From a directory holding any of the table files, or from one file with a table's name.
gen.extend("mods/user");
gen.extend("mods/locales.tsv");

// From text, for example supplied by a WebAssembly host.
gen.extend_from_string(user_table::statuses,
                       "key\tlabel\tlogin\tweight\npending\tPending\tnever\t5\n", "scenario");

// Replace a table instead of overriding rows.
gen.extend_from_string(user_table::timezones,
                       "country\ttimezone\tweight\nBR\tAmerica/Sao_Paulo\t1\n", "one-zone",
                       dasmig::core::layer_mode::replace);

// Remove a row: the key columns and disabled.
gen.extend_from_string(user_table::platforms,
                       "key\tlabel\tdevice\tos\tweight\tdisabled\nother\tx\tx\tx\t1\t1\n", "cleanup");
```

- Every value in a layer is checked before anything changes. A bad layer throws `dasmig::core::tsv_error` with the file, line and column, and the data stays as it was. A layer for a directory is applied to all its tables or to none.
- `layers(table)` lists what is loaded in a table: source, mode, rows added, replaced and removed, and warnings such as unknown columns.
- `load(...)` replaces the base and drops every layer.
- To extend the data of a library it composes (for example more phone ranges), call that library's `extend`; the user's `data_hash()` follows.

## Replay and JSON

Every result records the seed and the data it came from:

```cpp
auto u = gen.get_user(dasmig::user_request{});
u.signature().seed; // the call seed
u.signature().data; // gen.data_hash() when it was generated: the account tables and the seven libraries
                    // (combined with device-generator's data_hash() for a user with a device)
```

`to_json()` gives the result as JSON. Each library's result is written as that library writes it, next to the user's own fields:

```json
{"name":"Yara Carvalho Souza","given":"Yara","family":"Carvalho Souza","sex":"female","age":26,
 "country":{"code":"BR","name":"Brazil"},
 "birth":{...},"address":{...},"phone":{...},"email":{...},
 "account":{"username":"iam_ycarvalho63","password":"684*+!ZsY=zc43",
            "created":"2019-02-05T07:32:31Z","last_login":"2023-05-22T04:07:35Z",
            "status":"active","platform":"android-phone","device":"phone","os":"Android",
            "locale":"pt-BR","timezone":"America/Sao_Paulo","currency":"BRL"},
 "signature":{"seed":"000000000000002a","data":"..."}}
```

A user with a device has `"device":{...}` after `"account"`, as device-generator writes it.

Seeds are strings because JavaScript numbers cannot hold every 64-bit value. `to_json(writer)` writes into an existing `dasmig::core::json_writer`, for composing larger documents.

## Compiled-in Data

Define `DASMIG_USER_EMBED` before including the header to compile the account tables' `lite` tier into the program. `load(dataset::lite)` and `instance()` then use it and need no files. The compiled-in data has the same `data_hash()` as the files, so seeds replay the same either way.

`dasmig/usergen_data.hpp` is generated from `resources/lite` by `make embed`; CI checks it is up to date. Define the other libraries' macros too (`DASMIG_ADDRESS_EMBED`, `DASMIG_BIRTH_EMBED`, `DASMIG_COUNTRY_EMBED`, `DASMIG_DEVICE_EMBED`, `DASMIG_EMAIL_EMBED`, `DASMIG_NAME_EMBED`, `DASMIG_NICKNAME_EMBED`, `DASMIG_PHONE_EMBED`) for a build without files.

## Thread Safety

`get_user(request, seed)` is `const` and only calls `const` members of the libraries, so it may run concurrently on one instance, which is what the batch calls do. `get_user(request)` draws from the instance engine, and `load()` and `extend()` change the tables: both need external synchronisation (e.g. a `std::mutex`) on the same instance. The libraries' singletons follow their own rules.

## Batch Generation

Every seeded getter has batch forms: `get_user_batch` returns a `std::vector`, and `for_each_user` streams the items to a sink in index order without holding the batch.

```cpp
auto& gen = dasmig::usg::instance();
const auto request = dasmig::user_request{};

// 1,000 users from one batch seed, on four threads.
auto items = gen.get_user_batch(request, {.count = 1000, .threads = 4}, 42);

// Item 1 replays on its own: the batch seed and the index give its seed.
auto again = gen.get_user(request, dasmig::core::item_seed(42, 1));

// Unique emails.
const auto rule = dasmig::core::unique_rule::by<dasmig::user>(
    [](const dasmig::user& u) { return u.mail.address(); });
auto unique = gen.get_user_batch(request, {.count = 1000, .unique = rule}, 42);

// A million users to a file as JSON Lines, never all in memory.
std::ofstream out{"users.jsonl"};
dasmig::core::jsonl_writer lines{out};
gen.for_each_user(request, {.count = 1'000'000, .threads = 0}, 42, lines);
```

- **Item seeds.** Item `i` is exactly `get_user(request, core::item_seed(batch_seed, first + i))`. A range can be generated directly (`first = 1000` starts at item 1000), two batches that follow each other equal one, and the output is the same for any `threads` (0 means the hardware count). Without a `batch_seed` one is drawn like the single getter draws its own.
- **Options.** `core::batch_options` has `count`, `first`, `threads` and `unique`. The request describes one item; the options describe the batch.
- **Uniqueness.** `.unique = core::unique_rule::by<user>(key)` regenerates an item whose key already appeared in the batch, with a deterministic retry seed, and throws `std::runtime_error` naming the item when the attempts run out. Unique by the email address, the username or the phone number is the usual choice; with one country the number of distinct values bounds the batch.
- **Sinks.** `core::jsonl_writer` writes one `to_json()` per line, `core::csv_writer` flattens each result's JSON into dotted columns (`account.username`, `address.city`, ...). Any callable `(std::uint64_t index, user&)` works.
- Batch calls use the same const getters from several threads, which the thread-safety rules above allow; the CI runs the batch tests under ThreadSanitizer.

## Error Reference

| Exception | When |
|-----------|------|
| `std::runtime_error` | Any generation call before the account tables or a library has data; a user with a device when device-generator has none |
| `std::invalid_argument` | `ages.min > ages.max`, or `ages.min < sign_up_age`; `reference_date` or `service_start` is not a date; a password policy with `min_length` of 0, `min_length > max_length`, `max_length` over 256, no allowed class, or `require_each` with fewer characters than classes; an unknown `status`; a `country` that is not a code or that a library has no data for (including a name culture or birth table without the sex asked for, and device-generator for a user with a device); a user with a device whose platform device-generator does not know (from a layer of `platforms.tsv`); `extend` of a file that is not one of the user tables |
| `dasmig::core::tsv_error` | A malformed table or layer (message gives file, line and column); the data is unchanged |
