// The generator over the WebAssembly module's C API (wasm/user_api.cpp),
// shared by the browser entry (index.mjs) and the Node entry (node.mjs). It
// checks the request, passes it on and parses the JSON that comes back.
import { versions } from './versions.mjs';
import { batchPlan, integer, malformed, outOfRange, seedText } from './batch.mjs';

const keys = new Set(['country', 'sex', 'ages', 'referenceDate', 'safe', 'status', 'device', 'seed']);
const sexes = new Set(['male', 'female']);
const statuses = new Set(['active', 'inactive', 'unverified', 'suspended']);

function dateText(date) {
  if (date === undefined) return '';
  let year, month, day;
  if (date instanceof Date) {
    if (Number.isNaN(date.getTime())) outOfRange('referenceDate is an invalid Date');
    [year, month, day] = [date.getFullYear(), date.getMonth() + 1, date.getDate()];
  } else if (typeof date === 'string') {
    const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(date);
    if (!match) outOfRange(`referenceDate must be YYYY-MM-DD, not "${date}"`);
    [year, month, day] = match.slice(1).map(Number);
    const check = new Date(Date.UTC(year, month - 1, day));
    if (check.getUTCMonth() !== month - 1 || check.getUTCDate() !== day) outOfRange(`referenceDate ${date} is not a calendar date`);
  } else {
    return malformed('referenceDate must be a YYYY-MM-DD string or a Date');
  }
  if (year < 1900 || year > 2100) outOfRange('referenceDate must be between 1900 and 2100');
  const pad = n => String(n).padStart(2, '0');
  return `${year}-${pad(month)}-${pad(day)}`;
}

function parse(json) {
  const result = JSON.parse(json);
  if (result.error) throw new Error(result.error);
  return result;
}

/**
 * Wrap an instantiated module.
 * @param {Function} createModule The Emscripten factory (user.mjs).
 * @param {object} moduleOptions Emscripten module options.
 */
export async function createGenerator(createModule, moduleOptions) {
  const module = await createModule(moduleOptions);
  const generate = module.cwrap('dasmig_user_generate', 'string',
    ['string', 'string', 'number', 'number', 'string', 'number', 'string', 'number', 'string']);
  const generateBatchRaw = module.cwrap('dasmig_user_generate_batch', 'string',
    ['string', 'string', 'number', 'number', 'string', 'number', 'string', 'number', 'string', 'number', 'number']);
  const requestKeys = new Set([...keys].filter(key => key !== 'seed'));
  const info = parse(module.cwrap('dasmig_user_info', 'string', [])());
  const countries = Object.freeze([...info.countries]);
  const supported = new Set(countries);

  // The request's fields as the module's arguments (everything but the seed).
  function requestArgs(request, allowed) {
    if (request === null || typeof request !== 'object' || Array.isArray(request)) malformed('must be an object');
    for (const key of Object.keys(request))
      if (!allowed.has(key)) malformed(`unknown field "${key}"; the fields are ${[...allowed].join(', ')}`);
    const { country, sex, ages, referenceDate, safe, status, device } = request;

    let code = '';
    if (country !== undefined) {
      if (typeof country !== 'string') malformed('country must be a string');
      code = country.toUpperCase();
      if (!supported.has(code))
        outOfRange(`country "${country}" is not supported; the supported countries are the generator's countries list`);
    }
    if (sex !== undefined && !sexes.has(sex))
      (typeof sex === 'string' ? outOfRange : malformed)(`sex must be "male" or "female", not ${JSON.stringify(sex)}`);
    let min = -1, max = -1;
    if (ages !== undefined) {
      if (ages === null || typeof ages !== 'object') malformed('ages must be { min, max }');
      min = integer(ages.min, 'ages.min', 13, 120);
      max = integer(ages.max, 'ages.max', 13, 120);
      if (min > max) outOfRange(`ages.min (${min}) must not be greater than ages.max (${max})`);
    }
    if (safe !== undefined && typeof safe !== 'boolean') malformed('safe must be a boolean');
    if (device !== undefined && typeof device !== 'boolean') malformed('device must be a boolean');
    if (status !== undefined) {
      if (typeof status !== 'string') malformed('status must be a string');
      if (!statuses.has(status)) outOfRange(`status must be one of ${[...statuses].join(', ')}, not "${status}"`);
    }
    return [code, sex ?? '', min, max, dateText(referenceDate), safe ? 1 : 0, status ?? '', device ? 1 : 0];
  }

  function generateJson(request = {}) {
    return generate(...requestArgs(request, keys), seedText(request.seed));
  }

  function generateBatchJsonl(options) {
    const plan = batchPlan(options);
    const text = generateBatchRaw(...requestArgs(plan.request, requestKeys), plan.seed, plan.first, plan.count);
    if (text.startsWith('{"error"')) throw new Error(JSON.parse(text).error);
    return text;
  }

  return Object.freeze({
    countries,
    dataHash: info.data,
    deviceDataHash: info.device_data,
    versions,
    generateJson(request) {
      const json = generateJson(request);
      parse(json);
      return json;
    },
    generate(request) {
      return parse(generateJson(request));
    },
    generateBatchJsonl,
    generateBatch(options) {
      const text = generateBatchJsonl(options);
      return text === '' ? [] : text.slice(0, -1).split('\n').map(line => JSON.parse(line));
    }
  });
}
