import * as assert from 'node:assert';
import { describe, test } from 'node:test';
import * as fs from 'fs';
import { LLParse } from 'llparse';
import { Group, MDGator, Metadata, Test } from 'mdgator';
import * as path from 'path';
import * as vm from 'vm';

import * as llhttp from '../src/llhttp';
import { allowedTypes, build, FixtureResult, Node, TestType } from './fixtures';

//
// Cache nodes/llparse instances ahead of time
// (different types of tests will re-use them)
//

const modeCache = new Map<string, FixtureResult>();

function buildNode() {
  const p = new LLParse();
  const instance = new llhttp.HTTP(p);

  return { llparse: p, entry: instance.build().entry };
}

function buildURL() {
  const p = new LLParse();
  const instance = new llhttp.URL(p, true);

  const node = instance.build();

  // Loop
  node.exit.toHTTP.otherwise(node.entry.normal);
  node.exit.toHTTP09.otherwise(node.entry.normal);

  return { llparse: p, entry: node.entry.normal };
}

//
// Build binaries using cached nodes/llparse
//

async function buildMode(ty: TestType, meta: Metadata): Promise<FixtureResult> {
  const cacheKey = `${ty}:${JSON.stringify(meta || {})}`;
  let entry = modeCache.get(cacheKey);

  if (entry) {
    return entry;
  }

  let node: { llparse: LLParse; entry: Node };
  let prefix: string;
  let extra: string[];
  if (ty === 'url') {
    node = buildURL();
    prefix = 'url';
    extra = [];
  } else {
    node = buildNode();
    prefix = 'http';
    extra = [
      '-DLLHTTP__TEST_HTTP',
      path.join(__dirname, '..', 'src', 'native', 'http.c'),
    ];
  }

  if (meta.pause) {
    extra.push(`-DLLHTTP__TEST_PAUSE_${meta.pause.toUpperCase()}=1`);
  }

  if (meta.skipBody) {
    extra.push('-DLLHTTP__TEST_SKIP_BODY=1');
  }

  entry = await build(node.llparse, node.entry, `${prefix}-${ty}`, {
    extra,
  }, ty);

  modeCache.set(cacheKey, entry);
  return entry;
}

//
// Run test suite
//

function run(name: string): void {
  const md = new MDGator();

  const raw = fs.readFileSync(path.join(__dirname, name + '.md')).toString();
  const groups = md.parse(raw);

  function runSingleTest(ty: TestType, meta: Metadata,
                         input: string,
                         expected: ReadonlyArray<string | RegExp>): void {
    test(`should pass for type="${ty}"`, { timeout: 60000 }, async () => {
      const binary = await buildMode(ty, meta);
      await binary.check(input, expected, {
        noScan: meta.noScan === true,
      });
    });
  }

  function runTest(test: Test) {
    describe(test.name + ` at ${name}.md:${test.line + 1}`, () => {
      let types: TestType[] = [];

      const isURL = test.values.has('url');
      const inputKey = isURL ? 'url' : 'http';

      assert(test.values.has(inputKey),
        `Missing "${inputKey}" code in md file`);
      assert.strictEqual(test.values.get(inputKey)!.length, 1,
        `Expected just one "${inputKey}" input`);

      let meta: Metadata;
      if (test.meta.has(inputKey)) {
        meta = test.meta.get(inputKey)![0]!;
      } else {
        assert(isURL, 'Missing required http metadata');
        meta = {};
      }

      if (isURL) {
        types = [ 'url' ];
      } else {
        assert(Object.prototype.hasOwnProperty.call(meta, 'type'), 'Missing required `type` metadata');

        if (meta.type) {
          if (!allowedTypes.includes(meta.type)) {
            throw new Error(`Invalid value of \`type\` metadata: "${meta.type}"`);
          }

          types.push(meta.type);
        }
      }

      assert(test.values.has('log'), 'Missing `log` code in md file');

      assert.strictEqual(test.values.get('log')!.length, 1,
        'Expected just one output');

      let input: string = test.values.get(inputKey)![0];
      let expected: string = test.values.get('log')![0];

      // Remove trailing newline
      input = input.replace(/\n$/, '');

      // Remove escaped newlines
      input = input.replace(/\\(\r\n|\r|\n)/g, '');

      // Normalize all newlines
      input = input.replace(/\r\n|\r|\n/g, '\r\n');

      // Replace escaped CRLF, tabs, form-feed
      input = input.replace(/\\r/g, '\r');
      input = input.replace(/\\n/g, '\n');
      input = input.replace(/\\t/g, '\t');
      input = input.replace(/\\f/g, '\f');
      input = input.replace(/\\x([0-9a-fA-F]+)/g, (all, hex) => {
        return String.fromCharCode(parseInt(hex, 16));
      });

      // Useful in token tests
      input = input.replace(/\\([0-7]{1,3})/g, (_, digits) => {
        return String.fromCharCode(parseInt(digits, 8));
      });

      // Evaluate inline JavaScript
      input = input.replace(/\$\{(.+?)\}/g, (_, code) => {
        return vm.runInNewContext(code) + '';
      });

      // Escape first symbol `\r` or `\n`, `|`, `&` for Windows
      if (process.platform === 'win32') {
        const firstByte = Buffer.from(input)[0];
        if (firstByte === 0x0a || firstByte === 0x0d) {
          input = '\\' + input;
        }

        input = input.replace(/\|/g, '^|');
        input = input.replace(/&/g, '^&');
      }

      // Replace escaped tabs/form-feed in expected too
      expected = expected.replace(/\\t/g, '\t');
      expected = expected.replace(/\\f/g, '\f');

      // Split
      const expectedLines = expected.split(/\n/g).slice(0, -1);

      const fullExpected = expectedLines.map((line) => {
        if (line.startsWith('/')) {
          return new RegExp(line.trim().slice(1, -1));
        } else {
          return line;
        }
      });

      for (const ty of types) {
        if (meta.skip === true || (process.env.ONLY === 'true' && !meta.only)) {
          continue;
        }

        runSingleTest(ty, meta, input, fullExpected);
      }
    });
  }

  function runGroup(group: Group) {
    describe(group.name + ` at ${name}.md:${group.line + 1}`, function () {
      for (const child of group.children) {
        runGroup(child);
      }

      for (const test of group.tests) {
        runTest(test);
      }
    });
  }

  for (const group of groups) {
    runGroup(group);
  }
}

run('request/sample');
run('request/lenient-headers');
run('request/lenient-version');
run('request/method');
run('request/uri');
run('request/connection');
run('request/content-length');
run('request/transfer-encoding');
run('request/invalid');
run('request/finish');
run('request/pausing');
run('request/pipelining');

run('response/sample');
run('response/connection');
run('response/content-length');
run('response/transfer-encoding');
run('response/invalid');
run('response/finish');
run('response/lenient-version');
run('response/pausing');
run('response/pipelining');

run('url');
