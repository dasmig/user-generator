# @dasmig/user

[![npm](https://img.shields.io/npm/v/@dasmig/user.svg)](https://www.npmjs.com/package/@dasmig/user)
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/dasmig/user-generator/blob/main/LICENSE.MIT)
[![node](https://img.shields.io/node/v/@dasmig/user.svg)](https://www.npmjs.com/package/@dasmig/user)
[![types](https://img.shields.io/badge/types-included-blue.svg)](https://www.npmjs.com/package/@dasmig/user)

Fictional user records from one seeded call: name, birth date, postal address, phone number, email address, and an account with username, password, sign-up and last-login times, status, device, locale and time zone, and, when asked, the device of the last sign-in with its user agent and IP address. It is [user-generator](https://github.com/dasmig/user-generator) (C++23) compiled to WebAssembly, with the `lite` data of the nine libraries it composes built in, so nothing is fetched at run time but the module's own `user.wasm`.

The output is fictional test data, not a description of a real person. Passwords are plain text for the account's policy and never real credentials; `safe: true` makes emails and (where a country has a range) phone numbers that reach no one.

```js
import { createUserGenerator } from '@dasmig/user';

createUserGenerator().then(users => {
  const u = users.generate({ country: 'BR', sex: 'female', ages: { min: 25, max: 40 }, safe: true });
  console.log(u.name, u.email.address, u.account.username, u.address.formatted);

  // The same seed, request and data give the same user, on every platform.
  const again = users.generate({ country: 'BR', sex: 'female', ages: { min: 25, max: 40 }, safe: true, seed: u.signature.seed });
});
```

## Install

```
npm install @dasmig/user
```

The package is an ES module only (no CommonJS build), for browsers, Web Workers and Node 20+.

It ships its own TypeScript types (`index.d.ts`) and the WebAssembly module (`user.wasm`), which bundlers such as Vite, webpack and esbuild pick up from `new URL('user.wasm', import.meta.url)`; pass `locateFile` or `wasmBinary` to `createUserGenerator()` to load it some other way. It has no dependencies.

## Loading

`createUserGenerator()` instantiates the module and loads the data of all eight libraries (device-generator's is loaded by the first user with a device). That takes **a second or two** and blocks the thread it runs on. So:

- **Create one generator and reuse it.**
- **In a browser, create it in a Web Worker** so the page stays responsive, and post the users (plain JSON) back:

```js
// user-worker.js, started with new Worker(new URL('./user-worker.js', import.meta.url), { type: 'module' })
import { createUserGenerator } from '@dasmig/user';

const ready = createUserGenerator();
onmessage = async ({ data: request }) => {
  const users = await ready;
  try {
    postMessage({ user: users.generate(request) });
  } catch (error) {
    postMessage({ error: error.message });
  }
};
```

In a browser the module finds `user.wasm` next to itself. To serve the file from elsewhere, pass Emscripten's `locateFile`, or pass its bytes as `wasmBinary`:

```js
const users = await createUserGenerator({ locateFile: file => '/assets/' + file });
```

In Node, the package's `node` export reads `user.wasm` from disk. Serve the module compressed.

## Request

Every field is optional. What is left out is drawn; what is given conditions the draws that depend on it. The country sets the name culture, address, phone number, email domain, locale and time zone; the age and sex set the rest.

| Field | Type | Default |
| --- | --- | --- |
| `country` | An ISO 3166-1 alpha-2 code from `users.countries` (100 countries), any case | One of `users.countries`, weighted by population |
| `sex` | `'male'` or `'female'` | Drawn with the birth |
| `ages` | `{ min, max }`, completed years, 13-120 | 18 to 80 |
| `referenceDate` | `'YYYY-MM-DD'` or a `Date` (its local date), 1900-2100 | `'2023-07-01'` |
| `safe` | `true` for contacts that reach no one | `false` |
| `status` | `'active'`, `'inactive'`, `'unverified'` or `'suspended'` | Drawn |
| `device` | `true` to add the device of the last sign-in | `false` |
| `seed` | A `bigint`, a non-negative safe integer, or a **hexadecimal** string | Random |

- **Countries.** Only the countries in `users.countries` have data in all the libraries, so another code is an error rather than a user with a name from the wrong culture.
- **Dates.** The generator never reads the clock. Pass `referenceDate: new Date()` for ages and account dates as of today, and keep the date with the seed if the user must replay.
- **Device.** `device: true` adds `device`, from [device-generator](https://github.com/dasmig/device-generator): a model, OS and browser version that fit together at the last login (the sign-up time for an account that never logged in; a date before 2021 is seen as 2021-01-01), the user agent and client hints that combination sends, the `Accept-Language` header of the account's locale, a screen and an IP address in the country. It uses the account's platform, so the two agree; with `safe: true` the address is from a documentation range. Nothing else in the user changes.
- **Seeds.** A seed is a 64-bit unsigned integer. A string is read as hexadecimal, optionally after `0x`, which is the form of `signature.seed`: `seed: '42'` is 0x42 (66), not 42.

An invalid request throws before anything is generated: a `TypeError` for a field of the wrong type or an unknown field, and a `RangeError` for a value out of range. The message says which field and why. A failed call leaves the generator usable.

## Result

`generate()` returns a `User` (see `index.d.ts`). It has `name`, `given`, `family`, `sex`, `age`, `country` and `signature`, each library's own result under `birth`, `address`, `phone` and `email`, the `account`, and, with `device: true`, the `device`. `generateJson()` returns the same as JSON text, exactly as the C++ library writes it.

Each part draws from its own random stream, split from the user's seed. Forcing one choice (the status, say) does not shift the unrelated parts.

## Batches

For test data, generate many users from one batch seed:

```js
// 10,000 users, as objects or as JSON Lines (one user per line).
const people = users.generateBatch({ request: { country: 'BR', safe: true }, count: 10000, seed: 42 });
const lines = users.generateBatchJsonl({ request: { country: 'BR', safe: true }, count: 10000, seed: 42 });

// The same users, split across workers (Web Workers in a browser, worker threads in Node).
import { createUserPool } from '@dasmig/user';
const pool = await createUserPool({ workers: 4 });
const same = await pool.generateBatchJsonl({ request: { country: 'BR', safe: true }, count: 10000, seed: 42 });
pool.close();
```

- **User `i` of a batch** is exactly `generate({ ...request, seed })` for the seed derived from the batch seed and `i` (`core::item_seed` in the C++ libraries). So any user replays on its own from its `signature.seed`, a range can be generated alone (`first: 1000` starts at user 1000), two batches that follow each other equal one, and the output is the same for any pool size.
- **Options.** `request` (the fields above except `seed`), `count` (0 to 2^31 - 1), `seed` (a random one if left out, which makes the batch unrepeatable) and `first` (default 0).
- **Uniqueness.** A batch is not made unique, because the pieces of a split batch cannot see each other. Emails and usernames are drawn with digits and variants, so repeats are rare in the thousands and common in the millions; skip repeats and generate more, or use the C++ library or `dasmig-gen user --unique email`, which retries.
- **Memory.** Every worker loads its own module, and a pool defaults to the number of cores, at most 4. The pool needs no `SharedArrayBuffer`, so it works on any page: pieces travel as strings.

## Replaying users

To recreate a user later, store its `signature.seed` and the request it was made with, including `referenceDate`.

The same seed and request give the same user **only with the same data**. `users.dataHash` identifies the data of all eight libraries, `users.deviceDataHash` device-generator's, and `users.versions` lists the library versions the module was built from. Store `dataHash` with the seed (and `deviceDataHash` for users with a device): if a package update changes it, the saved seed gives a different user.

Each part of a user also has a `signature`, with its own seed and its library's data hash.

## Data

The data comes from open datasets and from tables written for the libraries (statuses, platform shares by country, locales and time zones, device models, browser shares and user-agent formats). `DATA_LICENSES.txt` lists each source and its licence. Each library documents its data's limits in its own `doc/data.md`; the account tables' estimates are described in [user-generator's data notes](https://github.com/dasmig/user-generator/blob/HEAD/doc/data.md).

## Versions

The package version is its own. `users.versions` lists the user-generator release and the library releases this build was made from, and each package release is tagged `package-v<version>` in the [repository](https://github.com/dasmig/user-generator/releases). The same seed gives the same user only for the same library versions, so a library update that changes output is a major version of the package.

Source: [user-generator/package](https://github.com/dasmig/user-generator/tree/main/package). Issues: [github.com/dasmig/user-generator/issues](https://github.com/dasmig/user-generator/issues).

## Licence

MIT for the code; see `DATA_LICENSES.txt` for the data.
