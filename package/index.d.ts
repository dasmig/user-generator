// Types for the dasmig user generator. The JSON shapes are those of the
// C++ libraries' to_json(); see each library's doc/usage.md.

/** What a user call asks for. Every field is optional: what is left out is drawn. */
export interface UserRequest {
  /**
   * ISO 3166-1 alpha-2 code, any case, from the generator's `countries`; another code is a RangeError.
   * Unset draws one of `countries`, weighted by population.
   */
  country?: string;
  /** Unset draws it with the birth. */
  sex?: Sex;
  /**
   * Inclusive age range in completed years, 13 (the age at which a person signs up) to 120.
   * Default 18 to 80.
   */
  ages?: { min: number; max: number };
  /**
   * The date ages and account dates are computed at: `YYYY-MM-DD`, or a Date (its local calendar date).
   * Default 2023-07-01. The generator never reads the clock, so pass today's date for ages "as of now".
   */
  referenceDate?: string | Date;
  /**
   * Make the contacts unable to reach anyone: reserved email domains (`example.org`) and, where a
   * country has one, a phone number from the range regulators set aside for fiction.
   */
  safe?: boolean;
  /** Force the account status. Unset draws one. */
  status?: AccountStatus;
  /**
   * The call seed, a 64-bit unsigned integer: a bigint, a non-negative safe integer, or a string
   * of 1-16 **hexadecimal** digits with an optional `0x` (so `'42'` is 0x42, not 42), the form of
   * `signature.seed`. Unset draws a random one. The same seed, request and data give the same user.
   */
  seed?: string | bigint | number;
}

export type Sex = 'male' | 'female';
export type AccountStatus = 'active' | 'inactive' | 'unverified' | 'suspended';

/** The seed a result was generated from and a hash of the data it used (16 hex digits each). */
export interface Signature {
  seed: string;
  data: string;
}

export interface Birth {
  country_code: string;
  /** YYYY-MM-DD. */
  date: string;
  year: number;
  month: number;
  day: number;
  /** Completed years at the reference date. */
  age: number;
  sex: Sex;
  /** 0 = Sunday. */
  weekday: number;
  /** Remaining life expectancy at that age, years (a population model, not a forecast). */
  le_remaining: number;
  cohort: string;
  signature: Signature;
}

export interface Address {
  country_code: string;
  city: string;
  region: string;
  street: string;
  number: number;
  unit: string;
  postcode: string;
  /** Whether the street exists in the place's real street list. */
  real_street: boolean;
  geonameid: number;
  /** The address as lines, in the country's order. */
  lines: string[];
  /** The address on one line. */
  formatted: string;
  signature: Signature;
}

export interface Phone {
  country_code: string;
  type: string;
  e164: string;
  international: string;
  national: string;
  /** True for a number from the range set aside for fiction. */
  reserved: boolean;
  signature: Signature;
}

export interface Email {
  country_code: string;
  address: string;
  local: string;
  domain: string;
  kind: string;
  /** True for a reserved domain that reaches no one. */
  reserved: boolean;
  signature: Signature;
}

export interface Account {
  username: string;
  /** Plain text, 12 to 16 characters; test data, never a real credential. */
  password: string;
  /** Sign-up time, ISO 8601 UTC (`2021-03-14T09:26:53Z`). */
  created: string;
  /** Last login, ISO 8601 UTC; null for an account that never logged in. */
  last_login: string | null;
  status: AccountStatus;
  /** Platform key, for example `ios-phone`. */
  platform: string;
  /** `phone`, `desktop`, `tablet` or `other`. */
  device: string;
  os: string;
  /** BCP 47 tag, for example `pt-BR`. */
  locale: string;
  /** IANA name, or a UTC offset where no zone is listed. */
  timezone: string;
  /** ISO 4217 code. */
  currency: string;
}

/** One user: each library's result, as that library writes it. */
export interface User {
  name: string;
  given: string;
  /** Empty when the name has one part. */
  family: string;
  sex: Sex;
  age: number;
  country: { code: string; name: string };
  birth: Birth;
  address: Address;
  phone: Phone;
  email: Email;
  account: Account;
  /** `generate({ ...request, seed: user.signature.seed })` replays this user. */
  signature: Signature;
}

export interface UserGenerator {
  /** The countries a user can be generated for (ISO 3166-1 alpha-2, uppercase, sorted). */
  readonly countries: readonly string[];
  /**
   * Hash of the data of all eight libraries (16 hex digits). A saved seed replays the same user
   * only while this is unchanged; it changes when a package update changes any library's data.
   */
  readonly dataHash: string;
  /** The library versions (git tags) this module was built from. */
  readonly versions: Readonly<Record<string, string>>;
  /**
   * Generate a user.
   * @throws {TypeError} A request field of the wrong type, or an unknown field.
   * @throws {RangeError} A field outside its range: an unsupported country, ages, status, date or seed.
   */
  generate(request?: UserRequest): User;
  /** Generate a user as the JSON text the module returns, exactly as the C++ library writes it. */
  generateJson(request?: UserRequest): string;
  /**
   * Generate a batch on this thread. User `i` is `generate({ ...request, seed })` for the seed
   * derived from the batch seed and `first + i`, so any user replays on its own, a range can be
   * generated alone, and two batches that follow each other equal one. For large batches use a
   * pool (`createUserPool`), which splits the work across workers with the same result. A batch
   * is not made unique: to avoid repeated emails or usernames, skip repeats and generate more.
   * @throws {TypeError} A field of the wrong type, an unknown field, or a seed inside the request.
   * @throws {RangeError} A count, first index, seed, country or other field out of range.
   */
  generateBatch(options: BatchOptions): User[];
  /** The same batch as JSON Lines: one user per line, each line ending in `\n`. Empty for a count of 0. */
  generateBatchJsonl(options: BatchOptions): string;
}

/** What a batch asks for. */
export interface BatchOptions {
  /** What every user is generated from (the same fields as `generate`, but no `seed`). */
  request?: Omit<UserRequest, 'seed'>;
  /** Number of users, 0 to 2^31 - 1. */
  count: number;
  /**
   * The batch seed, in the forms `UserRequest.seed` takes. Unset draws a random one, so the batch
   * cannot be repeated; each user's `signature.seed` still replays that user alone.
   */
  seed?: string | bigint | number;
  /** Index of the first user, default 0: `first: 1000` starts at user 1000 of the same batch. */
  first?: number;
}

/** A pool of workers that generates batches in pieces and joins them in order. */
export interface UserPool {
  /** Number of workers. */
  readonly size: number;
  /** Same users as `UserGenerator.generateBatch` with the same options, for any pool size. */
  generateBatch(options: BatchOptions): Promise<User[]>;
  /** Same text as `UserGenerator.generateBatchJsonl`. */
  generateBatchJsonl(options: BatchOptions): Promise<string>;
  /** Stop the workers. The pool cannot be used afterwards. */
  close(): void;
}

export interface PoolOptions {
  /**
   * Number of workers, 1 to 64. Each loads its own copy of the module and takes a second or two to
   * start. Default: the number of cores, at most 4.
   */
  workers?: number;
}

/** Emscripten module options this package passes on. */
export interface ModuleOptions {
  /** Map a file the module loads (`user.wasm`) to its URL. */
  locateFile?: (path: string, prefix: string) => string;
  /** The contents of user.wasm, to skip fetching it. */
  wasmBinary?: ArrayBuffer | Uint8Array;
  [option: string]: unknown;
}

/**
 * Load the WebAssembly module and its data, and return a generator. Loading takes a second or
 * two and blocks the thread it runs on, so create one generator, reuse it, and in a browser
 * prefer a Web Worker.
 */
export function createUserGenerator(moduleOptions?: ModuleOptions): Promise<UserGenerator>;

/**
 * Start a pool of Web Workers (worker threads in Node), each with its own module. Resolves when all
 * are ready. Call `close()` when done.
 */
export function createUserPool(options?: PoolOptions): Promise<UserPool>;
