import * as fs from 'fs';
import { ICompilerResult, LLParse } from 'llparse';
import { Dot } from 'llparse-dot';
import {
  Fixture, FixtureResult, IFixtureBuildOptions,
} from 'llparse-test-fixture';
import * as path from 'path';

import * as llhttp from '../../src/llhttp';

export type Node = Parameters<LLParse['build']>['0'];

export { FixtureResult };

export type TestType = 'request' | 'response' | 'request-finish' | 'response-finish' |
  'request-lenient-all' | 'response-lenient-all' |
  'request-lenient-headers' | 'response-lenient-headers' |
  'request-lenient-chunked-length' | 'request-lenient-transfer-encoding' |
  'request-lenient-keep-alive' | 'response-lenient-keep-alive' |
  'request-lenient-version' | 'response-lenient-version' |
  'request-lenient-data-after-close' | 'response-lenient-data-after-close' |
  'request-lenient-optional-lf-after-cr' | 'response-lenient-optional-lf-after-cr' |
  'request-lenient-optional-cr-before-lf' | 'response-lenient-optional-cr-before-lf' |
  'request-lenient-optional-crlf-after-chunk' | 'response-lenient-optional-crlf-after-chunk' |
  'request-lenient-spaces-after-chunk-size' | 'response-lenient-spaces-after-chunk-size' |
  'none' | 'url';

export const allowedTypes: TestType[] = [
  'request',
  'response',
  'request-finish',
  'response-finish',
  'request-lenient-all',
  'response-lenient-all',
  'request-lenient-headers',
  'response-lenient-headers',
  'request-lenient-keep-alive',
  'response-lenient-keep-alive',
  'request-lenient-chunked-length',
  'request-lenient-transfer-encoding',
  'request-lenient-version',
  'response-lenient-version',
  'request-lenient-data-after-close',
  'response-lenient-data-after-close',
  'request-lenient-optional-lf-after-cr',
  'response-lenient-optional-lf-after-cr',
  'request-lenient-optional-cr-before-lf',
  'response-lenient-optional-cr-before-lf',
  'request-lenient-optional-crlf-after-chunk',
  'response-lenient-optional-crlf-after-chunk',
  'request-lenient-spaces-after-chunk-size',
  'response-lenient-spaces-after-chunk-size',
];

const BUILD_DIR = path.join(__dirname, '..', 'tmp');
const CHEADERS_FILE = path.join(BUILD_DIR, 'cheaders.h');

const cheaders = new llhttp.CHeaders().build();
try {
  fs.mkdirSync(BUILD_DIR);
} catch (e) {
  // no-op
}
fs.writeFileSync(CHEADERS_FILE, cheaders);

const fixtures = new Fixture({
  buildDir: path.join(__dirname, '..', 'tmp'),
  extra: [
    '-msse4.2',
    '-DLLHTTP__TEST',
    '-DLLPARSE__ERROR_PAUSE=' + llhttp.constants.ERROR.PAUSED,
    '-include', CHEADERS_FILE,
    path.join(__dirname, 'extra.c'),
  ],
  maxParallel: process.env.LLPARSE_DEBUG ? 1 : undefined,
});

const cache: Map<Node, ICompilerResult> = new Map();

export async function build(
  llparse: LLParse, node: Node, outFile: string,
  options: IFixtureBuildOptions = {},
  ty: TestType = 'none'): Promise<FixtureResult> {
  const dot = new Dot();
  fs.writeFileSync(path.join(BUILD_DIR, outFile + '.dot'),
    dot.build(node));

  let artifacts: ICompilerResult;
  if (cache.has(node)) {
    artifacts = cache.get(node)!;
  } else {
    artifacts = llparse.build(node, {
      c: { header: outFile },
      debug: process.env.LLPARSE_DEBUG ? 'llparse__debug' : undefined,
    });
    cache.set(node, artifacts);
  }

  const extra = options.extra === undefined ? [] : options.extra.slice();

  if (allowedTypes.includes(ty)) {
    extra.push(
      `-DLLPARSE__TEST_INIT=llhttp__test_init_${ty.replace(/-/g, '_')}`);
  }

  if (ty === 'request-finish' || ty === 'response-finish') {
    if (ty === 'request-finish') {
      extra.push('-DLLPARSE__TEST_INIT=llhttp__test_init_request');
    } else {
      extra.push('-DLLPARSE__TEST_INIT=llhttp__test_init_response');
    }
    extra.push('-DLLPARSE__TEST_FINISH=llhttp__test_finish');
  }

  return await fixtures.build(artifacts, outFile, Object.assign(options, {
    extra,
  }));
}
