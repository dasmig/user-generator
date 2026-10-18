# Data and Method

The data in `resources/` and how user-generator uses it (rule 6 of the dasmig standard).

user-generator keeps the data of the **account** only: the platforms, the statuses, the locale and time-zone tables, and the words and patterns of human-style passwords. Everything else a user holds comes from the libraries it composes, and each documents its own data, sources and limits.

## Where the data comes from

| Part | Library | Data notes |
| --- | --- | --- |
| Country, currency, languages | [country-generator](https://github.com/dasmig/country-generator) | `doc/data.md` |
| Birth, age, sex | [birth-generator](https://github.com/dasmig/birth-generator) | UN World Population Prospects age pyramids |
| Name | [name-generator](https://github.com/dasmig/name-generator) | `philipperemy/name-dataset`; see the provenance note in its `doc/data.md` |
| Username | [nickname-generator](https://github.com/dasmig/nickname-generator) | Word lists, curated |
| Email | [email-generator](https://github.com/dasmig/email-generator) | Provider shares, curated |
| Phone | [phone-generator](https://github.com/dasmig/phone-generator) | libphonenumber (Apache-2.0) |
| Address | [address-generator](https://github.com/dasmig/address-generator) | OpenStreetMap, GeoNames, libaddressinput |
| Account | this library | Authored; below |

A user's data is the union of these, and its `signature().data` is a hash of the seven libraries' loaded data and the account tables.

## Tiers

| Tier | Files | What it holds |
| --- | --- | --- |
| `lite` | 8 | The account tables below; the same rows are all there is, and there is no `full` tier |

## Tables

All tables can be extended with layers (`extend()`); rows are keyed by the columns marked as keys, and a row with `disabled` set to 1 removes its key.

### `platforms.tsv`: the devices

Key `key`.

| Column | Meaning |
| --- | --- |
| `key` | Stable identifier, used by layers and by `platform_shares.tsv` |
| `label` | Display label |
| `device` | `phone`, `desktop`, `tablet` or `other` (any text is accepted, for example `wearable`) |
| `os` | Operating system name |
| `weight` | Global share, any scale; `>= 0` |

Nine rows: Android and iOS phones, Windows, macOS, Linux and ChromeOS computers, iPad and Android tablets, and other. The weights are **authored estimates**, rounded from public browser and operating-system usage statistics, not fitted to a survey.

### `platform_shares.tsv`: the devices of a country

Key `country` + `key`.

| Column | Meaning |
| --- | --- |
| `country` | ISO 3166-1 alpha-2 code, upper case |
| `key` | A `platforms.tsv` key |
| `weight` | The platform's share in that country, **in percent** |

In a country with rows, the platforms it lists get these shares, and the others split what is left of 100 in proportion to their global weights (all of them get 0 if the rows reach 100). Eighteen countries are listed (iPhones are about half the accounts in the US, Canada, the UK and Australia, over half in Japan, and about 5% in India). Authored estimates, as above. A country without rows uses the global weights.

### `statuses.tsv`: the account statuses

Key `key`.

| Column | Meaning |
| --- | --- |
| `key` | Stable identifier, also what `user_request::status` takes |
| `label` | Display label |
| `login` | How the last login is placed: `recent` (within the 90 days before the reference date, towards it), `stale` (a year or more before it) or `never` (no login) |
| `weight` | Share, any scale; `>= 0` |

Four rows: `active` 88, `inactive` 7, `unverified` 3 and `suspended` 2. Authored, as a plausible mix for a consumer service; there is no source.

### `locales.tsv` and `languages.tsv`: the locale

`locales.tsv`, key `country` + `locale`: BCP 47 tags by country with a speaker share (`weight`). 102 rows for 83 countries; countries with several languages have several rows (Switzerland has `de-CH`, `fr-CH` and `it-CH`; Canada `en-CA` and `fr-CA`). Authored.

A country without a row gets its first language from country-generator, written `<language>-<country>`, through `languages.tsv` (key `language`, column `code`: the language name as country-generator writes it, to ISO 639-1, or 639-2 where there is no two-letter code; 87 rows). A language that is not in the table gives `und` (undetermined): `und-XX`.

### `timezones.tsv`: the time zone

Key `country` + `timezone`: IANA names by country with a population share (`weight`). 109 rows for 83 countries; countries that span zones have several (the US and Canada six each, Russia five, Australia four, Brazil and Mexico three). Authored from the zones' populations, rounded.

A country without a row gets the first UTC offset country-generator lists for it (`UTC+03:00`), or `UTC` if it lists none. The offset is a valid fixed offset, not a zone with daylight-saving rules.

### `password_words.tsv`: the words of human passwords

Key `word` + `kind`.

| Column | Meaning |
| --- | --- |
| `word` | The text, lower case, without spaces or braces |
| `kind` | `exact` (a whole password people reuse: `123456`, `qwerty`, `iloveyou`) or `word` (a base word that patterns add digits, years and symbols to: `dragon`, `sunshine`, `football`) |
| `weight` | Share within its kind, any scale; `>= 0` |

117 `exact` rows and 166 `word` rows. The list is **authored from general knowledge** of the passwords that top public breach-derived rankings (for example the annual "most common passwords" lists), with the weights set by rank and rounded: no dataset was copied or fitted, so the shares show the shape (digit runs and keyboard walks first, then names, animals, sports and love words) and not exact frequencies. Words are mostly English, with some Portuguese, Spanish, Italian and club names; add your own language through a layer. The `human` style needs this table only for the `{word}` and `{exact}` tokens.

### `password_patterns.tsv`: how human passwords are built

Key `pattern`.

| Column | Meaning |
| --- | --- |
| `pattern` | Text and `{tokens}` |
| `weight` | Share, any scale; `>= 0` |

Tokens, each drawn on its own (so `{word}{word}` is two words):

| Token | Gives |
| --- | --- |
| `{word}`, `{Word}`, `{WORD}` | A base word, as written, capitalised, upper case |
| `{leet}`, `{Leet}` | A base word with look-alikes (`a`→`@`, `e`→`3`, `i`→`1`, `o`→`0`, `s`→`5`, each with a 70% chance), capitalised for `{Leet}` |
| `{exact}` | A whole common password |
| `{name}`, `{Name}` | The person's given name; a name that is not in Latin letters gives the first part of the username, then a word |
| `{user}` | The username without its trailing digits and separators |
| `{year}`, `{yy}` | The birth year, in four or two digits |
| `{num}` | A number people like: `1`, `123`, `12`, `1234`, `69`, `007`... or two or three random digits |
| `{sym}` | A symbol of ``!@#$%^&*-_=+?``, `!` in about 60% of the draws |

An unknown token or a brace without its pair is refused when the layer is loaded. 26 patterns: a base word or a name with a number, a year or a symbol is most of the weight, then whole common passwords, two words, and look-alike spellings. The shares are **authored**, from how these passwords usually look; there is no source.

## Generation model

A call draws, each from a stream of its own split from the call seed by name:

1. **`country`**: the request's, or one drawn by population until every part has data for it. Drawing skips a country whose name or birth data cannot give the sex asked for; asking for one is an error.
2. **`birth`** (birth-generator): the country, the sex and the age range, at the reference date.
3. **`name`** (name-generator): the country's culture and the sex; the given name is the first part, the family name the last.
4. **`username`** (nickname-generator, `username` style): from the full name and the birth year.
5. **`email`** (email-generator): the country, the given and family names, the birth year and the username without its trailing digits and separators (so the email does not double the digits).
6. **`phone`** (phone-generator): the country; `safe` gives a reserved number where there is a range.
7. **`address`** (address-generator): the country. Its streets are real names from OpenStreetMap with composed numbers, so an address is plausible, not a person's.
8. **`password`**: with the `random` style, a length drawn uniformly between the policy's bounds; one character from each allowed class if `require_each`, the rest from all allowed classes, then shuffled. With the `human` style, a pattern drawn by weight and filled in from the word table, the given name, the birth year and the username, then fitted to the policy: characters not allowed are dropped, it is cut to `max_length`, the missing class is added (capitalising the first letter, a digit or `!`) and it is padded to `min_length`; a password that cannot be fitted falls back to the random one.
9. **`dates`**: the sign-up time and a draw for the login. The sign-up is between the later of the service start and the person's birthday of the sign-up age (13 by default), and the reference date, with the density growing towards the reference date (the square root of a uniform draw), so most accounts are recent.
10. **`status`**: by weight, or the request's. The last login follows its `login` kind: `recent` is placed back from the reference date by the square of a uniform draw (most are very recent), `stale` uniformly between the sign-up and a year before the reference date (the sign-up itself when the account is younger than a year), `never` is empty.
11. **`platform`**, **`locale`**, **`timezone`**: from the tables above and the country. The currency is country-generator's.

Times are ISO 8601 in UTC, to the second; the reference date is taken as midnight UTC, and the library never reads the clock.

## Known limitations

- The account tables are authored estimates. They show the right shape (more phones than desktops, iPhones strong in rich markets), not exact shares, and the status mix has no source.
- Passwords are plain text and are not credentials; do not use them as such. There are no hashes. The `random` style looks nothing like what people choose. The `human` style looks like it, but its words and shares are authored, short lists, not a breach dataset: it does not reproduce real frequencies, and a language that is not in `password_words.tsv` is missing from it. The policy's rules reshape what people would choose (a form that asks for a symbol gets `Dragon123!`), so with the default 12 to 16 characters the padding is often visible.
- Age does not condition the platform, the status or the sign-up time beyond the earliest date. An 80-year-old is as likely to be on a phone as a 20-year-old.
- The email and the username are built from the same name, so they resemble each other, as they often do. They are not otherwise linked.
- The address is independent of the phone number's area. A São Paulo address can have a Rio phone number.
- The same name library data applies: before name-generator 4.1.0 some cultures' lists held fragments, and a country with no names for a sex is refused. See `supported_countries()`.
- The library supports the countries the contact libraries cover (100 with the shipped data); the `lite` tier of some of them has few names or streets, so they repeat more.
