# user-generator for C++

> **Requires C++23** (e.g., `-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

[![user-generator for C++](https://raw.githubusercontent.com/dasmig/user-generator/main/doc/user-generator.png)](https://github.com/dasmig/user-generator/releases)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/user-generator/main/LICENSE.MIT)
[![CI](https://github.com/dasmig/user-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/dasmig/user-generator/actions/workflows/ci.yml)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/user-generator.svg)](https://github.com/dasmig/user-generator/releases)
[![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/type-header--only-green.svg)](https://github.com/dasmig/user-generator#integration)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue.svg)](https://dasmig.github.io/user-generator/)

**[API Reference](https://dasmig.github.io/user-generator/)** · **[Usage Guide](doc/usage.md)** · **[Data and Method](doc/data.md)** · **[Releases](https://github.com/dasmig/user-generator/releases)**

Synthetic user records for apps, test databases and demos, composed from the other dasmig generators: one call draws a country, a birth and a name, then a username, an email, a phone number, a postal address and an account (password, sign-up and last-login times, status, platform, locale, time zone, currency), and returns them as typed results and one JSON document.

```
Yara Carvalho Souza (iam_ycarvalho63) <iam_ycarvalho@gmail.com>, Brazil
```

## Features

- **One Call, Seven Libraries**. The country is drawn once and passed to every part, so the name, the address, the phone number, the email and the account's locale agree. The username and the email are built from the name and the birth year.

- **A Whole Account**. A password to a policy (length, character classes, no look-alike characters), random or in the human style (common words, names, years and digits), a sign-up time after the person is old enough and after the service started, a last login that follows the account's status (active, inactive, unverified, suspended), a device and platform by country, a locale, a time zone and a currency.

- **Safe Test Data**. `safe` makes the email use a reserved domain (`example.com`, `.net`, `.org`) and the phone number a number from the range regulators set aside for fiction, where the country has one, so a generated user cannot reach anyone.

- **Ask for What You Need**. A `user_request` can fix the country, the sex, an age range, the reference date, the password policy, the youngest sign-up age, the service start and the status. What is left out is drawn.

- **Deterministic Seeding**. `get_user(request, seed)` is a pure function of the request, the seed and the data, and `user::seed()` replays a previous user. Each part draws from its own stream split from the seed, so forcing one choice, such as the status, does not shift the others.

- **Batch Generation**. Generate thousands of users from one seed, on several threads with the same output for any thread count, unique by a key such as the email, and stream them as JSON Lines or CSV (see the usage guide).

- **Identical Everywhere**. A seed gives the same result on every compiler, standard library and CPU (GCC, Clang, MSVC, Emscripten; x86-64 and ARM64), checked by known-answer tests.

- **Extensible Data**. The platform, status, locale and time-zone tables are TSV files you can extend with layers (`extend()`); the libraries it composes keep theirs, and their tiers and layers are the user's.

- **JSON and Replay**. `to_json()` writes each library's result as that library writes it; `signature()` records the seed and a hash of the data of all seven libraries and the account tables.

- **Compiled-in Data**. Define `DASMIG_USER_EMBED` to build the `lite` tier of the user tables into the program, for WebAssembly and other builds without files. The libraries it composes have their own `DASMIG_*_EMBED` switches.

- **Multi-Instance Support**. Construct independent `usg` instances, each with its own tables and its own set of source libraries (`usg::sources`).

## Integration

[`usergen.hpp`](https://github.com/dasmig/user-generator/blob/main/dasmig/usergen.hpp) is the library [released here](https://github.com/dasmig/user-generator/releases). Keep [`core.hpp`](https://github.com/dasmig/user-generator/blob/main/dasmig/core.hpp) and the [`core/`](https://github.com/dasmig/user-generator/tree/main/dasmig/core) directory ([dasmig-core](https://github.com/dasmig/dasmig-core)) in the same directory. Add

```cpp
#include <dasmig/usergen.hpp>
```

to the files you want to use it in, and supply the [`resources`](https://github.com/dasmig/user-generator/tree/main/resources) folder (or define `DASMIG_USER_EMBED`).

user-generator composes these libraries directly and does not use [persona-generator](https://github.com/dasmig/persona-generator): a user has no body, traits, interests, job or city. The two share only the identity core (country, birth, name, nickname).

## Usage

```cpp
#include <dasmig/usergen.hpp>
#include <iostream>

auto& gen = dasmig::usg::instance();

auto u = gen.get_user(dasmig::user_request{});
std::cout << u << '\n';           // one line: name, username, email, country
std::cout << u.to_json() << '\n'; // {"name":...,"birth":{...},"address":{...},"account":{...},...}

dasmig::user_request request;
request.country = "BR";
request.person_sex = dasmig::sex::female;
request.ages = dasmig::age_range{.min = 25, .max = 35};
request.safe = true;                       // contacts that reach no one
auto v = gen.get_user(request, 42);          // a seed, so the values below repeat

v.mail.address();      // iam_ycarvalho@example.com
v.mobile.e164();       // +5589445812689, from the range set aside for fiction
v.account.password;    // to the password policy: random, or human (`Dragon123`) with style = password_style::human
v.account.last_login;  // an ISO 8601 time, empty if the account never logged in

// The same seed gives the same result everywhere.
auto again = gen.get_user(request, v.seed());
```

See the [usage guide](doc/usage.md) for everything else.

## Building

```bash
make            # example
make test       # tests
make bench      # benchmarks, results in build/bench.tsv
make test-embed # tests with the lite tier compiled in
make coverage   # code coverage
make lint       # clang-tidy
make docs       # API docs
make embed      # regenerate dasmig/usergen_data.hpp from resources/lite
```

## Compiler Support

Tested with:
- Clang 18+ (`-std=c++23`)
- GCC 14+ (`-std=c++23`)
- MSVC 19.38+ (`/std:c++latest`)
- Emscripten 3.1.74

## Dependencies

| Dependency | Version | Bundled | Purpose |
|-----------|---------|---------|---------|
| [dasmig-core](https://github.com/dasmig/dasmig-core) | 1.0 | Yes (`core.hpp`, `core/`) | Platform-independent random engine and distributions, data layers, JSON |
| [Catch2](https://github.com/catchorg/Catch2) | 3.x | Yes (amalgamated) | Unit testing |

## Related Libraries

| Library | Description |
|---------|-------------|
| [address-generator](https://github.com/dasmig/address-generator) | Postal addresses with real streets by place |
| [biodata-generator](https://github.com/dasmig/biodata-generator) | Procedural human physical characteristics |
| [birth-generator](https://github.com/dasmig/birth-generator) | Demographically plausible birthdays |
| [city-generator](https://github.com/dasmig/city-generator) | Weighted city selection by population |
| [country-generator](https://github.com/dasmig/country-generator) | Weighted country selection by population |
| [email-generator](https://github.com/dasmig/email-generator) | Email addresses in each country's style |
| [entity-generator](https://github.com/dasmig/entity-generator) | ECS-based entity generation |
| [interest-generator](https://github.com/dasmig/interest-generator) | Religion, hobbies and favourite media by country, age and sex |
| [name-generator](https://github.com/dasmig/name-generator) | Culturally appropriate full names |
| [nickname-generator](https://github.com/dasmig/nickname-generator) | Gamer-style nicknames |
| [occupation-generator](https://github.com/dasmig/occupation-generator) | Employment status, education and occupation by country, age and sex |
| [persona-generator](https://github.com/dasmig/persona-generator) | Complete characters composed from the other generators |
| [phone-generator](https://github.com/dasmig/phone-generator) | Phone numbers valid in each country's numbering plan |
| [trait-generator](https://github.com/dasmig/trait-generator) | Personality traits with conflicts, synergies and Big Five axes |

## License

This library is released under the [MIT License](https://opensource.org/licenses/MIT).

```
MIT License

Copyright (c) 2026 Diego Dasso Migotto
```
