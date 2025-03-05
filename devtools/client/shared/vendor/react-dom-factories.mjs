'use strict';

/**
 * Copyright (c) 2015-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

import React from "resource://devtools/client/shared/vendor/react.mjs";

/**
 * Create a factory that creates HTML tag elements.
 */
function createDOMFactory(type) {
  var factory = React.createElement.bind(null, type);
  // Expose the type on the factory and the prototype so that it can be
  // easily accessed on elements. E.g. `<Foo />.type === Foo`.
  // This should not be named `constructor` since this may not be the function
  // that created the element, and it may not even be a constructor.
  factory.type = type;
  return factory;
};

/**
 * Creates a mapping from supported HTML tags to `ReactDOMComponent` classes.
 */
export const a = createDOMFactory('a');
export const abbr = createDOMFactory('abbr');
export const address = createDOMFactory('address');
export const area = createDOMFactory('area');
export const article = createDOMFactory('article');
export const aside = createDOMFactory('aside');
export const audio = createDOMFactory('audio');
export const b = createDOMFactory('b');
export const base = createDOMFactory('base');
export const bdi = createDOMFactory('bdi');
export const bdo = createDOMFactory('bdo');
export const big = createDOMFactory('big');
export const blockquote = createDOMFactory('blockquote');
export const body = createDOMFactory('body');
export const br = createDOMFactory('br');
export const button = createDOMFactory('button');
export const canvas = createDOMFactory('canvas');
export const caption = createDOMFactory('caption');
export const cite = createDOMFactory('cite');
export const code = createDOMFactory('code');
export const col = createDOMFactory('col');
export const colgroup = createDOMFactory('colgroup');
export const data = createDOMFactory('data');
export const datalist = createDOMFactory('datalist');
export const dd = createDOMFactory('dd');
export const del = createDOMFactory('del');
export const details = createDOMFactory('details');
export const dfn = createDOMFactory('dfn');
export const dialog = createDOMFactory('dialog');
export const div = createDOMFactory('div');
export const dl = createDOMFactory('dl');
export const dt = createDOMFactory('dt');
export const em = createDOMFactory('em');
export const embed = createDOMFactory('embed');
export const fieldset = createDOMFactory('fieldset');
export const figcaption = createDOMFactory('figcaption');
export const figure = createDOMFactory('figure');
export const footer = createDOMFactory('footer');
export const form = createDOMFactory('form');
export const h1 = createDOMFactory('h1');
export const h2 = createDOMFactory('h2');
export const h3 = createDOMFactory('h3');
export const h4 = createDOMFactory('h4');
export const h5 = createDOMFactory('h5');
export const h6 = createDOMFactory('h6');
export const head = createDOMFactory('head');
export const header = createDOMFactory('header');
export const hgroup = createDOMFactory('hgroup');
export const hr = createDOMFactory('hr');
export const html = createDOMFactory('html');
export const i = createDOMFactory('i');
export const iframe = createDOMFactory('iframe');
export const img = createDOMFactory('img');
export const input = createDOMFactory('input');
export const ins = createDOMFactory('ins');
export const kbd = createDOMFactory('kbd');
export const keygen = createDOMFactory('keygen');
export const label = createDOMFactory('label');
export const legend = createDOMFactory('legend');
export const li = createDOMFactory('li');
export const link = createDOMFactory('link');
export const main = createDOMFactory('main');
export const map = createDOMFactory('map');
export const mark = createDOMFactory('mark');
export const menu = createDOMFactory('menu');
export const menuitem = createDOMFactory('menuitem');
export const meta = createDOMFactory('meta');
export const meter = createDOMFactory('meter');
export const nav = createDOMFactory('nav');
export const noscript = createDOMFactory('noscript');
export const object = createDOMFactory('object');
export const ol = createDOMFactory('ol');
export const optgroup = createDOMFactory('optgroup');
export const option = createDOMFactory('option');
export const output = createDOMFactory('output');
export const p = createDOMFactory('p');
export const param = createDOMFactory('param');
export const picture = createDOMFactory('picture');
export const pre = createDOMFactory('pre');
export const progress = createDOMFactory('progress');
export const q = createDOMFactory('q');
export const rp = createDOMFactory('rp');
export const rt = createDOMFactory('rt');
export const ruby = createDOMFactory('ruby');
export const s = createDOMFactory('s');
export const samp = createDOMFactory('samp');
export const script = createDOMFactory('script');
export const section = createDOMFactory('section');
export const select = createDOMFactory('select');
export const small = createDOMFactory('small');
export const source = createDOMFactory('source');
export const span = createDOMFactory('span');
export const strong = createDOMFactory('strong');
export const style = createDOMFactory('style');
export const sub = createDOMFactory('sub');
export const summary = createDOMFactory('summary');
export const sup = createDOMFactory('sup');
export const table = createDOMFactory('table');
export const tbody = createDOMFactory('tbody');
export const td = createDOMFactory('td');
export const textarea = createDOMFactory('textarea');
export const tfoot = createDOMFactory('tfoot');
export const th = createDOMFactory('th');
export const thead = createDOMFactory('thead');
export const time = createDOMFactory('time');
export const title = createDOMFactory('title');
export const tr = createDOMFactory('tr');
export const track = createDOMFactory('track');
export const u = createDOMFactory('u');
export const ul = createDOMFactory('ul');
//export const var = createDOMFactory('var');
export const video = createDOMFactory('video');
export const wbr = createDOMFactory('wbr');

export const circle = createDOMFactory('circle');
export const clipPath = createDOMFactory('clipPath');
export const defs = createDOMFactory('defs');
export const ellipse = createDOMFactory('ellipse');
export const g = createDOMFactory('g');
export const image = createDOMFactory('image');
export const line = createDOMFactory('line');
export const linearGradient = createDOMFactory('linearGradient');
export const mask = createDOMFactory('mask');
export const path = createDOMFactory('path');
export const pattern = createDOMFactory('pattern');
export const polygon = createDOMFactory('polygon');
export const polyline = createDOMFactory('polyline');
export const radialGradient = createDOMFactory('radialGradient');
export const rect = createDOMFactory('rect');
export const stop = createDOMFactory('stop');
export const svg = createDOMFactory('svg');
export const text = createDOMFactory('text');
export const tspan = createDOMFactory('tspan');
