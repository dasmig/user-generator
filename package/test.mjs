// Tests the assembled npm package's Node entry: node package/test.mjs build/package
// (package/browser-test.mjs tests the browser entry).
//
// The known answers are the native build's (the same lite data compiled in), so
// a pass means the WebAssembly build gives the same users as a native one.
import assert from 'node:assert/strict';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const dir = path.resolve(process.argv[2] || 'build/package');
const { createUserGenerator, createUserPool } = await import(pathToFileURL(path.join(dir, 'node.mjs')).href);
const users = await createUserGenerator();

// user::to_string(), from the JSON.
const summary = u => `${u.name} (${u.account.username}) <${u.email.address}>, ${u.country.name}`;
const signature = u => u.signature.seed + '-' + u.signature.data;

assert.equal(users.countries.length, 100);
assert.ok(users.countries.includes('BR') && !users.countries.includes('NZ'));
assert.equal(users.dataHash, 'c388d6c0928e1a6c');
assert.equal(users.versions['user-generator'].startsWith('v'), true, JSON.stringify(users.versions));
assert.equal(Object.keys(users.versions).length, 8);
assert.ok(Object.isFrozen(users) && Object.isFrozen(users.countries));

// Known answers.
const one = users.generate({ seed: 1 });
assert.equal(summary(one), 'Maurício Marques (marques.mauricio) <marques.mauricio1990@gmail.com>, Brazil');
assert.equal(signature(one), '0000000000000001-c388d6c0928e1a6c');
assert.equal(one.age, 32);
const two = users.generate({ seed: '2' });
assert.equal(summary(two), 'Zhang Hsiung (hsiungz1996) <hsiungz@qq.com>, China');
assert.equal(signature(two), '0000000000000002-c388d6c0928e1a6c');
const three = users.generate({ country: 'BR', sex: 'female', ages: { min: 25, max: 35 }, seed: 3n });
assert.equal(summary(three), 'Rosangela Bonfim (rosangelabonfim7322) <rosangela_bonfim96@gmail.com>, Brazil');
assert.equal(signature(three), '0000000000000003-c388d6c0928e1a6c');
const four = users.generate({ country: 'jp', referenceDate: '2030-03-09', seed: 4 });
assert.equal(summary(four), 'Takashi Taka (ttaka66) <taka_takashi@gmail.com>, Japan');
assert.equal(four.age, 63);
const five = users.generate({ country: 'US', safe: true, status: 'suspended', seed: 5 });
assert.equal(summary(five), 'Fabian Larson (fabianl) <larson49@example.org>, United States');
assert.equal(five.email.reserved, true);
assert.equal(five.account.status, 'suspended');

// The hash of 2,000 JSON documents, as the native build computes it.
let hash = 0xcbf29ce484222325n;
const mask = (1n << 64n) - 1n;
const encoder = new TextEncoder();
for (let s = 0; s < 2000; ++s) {
  for (const byte of encoder.encode(users.generateJson({ seed: s }))) hash = ((hash ^ BigInt(byte)) * 0x100000001b3n) & mask;
}
assert.equal(hash.toString(16), 'af4b69b98e8c0301');

// A random user replays from its seed.
const random = users.generate({ country: 'DE', referenceDate: new Date(2026, 9, 15) });
assert.equal(random.birth.date <= '2026-10-15', true);
assert.deepEqual(users.generate({ country: 'DE', referenceDate: '2026-10-15', seed: random.signature.seed }), random);
assert.notEqual(users.generate().signature.seed, users.generate().signature.seed);

// Requests.
for (let s = 0; s < 40; ++s) {
  const u = users.generate({ ages: { min: 30, max: 35 }, seed: s });
  assert.ok(u.age >= 30 && u.age <= 35);
  assert.equal(u.birth.age, u.age);
  assert.ok(u.account.created > u.birth.date);
}
const female = users.generate({ country: 'FR', sex: 'female', seed: 9 });
const male = users.generate({ country: 'FR', sex: 'male', seed: 9 });
assert.equal(female.sex, 'female');
assert.equal(male.sex, 'male');
assert.equal(users.generate({ seed: 'ffffffffffffffff' }).signature.seed, 'ffffffffffffffff');
assert.equal(users.generate({ seed: (1n << 64n) - 1n }).signature.seed, 'ffffffffffffffff');
assert.equal(users.generate({ seed: '0x2A' }).signature.seed, '000000000000002a');
assert.deepEqual(users.generate({ seed: '2a' }), users.generate({ seed: 42 }));
assert.equal(users.generate({ country: 'jp', seed: 1 }).country.code, 'JP');
for (const status of ['active', 'inactive', 'unverified', 'suspended']) {
  assert.equal(users.generate({ status, seed: 1 }).account.status, status);
}
assert.equal(users.generate({ status: 'unverified', seed: 6 }).account.last_login, null);

// Errors: the wrapper checks the request, so a malformed field is a
// TypeError and a value out of range is a RangeError, before the module runs.
for (const [request, kind, message] of [
  [{ country: 'XX' }, RangeError, /"XX" is not supported/],
  [{ country: 'NZ' }, RangeError, /not supported/],
  [{ country: ' br ' }, RangeError, /not supported/],
  [{ country: 7 }, TypeError, /country/],
  [{ contry: 'BR' }, TypeError, /unknown field "contry"/],
  [{ country: 'US', referenceDate: '2023-02-30' }, RangeError, /not a calendar date/],
  [{ referenceDate: '2023-2-3' }, RangeError, /YYYY-MM-DD/],
  [{ referenceDate: '1899-12-31' }, RangeError, /1900 and 2100/],
  [{ referenceDate: new Date(NaN) }, RangeError, /invalid Date/],
  [{ referenceDate: 20230701 }, TypeError, /referenceDate/],
  [{ ages: { min: 40, max: 20 } }, RangeError, /ages.min \(40\)/],
  [{ ages: { min: 20 } }, TypeError, /ages.max/],
  [{ ages: { min: 12, max: 20 } }, RangeError, /ages.min/],
  [{ ages: { min: 20, max: 121 } }, RangeError, /ages.max/],
  [{ safe: 1 }, TypeError, /safe/],
  [{ status: 'banned' }, RangeError, /status/],
  [{ status: 1 }, TypeError, /status/],
  [{ traits: 3 }, TypeError, /unknown field "traits"/],
  [{ sex: 'other' }, RangeError, /sex/],
  [{ sex: 1 }, TypeError, /sex/],
  [{ seed: '10000000000000000' }, RangeError, /hexadecimal/],
  [{ seed: 'xyz' }, RangeError, /hexadecimal/],
  [{ seed: -1 }, RangeError, /seed/],
  [{ seed: 2 ** 53 }, RangeError, /bigint/],
  [{ seed: 1n << 64n }, RangeError, /seed/],
  [{ seed: {} }, TypeError, /seed/],
  [null, TypeError, /object/],
  [[], TypeError, /object/]
]) {
  assert.throws(() => users.generate(request), error => error instanceof kind && message.test(error.message),
    JSON.stringify(request, (_, v) => typeof v === 'bigint' ? v.toString() : v));
}
// A failed call leaves the generator usable.
assert.equal(signature(users.generate({ seed: 1 })), '0000000000000001-c388d6c0928e1a6c');

// Batches. User i of a batch is the single call with the seed
// core::item_seed(batchSeed, i): splitmix64_mix(seed ^ splitmix64_mix(i ^ 0x6a09e667f3bcc909)).
const mix = z => {
  z = ((z ^ (z >> 30n)) * 0xbf58476d1ce4e5b9n) & mask;
  z = ((z ^ (z >> 27n)) * 0x94d049bb133111ebn) & mask;
  return z ^ (z >> 31n);
};
const itemSeed = (batchSeed, index) => mix(batchSeed ^ mix(BigInt(index) ^ 0x6a09e667f3bcc909n));

const request = { country: 'BR', sex: 'female', ages: { min: 20, max: 40 }, safe: true };
const batch = users.generateBatch({ request, count: 60, seed: 7 });
assert.equal(batch.length, 60);
for (let i = 0; i < batch.length; ++i) {
  assert.deepEqual(batch[i], users.generate({ ...request, seed: itemSeed(7n, i) }), `user ${i}`);
}
// A range is generated directly, and two batches that follow each other equal one.
assert.deepEqual(users.generateBatch({ request, count: 20, first: 40, seed: 7 }), batch.slice(40));
assert.deepEqual([...users.generateBatch({ request, count: 25, seed: '7' }),
  ...users.generateBatch({ request, count: 35, first: 25, seed: 7n })], batch);
// JSON Lines: one user per line, as the C++ library writes it.
const lines = users.generateBatchJsonl({ request, count: 60, seed: 7 });
assert.ok(lines.endsWith('\n'));
assert.equal(lines.split('\n').length, 61);
assert.deepEqual(lines.slice(0, -1).split('\n').map(line => JSON.parse(line)), batch);
assert.deepEqual(users.generateBatch({ count: 0, seed: 1 }), []);
assert.equal(users.generateBatchJsonl({ count: 0 }), '');
// Without a seed the batch is random but each user replays from its signature.
const unseeded = users.generateBatch({ count: 3 });
assert.notEqual(unseeded[0].signature.seed, unseeded[1].signature.seed);
assert.deepEqual(users.generate({ seed: unseeded[2].signature.seed }), unseeded[2]);
for (const [options, kind, message] of [
  [{}, TypeError, /count is required/],
  [{ count: 1.5 }, TypeError, /count/],
  [{ count: -1 }, RangeError, /count/],
  [{ count: 2 ** 31 }, RangeError, /count/],
  [{ count: 1, first: -1 }, RangeError, /first/],
  [{ count: 1, first: 2 ** 53 }, RangeError, /first/],
  [{ count: 1, seed: 'xyz' }, RangeError, /hexadecimal/],
  [{ count: 1, request: { seed: 1 } }, TypeError, /own seed/],
  [{ count: 1, request: { country: 'XX' } }, RangeError, /not supported/],
  [{ count: 1, request: { contry: 'BR' } }, TypeError, /unknown field/],
  [{ count: 1, extra: 1 }, TypeError, /unknown batch field/],
  [null, TypeError, /object/]
]) {
  assert.throws(() => users.generateBatch(options), error => error instanceof kind && message.test(error.message),
    JSON.stringify(options));
}

// Pools of 1, 2 and 4 workers give the same users as one generator.
const poolOptions = { request: { sex: 'male' }, count: 203, first: 11, seed: 'abc123' };
const expected = users.generateBatchJsonl(poolOptions);
for (const workers of [1, 2, 4]) {
  const pool = await createUserPool({ workers });
  try {
    assert.equal(pool.size, workers);
    assert.equal(await pool.generateBatchJsonl(poolOptions), expected, `${workers} workers`);
    assert.deepEqual(await pool.generateBatch({ ...poolOptions, count: 3 }),
      users.generateBatch({ ...poolOptions, count: 3 }));
    assert.deepEqual(await pool.generateBatch({ count: 0 }), []);
    // Fewer users than workers, and two calls at once.
    const [few, other] = await Promise.all([pool.generateBatchJsonl({ ...poolOptions, count: 1 }),
      pool.generateBatchJsonl({ ...poolOptions, count: 5 })]);
    assert.equal(few, expected.slice(0, expected.indexOf('\n') + 1));
    assert.equal(other, expected.split('\n').slice(0, 5).join('\n') + '\n');
    await assert.rejects(pool.generateBatch({ count: 1, request: { country: 'XX' } }), RangeError);
    await assert.rejects(pool.generateBatch({ count: 1, seed: 'xyz' }), RangeError);
    assert.equal(await pool.generateBatchJsonl(poolOptions), expected, 'still usable after errors');
  } finally {
    pool.close();
  }
  await assert.rejects(pool.generateBatch({ count: 1 }), /closed/);
}
await assert.rejects(createUserPool({ workers: 0 }), RangeError);
await assert.rejects(createUserPool({ workers: 1.5 }), TypeError);
await assert.rejects(createUserPool({ threads: 2 }), TypeError);

console.log('PASS: @dasmig/user matches the native known answers (5 users, 2,000-document hash), replay, requests, errors, batches and worker pools.');
