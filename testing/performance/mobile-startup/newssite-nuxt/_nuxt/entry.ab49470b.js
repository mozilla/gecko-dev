function Ci(e, t) {
  const s = Object.create(null),
    a = e.split(",");
  for (let i = 0; i < a.length; i++) s[a[i]] = !0;
  return t ? i => !!s[i.toLowerCase()] : i => !!s[i];
}
const fe = {},
  Vt = [],
  Je = () => {},
  Sc = () => !1,
  Ic = /^on[^a-z]/,
  Os = e => Ic.test(e),
  Si = e => e.startsWith("onUpdate:"),
  ke = Object.assign,
  Ii = (e, t) => {
    const s = e.indexOf(t);
    s > -1 && e.splice(s, 1);
  },
  Tc = Object.prototype.hasOwnProperty,
  ie = (e, t) => Tc.call(e, t),
  G = Array.isArray,
  Wt = e => Us(e) === "[object Map]",
  Cl = e => Us(e) === "[object Set]",
  Rc = e => Us(e) === "[object RegExp]",
  ee = e => typeof e == "function",
  be = e => typeof e == "string",
  Ti = e => typeof e == "symbol",
  ge = e => e !== null && typeof e == "object",
  Sl = e => ge(e) && ee(e.then) && ee(e.catch),
  Il = Object.prototype.toString,
  Us = e => Il.call(e),
  Mc = e => Us(e).slice(8, -1),
  Tl = e => Us(e) === "[object Object]",
  Ri = e => be(e) && e !== "NaN" && e[0] !== "-" && "" + parseInt(e, 10) === e,
  ys = Ci(
    ",key,ref,ref_for,ref_key,onVnodeBeforeMount,onVnodeMounted,onVnodeBeforeUpdate,onVnodeUpdated,onVnodeBeforeUnmount,onVnodeUnmounted"
  ),
  ja = e => {
    const t = Object.create(null);
    return s => t[s] || (t[s] = e(s));
  },
  Nc = /-(\w)/g,
  lt = ja(e => e.replace(Nc, (t, s) => (s ? s.toUpperCase() : ""))),
  Oc = /\B([A-Z])/g,
  as = ja(e => e.replace(Oc, "-$1").toLowerCase()),
  xa = ja(e => e.charAt(0).toUpperCase() + e.slice(1)),
  Ua = ja(e => (e ? `on${xa(e)}` : "")),
  As = (e, t) => !Object.is(e, t),
  vs = (e, t) => {
    for (let s = 0; s < e.length; s++) e[s](t);
  },
  ca = (e, t, s) => {
    Object.defineProperty(e, t, { configurable: !0, enumerable: !1, value: s });
  },
  Uc = e => {
    const t = parseFloat(e);
    return isNaN(t) ? e : t;
  },
  Rl = e => {
    const t = be(e) ? Number(e) : NaN;
    return isNaN(t) ? e : t;
  };
let hn;
const ti = () =>
  hn ||
  (hn =
    typeof globalThis < "u"
      ? globalThis
      : typeof self < "u"
        ? self
        : typeof window < "u"
          ? window
          : typeof global < "u"
            ? global
            : {});
function qa(e) {
  if (G(e)) {
    const t = {};
    for (let s = 0; s < e.length; s++) {
      const a = e[s],
        i = be(a) ? Dc(a) : qa(a);
      if (i) for (const n in i) t[n] = i[n];
    }
    return t;
  } else {
    if (be(e)) return e;
    if (ge(e)) return e;
  }
}
const Lc = /;(?![^(]*\))/g,
  Hc = /:([^]+)/,
  Fc = /\/\*[^]*?\*\//g;
function Dc(e) {
  const t = {};
  return (
    e
      .replace(Fc, "")
      .split(Lc)
      .forEach(s => {
        if (s) {
          const a = s.split(Hc);
          a.length > 1 && (t[a[0].trim()] = a[1].trim());
        }
      }),
    t
  );
}
function E(e) {
  let t = "";
  if (be(e)) t = e;
  else if (G(e))
    for (let s = 0; s < e.length; s++) {
      const a = E(e[s]);
      a && (t += a + " ");
    }
  else if (ge(e)) for (const s in e) e[s] && (t += s + " ");
  return t.trim();
}
function _y(e) {
  if (!e) return null;
  let { class: t, style: s } = e;
  return t && !be(t) && (e.class = E(t)), s && (e.style = qa(s)), e;
}
const zc =
    "itemscope,allowfullscreen,formnovalidate,ismap,nomodule,novalidate,readonly",
  Bc = Ci(zc);
function Ml(e) {
  return !!e || e === "";
}
const ye = e =>
    be(e)
      ? e
      : e == null
        ? ""
        : G(e) || (ge(e) && (e.toString === Il || !ee(e.toString)))
          ? JSON.stringify(e, Nl, 2)
          : String(e),
  Nl = (e, t) =>
    t && t.__v_isRef
      ? Nl(e, t.value)
      : Wt(t)
        ? {
            [`Map(${t.size})`]: [...t.entries()].reduce(
              (s, [a, i]) => ((s[`${a} =>`] = i), s),
              {}
            ),
          }
        : Cl(t)
          ? { [`Set(${t.size})`]: [...t.values()] }
          : ge(t) && !G(t) && !Tl(t)
            ? String(t)
            : t;
let Ye;
class Qc {
  constructor(t = !1) {
    (this.detached = t),
      (this._active = !0),
      (this.effects = []),
      (this.cleanups = []),
      (this.parent = Ye),
      !t && Ye && (this.index = (Ye.scopes || (Ye.scopes = [])).push(this) - 1);
  }
  get active() {
    return this._active;
  }
  run(t) {
    if (this._active) {
      const s = Ye;
      try {
        return (Ye = this), t();
      } finally {
        Ye = s;
      }
    }
  }
  on() {
    Ye = this;
  }
  off() {
    Ye = this.parent;
  }
  stop(t) {
    if (this._active) {
      let s, a;
      for (s = 0, a = this.effects.length; s < a; s++) this.effects[s].stop();
      for (s = 0, a = this.cleanups.length; s < a; s++) this.cleanups[s]();
      if (this.scopes)
        for (s = 0, a = this.scopes.length; s < a; s++) this.scopes[s].stop(!0);
      if (!this.detached && this.parent && !t) {
        const i = this.parent.scopes.pop();
        i &&
          i !== this &&
          ((this.parent.scopes[this.index] = i), (i.index = this.index));
      }
      (this.parent = void 0), (this._active = !1);
    }
  }
}
function Vc(e, t = Ye) {
  t && t.active && t.effects.push(e);
}
function Wc() {
  return Ye;
}
const Mi = e => {
    const t = new Set(e);
    return (t.w = 0), (t.n = 0), t;
  },
  Ol = e => (e.w & qt) > 0,
  Ul = e => (e.n & qt) > 0,
  Yc = ({ deps: e }) => {
    if (e.length) for (let t = 0; t < e.length; t++) e[t].w |= qt;
  },
  $c = e => {
    const { deps: t } = e;
    if (t.length) {
      let s = 0;
      for (let a = 0; a < t.length; a++) {
        const i = t[a];
        Ol(i) && !Ul(i) ? i.delete(e) : (t[s++] = i),
          (i.w &= ~qt),
          (i.n &= ~qt);
      }
      t.length = s;
    }
  },
  oa = new WeakMap();
let fs = 0,
  qt = 1;
const si = 30;
let $e;
const Tt = Symbol(""),
  ai = Symbol("");
class Ni {
  constructor(t, s = null, a) {
    (this.fn = t),
      (this.scheduler = s),
      (this.active = !0),
      (this.deps = []),
      (this.parent = void 0),
      Vc(this, a);
  }
  run() {
    if (!this.active) return this.fn();
    let t = $e,
      s = jt;
    for (; t; ) {
      if (t === this) return;
      t = t.parent;
    }
    try {
      return (
        (this.parent = $e),
        ($e = this),
        (jt = !0),
        (qt = 1 << ++fs),
        fs <= si ? Yc(this) : dn(this),
        this.fn()
      );
    } finally {
      fs <= si && $c(this),
        (qt = 1 << --fs),
        ($e = this.parent),
        (jt = s),
        (this.parent = void 0),
        this.deferStop && this.stop();
    }
  }
  stop() {
    $e === this
      ? (this.deferStop = !0)
      : this.active &&
        (dn(this), this.onStop && this.onStop(), (this.active = !1));
  }
}
function dn(e) {
  const { deps: t } = e;
  if (t.length) {
    for (let s = 0; s < t.length; s++) t[s].delete(e);
    t.length = 0;
  }
}
let jt = !0;
const Ll = [];
function is() {
  Ll.push(jt), (jt = !1);
}
function ns() {
  const e = Ll.pop();
  jt = e === void 0 ? !0 : e;
}
function Le(e, t, s) {
  if (jt && $e) {
    let a = oa.get(e);
    a || oa.set(e, (a = new Map()));
    let i = a.get(s);
    i || a.set(s, (i = Mi())), Hl(i);
  }
}
function Hl(e, t) {
  let s = !1;
  fs <= si ? Ul(e) || ((e.n |= qt), (s = !Ol(e))) : (s = !e.has($e)),
    s && (e.add($e), $e.deps.push(e));
}
function ut(e, t, s, a, i, n) {
  const l = oa.get(e);
  if (!l) return;
  let c = [];
  if (t === "clear") c = [...l.values()];
  else if (s === "length" && G(e)) {
    const r = Number(a);
    l.forEach((o, u) => {
      (u === "length" || u >= r) && c.push(o);
    });
  } else
    switch ((s !== void 0 && c.push(l.get(s)), t)) {
      case "add":
        G(e)
          ? Ri(s) && c.push(l.get("length"))
          : (c.push(l.get(Tt)), Wt(e) && c.push(l.get(ai)));
        break;
      case "delete":
        G(e) || (c.push(l.get(Tt)), Wt(e) && c.push(l.get(ai)));
        break;
      case "set":
        Wt(e) && c.push(l.get(Tt));
        break;
    }
  if (c.length === 1) c[0] && ii(c[0]);
  else {
    const r = [];
    for (const o of c) o && r.push(...o);
    ii(Mi(r));
  }
}
function ii(e, t) {
  const s = G(e) ? e : [...e];
  for (const a of s) a.computed && gn(a);
  for (const a of s) a.computed || gn(a);
}
function gn(e, t) {
  (e !== $e || e.allowRecurse) && (e.scheduler ? e.scheduler() : e.run());
}
function Kc(e, t) {
  var s;
  return (s = oa.get(e)) == null ? void 0 : s.get(t);
}
const Jc = Ci("__proto__,__v_isRef,__isVue"),
  Fl = new Set(
    Object.getOwnPropertyNames(Symbol)
      .filter(e => e !== "arguments" && e !== "caller")
      .map(e => Symbol[e])
      .filter(Ti)
  ),
  Zc = Oi(),
  Gc = Oi(!1, !0),
  Xc = Oi(!0),
  pn = eo();
function eo() {
  const e = {};
  return (
    ["includes", "indexOf", "lastIndexOf"].forEach(t => {
      e[t] = function (...s) {
        const a = le(this);
        for (let n = 0, l = this.length; n < l; n++) Le(a, "get", n + "");
        const i = a[t](...s);
        return i === -1 || i === !1 ? a[t](...s.map(le)) : i;
      };
    }),
    ["push", "pop", "shift", "unshift", "splice"].forEach(t => {
      e[t] = function (...s) {
        is();
        const a = le(this)[t].apply(this, s);
        return ns(), a;
      };
    }),
    e
  );
}
function to(e) {
  const t = le(this);
  return Le(t, "has", e), t.hasOwnProperty(e);
}
function Oi(e = !1, t = !1) {
  return function (a, i, n) {
    if (i === "__v_isReactive") return !e;
    if (i === "__v_isReadonly") return e;
    if (i === "__v_isShallow") return t;
    if (i === "__v_raw" && n === (e ? (t ? _o : Vl) : t ? Ql : Bl).get(a))
      return a;
    const l = G(a);
    if (!e) {
      if (l && ie(pn, i)) return Reflect.get(pn, i, n);
      if (i === "hasOwnProperty") return to;
    }
    const c = Reflect.get(a, i, n);
    return (Ti(i) ? Fl.has(i) : Jc(i)) || (e || Le(a, "get", i), t)
      ? c
      : Ee(c)
        ? l && Ri(i)
          ? c
          : c.value
        : ge(c)
          ? e
            ? Yl(c)
            : Ge(c)
          : c;
  };
}
const so = Dl(),
  ao = Dl(!0);
function Dl(e = !1) {
  return function (s, a, i, n) {
    let l = s[a];
    if (Ut(l) && Ee(l) && !Ee(i)) return !1;
    if (
      !e &&
      (!ua(i) && !Ut(i) && ((l = le(l)), (i = le(i))), !G(s) && Ee(l) && !Ee(i))
    )
      return (l.value = i), !0;
    const c = G(s) && Ri(a) ? Number(a) < s.length : ie(s, a),
      r = Reflect.set(s, a, i, n);
    return (
      s === le(n) && (c ? As(i, l) && ut(s, "set", a, i) : ut(s, "add", a, i)),
      r
    );
  };
}
function io(e, t) {
  const s = ie(e, t);
  e[t];
  const a = Reflect.deleteProperty(e, t);
  return a && s && ut(e, "delete", t, void 0), a;
}
function no(e, t) {
  const s = Reflect.has(e, t);
  return (!Ti(t) || !Fl.has(t)) && Le(e, "has", t), s;
}
function lo(e) {
  return Le(e, "iterate", G(e) ? "length" : Tt), Reflect.ownKeys(e);
}
const zl = { get: Zc, set: so, deleteProperty: io, has: no, ownKeys: lo },
  ro = {
    get: Xc,
    set(e, t) {
      return !0;
    },
    deleteProperty(e, t) {
      return !0;
    },
  },
  co = ke({}, zl, { get: Gc, set: ao }),
  Ui = e => e,
  ka = e => Reflect.getPrototypeOf(e);
function Ws(e, t, s = !1, a = !1) {
  e = e.__v_raw;
  const i = le(e),
    n = le(t);
  s || (t !== n && Le(i, "get", t), Le(i, "get", n));
  const { has: l } = ka(i),
    c = a ? Ui : s ? Fi : Cs;
  if (l.call(i, t)) return c(e.get(t));
  if (l.call(i, n)) return c(e.get(n));
  e !== i && e.get(t);
}
function Ys(e, t = !1) {
  const s = this.__v_raw,
    a = le(s),
    i = le(e);
  return (
    t || (e !== i && Le(a, "has", e), Le(a, "has", i)),
    e === i ? s.has(e) : s.has(e) || s.has(i)
  );
}
function $s(e, t = !1) {
  return (
    (e = e.__v_raw), !t && Le(le(e), "iterate", Tt), Reflect.get(e, "size", e)
  );
}
function fn(e) {
  e = le(e);
  const t = le(this);
  return ka(t).has.call(t, e) || (t.add(e), ut(t, "add", e, e)), this;
}
function bn(e, t) {
  t = le(t);
  const s = le(this),
    { has: a, get: i } = ka(s);
  let n = a.call(s, e);
  n || ((e = le(e)), (n = a.call(s, e)));
  const l = i.call(s, e);
  return (
    s.set(e, t), n ? As(t, l) && ut(s, "set", e, t) : ut(s, "add", e, t), this
  );
}
function _n(e) {
  const t = le(this),
    { has: s, get: a } = ka(t);
  let i = s.call(t, e);
  i || ((e = le(e)), (i = s.call(t, e))), a && a.call(t, e);
  const n = t.delete(e);
  return i && ut(t, "delete", e, void 0), n;
}
function yn() {
  const e = le(this),
    t = e.size !== 0,
    s = e.clear();
  return t && ut(e, "clear", void 0, void 0), s;
}
function Ks(e, t) {
  return function (a, i) {
    const n = this,
      l = n.__v_raw,
      c = le(l),
      r = t ? Ui : e ? Fi : Cs;
    return (
      !e && Le(c, "iterate", Tt), l.forEach((o, u) => a.call(i, r(o), r(u), n))
    );
  };
}
function Js(e, t, s) {
  return function (...a) {
    const i = this.__v_raw,
      n = le(i),
      l = Wt(n),
      c = e === "entries" || (e === Symbol.iterator && l),
      r = e === "keys" && l,
      o = i[e](...a),
      u = s ? Ui : t ? Fi : Cs;
    return (
      !t && Le(n, "iterate", r ? ai : Tt),
      {
        next() {
          const { value: m, done: d } = o.next();
          return d
            ? { value: m, done: d }
            : { value: c ? [u(m[0]), u(m[1])] : u(m), done: d };
        },
        [Symbol.iterator]() {
          return this;
        },
      }
    );
  };
}
function pt(e) {
  return function (...t) {
    return e === "delete" ? !1 : this;
  };
}
function oo() {
  const e = {
      get(n) {
        return Ws(this, n);
      },
      get size() {
        return $s(this);
      },
      has: Ys,
      add: fn,
      set: bn,
      delete: _n,
      clear: yn,
      forEach: Ks(!1, !1),
    },
    t = {
      get(n) {
        return Ws(this, n, !1, !0);
      },
      get size() {
        return $s(this);
      },
      has: Ys,
      add: fn,
      set: bn,
      delete: _n,
      clear: yn,
      forEach: Ks(!1, !0),
    },
    s = {
      get(n) {
        return Ws(this, n, !0);
      },
      get size() {
        return $s(this, !0);
      },
      has(n) {
        return Ys.call(this, n, !0);
      },
      add: pt("add"),
      set: pt("set"),
      delete: pt("delete"),
      clear: pt("clear"),
      forEach: Ks(!0, !1),
    },
    a = {
      get(n) {
        return Ws(this, n, !0, !0);
      },
      get size() {
        return $s(this, !0);
      },
      has(n) {
        return Ys.call(this, n, !0);
      },
      add: pt("add"),
      set: pt("set"),
      delete: pt("delete"),
      clear: pt("clear"),
      forEach: Ks(!0, !0),
    };
  return (
    ["keys", "values", "entries", Symbol.iterator].forEach(n => {
      (e[n] = Js(n, !1, !1)),
        (s[n] = Js(n, !0, !1)),
        (t[n] = Js(n, !1, !0)),
        (a[n] = Js(n, !0, !0));
    }),
    [e, s, t, a]
  );
}
const [uo, mo, ho, go] = oo();
function Li(e, t) {
  const s = t ? (e ? go : ho) : e ? mo : uo;
  return (a, i, n) =>
    i === "__v_isReactive"
      ? !e
      : i === "__v_isReadonly"
        ? e
        : i === "__v_raw"
          ? a
          : Reflect.get(ie(s, i) && i in a ? s : a, i, n);
}
const po = { get: Li(!1, !1) },
  fo = { get: Li(!1, !0) },
  bo = { get: Li(!0, !1) },
  Bl = new WeakMap(),
  Ql = new WeakMap(),
  Vl = new WeakMap(),
  _o = new WeakMap();
function yo(e) {
  switch (e) {
    case "Object":
    case "Array":
      return 1;
    case "Map":
    case "Set":
    case "WeakMap":
    case "WeakSet":
      return 2;
    default:
      return 0;
  }
}
function vo(e) {
  return e.__v_skip || !Object.isExtensible(e) ? 0 : yo(Mc(e));
}
function Ge(e) {
  return Ut(e) ? e : Hi(e, !1, zl, po, Bl);
}
function Wl(e) {
  return Hi(e, !1, co, fo, Ql);
}
function Yl(e) {
  return Hi(e, !0, ro, bo, Vl);
}
function Hi(e, t, s, a, i) {
  if (!ge(e) || (e.__v_raw && !(t && e.__v_isReactive))) return e;
  const n = i.get(e);
  if (n) return n;
  const l = vo(e);
  if (l === 0) return e;
  const c = new Proxy(e, l === 2 ? a : s);
  return i.set(e, c), c;
}
function Yt(e) {
  return Ut(e) ? Yt(e.__v_raw) : !!(e && e.__v_isReactive);
}
function Ut(e) {
  return !!(e && e.__v_isReadonly);
}
function ua(e) {
  return !!(e && e.__v_isShallow);
}
function $l(e) {
  return Yt(e) || Ut(e);
}
function le(e) {
  const t = e && e.__v_raw;
  return t ? le(t) : e;
}
function Kl(e) {
  return ca(e, "__v_skip", !0), e;
}
const Cs = e => (ge(e) ? Ge(e) : e),
  Fi = e => (ge(e) ? Yl(e) : e);
function Jl(e) {
  jt && $e && ((e = le(e)), Hl(e.dep || (e.dep = Mi())));
}
function Zl(e, t) {
  e = le(e);
  const s = e.dep;
  s && ii(s);
}
function Ee(e) {
  return !!(e && e.__v_isRef === !0);
}
function Qe(e) {
  return Gl(e, !1);
}
function Ss(e) {
  return Gl(e, !0);
}
function Gl(e, t) {
  return Ee(e) ? e : new wo(e, t);
}
class wo {
  constructor(t, s) {
    (this.__v_isShallow = s),
      (this.dep = void 0),
      (this.__v_isRef = !0),
      (this._rawValue = s ? t : le(t)),
      (this._value = s ? t : Cs(t));
  }
  get value() {
    return Jl(this), this._value;
  }
  set value(t) {
    const s = this.__v_isShallow || ua(t) || Ut(t);
    (t = s ? t : le(t)),
      As(t, this._rawValue) &&
        ((this._rawValue = t), (this._value = s ? t : Cs(t)), Zl(this));
  }
}
function pe(e) {
  return Ee(e) ? e.value : e;
}
const jo = {
  get: (e, t, s) => pe(Reflect.get(e, t, s)),
  set: (e, t, s, a) => {
    const i = e[t];
    return Ee(i) && !Ee(s) ? ((i.value = s), !0) : Reflect.set(e, t, s, a);
  },
};
function Xl(e) {
  return Yt(e) ? e : new Proxy(e, jo);
}
class xo {
  constructor(t, s, a) {
    (this._object = t),
      (this._key = s),
      (this._defaultValue = a),
      (this.__v_isRef = !0);
  }
  get value() {
    const t = this._object[this._key];
    return t === void 0 ? this._defaultValue : t;
  }
  set value(t) {
    this._object[this._key] = t;
  }
  get dep() {
    return Kc(le(this._object), this._key);
  }
}
class qo {
  constructor(t) {
    (this._getter = t), (this.__v_isRef = !0), (this.__v_isReadonly = !0);
  }
  get value() {
    return this._getter();
  }
}
function er(e, t, s) {
  return Ee(e)
    ? e
    : ee(e)
      ? new qo(e)
      : ge(e) && arguments.length > 1
        ? ko(e, t, s)
        : Qe(e);
}
function ko(e, t, s) {
  const a = e[t];
  return Ee(a) ? a : new xo(e, t, s);
}
class Po {
  constructor(t, s, a, i) {
    (this._setter = s),
      (this.dep = void 0),
      (this.__v_isRef = !0),
      (this.__v_isReadonly = !1),
      (this._dirty = !0),
      (this.effect = new Ni(t, () => {
        this._dirty || ((this._dirty = !0), Zl(this));
      })),
      (this.effect.computed = this),
      (this.effect.active = this._cacheable = !i),
      (this.__v_isReadonly = a);
  }
  get value() {
    const t = le(this);
    return (
      Jl(t),
      (t._dirty || !t._cacheable) &&
        ((t._dirty = !1), (t._value = t.effect.run())),
      t._value
    );
  }
  set value(t) {
    this._setter(t);
  }
}
function Eo(e, t, s = !1) {
  let a, i;
  const n = ee(e);
  return (
    n ? ((a = e), (i = Je)) : ((a = e.get), (i = e.set)),
    new Po(a, i, n || !i, s)
  );
}
function xt(e, t, s, a) {
  let i;
  try {
    i = a ? e(...a) : e();
  } catch (n) {
    ls(n, t, s);
  }
  return i;
}
function Ve(e, t, s, a) {
  if (ee(e)) {
    const n = xt(e, t, s, a);
    return (
      n &&
        Sl(n) &&
        n.catch(l => {
          ls(l, t, s);
        }),
      n
    );
  }
  const i = [];
  for (let n = 0; n < e.length; n++) i.push(Ve(e[n], t, s, a));
  return i;
}
function ls(e, t, s, a = !0) {
  const i = t ? t.vnode : null;
  if (t) {
    let n = t.parent;
    const l = t.proxy,
      c = s;
    for (; n; ) {
      const o = n.ec;
      if (o) {
        for (let u = 0; u < o.length; u++) if (o[u](e, l, c) === !1) return;
      }
      n = n.parent;
    }
    const r = t.appContext.config.errorHandler;
    if (r) {
      xt(r, null, 10, [e, l, c]);
      return;
    }
  }
  Ao(e, s, i, a);
}
function Ao(e, t, s, a = !0) {
  console.error(e);
}
let Is = !1,
  ni = !1;
const Ie = [];
let nt = 0;
const $t = [];
let ot = null,
  Ct = 0;
const tr = Promise.resolve();
let Di = null;
function Lt(e) {
  const t = Di || tr;
  return e ? t.then(this ? e.bind(this) : e) : t;
}
function Co(e) {
  let t = nt + 1,
    s = Ie.length;
  for (; t < s; ) {
    const a = (t + s) >>> 1;
    Ts(Ie[a]) < e ? (t = a + 1) : (s = a);
  }
  return t;
}
function Pa(e) {
  (!Ie.length || !Ie.includes(e, Is && e.allowRecurse ? nt + 1 : nt)) &&
    (e.id == null ? Ie.push(e) : Ie.splice(Co(e.id), 0, e), sr());
}
function sr() {
  !Is && !ni && ((ni = !0), (Di = tr.then(ir)));
}
function So(e) {
  const t = Ie.indexOf(e);
  t > nt && Ie.splice(t, 1);
}
function ar(e) {
  G(e)
    ? $t.push(...e)
    : (!ot || !ot.includes(e, e.allowRecurse ? Ct + 1 : Ct)) && $t.push(e),
    sr();
}
function vn(e, t = Is ? nt + 1 : 0) {
  for (; t < Ie.length; t++) {
    const s = Ie[t];
    s && s.pre && (Ie.splice(t, 1), t--, s());
  }
}
function ma(e) {
  if ($t.length) {
    const t = [...new Set($t)];
    if ((($t.length = 0), ot)) {
      ot.push(...t);
      return;
    }
    for (ot = t, ot.sort((s, a) => Ts(s) - Ts(a)), Ct = 0; Ct < ot.length; Ct++)
      ot[Ct]();
    (ot = null), (Ct = 0);
  }
}
const Ts = e => (e.id == null ? 1 / 0 : e.id),
  Io = (e, t) => {
    const s = Ts(e) - Ts(t);
    if (s === 0) {
      if (e.pre && !t.pre) return -1;
      if (t.pre && !e.pre) return 1;
    }
    return s;
  };
function ir(e) {
  (ni = !1), (Is = !0), Ie.sort(Io);
  const t = Je;
  try {
    for (nt = 0; nt < Ie.length; nt++) {
      const s = Ie[nt];
      s && s.active !== !1 && xt(s, null, 14);
    }
  } finally {
    (nt = 0),
      (Ie.length = 0),
      ma(),
      (Is = !1),
      (Di = null),
      (Ie.length || $t.length) && ir();
  }
}
function To(e, t, ...s) {
  if (e.isUnmounted) return;
  const a = e.vnode.props || fe;
  let i = s;
  const n = t.startsWith("update:"),
    l = n && t.slice(7);
  if (l && l in a) {
    const u = `${l === "modelValue" ? "model" : l}Modifiers`,
      { number: m, trim: d } = a[u] || fe;
    d && (i = s.map(_ => (be(_) ? _.trim() : _))), m && (i = s.map(Uc));
  }
  let c,
    r = a[(c = Ua(t))] || a[(c = Ua(lt(t)))];
  !r && n && (r = a[(c = Ua(as(t)))]), r && Ve(r, e, 6, i);
  const o = a[c + "Once"];
  if (o) {
    if (!e.emitted) e.emitted = {};
    else if (e.emitted[c]) return;
    (e.emitted[c] = !0), Ve(o, e, 6, i);
  }
}
function nr(e, t, s = !1) {
  const a = t.emitsCache,
    i = a.get(e);
  if (i !== void 0) return i;
  const n = e.emits;
  let l = {},
    c = !1;
  if (!ee(e)) {
    const r = o => {
      const u = nr(o, t, !0);
      u && ((c = !0), ke(l, u));
    };
    !s && t.mixins.length && t.mixins.forEach(r),
      e.extends && r(e.extends),
      e.mixins && e.mixins.forEach(r);
  }
  return !n && !c
    ? (ge(e) && a.set(e, null), null)
    : (G(n) ? n.forEach(r => (l[r] = null)) : ke(l, n),
      ge(e) && a.set(e, l),
      l);
}
function Ea(e, t) {
  return !e || !Os(t)
    ? !1
    : ((t = t.slice(2).replace(/Once$/, "")),
      ie(e, t[0].toLowerCase() + t.slice(1)) || ie(e, as(t)) || ie(e, t));
}
let Ae = null,
  Aa = null;
function ha(e) {
  const t = Ae;
  return (Ae = e), (Aa = (e && e.type.__scopeId) || null), t;
}
function yy(e) {
  Aa = e;
}
function vy() {
  Aa = null;
}
function Xe(e, t = Ae, s) {
  if (!t || e._n) return e;
  const a = (...i) => {
    a._d && Mn(-1);
    const n = ha(t);
    let l;
    try {
      l = e(...i);
    } finally {
      ha(n), a._d && Mn(1);
    }
    return l;
  };
  return (a._n = !0), (a._c = !0), (a._d = !0), a;
}
function La(e) {
  const {
    type: t,
    vnode: s,
    proxy: a,
    withProxy: i,
    props: n,
    propsOptions: [l],
    slots: c,
    attrs: r,
    emit: o,
    render: u,
    renderCache: m,
    data: d,
    setupState: _,
    ctx: b,
    inheritAttrs: w,
  } = e;
  let R, f;
  const p = ha(e);
  try {
    if (s.shapeFlag & 4) {
      const v = i || a;
      (R = ze(u.call(v, v, m, n, _, d, b))), (f = r);
    } else {
      const v = t;
      (R = ze(
        v.length > 1 ? v(n, { attrs: r, slots: c, emit: o }) : v(n, null)
      )),
        (f = t.props ? r : Mo(r));
    }
  } catch (v) {
    (xs.length = 0), ls(v, e, 1), (R = Q(Me));
  }
  let q = R;
  if (f && w !== !1) {
    const v = Object.keys(f),
      { shapeFlag: C } = q;
    v.length && C & 7 && (l && v.some(Si) && (f = No(f, l)), (q = mt(q, f)));
  }
  return (
    s.dirs && ((q = mt(q)), (q.dirs = q.dirs ? q.dirs.concat(s.dirs) : s.dirs)),
    s.transition && (q.transition = s.transition),
    (R = q),
    ha(p),
    R
  );
}
function Ro(e) {
  let t;
  for (let s = 0; s < e.length; s++) {
    const a = e[s];
    if (Gt(a)) {
      if (a.type !== Me || a.children === "v-if") {
        if (t) return;
        t = a;
      }
    } else return;
  }
  return t;
}
const Mo = e => {
    let t;
    for (const s in e)
      (s === "class" || s === "style" || Os(s)) && ((t || (t = {}))[s] = e[s]);
    return t;
  },
  No = (e, t) => {
    const s = {};
    for (const a in e) (!Si(a) || !(a.slice(9) in t)) && (s[a] = e[a]);
    return s;
  };
function Oo(e, t, s) {
  const { props: a, children: i, component: n } = e,
    { props: l, children: c, patchFlag: r } = t,
    o = n.emitsOptions;
  if (t.dirs || t.transition) return !0;
  if (s && r >= 0) {
    if (r & 1024) return !0;
    if (r & 16) return a ? wn(a, l, o) : !!l;
    if (r & 8) {
      const u = t.dynamicProps;
      for (let m = 0; m < u.length; m++) {
        const d = u[m];
        if (l[d] !== a[d] && !Ea(o, d)) return !0;
      }
    }
  } else
    return (i || c) && (!c || !c.$stable)
      ? !0
      : a === l
        ? !1
        : a
          ? l
            ? wn(a, l, o)
            : !0
          : !!l;
  return !1;
}
function wn(e, t, s) {
  const a = Object.keys(t);
  if (a.length !== Object.keys(e).length) return !0;
  for (let i = 0; i < a.length; i++) {
    const n = a[i];
    if (t[n] !== e[n] && !Ea(s, n)) return !0;
  }
  return !1;
}
function zi({ vnode: e, parent: t }, s) {
  for (; t && t.subTree === e; ) ((e = t.vnode).el = s), (t = t.parent);
}
const lr = e => e.__isSuspense,
  Uo = {
    name: "Suspense",
    __isSuspense: !0,
    process(e, t, s, a, i, n, l, c, r, o) {
      e == null ? Lo(t, s, a, i, n, l, c, r, o) : Ho(e, t, s, a, i, l, c, r, o);
    },
    hydrate: Fo,
    create: Bi,
    normalize: Do,
  },
  rr = Uo;
function Rs(e, t) {
  const s = e.props && e.props[t];
  ee(s) && s();
}
function Lo(e, t, s, a, i, n, l, c, r) {
  const {
      p: o,
      o: { createElement: u },
    } = r,
    m = u("div"),
    d = (e.suspense = Bi(e, i, a, t, m, s, n, l, c, r));
  o(null, (d.pendingBranch = e.ssContent), m, null, a, d, n, l),
    d.deps > 0
      ? (Rs(e, "onPending"),
        Rs(e, "onFallback"),
        o(null, e.ssFallback, t, s, a, null, n, l),
        Kt(d, e.ssFallback))
      : d.resolve(!1, !0);
}
function Ho(e, t, s, a, i, n, l, c, { p: r, um: o, o: { createElement: u } }) {
  const m = (t.suspense = e.suspense);
  (m.vnode = t), (t.el = e.el);
  const d = t.ssContent,
    _ = t.ssFallback,
    { activeBranch: b, pendingBranch: w, isInFallback: R, isHydrating: f } = m;
  if (w)
    (m.pendingBranch = d),
      Ke(d, w)
        ? (r(w, d, m.hiddenContainer, null, i, m, n, l, c),
          m.deps <= 0
            ? m.resolve()
            : R && (r(b, _, s, a, i, null, n, l, c), Kt(m, _)))
        : (m.pendingId++,
          f ? ((m.isHydrating = !1), (m.activeBranch = w)) : o(w, i, m),
          (m.deps = 0),
          (m.effects.length = 0),
          (m.hiddenContainer = u("div")),
          R
            ? (r(null, d, m.hiddenContainer, null, i, m, n, l, c),
              m.deps <= 0
                ? m.resolve()
                : (r(b, _, s, a, i, null, n, l, c), Kt(m, _)))
            : b && Ke(d, b)
              ? (r(b, d, s, a, i, m, n, l, c), m.resolve(!0))
              : (r(null, d, m.hiddenContainer, null, i, m, n, l, c),
                m.deps <= 0 && m.resolve()));
  else if (b && Ke(d, b)) r(b, d, s, a, i, m, n, l, c), Kt(m, d);
  else if (
    (Rs(t, "onPending"),
    (m.pendingBranch = d),
    m.pendingId++,
    r(null, d, m.hiddenContainer, null, i, m, n, l, c),
    m.deps <= 0)
  )
    m.resolve();
  else {
    const { timeout: p, pendingId: q } = m;
    p > 0
      ? setTimeout(() => {
          m.pendingId === q && m.fallback(_);
        }, p)
      : p === 0 && m.fallback(_);
  }
}
function Bi(e, t, s, a, i, n, l, c, r, o, u = !1) {
  const {
    p: m,
    m: d,
    um: _,
    n: b,
    o: { parentNode: w, remove: R },
  } = o;
  let f;
  const p = zo(e);
  p && t != null && t.pendingBranch && ((f = t.pendingId), t.deps++);
  const q = e.props ? Rl(e.props.timeout) : void 0,
    v = {
      vnode: e,
      parent: t,
      parentComponent: s,
      isSVG: l,
      container: a,
      hiddenContainer: i,
      anchor: n,
      deps: 0,
      pendingId: 0,
      timeout: typeof q == "number" ? q : -1,
      activeBranch: null,
      pendingBranch: null,
      isInFallback: !0,
      isHydrating: u,
      isUnmounted: !1,
      effects: [],
      resolve(C = !1, O = !1) {
        const {
          vnode: M,
          activeBranch: x,
          pendingBranch: F,
          pendingId: W,
          effects: Z,
          parentComponent: z,
          container: X,
        } = v;
        if (v.isHydrating) v.isHydrating = !1;
        else if (!C) {
          const ae = x && F.transition && F.transition.mode === "out-in";
          ae &&
            (x.transition.afterLeave = () => {
              W === v.pendingId && d(F, X, ce, 0);
            });
          let { anchor: ce } = v;
          x && ((ce = b(x)), _(x, z, v, !0)), ae || d(F, X, ce, 0);
        }
        Kt(v, F), (v.pendingBranch = null), (v.isInFallback = !1);
        let V = v.parent,
          je = !1;
        for (; V; ) {
          if (V.pendingBranch) {
            V.effects.push(...Z), (je = !0);
            break;
          }
          V = V.parent;
        }
        je || ar(Z),
          (v.effects = []),
          p &&
            t &&
            t.pendingBranch &&
            f === t.pendingId &&
            (t.deps--, t.deps === 0 && !O && t.resolve()),
          Rs(M, "onResolve");
      },
      fallback(C) {
        if (!v.pendingBranch) return;
        const {
          vnode: O,
          activeBranch: M,
          parentComponent: x,
          container: F,
          isSVG: W,
        } = v;
        Rs(O, "onFallback");
        const Z = b(M),
          z = () => {
            v.isInFallback && (m(null, C, F, Z, x, null, W, c, r), Kt(v, C));
          },
          X = C.transition && C.transition.mode === "out-in";
        X && (M.transition.afterLeave = z),
          (v.isInFallback = !0),
          _(M, x, null, !0),
          X || z();
      },
      move(C, O, M) {
        v.activeBranch && d(v.activeBranch, C, O, M), (v.container = C);
      },
      next() {
        return v.activeBranch && b(v.activeBranch);
      },
      registerDep(C, O) {
        const M = !!v.pendingBranch;
        M && v.deps++;
        const x = C.vnode.el;
        C.asyncDep
          .catch(F => {
            ls(F, C, 0);
          })
          .then(F => {
            if (C.isUnmounted || v.isUnmounted || v.pendingId !== C.suspenseId)
              return;
            C.asyncResolved = !0;
            const { vnode: W } = C;
            hi(C, F, !1), x && (W.el = x);
            const Z = !x && C.subTree.el;
            O(C, W, w(x || C.subTree.el), x ? null : b(C.subTree), v, l, r),
              Z && R(Z),
              zi(C, W.el),
              M && --v.deps === 0 && v.resolve();
          });
      },
      unmount(C, O) {
        (v.isUnmounted = !0),
          v.activeBranch && _(v.activeBranch, s, C, O),
          v.pendingBranch && _(v.pendingBranch, s, C, O);
      },
    };
  return v;
}
function Fo(e, t, s, a, i, n, l, c, r) {
  const o = (t.suspense = Bi(
      t,
      a,
      s,
      e.parentNode,
      document.createElement("div"),
      null,
      i,
      n,
      l,
      c,
      !0
    )),
    u = r(e, (o.pendingBranch = t.ssContent), s, o, n, l);
  return o.deps === 0 && o.resolve(!1, !0), u;
}
function Do(e) {
  const { shapeFlag: t, children: s } = e,
    a = t & 32;
  (e.ssContent = jn(a ? s.default : s)),
    (e.ssFallback = a ? jn(s.fallback) : Q(Me));
}
function jn(e) {
  let t;
  if (ee(e)) {
    const s = Zt && e._c;
    s && ((e._d = !1), T()), (e = e()), s && ((e._d = !0), (t = Be), Mr());
  }
  return (
    G(e) && (e = Ro(e)),
    (e = ze(e)),
    t && !e.dynamicChildren && (e.dynamicChildren = t.filter(s => s !== e)),
    e
  );
}
function cr(e, t) {
  t && t.pendingBranch
    ? G(e)
      ? t.effects.push(...e)
      : t.effects.push(e)
    : ar(e);
}
function Kt(e, t) {
  e.activeBranch = t;
  const { vnode: s, parentComponent: a } = e,
    i = (s.el = t.el);
  a && a.subTree === s && ((a.vnode.el = i), zi(a, i));
}
function zo(e) {
  var t;
  return (
    ((t = e.props) == null ? void 0 : t.suspensible) != null &&
    e.props.suspensible !== !1
  );
}
function Bo(e, t) {
  return Qi(e, null, t);
}
const Zs = {};
function Rt(e, t, s) {
  return Qi(e, t, s);
}
function Qi(
  e,
  t,
  { immediate: s, deep: a, flush: i, onTrack: n, onTrigger: l } = fe
) {
  var c;
  const r = Wc() === ((c = qe) == null ? void 0 : c.scope) ? qe : null;
  let o,
    u = !1,
    m = !1;
  if (
    (Ee(e)
      ? ((o = () => e.value), (u = ua(e)))
      : Yt(e)
        ? ((o = () => e), (a = !0))
        : G(e)
          ? ((m = !0),
            (u = e.some(v => Yt(v) || ua(v))),
            (o = () =>
              e.map(v => {
                if (Ee(v)) return v.value;
                if (Yt(v)) return It(v);
                if (ee(v)) return xt(v, r, 2);
              })))
          : ee(e)
            ? t
              ? (o = () => xt(e, r, 2))
              : (o = () => {
                  if (!(r && r.isUnmounted)) return d && d(), Ve(e, r, 3, [_]);
                })
            : (o = Je),
    t && a)
  ) {
    const v = o;
    o = () => It(v());
  }
  let d,
    _ = v => {
      d = p.onStop = () => {
        xt(v, r, 4);
      };
    },
    b;
  if (es)
    if (
      ((_ = Je),
      t ? s && Ve(t, r, 3, [o(), m ? [] : void 0, _]) : o(),
      i === "sync")
    ) {
      const v = Ou();
      b = v.__watcherHandles || (v.__watcherHandles = []);
    } else return Je;
  let w = m ? new Array(e.length).fill(Zs) : Zs;
  const R = () => {
    if (p.active)
      if (t) {
        const v = p.run();
        (a || u || (m ? v.some((C, O) => As(C, w[O])) : As(v, w))) &&
          (d && d(),
          Ve(t, r, 3, [v, w === Zs ? void 0 : m && w[0] === Zs ? [] : w, _]),
          (w = v));
      } else p.run();
  };
  R.allowRecurse = !!t;
  let f;
  i === "sync"
    ? (f = R)
    : i === "post"
      ? (f = () => Ce(R, r && r.suspense))
      : ((R.pre = !0), r && (R.id = r.uid), (f = () => Pa(R)));
  const p = new Ni(o, f);
  t
    ? s
      ? R()
      : (w = p.run())
    : i === "post"
      ? Ce(p.run.bind(p), r && r.suspense)
      : p.run();
  const q = () => {
    p.stop(), r && r.scope && Ii(r.scope.effects, p);
  };
  return b && b.push(q), q;
}
function Qo(e, t, s) {
  const a = this.proxy,
    i = be(e) ? (e.includes(".") ? or(a, e) : () => a[e]) : e.bind(a, a);
  let n;
  ee(t) ? (n = t) : ((n = t.handler), (s = t));
  const l = qe;
  Xt(this);
  const c = Qi(i, n.bind(a), s);
  return l ? Xt(l) : Ot(), c;
}
function or(e, t) {
  const s = t.split(".");
  return () => {
    let a = e;
    for (let i = 0; i < s.length && a; i++) a = a[s[i]];
    return a;
  };
}
function It(e, t) {
  if (!ge(e) || e.__v_skip || ((t = t || new Set()), t.has(e))) return e;
  if ((t.add(e), Ee(e))) It(e.value, t);
  else if (G(e)) for (let s = 0; s < e.length; s++) It(e[s], t);
  else if (Cl(e) || Wt(e))
    e.forEach(s => {
      It(s, t);
    });
  else if (Tl(e)) for (const s in e) It(e[s], t);
  return e;
}
function Vi(e, t) {
  const s = Ae;
  if (s === null) return e;
  const a = Ia(s) || s.proxy,
    i = e.dirs || (e.dirs = []);
  for (let n = 0; n < t.length; n++) {
    let [l, c, r, o = fe] = t[n];
    l &&
      (ee(l) && (l = { mounted: l, updated: l }),
      l.deep && It(c),
      i.push({
        dir: l,
        instance: a,
        value: c,
        oldValue: void 0,
        arg: r,
        modifiers: o,
      }));
  }
  return e;
}
function it(e, t, s, a) {
  const i = e.dirs,
    n = t && t.dirs;
  for (let l = 0; l < i.length; l++) {
    const c = i[l];
    n && (c.oldValue = n[l].value);
    let r = c.dir[a];
    r && (is(), Ve(r, s, 8, [e.el, c, e, t]), ns());
  }
}
function Vo() {
  const e = {
    isMounted: !1,
    isLeaving: !1,
    isUnmounting: !1,
    leavingVNodes: new Map(),
  };
  return (
    Hs(() => {
      e.isMounted = !0;
    }),
    Fs(() => {
      e.isUnmounting = !0;
    }),
    e
  );
}
const De = [Function, Array],
  ur = {
    mode: String,
    appear: Boolean,
    persisted: Boolean,
    onBeforeEnter: De,
    onEnter: De,
    onAfterEnter: De,
    onEnterCancelled: De,
    onBeforeLeave: De,
    onLeave: De,
    onAfterLeave: De,
    onLeaveCancelled: De,
    onBeforeAppear: De,
    onAppear: De,
    onAfterAppear: De,
    onAppearCancelled: De,
  },
  Wo = {
    name: "BaseTransition",
    props: ur,
    setup(e, { slots: t }) {
      const s = Ds(),
        a = Vo();
      let i;
      return () => {
        const n = t.default && hr(t.default(), !0);
        if (!n || !n.length) return;
        let l = n[0];
        if (n.length > 1) {
          for (const w of n)
            if (w.type !== Me) {
              l = w;
              break;
            }
        }
        const c = le(e),
          { mode: r } = c;
        if (a.isLeaving) return Ha(l);
        const o = xn(l);
        if (!o) return Ha(l);
        const u = li(o, c, a, s);
        da(o, u);
        const m = s.subTree,
          d = m && xn(m);
        let _ = !1;
        const { getTransitionKey: b } = o.type;
        if (b) {
          const w = b();
          i === void 0 ? (i = w) : w !== i && ((i = w), (_ = !0));
        }
        if (d && d.type !== Me && (!Ke(o, d) || _)) {
          const w = li(d, c, a, s);
          if ((da(d, w), r === "out-in"))
            return (
              (a.isLeaving = !0),
              (w.afterLeave = () => {
                (a.isLeaving = !1), s.update.active !== !1 && s.update();
              }),
              Ha(l)
            );
          r === "in-out" &&
            o.type !== Me &&
            (w.delayLeave = (R, f, p) => {
              const q = mr(a, d);
              (q[String(d.key)] = d),
                (R._leaveCb = () => {
                  f(), (R._leaveCb = void 0), delete u.delayedLeave;
                }),
                (u.delayedLeave = p);
            });
        }
        return l;
      };
    },
  },
  Yo = Wo;
function mr(e, t) {
  const { leavingVNodes: s } = e;
  let a = s.get(t.type);
  return a || ((a = Object.create(null)), s.set(t.type, a)), a;
}
function li(e, t, s, a) {
  const {
      appear: i,
      mode: n,
      persisted: l = !1,
      onBeforeEnter: c,
      onEnter: r,
      onAfterEnter: o,
      onEnterCancelled: u,
      onBeforeLeave: m,
      onLeave: d,
      onAfterLeave: _,
      onLeaveCancelled: b,
      onBeforeAppear: w,
      onAppear: R,
      onAfterAppear: f,
      onAppearCancelled: p,
    } = t,
    q = String(e.key),
    v = mr(s, e),
    C = (x, F) => {
      x && Ve(x, a, 9, F);
    },
    O = (x, F) => {
      const W = F[1];
      C(x, F), G(x) ? x.every(Z => Z.length <= 1) && W() : x.length <= 1 && W();
    },
    M = {
      mode: n,
      persisted: l,
      beforeEnter(x) {
        let F = c;
        if (!s.isMounted)
          if (i) F = w || c;
          else return;
        x._leaveCb && x._leaveCb(!0);
        const W = v[q];
        W && Ke(e, W) && W.el._leaveCb && W.el._leaveCb(), C(F, [x]);
      },
      enter(x) {
        let F = r,
          W = o,
          Z = u;
        if (!s.isMounted)
          if (i) (F = R || r), (W = f || o), (Z = p || u);
          else return;
        let z = !1;
        const X = (x._enterCb = V => {
          z ||
            ((z = !0),
            V ? C(Z, [x]) : C(W, [x]),
            M.delayedLeave && M.delayedLeave(),
            (x._enterCb = void 0));
        });
        F ? O(F, [x, X]) : X();
      },
      leave(x, F) {
        const W = String(e.key);
        if ((x._enterCb && x._enterCb(!0), s.isUnmounting)) return F();
        C(m, [x]);
        let Z = !1;
        const z = (x._leaveCb = X => {
          Z ||
            ((Z = !0),
            F(),
            X ? C(b, [x]) : C(_, [x]),
            (x._leaveCb = void 0),
            v[W] === e && delete v[W]);
        });
        (v[W] = e), d ? O(d, [x, z]) : z();
      },
      clone(x) {
        return li(x, t, s, a);
      },
    };
  return M;
}
function Ha(e) {
  if (Ls(e)) return (e = mt(e)), (e.children = null), e;
}
function xn(e) {
  return Ls(e) ? (e.children ? e.children[0] : void 0) : e;
}
function da(e, t) {
  e.shapeFlag & 6 && e.component
    ? da(e.component.subTree, t)
    : e.shapeFlag & 128
      ? ((e.ssContent.transition = t.clone(e.ssContent)),
        (e.ssFallback.transition = t.clone(e.ssFallback)))
      : (e.transition = t);
}
function hr(e, t = !1, s) {
  let a = [],
    i = 0;
  for (let n = 0; n < e.length; n++) {
    let l = e[n];
    const c = s == null ? l.key : String(s) + String(l.key != null ? l.key : n);
    l.type === ne
      ? (l.patchFlag & 128 && i++, (a = a.concat(hr(l.children, t, c))))
      : (t || l.type !== Me) && a.push(c != null ? mt(l, { key: c }) : l);
  }
  if (i > 1) for (let n = 0; n < a.length; n++) a[n].patchFlag = -2;
  return a;
}
function rs(e, t) {
  return ee(e) ? (() => ke({ name: e.name }, t, { setup: e }))() : e;
}
const Mt = e => !!e.type.__asyncLoader;
function $o(e) {
  ee(e) && (e = { loader: e });
  const {
    loader: t,
    loadingComponent: s,
    errorComponent: a,
    delay: i = 200,
    timeout: n,
    suspensible: l = !0,
    onError: c,
  } = e;
  let r = null,
    o,
    u = 0;
  const m = () => (u++, (r = null), d()),
    d = () => {
      let _;
      return (
        r ||
        (_ = r =
          t()
            .catch(b => {
              if (((b = b instanceof Error ? b : new Error(String(b))), c))
                return new Promise((w, R) => {
                  c(
                    b,
                    () => w(m()),
                    () => R(b),
                    u + 1
                  );
                });
              throw b;
            })
            .then(b =>
              _ !== r && r
                ? r
                : (b &&
                    (b.__esModule || b[Symbol.toStringTag] === "Module") &&
                    (b = b.default),
                  (o = b),
                  b)
            ))
      );
    };
  return rs({
    name: "AsyncComponentWrapper",
    __asyncLoader: d,
    get __asyncResolved() {
      return o;
    },
    setup() {
      const _ = qe;
      if (o) return () => Fa(o, _);
      const b = p => {
        (r = null), ls(p, _, 13, !a);
      };
      if ((l && _.suspense) || es)
        return d()
          .then(p => () => Fa(p, _))
          .catch(p => (b(p), () => (a ? Q(a, { error: p }) : null)));
      const w = Qe(!1),
        R = Qe(),
        f = Qe(!!i);
      return (
        i &&
          setTimeout(() => {
            f.value = !1;
          }, i),
        n != null &&
          setTimeout(() => {
            if (!w.value && !R.value) {
              const p = new Error(`Async component timed out after ${n}ms.`);
              b(p), (R.value = p);
            }
          }, n),
        d()
          .then(() => {
            (w.value = !0),
              _.parent && Ls(_.parent.vnode) && Pa(_.parent.update);
          })
          .catch(p => {
            b(p), (R.value = p);
          }),
        () => {
          if (w.value && o) return Fa(o, _);
          if (R.value && a) return Q(a, { error: R.value });
          if (s && !f.value) return Q(s);
        }
      );
    },
  });
}
function Fa(e, t) {
  const { ref: s, props: a, children: i, ce: n } = t.vnode,
    l = Q(e, a, i);
  return (l.ref = s), (l.ce = n), delete t.vnode.ce, l;
}
const Ls = e => e.type.__isKeepAlive,
  Ko = {
    name: "KeepAlive",
    __isKeepAlive: !0,
    props: {
      include: [String, RegExp, Array],
      exclude: [String, RegExp, Array],
      max: [String, Number],
    },
    setup(e, { slots: t }) {
      const s = Ds(),
        a = s.ctx;
      if (!a.renderer)
        return () => {
          const p = t.default && t.default();
          return p && p.length === 1 ? p[0] : p;
        };
      const i = new Map(),
        n = new Set();
      let l = null;
      const c = s.suspense,
        {
          renderer: {
            p: r,
            m: o,
            um: u,
            o: { createElement: m },
          },
        } = a,
        d = m("div");
      (a.activate = (p, q, v, C, O) => {
        const M = p.component;
        o(p, q, v, 0, c),
          r(M.vnode, p, q, v, M, c, C, p.slotScopeIds, O),
          Ce(() => {
            (M.isDeactivated = !1), M.a && vs(M.a);
            const x = p.props && p.props.onVnodeMounted;
            x && Ue(x, M.parent, p);
          }, c);
      }),
        (a.deactivate = p => {
          const q = p.component;
          o(p, d, null, 1, c),
            Ce(() => {
              q.da && vs(q.da);
              const v = p.props && p.props.onVnodeUnmounted;
              v && Ue(v, q.parent, p), (q.isDeactivated = !0);
            }, c);
        });
      function _(p) {
        Da(p), u(p, s, c, !0);
      }
      function b(p) {
        i.forEach((q, v) => {
          const C = di(q.type);
          C && (!p || !p(C)) && w(v);
        });
      }
      function w(p) {
        const q = i.get(p);
        !l || !Ke(q, l) ? _(q) : l && Da(l), i.delete(p), n.delete(p);
      }
      Rt(
        () => [e.include, e.exclude],
        ([p, q]) => {
          p && b(v => bs(p, v)), q && b(v => !bs(q, v));
        },
        { flush: "post", deep: !0 }
      );
      let R = null;
      const f = () => {
        R != null && i.set(R, za(s.subTree));
      };
      return (
        Hs(f),
        fr(f),
        Fs(() => {
          i.forEach(p => {
            const { subTree: q, suspense: v } = s,
              C = za(q);
            if (p.type === C.type && p.key === C.key) {
              Da(C);
              const O = C.component.da;
              O && Ce(O, v);
              return;
            }
            _(p);
          });
        }),
        () => {
          if (((R = null), !t.default)) return null;
          const p = t.default(),
            q = p[0];
          if (p.length > 1) return (l = null), p;
          if (!Gt(q) || (!(q.shapeFlag & 4) && !(q.shapeFlag & 128)))
            return (l = null), q;
          let v = za(q);
          const C = v.type,
            O = di(Mt(v) ? v.type.__asyncResolved || {} : C),
            { include: M, exclude: x, max: F } = e;
          if ((M && (!O || !bs(M, O))) || (x && O && bs(x, O)))
            return (l = v), q;
          const W = v.key == null ? C : v.key,
            Z = i.get(W);
          return (
            v.el && ((v = mt(v)), q.shapeFlag & 128 && (q.ssContent = v)),
            (R = W),
            Z
              ? ((v.el = Z.el),
                (v.component = Z.component),
                v.transition && da(v, v.transition),
                (v.shapeFlag |= 512),
                n.delete(W),
                n.add(W))
              : (n.add(W),
                F && n.size > parseInt(F, 10) && w(n.values().next().value)),
            (v.shapeFlag |= 256),
            (l = v),
            lr(q.type) ? q : v
          );
        }
      );
    },
  },
  Jo = Ko;
function bs(e, t) {
  return G(e)
    ? e.some(s => bs(s, t))
    : be(e)
      ? e.split(",").includes(t)
      : Rc(e)
        ? e.test(t)
        : !1;
}
function dr(e, t) {
  pr(e, "a", t);
}
function gr(e, t) {
  pr(e, "da", t);
}
function pr(e, t, s = qe) {
  const a =
    e.__wdc ||
    (e.__wdc = () => {
      let i = s;
      for (; i; ) {
        if (i.isDeactivated) return;
        i = i.parent;
      }
      return e();
    });
  if ((Ca(t, a, s), s)) {
    let i = s.parent;
    for (; i && i.parent; )
      Ls(i.parent.vnode) && Zo(a, t, s, i), (i = i.parent);
  }
}
function Zo(e, t, s, a) {
  const i = Ca(t, e, a, !0);
  br(() => {
    Ii(a[t], i);
  }, s);
}
function Da(e) {
  (e.shapeFlag &= -257), (e.shapeFlag &= -513);
}
function za(e) {
  return e.shapeFlag & 128 ? e.ssContent : e;
}
function Ca(e, t, s = qe, a = !1) {
  if (s) {
    const i = s[e] || (s[e] = []),
      n =
        t.__weh ||
        (t.__weh = (...l) => {
          if (s.isUnmounted) return;
          is(), Xt(s);
          const c = Ve(t, s, e, l);
          return Ot(), ns(), c;
        });
    return a ? i.unshift(n) : i.push(n), n;
  }
}
const ht =
    e =>
    (t, s = qe) =>
      (!es || e === "sp") && Ca(e, (...a) => t(...a), s),
  Go = ht("bm"),
  Hs = ht("m"),
  Xo = ht("bu"),
  fr = ht("u"),
  Fs = ht("bum"),
  br = ht("um"),
  eu = ht("sp"),
  tu = ht("rtg"),
  su = ht("rtc");
function _r(e, t = qe) {
  Ca("ec", e, t);
}
const Wi = "components";
function au(e, t) {
  return wr(Wi, e, !0, t) || e;
}
const yr = Symbol.for("v-ndc");
function vr(e) {
  return be(e) ? wr(Wi, e, !1) || e : e || yr;
}
function wr(e, t, s = !0, a = !1) {
  const i = Ae || qe;
  if (i) {
    const n = i.type;
    if (e === Wi) {
      const c = di(n, !1);
      if (c && (c === t || c === lt(t) || c === xa(lt(t)))) return n;
    }
    const l = qn(i[e] || n[e], t) || qn(i.appContext[e], t);
    return !l && a ? n : l;
  }
}
function qn(e, t) {
  return e && (e[t] || e[lt(t)] || e[xa(lt(t))]);
}
function Fe(e, t, s, a) {
  let i;
  const n = s && s[a];
  if (G(e) || be(e)) {
    i = new Array(e.length);
    for (let l = 0, c = e.length; l < c; l++)
      i[l] = t(e[l], l, void 0, n && n[l]);
  } else if (typeof e == "number") {
    i = new Array(e);
    for (let l = 0; l < e; l++) i[l] = t(l + 1, l, void 0, n && n[l]);
  } else if (ge(e))
    if (e[Symbol.iterator])
      i = Array.from(e, (l, c) => t(l, c, void 0, n && n[c]));
    else {
      const l = Object.keys(e);
      i = new Array(l.length);
      for (let c = 0, r = l.length; c < r; c++) {
        const o = l[c];
        i[c] = t(e[o], o, c, n && n[c]);
      }
    }
  else i = [];
  return s && (s[a] = i), i;
}
function Yi(e, t, s = {}, a, i) {
  if (Ae.isCE || (Ae.parent && Mt(Ae.parent) && Ae.parent.isCE))
    return t !== "default" && (s.name = t), Q("slot", s, a && a());
  let n = e[t];
  n && n._c && (n._d = !1), T();
  const l = n && jr(n(s)),
    c = _e(
      ne,
      { key: s.key || (l && l.key) || `_${t}` },
      l || (a ? a() : []),
      l && e._ === 1 ? 64 : -2
    );
  return (
    !i && c.scopeId && (c.slotScopeIds = [c.scopeId + "-s"]),
    n && n._c && (n._d = !0),
    c
  );
}
function jr(e) {
  return e.some(t =>
    Gt(t) ? !(t.type === Me || (t.type === ne && !jr(t.children))) : !0
  )
    ? e
    : null;
}
const ri = e => (e ? (Ur(e) ? Ia(e) || e.proxy : ri(e.parent)) : null),
  ws = ke(Object.create(null), {
    $: e => e,
    $el: e => e.vnode.el,
    $data: e => e.data,
    $props: e => e.props,
    $attrs: e => e.attrs,
    $slots: e => e.slots,
    $refs: e => e.refs,
    $parent: e => ri(e.parent),
    $root: e => ri(e.root),
    $emit: e => e.emit,
    $options: e => $i(e),
    $forceUpdate: e => e.f || (e.f = () => Pa(e.update)),
    $nextTick: e => e.n || (e.n = Lt.bind(e.proxy)),
    $watch: e => Qo.bind(e),
  }),
  Ba = (e, t) => e !== fe && !e.__isScriptSetup && ie(e, t),
  iu = {
    get({ _: e }, t) {
      const {
        ctx: s,
        setupState: a,
        data: i,
        props: n,
        accessCache: l,
        type: c,
        appContext: r,
      } = e;
      let o;
      if (t[0] !== "$") {
        const _ = l[t];
        if (_ !== void 0)
          switch (_) {
            case 1:
              return a[t];
            case 2:
              return i[t];
            case 4:
              return s[t];
            case 3:
              return n[t];
          }
        else {
          if (Ba(a, t)) return (l[t] = 1), a[t];
          if (i !== fe && ie(i, t)) return (l[t] = 2), i[t];
          if ((o = e.propsOptions[0]) && ie(o, t)) return (l[t] = 3), n[t];
          if (s !== fe && ie(s, t)) return (l[t] = 4), s[t];
          ci && (l[t] = 0);
        }
      }
      const u = ws[t];
      let m, d;
      if (u) return t === "$attrs" && Le(e, "get", t), u(e);
      if ((m = c.__cssModules) && (m = m[t])) return m;
      if (s !== fe && ie(s, t)) return (l[t] = 4), s[t];
      if (((d = r.config.globalProperties), ie(d, t))) return d[t];
    },
    set({ _: e }, t, s) {
      const { data: a, setupState: i, ctx: n } = e;
      return Ba(i, t)
        ? ((i[t] = s), !0)
        : a !== fe && ie(a, t)
          ? ((a[t] = s), !0)
          : ie(e.props, t) || (t[0] === "$" && t.slice(1) in e)
            ? !1
            : ((n[t] = s), !0);
    },
    has(
      {
        _: {
          data: e,
          setupState: t,
          accessCache: s,
          ctx: a,
          appContext: i,
          propsOptions: n,
        },
      },
      l
    ) {
      let c;
      return (
        !!s[l] ||
        (e !== fe && ie(e, l)) ||
        Ba(t, l) ||
        ((c = n[0]) && ie(c, l)) ||
        ie(a, l) ||
        ie(ws, l) ||
        ie(i.config.globalProperties, l)
      );
    },
    defineProperty(e, t, s) {
      return (
        s.get != null
          ? (e._.accessCache[t] = 0)
          : ie(s, "value") && this.set(e, t, s.value, null),
        Reflect.defineProperty(e, t, s)
      );
    },
  };
function kn(e) {
  return G(e) ? e.reduce((t, s) => ((t[s] = null), t), {}) : e;
}
let ci = !0;
function nu(e) {
  const t = $i(e),
    s = e.proxy,
    a = e.ctx;
  (ci = !1), t.beforeCreate && Pn(t.beforeCreate, e, "bc");
  const {
    data: i,
    computed: n,
    methods: l,
    watch: c,
    provide: r,
    inject: o,
    created: u,
    beforeMount: m,
    mounted: d,
    beforeUpdate: _,
    updated: b,
    activated: w,
    deactivated: R,
    beforeDestroy: f,
    beforeUnmount: p,
    destroyed: q,
    unmounted: v,
    render: C,
    renderTracked: O,
    renderTriggered: M,
    errorCaptured: x,
    serverPrefetch: F,
    expose: W,
    inheritAttrs: Z,
    components: z,
    directives: X,
    filters: V,
  } = t;
  if ((o && lu(o, a, null), l))
    for (const ce in l) {
      const oe = l[ce];
      ee(oe) && (a[ce] = oe.bind(s));
    }
  if (i) {
    const ce = i.call(s, s);
    ge(ce) && (e.data = Ge(ce));
  }
  if (((ci = !0), n))
    for (const ce in n) {
      const oe = n[ce],
        rt = ee(oe) ? oe.bind(s, s) : ee(oe.get) ? oe.get.bind(s, s) : Je,
        gt = !ee(oe) && ee(oe.set) ? oe.set.bind(s) : Je,
        tt = Te({ get: rt, set: gt });
      Object.defineProperty(a, ce, {
        enumerable: !0,
        configurable: !0,
        get: () => tt.value,
        set: Ne => (tt.value = Ne),
      });
    }
  if (c) for (const ce in c) xr(c[ce], a, s, ce);
  if (r) {
    const ce = ee(r) ? r.call(s) : r;
    Reflect.ownKeys(ce).forEach(oe => {
      Nt(oe, ce[oe]);
    });
  }
  u && Pn(u, e, "c");
  function ae(ce, oe) {
    G(oe) ? oe.forEach(rt => ce(rt.bind(s))) : oe && ce(oe.bind(s));
  }
  if (
    (ae(Go, m),
    ae(Hs, d),
    ae(Xo, _),
    ae(fr, b),
    ae(dr, w),
    ae(gr, R),
    ae(_r, x),
    ae(su, O),
    ae(tu, M),
    ae(Fs, p),
    ae(br, v),
    ae(eu, F),
    G(W))
  )
    if (W.length) {
      const ce = e.exposed || (e.exposed = {});
      W.forEach(oe => {
        Object.defineProperty(ce, oe, {
          get: () => s[oe],
          set: rt => (s[oe] = rt),
        });
      });
    } else e.exposed || (e.exposed = {});
  C && e.render === Je && (e.render = C),
    Z != null && (e.inheritAttrs = Z),
    z && (e.components = z),
    X && (e.directives = X);
}
function lu(e, t, s = Je) {
  G(e) && (e = oi(e));
  for (const a in e) {
    const i = e[a];
    let n;
    ge(i)
      ? "default" in i
        ? (n = ve(i.from || a, i.default, !0))
        : (n = ve(i.from || a))
      : (n = ve(i)),
      Ee(n)
        ? Object.defineProperty(t, a, {
            enumerable: !0,
            configurable: !0,
            get: () => n.value,
            set: l => (n.value = l),
          })
        : (t[a] = n);
  }
}
function Pn(e, t, s) {
  Ve(G(e) ? e.map(a => a.bind(t.proxy)) : e.bind(t.proxy), t, s);
}
function xr(e, t, s, a) {
  const i = a.includes(".") ? or(s, a) : () => s[a];
  if (be(e)) {
    const n = t[e];
    ee(n) && Rt(i, n);
  } else if (ee(e)) Rt(i, e.bind(s));
  else if (ge(e))
    if (G(e)) e.forEach(n => xr(n, t, s, a));
    else {
      const n = ee(e.handler) ? e.handler.bind(s) : t[e.handler];
      ee(n) && Rt(i, n, e);
    }
}
function $i(e) {
  const t = e.type,
    { mixins: s, extends: a } = t,
    {
      mixins: i,
      optionsCache: n,
      config: { optionMergeStrategies: l },
    } = e.appContext,
    c = n.get(t);
  let r;
  return (
    c
      ? (r = c)
      : !i.length && !s && !a
        ? (r = t)
        : ((r = {}), i.length && i.forEach(o => ga(r, o, l, !0)), ga(r, t, l)),
    ge(t) && n.set(t, r),
    r
  );
}
function ga(e, t, s, a = !1) {
  const { mixins: i, extends: n } = t;
  n && ga(e, n, s, !0), i && i.forEach(l => ga(e, l, s, !0));
  for (const l in t)
    if (!(a && l === "expose")) {
      const c = ru[l] || (s && s[l]);
      e[l] = c ? c(e[l], t[l]) : t[l];
    }
  return e;
}
const ru = {
  data: En,
  props: An,
  emits: An,
  methods: _s,
  computed: _s,
  beforeCreate: Re,
  created: Re,
  beforeMount: Re,
  mounted: Re,
  beforeUpdate: Re,
  updated: Re,
  beforeDestroy: Re,
  beforeUnmount: Re,
  destroyed: Re,
  unmounted: Re,
  activated: Re,
  deactivated: Re,
  errorCaptured: Re,
  serverPrefetch: Re,
  components: _s,
  directives: _s,
  watch: ou,
  provide: En,
  inject: cu,
};
function En(e, t) {
  return t
    ? e
      ? function () {
          return ke(
            ee(e) ? e.call(this, this) : e,
            ee(t) ? t.call(this, this) : t
          );
        }
      : t
    : e;
}
function cu(e, t) {
  return _s(oi(e), oi(t));
}
function oi(e) {
  if (G(e)) {
    const t = {};
    for (let s = 0; s < e.length; s++) t[e[s]] = e[s];
    return t;
  }
  return e;
}
function Re(e, t) {
  return e ? [...new Set([].concat(e, t))] : t;
}
function _s(e, t) {
  return e ? ke(Object.create(null), e, t) : t;
}
function An(e, t) {
  return e
    ? G(e) && G(t)
      ? [...new Set([...e, ...t])]
      : ke(Object.create(null), kn(e), kn(t ?? {}))
    : t;
}
function ou(e, t) {
  if (!e) return t;
  if (!t) return e;
  const s = ke(Object.create(null), e);
  for (const a in t) s[a] = Re(e[a], t[a]);
  return s;
}
function qr() {
  return {
    app: null,
    config: {
      isNativeTag: Sc,
      performance: !1,
      globalProperties: {},
      optionMergeStrategies: {},
      errorHandler: void 0,
      warnHandler: void 0,
      compilerOptions: {},
    },
    mixins: [],
    components: {},
    directives: {},
    provides: Object.create(null),
    optionsCache: new WeakMap(),
    propsCache: new WeakMap(),
    emitsCache: new WeakMap(),
  };
}
let uu = 0;
function mu(e, t) {
  return function (a, i = null) {
    ee(a) || (a = ke({}, a)), i != null && !ge(i) && (i = null);
    const n = qr(),
      l = new Set();
    let c = !1;
    const r = (n.app = {
      _uid: uu++,
      _component: a,
      _props: i,
      _container: null,
      _context: n,
      _instance: null,
      version: Hr,
      get config() {
        return n.config;
      },
      set config(o) {},
      use(o, ...u) {
        return (
          l.has(o) ||
            (o && ee(o.install)
              ? (l.add(o), o.install(r, ...u))
              : ee(o) && (l.add(o), o(r, ...u))),
          r
        );
      },
      mixin(o) {
        return n.mixins.includes(o) || n.mixins.push(o), r;
      },
      component(o, u) {
        return u ? ((n.components[o] = u), r) : n.components[o];
      },
      directive(o, u) {
        return u ? ((n.directives[o] = u), r) : n.directives[o];
      },
      mount(o, u, m) {
        if (!c) {
          const d = Q(a, i);
          return (
            (d.appContext = n),
            u && t ? t(d, o) : e(d, o, m),
            (c = !0),
            (r._container = o),
            (o.__vue_app__ = r),
            Ia(d.component) || d.component.proxy
          );
        }
      },
      unmount() {
        c && (e(null, r._container), delete r._container.__vue_app__);
      },
      provide(o, u) {
        return (n.provides[o] = u), r;
      },
      runWithContext(o) {
        Ms = r;
        try {
          return o();
        } finally {
          Ms = null;
        }
      },
    });
    return r;
  };
}
let Ms = null;
function Nt(e, t) {
  if (qe) {
    let s = qe.provides;
    const a = qe.parent && qe.parent.provides;
    a === s && (s = qe.provides = Object.create(a)), (s[e] = t);
  }
}
function ve(e, t, s = !1) {
  const a = qe || Ae;
  if (a || Ms) {
    const i = a
      ? a.parent == null
        ? a.vnode.appContext && a.vnode.appContext.provides
        : a.parent.provides
      : Ms._context.provides;
    if (i && e in i) return i[e];
    if (arguments.length > 1) return s && ee(t) ? t.call(a && a.proxy) : t;
  }
}
function kr() {
  return !!(qe || Ae || Ms);
}
function hu(e, t, s, a = !1) {
  const i = {},
    n = {};
  ca(n, Sa, 1), (e.propsDefaults = Object.create(null)), Pr(e, t, i, n);
  for (const l in e.propsOptions[0]) l in i || (i[l] = void 0);
  s ? (e.props = a ? i : Wl(i)) : e.type.props ? (e.props = i) : (e.props = n),
    (e.attrs = n);
}
function du(e, t, s, a) {
  const {
      props: i,
      attrs: n,
      vnode: { patchFlag: l },
    } = e,
    c = le(i),
    [r] = e.propsOptions;
  let o = !1;
  if ((a || l > 0) && !(l & 16)) {
    if (l & 8) {
      const u = e.vnode.dynamicProps;
      for (let m = 0; m < u.length; m++) {
        let d = u[m];
        if (Ea(e.emitsOptions, d)) continue;
        const _ = t[d];
        if (r)
          if (ie(n, d)) _ !== n[d] && ((n[d] = _), (o = !0));
          else {
            const b = lt(d);
            i[b] = ui(r, c, b, _, e, !1);
          }
        else _ !== n[d] && ((n[d] = _), (o = !0));
      }
    }
  } else {
    Pr(e, t, i, n) && (o = !0);
    let u;
    for (const m in c)
      (!t || (!ie(t, m) && ((u = as(m)) === m || !ie(t, u)))) &&
        (r
          ? s &&
            (s[m] !== void 0 || s[u] !== void 0) &&
            (i[m] = ui(r, c, m, void 0, e, !0))
          : delete i[m]);
    if (n !== c)
      for (const m in n) (!t || !ie(t, m)) && (delete n[m], (o = !0));
  }
  o && ut(e, "set", "$attrs");
}
function Pr(e, t, s, a) {
  const [i, n] = e.propsOptions;
  let l = !1,
    c;
  if (t)
    for (let r in t) {
      if (ys(r)) continue;
      const o = t[r];
      let u;
      i && ie(i, (u = lt(r)))
        ? !n || !n.includes(u)
          ? (s[u] = o)
          : ((c || (c = {}))[u] = o)
        : Ea(e.emitsOptions, r) ||
          ((!(r in a) || o !== a[r]) && ((a[r] = o), (l = !0)));
    }
  if (n) {
    const r = le(s),
      o = c || fe;
    for (let u = 0; u < n.length; u++) {
      const m = n[u];
      s[m] = ui(i, r, m, o[m], e, !ie(o, m));
    }
  }
  return l;
}
function ui(e, t, s, a, i, n) {
  const l = e[s];
  if (l != null) {
    const c = ie(l, "default");
    if (c && a === void 0) {
      const r = l.default;
      if (l.type !== Function && !l.skipFactory && ee(r)) {
        const { propsDefaults: o } = i;
        s in o ? (a = o[s]) : (Xt(i), (a = o[s] = r.call(null, t)), Ot());
      } else a = r;
    }
    l[0] &&
      (n && !c ? (a = !1) : l[1] && (a === "" || a === as(s)) && (a = !0));
  }
  return a;
}
function Er(e, t, s = !1) {
  const a = t.propsCache,
    i = a.get(e);
  if (i) return i;
  const n = e.props,
    l = {},
    c = [];
  let r = !1;
  if (!ee(e)) {
    const u = m => {
      r = !0;
      const [d, _] = Er(m, t, !0);
      ke(l, d), _ && c.push(..._);
    };
    !s && t.mixins.length && t.mixins.forEach(u),
      e.extends && u(e.extends),
      e.mixins && e.mixins.forEach(u);
  }
  if (!n && !r) return ge(e) && a.set(e, Vt), Vt;
  if (G(n))
    for (let u = 0; u < n.length; u++) {
      const m = lt(n[u]);
      Cn(m) && (l[m] = fe);
    }
  else if (n)
    for (const u in n) {
      const m = lt(u);
      if (Cn(m)) {
        const d = n[u],
          _ = (l[m] = G(d) || ee(d) ? { type: d } : ke({}, d));
        if (_) {
          const b = Tn(Boolean, _.type),
            w = Tn(String, _.type);
          (_[0] = b > -1),
            (_[1] = w < 0 || b < w),
            (b > -1 || ie(_, "default")) && c.push(m);
        }
      }
    }
  const o = [l, c];
  return ge(e) && a.set(e, o), o;
}
function Cn(e) {
  return e[0] !== "$";
}
function Sn(e) {
  const t = e && e.toString().match(/^\s*(function|class) (\w+)/);
  return t ? t[2] : e === null ? "null" : "";
}
function In(e, t) {
  return Sn(e) === Sn(t);
}
function Tn(e, t) {
  return G(t) ? t.findIndex(s => In(s, e)) : ee(t) && In(t, e) ? 0 : -1;
}
const Ar = e => e[0] === "_" || e === "$stable",
  Ki = e => (G(e) ? e.map(ze) : [ze(e)]),
  gu = (e, t, s) => {
    if (t._n) return t;
    const a = Xe((...i) => Ki(t(...i)), s);
    return (a._c = !1), a;
  },
  Cr = (e, t, s) => {
    const a = e._ctx;
    for (const i in e) {
      if (Ar(i)) continue;
      const n = e[i];
      if (ee(n)) t[i] = gu(i, n, a);
      else if (n != null) {
        const l = Ki(n);
        t[i] = () => l;
      }
    }
  },
  Sr = (e, t) => {
    const s = Ki(t);
    e.slots.default = () => s;
  },
  pu = (e, t) => {
    if (e.vnode.shapeFlag & 32) {
      const s = t._;
      s ? ((e.slots = le(t)), ca(t, "_", s)) : Cr(t, (e.slots = {}));
    } else (e.slots = {}), t && Sr(e, t);
    ca(e.slots, Sa, 1);
  },
  fu = (e, t, s) => {
    const { vnode: a, slots: i } = e;
    let n = !0,
      l = fe;
    if (a.shapeFlag & 32) {
      const c = t._;
      c
        ? s && c === 1
          ? (n = !1)
          : (ke(i, t), !s && c === 1 && delete i._)
        : ((n = !t.$stable), Cr(t, i)),
        (l = t);
    } else t && (Sr(e, t), (l = { default: 1 }));
    if (n) for (const c in i) !Ar(c) && !(c in l) && delete i[c];
  };
function pa(e, t, s, a, i = !1) {
  if (G(e)) {
    e.forEach((d, _) => pa(d, t && (G(t) ? t[_] : t), s, a, i));
    return;
  }
  if (Mt(a) && !i) return;
  const n = a.shapeFlag & 4 ? Ia(a.component) || a.component.proxy : a.el,
    l = i ? null : n,
    { i: c, r } = e,
    o = t && t.r,
    u = c.refs === fe ? (c.refs = {}) : c.refs,
    m = c.setupState;
  if (
    (o != null &&
      o !== r &&
      (be(o)
        ? ((u[o] = null), ie(m, o) && (m[o] = null))
        : Ee(o) && (o.value = null)),
    ee(r))
  )
    xt(r, c, 12, [l, u]);
  else {
    const d = be(r),
      _ = Ee(r);
    if (d || _) {
      const b = () => {
        if (e.f) {
          const w = d ? (ie(m, r) ? m[r] : u[r]) : r.value;
          i
            ? G(w) && Ii(w, n)
            : G(w)
              ? w.includes(n) || w.push(n)
              : d
                ? ((u[r] = [n]), ie(m, r) && (m[r] = u[r]))
                : ((r.value = [n]), e.k && (u[e.k] = r.value));
        } else
          d
            ? ((u[r] = l), ie(m, r) && (m[r] = l))
            : _ && ((r.value = l), e.k && (u[e.k] = l));
      };
      l ? ((b.id = -1), Ce(b, s)) : b();
    }
  }
}
let ft = !1;
const Gs = e => /svg/.test(e.namespaceURI) && e.tagName !== "foreignObject",
  Xs = e => e.nodeType === 8;
function bu(e) {
  const {
      mt: t,
      p: s,
      o: {
        patchProp: a,
        createText: i,
        nextSibling: n,
        parentNode: l,
        remove: c,
        insert: r,
        createComment: o,
      },
    } = e,
    u = (f, p) => {
      if (!p.hasChildNodes()) {
        s(null, f, p), ma(), (p._vnode = f);
        return;
      }
      (ft = !1),
        m(p.firstChild, f, null, null, null),
        ma(),
        (p._vnode = f),
        ft && console.error("Hydration completed but contains mismatches.");
    },
    m = (f, p, q, v, C, O = !1) => {
      const M = Xs(f) && f.data === "[",
        x = () => w(f, p, q, v, C, M),
        { type: F, ref: W, shapeFlag: Z, patchFlag: z } = p;
      let X = f.nodeType;
      (p.el = f), z === -2 && ((O = !1), (p.dynamicChildren = null));
      let V = null;
      switch (F) {
        case Jt:
          X !== 3
            ? p.children === ""
              ? (r((p.el = i("")), l(f), f), (V = f))
              : (V = x())
            : (f.data !== p.children && ((ft = !0), (f.data = p.children)),
              (V = n(f)));
          break;
        case Me:
          X !== 8 || M ? (V = x()) : (V = n(f));
          break;
        case la:
          if ((M && ((f = n(f)), (X = f.nodeType)), X === 1 || X === 3)) {
            V = f;
            const je = !p.children.length;
            for (let ae = 0; ae < p.staticCount; ae++)
              je && (p.children += V.nodeType === 1 ? V.outerHTML : V.data),
                ae === p.staticCount - 1 && (p.anchor = V),
                (V = n(V));
            return M ? n(V) : V;
          } else x();
          break;
        case ne:
          M ? (V = b(f, p, q, v, C, O)) : (V = x());
          break;
        default:
          if (Z & 1)
            X !== 1 || p.type.toLowerCase() !== f.tagName.toLowerCase()
              ? (V = x())
              : (V = d(f, p, q, v, C, O));
          else if (Z & 6) {
            p.slotScopeIds = C;
            const je = l(f);
            if (
              (t(p, je, null, q, v, Gs(je), O),
              (V = M ? R(f) : n(f)),
              V && Xs(V) && V.data === "teleport end" && (V = n(V)),
              Mt(p))
            ) {
              let ae;
              M
                ? ((ae = Q(ne)),
                  (ae.anchor = V ? V.previousSibling : je.lastChild))
                : (ae = f.nodeType === 3 ? cs("") : Q("div")),
                (ae.el = f),
                (p.component.subTree = ae);
            }
          } else
            Z & 64
              ? X !== 8
                ? (V = x())
                : (V = p.type.hydrate(f, p, q, v, C, O, e, _))
              : Z & 128 &&
                (V = p.type.hydrate(f, p, q, v, Gs(l(f)), C, O, e, m));
      }
      return W != null && pa(W, null, v, p), V;
    },
    d = (f, p, q, v, C, O) => {
      O = O || !!p.dynamicChildren;
      const { type: M, props: x, patchFlag: F, shapeFlag: W, dirs: Z } = p,
        z = (M === "input" && Z) || M === "option";
      if (z || F !== -1) {
        if ((Z && it(p, null, q, "created"), x))
          if (z || !O || F & 48)
            for (const V in x)
              ((z && V.endsWith("value")) || (Os(V) && !ys(V))) &&
                a(f, V, null, x[V], !1, void 0, q);
          else x.onClick && a(f, "onClick", null, x.onClick, !1, void 0, q);
        let X;
        if (
          ((X = x && x.onVnodeBeforeMount) && Ue(X, q, p),
          Z && it(p, null, q, "beforeMount"),
          ((X = x && x.onVnodeMounted) || Z) &&
            cr(() => {
              X && Ue(X, q, p), Z && it(p, null, q, "mounted");
            }, v),
          W & 16 && !(x && (x.innerHTML || x.textContent)))
        ) {
          let V = _(f.firstChild, p, f, q, v, C, O);
          for (; V; ) {
            ft = !0;
            const je = V;
            (V = V.nextSibling), c(je);
          }
        } else
          W & 8 &&
            f.textContent !== p.children &&
            ((ft = !0), (f.textContent = p.children));
      }
      return f.nextSibling;
    },
    _ = (f, p, q, v, C, O, M) => {
      M = M || !!p.dynamicChildren;
      const x = p.children,
        F = x.length;
      for (let W = 0; W < F; W++) {
        const Z = M ? x[W] : (x[W] = ze(x[W]));
        if (f) f = m(f, Z, v, C, O, M);
        else {
          if (Z.type === Jt && !Z.children) continue;
          (ft = !0), s(null, Z, q, null, v, C, Gs(q), O);
        }
      }
      return f;
    },
    b = (f, p, q, v, C, O) => {
      const { slotScopeIds: M } = p;
      M && (C = C ? C.concat(M) : M);
      const x = l(f),
        F = _(n(f), p, x, q, v, C, O);
      return F && Xs(F) && F.data === "]"
        ? n((p.anchor = F))
        : ((ft = !0), r((p.anchor = o("]")), x, F), F);
    },
    w = (f, p, q, v, C, O) => {
      if (((ft = !0), (p.el = null), O)) {
        const F = R(f);
        for (;;) {
          const W = n(f);
          if (W && W !== F) c(W);
          else break;
        }
      }
      const M = n(f),
        x = l(f);
      return c(f), s(null, p, x, M, q, v, Gs(x), C), M;
    },
    R = f => {
      let p = 0;
      for (; f; )
        if (
          ((f = n(f)), f && Xs(f) && (f.data === "[" && p++, f.data === "]"))
        ) {
          if (p === 0) return n(f);
          p--;
        }
      return f;
    };
  return [u, m];
}
const Ce = cr;
function _u(e) {
  return Ir(e);
}
function yu(e) {
  return Ir(e, bu);
}
function Ir(e, t) {
  const s = ti();
  s.__VUE__ = !0;
  const {
      insert: a,
      remove: i,
      patchProp: n,
      createElement: l,
      createText: c,
      createComment: r,
      setText: o,
      setElementText: u,
      parentNode: m,
      nextSibling: d,
      setScopeId: _ = Je,
      insertStaticContent: b,
    } = e,
    w = (
      h,
      g,
      y,
      j = null,
      A = null,
      S = null,
      D = !1,
      N = null,
      U = !!g.dynamicChildren
    ) => {
      if (h === g) return;
      h && !Ke(h, g) && ((j = P(h)), Ne(h, A, S, !0), (h = null)),
        g.patchFlag === -2 && ((U = !1), (g.dynamicChildren = null));
      const { type: I, ref: K, shapeFlag: Y } = g;
      switch (I) {
        case Jt:
          R(h, g, y, j);
          break;
        case Me:
          f(h, g, y, j);
          break;
        case la:
          h == null && p(g, y, j, D);
          break;
        case ne:
          z(h, g, y, j, A, S, D, N, U);
          break;
        default:
          Y & 1
            ? C(h, g, y, j, A, S, D, N, U)
            : Y & 6
              ? X(h, g, y, j, A, S, D, N, U)
              : (Y & 64 || Y & 128) && I.process(h, g, y, j, A, S, D, N, U, L);
      }
      K != null && A && pa(K, h && h.ref, S, g || h, !g);
    },
    R = (h, g, y, j) => {
      if (h == null) a((g.el = c(g.children)), y, j);
      else {
        const A = (g.el = h.el);
        g.children !== h.children && o(A, g.children);
      }
    },
    f = (h, g, y, j) => {
      h == null ? a((g.el = r(g.children || "")), y, j) : (g.el = h.el);
    },
    p = (h, g, y, j) => {
      [h.el, h.anchor] = b(h.children, g, y, j, h.el, h.anchor);
    },
    q = ({ el: h, anchor: g }, y, j) => {
      let A;
      for (; h && h !== g; ) (A = d(h)), a(h, y, j), (h = A);
      a(g, y, j);
    },
    v = ({ el: h, anchor: g }) => {
      let y;
      for (; h && h !== g; ) (y = d(h)), i(h), (h = y);
      i(g);
    },
    C = (h, g, y, j, A, S, D, N, U) => {
      (D = D || g.type === "svg"),
        h == null ? O(g, y, j, A, S, D, N, U) : F(h, g, A, S, D, N, U);
    },
    O = (h, g, y, j, A, S, D, N) => {
      let U, I;
      const { type: K, props: Y, shapeFlag: J, transition: te, dirs: se } = h;
      if (
        ((U = h.el = l(h.type, S, Y && Y.is, Y)),
        J & 8
          ? u(U, h.children)
          : J & 16 &&
            x(h.children, U, null, j, A, S && K !== "foreignObject", D, N),
        se && it(h, null, j, "created"),
        M(U, h, h.scopeId, D, j),
        Y)
      ) {
        for (const he in Y)
          he !== "value" &&
            !ys(he) &&
            n(U, he, null, Y[he], S, h.children, j, A, Se);
        "value" in Y && n(U, "value", null, Y.value),
          (I = Y.onVnodeBeforeMount) && Ue(I, j, h);
      }
      se && it(h, null, j, "beforeMount");
      const de = (!A || (A && !A.pendingBranch)) && te && !te.persisted;
      de && te.beforeEnter(U),
        a(U, g, y),
        ((I = Y && Y.onVnodeMounted) || de || se) &&
          Ce(() => {
            I && Ue(I, j, h),
              de && te.enter(U),
              se && it(h, null, j, "mounted");
          }, A);
    },
    M = (h, g, y, j, A) => {
      if ((y && _(h, y), j)) for (let S = 0; S < j.length; S++) _(h, j[S]);
      if (A) {
        let S = A.subTree;
        if (g === S) {
          const D = A.vnode;
          M(h, D, D.scopeId, D.slotScopeIds, A.parent);
        }
      }
    },
    x = (h, g, y, j, A, S, D, N, U = 0) => {
      for (let I = U; I < h.length; I++) {
        const K = (h[I] = N ? vt(h[I]) : ze(h[I]));
        w(null, K, g, y, j, A, S, D, N);
      }
    },
    F = (h, g, y, j, A, S, D) => {
      const N = (g.el = h.el);
      let { patchFlag: U, dynamicChildren: I, dirs: K } = g;
      U |= h.patchFlag & 16;
      const Y = h.props || fe,
        J = g.props || fe;
      let te;
      y && Pt(y, !1),
        (te = J.onVnodeBeforeUpdate) && Ue(te, y, g, h),
        K && it(g, h, y, "beforeUpdate"),
        y && Pt(y, !0);
      const se = A && g.type !== "foreignObject";
      if (
        (I
          ? W(h.dynamicChildren, I, N, y, j, se, S)
          : D || oe(h, g, N, null, y, j, se, S, !1),
        U > 0)
      ) {
        if (U & 16) Z(N, g, Y, J, y, j, A);
        else if (
          (U & 2 && Y.class !== J.class && n(N, "class", null, J.class, A),
          U & 4 && n(N, "style", Y.style, J.style, A),
          U & 8)
        ) {
          const de = g.dynamicProps;
          for (let he = 0; he < de.length; he++) {
            const xe = de[he],
              We = Y[xe],
              Dt = J[xe];
            (Dt !== We || xe === "value") &&
              n(N, xe, We, Dt, A, h.children, y, j, Se);
          }
        }
        U & 1 && h.children !== g.children && u(N, g.children);
      } else !D && I == null && Z(N, g, Y, J, y, j, A);
      ((te = J.onVnodeUpdated) || K) &&
        Ce(() => {
          te && Ue(te, y, g, h), K && it(g, h, y, "updated");
        }, j);
    },
    W = (h, g, y, j, A, S, D) => {
      for (let N = 0; N < g.length; N++) {
        const U = h[N],
          I = g[N],
          K =
            U.el && (U.type === ne || !Ke(U, I) || U.shapeFlag & 70)
              ? m(U.el)
              : y;
        w(U, I, K, null, j, A, S, D, !0);
      }
    },
    Z = (h, g, y, j, A, S, D) => {
      if (y !== j) {
        if (y !== fe)
          for (const N in y)
            !ys(N) && !(N in j) && n(h, N, y[N], null, D, g.children, A, S, Se);
        for (const N in j) {
          if (ys(N)) continue;
          const U = j[N],
            I = y[N];
          U !== I && N !== "value" && n(h, N, I, U, D, g.children, A, S, Se);
        }
        "value" in j && n(h, "value", y.value, j.value);
      }
    },
    z = (h, g, y, j, A, S, D, N, U) => {
      const I = (g.el = h ? h.el : c("")),
        K = (g.anchor = h ? h.anchor : c(""));
      let { patchFlag: Y, dynamicChildren: J, slotScopeIds: te } = g;
      te && (N = N ? N.concat(te) : te),
        h == null
          ? (a(I, y, j), a(K, y, j), x(g.children, y, K, A, S, D, N, U))
          : Y > 0 && Y & 64 && J && h.dynamicChildren
            ? (W(h.dynamicChildren, J, y, A, S, D, N),
              (g.key != null || (A && g === A.subTree)) && Ji(h, g, !0))
            : oe(h, g, y, K, A, S, D, N, U);
    },
    X = (h, g, y, j, A, S, D, N, U) => {
      (g.slotScopeIds = N),
        h == null
          ? g.shapeFlag & 512
            ? A.ctx.activate(g, y, j, D, U)
            : V(g, y, j, A, S, D, U)
          : je(h, g, U);
    },
    V = (h, g, y, j, A, S, D) => {
      const N = (h.component = Cu(h, j, A));
      if ((Ls(h) && (N.ctx.renderer = L), Su(N), N.asyncDep)) {
        if ((A && A.registerDep(N, ae), !h.el)) {
          const U = (N.subTree = Q(Me));
          f(null, U, g, y);
        }
        return;
      }
      ae(N, h, g, y, A, S, D);
    },
    je = (h, g, y) => {
      const j = (g.component = h.component);
      if (Oo(h, g, y))
        if (j.asyncDep && !j.asyncResolved) {
          ce(j, g, y);
          return;
        } else (j.next = g), So(j.update), j.update();
      else (g.el = h.el), (j.vnode = g);
    },
    ae = (h, g, y, j, A, S, D) => {
      const N = () => {
          if (h.isMounted) {
            let { next: K, bu: Y, u: J, parent: te, vnode: se } = h,
              de = K,
              he;
            Pt(h, !1),
              K ? ((K.el = se.el), ce(h, K, D)) : (K = se),
              Y && vs(Y),
              (he = K.props && K.props.onVnodeBeforeUpdate) &&
                Ue(he, te, K, se),
              Pt(h, !0);
            const xe = La(h),
              We = h.subTree;
            (h.subTree = xe),
              w(We, xe, m(We.el), P(We), h, A, S),
              (K.el = xe.el),
              de === null && zi(h, xe.el),
              J && Ce(J, A),
              (he = K.props && K.props.onVnodeUpdated) &&
                Ce(() => Ue(he, te, K, se), A);
          } else {
            let K;
            const { el: Y, props: J } = g,
              { bm: te, m: se, parent: de } = h,
              he = Mt(g);
            if (
              (Pt(h, !1),
              te && vs(te),
              !he && (K = J && J.onVnodeBeforeMount) && Ue(K, de, g),
              Pt(h, !0),
              Y && ue)
            ) {
              const xe = () => {
                (h.subTree = La(h)), ue(Y, h.subTree, h, A, null);
              };
              he
                ? g.type.__asyncLoader().then(() => !h.isUnmounted && xe())
                : xe();
            } else {
              const xe = (h.subTree = La(h));
              w(null, xe, y, j, h, A, S), (g.el = xe.el);
            }
            if ((se && Ce(se, A), !he && (K = J && J.onVnodeMounted))) {
              const xe = g;
              Ce(() => Ue(K, de, xe), A);
            }
            (g.shapeFlag & 256 ||
              (de && Mt(de.vnode) && de.vnode.shapeFlag & 256)) &&
              h.a &&
              Ce(h.a, A),
              (h.isMounted = !0),
              (g = y = j = null);
          }
        },
        U = (h.effect = new Ni(N, () => Pa(I), h.scope)),
        I = (h.update = () => U.run());
      (I.id = h.uid), Pt(h, !0), I();
    },
    ce = (h, g, y) => {
      g.component = h;
      const j = h.vnode.props;
      (h.vnode = g),
        (h.next = null),
        du(h, g.props, j, y),
        fu(h, g.children, y),
        is(),
        vn(),
        ns();
    },
    oe = (h, g, y, j, A, S, D, N, U = !1) => {
      const I = h && h.children,
        K = h ? h.shapeFlag : 0,
        Y = g.children,
        { patchFlag: J, shapeFlag: te } = g;
      if (J > 0) {
        if (J & 128) {
          gt(I, Y, y, j, A, S, D, N, U);
          return;
        } else if (J & 256) {
          rt(I, Y, y, j, A, S, D, N, U);
          return;
        }
      }
      te & 8
        ? (K & 16 && Se(I, A, S), Y !== I && u(y, Y))
        : K & 16
          ? te & 16
            ? gt(I, Y, y, j, A, S, D, N, U)
            : Se(I, A, S, !0)
          : (K & 8 && u(y, ""), te & 16 && x(Y, y, j, A, S, D, N, U));
    },
    rt = (h, g, y, j, A, S, D, N, U) => {
      (h = h || Vt), (g = g || Vt);
      const I = h.length,
        K = g.length,
        Y = Math.min(I, K);
      let J;
      for (J = 0; J < Y; J++) {
        const te = (g[J] = U ? vt(g[J]) : ze(g[J]));
        w(h[J], te, y, null, A, S, D, N, U);
      }
      I > K ? Se(h, A, S, !0, !1, Y) : x(g, y, j, A, S, D, N, U, Y);
    },
    gt = (h, g, y, j, A, S, D, N, U) => {
      let I = 0;
      const K = g.length;
      let Y = h.length - 1,
        J = K - 1;
      for (; I <= Y && I <= J; ) {
        const te = h[I],
          se = (g[I] = U ? vt(g[I]) : ze(g[I]));
        if (Ke(te, se)) w(te, se, y, null, A, S, D, N, U);
        else break;
        I++;
      }
      for (; I <= Y && I <= J; ) {
        const te = h[Y],
          se = (g[J] = U ? vt(g[J]) : ze(g[J]));
        if (Ke(te, se)) w(te, se, y, null, A, S, D, N, U);
        else break;
        Y--, J--;
      }
      if (I > Y) {
        if (I <= J) {
          const te = J + 1,
            se = te < K ? g[te].el : j;
          for (; I <= J; )
            w(null, (g[I] = U ? vt(g[I]) : ze(g[I])), y, se, A, S, D, N, U),
              I++;
        }
      } else if (I > J) for (; I <= Y; ) Ne(h[I], A, S, !0), I++;
      else {
        const te = I,
          se = I,
          de = new Map();
        for (I = se; I <= J; I++) {
          const He = (g[I] = U ? vt(g[I]) : ze(g[I]));
          He.key != null && de.set(He.key, I);
        }
        let he,
          xe = 0;
        const We = J - se + 1;
        let Dt = !1,
          on = 0;
        const ms = new Array(We);
        for (I = 0; I < We; I++) ms[I] = 0;
        for (I = te; I <= Y; I++) {
          const He = h[I];
          if (xe >= We) {
            Ne(He, A, S, !0);
            continue;
          }
          let st;
          if (He.key != null) st = de.get(He.key);
          else
            for (he = se; he <= J; he++)
              if (ms[he - se] === 0 && Ke(He, g[he])) {
                st = he;
                break;
              }
          st === void 0
            ? Ne(He, A, S, !0)
            : ((ms[st - se] = I + 1),
              st >= on ? (on = st) : (Dt = !0),
              w(He, g[st], y, null, A, S, D, N, U),
              xe++);
        }
        const un = Dt ? vu(ms) : Vt;
        for (he = un.length - 1, I = We - 1; I >= 0; I--) {
          const He = se + I,
            st = g[He],
            mn = He + 1 < K ? g[He + 1].el : j;
          ms[I] === 0
            ? w(null, st, y, mn, A, S, D, N, U)
            : Dt && (he < 0 || I !== un[he] ? tt(st, y, mn, 2) : he--);
        }
      }
    },
    tt = (h, g, y, j, A = null) => {
      const { el: S, type: D, transition: N, children: U, shapeFlag: I } = h;
      if (I & 6) {
        tt(h.component.subTree, g, y, j);
        return;
      }
      if (I & 128) {
        h.suspense.move(g, y, j);
        return;
      }
      if (I & 64) {
        D.move(h, g, y, L);
        return;
      }
      if (D === ne) {
        a(S, g, y);
        for (let Y = 0; Y < U.length; Y++) tt(U[Y], g, y, j);
        a(h.anchor, g, y);
        return;
      }
      if (D === la) {
        q(h, g, y);
        return;
      }
      if (j !== 2 && I & 1 && N)
        if (j === 0) N.beforeEnter(S), a(S, g, y), Ce(() => N.enter(S), A);
        else {
          const { leave: Y, delayLeave: J, afterLeave: te } = N,
            se = () => a(S, g, y),
            de = () => {
              Y(S, () => {
                se(), te && te();
              });
            };
          J ? J(S, se, de) : de();
        }
      else a(S, g, y);
    },
    Ne = (h, g, y, j = !1, A = !1) => {
      const {
        type: S,
        props: D,
        ref: N,
        children: U,
        dynamicChildren: I,
        shapeFlag: K,
        patchFlag: Y,
        dirs: J,
      } = h;
      if ((N != null && pa(N, null, y, h, !0), K & 256)) {
        g.ctx.deactivate(h);
        return;
      }
      const te = K & 1 && J,
        se = !Mt(h);
      let de;
      if ((se && (de = D && D.onVnodeBeforeUnmount) && Ue(de, g, h), K & 6))
        Vs(h.component, y, j);
      else {
        if (K & 128) {
          h.suspense.unmount(y, j);
          return;
        }
        te && it(h, null, g, "beforeUnmount"),
          K & 64
            ? h.type.remove(h, g, y, A, L, j)
            : I && (S !== ne || (Y > 0 && Y & 64))
              ? Se(I, g, y, !1, !0)
              : ((S === ne && Y & 384) || (!A && K & 16)) && Se(U, g, y),
          j && Ht(h);
      }
      ((se && (de = D && D.onVnodeUnmounted)) || te) &&
        Ce(() => {
          de && Ue(de, g, h), te && it(h, null, g, "unmounted");
        }, y);
    },
    Ht = h => {
      const { type: g, el: y, anchor: j, transition: A } = h;
      if (g === ne) {
        Ft(y, j);
        return;
      }
      if (g === la) {
        v(h);
        return;
      }
      const S = () => {
        i(y), A && !A.persisted && A.afterLeave && A.afterLeave();
      };
      if (h.shapeFlag & 1 && A && !A.persisted) {
        const { leave: D, delayLeave: N } = A,
          U = () => D(y, S);
        N ? N(h.el, S, U) : U();
      } else S();
    },
    Ft = (h, g) => {
      let y;
      for (; h !== g; ) (y = d(h)), i(h), (h = y);
      i(g);
    },
    Vs = (h, g, y) => {
      const { bum: j, scope: A, update: S, subTree: D, um: N } = h;
      j && vs(j),
        A.stop(),
        S && ((S.active = !1), Ne(D, h, g, y)),
        N && Ce(N, g),
        Ce(() => {
          h.isUnmounted = !0;
        }, g),
        g &&
          g.pendingBranch &&
          !g.isUnmounted &&
          h.asyncDep &&
          !h.asyncResolved &&
          h.suspenseId === g.pendingId &&
          (g.deps--, g.deps === 0 && g.resolve());
    },
    Se = (h, g, y, j = !1, A = !1, S = 0) => {
      for (let D = S; D < h.length; D++) Ne(h[D], g, y, j, A);
    },
    P = h =>
      h.shapeFlag & 6
        ? P(h.component.subTree)
        : h.shapeFlag & 128
          ? h.suspense.next()
          : d(h.anchor || h.el),
    B = (h, g, y) => {
      h == null
        ? g._vnode && Ne(g._vnode, null, null, !0)
        : w(g._vnode || null, h, g, null, null, null, y),
        vn(),
        ma(),
        (g._vnode = h);
    },
    L = {
      p: w,
      um: Ne,
      m: tt,
      r: Ht,
      mt: V,
      mc: x,
      pc: oe,
      pbc: W,
      n: P,
      o: e,
    };
  let $, ue;
  return t && ([$, ue] = t(L)), { render: B, hydrate: $, createApp: mu(B, $) };
}
function Pt({ effect: e, update: t }, s) {
  e.allowRecurse = t.allowRecurse = s;
}
function Ji(e, t, s = !1) {
  const a = e.children,
    i = t.children;
  if (G(a) && G(i))
    for (let n = 0; n < a.length; n++) {
      const l = a[n];
      let c = i[n];
      c.shapeFlag & 1 &&
        !c.dynamicChildren &&
        ((c.patchFlag <= 0 || c.patchFlag === 32) &&
          ((c = i[n] = vt(i[n])), (c.el = l.el)),
        s || Ji(l, c)),
        c.type === Jt && (c.el = l.el);
    }
}
function vu(e) {
  const t = e.slice(),
    s = [0];
  let a, i, n, l, c;
  const r = e.length;
  for (a = 0; a < r; a++) {
    const o = e[a];
    if (o !== 0) {
      if (((i = s[s.length - 1]), e[i] < o)) {
        (t[a] = i), s.push(a);
        continue;
      }
      for (n = 0, l = s.length - 1; n < l; )
        (c = (n + l) >> 1), e[s[c]] < o ? (n = c + 1) : (l = c);
      o < e[s[n]] && (n > 0 && (t[a] = s[n - 1]), (s[n] = a));
    }
  }
  for (n = s.length, l = s[n - 1]; n-- > 0; ) (s[n] = l), (l = t[l]);
  return s;
}
const wu = e => e.__isTeleport,
  js = e => e && (e.disabled || e.disabled === ""),
  Rn = e => typeof SVGElement < "u" && e instanceof SVGElement,
  mi = (e, t) => {
    const s = e && e.to;
    return be(s) ? (t ? t(s) : null) : s;
  },
  ju = {
    __isTeleport: !0,
    process(e, t, s, a, i, n, l, c, r, o) {
      const {
          mc: u,
          pc: m,
          pbc: d,
          o: { insert: _, querySelector: b, createText: w, createComment: R },
        } = o,
        f = js(t.props);
      let { shapeFlag: p, children: q, dynamicChildren: v } = t;
      if (e == null) {
        const C = (t.el = w("")),
          O = (t.anchor = w(""));
        _(C, s, a), _(O, s, a);
        const M = (t.target = mi(t.props, b)),
          x = (t.targetAnchor = w(""));
        M && (_(x, M), (l = l || Rn(M)));
        const F = (W, Z) => {
          p & 16 && u(q, W, Z, i, n, l, c, r);
        };
        f ? F(s, O) : M && F(M, x);
      } else {
        t.el = e.el;
        const C = (t.anchor = e.anchor),
          O = (t.target = e.target),
          M = (t.targetAnchor = e.targetAnchor),
          x = js(e.props),
          F = x ? s : O,
          W = x ? C : M;
        if (
          ((l = l || Rn(O)),
          v
            ? (d(e.dynamicChildren, v, F, i, n, l, c), Ji(e, t, !0))
            : r || m(e, t, F, W, i, n, l, c, !1),
          f)
        )
          x || ea(t, s, C, o, 1);
        else if ((t.props && t.props.to) !== (e.props && e.props.to)) {
          const Z = (t.target = mi(t.props, b));
          Z && ea(t, Z, null, o, 0);
        } else x && ea(t, O, M, o, 1);
      }
      Rr(t);
    },
    remove(e, t, s, a, { um: i, o: { remove: n } }, l) {
      const {
        shapeFlag: c,
        children: r,
        anchor: o,
        targetAnchor: u,
        target: m,
        props: d,
      } = e;
      if ((m && n(u), (l || !js(d)) && (n(o), c & 16)))
        for (let _ = 0; _ < r.length; _++) {
          const b = r[_];
          i(b, t, s, !0, !!b.dynamicChildren);
        }
    },
    move: ea,
    hydrate: xu,
  };
function ea(e, t, s, { o: { insert: a }, m: i }, n = 2) {
  n === 0 && a(e.targetAnchor, t, s);
  const { el: l, anchor: c, shapeFlag: r, children: o, props: u } = e,
    m = n === 2;
  if ((m && a(l, t, s), (!m || js(u)) && r & 16))
    for (let d = 0; d < o.length; d++) i(o[d], t, s, 2);
  m && a(c, t, s);
}
function xu(
  e,
  t,
  s,
  a,
  i,
  n,
  { o: { nextSibling: l, parentNode: c, querySelector: r } },
  o
) {
  const u = (t.target = mi(t.props, r));
  if (u) {
    const m = u._lpa || u.firstChild;
    if (t.shapeFlag & 16)
      if (js(t.props))
        (t.anchor = o(l(e), t, c(e), s, a, i, n)), (t.targetAnchor = m);
      else {
        t.anchor = l(e);
        let d = m;
        for (; d; )
          if (
            ((d = l(d)), d && d.nodeType === 8 && d.data === "teleport anchor")
          ) {
            (t.targetAnchor = d),
              (u._lpa = t.targetAnchor && l(t.targetAnchor));
            break;
          }
        o(m, t, u, s, a, i, n);
      }
    Rr(t);
  }
  return t.anchor && l(t.anchor);
}
const Tr = ju;
function Rr(e) {
  const t = e.ctx;
  if (t && t.ut) {
    let s = e.children[0].el;
    for (; s !== e.targetAnchor; )
      s.nodeType === 1 && s.setAttribute("data-v-owner", t.uid),
        (s = s.nextSibling);
    t.ut();
  }
}
const ne = Symbol.for("v-fgt"),
  Jt = Symbol.for("v-txt"),
  Me = Symbol.for("v-cmt"),
  la = Symbol.for("v-stc"),
  xs = [];
let Be = null;
function T(e = !1) {
  xs.push((Be = e ? null : []));
}
function Mr() {
  xs.pop(), (Be = xs[xs.length - 1] || null);
}
let Zt = 1;
function Mn(e) {
  Zt += e;
}
function Nr(e) {
  return (
    (e.dynamicChildren = Zt > 0 ? Be || Vt : null),
    Mr(),
    Zt > 0 && Be && Be.push(e),
    e
  );
}
function H(e, t, s, a, i, n) {
  return Nr(k(e, t, s, a, i, n, !0));
}
function _e(e, t, s, a, i) {
  return Nr(Q(e, t, s, a, i, !0));
}
function Gt(e) {
  return e ? e.__v_isVNode === !0 : !1;
}
function Ke(e, t) {
  return e.type === t.type && e.key === t.key;
}
const Sa = "__vInternal",
  Or = ({ key: e }) => e ?? null,
  ra = ({ ref: e, ref_key: t, ref_for: s }) => (
    typeof e == "number" && (e = "" + e),
    e != null
      ? be(e) || Ee(e) || ee(e)
        ? { i: Ae, r: e, k: t, f: !!s }
        : e
      : null
  );
function k(
  e,
  t = null,
  s = null,
  a = 0,
  i = null,
  n = e === ne ? 0 : 1,
  l = !1,
  c = !1
) {
  const r = {
    __v_isVNode: !0,
    __v_skip: !0,
    type: e,
    props: t,
    key: t && Or(t),
    ref: t && ra(t),
    scopeId: Aa,
    slotScopeIds: null,
    children: s,
    component: null,
    suspense: null,
    ssContent: null,
    ssFallback: null,
    dirs: null,
    transition: null,
    el: null,
    anchor: null,
    target: null,
    targetAnchor: null,
    staticCount: 0,
    shapeFlag: n,
    patchFlag: a,
    dynamicProps: i,
    dynamicChildren: null,
    appContext: null,
    ctx: Ae,
  };
  return (
    c
      ? (Zi(r, s), n & 128 && e.normalize(r))
      : s && (r.shapeFlag |= be(s) ? 8 : 16),
    Zt > 0 &&
      !l &&
      Be &&
      (r.patchFlag > 0 || n & 6) &&
      r.patchFlag !== 32 &&
      Be.push(r),
    r
  );
}
const Q = qu;
function qu(e, t = null, s = null, a = 0, i = null, n = !1) {
  if (((!e || e === yr) && (e = Me), Gt(e))) {
    const c = mt(e, t, !0);
    return (
      s && Zi(c, s),
      Zt > 0 &&
        !n &&
        Be &&
        (c.shapeFlag & 6 ? (Be[Be.indexOf(e)] = c) : Be.push(c)),
      (c.patchFlag |= -2),
      c
    );
  }
  if ((Mu(e) && (e = e.__vccOpts), t)) {
    t = ku(t);
    let { class: c, style: r } = t;
    c && !be(c) && (t.class = E(c)),
      ge(r) && ($l(r) && !G(r) && (r = ke({}, r)), (t.style = qa(r)));
  }
  const l = be(e) ? 1 : lr(e) ? 128 : wu(e) ? 64 : ge(e) ? 4 : ee(e) ? 2 : 0;
  return k(e, t, s, a, i, l, n, !0);
}
function ku(e) {
  return e ? ($l(e) || Sa in e ? ke({}, e) : e) : null;
}
function mt(e, t, s = !1) {
  const { props: a, ref: i, patchFlag: n, children: l } = e,
    c = t ? Pu(a || {}, t) : a;
  return {
    __v_isVNode: !0,
    __v_skip: !0,
    type: e.type,
    props: c,
    key: c && Or(c),
    ref:
      t && t.ref ? (s && i ? (G(i) ? i.concat(ra(t)) : [i, ra(t)]) : ra(t)) : i,
    scopeId: e.scopeId,
    slotScopeIds: e.slotScopeIds,
    children: l,
    target: e.target,
    targetAnchor: e.targetAnchor,
    staticCount: e.staticCount,
    shapeFlag: e.shapeFlag,
    patchFlag: t && e.type !== ne ? (n === -1 ? 16 : n | 16) : n,
    dynamicProps: e.dynamicProps,
    dynamicChildren: e.dynamicChildren,
    appContext: e.appContext,
    dirs: e.dirs,
    transition: e.transition,
    component: e.component,
    suspense: e.suspense,
    ssContent: e.ssContent && mt(e.ssContent),
    ssFallback: e.ssFallback && mt(e.ssFallback),
    el: e.el,
    anchor: e.anchor,
    ctx: e.ctx,
    ce: e.ce,
  };
}
function cs(e = " ", t = 0) {
  return Q(Jt, null, e, t);
}
function Pe(e = "", t = !1) {
  return t ? (T(), _e(Me, null, e)) : Q(Me, null, e);
}
function ze(e) {
  return e == null || typeof e == "boolean"
    ? Q(Me)
    : G(e)
      ? Q(ne, null, e.slice())
      : typeof e == "object"
        ? vt(e)
        : Q(Jt, null, String(e));
}
function vt(e) {
  return (e.el === null && e.patchFlag !== -1) || e.memo ? e : mt(e);
}
function Zi(e, t) {
  let s = 0;
  const { shapeFlag: a } = e;
  if (t == null) t = null;
  else if (G(t)) s = 16;
  else if (typeof t == "object")
    if (a & 65) {
      const i = t.default;
      i && (i._c && (i._d = !1), Zi(e, i()), i._c && (i._d = !0));
      return;
    } else {
      s = 32;
      const i = t._;
      !i && !(Sa in t)
        ? (t._ctx = Ae)
        : i === 3 &&
          Ae &&
          (Ae.slots._ === 1 ? (t._ = 1) : ((t._ = 2), (e.patchFlag |= 1024)));
    }
  else
    ee(t)
      ? ((t = { default: t, _ctx: Ae }), (s = 32))
      : ((t = String(t)), a & 64 ? ((s = 16), (t = [cs(t)])) : (s = 8));
  (e.children = t), (e.shapeFlag |= s);
}
function Pu(...e) {
  const t = {};
  for (let s = 0; s < e.length; s++) {
    const a = e[s];
    for (const i in a)
      if (i === "class")
        t.class !== a.class && (t.class = E([t.class, a.class]));
      else if (i === "style") t.style = qa([t.style, a.style]);
      else if (Os(i)) {
        const n = t[i],
          l = a[i];
        l &&
          n !== l &&
          !(G(n) && n.includes(l)) &&
          (t[i] = n ? [].concat(n, l) : l);
      } else i !== "" && (t[i] = a[i]);
  }
  return t;
}
function Ue(e, t, s, a = null) {
  Ve(e, t, 7, [s, a]);
}
const Eu = qr();
let Au = 0;
function Cu(e, t, s) {
  const a = e.type,
    i = (t ? t.appContext : e.appContext) || Eu,
    n = {
      uid: Au++,
      vnode: e,
      type: a,
      parent: t,
      appContext: i,
      root: null,
      next: null,
      subTree: null,
      effect: null,
      update: null,
      scope: new Qc(!0),
      render: null,
      proxy: null,
      exposed: null,
      exposeProxy: null,
      withProxy: null,
      provides: t ? t.provides : Object.create(i.provides),
      accessCache: null,
      renderCache: [],
      components: null,
      directives: null,
      propsOptions: Er(a, i),
      emitsOptions: nr(a, i),
      emit: null,
      emitted: null,
      propsDefaults: fe,
      inheritAttrs: a.inheritAttrs,
      ctx: fe,
      data: fe,
      props: fe,
      attrs: fe,
      slots: fe,
      refs: fe,
      setupState: fe,
      setupContext: null,
      attrsProxy: null,
      slotsProxy: null,
      suspense: s,
      suspenseId: s ? s.pendingId : 0,
      asyncDep: null,
      asyncResolved: !1,
      isMounted: !1,
      isUnmounted: !1,
      isDeactivated: !1,
      bc: null,
      c: null,
      bm: null,
      m: null,
      bu: null,
      u: null,
      um: null,
      bum: null,
      da: null,
      a: null,
      rtg: null,
      rtc: null,
      ec: null,
      sp: null,
    };
  return (
    (n.ctx = { _: n }),
    (n.root = t ? t.root : n),
    (n.emit = To.bind(null, n)),
    e.ce && e.ce(n),
    n
  );
}
let qe = null;
const Ds = () => qe || Ae;
let Gi,
  zt,
  Nn = "__VUE_INSTANCE_SETTERS__";
(zt = ti()[Nn]) || (zt = ti()[Nn] = []),
  zt.push(e => (qe = e)),
  (Gi = e => {
    zt.length > 1 ? zt.forEach(t => t(e)) : zt[0](e);
  });
const Xt = e => {
    Gi(e), e.scope.on();
  },
  Ot = () => {
    qe && qe.scope.off(), Gi(null);
  };
function Ur(e) {
  return e.vnode.shapeFlag & 4;
}
let es = !1;
function Su(e, t = !1) {
  es = t;
  const { props: s, children: a } = e.vnode,
    i = Ur(e);
  hu(e, s, i, t), pu(e, a);
  const n = i ? Iu(e, t) : void 0;
  return (es = !1), n;
}
function Iu(e, t) {
  const s = e.type;
  (e.accessCache = Object.create(null)), (e.proxy = Kl(new Proxy(e.ctx, iu)));
  const { setup: a } = s;
  if (a) {
    const i = (e.setupContext = a.length > 1 ? Ru(e) : null);
    Xt(e), is();
    const n = xt(a, e, 0, [e.props, i]);
    if ((ns(), Ot(), Sl(n))) {
      if ((n.then(Ot, Ot), t))
        return n
          .then(l => {
            hi(e, l, t);
          })
          .catch(l => {
            ls(l, e, 0);
          });
      e.asyncDep = n;
    } else hi(e, n, t);
  } else Lr(e, t);
}
function hi(e, t, s) {
  ee(t)
    ? e.type.__ssrInlineRender
      ? (e.ssrRender = t)
      : (e.render = t)
    : ge(t) && (e.setupState = Xl(t)),
    Lr(e, s);
}
let On;
function Lr(e, t, s) {
  const a = e.type;
  if (!e.render) {
    if (!t && On && !a.render) {
      const i = a.template || $i(e).template;
      if (i) {
        const { isCustomElement: n, compilerOptions: l } = e.appContext.config,
          { delimiters: c, compilerOptions: r } = a,
          o = ke(ke({ isCustomElement: n, delimiters: c }, l), r);
        a.render = On(i, o);
      }
    }
    e.render = a.render || Je;
  }
  Xt(e), is(), nu(e), ns(), Ot();
}
function Tu(e) {
  return (
    e.attrsProxy ||
    (e.attrsProxy = new Proxy(e.attrs, {
      get(t, s) {
        return Le(e, "get", "$attrs"), t[s];
      },
    }))
  );
}
function Ru(e) {
  const t = s => {
    e.exposed = s || {};
  };
  return {
    get attrs() {
      return Tu(e);
    },
    slots: e.slots,
    emit: e.emit,
    expose: t,
  };
}
function Ia(e) {
  if (e.exposed)
    return (
      e.exposeProxy ||
      (e.exposeProxy = new Proxy(Xl(Kl(e.exposed)), {
        get(t, s) {
          if (s in t) return t[s];
          if (s in ws) return ws[s](e);
        },
        has(t, s) {
          return s in t || s in ws;
        },
      }))
    );
}
function di(e, t = !0) {
  return ee(e) ? e.displayName || e.name : e.name || (t && e.__name);
}
function Mu(e) {
  return ee(e) && "__vccOpts" in e;
}
const Te = (e, t) => Eo(e, t, es);
function Ze(e, t, s) {
  const a = arguments.length;
  return a === 2
    ? ge(t) && !G(t)
      ? Gt(t)
        ? Q(e, null, [t])
        : Q(e, t)
      : Q(e, null, t)
    : (a > 3
        ? (s = Array.prototype.slice.call(arguments, 2))
        : a === 3 && Gt(s) && (s = [s]),
      Q(e, t, s));
}
const Nu = Symbol.for("v-scx"),
  Ou = () => ve(Nu),
  Hr = "3.3.4",
  Uu = "http://www.w3.org/2000/svg",
  St = typeof document < "u" ? document : null,
  Un = St && St.createElement("template"),
  Lu = {
    insert: (e, t, s) => {
      t.insertBefore(e, s || null);
    },
    remove: e => {
      const t = e.parentNode;
      t && t.removeChild(e);
    },
    createElement: (e, t, s, a) => {
      const i = t
        ? St.createElementNS(Uu, e)
        : St.createElement(e, s ? { is: s } : void 0);
      return (
        e === "select" &&
          a &&
          a.multiple != null &&
          i.setAttribute("multiple", a.multiple),
        i
      );
    },
    createText: e => St.createTextNode(e),
    createComment: e => St.createComment(e),
    setText: (e, t) => {
      e.nodeValue = t;
    },
    setElementText: (e, t) => {
      e.textContent = t;
    },
    parentNode: e => e.parentNode,
    nextSibling: e => e.nextSibling,
    querySelector: e => St.querySelector(e),
    setScopeId(e, t) {
      e.setAttribute(t, "");
    },
    insertStaticContent(e, t, s, a, i, n) {
      const l = s ? s.previousSibling : t.lastChild;
      if (i && (i === n || i.nextSibling))
        for (
          ;
          t.insertBefore(i.cloneNode(!0), s),
            !(i === n || !(i = i.nextSibling));

        );
      else {
        Un.innerHTML = a ? `<svg>${e}</svg>` : e;
        const c = Un.content;
        if (a) {
          const r = c.firstChild;
          for (; r.firstChild; ) c.appendChild(r.firstChild);
          c.removeChild(r);
        }
        t.insertBefore(c, s);
      }
      return [
        l ? l.nextSibling : t.firstChild,
        s ? s.previousSibling : t.lastChild,
      ];
    },
  };
function Hu(e, t, s) {
  const a = e._vtc;
  a && (t = (t ? [t, ...a] : [...a]).join(" ")),
    t == null
      ? e.removeAttribute("class")
      : s
        ? e.setAttribute("class", t)
        : (e.className = t);
}
function Fu(e, t, s) {
  const a = e.style,
    i = be(s);
  if (s && !i) {
    if (t && !be(t)) for (const n in t) s[n] == null && gi(a, n, "");
    for (const n in s) gi(a, n, s[n]);
  } else {
    const n = a.display;
    i ? t !== s && (a.cssText = s) : t && e.removeAttribute("style"),
      "_vod" in e && (a.display = n);
  }
}
const Ln = /\s*!important$/;
function gi(e, t, s) {
  if (G(s)) s.forEach(a => gi(e, t, a));
  else if ((s == null && (s = ""), t.startsWith("--"))) e.setProperty(t, s);
  else {
    const a = Du(e, t);
    Ln.test(s)
      ? e.setProperty(as(a), s.replace(Ln, ""), "important")
      : (e[a] = s);
  }
}
const Hn = ["Webkit", "Moz", "ms"],
  Qa = {};
function Du(e, t) {
  const s = Qa[t];
  if (s) return s;
  let a = lt(t);
  if (a !== "filter" && a in e) return (Qa[t] = a);
  a = xa(a);
  for (let i = 0; i < Hn.length; i++) {
    const n = Hn[i] + a;
    if (n in e) return (Qa[t] = n);
  }
  return t;
}
const Fn = "http://www.w3.org/1999/xlink";
function zu(e, t, s, a, i) {
  if (a && t.startsWith("xlink:"))
    s == null
      ? e.removeAttributeNS(Fn, t.slice(6, t.length))
      : e.setAttributeNS(Fn, t, s);
  else {
    const n = Bc(t);
    s == null || (n && !Ml(s))
      ? e.removeAttribute(t)
      : e.setAttribute(t, n ? "" : s);
  }
}
function Bu(e, t, s, a, i, n, l) {
  if (t === "innerHTML" || t === "textContent") {
    a && l(a, i, n), (e[t] = s ?? "");
    return;
  }
  const c = e.tagName;
  if (t === "value" && c !== "PROGRESS" && !c.includes("-")) {
    e._value = s;
    const o = c === "OPTION" ? e.getAttribute("value") : e.value,
      u = s ?? "";
    o !== u && (e.value = u), s == null && e.removeAttribute(t);
    return;
  }
  let r = !1;
  if (s === "" || s == null) {
    const o = typeof e[t];
    o === "boolean"
      ? (s = Ml(s))
      : s == null && o === "string"
        ? ((s = ""), (r = !0))
        : o === "number" && ((s = 0), (r = !0));
  }
  try {
    e[t] = s;
  } catch {}
  r && e.removeAttribute(t);
}
function Qu(e, t, s, a) {
  e.addEventListener(t, s, a);
}
function Vu(e, t, s, a) {
  e.removeEventListener(t, s, a);
}
function Wu(e, t, s, a, i = null) {
  const n = e._vei || (e._vei = {}),
    l = n[t];
  if (a && l) l.value = a;
  else {
    const [c, r] = Yu(t);
    if (a) {
      const o = (n[t] = Ju(a, i));
      Qu(e, c, o, r);
    } else l && (Vu(e, c, l, r), (n[t] = void 0));
  }
}
const Dn = /(?:Once|Passive|Capture)$/;
function Yu(e) {
  let t;
  if (Dn.test(e)) {
    t = {};
    let a;
    for (; (a = e.match(Dn)); )
      (e = e.slice(0, e.length - a[0].length)), (t[a[0].toLowerCase()] = !0);
  }
  return [e[2] === ":" ? e.slice(3) : as(e.slice(2)), t];
}
let Va = 0;
const $u = Promise.resolve(),
  Ku = () => Va || ($u.then(() => (Va = 0)), (Va = Date.now()));
function Ju(e, t) {
  const s = a => {
    if (!a._vts) a._vts = Date.now();
    else if (a._vts <= s.attached) return;
    Ve(Zu(a, s.value), t, 5, [a]);
  };
  return (s.value = e), (s.attached = Ku()), s;
}
function Zu(e, t) {
  if (G(t)) {
    const s = e.stopImmediatePropagation;
    return (
      (e.stopImmediatePropagation = () => {
        s.call(e), (e._stopped = !0);
      }),
      t.map(a => i => !i._stopped && a && a(i))
    );
  } else return t;
}
const zn = /^on[a-z]/,
  Gu = (e, t, s, a, i = !1, n, l, c, r) => {
    t === "class"
      ? Hu(e, a, i)
      : t === "style"
        ? Fu(e, s, a)
        : Os(t)
          ? Si(t) || Wu(e, t, s, a, l)
          : (
                t[0] === "."
                  ? ((t = t.slice(1)), !0)
                  : t[0] === "^"
                    ? ((t = t.slice(1)), !1)
                    : Xu(e, t, a, i)
              )
            ? Bu(e, t, a, n, l, c, r)
            : (t === "true-value"
                ? (e._trueValue = a)
                : t === "false-value" && (e._falseValue = a),
              zu(e, t, a, i));
  };
function Xu(e, t, s, a) {
  return a
    ? !!(
        t === "innerHTML" ||
        t === "textContent" ||
        (t in e && zn.test(t) && ee(s))
      )
    : t === "spellcheck" ||
        t === "draggable" ||
        t === "translate" ||
        t === "form" ||
        (t === "list" && e.tagName === "INPUT") ||
        (t === "type" && e.tagName === "TEXTAREA") ||
        (zn.test(t) && be(s))
      ? !1
      : t in e;
}
const bt = "transition",
  hs = "animation",
  Xi = (e, { slots: t }) => Ze(Yo, em(e), t);
Xi.displayName = "Transition";
const Fr = {
  name: String,
  type: String,
  css: { type: Boolean, default: !0 },
  duration: [String, Number, Object],
  enterFromClass: String,
  enterActiveClass: String,
  enterToClass: String,
  appearFromClass: String,
  appearActiveClass: String,
  appearToClass: String,
  leaveFromClass: String,
  leaveActiveClass: String,
  leaveToClass: String,
};
Xi.props = ke({}, ur, Fr);
const Et = (e, t = []) => {
    G(e) ? e.forEach(s => s(...t)) : e && e(...t);
  },
  Bn = e => (e ? (G(e) ? e.some(t => t.length > 1) : e.length > 1) : !1);
function em(e) {
  const t = {};
  for (const z in e) z in Fr || (t[z] = e[z]);
  if (e.css === !1) return t;
  const {
      name: s = "v",
      type: a,
      duration: i,
      enterFromClass: n = `${s}-enter-from`,
      enterActiveClass: l = `${s}-enter-active`,
      enterToClass: c = `${s}-enter-to`,
      appearFromClass: r = n,
      appearActiveClass: o = l,
      appearToClass: u = c,
      leaveFromClass: m = `${s}-leave-from`,
      leaveActiveClass: d = `${s}-leave-active`,
      leaveToClass: _ = `${s}-leave-to`,
    } = e,
    b = tm(i),
    w = b && b[0],
    R = b && b[1],
    {
      onBeforeEnter: f,
      onEnter: p,
      onEnterCancelled: q,
      onLeave: v,
      onLeaveCancelled: C,
      onBeforeAppear: O = f,
      onAppear: M = p,
      onAppearCancelled: x = q,
    } = t,
    F = (z, X, V) => {
      At(z, X ? u : c), At(z, X ? o : l), V && V();
    },
    W = (z, X) => {
      (z._isLeaving = !1), At(z, m), At(z, _), At(z, d), X && X();
    },
    Z = z => (X, V) => {
      const je = z ? M : p,
        ae = () => F(X, z, V);
      Et(je, [X, ae]),
        Qn(() => {
          At(X, z ? r : n), _t(X, z ? u : c), Bn(je) || Vn(X, a, w, ae);
        });
    };
  return ke(t, {
    onBeforeEnter(z) {
      Et(f, [z]), _t(z, n), _t(z, l);
    },
    onBeforeAppear(z) {
      Et(O, [z]), _t(z, r), _t(z, o);
    },
    onEnter: Z(!1),
    onAppear: Z(!0),
    onLeave(z, X) {
      z._isLeaving = !0;
      const V = () => W(z, X);
      _t(z, m),
        im(),
        _t(z, d),
        Qn(() => {
          z._isLeaving && (At(z, m), _t(z, _), Bn(v) || Vn(z, a, R, V));
        }),
        Et(v, [z, V]);
    },
    onEnterCancelled(z) {
      F(z, !1), Et(q, [z]);
    },
    onAppearCancelled(z) {
      F(z, !0), Et(x, [z]);
    },
    onLeaveCancelled(z) {
      W(z), Et(C, [z]);
    },
  });
}
function tm(e) {
  if (e == null) return null;
  if (ge(e)) return [Wa(e.enter), Wa(e.leave)];
  {
    const t = Wa(e);
    return [t, t];
  }
}
function Wa(e) {
  return Rl(e);
}
function _t(e, t) {
  t.split(/\s+/).forEach(s => s && e.classList.add(s)),
    (e._vtc || (e._vtc = new Set())).add(t);
}
function At(e, t) {
  t.split(/\s+/).forEach(a => a && e.classList.remove(a));
  const { _vtc: s } = e;
  s && (s.delete(t), s.size || (e._vtc = void 0));
}
function Qn(e) {
  requestAnimationFrame(() => {
    requestAnimationFrame(e);
  });
}
let sm = 0;
function Vn(e, t, s, a) {
  const i = (e._endId = ++sm),
    n = () => {
      i === e._endId && a();
    };
  if (s) return setTimeout(n, s);
  const { type: l, timeout: c, propCount: r } = am(e, t);
  if (!l) return a();
  const o = l + "end";
  let u = 0;
  const m = () => {
      e.removeEventListener(o, d), n();
    },
    d = _ => {
      _.target === e && ++u >= r && m();
    };
  setTimeout(() => {
    u < r && m();
  }, c + 1),
    e.addEventListener(o, d);
}
function am(e, t) {
  const s = window.getComputedStyle(e),
    a = b => (s[b] || "").split(", "),
    i = a(`${bt}Delay`),
    n = a(`${bt}Duration`),
    l = Wn(i, n),
    c = a(`${hs}Delay`),
    r = a(`${hs}Duration`),
    o = Wn(c, r);
  let u = null,
    m = 0,
    d = 0;
  t === bt
    ? l > 0 && ((u = bt), (m = l), (d = n.length))
    : t === hs
      ? o > 0 && ((u = hs), (m = o), (d = r.length))
      : ((m = Math.max(l, o)),
        (u = m > 0 ? (l > o ? bt : hs) : null),
        (d = u ? (u === bt ? n.length : r.length) : 0));
  const _ =
    u === bt && /\b(transform|all)(,|$)/.test(a(`${bt}Property`).toString());
  return { type: u, timeout: m, propCount: d, hasTransform: _ };
}
function Wn(e, t) {
  for (; e.length < t.length; ) e = e.concat(e);
  return Math.max(...t.map((s, a) => Yn(s) + Yn(e[a])));
}
function Yn(e) {
  return Number(e.slice(0, -1).replace(",", ".")) * 1e3;
}
function im() {
  return document.body.offsetHeight;
}
const en = {
  beforeMount(e, { value: t }, { transition: s }) {
    (e._vod = e.style.display === "none" ? "" : e.style.display),
      s && t ? s.beforeEnter(e) : ds(e, t);
  },
  mounted(e, { value: t }, { transition: s }) {
    s && t && s.enter(e);
  },
  updated(e, { value: t, oldValue: s }, { transition: a }) {
    !t != !s &&
      (a
        ? t
          ? (a.beforeEnter(e), ds(e, !0), a.enter(e))
          : a.leave(e, () => {
              ds(e, !1);
            })
        : ds(e, t));
  },
  beforeUnmount(e, { value: t }) {
    ds(e, t);
  },
};
function ds(e, t) {
  e.style.display = t ? e._vod : "none";
}
const Dr = ke({ patchProp: Gu }, Lu);
let qs,
  $n = !1;
function nm() {
  return qs || (qs = _u(Dr));
}
function lm() {
  return (qs = $n ? qs : yu(Dr)), ($n = !0), qs;
}
const rm = (...e) => {
    const t = nm().createApp(...e),
      { mount: s } = t;
    return (
      (t.mount = a => {
        const i = zr(a);
        if (!i) return;
        const n = t._component;
        !ee(n) && !n.render && !n.template && (n.template = i.innerHTML),
          (i.innerHTML = "");
        const l = s(i, !1, i instanceof SVGElement);
        return (
          i instanceof Element &&
            (i.removeAttribute("v-cloak"), i.setAttribute("data-v-app", "")),
          l
        );
      }),
      t
    );
  },
  cm = (...e) => {
    const t = lm().createApp(...e),
      { mount: s } = t;
    return (
      (t.mount = a => {
        const i = zr(a);
        if (i) return s(i, !0, i instanceof SVGElement);
      }),
      t
    );
  };
function zr(e) {
  return be(e) ? document.querySelector(e) : e;
}
const om =
    /"(?:_|\\u0{2}5[Ff]){2}(?:p|\\u0{2}70)(?:r|\\u0{2}72)(?:o|\\u0{2}6[Ff])(?:t|\\u0{2}74)(?:o|\\u0{2}6[Ff])(?:_|\\u0{2}5[Ff]){2}"\s*:/,
  um =
    /"(?:c|\\u0063)(?:o|\\u006[Ff])(?:n|\\u006[Ee])(?:s|\\u0073)(?:t|\\u0074)(?:r|\\u0072)(?:u|\\u0075)(?:c|\\u0063)(?:t|\\u0074)(?:o|\\u006[Ff])(?:r|\\u0072)"\s*:/,
  mm = /^\s*["[{]|^\s*-?\d[\d.]{0,14}\s*$/;
function hm(e, t) {
  if (
    e !== "__proto__" &&
    !(e === "constructor" && t && typeof t == "object" && "prototype" in t)
  )
    return t;
}
function dm(e, t = {}) {
  if (typeof e != "string") return e;
  const s = e.toLowerCase().trim();
  if (s === "true") return !0;
  if (s === "false") return !1;
  if (s === "null") return null;
  if (s === "nan") return Number.NaN;
  if (s === "infinity") return Number.POSITIVE_INFINITY;
  if (s !== "undefined") {
    if (!mm.test(e)) {
      if (t.strict) throw new SyntaxError("Invalid JSON");
      return e;
    }
    try {
      return om.test(e) || um.test(e) ? JSON.parse(e, hm) : JSON.parse(e);
    } catch (a) {
      if (t.strict) throw a;
      return e;
    }
  }
}
const gm = /#/g,
  pm = /&/g,
  fm = /=/g,
  Br = /\+/g,
  bm = /%5e/gi,
  _m = /%60/gi,
  ym = /%7c/gi,
  vm = /%20/gi;
function wm(e) {
  return encodeURI("" + e).replace(ym, "|");
}
function pi(e) {
  return wm(typeof e == "string" ? e : JSON.stringify(e))
    .replace(Br, "%2B")
    .replace(vm, "+")
    .replace(gm, "%23")
    .replace(pm, "%26")
    .replace(_m, "`")
    .replace(bm, "^");
}
function Ya(e) {
  return pi(e).replace(fm, "%3D");
}
function Qr(e = "") {
  try {
    return decodeURIComponent("" + e);
  } catch {
    return "" + e;
  }
}
function jm(e) {
  return Qr(e.replace(Br, " "));
}
function Vr(e = "") {
  const t = {};
  e[0] === "?" && (e = e.slice(1));
  for (const s of e.split("&")) {
    const a = s.match(/([^=]+)=?(.*)/) || [];
    if (a.length < 2) continue;
    const i = Qr(a[1]);
    if (i === "__proto__" || i === "constructor") continue;
    const n = jm(a[2] || "");
    typeof t[i] < "u"
      ? Array.isArray(t[i])
        ? t[i].push(n)
        : (t[i] = [t[i], n])
      : (t[i] = n);
  }
  return t;
}
function xm(e, t) {
  return (
    (typeof t == "number" || typeof t == "boolean") && (t = String(t)),
    t
      ? Array.isArray(t)
        ? t.map(s => `${Ya(e)}=${pi(s)}`).join("&")
        : `${Ya(e)}=${pi(t)}`
      : Ya(e)
  );
}
function qm(e) {
  return Object.keys(e)
    .filter(t => e[t] !== void 0)
    .map(t => xm(t, e[t]))
    .join("&");
}
const km = /^\w{2,}:([/\\]{1,2})/,
  Pm = /^\w{2,}:([/\\]{2})?/,
  Em = /^([/\\]\s*){2,}[^/\\]/;
function os(e, t = {}) {
  return (
    typeof t == "boolean" && (t = { acceptRelative: t }),
    t.strict ? km.test(e) : Pm.test(e) || (t.acceptRelative ? Em.test(e) : !1)
  );
}
const Am = /\/$|\/\?/;
function fi(e = "", t = !1) {
  return t ? Am.test(e) : e.endsWith("/");
}
function tn(e = "", t = !1) {
  if (!t) return (fi(e) ? e.slice(0, -1) : e) || "/";
  if (!fi(e, !0)) return e || "/";
  const [s, ...a] = e.split("?");
  return (s.slice(0, -1) || "/") + (a.length > 0 ? `?${a.join("?")}` : "");
}
function Wr(e = "", t = !1) {
  if (!t) return e.endsWith("/") ? e : e + "/";
  if (fi(e, !0)) return e || "/";
  const [s, ...a] = e.split("?");
  return s + "/" + (a.length > 0 ? `?${a.join("?")}` : "");
}
function Cm(e = "") {
  return e.startsWith("/");
}
function Sm(e = "") {
  return (Cm(e) ? e.slice(1) : e) || "/";
}
function Im(e, t) {
  if (Yr(t) || os(e)) return e;
  const s = tn(t);
  return e.startsWith(s) ? e : zs(s, e);
}
function Kn(e, t) {
  if (Yr(t)) return e;
  const s = tn(t);
  if (!e.startsWith(s)) return e;
  const a = e.slice(s.length);
  return a[0] === "/" ? a : "/" + a;
}
function Tm(e, t) {
  const s = Bs(e),
    a = { ...Vr(s.search), ...t };
  return (s.search = qm(a)), Mm(s);
}
function Yr(e) {
  return !e || e === "/";
}
function Rm(e) {
  return e && e !== "/";
}
function zs(e, ...t) {
  let s = e || "";
  for (const a of t.filter(i => Rm(i))) s = s ? Wr(s) + Sm(a) : a;
  return s;
}
function Bs(e = "", t) {
  if (!os(e, { acceptRelative: !0 })) return t ? Bs(t + e) : Jn(e);
  const [s = "", a, i = ""] = (
      e.replace(/\\/g, "/").match(/([^/:]+:)?\/\/([^/@]+@)?(.*)/) || []
    ).splice(1),
    [n = "", l = ""] = (i.match(/([^#/?]*)(.*)?/) || []).splice(1),
    { pathname: c, search: r, hash: o } = Jn(l.replace(/\/(?=[A-Za-z]:)/, ""));
  return {
    protocol: s,
    auth: a ? a.slice(0, Math.max(0, a.length - 1)) : "",
    host: n,
    pathname: c,
    search: r,
    hash: o,
  };
}
function Jn(e = "") {
  const [t = "", s = "", a = ""] = (
    e.match(/([^#?]*)(\?[^#]*)?(#.*)?/) || []
  ).splice(1);
  return { pathname: t, search: s, hash: a };
}
function Mm(e) {
  const t =
    e.pathname +
    (e.search ? (e.search.startsWith("?") ? "" : "?") + e.search : "") +
    e.hash;
  return e.protocol
    ? e.protocol + "//" + (e.auth ? e.auth + "@" : "") + e.host + t
    : t;
}
class Nm extends Error {
  constructor() {
    super(...arguments), (this.name = "FetchError");
  }
}
function Om(e, t, s) {
  let a = "";
  t && (a = t.message),
    e && s
      ? (a = `${a} (${s.status} ${s.statusText} (${e.toString()}))`)
      : e && (a = `${a} (${e.toString()})`);
  const i = new Nm(a);
  return (
    Object.defineProperty(i, "request", {
      get() {
        return e;
      },
    }),
    Object.defineProperty(i, "response", {
      get() {
        return s;
      },
    }),
    Object.defineProperty(i, "data", {
      get() {
        return s && s._data;
      },
    }),
    Object.defineProperty(i, "status", {
      get() {
        return s && s.status;
      },
    }),
    Object.defineProperty(i, "statusText", {
      get() {
        return s && s.statusText;
      },
    }),
    Object.defineProperty(i, "statusCode", {
      get() {
        return s && s.status;
      },
    }),
    Object.defineProperty(i, "statusMessage", {
      get() {
        return s && s.statusText;
      },
    }),
    i
  );
}
const Um = new Set(Object.freeze(["PATCH", "POST", "PUT", "DELETE"]));
function Zn(e = "GET") {
  return Um.has(e.toUpperCase());
}
function Lm(e) {
  if (e === void 0) return !1;
  const t = typeof e;
  return t === "string" || t === "number" || t === "boolean" || t === null
    ? !0
    : t !== "object"
      ? !1
      : Array.isArray(e)
        ? !0
        : (e.constructor && e.constructor.name === "Object") ||
          typeof e.toJSON == "function";
}
const Hm = new Set([
    "image/svg",
    "application/xml",
    "application/xhtml",
    "application/html",
  ]),
  Fm = /^application\/(?:[\w!#$%&*.^`~-]*\+)?json(;.+)?$/i;
function Dm(e = "") {
  if (!e) return "json";
  const t = e.split(";").shift() || "";
  return Fm.test(t)
    ? "json"
    : Hm.has(t) || t.startsWith("text/")
      ? "text"
      : "blob";
}
const zm = new Set([408, 409, 425, 429, 500, 502, 503, 504]);
function $r(e) {
  const { fetch: t, Headers: s } = e;
  function a(l) {
    const c = (l.error && l.error.name === "AbortError") || !1;
    if (l.options.retry !== !1 && !c) {
      let o;
      typeof l.options.retry == "number"
        ? (o = l.options.retry)
        : (o = Zn(l.options.method) ? 0 : 1);
      const u = (l.response && l.response.status) || 500;
      if (o > 0 && zm.has(u))
        return i(l.request, { ...l.options, retry: o - 1 });
    }
    const r = Om(l.request, l.error, l.response);
    throw (Error.captureStackTrace && Error.captureStackTrace(r, i), r);
  }
  const i = async function (c, r = {}) {
      const o = {
        request: c,
        options: { ...e.defaults, ...r },
        response: void 0,
        error: void 0,
      };
      o.options.onRequest && (await o.options.onRequest(o)),
        typeof o.request == "string" &&
          (o.options.baseURL && (o.request = Im(o.request, o.options.baseURL)),
          (o.options.query || o.options.params) &&
            (o.request = Tm(o.request, {
              ...o.options.params,
              ...o.options.query,
            })),
          o.options.body &&
            Zn(o.options.method) &&
            Lm(o.options.body) &&
            ((o.options.body =
              typeof o.options.body == "string"
                ? o.options.body
                : JSON.stringify(o.options.body)),
            (o.options.headers = new s(o.options.headers)),
            o.options.headers.has("content-type") ||
              o.options.headers.set("content-type", "application/json"),
            o.options.headers.has("accept") ||
              o.options.headers.set("accept", "application/json"))),
        (o.response = await t(o.request, o.options).catch(
          async m => (
            (o.error = m),
            o.options.onRequestError && (await o.options.onRequestError(o)),
            a(o)
          )
        ));
      const u =
        (o.options.parseResponse ? "json" : o.options.responseType) ||
        Dm(o.response.headers.get("content-type") || "");
      if (u === "json") {
        const m = await o.response.text(),
          d = o.options.parseResponse || dm;
        o.response._data = d(m);
      } else
        u === "stream"
          ? (o.response._data = o.response.body)
          : (o.response._data = await o.response[u]());
      return (
        o.options.onResponse && (await o.options.onResponse(o)),
        o.response.status >= 400 && o.response.status < 600
          ? (o.options.onResponseError && (await o.options.onResponseError(o)),
            a(o))
          : o.response
      );
    },
    n = function (c, r) {
      return i(c, r).then(o => o._data);
    };
  return (
    (n.raw = i),
    (n.native = t),
    (n.create = (l = {}) => $r({ ...e, defaults: { ...e.defaults, ...l } })),
    n
  );
}
const Kr = (function () {
    if (typeof globalThis < "u") return globalThis;
    if (typeof self < "u") return self;
    if (typeof window < "u") return window;
    if (typeof global < "u") return global;
    throw new Error("unable to locate global object");
  })(),
  Bm =
    Kr.fetch ||
    (() =>
      Promise.reject(new Error("[ofetch] global.fetch is not supported!"))),
  Qm = Kr.Headers,
  Vm = $r({ fetch: Bm, Headers: Qm }),
  Wm = Vm,
  Ym = () => {
    var e;
    return (
      ((e = window == null ? void 0 : window.__NUXT__) == null
        ? void 0
        : e.config) || {}
    );
  },
  fa = Ym().app,
  $m = () => fa.baseURL,
  Km = () => fa.buildAssetsDir,
  Jm = (...e) => zs(Jr(), Km(), ...e),
  Jr = (...e) => {
    const t = fa.cdnURL || fa.baseURL;
    return e.length ? zs(t, ...e) : t;
  };
(globalThis.__buildAssetsURL = Jm), (globalThis.__publicAssetsURL = Jr);
function bi(e, t = {}, s) {
  for (const a in e) {
    const i = e[a],
      n = s ? `${s}:${a}` : a;
    typeof i == "object" && i !== null
      ? bi(i, t, n)
      : typeof i == "function" && (t[n] = i);
  }
  return t;
}
const Zm = { run: e => e() },
  Gm = () => Zm,
  Zr = typeof console.createTask < "u" ? console.createTask : Gm;
function Xm(e, t) {
  const s = t.shift(),
    a = Zr(s);
  return e.reduce(
    (i, n) => i.then(() => a.run(() => n(...t))),
    Promise.resolve()
  );
}
function eh(e, t) {
  const s = t.shift(),
    a = Zr(s);
  return Promise.all(e.map(i => a.run(() => i(...t))));
}
function $a(e, t) {
  for (const s of [...e]) s(t);
}
class th {
  constructor() {
    (this._hooks = {}),
      (this._before = void 0),
      (this._after = void 0),
      (this._deprecatedMessages = void 0),
      (this._deprecatedHooks = {}),
      (this.hook = this.hook.bind(this)),
      (this.callHook = this.callHook.bind(this)),
      (this.callHookWith = this.callHookWith.bind(this));
  }
  hook(t, s, a = {}) {
    if (!t || typeof s != "function") return () => {};
    const i = t;
    let n;
    for (; this._deprecatedHooks[t]; )
      (n = this._deprecatedHooks[t]), (t = n.to);
    if (n && !a.allowDeprecated) {
      let l = n.message;
      l ||
        (l =
          `${i} hook has been deprecated` +
          (n.to ? `, please use ${n.to}` : "")),
        this._deprecatedMessages || (this._deprecatedMessages = new Set()),
        this._deprecatedMessages.has(l) ||
          (console.warn(l), this._deprecatedMessages.add(l));
    }
    if (!s.name)
      try {
        Object.defineProperty(s, "name", {
          get: () => "_" + t.replace(/\W+/g, "_") + "_hook_cb",
          configurable: !0,
        });
      } catch {}
    return (
      (this._hooks[t] = this._hooks[t] || []),
      this._hooks[t].push(s),
      () => {
        s && (this.removeHook(t, s), (s = void 0));
      }
    );
  }
  hookOnce(t, s) {
    let a,
      i = (...n) => (
        typeof a == "function" && a(), (a = void 0), (i = void 0), s(...n)
      );
    return (a = this.hook(t, i)), a;
  }
  removeHook(t, s) {
    if (this._hooks[t]) {
      const a = this._hooks[t].indexOf(s);
      a !== -1 && this._hooks[t].splice(a, 1),
        this._hooks[t].length === 0 && delete this._hooks[t];
    }
  }
  deprecateHook(t, s) {
    this._deprecatedHooks[t] = typeof s == "string" ? { to: s } : s;
    const a = this._hooks[t] || [];
    delete this._hooks[t];
    for (const i of a) this.hook(t, i);
  }
  deprecateHooks(t) {
    Object.assign(this._deprecatedHooks, t);
    for (const s in t) this.deprecateHook(s, t[s]);
  }
  addHooks(t) {
    const s = bi(t),
      a = Object.keys(s).map(i => this.hook(i, s[i]));
    return () => {
      for (const i of a.splice(0, a.length)) i();
    };
  }
  removeHooks(t) {
    const s = bi(t);
    for (const a in s) this.removeHook(a, s[a]);
  }
  removeAllHooks() {
    for (const t in this._hooks) delete this._hooks[t];
  }
  callHook(t, ...s) {
    return s.unshift(t), this.callHookWith(Xm, t, ...s);
  }
  callHookParallel(t, ...s) {
    return s.unshift(t), this.callHookWith(eh, t, ...s);
  }
  callHookWith(t, s, ...a) {
    const i =
      this._before || this._after ? { name: s, args: a, context: {} } : void 0;
    this._before && $a(this._before, i);
    const n = t(s in this._hooks ? [...this._hooks[s]] : [], a);
    return n instanceof Promise
      ? n.finally(() => {
          this._after && i && $a(this._after, i);
        })
      : (this._after && i && $a(this._after, i), n);
  }
  beforeEach(t) {
    return (
      (this._before = this._before || []),
      this._before.push(t),
      () => {
        if (this._before !== void 0) {
          const s = this._before.indexOf(t);
          s !== -1 && this._before.splice(s, 1);
        }
      }
    );
  }
  afterEach(t) {
    return (
      (this._after = this._after || []),
      this._after.push(t),
      () => {
        if (this._after !== void 0) {
          const s = this._after.indexOf(t);
          s !== -1 && this._after.splice(s, 1);
        }
      }
    );
  }
}
function Gr() {
  return new th();
}
function sh(e = {}) {
  let t,
    s = !1;
  const a = l => {
    if (t && t !== l) throw new Error("Context conflict");
  };
  let i;
  if (e.asyncContext) {
    const l = e.AsyncLocalStorage || globalThis.AsyncLocalStorage;
    l
      ? (i = new l())
      : console.warn("[unctx] `AsyncLocalStorage` is not provided.");
  }
  const n = () => {
    if (i && t === void 0) {
      const l = i.getStore();
      if (l !== void 0) return l;
    }
    return t;
  };
  return {
    use: () => {
      const l = n();
      if (l === void 0) throw new Error("Context is not available");
      return l;
    },
    tryUse: () => n(),
    set: (l, c) => {
      c || a(l), (t = l), (s = !0);
    },
    unset: () => {
      (t = void 0), (s = !1);
    },
    call: (l, c) => {
      a(l), (t = l);
      try {
        return i ? i.run(l, c) : c();
      } finally {
        s || (t = void 0);
      }
    },
    async callAsync(l, c) {
      t = l;
      const r = () => {
          t = l;
        },
        o = () => (t === l ? r : void 0);
      _i.add(o);
      try {
        const u = i ? i.run(l, c) : c();
        return s || (t = void 0), await u;
      } finally {
        _i.delete(o);
      }
    },
  };
}
function ah(e = {}) {
  const t = {};
  return {
    get(s, a = {}) {
      return t[s] || (t[s] = sh({ ...e, ...a })), t[s], t[s];
    },
  };
}
const ba =
    typeof globalThis < "u"
      ? globalThis
      : typeof self < "u"
        ? self
        : typeof global < "u"
          ? global
          : typeof window < "u"
            ? window
            : {},
  Gn = "__unctx__",
  ih = ba[Gn] || (ba[Gn] = ah()),
  nh = (e, t = {}) => ih.get(e, t),
  Xn = "__unctx_async_handlers__",
  _i = ba[Xn] || (ba[Xn] = new Set());
function _a(e) {
  const t = [];
  for (const i of _i) {
    const n = i();
    n && t.push(n);
  }
  const s = () => {
    for (const i of t) i();
  };
  let a = e();
  return (
    a &&
      typeof a == "object" &&
      "catch" in a &&
      (a = a.catch(i => {
        throw (s(), i);
      })),
    [a, s]
  );
}
const Xr = nh("nuxt-app"),
  lh = "__nuxt_plugin";
function rh(e) {
  let t = 0;
  const s = {
    provide: void 0,
    globalName: "nuxt",
    versions: {
      get nuxt() {
        return "3.5.2";
      },
      get vue() {
        return s.vueApp.version;
      },
    },
    payload: Ge({
      data: {},
      state: {},
      _errors: {},
      ...(window.__NUXT__ ?? {}),
    }),
    static: { data: {} },
    runWithContext: i => mh(s, i),
    isHydrating: !0,
    deferHydration() {
      if (!s.isHydrating) return () => {};
      t++;
      let i = !1;
      return () => {
        if (!i && ((i = !0), t--, t === 0))
          return (s.isHydrating = !1), s.callHook("app:suspense:resolve");
      };
    },
    _asyncDataPromises: {},
    _asyncData: {},
    _payloadRevivers: {},
    ...e,
  };
  (s.hooks = Gr()),
    (s.hook = s.hooks.hook),
    (s.callHook = s.hooks.callHook),
    (s.provide = (i, n) => {
      const l = "$" + i;
      ta(s, l, n), ta(s.vueApp.config.globalProperties, l, n);
    }),
    ta(s.vueApp, "$nuxt", s),
    ta(s.vueApp.config.globalProperties, "$nuxt", s);
  {
    window.addEventListener("nuxt.preloadError", n => {
      s.callHook("app:chunkError", { error: n.payload });
    });
    const i = s.hook("app:error", (...n) => {
      console.error("[nuxt] error caught during app initialization", ...n);
    });
    s.hook("app:mounted", i);
  }
  const a = Ge(s.payload.config);
  return s.provide("config", a), s;
}
async function ch(e, t) {
  if (typeof t != "function") return;
  const { provide: s } = (await e.runWithContext(() => t(e))) || {};
  if (s && typeof s == "object") for (const a in s) e.provide(a, s[a]);
}
async function oh(e, t) {
  var i;
  const s = [],
    a = [];
  for (const n of t) {
    const l = ch(e, n);
    (i = n.meta) != null && i.parallel
      ? s.push(l.catch(c => a.push(c)))
      : await l;
  }
  if ((await Promise.all(s), a.length)) throw a[0];
}
function uh(e) {
  const t = [];
  for (const s of e) {
    if (typeof s != "function") continue;
    let a = s;
    s.length > 1 && (a = i => s(i, i.provide)), t.push(a);
  }
  return (
    t.sort((s, a) => {
      var i, n;
      return (
        (((i = s.meta) == null ? void 0 : i.order) || ya.default) -
        (((n = a.meta) == null ? void 0 : n.order) || ya.default)
      );
    }),
    t
  );
}
const ya = { pre: -20, default: 0, post: 20 };
function dt(e, t) {
  var a;
  if (typeof e == "function") return dt({ setup: e }, t);
  const s = i => {
    if ((e.hooks && i.hooks.addHooks(e.hooks), e.setup)) return e.setup(i);
  };
  return (
    (s.meta = {
      name:
        (t == null ? void 0 : t.name) ||
        e.name ||
        ((a = e.setup) == null ? void 0 : a.name),
      parallel: e.parallel,
      order:
        (t == null ? void 0 : t.order) ||
        e.order ||
        ya[e.enforce || "default"] ||
        ya.default,
    }),
    (s[lh] = !0),
    s
  );
}
function mh(e, t, s) {
  const a = () => (s ? t(...s) : t());
  return Xr.set(e), e.vueApp.runWithContext(a);
}
function we() {
  var t;
  let e;
  if (
    (kr() && (e = (t = Ds()) == null ? void 0 : t.appContext.app.$nuxt),
    (e = e || Xr.tryUse()),
    !e)
  )
    throw new Error("[nuxt] instance unavailable");
  return e;
}
function sn() {
  return we().$config;
}
function ta(e, t, s) {
  Object.defineProperty(e, t, { get: () => s });
}
const hh = "modulepreload",
  dh = function (e, t) {
    return e.startsWith(".") ? new URL(e, t).href : e;
  },
  el = {},
  gh = function (t, s, a) {
    if (!s || s.length === 0) return t();
    const i = document.getElementsByTagName("link");
    return Promise.all(
      s.map(n => {
        if (((n = dh(n, a)), n in el)) return;
        el[n] = !0;
        const l = n.endsWith(".css"),
          c = l ? '[rel="stylesheet"]' : "";
        if (!!a)
          for (let u = i.length - 1; u >= 0; u--) {
            const m = i[u];
            if (m.href === n && (!l || m.rel === "stylesheet")) return;
          }
        else if (document.querySelector(`link[href="${n}"]${c}`)) return;
        const o = document.createElement("link");
        if (
          ((o.rel = l ? "stylesheet" : hh),
          l || ((o.as = "script"), (o.crossOrigin = "")),
          (o.href = n),
          document.head.appendChild(o),
          l)
        )
          return new Promise((u, m) => {
            o.addEventListener("load", u),
              o.addEventListener("error", () =>
                m(new Error(`Unable to preload CSS for ${n}`))
              );
          });
      })
    ).then(() => t());
  },
  ec = (...e) =>
    gh(...e).catch(t => {
      const s = new Event("nuxt.preloadError");
      throw ((s.payload = t), window.dispatchEvent(s), t);
    }),
  ph = -1,
  fh = -2,
  bh = -3,
  _h = -4,
  yh = -5,
  vh = -6;
function wh(e, t) {
  return jh(JSON.parse(e), t);
}
function jh(e, t) {
  if (typeof e == "number") return i(e, !0);
  if (!Array.isArray(e) || e.length === 0) throw new Error("Invalid input");
  const s = e,
    a = Array(s.length);
  function i(n, l = !1) {
    if (n === ph) return;
    if (n === bh) return NaN;
    if (n === _h) return 1 / 0;
    if (n === yh) return -1 / 0;
    if (n === vh) return -0;
    if (l) throw new Error("Invalid input");
    if (n in a) return a[n];
    const c = s[n];
    if (!c || typeof c != "object") a[n] = c;
    else if (Array.isArray(c))
      if (typeof c[0] == "string") {
        const r = c[0],
          o = t == null ? void 0 : t[r];
        if (o) return (a[n] = o(i(c[1])));
        switch (r) {
          case "Date":
            a[n] = new Date(c[1]);
            break;
          case "Set":
            const u = new Set();
            a[n] = u;
            for (let _ = 1; _ < c.length; _ += 1) u.add(i(c[_]));
            break;
          case "Map":
            const m = new Map();
            a[n] = m;
            for (let _ = 1; _ < c.length; _ += 2) m.set(i(c[_]), i(c[_ + 1]));
            break;
          case "RegExp":
            a[n] = new RegExp(c[1], c[2]);
            break;
          case "Object":
            a[n] = Object(c[1]);
            break;
          case "BigInt":
            a[n] = BigInt(c[1]);
            break;
          case "null":
            const d = Object.create(null);
            a[n] = d;
            for (let _ = 1; _ < c.length; _ += 2) d[c[_]] = i(c[_ + 1]);
            break;
          default:
            throw new Error(`Unknown type ${r}`);
        }
      } else {
        const r = new Array(c.length);
        a[n] = r;
        for (let o = 0; o < c.length; o += 1) {
          const u = c[o];
          u !== fh && (r[o] = i(u));
        }
      }
    else {
      const r = {};
      a[n] = r;
      for (const o in c) {
        const u = c[o];
        r[o] = i(u);
      }
    }
    return a[n];
  }
  return i(0);
}
function xh(e) {
  return Array.isArray(e) ? e : [e];
}
const tc = ["title", "script", "style", "noscript"],
  sc = ["base", "meta", "link", "style", "script", "noscript"],
  qh = [
    "title",
    "titleTemplate",
    "templateParams",
    "base",
    "htmlAttrs",
    "bodyAttrs",
    "meta",
    "link",
    "style",
    "script",
    "noscript",
  ],
  kh = [
    "base",
    "title",
    "titleTemplate",
    "bodyAttrs",
    "htmlAttrs",
    "templateParams",
  ],
  Ph = [
    "tagPosition",
    "tagPriority",
    "tagDuplicateStrategy",
    "innerHTML",
    "textContent",
  ];
function ac(e) {
  let t = 9;
  for (let s = 0; s < e.length; ) t = Math.imul(t ^ e.charCodeAt(s++), 9 ** 9);
  return ((t ^ (t >>> 9)) + 65536).toString(16).substring(1, 8).toLowerCase();
}
function yi(e) {
  return ac(
    `${e.tag}:${e.textContent || e.innerHTML || ""}:${Object.entries(e.props)
      .map(([t, s]) => `${t}:${String(s)}`)
      .join(",")}`
  );
}
function Eh(e) {
  let t = 9;
  for (const s of e)
    for (let a = 0; a < s.length; )
      t = Math.imul(t ^ s.charCodeAt(a++), 9 ** 9);
  return ((t ^ (t >>> 9)) + 65536).toString(16).substring(1, 8).toLowerCase();
}
function ic(e, t) {
  const { props: s, tag: a } = e;
  if (kh.includes(a)) return a;
  if (a === "link" && s.rel === "canonical") return "canonical";
  if (s.charset) return "charset";
  const i = ["id"];
  a === "meta" && i.push("name", "property", "http-equiv");
  for (const n of i)
    if (typeof s[n] < "u") {
      const l = String(s[n]);
      return t && !t(l) ? !1 : `${a}:${n}:${l}`;
    }
  return !1;
}
function tl(e, t) {
  return e == null ? t || null : typeof e == "function" ? e(t) : e;
}
function sa(e, t = !1, s) {
  const { tag: a, $el: i } = e;
  i &&
    (Object.entries(a.props).forEach(([n, l]) => {
      l = String(l);
      const c = `attr:${n}`;
      if (n === "class") {
        if (!l) return;
        for (const r of l.split(" ")) {
          const o = `${c}:${r}`;
          s && s(e, o, () => i.classList.remove(r)),
            i.classList.contains(r) || i.classList.add(r);
        }
        return;
      }
      s && !n.startsWith("data-h-") && s(e, c, () => i.removeAttribute(n)),
        (t || i.getAttribute(n) !== l) && i.setAttribute(n, l);
    }),
    tc.includes(a.tag) &&
      (a.textContent && a.textContent !== i.textContent
        ? (i.textContent = a.textContent)
        : a.innerHTML &&
          a.innerHTML !== i.innerHTML &&
          (i.innerHTML = a.innerHTML)));
}
let gs = !1;
async function Ah(e, t = {}) {
  var d, _;
  const s = { shouldRender: !0 };
  if ((await e.hooks.callHook("dom:beforeRender", s), !s.shouldRender)) return;
  const a = t.document || e.resolvedOptions.document || window.document,
    i = (await e.resolveTags()).map(c);
  if (
    e.resolvedOptions.experimentalHashHydration &&
    ((gs = gs || e._hash || !1), gs)
  ) {
    const b = Eh(i.map(w => w.tag._h));
    if (gs === b) return;
    gs = b;
  }
  const n = e._popSideEffectQueue();
  e.headEntries()
    .map(b => b._sde)
    .forEach(b => {
      Object.entries(b).forEach(([w, R]) => {
        n[w] = R;
      });
    });
  const l = (b, w, R) => {
    (w = `${b.renderId}:${w}`), b.entry && (b.entry._sde[w] = R), delete n[w];
  };
  function c(b) {
    const w = e.headEntries().find(f => f._i === b._e),
      R = {
        renderId: b._d || yi(b),
        $el: null,
        shouldRender: !0,
        tag: b,
        entry: w,
        markSideEffect: (f, p) => l(R, f, p),
      };
    return R;
  }
  const r = [],
    o = { body: [], head: [] },
    u = b => {
      (e._elMap[b.renderId] = b.$el),
        r.push(b),
        l(b, "el", () => {
          var w;
          (w = b.$el) == null || w.remove(), delete e._elMap[b.renderId];
        });
    };
  for (const b of i) {
    if ((await e.hooks.callHook("dom:beforeRenderTag", b), !b.shouldRender))
      continue;
    const { tag: w } = b;
    if (w.tag === "title") {
      (a.title = w.textContent || ""), r.push(b);
      continue;
    }
    if (w.tag === "htmlAttrs" || w.tag === "bodyAttrs") {
      (b.$el = a[w.tag === "htmlAttrs" ? "documentElement" : "body"]),
        sa(b, !1, l),
        r.push(b);
      continue;
    }
    if (
      ((b.$el = e._elMap[b.renderId]),
      !b.$el &&
        w.key &&
        (b.$el = a.querySelector(
          `${(d = w.tagPosition) != null && d.startsWith("body") ? "body" : "head"} > ${w.tag}[data-h-${w._h}]`
        )),
      b.$el)
    ) {
      b.tag._d && sa(b), u(b);
      continue;
    }
    o[
      (_ = w.tagPosition) != null && _.startsWith("body") ? "body" : "head"
    ].push(b);
  }
  const m = { bodyClose: void 0, bodyOpen: void 0, head: void 0 };
  Object.entries(o).forEach(([b, w]) => {
    var f;
    if (!w.length) return;
    const R = (f = a == null ? void 0 : a[b]) == null ? void 0 : f.children;
    if (R) {
      for (const p of [...R].reverse()) {
        const q = p.tagName.toLowerCase();
        if (!sc.includes(q)) continue;
        const v = p
            .getAttributeNames()
            .reduce((x, F) => ({ ...x, [F]: p.getAttribute(F) }), {}),
          C = { tag: q, props: v };
        p.innerHTML && (C.innerHTML = p.innerHTML);
        const O = yi(C);
        let M = w.findIndex(x => (x == null ? void 0 : x.renderId) === O);
        if (M === -1) {
          const x = ic(C);
          M = w.findIndex(
            F => (F == null ? void 0 : F.tag._d) && F.tag._d === x
          );
        }
        if (M !== -1) {
          const x = w[M];
          (x.$el = p), sa(x), u(x), delete w[M];
        }
      }
      w.forEach(p => {
        const q = p.tag.tagPosition || "head";
        (m[q] = m[q] || a.createDocumentFragment()),
          p.$el || ((p.$el = a.createElement(p.tag.tag)), sa(p, !0)),
          m[q].appendChild(p.$el),
          u(p);
      });
    }
  }),
    m.head && a.head.appendChild(m.head),
    m.bodyOpen && a.body.insertBefore(m.bodyOpen, a.body.firstChild),
    m.bodyClose && a.body.appendChild(m.bodyClose);
  for (const b of r) await e.hooks.callHook("dom:renderTag", b);
  Object.values(n).forEach(b => b());
}
let Ka = null;
async function Ch(e, t = {}) {
  function s() {
    return (Ka = null), Ah(e, t);
  }
  const a = t.delayFn || (i => setTimeout(i, 10));
  return (Ka = Ka || new Promise(i => a(() => i(s()))));
}
function Sh(e) {
  return {
    hooks: {
      "entries:updated": function (t) {
        if (
          typeof (e == null ? void 0 : e.document) > "u" &&
          typeof window > "u"
        )
          return;
        let s = e == null ? void 0 : e.delayFn;
        !s && typeof requestAnimationFrame < "u" && (s = requestAnimationFrame),
          Ch(t, {
            document: (e == null ? void 0 : e.document) || window.document,
            delayFn: s,
          });
      },
    },
  };
}
function Ih(e) {
  var t;
  return (
    ((t =
      e == null ? void 0 : e.head.querySelector('meta[name="unhead:ssr"]')) ==
    null
      ? void 0
      : t.getAttribute("content")) || !1
  );
}
const sl = { critical: 2, high: 9, low: 12, base: -1, title: 1, meta: 10 };
function al(e) {
  if (typeof e.tagPriority == "number") return e.tagPriority;
  if (e.tag === "meta") {
    if (e.props.charset) return -2;
    if (e.props["http-equiv"] === "content-security-policy") return 0;
  }
  const t = e.tagPriority || e.tag;
  return t in sl ? sl[t] : 10;
}
const Th = [
  { prefix: "before:", offset: -1 },
  { prefix: "after:", offset: 1 },
];
function Rh() {
  return {
    hooks: {
      "tags:resolve": e => {
        const t = s => {
          var a;
          return (a = e.tags.find(i => i._d === s)) == null ? void 0 : a._p;
        };
        for (const { prefix: s, offset: a } of Th)
          for (const i of e.tags.filter(
            n => typeof n.tagPriority == "string" && n.tagPriority.startsWith(s)
          )) {
            const n = t(i.tagPriority.replace(s, ""));
            typeof n < "u" && (i._p = n + a);
          }
        e.tags.sort((s, a) => s._p - a._p).sort((s, a) => al(s) - al(a));
      },
    },
  };
}
function Mh() {
  return {
    hooks: {
      "tags:resolve": e => {
        const { tags: t } = e;
        let s = t.findIndex(i => i.tag === "titleTemplate");
        const a = t.findIndex(i => i.tag === "title");
        if (a !== -1 && s !== -1) {
          const i = tl(t[s].textContent, t[a].textContent);
          i !== null ? (t[a].textContent = i || t[a].textContent) : delete t[a];
        } else if (s !== -1) {
          const i = tl(t[s].textContent);
          i !== null &&
            ((t[s].textContent = i), (t[s].tag = "title"), (s = -1));
        }
        s !== -1 && delete t[s], (e.tags = t.filter(Boolean));
      },
    },
  };
}
function Nh() {
  return {
    hooks: {
      "tag:normalise": function ({ tag: e }) {
        typeof e.props.body < "u" &&
          ((e.tagPosition = "bodyClose"), delete e.props.body);
      },
    },
  };
}
const Oh = ["link", "style", "script", "noscript"];
function Uh() {
  return {
    hooks: {
      "tag:normalise": ({ tag: e, resolvedOptions: t }) => {
        t.experimentalHashHydration === !0 && (e._h = yi(e)),
          e.key &&
            Oh.includes(e.tag) &&
            ((e._h = ac(e.key)), (e.props[`data-h-${e._h}`] = ""));
      },
    },
  };
}
const il = ["script", "link", "bodyAttrs"];
function Lh() {
  const e = (t, s) => {
    const a = {},
      i = {};
    Object.entries(s.props).forEach(([l, c]) => {
      l.startsWith("on") && typeof c == "function" ? (i[l] = c) : (a[l] = c);
    });
    let n;
    return (
      t === "dom" &&
        s.tag === "script" &&
        typeof a.src == "string" &&
        typeof i.onload < "u" &&
        ((n = a.src), delete a.src),
      { props: a, eventHandlers: i, delayedSrc: n }
    );
  };
  return {
    hooks: {
      "ssr:render": function (t) {
        t.tags = t.tags.map(
          s => (
            !il.includes(s.tag) ||
              !Object.entries(s.props).find(
                ([a, i]) => a.startsWith("on") && typeof i == "function"
              ) ||
              (s.props = e("ssr", s).props),
            s
          )
        );
      },
      "dom:beforeRenderTag": function (t) {
        if (
          !il.includes(t.tag.tag) ||
          !Object.entries(t.tag.props).find(
            ([n, l]) => n.startsWith("on") && typeof l == "function"
          )
        )
          return;
        const { props: s, eventHandlers: a, delayedSrc: i } = e("dom", t.tag);
        Object.keys(a).length &&
          ((t.tag.props = s),
          (t.tag._eventHandlers = a),
          (t.tag._delayedSrc = i));
      },
      "dom:renderTag": function (t) {
        const s = t.$el;
        if (!t.tag._eventHandlers || !s) return;
        const a = t.tag.tag === "bodyAttrs" && typeof window < "u" ? window : s;
        Object.entries(t.tag._eventHandlers).forEach(([i, n]) => {
          const l = `${t.tag._d || t.tag._p}:${i}`,
            c = i.slice(2).toLowerCase(),
            r = `data-h-${c}`;
          if ((t.markSideEffect(l, () => {}), s.hasAttribute(r))) return;
          const o = n;
          s.setAttribute(r, ""),
            a.addEventListener(c, o),
            t.entry &&
              (t.entry._sde[l] = () => {
                a.removeEventListener(c, o), s.removeAttribute(r);
              });
        }),
          t.tag._delayedSrc && s.setAttribute("src", t.tag._delayedSrc);
      },
    },
  };
}
const Hh = ["templateParams", "htmlAttrs", "bodyAttrs"];
function Fh() {
  return {
    hooks: {
      "tag:normalise": function ({ tag: e }) {
        ["hid", "vmid", "key"].forEach(a => {
          e.props[a] && ((e.key = e.props[a]), delete e.props[a]);
        });
        const s = ic(e) || (e.key ? `${e.tag}:${e.key}` : !1);
        s && (e._d = s);
      },
      "tags:resolve": function (e) {
        const t = {};
        e.tags.forEach(a => {
          const i = (a.key ? `${a.tag}:${a.key}` : a._d) || a._p,
            n = t[i];
          if (n) {
            let c = a == null ? void 0 : a.tagDuplicateStrategy;
            if ((!c && Hh.includes(a.tag) && (c = "merge"), c === "merge")) {
              const r = n.props;
              ["class", "style"].forEach(o => {
                a.props[o] &&
                  r[o] &&
                  (o === "style" && !r[o].endsWith(";") && (r[o] += ";"),
                  (a.props[o] = `${r[o]} ${a.props[o]}`));
              }),
                (t[i].props = { ...r, ...a.props });
              return;
            } else if (a._e === n._e) {
              (n._duped = n._duped || []),
                (a._d = `${n._d}:${n._duped.length + 1}`),
                n._duped.push(a);
              return;
            }
          }
          const l =
            Object.keys(a.props).length +
            (a.innerHTML ? 1 : 0) +
            (a.textContent ? 1 : 0);
          if (sc.includes(a.tag) && l === 0) {
            delete t[i];
            return;
          }
          t[i] = a;
        });
        const s = [];
        Object.values(t).forEach(a => {
          const i = a._duped;
          delete a._duped, s.push(a), i && s.push(...i);
        }),
          (e.tags = s);
      },
    },
  };
}
function aa(e, t) {
  function s(n) {
    if (["s", "pageTitle"].includes(n)) return t.pageTitle;
    let l;
    return (
      n.includes(".")
        ? (l = n.split(".").reduce((c, r) => (c && c[r]) || void 0, t))
        : (l = t[n]),
      typeof l < "u" ? l || "" : !1
    );
  }
  let a = e;
  try {
    a = decodeURI(e);
  } catch {}
  return (
    (a.match(/%(\w+\.+\w+)|%(\w+)/g) || [])
      .sort()
      .reverse()
      .forEach(n => {
        const l = s(n.slice(1));
        typeof l == "string" &&
          (e = e.replace(new RegExp(`\\${n}(\\W|$)`, "g"), `${l}$1`).trim());
      }),
    t.separator &&
      (e.endsWith(t.separator) && (e = e.slice(0, -t.separator.length).trim()),
      e.startsWith(t.separator) && (e = e.slice(t.separator.length).trim()),
      (e = e.replace(
        new RegExp(`\\${t.separator}\\s*\\${t.separator}`, "g"),
        t.separator
      ))),
    e
  );
}
function Dh() {
  return {
    hooks: {
      "tags:resolve": e => {
        var n;
        const { tags: t } = e,
          s =
            (n = t.find(l => l.tag === "title")) == null
              ? void 0
              : n.textContent,
          a = t.findIndex(l => l.tag === "templateParams"),
          i = a !== -1 ? t[a].props : {};
        i.pageTitle = i.pageTitle || s || "";
        for (const l of t)
          if (
            ["titleTemplate", "title"].includes(l.tag) &&
            typeof l.textContent == "string"
          )
            l.textContent = aa(l.textContent, i);
          else if (l.tag === "meta" && typeof l.props.content == "string")
            l.props.content = aa(l.props.content, i);
          else if (l.tag === "link" && typeof l.props.href == "string")
            l.props.href = aa(l.props.href, i);
          else if (
            l.tag === "script" &&
            ["application/json", "application/ld+json"].includes(
              l.props.type
            ) &&
            typeof l.innerHTML == "string"
          )
            try {
              l.innerHTML = JSON.stringify(JSON.parse(l.innerHTML), (c, r) =>
                typeof r == "string" ? aa(r, i) : r
              );
            } catch {}
        e.tags = t.filter(l => l.tag !== "templateParams");
      },
    },
  };
}
const zh = typeof window < "u";
let nc;
function Bh(e) {
  return (nc = e);
}
function Qh() {
  return nc;
}
async function Vh(e, t) {
  const s = { tag: e, props: {} };
  return e === "templateParams"
    ? ((s.props = t), s)
    : ["title", "titleTemplate"].includes(e)
      ? ((s.textContent = t instanceof Promise ? await t : t), s)
      : typeof t == "string"
        ? ["script", "noscript", "style"].includes(e)
          ? (e === "script" && (/^(https?:)?\/\//.test(t) || t.startsWith("/"))
              ? (s.props.src = t)
              : (s.innerHTML = t),
            s)
          : !1
        : ((s.props = await Yh(e, { ...t })),
          s.props.children && (s.props.innerHTML = s.props.children),
          delete s.props.children,
          Object.keys(s.props)
            .filter(a => Ph.includes(a))
            .forEach(a => {
              (!["innerHTML", "textContent"].includes(a) ||
                tc.includes(s.tag)) &&
                (s[a] = s.props[a]),
                delete s.props[a];
            }),
          ["innerHTML", "textContent"].forEach(a => {
            if (
              s.tag === "script" &&
              typeof s[a] == "string" &&
              ["application/ld+json", "application/json"].includes(s.props.type)
            )
              try {
                s[a] = JSON.parse(s[a]);
              } catch {
                s[a] = "";
              }
            typeof s[a] == "object" && (s[a] = JSON.stringify(s[a]));
          }),
          s.props.class && (s.props.class = Wh(s.props.class)),
          s.props.content && Array.isArray(s.props.content)
            ? s.props.content.map(a => ({
                ...s,
                props: { ...s.props, content: a },
              }))
            : s);
}
function Wh(e) {
  return (
    typeof e == "object" &&
      !Array.isArray(e) &&
      (e = Object.keys(e).filter(t => e[t])),
    (Array.isArray(e) ? e.join(" ") : e)
      .split(" ")
      .filter(t => t.trim())
      .filter(Boolean)
      .join(" ")
  );
}
async function Yh(e, t) {
  for (const s of Object.keys(t)) {
    const a = s.startsWith("data-");
    t[s] instanceof Promise && (t[s] = await t[s]),
      String(t[s]) === "true"
        ? (t[s] = a ? "true" : "")
        : String(t[s]) === "false" && (a ? (t[s] = "false") : delete t[s]);
  }
  return t;
}
const $h = 10;
async function Kh(e) {
  const t = [];
  return (
    Object.entries(e.resolvedInput)
      .filter(([s, a]) => typeof a < "u" && qh.includes(s))
      .forEach(([s, a]) => {
        const i = xh(a);
        t.push(...i.map(n => Vh(s, n)).flat());
      }),
    (await Promise.all(t))
      .flat()
      .filter(Boolean)
      .map((s, a) => ((s._e = e._i), (s._p = (e._i << $h) + a), s))
  );
}
function Jh() {
  return [Fh(), Rh(), Dh(), Mh(), Uh(), Lh(), Nh()];
}
function Zh(e = {}) {
  return [
    Sh({
      document: e == null ? void 0 : e.document,
      delayFn: e == null ? void 0 : e.domDelayFn,
    }),
  ];
}
function Gh(e = {}) {
  const t = Xh({
    ...e,
    plugins: [...Zh(e), ...((e == null ? void 0 : e.plugins) || [])],
  });
  return (
    e.experimentalHashHydration &&
      t.resolvedOptions.document &&
      (t._hash = Ih(t.resolvedOptions.document)),
    Bh(t),
    t
  );
}
function Xh(e = {}) {
  let t = [],
    s = {},
    a = 0;
  const i = Gr();
  e != null && e.hooks && i.addHooks(e.hooks),
    (e.plugins = [...Jh(), ...((e == null ? void 0 : e.plugins) || [])]),
    e.plugins.forEach(c => c.hooks && i.addHooks(c.hooks)),
    (e.document = e.document || (zh ? document : void 0));
  const n = () => i.callHook("entries:updated", l),
    l = {
      resolvedOptions: e,
      headEntries() {
        return t;
      },
      get hooks() {
        return i;
      },
      use(c) {
        c.hooks && i.addHooks(c.hooks);
      },
      push(c, r) {
        const o = { _i: a++, input: c, _sde: {} };
        return (
          r != null && r.mode && (o._m = r == null ? void 0 : r.mode),
          r != null && r.transform && (o._t = r == null ? void 0 : r.transform),
          t.push(o),
          n(),
          {
            dispose() {
              t = t.filter(u =>
                u._i !== o._i
                  ? !0
                  : ((s = { ...s, ...(u._sde || {}) }), (u._sde = {}), n(), !1)
              );
            },
            patch(u) {
              t = t.map(
                m => (m._i === o._i && ((o.input = m.input = u), n()), m)
              );
            },
          }
        );
      },
      async resolveTags() {
        const c = { tags: [], entries: [...t] };
        await i.callHook("entries:resolve", c);
        for (const r of c.entries) {
          const o = r._t || (u => u);
          if (
            ((r.resolvedInput = o(r.resolvedInput || r.input)), r.resolvedInput)
          )
            for (const u of await Kh(r)) {
              const m = {
                tag: u,
                entry: r,
                resolvedOptions: l.resolvedOptions,
              };
              await i.callHook("tag:normalise", m), c.tags.push(m.tag);
            }
        }
        return await i.callHook("tags:resolve", c), c.tags;
      },
      _popSideEffectQueue() {
        const c = { ...s };
        return (s = {}), c;
      },
      _elMap: {},
    };
  return l.hooks.callHook("init", l), l;
}
function ed(e) {
  return typeof e == "function" ? e() : pe(e);
}
function va(e, t = "") {
  if (e instanceof Promise) return e;
  const s = ed(e);
  return !e || !s
    ? s
    : Array.isArray(s)
      ? s.map(a => va(a, t))
      : typeof s == "object"
        ? Object.fromEntries(
            Object.entries(s).map(([a, i]) =>
              a === "titleTemplate" || a.startsWith("on")
                ? [a, pe(i)]
                : [a, va(i, a)]
            )
          )
        : s;
}
const td = Hr.startsWith("3"),
  sd = typeof window < "u",
  lc = "usehead";
function an() {
  return (Ds() && ve(lc)) || Qh();
}
function ad(e) {
  return {
    install(s) {
      td &&
        ((s.config.globalProperties.$unhead = e),
        (s.config.globalProperties.$head = e),
        s.provide(lc, e));
    },
  }.install;
}
function id(e = {}) {
  const t = Gh({
    ...e,
    domDelayFn: s => setTimeout(() => Lt(() => s()), 10),
    plugins: [nd(), ...((e == null ? void 0 : e.plugins) || [])],
  });
  return (t.install = ad(t)), t;
}
function nd() {
  return {
    hooks: {
      "entries:resolve": function (e) {
        for (const t of e.entries) t.resolvedInput = va(t.input);
      },
    },
  };
}
function ld(e, t = {}) {
  const s = an(),
    a = Qe(!1),
    i = Qe({});
  Bo(() => {
    i.value = a.value ? {} : va(e);
  });
  const n = s.push(i.value, t);
  return (
    Rt(i, c => {
      n.patch(c);
    }),
    Ds() &&
      (Fs(() => {
        n.dispose();
      }),
      gr(() => {
        a.value = !0;
      }),
      dr(() => {
        a.value = !1;
      })),
    n
  );
}
function rd(e, t = {}) {
  return an().push(e, t);
}
function cd(e, t = {}) {
  var a;
  const s = an();
  if (s) {
    const i = sd || !!((a = s.resolvedOptions) != null && a.document);
    return (t.mode === "server" && i) || (t.mode === "client" && !i)
      ? void 0
      : i
        ? ld(e, t)
        : rd(e, t);
  }
}
const od = {
    meta: [
      { charset: "utf-8" },
      { name: "viewport", content: "width=device-width, initial-scale=1" },
      {
        hid: "description",
        name: "description",
        content: "A news site developed with Nuxt.",
      },
    ],
    link: [],
    style: [],
    script: [],
    noscript: [],
    title: "The Daily Broadcast",
    htmlAttrs: { lang: "en" },
  },
  vi = !1,
  ud = !1,
  md = "__nuxt",
  hd = !0;
function nl(e, t = {}) {
  const s = dd(e, t),
    a = we(),
    i = (a._payloadCache = a._payloadCache || {});
  return i[s] || (i[s] = rc(s).then(n => n || (delete i[s], null))), i[s];
}
const ll = "json";
function dd(e, t = {}) {
  const s = new URL(e, "http://localhost");
  if (s.search)
    throw new Error("Payload URL cannot contain search params: " + e);
  if (s.host !== "localhost" || os(s.pathname, { acceptRelative: !0 }))
    throw new Error("Payload URL must not include hostname: " + e);
  const a = t.hash || (t.fresh ? Date.now() : "");
  return zs(
    sn().app.baseURL,
    s.pathname,
    a ? `_payload.${a}.${ll}` : `_payload.${ll}`
  );
}
async function rc(e) {
  try {
    return hd
      ? cc(await fetch(e).then(t => t.text()))
      : await ec(() => import(e), [], import.meta.url).then(
          t => t.default || t
        );
  } catch (t) {
    console.warn("[nuxt] Cannot load payload ", e, t);
  }
  return null;
}
function gd() {
  return !!we().payload.prerenderedAt;
}
let ia = null;
async function pd() {
  if (ia) return ia;
  const e = document.getElementById("__NUXT_DATA__");
  if (!e) return {};
  const t = cc(e.textContent || ""),
    s = e.dataset.src ? await rc(e.dataset.src) : void 0;
  return (ia = { ...t, ...s, ...window.__NUXT__ }), ia;
}
function cc(e) {
  return wh(e, we()._payloadRevivers);
}
function fd(e, t) {
  we()._payloadRevivers[e] = t;
}
function Ja(e) {
  return e !== null && typeof e == "object";
}
function wi(e, t, s = ".", a) {
  if (!Ja(t)) return wi(e, {}, s, a);
  const i = Object.assign({}, t);
  for (const n in e) {
    if (n === "__proto__" || n === "constructor") continue;
    const l = e[n];
    l != null &&
      ((a && a(i, n, l, s)) ||
        (Array.isArray(l) && Array.isArray(i[n])
          ? (i[n] = [...l, ...i[n]])
          : Ja(l) && Ja(i[n])
            ? (i[n] = wi(l, i[n], (s ? `${s}.` : "") + n.toString(), a))
            : (i[n] = l)));
  }
  return i;
}
function bd(e) {
  return (...t) => t.reduce((s, a) => wi(s, a, "", e), {});
}
const _d = bd();
class ji extends Error {
  constructor() {
    super(...arguments),
      (this.statusCode = 500),
      (this.fatal = !1),
      (this.unhandled = !1),
      (this.statusMessage = void 0);
  }
  toJSON() {
    const t = { message: this.message, statusCode: qi(this.statusCode, 500) };
    return (
      this.statusMessage && (t.statusMessage = oc(this.statusMessage)),
      this.data !== void 0 && (t.data = this.data),
      t
    );
  }
}
ji.__h3_error__ = !0;
function xi(e) {
  if (typeof e == "string") return new ji(e);
  if (yd(e)) return e;
  const t = new ji(
    e.message ?? e.statusMessage,
    e.cause ? { cause: e.cause } : void 0
  );
  if ("stack" in e)
    try {
      Object.defineProperty(t, "stack", {
        get() {
          return e.stack;
        },
      });
    } catch {
      try {
        t.stack = e.stack;
      } catch {}
    }
  if (
    (e.data && (t.data = e.data),
    e.statusCode
      ? (t.statusCode = qi(e.statusCode, t.statusCode))
      : e.status && (t.statusCode = qi(e.status, t.statusCode)),
    e.statusMessage
      ? (t.statusMessage = e.statusMessage)
      : e.statusText && (t.statusMessage = e.statusText),
    t.statusMessage)
  ) {
    const s = t.statusMessage;
    oc(t.statusMessage) !== s &&
      console.warn(
        "[h3] Please prefer using `message` for longer error messages instead of `statusMessage`. In the future `statusMessage` will be sanitized by default."
      );
  }
  return (
    e.fatal !== void 0 && (t.fatal = e.fatal),
    e.unhandled !== void 0 && (t.unhandled = e.unhandled),
    t
  );
}
function yd(e) {
  var t;
  return (
    ((t = e == null ? void 0 : e.constructor) == null
      ? void 0
      : t.__h3_error__) === !0
  );
}
const vd = /[^\u0009\u0020-\u007E]/g;
function oc(e = "") {
  return e.replace(vd, "");
}
function qi(e, t = 200) {
  return !e ||
    (typeof e == "string" && (e = Number.parseInt(e, 10)), e < 100 || e > 999)
    ? t
    : e;
}
function wd(...e) {
  const t = typeof e[e.length - 1] == "string" ? e.pop() : void 0;
  typeof e[0] != "string" && e.unshift(t);
  const [s, a] = e;
  if (!s || typeof s != "string")
    throw new TypeError("[nuxt] [useState] key must be a string: " + s);
  if (a !== void 0 && typeof a != "function")
    throw new Error("[nuxt] [useState] init must be a function: " + a);
  const i = "$s" + s,
    n = we(),
    l = er(n.payload.state, i);
  if (l.value === void 0 && a) {
    const c = a();
    if (Ee(c)) return (n.payload.state[i] = c), c;
    l.value = c;
  }
  return l;
}
const kt = () => {
    var e;
    return (e = we()) == null ? void 0 : e.$router;
  },
  us = () => (kr() ? ve("_route", we()._route) : we()._route),
  jd = e => e,
  xd = () => {
    try {
      if (we()._processingMiddleware) return !0;
    } catch {
      return !0;
    }
    return !1;
  },
  uc = (e, t) => {
    e || (e = "/");
    const s = typeof e == "string" ? e : e.path || "/",
      a = (t == null ? void 0 : t.external) || os(s, { acceptRelative: !0 });
    if (a && !(t != null && t.external))
      throw new Error(
        "Navigating to external URL is not allowed by default. Use `navigateTo (url, { external: true })`."
      );
    if (a && Bs(s).protocol === "script:")
      throw new Error("Cannot navigate to an URL with script protocol.");
    const i = xd();
    if (!a && i) return e;
    const n = kt();
    return a
      ? (t != null && t.replace ? location.replace(s) : (location.href = s),
        Promise.resolve())
      : t != null && t.replace
        ? n.replace(e)
        : n.push(e);
  },
  Ta = () => er(we().payload, "error"),
  Qt = e => {
    const t = nn(e);
    try {
      const s = we(),
        a = Ta();
      s.hooks.callHook("app:error", t), (a.value = a.value || t);
    } catch {
      throw t;
    }
    return t;
  },
  qd = async (e = {}) => {
    const t = we(),
      s = Ta();
    t.callHook("app:error:cleared", e),
      e.redirect && (await kt().replace(e.redirect)),
      (s.value = null);
  },
  kd = e => !!(e && typeof e == "object" && "__nuxt_error" in e),
  nn = e => {
    const t = xi(e);
    return (t.__nuxt_error = !0), t;
  },
  rl = {
    NuxtError: e => nn(e),
    EmptyShallowRef: e => Ss(e === "_" ? void 0 : JSON.parse(e)),
    EmptyRef: e => Qe(e === "_" ? void 0 : JSON.parse(e)),
    ShallowRef: e => Ss(e),
    ShallowReactive: e => Wl(e),
    Ref: e => Qe(e),
    Reactive: e => Ge(e),
  },
  Pd = dt(
    {
      name: "nuxt:revive-payload:client",
      order: -30,
      async setup(e) {
        let t, s;
        for (const a in rl) fd(a, rl[a]);
        Object.assign(
          e.payload,
          (([t, s] = _a(() => e.runWithContext(pd))), (t = await t), s(), t)
        ),
          (window.__NUXT__ = e.payload);
      },
    },
    1
  ),
  Ed = dt({ name: "nuxt:global-components" }),
  Ad = dt({
    name: "nuxt:head",
    setup(e) {
      const s = id();
      s.push(od), e.vueApp.use(s);
      {
        let a = !0;
        const i = () => {
          (a = !1), s.hooks.callHook("entries:updated", s);
        };
        s.hooks.hook("dom:beforeRender", n => {
          n.shouldRender = !a;
        }),
          e.hooks.hook("page:start", () => {
            a = !0;
          }),
          e.hooks.hook("page:finish", i),
          e.hooks.hook("app:suspense:resolve", i);
      }
    },
  });
/*!
 * vue-router v4.2.2
 * (c) 2023 Eduardo San Martin Morote
 * @license MIT
 */ const Bt = typeof window < "u";
function Cd(e) {
  return e.__esModule || e[Symbol.toStringTag] === "Module";
}
const me = Object.assign;
function Za(e, t) {
  const s = {};
  for (const a in t) {
    const i = t[a];
    s[a] = et(i) ? i.map(e) : e(i);
  }
  return s;
}
const ks = () => {},
  et = Array.isArray,
  Sd = /\/$/,
  Id = e => e.replace(Sd, "");
function Ga(e, t, s = "/") {
  let a,
    i = {},
    n = "",
    l = "";
  const c = t.indexOf("#");
  let r = t.indexOf("?");
  return (
    c < r && c >= 0 && (r = -1),
    r > -1 &&
      ((a = t.slice(0, r)),
      (n = t.slice(r + 1, c > -1 ? c : t.length)),
      (i = e(n))),
    c > -1 && ((a = a || t.slice(0, c)), (l = t.slice(c, t.length))),
    (a = Nd(a ?? t, s)),
    { fullPath: a + (n && "?") + n + l, path: a, query: i, hash: l }
  );
}
function Td(e, t) {
  const s = t.query ? e(t.query) : "";
  return t.path + (s && "?") + s + (t.hash || "");
}
function cl(e, t) {
  return !t || !e.toLowerCase().startsWith(t.toLowerCase())
    ? e
    : e.slice(t.length) || "/";
}
function Rd(e, t, s) {
  const a = t.matched.length - 1,
    i = s.matched.length - 1;
  return (
    a > -1 &&
    a === i &&
    ts(t.matched[a], s.matched[i]) &&
    mc(t.params, s.params) &&
    e(t.query) === e(s.query) &&
    t.hash === s.hash
  );
}
function ts(e, t) {
  return (e.aliasOf || e) === (t.aliasOf || t);
}
function mc(e, t) {
  if (Object.keys(e).length !== Object.keys(t).length) return !1;
  for (const s in e) if (!Md(e[s], t[s])) return !1;
  return !0;
}
function Md(e, t) {
  return et(e) ? ol(e, t) : et(t) ? ol(t, e) : e === t;
}
function ol(e, t) {
  return et(t)
    ? e.length === t.length && e.every((s, a) => s === t[a])
    : e.length === 1 && e[0] === t;
}
function Nd(e, t) {
  if (e.startsWith("/")) return e;
  if (!e) return t;
  const s = t.split("/"),
    a = e.split("/"),
    i = a[a.length - 1];
  (i === ".." || i === ".") && a.push("");
  let n = s.length - 1,
    l,
    c;
  for (l = 0; l < a.length; l++)
    if (((c = a[l]), c !== "."))
      if (c === "..") n > 1 && n--;
      else break;
  return (
    s.slice(0, n).join("/") +
    "/" +
    a.slice(l - (l === a.length ? 1 : 0)).join("/")
  );
}
var Ns;
(function (e) {
  (e.pop = "pop"), (e.push = "push");
})(Ns || (Ns = {}));
var Ps;
(function (e) {
  (e.back = "back"), (e.forward = "forward"), (e.unknown = "");
})(Ps || (Ps = {}));
function Od(e) {
  if (!e)
    if (Bt) {
      const t = document.querySelector("base");
      (e = (t && t.getAttribute("href")) || "/"),
        (e = e.replace(/^\w+:\/\/[^\/]+/, ""));
    } else e = "/";
  return e[0] !== "/" && e[0] !== "#" && (e = "/" + e), Id(e);
}
const Ud = /^[^#]+#/;
function Ld(e, t) {
  return e.replace(Ud, "#") + t;
}
function Hd(e, t) {
  const s = document.documentElement.getBoundingClientRect(),
    a = e.getBoundingClientRect();
  return {
    behavior: t.behavior,
    left: a.left - s.left - (t.left || 0),
    top: a.top - s.top - (t.top || 0),
  };
}
const Ra = () => ({ left: window.pageXOffset, top: window.pageYOffset });
function Fd(e) {
  let t;
  if ("el" in e) {
    const s = e.el,
      a = typeof s == "string" && s.startsWith("#"),
      i =
        typeof s == "string"
          ? a
            ? document.getElementById(s.slice(1))
            : document.querySelector(s)
          : s;
    if (!i) return;
    t = Hd(i, e);
  } else t = e;
  "scrollBehavior" in document.documentElement.style
    ? window.scrollTo(t)
    : window.scrollTo(
        t.left != null ? t.left : window.pageXOffset,
        t.top != null ? t.top : window.pageYOffset
      );
}
function ul(e, t) {
  return (history.state ? history.state.position - t : -1) + e;
}
const ki = new Map();
function Dd(e, t) {
  ki.set(e, t);
}
function zd(e) {
  const t = ki.get(e);
  return ki.delete(e), t;
}
let Bd = () => location.protocol + "//" + location.host;
function hc(e, t) {
  const { pathname: s, search: a, hash: i } = t,
    n = e.indexOf("#");
  if (n > -1) {
    let c = i.includes(e.slice(n)) ? e.slice(n).length : 1,
      r = i.slice(c);
    return r[0] !== "/" && (r = "/" + r), cl(r, "");
  }
  return cl(s, e) + a + i;
}
function Qd(e, t, s, a) {
  let i = [],
    n = [],
    l = null;
  const c = ({ state: d }) => {
    const _ = hc(e, location),
      b = s.value,
      w = t.value;
    let R = 0;
    if (d) {
      if (((s.value = _), (t.value = d), l && l === b)) {
        l = null;
        return;
      }
      R = w ? d.position - w.position : 0;
    } else a(_);
    i.forEach(f => {
      f(s.value, b, {
        delta: R,
        type: Ns.pop,
        direction: R ? (R > 0 ? Ps.forward : Ps.back) : Ps.unknown,
      });
    });
  };
  function r() {
    l = s.value;
  }
  function o(d) {
    i.push(d);
    const _ = () => {
      const b = i.indexOf(d);
      b > -1 && i.splice(b, 1);
    };
    return n.push(_), _;
  }
  function u() {
    const { history: d } = window;
    d.state && d.replaceState(me({}, d.state, { scroll: Ra() }), "");
  }
  function m() {
    for (const d of n) d();
    (n = []),
      window.removeEventListener("popstate", c),
      window.removeEventListener("beforeunload", u);
  }
  return (
    window.addEventListener("popstate", c),
    window.addEventListener("beforeunload", u, { passive: !0 }),
    { pauseListeners: r, listen: o, destroy: m }
  );
}
function ml(e, t, s, a = !1, i = !1) {
  return {
    back: e,
    current: t,
    forward: s,
    replaced: a,
    position: window.history.length,
    scroll: i ? Ra() : null,
  };
}
function Vd(e) {
  const { history: t, location: s } = window,
    a = { value: hc(e, s) },
    i = { value: t.state };
  i.value ||
    n(
      a.value,
      {
        back: null,
        current: a.value,
        forward: null,
        position: t.length - 1,
        replaced: !0,
        scroll: null,
      },
      !0
    );
  function n(r, o, u) {
    const m = e.indexOf("#"),
      d =
        m > -1
          ? (s.host && document.querySelector("base") ? e : e.slice(m)) + r
          : Bd() + e + r;
    try {
      t[u ? "replaceState" : "pushState"](o, "", d), (i.value = o);
    } catch (_) {
      console.error(_), s[u ? "replace" : "assign"](d);
    }
  }
  function l(r, o) {
    const u = me({}, t.state, ml(i.value.back, r, i.value.forward, !0), o, {
      position: i.value.position,
    });
    n(r, u, !0), (a.value = r);
  }
  function c(r, o) {
    const u = me({}, i.value, t.state, { forward: r, scroll: Ra() });
    n(u.current, u, !0);
    const m = me({}, ml(a.value, r, null), { position: u.position + 1 }, o);
    n(r, m, !1), (a.value = r);
  }
  return { location: a, state: i, push: c, replace: l };
}
function dc(e) {
  e = Od(e);
  const t = Vd(e),
    s = Qd(e, t.state, t.location, t.replace);
  function a(n, l = !0) {
    l || s.pauseListeners(), history.go(n);
  }
  const i = me(
    { location: "", base: e, go: a, createHref: Ld.bind(null, e) },
    t,
    s
  );
  return (
    Object.defineProperty(i, "location", {
      enumerable: !0,
      get: () => t.location.value,
    }),
    Object.defineProperty(i, "state", {
      enumerable: !0,
      get: () => t.state.value,
    }),
    i
  );
}
function Wd(e) {
  return (
    (e = location.host ? e || location.pathname + location.search : ""),
    e.includes("#") || (e += "#"),
    dc(e)
  );
}
function Yd(e) {
  return typeof e == "string" || (e && typeof e == "object");
}
function gc(e) {
  return typeof e == "string" || typeof e == "symbol";
}
const at = {
    path: "/",
    name: void 0,
    params: {},
    query: {},
    hash: "",
    fullPath: "/",
    matched: [],
    meta: {},
    redirectedFrom: void 0,
  },
  pc = Symbol("");
var hl;
(function (e) {
  (e[(e.aborted = 4)] = "aborted"),
    (e[(e.cancelled = 8)] = "cancelled"),
    (e[(e.duplicated = 16)] = "duplicated");
})(hl || (hl = {}));
function ss(e, t) {
  return me(new Error(), { type: e, [pc]: !0 }, t);
}
function ct(e, t) {
  return e instanceof Error && pc in e && (t == null || !!(e.type & t));
}
const dl = "[^/]+?",
  $d = { sensitive: !1, strict: !1, start: !0, end: !0 },
  Kd = /[.+*?^${}()[\]/\\]/g;
function Jd(e, t) {
  const s = me({}, $d, t),
    a = [];
  let i = s.start ? "^" : "";
  const n = [];
  for (const o of e) {
    const u = o.length ? [] : [90];
    s.strict && !o.length && (i += "/");
    for (let m = 0; m < o.length; m++) {
      const d = o[m];
      let _ = 40 + (s.sensitive ? 0.25 : 0);
      if (d.type === 0)
        m || (i += "/"), (i += d.value.replace(Kd, "\\$&")), (_ += 40);
      else if (d.type === 1) {
        const { value: b, repeatable: w, optional: R, regexp: f } = d;
        n.push({ name: b, repeatable: w, optional: R });
        const p = f || dl;
        if (p !== dl) {
          _ += 10;
          try {
            new RegExp(`(${p})`);
          } catch (v) {
            throw new Error(
              `Invalid custom RegExp for param "${b}" (${p}): ` + v.message
            );
          }
        }
        let q = w ? `((?:${p})(?:/(?:${p}))*)` : `(${p})`;
        m || (q = R && o.length < 2 ? `(?:/${q})` : "/" + q),
          R && (q += "?"),
          (i += q),
          (_ += 20),
          R && (_ += -8),
          w && (_ += -20),
          p === ".*" && (_ += -50);
      }
      u.push(_);
    }
    a.push(u);
  }
  if (s.strict && s.end) {
    const o = a.length - 1;
    a[o][a[o].length - 1] += 0.7000000000000001;
  }
  s.strict || (i += "/?"), s.end ? (i += "$") : s.strict && (i += "(?:/|$)");
  const l = new RegExp(i, s.sensitive ? "" : "i");
  function c(o) {
    const u = o.match(l),
      m = {};
    if (!u) return null;
    for (let d = 1; d < u.length; d++) {
      const _ = u[d] || "",
        b = n[d - 1];
      m[b.name] = _ && b.repeatable ? _.split("/") : _;
    }
    return m;
  }
  function r(o) {
    let u = "",
      m = !1;
    for (const d of e) {
      (!m || !u.endsWith("/")) && (u += "/"), (m = !1);
      for (const _ of d)
        if (_.type === 0) u += _.value;
        else if (_.type === 1) {
          const { value: b, repeatable: w, optional: R } = _,
            f = b in o ? o[b] : "";
          if (et(f) && !w)
            throw new Error(
              `Provided param "${b}" is an array but it is not repeatable (* or + modifiers)`
            );
          const p = et(f) ? f.join("/") : f;
          if (!p)
            if (R)
              d.length < 2 &&
                (u.endsWith("/") ? (u = u.slice(0, -1)) : (m = !0));
            else throw new Error(`Missing required param "${b}"`);
          u += p;
        }
    }
    return u || "/";
  }
  return { re: l, score: a, keys: n, parse: c, stringify: r };
}
function Zd(e, t) {
  let s = 0;
  for (; s < e.length && s < t.length; ) {
    const a = t[s] - e[s];
    if (a) return a;
    s++;
  }
  return e.length < t.length
    ? e.length === 1 && e[0] === 40 + 40
      ? -1
      : 1
    : e.length > t.length
      ? t.length === 1 && t[0] === 40 + 40
        ? 1
        : -1
      : 0;
}
function Gd(e, t) {
  let s = 0;
  const a = e.score,
    i = t.score;
  for (; s < a.length && s < i.length; ) {
    const n = Zd(a[s], i[s]);
    if (n) return n;
    s++;
  }
  if (Math.abs(i.length - a.length) === 1) {
    if (gl(a)) return 1;
    if (gl(i)) return -1;
  }
  return i.length - a.length;
}
function gl(e) {
  const t = e[e.length - 1];
  return e.length > 0 && t[t.length - 1] < 0;
}
const Xd = { type: 0, value: "" },
  eg = /[a-zA-Z0-9_]/;
function tg(e) {
  if (!e) return [[]];
  if (e === "/") return [[Xd]];
  if (!e.startsWith("/")) throw new Error(`Invalid path "${e}"`);
  function t(_) {
    throw new Error(`ERR (${s})/"${o}": ${_}`);
  }
  let s = 0,
    a = s;
  const i = [];
  let n;
  function l() {
    n && i.push(n), (n = []);
  }
  let c = 0,
    r,
    o = "",
    u = "";
  function m() {
    o &&
      (s === 0
        ? n.push({ type: 0, value: o })
        : s === 1 || s === 2 || s === 3
          ? (n.length > 1 &&
              (r === "*" || r === "+") &&
              t(
                `A repeatable param (${o}) must be alone in its segment. eg: '/:ids+.`
              ),
            n.push({
              type: 1,
              value: o,
              regexp: u,
              repeatable: r === "*" || r === "+",
              optional: r === "*" || r === "?",
            }))
          : t("Invalid state to consume buffer"),
      (o = ""));
  }
  function d() {
    o += r;
  }
  for (; c < e.length; ) {
    if (((r = e[c++]), r === "\\" && s !== 2)) {
      (a = s), (s = 4);
      continue;
    }
    switch (s) {
      case 0:
        r === "/" ? (o && m(), l()) : r === ":" ? (m(), (s = 1)) : d();
        break;
      case 4:
        d(), (s = a);
        break;
      case 1:
        r === "("
          ? (s = 2)
          : eg.test(r)
            ? d()
            : (m(), (s = 0), r !== "*" && r !== "?" && r !== "+" && c--);
        break;
      case 2:
        r === ")"
          ? u[u.length - 1] == "\\"
            ? (u = u.slice(0, -1) + r)
            : (s = 3)
          : (u += r);
        break;
      case 3:
        m(), (s = 0), r !== "*" && r !== "?" && r !== "+" && c--, (u = "");
        break;
      default:
        t("Unknown state");
        break;
    }
  }
  return s === 2 && t(`Unfinished custom RegExp for param "${o}"`), m(), l(), i;
}
function sg(e, t, s) {
  const a = Jd(tg(e.path), s),
    i = me(a, { record: e, parent: t, children: [], alias: [] });
  return t && !i.record.aliasOf == !t.record.aliasOf && t.children.push(i), i;
}
function ag(e, t) {
  const s = [],
    a = new Map();
  t = bl({ strict: !1, end: !0, sensitive: !1 }, t);
  function i(u) {
    return a.get(u);
  }
  function n(u, m, d) {
    const _ = !d,
      b = ig(u);
    b.aliasOf = d && d.record;
    const w = bl(t, u),
      R = [b];
    if ("alias" in u) {
      const q = typeof u.alias == "string" ? [u.alias] : u.alias;
      for (const v of q)
        R.push(
          me({}, b, {
            components: d ? d.record.components : b.components,
            path: v,
            aliasOf: d ? d.record : b,
          })
        );
    }
    let f, p;
    for (const q of R) {
      const { path: v } = q;
      if (m && v[0] !== "/") {
        const C = m.record.path,
          O = C[C.length - 1] === "/" ? "" : "/";
        q.path = m.record.path + (v && O + v);
      }
      if (
        ((f = sg(q, m, w)),
        d
          ? d.alias.push(f)
          : ((p = p || f),
            p !== f && p.alias.push(f),
            _ && u.name && !fl(f) && l(u.name)),
        b.children)
      ) {
        const C = b.children;
        for (let O = 0; O < C.length; O++) n(C[O], f, d && d.children[O]);
      }
      (d = d || f),
        ((f.record.components && Object.keys(f.record.components).length) ||
          f.record.name ||
          f.record.redirect) &&
          r(f);
    }
    return p
      ? () => {
          l(p);
        }
      : ks;
  }
  function l(u) {
    if (gc(u)) {
      const m = a.get(u);
      m &&
        (a.delete(u),
        s.splice(s.indexOf(m), 1),
        m.children.forEach(l),
        m.alias.forEach(l));
    } else {
      const m = s.indexOf(u);
      m > -1 &&
        (s.splice(m, 1),
        u.record.name && a.delete(u.record.name),
        u.children.forEach(l),
        u.alias.forEach(l));
    }
  }
  function c() {
    return s;
  }
  function r(u) {
    let m = 0;
    for (
      ;
      m < s.length &&
      Gd(u, s[m]) >= 0 &&
      (u.record.path !== s[m].record.path || !fc(u, s[m]));

    )
      m++;
    s.splice(m, 0, u), u.record.name && !fl(u) && a.set(u.record.name, u);
  }
  function o(u, m) {
    let d,
      _ = {},
      b,
      w;
    if ("name" in u && u.name) {
      if (((d = a.get(u.name)), !d)) throw ss(1, { location: u });
      (w = d.record.name),
        (_ = me(
          pl(
            m.params,
            d.keys.filter(p => !p.optional).map(p => p.name)
          ),
          u.params &&
            pl(
              u.params,
              d.keys.map(p => p.name)
            )
        )),
        (b = d.stringify(_));
    } else if ("path" in u)
      (b = u.path),
        (d = s.find(p => p.re.test(b))),
        d && ((_ = d.parse(b)), (w = d.record.name));
    else {
      if (((d = m.name ? a.get(m.name) : s.find(p => p.re.test(m.path))), !d))
        throw ss(1, { location: u, currentLocation: m });
      (w = d.record.name),
        (_ = me({}, m.params, u.params)),
        (b = d.stringify(_));
    }
    const R = [];
    let f = d;
    for (; f; ) R.unshift(f.record), (f = f.parent);
    return { name: w, path: b, params: _, matched: R, meta: lg(R) };
  }
  return (
    e.forEach(u => n(u)),
    {
      addRoute: n,
      resolve: o,
      removeRoute: l,
      getRoutes: c,
      getRecordMatcher: i,
    }
  );
}
function pl(e, t) {
  const s = {};
  for (const a of t) a in e && (s[a] = e[a]);
  return s;
}
function ig(e) {
  return {
    path: e.path,
    redirect: e.redirect,
    name: e.name,
    meta: e.meta || {},
    aliasOf: void 0,
    beforeEnter: e.beforeEnter,
    props: ng(e),
    children: e.children || [],
    instances: {},
    leaveGuards: new Set(),
    updateGuards: new Set(),
    enterCallbacks: {},
    components:
      "components" in e
        ? e.components || null
        : e.component && { default: e.component },
  };
}
function ng(e) {
  const t = {},
    s = e.props || !1;
  if ("component" in e) t.default = s;
  else for (const a in e.components) t[a] = typeof s == "boolean" ? s : s[a];
  return t;
}
function fl(e) {
  for (; e; ) {
    if (e.record.aliasOf) return !0;
    e = e.parent;
  }
  return !1;
}
function lg(e) {
  return e.reduce((t, s) => me(t, s.meta), {});
}
function bl(e, t) {
  const s = {};
  for (const a in e) s[a] = a in t ? t[a] : e[a];
  return s;
}
function fc(e, t) {
  return t.children.some(s => s === e || fc(e, s));
}
const bc = /#/g,
  rg = /&/g,
  cg = /\//g,
  og = /=/g,
  ug = /\?/g,
  _c = /\+/g,
  mg = /%5B/g,
  hg = /%5D/g,
  yc = /%5E/g,
  dg = /%60/g,
  vc = /%7B/g,
  gg = /%7C/g,
  wc = /%7D/g,
  pg = /%20/g;
function ln(e) {
  return encodeURI("" + e)
    .replace(gg, "|")
    .replace(mg, "[")
    .replace(hg, "]");
}
function fg(e) {
  return ln(e).replace(vc, "{").replace(wc, "}").replace(yc, "^");
}
function Pi(e) {
  return ln(e)
    .replace(_c, "%2B")
    .replace(pg, "+")
    .replace(bc, "%23")
    .replace(rg, "%26")
    .replace(dg, "`")
    .replace(vc, "{")
    .replace(wc, "}")
    .replace(yc, "^");
}
function bg(e) {
  return Pi(e).replace(og, "%3D");
}
function _g(e) {
  return ln(e).replace(bc, "%23").replace(ug, "%3F");
}
function yg(e) {
  return e == null ? "" : _g(e).replace(cg, "%2F");
}
function wa(e) {
  try {
    return decodeURIComponent("" + e);
  } catch {}
  return "" + e;
}
function vg(e) {
  const t = {};
  if (e === "" || e === "?") return t;
  const a = (e[0] === "?" ? e.slice(1) : e).split("&");
  for (let i = 0; i < a.length; ++i) {
    const n = a[i].replace(_c, " "),
      l = n.indexOf("="),
      c = wa(l < 0 ? n : n.slice(0, l)),
      r = l < 0 ? null : wa(n.slice(l + 1));
    if (c in t) {
      let o = t[c];
      et(o) || (o = t[c] = [o]), o.push(r);
    } else t[c] = r;
  }
  return t;
}
function _l(e) {
  let t = "";
  for (let s in e) {
    const a = e[s];
    if (((s = bg(s)), a == null)) {
      a !== void 0 && (t += (t.length ? "&" : "") + s);
      continue;
    }
    (et(a) ? a.map(n => n && Pi(n)) : [a && Pi(a)]).forEach(n => {
      n !== void 0 &&
        ((t += (t.length ? "&" : "") + s), n != null && (t += "=" + n));
    });
  }
  return t;
}
function wg(e) {
  const t = {};
  for (const s in e) {
    const a = e[s];
    a !== void 0 &&
      (t[s] = et(a)
        ? a.map(i => (i == null ? null : "" + i))
        : a == null
          ? a
          : "" + a);
  }
  return t;
}
const jg = Symbol(""),
  yl = Symbol(""),
  rn = Symbol(""),
  jc = Symbol(""),
  Ei = Symbol("");
function ps() {
  let e = [];
  function t(a) {
    return (
      e.push(a),
      () => {
        const i = e.indexOf(a);
        i > -1 && e.splice(i, 1);
      }
    );
  }
  function s() {
    e = [];
  }
  return { add: t, list: () => e, reset: s };
}
function wt(e, t, s, a, i) {
  const n = a && (a.enterCallbacks[i] = a.enterCallbacks[i] || []);
  return () =>
    new Promise((l, c) => {
      const r = m => {
          m === !1
            ? c(ss(4, { from: s, to: t }))
            : m instanceof Error
              ? c(m)
              : Yd(m)
                ? c(ss(2, { from: t, to: m }))
                : (n &&
                    a.enterCallbacks[i] === n &&
                    typeof m == "function" &&
                    n.push(m),
                  l());
        },
        o = e.call(a && a.instances[i], t, s, r);
      let u = Promise.resolve(o);
      e.length < 3 && (u = u.then(r)), u.catch(m => c(m));
    });
}
function Xa(e, t, s, a) {
  const i = [];
  for (const n of e)
    for (const l in n.components) {
      let c = n.components[l];
      if (!(t !== "beforeRouteEnter" && !n.instances[l]))
        if (xg(c)) {
          const o = (c.__vccOpts || c)[t];
          o && i.push(wt(o, s, a, n, l));
        } else {
          let r = c();
          i.push(() =>
            r.then(o => {
              if (!o)
                return Promise.reject(
                  new Error(`Couldn't resolve component "${l}" at "${n.path}"`)
                );
              const u = Cd(o) ? o.default : o;
              n.components[l] = u;
              const d = (u.__vccOpts || u)[t];
              return d && wt(d, s, a, n, l)();
            })
          );
        }
    }
  return i;
}
function xg(e) {
  return (
    typeof e == "object" ||
    "displayName" in e ||
    "props" in e ||
    "__vccOpts" in e
  );
}
function vl(e) {
  const t = ve(rn),
    s = ve(jc),
    a = Te(() => t.resolve(pe(e.to))),
    i = Te(() => {
      const { matched: r } = a.value,
        { length: o } = r,
        u = r[o - 1],
        m = s.matched;
      if (!u || !m.length) return -1;
      const d = m.findIndex(ts.bind(null, u));
      if (d > -1) return d;
      const _ = wl(r[o - 2]);
      return o > 1 && wl(u) === _ && m[m.length - 1].path !== _
        ? m.findIndex(ts.bind(null, r[o - 2]))
        : d;
    }),
    n = Te(() => i.value > -1 && Eg(s.params, a.value.params)),
    l = Te(
      () =>
        i.value > -1 &&
        i.value === s.matched.length - 1 &&
        mc(s.params, a.value.params)
    );
  function c(r = {}) {
    return Pg(r)
      ? t[pe(e.replace) ? "replace" : "push"](pe(e.to)).catch(ks)
      : Promise.resolve();
  }
  return {
    route: a,
    href: Te(() => a.value.href),
    isActive: n,
    isExactActive: l,
    navigate: c,
  };
}
const qg = rs({
    name: "RouterLink",
    compatConfig: { MODE: 3 },
    props: {
      to: { type: [String, Object], required: !0 },
      replace: Boolean,
      activeClass: String,
      exactActiveClass: String,
      custom: Boolean,
      ariaCurrentValue: { type: String, default: "page" },
    },
    useLink: vl,
    setup(e, { slots: t }) {
      const s = Ge(vl(e)),
        { options: a } = ve(rn),
        i = Te(() => ({
          [jl(e.activeClass, a.linkActiveClass, "router-link-active")]:
            s.isActive,
          [jl(
            e.exactActiveClass,
            a.linkExactActiveClass,
            "router-link-exact-active"
          )]: s.isExactActive,
        }));
      return () => {
        const n = t.default && t.default(s);
        return e.custom
          ? n
          : Ze(
              "a",
              {
                "aria-current": s.isExactActive ? e.ariaCurrentValue : null,
                href: s.href,
                onClick: s.navigate,
                class: i.value,
              },
              n
            );
      };
    },
  }),
  kg = qg;
function Pg(e) {
  if (
    !(e.metaKey || e.altKey || e.ctrlKey || e.shiftKey) &&
    !e.defaultPrevented &&
    !(e.button !== void 0 && e.button !== 0)
  ) {
    if (e.currentTarget && e.currentTarget.getAttribute) {
      const t = e.currentTarget.getAttribute("target");
      if (/\b_blank\b/i.test(t)) return;
    }
    return e.preventDefault && e.preventDefault(), !0;
  }
}
function Eg(e, t) {
  for (const s in t) {
    const a = t[s],
      i = e[s];
    if (typeof a == "string") {
      if (a !== i) return !1;
    } else if (!et(i) || i.length !== a.length || a.some((n, l) => n !== i[l]))
      return !1;
  }
  return !0;
}
function wl(e) {
  return e ? (e.aliasOf ? e.aliasOf.path : e.path) : "";
}
const jl = (e, t, s) => e ?? t ?? s,
  Ag = rs({
    name: "RouterView",
    inheritAttrs: !1,
    props: { name: { type: String, default: "default" }, route: Object },
    compatConfig: { MODE: 3 },
    setup(e, { attrs: t, slots: s }) {
      const a = ve(Ei),
        i = Te(() => e.route || a.value),
        n = ve(yl, 0),
        l = Te(() => {
          let o = pe(n);
          const { matched: u } = i.value;
          let m;
          for (; (m = u[o]) && !m.components; ) o++;
          return o;
        }),
        c = Te(() => i.value.matched[l.value]);
      Nt(
        yl,
        Te(() => l.value + 1)
      ),
        Nt(jg, c),
        Nt(Ei, i);
      const r = Qe();
      return (
        Rt(
          () => [r.value, c.value, e.name],
          ([o, u, m], [d, _, b]) => {
            u &&
              ((u.instances[m] = o),
              _ &&
                _ !== u &&
                o &&
                o === d &&
                (u.leaveGuards.size || (u.leaveGuards = _.leaveGuards),
                u.updateGuards.size || (u.updateGuards = _.updateGuards))),
              o &&
                u &&
                (!_ || !ts(u, _) || !d) &&
                (u.enterCallbacks[m] || []).forEach(w => w(o));
          },
          { flush: "post" }
        ),
        () => {
          const o = i.value,
            u = e.name,
            m = c.value,
            d = m && m.components[u];
          if (!d) return xl(s.default, { Component: d, route: o });
          const _ = m.props[u],
            b = _
              ? _ === !0
                ? o.params
                : typeof _ == "function"
                  ? _(o)
                  : _
              : null,
            R = Ze(
              d,
              me({}, b, t, {
                onVnodeUnmounted: f => {
                  f.component.isUnmounted && (m.instances[u] = null);
                },
                ref: r,
              })
            );
          return xl(s.default, { Component: R, route: o }) || R;
        }
      );
    },
  });
function xl(e, t) {
  if (!e) return null;
  const s = e(t);
  return s.length === 1 ? s[0] : s;
}
const xc = Ag;
function Cg(e) {
  const t = ag(e.routes, e),
    s = e.parseQuery || vg,
    a = e.stringifyQuery || _l,
    i = e.history,
    n = ps(),
    l = ps(),
    c = ps(),
    r = Ss(at);
  let o = at;
  Bt &&
    e.scrollBehavior &&
    "scrollRestoration" in history &&
    (history.scrollRestoration = "manual");
  const u = Za.bind(null, P => "" + P),
    m = Za.bind(null, yg),
    d = Za.bind(null, wa);
  function _(P, B) {
    let L, $;
    return (
      gc(P) ? ((L = t.getRecordMatcher(P)), ($ = B)) : ($ = P), t.addRoute($, L)
    );
  }
  function b(P) {
    const B = t.getRecordMatcher(P);
    B && t.removeRoute(B);
  }
  function w() {
    return t.getRoutes().map(P => P.record);
  }
  function R(P) {
    return !!t.getRecordMatcher(P);
  }
  function f(P, B) {
    if (((B = me({}, B || r.value)), typeof P == "string")) {
      const y = Ga(s, P, B.path),
        j = t.resolve({ path: y.path }, B),
        A = i.createHref(y.fullPath);
      return me(y, j, {
        params: d(j.params),
        hash: wa(y.hash),
        redirectedFrom: void 0,
        href: A,
      });
    }
    let L;
    if ("path" in P) L = me({}, P, { path: Ga(s, P.path, B.path).path });
    else {
      const y = me({}, P.params);
      for (const j in y) y[j] == null && delete y[j];
      (L = me({}, P, { params: m(y) })), (B.params = m(B.params));
    }
    const $ = t.resolve(L, B),
      ue = P.hash || "";
    $.params = u(d($.params));
    const h = Td(a, me({}, P, { hash: fg(ue), path: $.path })),
      g = i.createHref(h);
    return me(
      { fullPath: h, hash: ue, query: a === _l ? wg(P.query) : P.query || {} },
      $,
      { redirectedFrom: void 0, href: g }
    );
  }
  function p(P) {
    return typeof P == "string" ? Ga(s, P, r.value.path) : me({}, P);
  }
  function q(P, B) {
    if (o !== P) return ss(8, { from: B, to: P });
  }
  function v(P) {
    return M(P);
  }
  function C(P) {
    return v(me(p(P), { replace: !0 }));
  }
  function O(P) {
    const B = P.matched[P.matched.length - 1];
    if (B && B.redirect) {
      const { redirect: L } = B;
      let $ = typeof L == "function" ? L(P) : L;
      return (
        typeof $ == "string" &&
          (($ = $.includes("?") || $.includes("#") ? ($ = p($)) : { path: $ }),
          ($.params = {})),
        me(
          { query: P.query, hash: P.hash, params: "path" in $ ? {} : P.params },
          $
        )
      );
    }
  }
  function M(P, B) {
    const L = (o = f(P)),
      $ = r.value,
      ue = P.state,
      h = P.force,
      g = P.replace === !0,
      y = O(L);
    if (y)
      return M(
        me(p(y), {
          state: typeof y == "object" ? me({}, ue, y.state) : ue,
          force: h,
          replace: g,
        }),
        B || L
      );
    const j = L;
    j.redirectedFrom = B;
    let A;
    return (
      !h && Rd(a, $, L) && ((A = ss(16, { to: j, from: $ })), tt($, $, !0, !1)),
      (A ? Promise.resolve(A) : W(j, $))
        .catch(S => (ct(S) ? (ct(S, 2) ? S : gt(S)) : oe(S, j, $)))
        .then(S => {
          if (S) {
            if (ct(S, 2))
              return M(
                me({ replace: g }, p(S.to), {
                  state: typeof S.to == "object" ? me({}, ue, S.to.state) : ue,
                  force: h,
                }),
                B || j
              );
          } else S = z(j, $, !0, g, ue);
          return Z(j, $, S), S;
        })
    );
  }
  function x(P, B) {
    const L = q(P, B);
    return L ? Promise.reject(L) : Promise.resolve();
  }
  function F(P) {
    const B = Ft.values().next().value;
    return B && typeof B.runWithContext == "function"
      ? B.runWithContext(P)
      : P();
  }
  function W(P, B) {
    let L;
    const [$, ue, h] = Sg(P, B);
    L = Xa($.reverse(), "beforeRouteLeave", P, B);
    for (const y of $)
      y.leaveGuards.forEach(j => {
        L.push(wt(j, P, B));
      });
    const g = x.bind(null, P, B);
    return (
      L.push(g),
      Se(L)
        .then(() => {
          L = [];
          for (const y of n.list()) L.push(wt(y, P, B));
          return L.push(g), Se(L);
        })
        .then(() => {
          L = Xa(ue, "beforeRouteUpdate", P, B);
          for (const y of ue)
            y.updateGuards.forEach(j => {
              L.push(wt(j, P, B));
            });
          return L.push(g), Se(L);
        })
        .then(() => {
          L = [];
          for (const y of P.matched)
            if (y.beforeEnter && !B.matched.includes(y))
              if (et(y.beforeEnter))
                for (const j of y.beforeEnter) L.push(wt(j, P, B));
              else L.push(wt(y.beforeEnter, P, B));
          return L.push(g), Se(L);
        })
        .then(
          () => (
            P.matched.forEach(y => (y.enterCallbacks = {})),
            (L = Xa(h, "beforeRouteEnter", P, B)),
            L.push(g),
            Se(L)
          )
        )
        .then(() => {
          L = [];
          for (const y of l.list()) L.push(wt(y, P, B));
          return L.push(g), Se(L);
        })
        .catch(y => (ct(y, 8) ? y : Promise.reject(y)))
    );
  }
  function Z(P, B, L) {
    for (const $ of c.list()) F(() => $(P, B, L));
  }
  function z(P, B, L, $, ue) {
    const h = q(P, B);
    if (h) return h;
    const g = B === at,
      y = Bt ? history.state : {};
    L &&
      ($ || g
        ? i.replace(P.fullPath, me({ scroll: g && y && y.scroll }, ue))
        : i.push(P.fullPath, ue)),
      (r.value = P),
      tt(P, B, L, g),
      gt();
  }
  let X;
  function V() {
    X ||
      (X = i.listen((P, B, L) => {
        if (!Vs.listening) return;
        const $ = f(P),
          ue = O($);
        if (ue) {
          M(me(ue, { replace: !0 }), $).catch(ks);
          return;
        }
        o = $;
        const h = r.value;
        Bt && Dd(ul(h.fullPath, L.delta), Ra()),
          W($, h)
            .catch(g =>
              ct(g, 12)
                ? g
                : ct(g, 2)
                  ? (M(g.to, $)
                      .then(y => {
                        ct(y, 20) &&
                          !L.delta &&
                          L.type === Ns.pop &&
                          i.go(-1, !1);
                      })
                      .catch(ks),
                    Promise.reject())
                  : (L.delta && i.go(-L.delta, !1), oe(g, $, h))
            )
            .then(g => {
              (g = g || z($, h, !1)),
                g &&
                  (L.delta && !ct(g, 8)
                    ? i.go(-L.delta, !1)
                    : L.type === Ns.pop && ct(g, 20) && i.go(-1, !1)),
                Z($, h, g);
            })
            .catch(ks);
      }));
  }
  let je = ps(),
    ae = ps(),
    ce;
  function oe(P, B, L) {
    gt(P);
    const $ = ae.list();
    return (
      $.length ? $.forEach(ue => ue(P, B, L)) : console.error(P),
      Promise.reject(P)
    );
  }
  function rt() {
    return ce && r.value !== at
      ? Promise.resolve()
      : new Promise((P, B) => {
          je.add([P, B]);
        });
  }
  function gt(P) {
    return (
      ce ||
        ((ce = !P),
        V(),
        je.list().forEach(([B, L]) => (P ? L(P) : B())),
        je.reset()),
      P
    );
  }
  function tt(P, B, L, $) {
    const { scrollBehavior: ue } = e;
    if (!Bt || !ue) return Promise.resolve();
    const h =
      (!L && zd(ul(P.fullPath, 0))) ||
      (($ || !L) && history.state && history.state.scroll) ||
      null;
    return Lt()
      .then(() => ue(P, B, h))
      .then(g => g && Fd(g))
      .catch(g => oe(g, P, B));
  }
  const Ne = P => i.go(P);
  let Ht;
  const Ft = new Set(),
    Vs = {
      currentRoute: r,
      listening: !0,
      addRoute: _,
      removeRoute: b,
      hasRoute: R,
      getRoutes: w,
      resolve: f,
      options: e,
      push: v,
      replace: C,
      go: Ne,
      back: () => Ne(-1),
      forward: () => Ne(1),
      beforeEach: n.add,
      beforeResolve: l.add,
      afterEach: c.add,
      onError: ae.add,
      isReady: rt,
      install(P) {
        const B = this;
        P.component("RouterLink", kg),
          P.component("RouterView", xc),
          (P.config.globalProperties.$router = B),
          Object.defineProperty(P.config.globalProperties, "$route", {
            enumerable: !0,
            get: () => pe(r),
          }),
          Bt &&
            !Ht &&
            r.value === at &&
            ((Ht = !0), v(i.location).catch(ue => {}));
        const L = {};
        for (const ue in at) L[ue] = Te(() => r.value[ue]);
        P.provide(rn, B), P.provide(jc, Ge(L)), P.provide(Ei, r);
        const $ = P.unmount;
        Ft.add(P),
          (P.unmount = function () {
            Ft.delete(P),
              Ft.size < 1 &&
                ((o = at),
                X && X(),
                (X = null),
                (r.value = at),
                (Ht = !1),
                (ce = !1)),
              $();
          });
      },
    };
  function Se(P) {
    return P.reduce((B, L) => B.then(() => F(L)), Promise.resolve());
  }
  return Vs;
}
function Sg(e, t) {
  const s = [],
    a = [],
    i = [],
    n = Math.max(t.matched.length, e.matched.length);
  for (let l = 0; l < n; l++) {
    const c = t.matched[l];
    c && (e.matched.find(o => ts(o, c)) ? a.push(c) : s.push(c));
    const r = e.matched[l];
    r && (t.matched.find(o => ts(o, r)) || i.push(r));
  }
  return [s, a, i];
}
const ql = [],
  Ig = { props: { headerClass: String, text: String, link: String } },
  re = (e, t) => {
    const s = e.__vccOpts || e;
    for (const [a, i] of t) s[a] = i;
    return s;
  },
  Tg = ["href"],
  Rg = { key: 1 };
function Mg(e, t, s, a, i, n) {
  return s.text
    ? (T(),
      H(
        "header",
        { key: 0, class: E(s.headerClass) },
        [
          s.link
            ? (T(),
              H(
                "a",
                { key: 0, href: s.link },
                [k("h2", null, ye(s.text), 1)],
                8,
                Tg
              ))
            : (T(), H("h2", Rg, ye(s.text), 1)),
        ],
        2
      ))
    : Pe("", !0);
}
const Ng = re(Ig, [["render", Mg]]),
  Og = {},
  Ug = { width: "24", height: "24", viewBox: "0 0 24 24" },
  Lg = k("title", null, "Lightning Icon", -1),
  Hg = k("path", { d: "M8 24l3-9h-9l14-15-3 9h9l-14 15z" }, null, -1),
  Fg = [Lg, Hg];
function Dg(e, t) {
  return T(), H("svg", Ug, Fg);
}
const zg = re(Og, [["render", Dg]]),
  Bg = {},
  Qg = { width: "24", height: "24", viewBox: "0 0 24 24" },
  Vg = k("title", null, "Play Icon", -1),
  Wg = k(
    "path",
    {
      d: "M12 2c5.514 0 10 4.486 10 10s-4.486 10-10 10-10-4.486-10-10 4.486-10 10-10zm0-2c-6.627 0-12 5.373-12 12s5.373 12 12 12 12-5.373 12-12-5.373-12-12-12zm-3 17v-10l9 5.146-9 4.854z",
    },
    null,
    -1
  ),
  Yg = [Vg, Wg];
function $g(e, t) {
  return T(), H("svg", Qg, Yg);
}
const Kg = re(Bg, [["render", $g]]),
  Jg = {},
  Zg = {
    width: "24",
    height: "24",
    viewBox: "0 0 24 24",
    fillRule: "evenodd",
    clipRule: "evenodd",
  },
  Gg = k("title", null, "Fire Icon", -1),
  Xg = k(
    "path",
    {
      d: "M8.625 0c.61 7.189-5.625 9.664-5.625 15.996 0 4.301 3.069 7.972 9 8.004 5.931.032 9-4.414 9-8.956 0-4.141-2.062-8.046-5.952-10.474.924 2.607-.306 4.988-1.501 5.808.07-3.337-1.125-8.289-4.922-10.378zm4.711 13c3.755 3.989 1.449 9-1.567 9-1.835 0-2.779-1.265-2.769-2.577.019-2.433 2.737-2.435 4.336-6.423z",
    },
    null,
    -1
  ),
  ep = [Gg, Xg];
function tp(e, t) {
  return T(), H("svg", Zg, ep);
}
const sp = re(Jg, [["render", tp]]),
  ap = { props: { text: String, textClass: [String, Array], type: String } };
function ip(e, t, s, a, i, n) {
  return s.text
    ? (T(),
      _e(
        vr(s.type || "p"),
        { key: 0, class: E(s.textClass) },
        { default: Xe(() => [cs(ye(s.text), 1)]), _: 1 },
        8,
        ["class"]
      ))
    : Pe("", !0);
}
const Ma = re(ap, [["render", ip]]),
  np = "_breaking_1esiw_110",
  lp = "_watch_1esiw_114",
  rp = "_horizontal_1esiw_166",
  cp = "_vertical_1esiw_170",
  op = "_bullets_1esiw_180",
  Na = {
    "article-header": "_article-header_1esiw_1",
    "article-body": "_article-body_1esiw_46",
    "article-image-container": "_article-image-container_1esiw_66",
    "article-image": "_article-image_1esiw_66",
    "article-image-captions": "_article-image-captions_1esiw_90",
    "article-image-tag": "_article-image-tag_1esiw_95",
    breaking: np,
    watch: lp,
    "article-title": "_article-title_1esiw_130",
    "article-content": "_article-content_1esiw_139",
    "article-list": "_article-list_1esiw_153",
    "article-list-item": "_article-list-item_1esiw_161",
    horizontal: rp,
    vertical: cp,
    bullets: op,
    "article-hero": "_article-hero_1esiw_195",
    "article-list-content": "_article-list-content_1esiw_213",
  },
  up = {
    props: { tag: Object },
    data() {
      return { styles: Na };
    },
  };
function mp(e, t, s, a, i, n) {
  const l = zg,
    c = Kg,
    r = sp,
    o = Ma;
  return s.tag
    ? (T(),
      H(
        "div",
        {
          key: 0,
          class: E([i.styles["article-image-tag"], i.styles[s.tag.type]]),
        },
        [
          s.tag.type === "breaking" ? (T(), _e(l, { key: 0 })) : Pe("", !0),
          s.tag.type === "watch" ? (T(), _e(c, { key: 1 })) : Pe("", !0),
          s.tag.type === "new" ? (T(), _e(r, { key: 2 })) : Pe("", !0),
          Q(o, { text: s.tag.label }, null, 8, ["text"]),
        ],
        2
      ))
    : Pe("", !0);
}
const hp = re(up, [["render", mp]]),
  dp = {
    props: { image: Object, imageClass: String, meta: Object },
    data() {
      return { styles: Na };
    },
  },
  gp = ["src", "width", "height", "alt"];
function pp(e, t, s, a, i, n) {
  var r, o;
  const l = hp,
    c = Ma;
  return (
    T(),
    H(
      ne,
      null,
      [
        s.image
          ? (T(),
            H(
              "div",
              { key: 0, class: E(s.imageClass), style: { width: "auto" } },
              [
                k(
                  "img",
                  {
                    class: E(i.styles["article-image"]),
                    src: s.image.src,
                    width: s.image.width,
                    height: s.image.height,
                    alt: s.image.alt,
                  },
                  null,
                  10,
                  gp
                ),
                Q(l, { tag: (r = s.meta) == null ? void 0 : r.tag }, null, 8, [
                  "tag",
                ]),
              ],
              2
            ))
          : Pe("", !0),
        Q(
          c,
          {
            "text-class": i.styles["article-image-captions"],
            text: (o = s.meta) == null ? void 0 : o.captions,
          },
          null,
          8,
          ["text-class", "text"]
        ),
      ],
      64
    )
  );
}
const qc = re(dp, [["render", pp]]),
  fp = "_preview_3uw7j_2",
  bp = "_page_3uw7j_12",
  _p = "_row_3uw7j_46",
  yp = "_column_3uw7j_52",
  Qs = {
    preview: fp,
    "no-scroll": "_no-scroll_3uw7j_8",
    page: bp,
    "page-main": "_page-main_3uw7j_28",
    row: _p,
    column: yp,
    "columns-1": "_columns-1_3uw7j_59",
    "columns-2-balanced": "_columns-2-balanced_3uw7j_63",
    "columns-3-balanced": "_columns-3-balanced_3uw7j_67",
    "columns-4-balanced": "_columns-4-balanced_3uw7j_71",
    "columns-3-wide": "_columns-3-wide_3uw7j_75",
    "columns-3-narrow": "_columns-3-narrow_3uw7j_79",
    "columns-wrap": "_columns-wrap_3uw7j_83",
    "grid-container": "_grid-container_3uw7j_88",
    "grid-wrap": "_grid-wrap_3uw7j_95",
    "grid-item": "_grid-item_3uw7j_99",
    "row-header": "_row-header_3uw7j_104",
  },
  vp = {
    props: { type: String, content: [String, Array], display: String },
    data() {
      return { styles: Na, layoutStyles: Qs };
    },
  },
  wp = ["href"],
  jp = ["href"],
  xp = ["href"];
function qp(e, t, s, a, i, n) {
  const l = Ma,
    c = qc;
  return (
    T(),
    H(
      ne,
      null,
      [
        s.type === "text"
          ? (T(),
            H(
              "div",
              { key: 0, class: E(i.styles["article-content"]) },
              [Q(l, { text: s.content }, null, 8, ["text"])],
              2
            ))
          : Pe("", !0),
        s.type === "list"
          ? (T(),
            H(
              "div",
              { key: 1, class: E(i.styles["article-content"]) },
              [
                k(
                  "ul",
                  {
                    class: E([
                      i.styles["article-list"],
                      i.styles.vertical,
                      { [i.styles[s.display]]: s.display },
                    ]),
                  },
                  [
                    (T(!0),
                    H(
                      ne,
                      null,
                      Fe(
                        s.content,
                        r => (
                          T(),
                          H(
                            "li",
                            {
                              key: r.id,
                              class: E(i.styles["article-list-item"]),
                            },
                            [
                              r.url && !r.title
                                ? (T(),
                                  H(
                                    "a",
                                    { key: 0, href: r.url },
                                    [
                                      Q(l, { text: r.content }, null, 8, [
                                        "text",
                                      ]),
                                    ],
                                    8,
                                    wp
                                  ))
                                : (T(),
                                  _e(l, { key: 1, text: r.content }, null, 8, [
                                    "text",
                                  ])),
                            ],
                            2
                          )
                        )
                      ),
                      128
                    )),
                  ],
                  2
                ),
              ],
              2
            ))
          : Pe("", !0),
        s.type === "articles-list"
          ? (T(),
            H(
              "div",
              { key: 2, class: E(i.styles["article-list-content"]) },
              [
                k(
                  "ul",
                  { class: E([i.styles["article-list"], i.styles.vertical]) },
                  [
                    (T(!0),
                    H(
                      ne,
                      null,
                      Fe(
                        s.content,
                        r => (
                          T(),
                          H(
                            "li",
                            {
                              key: r.id,
                              class: E(i.styles["article-list-item"]),
                            },
                            [
                              Q(
                                l,
                                {
                                  "text-class": [
                                    i.styles["article-title"],
                                    "truncate-multiline",
                                    "truncate-multiline-3",
                                  ],
                                  text: r.title,
                                  type: "h3",
                                },
                                null,
                                8,
                                ["text-class", "text"]
                              ),
                              r.url && !r.title
                                ? (T(),
                                  H(
                                    "a",
                                    { key: 0, href: r.url },
                                    [
                                      Q(l, { text: r.content }, null, 8, [
                                        "text",
                                      ]),
                                    ],
                                    8,
                                    jp
                                  ))
                                : (T(),
                                  _e(l, { key: 1, text: r.content }, null, 8, [
                                    "text",
                                  ])),
                            ],
                            2
                          )
                        )
                      ),
                      128
                    )),
                  ],
                  2
                ),
              ],
              2
            ))
          : Pe("", !0),
        s.type === "excerpt"
          ? (T(),
            H(
              "ul",
              {
                key: 3,
                class: E([i.styles["article-list"], i.styles.horizontal]),
              },
              [
                (T(!0),
                H(
                  ne,
                  null,
                  Fe(
                    s.content,
                    r => (
                      T(),
                      H(
                        "li",
                        { key: r.id, class: E(i.styles["article-list-item"]) },
                        [
                          Q(
                            c,
                            {
                              "image-class": i.styles["article-hero"],
                              image: r.image,
                            },
                            null,
                            8,
                            ["image-class", "image"]
                          ),
                          k(
                            "div",
                            { class: E(i.styles["article-content"]) },
                            [
                              Q(
                                l,
                                {
                                  "text-class": [
                                    "truncate-multiline",
                                    "truncate-multiline-3",
                                  ],
                                  text: r.text,
                                  type: "div",
                                },
                                null,
                                8,
                                ["text"]
                              ),
                            ],
                            2
                          ),
                        ],
                        2
                      )
                    )
                  ),
                  128
                )),
              ],
              2
            ))
          : Pe("", !0),
        s.type === "grid"
          ? (T(),
            H(
              "div",
              {
                key: 4,
                class: E([
                  i.layoutStyles["grid-container"],
                  { [i.layoutStyles[s.display]]: s.display },
                ]),
              },
              [
                (T(!0),
                H(
                  ne,
                  null,
                  Fe(
                    s.content,
                    r => (
                      T(),
                      H(
                        "div",
                        { key: r.id, class: E(i.layoutStyles["grid-item"]) },
                        [
                          Q(
                            c,
                            {
                              "image-class":
                                i.styles["article-image-container"],
                              image: r.image,
                              meta: r.meta,
                            },
                            null,
                            8,
                            ["image-class", "image", "meta"]
                          ),
                          r.url
                            ? (T(),
                              H(
                                "a",
                                { key: 0, href: r.url },
                                [
                                  Q(
                                    l,
                                    {
                                      "text-class": [
                                        i.styles["article-content"],
                                        "truncate-multiline",
                                        "truncate-multiline-3",
                                      ],
                                      text: r.text,
                                      type: "h3",
                                    },
                                    null,
                                    8,
                                    ["text-class", "text"]
                                  ),
                                ],
                                8,
                                xp
                              ))
                            : (T(),
                              _e(
                                l,
                                {
                                  key: 1,
                                  "text-class": [
                                    i.styles["article-content"],
                                    "truncate-multiline",
                                    "truncate-multiline-3",
                                  ],
                                  text: r.text,
                                  type: "h3",
                                },
                                null,
                                8,
                                ["text-class", "text"]
                              )),
                        ],
                        2
                      )
                    )
                  ),
                  128
                )),
              ],
              2
            ))
          : Pe("", !0),
        s.type === "preview"
          ? (T(),
            H(
              "ul",
              {
                key: 5,
                class: E([i.styles["article-list"], i.styles.vertical]),
              },
              [
                (T(!0),
                H(
                  ne,
                  null,
                  Fe(
                    s.content,
                    r => (
                      T(),
                      H(
                        "li",
                        { key: r.id, class: E(i.styles["article-list-item"]) },
                        [
                          Q(
                            c,
                            {
                              "image-class":
                                i.styles["article-image-container"],
                              image: r.image,
                            },
                            null,
                            8,
                            ["image-class", "image"]
                          ),
                          Q(
                            l,
                            {
                              "text-class": [
                                i.styles["article-title"],
                                "truncate-multiline",
                                "truncate-multiline-3",
                              ],
                              text: r.title,
                              type: "h3",
                            },
                            null,
                            8,
                            ["text-class", "text"]
                          ),
                        ],
                        2
                      )
                    )
                  ),
                  128
                )),
              ],
              2
            ))
          : Pe("", !0),
      ],
      64
    )
  );
}
const kp = re(vp, [["render", qp]]),
  Pp = {
    props: { article: Object },
    data() {
      return { layoutStyles: Qs, articleStyles: Na };
    },
  };
function Ep(e, t, s, a, i, n) {
  const l = Ng,
    c = qc,
    r = Ma,
    o = kp;
  return (
    T(),
    H(
      "article",
      {
        class: E([
          i.layoutStyles.column,
          i.layoutStyles[s.article.class],
          i.articleStyles.article,
        ]),
      },
      [
        Q(
          l,
          {
            "header-class": i.articleStyles["article-header"],
            text: s.article.header,
            link: s.article.url,
          },
          null,
          8,
          ["header-class", "text", "link"]
        ),
        k(
          "section",
          { class: E(i.articleStyles["article-body"]) },
          [
            Q(
              c,
              {
                "image-class": i.articleStyles["article-image-container"],
                image: s.article.image,
                meta: s.article.meta,
              },
              null,
              8,
              ["image-class", "image", "meta"]
            ),
            Q(
              r,
              {
                "text-class": [
                  i.articleStyles["article-title"],
                  "truncate-singleline",
                ],
                text: s.article.title,
                type: "h3",
              },
              null,
              8,
              ["text-class", "text"]
            ),
            Q(
              o,
              {
                type: s.article.type,
                content: s.article.content,
                display: s.article.display,
              },
              null,
              8,
              ["type", "content", "display"]
            ),
          ],
          2
        ),
      ],
      2
    )
  );
}
const Ap = re(Pp, [["render", Ep]]),
  Cp = {
    props: { section: Object },
    data() {
      return { styles: Qs };
    },
  },
  Sp = ["id"];
function Ip(e, t, s, a, i, n) {
  var c;
  const l = Ap;
  return (
    T(),
    H(
      ne,
      null,
      [
        (c = s.section) != null && c.name
          ? (T(),
            H(
              "div",
              { key: 0, id: s.section.id, class: E(i.styles["row-header"]) },
              [k("h2", null, ye(s.section.name), 1)],
              10,
              Sp
            ))
          : Pe("", !0),
        k(
          "section",
          { class: E(i.styles.row) },
          [
            (T(!0),
            H(
              ne,
              null,
              Fe(
                s.section.articles,
                (r, o) => (
                  T(),
                  _e(l, { key: `${s.section.id}-${o}`, article: r }, null, 8, [
                    "article",
                  ])
                )
              ),
              128
            )),
          ],
          2
        ),
      ],
      64
    )
  );
}
const Tp = re(Cp, [["render", Ip]]),
  Rp = "_toast_h9j28_1",
  Mp = "_open_h9j28_17",
  Np = {
    toast: Rp,
    open: Mp,
    "toast-close-button": "_toast-close-button_h9j28_24",
    "toast-close-button-icon": "_toast-close-button-icon_h9j28_36",
    "toast-header": "_toast-header_h9j28_43",
    "toast-body": "_toast-body_h9j28_54",
    "toast-description": "_toast-description_h9j28_61",
    "toast-actions": "_toast-actions_h9j28_80",
    "toast-actions-button": "_toast-actions-button_h9j28_85",
  },
  Op = "_button_n5y7z_1",
  Up = "_dark_n5y7z_41",
  kc = {
    button: Op,
    "primary-button": "_primary-button_n5y7z_18",
    "secondary-button": "_secondary-button_n5y7z_30",
    dark: Up,
  },
  Lp = {
    props: {
      onClose: Function,
      onAccept: Function,
      onReject: Function,
      notification: Object,
    },
    data() {
      return {
        toastStyles: Np,
        buttonStyles: kc,
        callbacks: { accept: this.onAccept, reject: this.onReject },
      };
    },
  },
  Hp = k("span", { class: "animated-icon-inner" }, [k("span"), k("span")], -1),
  Fp = [Hp],
  Dp = ["id", "onClick"];
function zp(e, t, s, a, i, n) {
  return (
    T(),
    H(
      "div",
      { class: E([i.toastStyles.toast, i.toastStyles.open]) },
      [
        k(
          "button",
          {
            id: "close-toast-link",
            class: E(i.toastStyles["toast-close-button"]),
            title: "Close Button",
            onClick: t[0] || (t[0] = (...l) => s.onClose && s.onClose(...l)),
          },
          [
            k(
              "div",
              {
                class: E([
                  i.toastStyles["toast-close-button-icon"],
                  "animated-icon",
                  "close-icon",
                  "hover",
                ]),
                title: "Close Icon",
              },
              Fp,
              2
            ),
          ],
          2
        ),
        s.notification.title
          ? (T(),
            H(
              "header",
              { key: 0, class: E(i.toastStyles["toast-header"]) },
              [k("h2", null, ye(s.notification.title), 1)],
              2
            ))
          : Pe("", !0),
        k(
          "section",
          { class: E(i.toastStyles["toast-body"]) },
          [
            k(
              "div",
              { class: E(i.toastStyles["toast-description"]) },
              ye(s.notification.description),
              3
            ),
            k(
              "div",
              { class: E(i.toastStyles["toast-actions"]) },
              [
                (T(!0),
                H(
                  ne,
                  null,
                  Fe(
                    s.notification.actions,
                    l => (
                      T(),
                      H(
                        "button",
                        {
                          id: `toast-${l.type}-button`,
                          key: `toast-${l.type}-button`,
                          class: E([
                            i.buttonStyles.button,
                            i.buttonStyles[`${l.priority}-button`],
                            i.toastStyles["toast-actions-button"],
                          ]),
                          onClick: i.callbacks[l.type],
                        },
                        ye(l.name),
                        11,
                        Dp
                      )
                    )
                  ),
                  128
                )),
              ],
              2
            ),
          ],
          2
        ),
      ],
      2
    )
  );
}
const Bp = re(Lp, [["render", zp]]),
  Ai =
    globalThis.requestIdleCallback ||
    (e => {
      const t = Date.now(),
        s = {
          didTimeout: !1,
          timeRemaining: () => Math.max(0, 50 - (Date.now() - t)),
        };
      return setTimeout(() => {
        e(s);
      }, 1);
    }),
  Qp =
    globalThis.cancelIdleCallback ||
    (e => {
      clearTimeout(e);
    }),
  Vp = e => {
    const t = we();
    t.isHydrating
      ? t.hooks.hookOnce("app:suspense:resolve", () => {
          Ai(e);
        })
      : Ai(e);
  };
async function Pc(e, t = kt()) {
  const { path: s, matched: a } = t.resolve(e);
  if (
    !a.length ||
    (t._routePreloaded || (t._routePreloaded = new Set()),
    t._routePreloaded.has(s))
  )
    return;
  const i = (t._preloadPromises = t._preloadPromises || []);
  if (i.length > 4) return Promise.all(i).then(() => Pc(e, t));
  t._routePreloaded.add(s);
  const n = a
    .map(l => {
      var c;
      return (c = l.components) == null ? void 0 : c.default;
    })
    .filter(l => typeof l == "function");
  for (const l of n) {
    const c = Promise.resolve(l())
      .catch(() => {})
      .finally(() => i.splice(i.indexOf(c)));
    i.push(c);
  }
  await Promise.all(i);
}
function Wp(e = {}) {
  const t = e.path || window.location.pathname;
  let s = {};
  try {
    s = JSON.parse(sessionStorage.getItem("nuxt:reload") || "{}");
  } catch {}
  if (
    e.force ||
    (s == null ? void 0 : s.path) !== t ||
    (s == null ? void 0 : s.expires) < Date.now()
  ) {
    try {
      sessionStorage.setItem(
        "nuxt:reload",
        JSON.stringify({ path: t, expires: Date.now() + (e.ttl ?? 1e4) })
      );
    } catch {}
    if (e.persistState)
      try {
        sessionStorage.setItem(
          "nuxt:reload:state",
          JSON.stringify({ state: we().payload.state })
        );
      } catch {}
    window.location.pathname !== t
      ? (window.location.href = t)
      : window.location.reload();
  }
}
const Yp = (...e) => e.find(t => t !== void 0),
  $p = "noopener noreferrer";
function Kp(e) {
  const t = e.componentName || "NuxtLink",
    s = (a, i) => {
      if (!a || (e.trailingSlash !== "append" && e.trailingSlash !== "remove"))
        return a;
      const n = e.trailingSlash === "append" ? Wr : tn;
      if (typeof a == "string") return n(a, !0);
      const l = "path" in a ? a.path : i(a).path;
      return { ...a, name: void 0, path: n(l, !0) };
    };
  return rs({
    name: t,
    props: {
      to: { type: [String, Object], default: void 0, required: !1 },
      href: { type: [String, Object], default: void 0, required: !1 },
      target: { type: String, default: void 0, required: !1 },
      rel: { type: String, default: void 0, required: !1 },
      noRel: { type: Boolean, default: void 0, required: !1 },
      prefetch: { type: Boolean, default: void 0, required: !1 },
      noPrefetch: { type: Boolean, default: void 0, required: !1 },
      activeClass: { type: String, default: void 0, required: !1 },
      exactActiveClass: { type: String, default: void 0, required: !1 },
      prefetchedClass: { type: String, default: void 0, required: !1 },
      replace: { type: Boolean, default: void 0, required: !1 },
      ariaCurrentValue: { type: String, default: void 0, required: !1 },
      external: { type: Boolean, default: void 0, required: !1 },
      custom: { type: Boolean, default: void 0, required: !1 },
    },
    setup(a, { slots: i }) {
      const n = kt(),
        l = Te(() => {
          const m = a.to || a.href || "";
          return s(m, n.resolve);
        }),
        c = Te(() =>
          a.external || (a.target && a.target !== "_self")
            ? !0
            : typeof l.value == "object"
              ? !1
              : l.value === "" || os(l.value, { acceptRelative: !0 })
        ),
        r = Qe(!1),
        o = Qe(null),
        u = m => {
          var d;
          o.value = a.custom
            ? (d = m == null ? void 0 : m.$el) == null
              ? void 0
              : d.nextElementSibling
            : m == null
              ? void 0
              : m.$el;
        };
      if (
        a.prefetch !== !1 &&
        a.noPrefetch !== !0 &&
        a.target !== "_blank" &&
        !Zp()
      ) {
        const d = we();
        let _,
          b = null;
        Hs(() => {
          const w = Jp();
          Vp(() => {
            _ = Ai(() => {
              var R;
              (R = o == null ? void 0 : o.value) != null &&
                R.tagName &&
                (b = w.observe(o.value, async () => {
                  b == null || b(), (b = null);
                  const f =
                    typeof l.value == "string"
                      ? l.value
                      : n.resolve(l.value).fullPath;
                  await Promise.all([
                    d.hooks.callHook("link:prefetch", f).catch(() => {}),
                    !c.value && Pc(l.value, n).catch(() => {}),
                  ]),
                    (r.value = !0);
                }));
            });
          });
        }),
          Fs(() => {
            _ && Qp(_), b == null || b(), (b = null);
          });
      }
      return () => {
        var w, R;
        if (!c.value) {
          const f = {
            ref: u,
            to: l.value,
            activeClass: a.activeClass || e.activeClass,
            exactActiveClass: a.exactActiveClass || e.exactActiveClass,
            replace: a.replace,
            ariaCurrentValue: a.ariaCurrentValue,
            custom: a.custom,
          };
          return (
            a.custom ||
              (r.value && (f.class = a.prefetchedClass || e.prefetchedClass),
              (f.rel = a.rel)),
            Ze(au("RouterLink"), f, i.default)
          );
        }
        const m =
            typeof l.value == "object"
              ? (((w = n.resolve(l.value)) == null ? void 0 : w.href) ?? null)
              : l.value || null,
          d = a.target || null,
          _ = a.noRel
            ? null
            : Yp(a.rel, e.externalRelAttribute, m ? $p : "") || null,
          b = () => uc(m, { replace: a.replace });
        return a.custom
          ? i.default
            ? i.default({
                href: m,
                navigate: b,
                get route() {
                  if (!m) return;
                  const f = Bs(m);
                  return {
                    path: f.pathname,
                    fullPath: f.pathname,
                    get query() {
                      return Vr(f.search);
                    },
                    hash: f.hash,
                    params: {},
                    name: void 0,
                    matched: [],
                    redirectedFrom: void 0,
                    meta: {},
                    href: m,
                  };
                },
                rel: _,
                target: d,
                isExternal: c.value,
                isActive: !1,
                isExactActive: !1,
              })
            : null
          : Ze(
              "a",
              { ref: o, href: m, rel: _, target: d },
              (R = i.default) == null ? void 0 : R.call(i)
            );
      };
    },
  });
}
const Oa = Kp({ componentName: "NuxtLink" });
function Jp() {
  const e = we();
  if (e._observer) return e._observer;
  let t = null;
  const s = new Map(),
    a = (n, l) => (
      t ||
        (t = new IntersectionObserver(c => {
          for (const r of c) {
            const o = s.get(r.target);
            (r.isIntersecting || r.intersectionRatio > 0) && o && o();
          }
        })),
      s.set(n, l),
      t.observe(n),
      () => {
        s.delete(n),
          t.unobserve(n),
          s.size === 0 && (t.disconnect(), (t = null));
      }
    );
  return (e._observer = { observe: a });
}
function Zp() {
  const e = navigator.connection;
  return !!(e && (e.saveData || /2g/.test(e.effectiveType)));
}
const Gp = {
  setup() {
    const { content: e } = ve("data");
    return { route: us(), content: e };
  },
  data() {
    return { showPortal: !1 };
  },
  mounted() {
    this.showPortal = this.content[this.$route.name].notification;
  },
  methods: {
    openPortal() {
      this.showPortal = !0;
    },
    closePortal() {
      this.showPortal = !1;
    },
  },
};
function Xp(e, t, s, a, i, n) {
  const l = Tp,
    c = Bp;
  return (
    T(),
    H(
      ne,
      null,
      [
        (T(!0),
        H(
          ne,
          null,
          Fe(
            a.content[a.route.name].sections,
            r => (T(), _e(l, { key: r.id, section: r }, null, 8, ["section"]))
          ),
          128
        )),
        (T(),
        _e(Tr, { to: "body" }, [
          a.content[a.route.name].notification
            ? Vi(
                (T(),
                _e(
                  c,
                  {
                    key: 0,
                    "on-close": n.closePortal,
                    "on-accept": n.closePortal,
                    "on-reject": n.closePortal,
                    notification: a.content[a.route.name].notification,
                  },
                  null,
                  8,
                  ["on-close", "on-accept", "on-reject", "notification"]
                )),
                [[en, i.showPortal]]
              )
            : Pe("", !0),
        ])),
      ],
      64
    )
  );
}
const yt = re(Gp, [["render", Xp]]),
  ef = {
    routes: e => [
      { name: "home", path: "/", component: yt },
      { name: "us", path: "/us", component: yt },
      { name: "world", path: "/world", component: yt },
      { name: "politics", path: "/politics", component: yt },
      { name: "business", path: "/business", component: yt },
      { name: "opinion", path: "/opinion", component: yt },
      { name: "health", path: "/health", component: yt },
      { name: "", path: "/index.html", component: yt },
    ],
  },
  tf = {
    scrollBehavior(e, t, s) {
      const a = we();
      let i = s || void 0;
      if (
        (!i &&
          t &&
          e &&
          e.meta.scrollToTop !== !1 &&
          sf(t, e) &&
          (i = { left: 0, top: 0 }),
        e.path === t.path)
      ) {
        if (t.hash && !e.hash) return { left: 0, top: 0 };
        if (e.hash) return { el: e.hash, top: kl(e.hash) };
      }
      const n = c => !!(c.meta.pageTransition ?? vi),
        l = n(t) && n(e) ? "page:transition:finish" : "page:finish";
      return new Promise(c => {
        a.hooks.hookOnce(l, async () => {
          await Lt(), e.hash && (i = { el: e.hash, top: kl(e.hash) }), c(i);
        });
      });
    },
  };
function kl(e) {
  try {
    const t = document.querySelector(e);
    if (t) return parseFloat(getComputedStyle(t).scrollMarginTop);
  } catch {}
  return 0;
}
function sf(e, t) {
  const s = e.matched[0] === t.matched[0];
  return !!(!s || (s && JSON.stringify(e.params) !== JSON.stringify(t.params)));
}
const af = { hashMode: !0 },
  Oe = { ...af, ...tf, ...ef },
  nf = jd(async e => {
    var r;
    let t, s;
    if (!((r = e.meta) != null && r.validate)) return;
    const a = we(),
      i = kt();
    if (
      (([t, s] = _a(() => Promise.resolve(e.meta.validate(e)))),
      (t = await t),
      s(),
      t) === !0
    )
      return;
    const l = nn({
        statusCode: 404,
        statusMessage: `Page Not Found: ${e.fullPath}`,
      }),
      c = i.beforeResolve(o => {
        if ((c(), o === e)) {
          const u = i.afterEach(async () => {
            u(),
              await a.runWithContext(() => Qt(l)),
              window.history.pushState({}, "", e.fullPath);
          });
          return !1;
        }
      });
  }),
  lf = [nf],
  Es = {};
function rf(e, t) {
  const { pathname: s, search: a, hash: i } = t,
    n = e.indexOf("#");
  if (n > -1) {
    const c = i.includes(e.slice(n)) ? e.slice(n).length : 1;
    let r = i.slice(c);
    return r[0] !== "/" && (r = "/" + r), Kn(r, "");
  }
  return Kn(s, e) + a + i;
}
const cf = dt(
    {
      name: "nuxt:router",
      enforce: "pre",
      async setup(e) {
        var w, R;
        let t,
          s,
          a = sn().app.baseURL;
        Oe.hashMode && !a.includes("#") && (a += "#");
        const i =
            ((w = Oe.history) == null ? void 0 : w.call(Oe, a)) ??
            (Oe.hashMode ? Wd(a) : dc(a)),
          n = ((R = Oe.routes) == null ? void 0 : R.call(Oe, ql)) ?? ql;
        let l;
        const c = rf(a, window.location),
          r = Cg({
            ...Oe,
            scrollBehavior: (f, p, q) => {
              var v;
              if (p === at) {
                l = q;
                return;
              }
              return (
                (r.options.scrollBehavior = Oe.scrollBehavior),
                (v = Oe.scrollBehavior) == null
                  ? void 0
                  : v.call(Oe, f, at, l || q)
              );
            },
            history: i,
            routes: n,
          });
        e.vueApp.use(r);
        const o = Ss(r.currentRoute.value);
        r.afterEach((f, p) => {
          o.value = p;
        }),
          Object.defineProperty(
            e.vueApp.config.globalProperties,
            "previousRoute",
            { get: () => o.value }
          );
        const u = Ss(r.resolve(c)),
          m = () => {
            u.value = r.currentRoute.value;
          };
        e.hook("page:finish", m),
          r.afterEach((f, p) => {
            var q, v, C, O;
            ((v = (q = f.matched[0]) == null ? void 0 : q.components) == null
              ? void 0
              : v.default) ===
              ((O = (C = p.matched[0]) == null ? void 0 : C.components) == null
                ? void 0
                : O.default) && m();
          });
        const d = {};
        for (const f in u.value) d[f] = Te(() => u.value[f]);
        (e._route = Ge(d)),
          (e._middleware = e._middleware || { global: [], named: {} });
        const _ = Ta();
        try {
          ([t, s] = _a(() => r.isReady())), await t, s();
        } catch (f) {
          ([t, s] = _a(() => e.runWithContext(() => Qt(f)))), await t, s();
        }
        const b = wd("_layout");
        return (
          r.beforeEach(async (f, p) => {
            var q;
            (f.meta = Ge(f.meta)),
              e.isHydrating &&
                b.value &&
                !Ut(f.meta.layout) &&
                (f.meta.layout = b.value),
              (e._processingMiddleware = !0);
            {
              const v = new Set([...lf, ...e._middleware.global]);
              for (const C of f.matched) {
                const O = C.meta.middleware;
                if (O)
                  if (Array.isArray(O)) for (const M of O) v.add(M);
                  else v.add(O);
              }
              for (const C of v) {
                const O =
                  typeof C == "string"
                    ? e._middleware.named[C] ||
                      (await ((q = Es[C]) == null
                        ? void 0
                        : q.call(Es).then(x => x.default || x)))
                    : C;
                if (!O) throw new Error(`Unknown route middleware: '${C}'.`);
                const M = await e.runWithContext(() => O(f, p));
                if (
                  !e.payload.serverRendered &&
                  e.isHydrating &&
                  (M === !1 || M instanceof Error)
                ) {
                  const x =
                    M ||
                    xi({
                      statusCode: 404,
                      statusMessage: `Page Not Found: ${c}`,
                    });
                  return await e.runWithContext(() => Qt(x)), !1;
                }
                if (M || M === !1) return M;
              }
            }
          }),
          r.onError(() => {
            delete e._processingMiddleware;
          }),
          r.afterEach(async (f, p, q) => {
            delete e._processingMiddleware,
              !e.isHydrating && _.value && (await e.runWithContext(qd)),
              f.matched.length === 0 &&
                (await e.runWithContext(() =>
                  Qt(
                    xi({
                      statusCode: 404,
                      fatal: !1,
                      statusMessage: `Page not found: ${f.fullPath}`,
                    })
                  )
                ));
          }),
          e.hooks.hookOnce("app:created", async () => {
            try {
              await r.replace({ ...r.resolve(c), name: void 0, force: !0 }),
                (r.options.scrollBehavior = Oe.scrollBehavior);
            } catch (f) {
              await e.runWithContext(() => Qt(f));
            }
          }),
          { provide: { router: r } }
        );
      },
    },
    1
  ),
  na = {},
  of = dt({
    name: "nuxt:prefetch",
    setup(e) {
      const t = kt();
      e.hooks.hook("app:mounted", () => {
        t.beforeEach(async s => {
          var i;
          const a =
            (i = s == null ? void 0 : s.meta) == null ? void 0 : i.layout;
          a && typeof na[a] == "function" && (await na[a]());
        });
      }),
        e.hooks.hook("link:prefetch", s => {
          var l, c, r, o;
          if (os(s)) return;
          const a = t.resolve(s);
          if (!a) return;
          const i =
            (l = a == null ? void 0 : a.meta) == null ? void 0 : l.layout;
          let n = Array.isArray(
            (c = a == null ? void 0 : a.meta) == null ? void 0 : c.middleware
          )
            ? (r = a == null ? void 0 : a.meta) == null
              ? void 0
              : r.middleware
            : [
                (o = a == null ? void 0 : a.meta) == null
                  ? void 0
                  : o.middleware,
              ];
          n = n.filter(u => typeof u == "string");
          for (const u of n) typeof Es[u] == "function" && Es[u]();
          i && typeof na[i] == "function" && na[i]();
        });
    },
  }),
  uf = dt({
    name: "nuxt:chunk-reload",
    setup(e) {
      const t = kt(),
        s = sn(),
        a = new Set();
      t.beforeEach(() => {
        a.clear();
      }),
        e.hook("app:chunkError", ({ error: i }) => {
          a.add(i);
        }),
        t.onError((i, n) => {
          if (a.has(i)) {
            const c =
              "href" in n && n.href.startsWith("#")
                ? s.app.baseURL + n.href
                : zs(s.app.baseURL, n.fullPath);
            Wp({ path: c, persistState: !0 });
          }
        });
    },
  }),
  mf = dt({
    name: "nuxt:payload",
    setup(e) {
      gd() &&
        (e.hooks.hook("link:prefetch", async t => {
          Bs(t).protocol || (await nl(t));
        }),
        kt().beforeResolve(async (t, s) => {
          if (t.path === s.path) return;
          const a = await nl(t.path);
          a && Object.assign(e.static.data, a.data);
        }));
    },
  }),
  hf = dt({
    order: -40,
    setup(e) {
      (e.$config.app.baseURL = window.location.pathname.replace(
        /\/dist\/(.*)/,
        "/dist/"
      )),
        (e.$config.app.cdnURL = "/");
    },
  }),
  df = [Pd, Ed, Ad, cf, of, uf, mf, hf],
  gf = (e, t) =>
    t.path
      .replace(/(:\w+)\([^)]+\)/g, "$1")
      .replace(/(:\w+)[?+*]/g, "$1")
      .replace(/:\w+/g, s => {
        var a;
        return (
          ((a = e.params[s.slice(1)]) == null ? void 0 : a.toString()) || ""
        );
      }),
  pf = (e, t) => {
    const s = e.route.matched.find(i => {
        var n;
        return (
          ((n = i.components) == null ? void 0 : n.default) === e.Component.type
        );
      }),
      a = t ?? (s == null ? void 0 : s.meta.key) ?? (s && gf(e.route, s));
    return typeof a == "function" ? a(e.route) : a;
  },
  ff = (e, t) => ({ default: () => (e ? Ze(Jo, e === !0 ? {} : e, t) : t) }),
  bf = (e, t, s) => (
    (t = t === !0 ? {} : t),
    {
      default: () => {
        var a;
        return t ? Ze(e, t, s) : (a = s.default) == null ? void 0 : a.call(s);
      },
    }
  ),
  _f = rs({
    name: "NuxtPage",
    inheritAttrs: !1,
    props: {
      name: { type: String },
      transition: { type: [Boolean, Object], default: void 0 },
      keepalive: { type: [Boolean, Object], default: void 0 },
      route: { type: Object },
      pageKey: { type: [Function, String], default: null },
    },
    setup(e, { attrs: t }) {
      const s = we();
      return () =>
        Ze(
          xc,
          { name: e.name, route: e.route, ...t },
          {
            default: a => {
              if (!a.Component) return;
              const i = pf(a, e.pageKey),
                n = s.deferHydration(),
                l = !!(e.transition ?? a.route.meta.pageTransition ?? vi),
                c =
                  l &&
                  vf(
                    [
                      e.transition,
                      a.route.meta.pageTransition,
                      vi,
                      {
                        onAfterLeave: () => {
                          s.callHook("page:transition:finish", a.Component);
                        },
                      },
                    ].filter(Boolean)
                  );
              return bf(
                Xi,
                l && c,
                ff(
                  e.keepalive ?? a.route.meta.keepalive ?? ud,
                  Ze(
                    rr,
                    {
                      suspensible: !0,
                      onPending: () => s.callHook("page:start", a.Component),
                      onResolve: () => {
                        Lt(() =>
                          s.callHook("page:finish", a.Component).finally(n)
                        );
                      },
                    },
                    {
                      default: () =>
                        Ze(wf, {
                          key: i,
                          routeProps: a,
                          pageKey: i,
                          hasTransition: l,
                        }),
                    }
                  )
                )
              ).default();
            },
          }
        );
    },
  });
function yf(e) {
  return Array.isArray(e) ? e : e ? [e] : [];
}
function vf(e) {
  const t = e.map(s => ({ ...s, onAfterLeave: yf(s.onAfterLeave) }));
  return _d(...t);
}
const wf = rs({
    name: "RouteProvider",
    props: ["routeProps", "pageKey", "hasTransition"],
    setup(e) {
      const t = e.pageKey,
        s = e.routeProps.route,
        a = {};
      for (const i in e.routeProps.route)
        a[i] = Te(() => (t === e.pageKey ? e.routeProps.route[i] : s[i]));
      return Nt("_route", Ge(a)), () => Ze(e.routeProps.Component);
    },
  }),
  jf = {},
  xf = { viewBox: "0 0 469 64", width: "469", height: "64" },
  qf = k("title", null, "The Daily Broadcast", -1),
  kf = k(
    "path",
    {
      d: "m16.7 56h-10.3v-41.7h-6.1v-9.9h22.5v9.9h-6.1zm19.6 0h-10.8v-51.5h10.8v12q0.8-2.5 2.6-3.7 1.8-1.2 4.1-1.2 4.6 0 6.7 2.9 2 2.9 2 7.7v33.8h-10.6v-33.1q0-1.5-0.6-2.4-0.6-0.9-1.9-0.9-1 0-1.7 1-0.6 0.9-0.6 2.2zm31.8 0.5q-4.6 0-7.4-1.8-2.8-1.8-4-5.1-1.2-3.3-1.2-7.9v-17.7q0-6.1 3.5-9.3 3.5-3.1 9.7-3.1 12.6 0 12.6 12.4v3.2q0 5.8-0.1 7.8h-15.2v8.5q0 1.2 0.1 2.3 0.2 1.1 0.7 1.8 0.5 0.8 1.6 0.8 1.7 0 2.1-1.4 0.4-1.5 0.4-3.8v-4.2h10.4v2.5q0 4.9-1.2 8.3-1.2 3.3-4.1 5-2.9 1.7-7.9 1.7zm-2.2-32.7v6h5v-6q0-2.3-0.6-3.3-0.6-1.1-1.8-1.1-1.2 0-1.9 1-0.7 1-0.7 3.4zm47.3 32.2h-13.8v-51.6h14.1q5.6 0 8.4 3.1 2.8 3.1 2.8 9.1v24.1q0 7.3-2.5 11.3-2.6 4-9 4zm-3.5-42.6v33.5h1.8q2.9 0 2.9-2.8v-26.6q0-2.6-0.7-3.3-0.7-0.8-2.8-0.8zm27 43.1q-3.6 0-5.6-1.7-1.9-1.7-2.6-4.7-0.7-3-0.7-6.7 0-4 0.8-6.6 0.8-2.6 2.7-4.2 1.9-1.6 5.3-2.8l6.5-2.2v-4.5q0-3.6-2.3-3.6-2.1 0-2.1 2.9v2.7h-10.2q0-0.3 0-0.6 0-0.4 0-0.9 0-6.5 3-9.3 3.1-2.7 9.9-2.7 3.5 0 6.3 1.2 2.7 1.3 4.3 3.7 1.7 2.4 1.7 6v33.5h-10.4v-5.2q-0.8 2.7-2.6 4.2-1.7 1.5-4 1.5zm4.2-8.2q1.2 0 1.7-1.1 0.5-1.1 0.5-2.3v-12.3q-2.2 0.9-3.4 2.3-1.2 1.3-1.2 3.9v5.6q0 3.9 2.4 3.9zm27.3-39h-10.5v-8.7h10.5zm0 46.7h-10.5v-44h10.5zm14.8 0h-10.7v-51.6h10.7zm15.2 7.4h-11.8v-6.7h5q1.1 0 1.1-0.8 0-0.4-0.1-0.8l-6.8-43h10l2.9 32.3 3.5-32.3h10.1l-8.1 46.2q-0.5 2.5-1.7 3.8-1.3 1.3-4.1 1.3zm44.9-7.4h-14v-51.5h14q5.6 0 8.1 2.7 2.6 2.8 2.6 9.1v2.2q0 3.7-1.3 5.9-1.3 2.3-3.9 3 3.4 0.8 4.6 4.1 1.2 3.2 1.2 7.9 0 5-0.9 8.7-1 3.8-3.4 5.9-2.5 2-7 2zm-3.9-43.5v11.4h2.1q1.4 0 1.8-1.1 0.4-1.1 0.4-2.7v-5.2q0-2.4-2.2-2.4zm1.1 34.5q4 0 4-3.8v-6.5q0-2.2-0.7-3.4-0.6-1.3-2.5-1.3h-1.9v14.9q0.7 0.1 1.1 0.1zm28.1 9h-10.7v-43.9h10.7v4.9q0.7-2.6 2.7-4 1.9-1.4 4.8-1.4v8.7q-1.3 0-3.1 0.3-1.7 0.3-3.1 0.8-1.3 0.5-1.3 1zm22.9 0.5q-13.1 0-13.1-13.6v-17.6q0-6.3 3.4-9.9 3.5-3.8 9.7-3.8 6.2 0 9.6 3.8 3.5 3.6 3.5 9.9v17.6q0 13.6-13.1 13.6zm0-8.1q1.3 0 1.9-0.9 0.5-1 0.5-2.4v-21.5q0-3.9-2.4-3.9-2.5 0-2.5 3.9v21.5q0 1.4 0.6 2.4 0.6 0.9 1.9 0.9zm25 8.1q-3.7 0-5.6-1.7-1.9-1.7-2.6-4.6-0.7-3-0.7-6.8 0-4 0.8-6.5 0.8-2.6 2.7-4.2 1.9-1.7 5.3-2.8l6.5-2.2v-4.6q0-3.5-2.3-3.5-2.1 0-2.1 2.9v2.6h-10.2q-0.1-0.2-0.1-0.6 0-0.4 0-0.8 0-6.6 3.1-9.3 3.1-2.8 9.8-2.8 3.5 0 6.3 1.3 2.8 1.2 4.4 3.7 1.7 2.4 1.7 6v33.4h-10.5v-5.2q-0.7 2.8-2.5 4.3-1.7 1.4-4 1.4zm4.1-8.1q1.3 0 1.8-1.1 0.5-1.1 0.5-2.4v-12.2q-2.2 0.9-3.4 2.2-1.2 1.3-1.2 3.9v5.7q0 3.9 2.3 3.9zm25.5 8.1q-3 0-4.8-1.1-1.8-1.1-2.7-3.1-0.8-1.9-1.1-4.6-0.3-2.6-0.3-5.6v-19.1q0-5.1 1.8-8.2 1.8-3.2 6.1-3.2 3.2 0 4.9 1.4 1.7 1.4 2.7 3.8v-12.3h10.6v51.5h-10.6v-4.6q-0.9 2.4-2.4 3.7-1.4 1.4-4.2 1.4zm4.1-8.2q1.5 0 1.9-1.2 0.6-1.2 0.6-4.3v-18.5q0-1.5-0.5-3-0.4-1.6-2-1.6-1.7 0-2.1 1.5-0.5 1.4-0.5 3.1v18.5q0 5.5 2.6 5.5zm30.4 8.2q-7.4 0-10.5-3.8-3.1-3.7-3.1-11.1v-13.5q0-5.5 1.2-9.2 1.2-3.6 4.1-5.5 2.9-1.8 8.1-1.8 3.7 0 6.6 1.3 2.9 1.3 4.5 3.8 1.7 2.5 1.7 6.1v6.7h-10.7v-6.1q0-1.6-0.4-2.6-0.5-1.1-1.9-1.1-2.6 0-2.6 3.7v21.3q0 1.4 0.6 2.5 0.6 1.1 1.9 1.1 1.4 0 1.9-1.1 0.6-1.1 0.6-2.6v-7.3h10.6v7.6q0 3.7-1.6 6.3-1.7 2.6-4.5 3.9-2.8 1.4-6.5 1.4zm24.3 0q-3.7 0-5.6-1.7-1.9-1.7-2.6-4.6-0.8-3-0.8-6.8 0-4 0.8-6.5 0.8-2.6 2.7-4.2 2-1.7 5.3-2.8l6.5-2.2v-4.6q0-3.5-2.3-3.5-2.1 0-2.1 2.9v2.6h-10.2q0-0.2 0-0.6 0-0.4 0-0.8 0-6.6 3.1-9.3 3.1-2.8 9.8-2.8 3.5 0 6.3 1.3 2.8 1.2 4.4 3.7 1.6 2.4 1.6 6v33.4h-10.4v-5.2q-0.8 2.8-2.5 4.3-1.8 1.4-4 1.4zm4.1-8.1q1.3 0 1.7-1.1 0.5-1.1 0.5-2.4v-12.2q-2.2 0.9-3.4 2.2-1.2 1.3-1.2 3.9v5.7q0 3.9 2.4 3.9zm28.9 8.1q-13 0-13-13.2v-3.5h10.5v5.2q0 1.5 0.6 2.3 0.6 0.9 1.9 0.9 2.3 0 2.3-3.4 0-2.9-1.2-4.3-1.2-1.4-2.9-2.8l-5.6-4.3q-2.7-2-4.1-4.3-1.3-2.3-1.3-6.4 0-3.7 1.8-6.2 1.8-2.5 4.7-3.7 3-1.2 6.5-1.2 12.8 0 12.8 12.8v0.8h-10.9v-1.7q0-1.3-0.5-2.5-0.4-1.3-1.7-1.3-2.2 0-2.2 2.4 0 2.4 1.8 3.7l6.5 4.8q3.1 2.2 5.1 5.2 2.1 3 2.1 8 0 6.2-3.5 9.5-3.5 3.2-9.7 3.2zm25.5 0q-4.3 0-5.8-1.8-1.4-1.8-1.4-5.5v-27.4h-3v-8h3v-9.3h10.1v9.3h3v8h-3v24.9q0 1.1 0.4 1.6 0.4 0.4 1.3 0.4 0.8 0 1.3-0.1v7.1q-0.3 0.1-2.2 0.5-1.8 0.3-3.7 0.3z",
    },
    null,
    -1
  ),
  Pf = [qf, kf];
function Ef(e, t) {
  return T(), H("svg", xf, Pf);
}
const Af = re(jf, [["render", Ef]]),
  Cf = {
    "page-header": "_page-header_1cxgx_1",
    "page-header-title": "_page-header-title_1cxgx_15",
  },
  Sf = {
    data() {
      return { styles: Cf };
    },
  };
function If(e, t, s, a, i, n) {
  const l = Af,
    c = Oa;
  return (
    T(),
    H(
      "header",
      { class: E(i.styles["page-header"]) },
      [
        Q(
          c,
          { to: "/", class: E(i.styles["page-header-title"]) },
          { default: Xe(() => [Q(l)]), _: 1 },
          8,
          ["class"]
        ),
      ],
      2
    )
  );
}
const Tf = re(Sf, [["render", If]]),
  Rf = {},
  Mf = { width: "24", height: "24", viewBox: "0 0 24 24" },
  Nf = k("title", null, "Logo Icon", -1),
  Of = k(
    "path",
    {
      d: "M2 24h2.948c1-.923 2.004-2 3.55-2 1.547 0 2.55 1.077 3.55 2h2.948l-6.498-6-6.498 6zm20-8.042c0 3.269-5.858 3.387-9.787 1.79-6.835-2.779-9.629-9.79-7.817-15.17.84-2.496 1.852-3.84 6.333-.922 1.101.716 2.27 1.649 3.437 2.722l-1.72 1.152c-7.717-7.009-6.992-2.036-.983 4.55 5.858 6.417 11.668 8.615 5.767.717l1.199-1.745c1.223 1.634 3.571 4.873 3.571 6.906zm-1.026-12.437c-.004.829-.68 1.497-1.508 1.492-.225-.001-.436-.056-.628-.146l-3.829 5.646c-.784-.555-1.994-1.768-2.548-2.554l5.682-3.77c-.104-.207-.169-.437-.168-.684.005-.829.68-1.497 1.507-1.492.828.005 1.497.68 1.492 1.508z",
    },
    null,
    -1
  ),
  Uf = [Nf, Of];
function Lf(e, t) {
  return T(), H("svg", Mf, Uf);
}
const Hf = re(Rf, [["render", Lf]]),
  Ff = "_navbar_19jec_1",
  Df = "_active_19jec_113",
  cn = {
    navbar: Ff,
    "navbar-toggle": "_navbar-toggle_19jec_1",
    "navbar-label": "_navbar-label_19jec_19",
    "navbar-label-icon": "_navbar-label-icon_19jec_33",
    "navbar-content": "_navbar-content_19jec_54",
    "navbar-list": "_navbar-list_19jec_60",
    "navbar-item": "_navbar-item_19jec_69",
    "navbar-dropdown-item": "_navbar-dropdown-item_19jec_81",
    active: Df,
    "navbar-active-path": "_navbar-active-path_19jec_120",
    "navbar-icons": "_navbar-icons_19jec_124",
  },
  zf = {
    props: { label: String, url: String, callback: Function, id: String },
    data() {
      return { styles: cn };
    },
  };
function Bf(e, t, s, a, i, n) {
  const l = Oa;
  return (
    T(),
    _e(
      l,
      {
        id: s.id,
        "active-class": i.styles.active,
        to: s.url,
        onClick: s.callback,
      },
      { default: Xe(() => [cs(ye(s.label), 1)]), _: 1 },
      8,
      ["id", "active-class", "to", "onClick"]
    )
  );
}
const Qf = re(zf, [["render", Bf]]),
  Vf = "_dropdown_q5jug_1",
  Wf = {
    dropdown: Vf,
    "dropdown-toggle": "_dropdown-toggle_q5jug_8",
    "dropdown-label": "_dropdown-label_q5jug_21",
    "dropdown-label-text": "_dropdown-label-text_q5jug_44",
    "dropdown-content": "_dropdown-content_q5jug_58",
  },
  Yf = {
    props: { animatedIconClass: String },
    setup() {
      const { buttons: e } = ve("data");
      return { buttons: e };
    },
    data() {
      return { styles: Wf, isOpen: !1 };
    },
    methods: {
      closeDropdown() {
        this.isOpen = !1;
      },
      handleChange(e) {
        this.isOpen = e.target.checked;
      },
    },
  },
  $f = ["checked"],
  Kf = k(
    "span",
    { class: "animated-icon-inner", title: "Arrow Icon" },
    [k("span"), k("span")],
    -1
  ),
  Jf = [Kf];
function Zf(e, t, s, a, i, n) {
  return (
    T(),
    H(
      "div",
      { class: E(i.styles.dropdown) },
      [
        k(
          "input",
          {
            id: "navbar-dropdown-toggle",
            type: "checkbox",
            class: E(i.styles["dropdown-toggle"]),
            checked: i.isOpen,
            onChange:
              t[0] || (t[0] = (...l) => n.handleChange && n.handleChange(...l)),
          },
          null,
          42,
          $f
        ),
        k(
          "label",
          {
            for: "navbar-dropdown-toggle",
            class: E(i.styles["dropdown-label"]),
          },
          [
            k(
              "span",
              { class: E(i.styles["dropdown-label-text"]) },
              ye(a.buttons.more.label),
              3
            ),
            k(
              "div",
              {
                class: E([
                  "animated-icon",
                  "arrow-icon",
                  "arrow",
                  s.animatedIconClass,
                ]),
              },
              Jf,
              2
            ),
          ],
          2
        ),
        k(
          "ul",
          {
            class: E(i.styles["dropdown-content"]),
            onClick:
              t[1] ||
              (t[1] = (...l) => n.closeDropdown && n.closeDropdown(...l)),
          },
          [Yi(e.$slots, "default")],
          2
        ),
      ],
      2
    )
  );
}
const Gf = re(Yf, [["render", Zf]]),
  Xf = {
    props: { callback: Function, id: String },
    setup() {
      const { content: e } = ve("data"),
        t = [],
        s = [];
      return (
        Object.keys(e).forEach(i => {
          e[i].priority === 1 ? t.push(i) : e[i].priority === 2 && s.push(i);
        }),
        { route: us(), content: e, navItems: t, dropdownItems: s }
      );
    },
    data() {
      return { styles: cn };
    },
  };
function eb(e, t, s, a, i, n) {
  const l = Qf,
    c = Gf;
  return (
    T(),
    H(
      "ul",
      { class: E(i.styles["navbar-list"]) },
      [
        (T(!0),
        H(
          ne,
          null,
          Fe(
            a.navItems,
            r => (
              T(),
              H(
                "li",
                { key: r, class: E(i.styles["navbar-item"]) },
                [
                  Q(
                    l,
                    {
                      id: `${s.id}-${r}-link`,
                      label: a.content[r].name,
                      url: a.content[r].url,
                      callback: s.callback,
                    },
                    null,
                    8,
                    ["id", "label", "url", "callback"]
                  ),
                ],
                2
              )
            )
          ),
          128
        )),
        a.dropdownItems.length > 0
          ? (T(),
            H(
              "li",
              { key: 0, class: E(i.styles["navbar-item"]) },
              [
                Q(
                  c,
                  { "animated-icon-class": i.styles["navbar-label-icon"] },
                  {
                    default: Xe(() => [
                      (T(!0),
                      H(
                        ne,
                        null,
                        Fe(
                          a.dropdownItems,
                          r => (
                            T(),
                            H(
                              "li",
                              {
                                key: r,
                                class: E([
                                  i.styles["navbar-item"],
                                  i.styles["navbar-dropdown-item"],
                                ]),
                              },
                              [
                                Q(
                                  l,
                                  {
                                    id: `${s.id}-${r}-link`,
                                    label: a.content[r].name,
                                    url: a.content[r].url,
                                    callback: s.callback,
                                  },
                                  null,
                                  8,
                                  ["id", "label", "url", "callback"]
                                ),
                              ],
                              2
                            )
                          )
                        ),
                        128
                      )),
                    ]),
                    _: 1,
                  },
                  8,
                  ["animated-icon-class"]
                ),
              ],
              2
            ))
          : Pe("", !0),
      ],
      2
    )
  );
}
const tb = re(Xf, [["render", eb]]),
  sb = {},
  ab = { width: "24", height: "24", viewBox: "0 0 24 24" },
  ib = k("title", null, "Facebook Icon", -1),
  nb = k(
    "path",
    {
      d: "M9 8h-3v4h3v12h5v-12h3.642l.358-4h-4v-1.667c0-.955.192-1.333 1.115-1.333h2.885v-5h-3.808c-3.596 0-5.192 1.583-5.192 4.615v3.385z",
    },
    null,
    -1
  ),
  lb = [ib, nb];
function rb(e, t) {
  return T(), H("svg", ab, lb);
}
const cb = re(sb, [["render", rb]]),
  ob = {},
  ub = { width: "24", height: "24", viewBox: "0 0 24 24" },
  mb = k("title", null, "Instagram Icon", -1),
  hb = k(
    "path",
    {
      d: "M11.984 16.815c2.596 0 4.706-2.111 4.706-4.707 0-1.409-.623-2.674-1.606-3.538-.346-.303-.735-.556-1.158-.748-.593-.27-1.249-.421-1.941-.421s-1.349.151-1.941.421c-.424.194-.814.447-1.158.749-.985.864-1.608 2.129-1.608 3.538 0 2.595 2.112 4.706 4.706 4.706zm.016-8.184c1.921 0 3.479 1.557 3.479 3.478 0 1.921-1.558 3.479-3.479 3.479s-3.479-1.557-3.479-3.479c0-1.921 1.558-3.478 3.479-3.478zm5.223.369h6.777v10.278c0 2.608-2.114 4.722-4.722 4.722h-14.493c-2.608 0-4.785-2.114-4.785-4.722v-10.278h6.747c-.544.913-.872 1.969-.872 3.109 0 3.374 2.735 6.109 6.109 6.109s6.109-2.735 6.109-6.109c.001-1.14-.327-2.196-.87-3.109zm2.055-9h-12.278v5h-1v-5h-1v5h-1v-4.923c-.346.057-.682.143-1 .27v4.653h-1v-4.102c-1.202.857-2 2.246-2 3.824v3.278h7.473c1.167-1.282 2.798-2 4.511-2 1.722 0 3.351.725 4.511 2h7.505v-3.278c0-2.608-2.114-4.722-4.722-4.722zm2.722 5.265c0 .406-.333.735-.745.735h-2.511c-.411 0-.744-.329-.744-.735v-2.53c0-.406.333-.735.744-.735h2.511c.412 0 .745.329.745.735v2.53z",
    },
    null,
    -1
  ),
  db = [mb, hb];
function gb(e, t) {
  return T(), H("svg", ub, db);
}
const pb = re(ob, [["render", gb]]),
  fb = {},
  bb = { width: "24", height: "24", viewBox: "0 0 24 24" },
  _b = k("title", null, "Twitter Icon", -1),
  yb = k(
    "path",
    {
      d: "M24 4.557c-.883.392-1.832.656-2.828.775 1.017-.609 1.798-1.574 2.165-2.724-.951.564-2.005.974-3.127 1.195-.897-.957-2.178-1.555-3.594-1.555-3.179 0-5.515 2.966-4.797 6.045-4.091-.205-7.719-2.165-10.148-5.144-1.29 2.213-.669 5.108 1.523 6.574-.806-.026-1.566-.247-2.229-.616-.054 2.281 1.581 4.415 3.949 4.89-.693.188-1.452.232-2.224.084.626 1.956 2.444 3.379 4.6 3.419-2.07 1.623-4.678 2.348-7.29 2.04 2.179 1.397 4.768 2.212 7.548 2.212 9.142 0 14.307-7.721 13.995-14.646.962-.695 1.797-1.562 2.457-2.549z",
    },
    null,
    -1
  ),
  vb = [_b, yb];
function wb(e, t) {
  return T(), H("svg", bb, vb);
}
const jb = re(fb, [["render", wb]]),
  Ec = {
    "icons-group": "_icons-group_9dqku_1",
    "icons-group-list": "_icons-group-list_9dqku_5",
    "icons-group-item": "_icons-group-item_9dqku_14",
    "group-icon": "_group-icon_9dqku_28",
    "group-icon-small": "_group-icon-small_9dqku_33",
    "group-icon-medium": "_group-icon-medium_9dqku_38",
  },
  xb = {
    props: { callback: Function, id: String },
    setup() {
      const { links: e } = ve("data");
      return { links: e };
    },
    data() {
      return { styles: Ec };
    },
  },
  qb = ["id", "href"],
  kb = ["id", "href"],
  Pb = ["id", "href"];
function Eb(e, t, s, a, i, n) {
  const l = cb,
    c = pb,
    r = jb;
  return (
    T(),
    H(
      "div",
      { class: E(i.styles["icons-group"]) },
      [
        k(
          "ul",
          { class: E(i.styles["icons-group-list"]) },
          [
            k(
              "li",
              { class: E(i.styles["icons-group-item"]) },
              [
                k(
                  "a",
                  {
                    id: `${s.id}-facebook`,
                    href: a.links.social.facebook.href,
                  },
                  [
                    k(
                      "div",
                      {
                        class: E([
                          i.styles["group-icon"],
                          i.styles["group-icon-small"],
                        ]),
                      },
                      [Q(l)],
                      2
                    ),
                  ],
                  8,
                  qb
                ),
              ],
              2
            ),
            k(
              "li",
              { class: E(i.styles["icons-group-item"]) },
              [
                k(
                  "a",
                  {
                    id: `${s.id}-instagram`,
                    href: a.links.social.instagram.href,
                  },
                  [
                    k(
                      "div",
                      {
                        class: E([
                          i.styles["group-icon"],
                          i.styles["group-icon-small"],
                        ]),
                      },
                      [Q(c)],
                      2
                    ),
                  ],
                  8,
                  kb
                ),
              ],
              2
            ),
            k(
              "li",
              { class: E(i.styles["icons-group-item"]) },
              [
                k(
                  "a",
                  { id: `${s.id}-twitter`, href: a.links.social.twitter.href },
                  [
                    k(
                      "div",
                      {
                        class: E([
                          i.styles["group-icon"],
                          i.styles["group-icon-small"],
                        ]),
                      },
                      [Q(r)],
                      2
                    ),
                  ],
                  8,
                  Pb
                ),
              ],
              2
            ),
          ],
          2
        ),
      ],
      2
    )
  );
}
const Ac = re(xb, [["render", Eb]]),
  Cc = {
    "page-navigation": "_page-navigation_1gp5f_1",
    "page-navigation-row": "_page-navigation-row_1gp5f_24",
    "page-navigation-column-left": "_page-navigation-column-left_1gp5f_38",
    "page-navigation-column-right": "_page-navigation-column-right_1gp5f_39",
    "page-navigation-logo": "_page-navigation-logo_1gp5f_47",
    "page-navigation-button": "_page-navigation-button_1gp5f_66",
    "nav-button": "_nav-button_1gp5f_80",
  };
function ei() {
  const e = window.innerHeight * 0.01;
  document.documentElement.style.setProperty("--vh", `${e}px`);
}
const Ab = {
    props: { callback: Function },
    setup() {
      const { content: e } = ve("data");
      return { route: us(), content: e };
    },
    data() {
      return { navbarStyles: cn, navStyles: Cc, isOpen: !1 };
    },
    mounted() {
      ei(), window.addEventListener("resize", ei);
    },
    unmounted() {
      window.removeEventListener("resize", ei);
    },
    methods: {
      handleClick() {
        this.isOpen = !1;
      },
      handleChange(e) {
        this.isOpen = e.target.checked;
      },
    },
  },
  Cb = ["id", "checked"],
  Sb = ["for"],
  Ib = k("span", { class: "visually-hidden" }, "Navbar Toggle", -1),
  Tb = k(
    "span",
    { class: "animated-icon-inner" },
    [k("span"), k("span"), k("span")],
    -1
  ),
  Rb = [Tb];
function Mb(e, t, s, a, i, n) {
  var o;
  const l = Hf,
    c = tb,
    r = Ac;
  return (
    T(),
    H(
      "div",
      { class: E(i.navbarStyles.navbar) },
      [
        k(
          "input",
          {
            id: i.navbarStyles["navbar-toggle"],
            type: "checkbox",
            checked: i.isOpen,
            onChange:
              t[0] || (t[0] = (...u) => n.handleChange && n.handleChange(...u)),
          },
          null,
          40,
          Cb
        ),
        k(
          "label",
          {
            for: i.navbarStyles["navbar-toggle"],
            class: E(i.navbarStyles["navbar-label"]),
          },
          [
            Ib,
            k(
              "div",
              {
                class: E([
                  i.navbarStyles["navbar-label-icon"],
                  "animated-icon",
                  "hamburger-icon",
                ]),
                title: "Hamburger Icon",
              },
              Rb,
              2
            ),
          ],
          10,
          Sb
        ),
        k(
          "button",
          {
            id: "home-link",
            class: E(i.navStyles["page-navigation-logo"]),
            onClick: t[1] || (t[1] = (...u) => s.callback && s.callback(...u)),
          },
          [Q(l)],
          2
        ),
        k(
          "div",
          { class: E(i.navbarStyles["navbar-active-path"]) },
          ye(
            ((o = a.content[a.route.path.split("/")[1]]) == null
              ? void 0
              : o.name) ?? ""
          ),
          3
        ),
        k(
          "div",
          { class: E(i.navbarStyles["navbar-content"]) },
          [
            Q(c, { id: "navbar-navlist", callback: n.handleClick }, null, 8, [
              "callback",
            ]),
            k(
              "div",
              { class: E(i.navbarStyles["navbar-icons"]) },
              [Q(r, { id: "navbar-social-icons" })],
              2
            ),
          ],
          2
        ),
      ],
      2
    )
  );
}
const Nb = re(Ab, [["render", Mb]]),
  Ob = {
    setup() {
      const { buttons: e } = ve("data");
      return { buttons: e };
    },
    data() {
      return { navStyles: Cc, buttonStyles: kc };
    },
    methods: {
      logIn() {
        console.log("logIn clicked!");
      },
      openSitemap() {
        uc("/");
      },
    },
  };
function Ub(e, t, s, a, i, n) {
  const l = Nb;
  return (
    T(),
    H(
      "nav",
      { class: E(i.navStyles["page-navigation"]), "aria-label": "main menu" },
      [
        k(
          "div",
          { class: E(i.navStyles["page-navigation-row"]) },
          [
            k(
              "div",
              { class: E(i.navStyles["page-navigation-column-left"]) },
              [Q(l, { callback: n.openSitemap }, null, 8, ["callback"])],
              2
            ),
            k(
              "div",
              { class: E(i.navStyles["page-navigation-column-right"]) },
              [
                k(
                  "button",
                  {
                    id: "login-button",
                    class: E([
                      i.buttonStyles.button,
                      i.buttonStyles["secondary-button"],
                      i.navStyles["nav-button"],
                    ]),
                    onClick:
                      t[0] || (t[0] = (...c) => n.logIn && n.logIn(...c)),
                  },
                  ye(a.buttons.login.label),
                  3
                ),
              ],
              2
            ),
          ],
          2
        ),
      ],
      2
    )
  );
}
const Lb = re(Ob, [["render", Ub]]),
  Hb = "_message_7ak19_1",
  Fb = "_open_7ak19_23",
  Db = {
    message: Hb,
    open: Fb,
    "message-close-button": "_message-close-button_7ak19_30",
    "message-close-button-icon": "_message-close-button-icon_7ak19_42",
    "message-header": "_message-header_7ak19_49",
    "message-body": "_message-body_7ak19_60",
    "message-description": "_message-description_7ak19_67",
  },
  zb = {
    props: { onClose: Function, message: Object },
    data() {
      return { styles: Db };
    },
  },
  Bb = k("span", { class: "animated-icon-inner" }, [k("span"), k("span")], -1),
  Qb = [Bb];
function Vb(e, t, s, a, i, n) {
  return (
    T(),
    H(
      "div",
      { class: E([i.styles.message, i.styles.open]) },
      [
        k(
          "button",
          {
            id: "close-message-link",
            class: E(i.styles["message-close-button"]),
            title: "Close Button",
            onClick: t[0] || (t[0] = (...l) => s.onClose && s.onClose(...l)),
          },
          [
            k(
              "div",
              {
                class: E([
                  i.styles["message-close-button-icon"],
                  "animated-icon",
                  "close-icon",
                  "hover",
                ]),
                title: "Close Icon",
              },
              Qb,
              2
            ),
          ],
          2
        ),
        s.message.title
          ? (T(),
            H(
              "header",
              { key: 0, class: E(i.styles["message-header"]) },
              [k("h2", null, ye(s.message.title), 1)],
              2
            ))
          : Pe("", !0),
        k(
          "section",
          { class: E(i.styles["message-body"]) },
          [
            k(
              "div",
              { class: E(i.styles["message-description"]) },
              ye(s.message.description),
              3
            ),
          ],
          2
        ),
      ],
      2
    )
  );
}
const Wb = re(zb, [["render", Vb]]),
  Yb = {
    data() {
      return { styles: Qs };
    },
  };
function $b(e, t, s, a, i, n) {
  return (
    T(),
    H("main", { class: E(i.styles["page-main"]) }, [Yi(e.$slots, "default")], 2)
  );
}
const Kb = re(Yb, [["render", $b]]),
  Jb = "_sitemap_heahz_1",
  Zb = "_active_heahz_21",
  Gb = {
    sitemap: Jb,
    active: Zb,
    "sitemap-list": "_sitemap-list_heahz_27",
    "sitemap-item": "_sitemap-item_heahz_35",
    "sitemap-header": "_sitemap-header_heahz_40",
    "sitemap-sublist": "_sitemap-sublist_heahz_46",
    "sitemap-subitem": "_sitemap-subitem_heahz_52",
  },
  Xb = {
    props: { onClick: Function },
    setup() {
      const { content: e } = ve("data"),
        s = Object.keys(e).reduce((a, i) => (a.push(i), a), []);
      return { content: e, navItems: s };
    },
    data() {
      return { styles: Gb };
    },
  };
function e_(e, t, s, a, i, n) {
  const l = Oa;
  return (
    T(),
    H(
      "div",
      { class: E(i.styles.sitemap) },
      [
        k(
          "ul",
          { class: E(i.styles["sitemap-list"]) },
          [
            (T(!0),
            H(
              ne,
              null,
              Fe(
                a.navItems,
                c => (
                  T(),
                  H(
                    "li",
                    {
                      key: `sitemap-page-${a.content[c].name}`,
                      class: E(i.styles["sitemap-item"]),
                    },
                    [
                      Q(
                        l,
                        {
                          to: a.content[c].url,
                          "active-class": i.styles.active,
                        },
                        {
                          default: Xe(() => [
                            k(
                              "h4",
                              { class: E(i.styles["sitemap-header"]) },
                              ye(a.content[c].name),
                              3
                            ),
                          ]),
                          _: 2,
                        },
                        1032,
                        ["to", "active-class"]
                      ),
                      k(
                        "ul",
                        { class: E(i.styles["sitemap-sublist"]) },
                        [
                          (T(!0),
                          H(
                            ne,
                            null,
                            Fe(
                              a.content[c].sections,
                              r => (
                                T(),
                                H(
                                  "li",
                                  {
                                    key: `sitemap-section${r.id}`,
                                    class: E(i.styles["sitemap-subitem"]),
                                  },
                                  [
                                    Q(
                                      l,
                                      { to: `${a.content[c].url}#${r.id}` },
                                      {
                                        default: Xe(() => [cs(ye(r.name), 1)]),
                                        _: 2,
                                      },
                                      1032,
                                      ["to"]
                                    ),
                                  ],
                                  2
                                )
                              )
                            ),
                            128
                          )),
                        ],
                        2
                      ),
                    ],
                    2
                  )
                )
              ),
              128
            )),
          ],
          2
        ),
      ],
      2
    )
  );
}
const t_ = re(Xb, [["render", e_]]),
  s_ = {},
  a_ = {
    id: "ayy1-icon",
    "clip-rule": "evenodd",
    "fill-rule": "evenodd",
    "stroke-linejoin": "round",
    "stroke-miterlimit": "2",
    viewBox: "0 0 24 24",
  },
  i_ = k("title", null, "Accessibility Icon", -1),
  n_ = k(
    "path",
    {
      d: "m12.002 2c5.518 0 9.998 4.48 9.998 9.998 0 5.517-4.48 9.997-9.998 9.997-5.517 0-9.997-4.48-9.997-9.997 0-5.518 4.48-9.998 9.997-9.998zm0 1.5c-4.69 0-8.497 3.808-8.497 8.498s3.807 8.497 8.497 8.497 8.498-3.807 8.498-8.497-3.808-8.498-8.498-8.498zm4.044 5.607c-.235 0-1.892.576-4.044.576-2.166 0-3.791-.576-4.044-.576-.379 0-.687.308-.687.687 0 .318.225.599.531.669.613.16 1.261.293 1.756.542.459.231.781.566.781 1.14 0 2.027-1.326 3.92-1.86 4.817 0 0 0 0-.001.001-.06.105-.092.224-.092.344 0 .379.308.687.688.687.183 0 .357-.072.488-.204.447-.449 1.333-1.784 1.738-2.429.201-.319.396-.621.706-.622.302.001.498.303.698.622.405.645 1.291 1.98 1.738 2.429.13.132.304.204.489.204.379 0 .687-.308.687-.687 0-.119-.031-.237-.098-.353 0-.001-.001-.001-.001-.002-.547-.919-1.854-2.778-1.854-4.807 0-.609.369-.956.851-1.186.519-.247 1.167-.362 1.682-.495.31-.071.536-.352.536-.67 0-.379-.309-.687-.688-.687zm-4.03-3.113c-.875 0-1.587.713-1.587 1.593 0 .879.712 1.592 1.587 1.592.876 0 1.586-.713 1.586-1.592 0-.88-.71-1.593-1.586-1.593z",
      "fill-rule": "nonzero",
    },
    null,
    -1
  ),
  l_ = [i_, n_];
function r_(e, t) {
  return T(), H("svg", a_, l_);
}
const c_ = re(s_, [["render", r_]]),
  o_ = {
    props: { callback: Function, id: String },
    data() {
      return { styles: Ec };
    },
  },
  u_ = ["id"];
function m_(e, t, s, a, i, n) {
  const l = c_;
  return (
    T(),
    H(
      "div",
      { class: E(i.styles["icons-group"]) },
      [
        k(
          "ul",
          { class: E(i.styles["icons-group-list"]) },
          [
            k(
              "li",
              { class: E(i.styles["icons-group-item"]) },
              [
                k(
                  "button",
                  {
                    id: `${s.id}-a11y`,
                    onClick:
                      t[0] || (t[0] = (...c) => s.callback && s.callback(...c)),
                  },
                  [
                    k(
                      "div",
                      {
                        class: E([
                          i.styles["group-icon"],
                          i.styles["group-icon-medium"],
                        ]),
                      },
                      [Q(l)],
                      2
                    ),
                  ],
                  8,
                  u_
                ),
              ],
              2
            ),
          ],
          2
        ),
      ],
      2
    )
  );
}
const h_ = re(o_, [["render", m_]]),
  d_ = "_label_12a70_26",
  g_ = {
    "toggle-outer": "_toggle-outer_12a70_1",
    "toggle-description": "_toggle-description_12a70_13",
    "toggle-container": "_toggle-container_12a70_17",
    label: d_,
    switch: "_switch_12a70_36",
  },
  p_ = {
    props: { id: String, label: String, onChange: Function, checked: Boolean },
    data() {
      return { styles: g_, isSelected: !1 };
    },
    mount() {
      this.isSelected = this.checked;
    },
    methods: {
      handleChange(e) {
        (this.isSelected = e.target.checked), this.onChange(e);
      },
    },
  },
  f_ = ["for"],
  b_ = ["id", "checked"],
  __ = { class: "visually-hidden" };
function y_(e, t, s, a, i, n) {
  return (
    T(),
    H(
      "div",
      { class: E(i.styles["toggle-outer"]) },
      [
        k("div", { class: E(i.styles["toggle-description"]) }, ye(s.label), 3),
        k(
          "div",
          { class: E(i.styles["toggle-container"]) },
          [
            k(
              "label",
              { class: E(i.styles.label), for: `${s.id}-toggle` },
              [
                k(
                  "input",
                  {
                    id: `${s.id}-toggle`,
                    type: "checkbox",
                    checked: i.isSelected,
                    onChange:
                      t[0] ||
                      (t[0] = (...l) => n.handleChange && n.handleChange(...l)),
                  },
                  null,
                  40,
                  b_
                ),
                k("span", { class: E(i.styles.switch) }, null, 2),
                k(
                  "div",
                  __,
                  "selected: " + ye(i.isSelected ? "true" : "false"),
                  1
                ),
              ],
              10,
              f_
            ),
          ],
          2
        ),
      ],
      2
    )
  );
}
const v_ = re(p_, [["render", y_]]),
  w_ = "_dialog_1b8ms_1",
  j_ = "_open_1b8ms_21",
  x_ = {
    dialog: w_,
    open: j_,
    "dialog-close-button": "_dialog-close-button_1b8ms_28",
    "dialog-close-button-icon": "_dialog-close-button-icon_1b8ms_40",
    "dialog-header": "_dialog-header_1b8ms_47",
    "dialog-body": "_dialog-body_1b8ms_58",
    "dialog-item": "_dialog-item_1b8ms_65",
  },
  q_ = {
    props: { onClose: Function },
    setup() {
      const { settings: e } = ve("data");
      return { settings: e };
    },
    data() {
      return { styles: x_, reduceMotion: !1 };
    },
    mounted() {
      this.reduceMotion =
        document.documentElement.classList.contains("reduced-motion");
    },
    methods: {
      toggleMotion(e) {
        (this.reduceMotion = e.target.checked),
          e.target.checked
            ? document.documentElement.classList.add("reduced-motion")
            : document.documentElement.classList.remove("reduced-motion");
      },
    },
  },
  k_ = k("span", { class: "animated-icon-inner" }, [k("span"), k("span")], -1),
  P_ = [k_];
function E_(e, t, s, a, i, n) {
  const l = v_;
  return (
    T(),
    H(
      "div",
      { id: "settings", class: E([i.styles.dialog, i.styles.open]) },
      [
        k(
          "button",
          {
            id: "close-dialog-link",
            class: E(i.styles["dialog-close-button"]),
            title: "Close Button",
            onClick: t[0] || (t[0] = (...c) => s.onClose && s.onClose(...c)),
          },
          [
            k(
              "div",
              {
                class: E([
                  i.styles["dialog-close-button-icon"],
                  "animated-icon",
                  "close-icon",
                  "hover",
                ]),
                title: "Close Icon",
              },
              P_,
              2
            ),
          ],
          2
        ),
        k(
          "header",
          { class: E(i.styles["dialog-header"]) },
          [k("h2", null, ye(a.settings.header), 1)],
          2
        ),
        k(
          "section",
          { class: E(i.styles["dialog-body"]) },
          [
            k(
              "div",
              { class: E(i.styles["dialog-item"]) },
              [
                Q(
                  l,
                  {
                    id: "motion",
                    label: a.settings.items.motion.label,
                    "on-change": n.toggleMotion,
                    checked: i.reduceMotion,
                  },
                  null,
                  8,
                  ["label", "on-change", "checked"]
                ),
              ],
              2
            ),
          ],
          2
        ),
      ],
      2
    )
  );
}
const A_ = re(q_, [["render", E_]]),
  C_ = {
    "page-footer": "_page-footer_18lt6_1",
    "footer-row": "_footer-row_18lt6_26",
    "footer-column-left": "_footer-column-left_18lt6_44",
    "footer-column-center": "_footer-column-center_18lt6_45",
    "footer-column-right": "_footer-column-right_18lt6_46",
    "footer-links": "_footer-links_18lt6_66",
    "footer-links-list": "_footer-links-list_18lt6_66",
    "footer-links-item": "_footer-links-item_18lt6_75",
  },
  S_ = {
    setup() {
      const { footer: e, links: t } = ve("data");
      return { footer: e, links: t };
    },
    data() {
      return { styles: C_, showPortal: !1 };
    },
    methods: {
      openPortal() {
        this.showPortal = !0;
      },
      closePortal() {
        this.showPortal = !1;
      },
    },
  },
  I_ = ["id", "href"];
function T_(e, t, s, a, i, n) {
  const l = t_,
    c = Ac,
    r = h_,
    o = A_;
  return (
    T(),
    H(
      ne,
      null,
      [
        k(
          "footer",
          { class: E(i.styles["page-footer"]) },
          [
            k(
              "div",
              { class: E(i.styles["footer-row"]) },
              [
                k(
                  "div",
                  { class: E(i.styles["footer-column-center"]) },
                  [Q(l)],
                  2
                ),
              ],
              2
            ),
            k(
              "div",
              { class: E(i.styles["footer-row"]) },
              [
                k(
                  "div",
                  { class: E(i.styles["footer-column-center"]) },
                  [
                    k(
                      "div",
                      { class: E(i.styles["footer-links"]) },
                      [
                        k(
                          "ul",
                          { class: E(i.styles["footer-links-list"]) },
                          [
                            (T(!0),
                            H(
                              ne,
                              null,
                              Fe(
                                a.links.legal,
                                (u, m) => (
                                  T(),
                                  H(
                                    "li",
                                    {
                                      key: `footer-links-item-${m}`,
                                      class: E(i.styles["footer-links-item"]),
                                    },
                                    [
                                      k(
                                        "a",
                                        {
                                          id: `footer-link-${m}`,
                                          href: u.href,
                                          class: E(i.styles["footer-link"]),
                                        },
                                        ye(u.label),
                                        11,
                                        I_
                                      ),
                                    ],
                                    2
                                  )
                                )
                              ),
                              128
                            )),
                          ],
                          2
                        ),
                      ],
                      2
                    ),
                  ],
                  2
                ),
              ],
              2
            ),
            k(
              "div",
              { class: E(i.styles["footer-row"]) },
              [
                k(
                  "div",
                  { class: E(i.styles["footer-column-left"]) },
                  [Q(c, { id: "footer-social-icons" })],
                  2
                ),
                k(
                  "div",
                  { class: E(i.styles["footer-column-center"]) },
                  "© " + ye(new Date().getFullYear()) + " No Rights Reserved",
                  3
                ),
                k(
                  "div",
                  { class: E(i.styles["footer-column-right"]) },
                  [
                    Q(
                      r,
                      { id: "footer-settings-icons", callback: n.openPortal },
                      null,
                      8,
                      ["callback"]
                    ),
                  ],
                  2
                ),
              ],
              2
            ),
          ],
          2
        ),
        (T(),
        _e(Tr, { to: "body" }, [
          Vi(Q(o, { "on-close": n.closePortal }, null, 8, ["on-close"]), [
            [en, i.showPortal],
          ]),
        ])),
      ],
      64
    )
  );
}
const R_ = re(S_, [["render", T_]]),
  M_ = {
    __name: "Layout",
    setup(e) {
      const t = Qe(!1),
        s = us(),
        { content: a, links: i } = ve("data");
      Hs(() => {
        t.value = a[s.name].message;
      });
      const n = () => {
        t.value = !1;
      };
      return (l, c) => {
        const r = Oa,
          o = Tf,
          u = Lb,
          m = Wb,
          d = Kb,
          _ = R_;
        return (
          T(),
          H(
            ne,
            null,
            [
              Q(
                r,
                { to: `${pe(s).path}#content`, class: "skip-link" },
                { default: Xe(() => [cs(ye(pe(i).a11y.skip.label), 1)]), _: 1 },
                8,
                ["to"]
              ),
              k(
                "div",
                { id: "page", class: E(pe(Qs).page) },
                [
                  Q(o),
                  Q(u),
                  pe(a)[pe(s).name].message
                    ? Vi(
                        (T(),
                        _e(
                          m,
                          {
                            key: 0,
                            "on-close": n,
                            message: pe(a)[pe(s).name].message,
                          },
                          null,
                          8,
                          ["message"]
                        )),
                        [[en, t.value]]
                      )
                    : Pe("", !0),
                  Q(d, null, {
                    default: Xe(() => [Yi(l.$slots, "default")]),
                    _: 3,
                  }),
                  Q(_),
                ],
                2
              ),
            ],
            64
          )
        );
      };
    },
  },
  N_ = {
    home: {
      name: "Front Page",
      url: "/",
      priority: 0,
      notification: {
        name: "cookies",
        title: "This website uses cookies 🍪",
        description:
          "We use cookies to improve your experience on our site and to show you the most relevant content possible. To find out more, please read our privacy policy and our cookie policy.",
        actions: [
          { name: "Cancel", priority: "secondary", type: "reject" },
          { name: "Accept", priority: "primary", type: "accept" },
        ],
      },
      sections: [
        {
          id: "content-frontpage-breaking-news",
          name: "Breaking News",
          articles: [
            {
              class: "columns-3-narrow",
              header: "Uncensored",
              url: "#",
              image: {
                src: "assets/images/isai-ramos-Sp70YIWtuM8-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Nisl nunc mi ipsum faucibus vitae aliquet.",
              type: "text",
              content: `Velit dignissim sodales ut eu. Sed tempus urna et pharetra. Porttitor rhoncus dolor purus non. Elementum curabitur vitae nunc sed velit dignissim sodales.

Pretium fusce id velit ut tortor pretium viverra suspendisse potenti. In nulla posuere sollicitudin aliquam ultrices sagittis orci. Aliquam sem fringilla ut morbi tincidunt augue interdum velit. Nisl nunc mi ipsum faucibus vitae aliquet nec ullamcorper. Nunc mi ipsum faucibus vitae aliquet.`,
            },
            {
              class: "columns-3-wide",
              header: "More top stories",
              url: "#",
              image: {
                src: "assets/images/nasa-dCgbRAQmTQA-unsplash_684.jpg",
                alt: "Placeholder",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "Photo taken by someone.",
                tag: { type: "breaking", label: "breaking" },
              },
              title:
                "Justo eget magna fermentum iaculis eu non diam phasellus vestibulum.",
              type: "text",
              content: `Pulvinar etiam non quam lacus suspendisse faucibus interdum posuere. Arcu bibendum at varius vel pharetra vel turpis nunc. Eget dolor morbi non arcu risus quis varius. Ac odio tempor orci dapibus ultrices in.

Amet tellus cras adipiscing enim eu turpis. Tortor pretium viverra suspendisse potenti nullam. Condimentum vitae sapien pellentesque habitant morbi. Ultrices in iaculis nunc sed augue lacus viverra vitae.`,
            },
            {
              class: "columns-3-narrow",
              header: "Crime & justice",
              url: "#",
              image: {
                src: "assets/images/jordhan-madec-AD5ylD2T0UY-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Eu sem integer vitae justo eget magna fermentum iaculis.",
              type: "text",
              content: `Volutpat commodo sed egestas egestas. Eget lorem dolor sed viverra ipsum nunc aliquet bibendum enim. Felis eget velit aliquet sagittis id consectetur purus. Lorem ipsum dolor sit amet. Ut diam quam nulla porttitor. Id volutpat lacus laoreet non.

 Odio morbi quis commodo odio aenean sed adipiscing diam donec. Quis eleifend quam adipiscing vitae proin sagittis nisl. Praesent semper feugiat nibh sed pulvinar proin gravida hendrerit lectus.`,
            },
          ],
        },
        {
          id: "content-frontpage-latest-news",
          name: "Latest News",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Happening Now",
              type: "articles-list",
              content: [
                {
                  title: "Lorem ipsum dolor sit amet.",
                  content:
                    "Molestie nunc non blandit massa enim nec. Ornare suspendisse sed nisi lacus sed viverra tellus in. Id consectetur purus ut faucibus. At auctor urna nunc id cursus metus. Eget aliquet nibh praesent tristique magna. Morbi tristique senectus et netus et malesuada fames.",
                },
                {
                  title: "Consectetur adipiscing elit.",
                  content:
                    "Sit amet consectetur adipiscing elit ut aliquam purus sit. Consequat nisl vel pretium lectus quam. Sagittis id consectetur purus ut faucibus pulvinar elementum integer enim. Nec sagittis aliquam malesuada bibendum arcu.",
                },
                {
                  title: "Sed do eiusmod tempor incididunt.",
                  content:
                    "Pulvinar neque laoreet suspendisse interdum consectetur libero id faucibus nisl. Pulvinar elementum integer enim neque volutpat ac. Lorem donec massa sapien faucibus.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Noteworthy",
              image: {
                src: "assets/images/peter-lawrence-rXZa4ufjoGw-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Augue neque gravida in fermentum et sollicitudin ac orci.",
              type: "list",
              content: [
                {
                  content:
                    "Odio morbi quis commodo odio aenean sed adipiscing diam donec.",
                },
                {
                  content:
                    "Consequat semper viverra nam libero justo laoreet sit.",
                },
                {
                  content:
                    "Risus ultricies tristique nulla aliquet enim tortor at auctor.",
                },
                {
                  content:
                    "Diam vulputate ut pharetra sit amet aliquam id diam maecenas.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Around the Globe",
              image: {
                src: "assets/images/rufinochka-XonjCOZZN_w-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Nunc felis tellus, ultrices eget massa ac, lobortis laoreet lorem.",
              type: "list",
              content: [
                {
                  content:
                    "Nibh mauris cursus mattis molestie. Varius vel pharetra vel turpis nunc eget lorem dolor.",
                },
                {
                  content:
                    "Turpis egestas maecenas pharetra convallis posuere morbi leo urna molestie.",
                },
                {
                  content:
                    "Enim blandit volutpat maecenas volutpat blandit aliquam etiam erat.",
                },
                {
                  content:
                    "Fermentum dui faucibus in ornare. In hac habitasse platea dictumst vestibulum rhoncus est pellentesque elit.",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-latest-media",
          name: "Latest Media",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/steven-van-bTPP3jBnOb8-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-WUehAgqO5hE-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/david-everett-strickler-igCBFrMd11I-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/marco-oriolesi-wqLGlhjr6Og-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-highlights",
          name: "Highlights",
          articles: [
            {
              class: "columns-wrap",
              header: "Domestic Highlights",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/samuel-schroth-hyPt63Df3Dw-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "At urna condimentum mattis pellentesque id nibh tortor id. Urna cursus eget nunc scelerisque viverra mauris in. Pretium vulputate sapien nec sagittis aliquam malesuada bibendum arcu.",
                },
                {
                  image: {
                    src: "assets/images/denys-nevozhai-7nrsVjvALnA-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Enim lobortis scelerisque fermentum dui faucibus in. Vitae semper quis lectus nulla at volutpat. In nisl nisi scelerisque eu ultrices vitae auctor.",
                },
                {
                  image: {
                    src: "assets/images/mattia-bericchia-xkD79yf4tb8-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Lorem donec massa sapien faucibus et molestie ac feugiat. Quis varius quam quisque id diam vel. Ut tristique et egestas quis ipsum suspendisse. Fermentum posuere urna nec tincidunt praesent semper feugiat.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Global Highlights",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maximilian-bungart-nwqfl_HtJjk-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Augue eget arcu dictum varius duis at consectetur. Ornare arcu dui vivamus arcu felis bibendum ut. Magna eget est lorem ipsum dolor sit amet. Tincidunt nunc pulvinar sapien et ligula ullamcorper malesuada proin.",
                },
                {
                  image: {
                    src: "assets/images/gaku-suyama-VyiLZUcdJv0-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Leo urna molestie at elementum eu facilisis sed. Est lorem ipsum dolor sit amet consectetur adipiscing elit pellentesque.",
                },
                {
                  image: {
                    src: "assets/images/paul-bill-HLuPjCa6IYw-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nisi scelerisque eu ultrices vitae auctor. Quis risus sed vulputate odio. Pellentesque sit amet porttitor eget dolor morbi non. Nullam eget felis eget nunc lobortis mattis aliquam.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Local Highlights",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maarten-van-den-heuvel-gZXx8lKAb7Y-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Mattis ullamcorper velit sed ullamcorper. Orci ac auctor augue mauris augue neque. Condimentum mattis pellentesque id nibh tortor.",
                },
                {
                  image: {
                    src: "assets/images/quino-al-KydWCDJe9s0-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Fermentum odio eu feugiat pretium. Urna nec tincidunt praesent semper feugiat nibh sed. Adipiscing elit ut aliquam purus sit.",
                },
                {
                  image: {
                    src: "assets/images/mathurin-napoly-matnapo-pIJ34ZrZEEw-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Vitae tempus quam pellentesque nec nam aliquam sem et. Fringilla urna porttitor rhoncus dolor purus non enim praesent elementum. Congue nisi vitae suscipit tellus mauris a diam maecenas. Quis varius quam quisque id diam.",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-top-stories",
          name: "Top Stories",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/andrew-solok-LbckXdUVOlY-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Ut venenatis tellus in metus vulputate eu scelerisque. In nulla posuere sollicitudin aliquam ultrices sagittis orci a scelerisque. Mattis nunc sed blandit libero volutpat sed cras ornare arcu. Scelerisque eu ultrices vitae auctor eu augue. Libero justo laoreet sit amet cursus sit amet.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/hassan-kibwana-fmXLB_uHIh4-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Non consectetur a erat nam. Blandit massa enim nec dui nunc mattis enim ut. Tempor orci eu lobortis elementum nibh tellus molestie nunc. Facilisi etiam dignissim diam quis enim lobortis scelerisque fermentum dui.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/craig-manners-LvJCFOW3Ma8-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Eget est lorem ipsum dolor sit amet. Vivamus at augue eget arcu dictum varius duis at consectetur. Scelerisque fermentum dui faucibus in ornare quam viverra orci sagittis. Vitae sapien pellentesque habitant morbi tristique senectus et.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/albert-stoynov-fEdf0fig3os-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Diam in arcu cursus euismod quis viverra nibh cras pulvinar. Est velit egestas dui id ornare arcu odio ut sem. A cras semper auctor neque. Ipsum suspendisse ultrices gravida dictum fusce ut.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ehimetalor-akhere-unuabona-yS0uBoF4xDo-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Tellus integer feugiat scelerisque varius morbi enim. Diam donec adipiscing tristique risus nec feugiat in fermentum. Volutpat odio facilisis mauris sit amet massa vitae. Tempor orci dapibus ultrices in iaculis nunc sed. Aenean vel elit scelerisque mauris pellentesque pulvinar.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-international",
          name: "International",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Europe",
              type: "articles-list",
              content: [
                {
                  title:
                    "Commodo elit at imperdiet dui accumsan sit amet. Habitasse platea dictumst vestibulum rhoncus.",
                  content:
                    "Orci ac auctor augue mauris augue neque gravida. Lectus magna fringilla urna porttitor rhoncus dolor purus non enim. Sagittis aliquam malesuada bibendum arcu vitae. Pellentesque habitant morbi tristique senectus et netus. Etiam erat velit scelerisque in dictum non consectetur a.",
                },
                {
                  title:
                    "Suspendisse convallis efficitur felis ac mattis. Cras faucibus ultrices condimentum.",
                  content:
                    "Facilisis leo vel fringilla est. Turpis tincidunt id aliquet risus feugiat in ante metus. Viverra ipsum nunc aliquet bibendum enim facilisis. Tristique et egestas quis ipsum suspendisse ultrices gravida dictum. Tristique senectus et netus et malesuada fames ac turpis egestas.",
                },
                {
                  title:
                    "Ornare suspendisse sed nisi lacus sed viverra tellus in.",
                  content:
                    "Dui vivamus arcu felis bibendum. Purus ut faucibus pulvinar elementum integer enim neque volutpat ac. Auctor eu augue ut lectus arcu bibendum. Diam volutpat commodo sed egestas egestas fringilla phasellus.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "South America",
              type: "articles-list",
              content: [
                {
                  title: "Augue eget arcu dictum varius duis.",
                  content:
                    "Commodo ullamcorper a lacus vestibulum sed arcu non. Nullam ac tortor vitae purus faucibus ornare suspendisse sed. Id interdum velit laoreet id donec ultrices tincidunt arcu non.",
                },
                {
                  title:
                    "Fringilla ut morbi tincidunt augue interdum velit euismod in pellentesque.",
                  content:
                    "Turpis egestas maecenas pharetra convallis posuere morbi leo. Odio pellentesque diam volutpat commodo. Ornare massa eget egestas purus viverra accumsan in nisl nisi. Tellus integer feugiat scelerisque varius morbi enim nunc. Erat velit scelerisque in dictum non consectetur.",
                },
                {
                  title: "Mi bibendum neque egestas congue quisque.",
                  content:
                    "Sapien eget mi proin sed libero. Adipiscing elit duis tristique sollicitudin nibh sit. Faucibus scelerisque eleifend donec pretium. Ac tortor dignissim convallis aenean et tortor at risus.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Asia",
              type: "articles-list",
              content: [
                {
                  title:
                    "Sodales ut etiam sit amet nisl purus in. Enim sed faucibus turpis in eu mi bibendum neque.",
                  content:
                    "Tortor id aliquet lectus proin. Pulvinar elementum integer enim neque volutpat ac tincidunt. Auctor eu augue ut lectus arcu bibendum at varius. Congue mauris rhoncus aenean vel elit scelerisque mauris.",
                },
                {
                  title: "haretra convallis posuere morbi leo urna.",
                  content:
                    "Egestas diam in arcu cursus euismod quis. Ac turpis egestas integer eget aliquet nibh praesent tristique magna. Molestie at elementum eu facilisis sed odio morbi quis. Lectus arcu bibendum at varius. Eros in cursus turpis massa tincidunt dui.",
                },
                {
                  title:
                    "At varius vel pharetra vel turpis nunc eget lorem dolor. ",
                  content:
                    "Proin sagittis nisl rhoncus mattis rhoncus urna neque viverra. Lacus sed viverra tellus in. Sed nisi lacus sed viverra tellus in. Venenatis cras sed felis eget velit aliquet sagittis id consectetur.",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-featured",
          name: "Featured",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Washington",
              image: {
                src: "assets/images/heidi-kaden-L_U4jhwZ6hY-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Et netus et malesuada fames ac.",
              type: "list",
              display: "bullets",
              content: [
                {
                  content: "Vulputate dignissim suspendisse in est ante.",
                  url: "#",
                },
                {
                  content:
                    "Blandit turpis cursus in hac habitasse platea dictumst.",
                  url: "#",
                },
                {
                  content: "Sed nisi lacus sed viverra tellus in hac.",
                  url: "#",
                },
                {
                  content:
                    "Euismod in pellentesque massa placerat duis ultricies lacus sed.",
                  url: "#",
                },
                {
                  content: "Quam lacus suspendisse faucibus interdum posuere.",
                  url: "#",
                },
                {
                  content:
                    "Sit amet mattis vulputate enim nulla aliquet porttitor lacus.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "New York",
              image: {
                src: "assets/images/hannah-busing-0V6DmTuJaIk-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Commodo quis imperdiet massa tincidunt nunc pulvinar sapien et ligula.",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "Id semper risus in hendrerit gravida rutrum quisque non.",
                  url: "#",
                },
                {
                  content:
                    "Sit amet est placerat in egestas erat imperdiet sed euismod.",
                  url: "#",
                },
                {
                  content:
                    "Aliquam malesuada bibendum arcu vitae elementum curabitur vitae nunc.",
                  url: "#",
                },
                {
                  content:
                    "get gravida cum sociis natoque. Bibendum ut tristique et egestas.",
                  url: "#",
                },
                {
                  content: "Mauris cursus mattis molestie a iaculis at erat.",
                  url: "#",
                },
                {
                  content: "Sit amet massa vitae tortor condimentum lacinia.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Los Angeles",
              image: {
                src: "assets/images/martin-jernberg-jVNWCFwdjZU-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Parturient montes nascetur ridiculus mus mauris.",
              type: "list",
              display: "bullets",
              content: [
                {
                  content: "Mattis enim ut tellus elementum sagittis.",
                  url: "#",
                },
                {
                  content:
                    "Sit amet venenatis urna cursus eget nunc scelerisque viverra mauris.",
                  url: "#",
                },
                {
                  content: "Mi bibendum neque egestas congue quisque egestas.",
                  url: "#",
                },
                {
                  content: "Nunc scelerisque viverra mauris in aliquam.",
                  url: "#",
                },
                {
                  content:
                    "Egestas erat imperdiet sed euismod nisi porta lorem mollis aliquam.",
                  url: "#",
                },
                {
                  content:
                    "Phasellus egestas tellus rutrum tellus pellentesque eu tincidunt tortor aliquam.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-underscored",
          name: "Underscored",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/kevin-wang-t7vEVxwGGm0-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Rhoncus urna neque viverra justo nec. Dis parturient montes nascetur ridiculus mus mauris vitae ultricies leo. Praesent semper feugiat nibh sed pulvinar proin gravida hendrerit lectus. Enim nunc faucibus a pellentesque sit amet. Est ullamcorper eget nulla facilisi.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/claudio-schwarz-3cWxxW2ggKE-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Enim lobortis scelerisque fermentum dui faucibus in ornare quam. Iaculis urna id volutpat lacus laoreet non curabitur gravida. Non quam lacus suspendisse faucibus. Elit ullamcorper dignissim cras tincidunt lobortis feugiat vivamus at. Bibendum est ultricies integer quis auctor elit.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/braden-collum-9HI8UJMSdZA-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Faucibus scelerisque eleifend donec pretium vulputate. Lacus luctus accumsan tortor posuere. Nulla facilisi nullam vehicula ipsum a arcu cursus vitae. Viverra aliquet eget sit amet tellus cras adipiscing. Congue quisque egestas diam in arcu cursus.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/geoff-scott-8lUTnkZXZSA-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Cum sociis natoque penatibus et magnis dis parturient montes. Ut eu sem integer vitae justo eget magna fermentum iaculis. Amet venenatis urna cursus eget nunc scelerisque viverra. Quisque id diam vel quam elementum. Nulla facilisi cras fermentum odio eu feugiat pretium nibh.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-happening-now",
          name: "Happening Now",
          articles: [
            {
              class: "columns-wrap",
              header: "Political",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/jonathan-simcoe-S9J1HqoL9ns-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Cras semper auctor neque vitae tempus quam pellentesque. Consequat ac felis donec et odio pellentesque. Eu consequat ac felis donec et odio pellentesque diam volutpat. Suscipit tellus mauris a diam maecenas sed enim ut sem.",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-p2Xor4Lbrrk-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Sed faucibus turpis in eu mi bibendum neque. Condimentum id venenatis a condimentum vitae sapien pellentesque habitant morbi. In iaculis nunc sed augue lacus viverra. Pellentesque nec nam aliquam sem et. Tellus mauris a diam maecenas sed.",
                },
                {
                  image: {
                    src: "assets/images/marius-oprea-ySA9uj7zSmw-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Mattis vulputate enim nulla aliquet. Ac tortor dignissim convallis aenean. Nulla posuere sollicitudin aliquam ultrices sagittis orci a scelerisque. Consequat ac felis donec et odio pellentesque diam. Lorem ipsum dolor sit amet consectetur adipiscing.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Health",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/jannis-brandt-mmsQUgMLqUo-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Vitae tortor condimentum lacinia quis. Nisl nisi scelerisque eu ultrices vitae. Id velit ut tortor pretium viverra suspendisse potenti nullam. Viverra accumsan in nisl nisi scelerisque eu ultrices vitae.",
                },
                {
                  image: {
                    src: "assets/images/martha-dominguez-de-gouveia-k-NnVZ-z26w-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Ullamcorper malesuada proin libero nunc consequat. Imperdiet sed euismod nisi porta. Arcu cursus vitae congue mauris rhoncus aenean vel. Enim nunc faucibus a pellentesque. Gravida in fermentum et sollicitudin ac orci phasellus.",
                },
                {
                  image: {
                    src: "assets/images/freestocks-nss2eRzQwgw-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Morbi tristique senectus et netus et malesuada fames. Sit amet cursus sit amet dictum sit. Sagittis vitae et leo duis ut diam quam. Non consectetur a erat nam at lectus. Massa massa ultricies mi quis hendrerit dolor magna eget est.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Business",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/little-plant-TZw891-oMio-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Integer enim neque volutpat ac. Feugiat sed lectus vestibulum mattis. Ullamcorper malesuada proin libero nunc consequat interdum varius sit amet. Mattis molestie a iaculis at erat pellentesque. Adipiscing elit duis tristique sollicitudin.",
                },
                {
                  image: {
                    src: "assets/images/allan-wadsworth-Lp78NT-mf9o-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Dignissim sodales ut eu sem integer. Mauris cursus mattis molestie a iaculis at erat. Tempus quam pellentesque nec nam aliquam sem et tortor. Id diam vel quam elementum pulvinar etiam non quam.",
                },
                {
                  image: {
                    src: "assets/images/ant-rozetsky-SLIFI67jv5k-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Massa vitae tortor condimentum lacinia quis vel eros. Platea dictumst vestibulum rhoncus est pellentesque. Sollicitudin tempor id eu nisl nunc mi ipsum faucibus vitae. Sed risus ultricies tristique nulla aliquet. Magna sit amet purus gravida quis blandit turpis cursus in.",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-hot-topics",
          name: "Hot Topics",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/alexandre-debieve-FO7JIlwjOtU-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Amet nisl suscipit adipiscing bibendum. Elit ullamcorper dignissim cras tincidunt lobortis feugiat. Non odio euismod lacinia at. Risus viverra adipiscing at in tellus integer feugiat scelerisque.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/thisisengineering-ZPeXrWxOjRQ-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Viverra suspendisse potenti nullam ac tortor. Tellus id interdum velit laoreet id donec. Dui nunc mattis enim ut tellus. Nec ullamcorper sit amet risus nullam eget felis eget. Viverra suspendisse potenti nullam ac tortor vitae purus faucibus.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/isaw-company-Oqv_bQbZgS8-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Commodo ullamcorper a lacus vestibulum sed arcu non odio euismod. Etiam non quam lacus suspendisse. Hac habitasse platea dictumst vestibulum rhoncus est.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/aditya-chinchure-ZhQCZjr9fHo-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Mi eget mauris pharetra et ultrices neque ornare aenean euismod. Egestas congue quisque egestas diam in arcu cursus euismod quis. Tincidunt id aliquet risus feugiat. Viverra nibh cras pulvinar mattis nunc sed.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-paid-content",
          name: "Paid Content",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/tamara-bellis-IwVRO3TLjLc-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Nunc aliquet bibendum enim facilisis gravida neque. Nec feugiat in fermentum posuere urna. Molestie at elementum eu facilisis sed odio morbi. Scelerisque purus semper eget duis at tellus.",
                },
                {
                  image: {
                    src: "assets/images/david-lezcano-NfZiOJzZgcg-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Eget dolor morbi non arcu risus quis. Non curabitur gravida arcu ac tortor dignissim.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/heidi-fin-2TLREZi7BUg-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Quam lacus suspendisse faucibus interdum. In pellentesque massa placerat duis ultricies lacus sed. Convallis a cras semper auctor neque vitae tempus quam. Ut pharetra sit amet aliquam id diam.",
                },
                {
                  image: {
                    src: "assets/images/joshua-rawson-harris-YNaSz-E7Qss-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Vel fringilla est ullamcorper eget nulla facilisi etiam dignissim diam. Eu feugiat pretium nibh ipsum consequat.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/arturo-rey-5yP83RhaFGA-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Non tellus orci ac auctor augue mauris augue neque gravida. Nulla facilisi nullam vehicula ipsum a arcu cursus vitae. Quam nulla porttitor massa id neque aliquam vestibulum morbi. Diam quis enim lobortis scelerisque.",
                },
                {
                  image: {
                    src: "assets/images/clem-onojeghuo-RLJnH4Mt9A0-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Haretra diam sit amet nisl suscipit adipiscing bibendum est ultricies. Senectus et netus et malesuada fames.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/ashim-d-silva-ZmgJiztRHXE-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "It amet porttitor eget dolor morbi non. Sed lectus vestibulum mattis ullamcorper. Laoreet id donec ultrices tincidunt arcu non. Quam adipiscing vitae proin sagittis.",
                },
                {
                  image: {
                    src: "assets/images/toa-heftiba--abWByT3yg4-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Mollis aliquam ut porttitor leo a diam. Nunc aliquet bibendum enim facilisis gravida neque convallis.",
                },
              ],
            },
          ],
        },
      ],
    },
    us: {
      name: "US",
      url: "/us",
      priority: 1,
      message: {
        title: "Watch breaking news!",
        description: "Something important happened and you should watch it!",
      },
      sections: [
        {
          id: "content-us-world-news",
          name: "World News",
          articles: [
            {
              class: "columns-3-wide",
              header: "Happening Today",
              url: "#",
              image: {
                src: "assets/images/todd-trapani-vS54KomBEJU-unsplash_684.jpg",
                alt: "Placeholder",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "Photo taken by someone.",
                tag: { type: "breaking", label: "breaking" },
              },
              title:
                "Sed egestas egestas fringilla phasellus faucibus scelerisque eleifend.",
              type: "text",
              content:
                "Iaculis urna id volutpat lacus. Dictumst vestibulum rhoncus est pellentesque elit ullamcorper. Dictum varius duis at consectetur lorem donec. At tellus at urna condimentum mattis pellentesque id. Consectetur lorem donec massa sapien faucibus et molestie ac. Risus at ultrices mi tempus.",
            },
            {
              class: "columns-3-narrow",
              header: "Trending",
              url: "#",
              image: {
                src: "assets/images/mufid-majnun-tJJIGh703I4-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Ut eu sem integer vitae justo eget magna.",
              type: "text",
              content: `Id neque aliquam vestibulum morbi blandit cursus risus at ultrices. Arcu dui vivamus arcu felis bibendum ut tristique et. Justo donec enim diam vulputate ut.

Pellentesque elit ullamcorper dignissim cras tincidunt lobortis feugiat vivamus at. Ipsum suspendisse ultrices gravida dictum fusce ut placerat. Convallis tellus id interdum velit laoreet id.`,
            },
            {
              class: "columns-3-narrow",
              header: "Weather",
              url: "#",
              image: {
                src: "assets/images/noaa--urO88VoCRE-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Id consectetur purus ut faucibus pulvinar elementum integer enim.",
              type: "list",
              content: [
                {
                  content:
                    "Pellentesque habitant morbi tristique senectus et. Vel eros donec ac odio tempor orci dapibus ultrices in.",
                },
                {
                  content:
                    "Et odio pellentesque diam volutpat commodo sed egestas egestas fringilla.",
                },
                {
                  content:
                    "Et netus et malesuada fames ac turpis egestas. Maecenas ultricies mi eget mauris pharetra et ultrices.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-around-the-nation",
          name: "Around the Nation",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Latest",
              image: {
                src: "assets/images/fons-heijnsbroek-vBfEZdpEr-E-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Nullam eget felis eget nunc lobortis mattis aliquam.",
              type: "list",
              content: [
                {
                  content:
                    "Nibh ipsum consequat nisl vel. Senectus et netus et malesuada fames.",
                },
                {
                  content:
                    "Lectus vestibulum mattis ullamcorper velit sed ullamcorper morbi.",
                },
                {
                  content:
                    "Blandit volutpat maecenas volutpat blandit aliquam etiam erat.",
                },
                {
                  content:
                    "Non curabitur gravida arcu ac. Est sit amet facilisis magna etiam tempor orci eu lobortis.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Business",
              image: {
                src: "assets/images/bram-naus-oqnVnI5ixHg-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Vestibulum rhoncus est pellentesque elit. Enim lobortis scelerisque fermentum dui faucibus.",
              type: "list",
              content: [
                {
                  content:
                    "Sapien pellentesque habitant morbi tristique senectus et.",
                },
                { content: "Aliquet eget sit amet tellus cras adipiscing." },
                {
                  content:
                    "Tellus mauris a diam maecenas sed enim ut sem viverra.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Politics",
              image: {
                src: "assets/images/hansjorg-keller-CQqyv5uldW4-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Hendrerit dolor magna eget est. Nec dui nunc mattis enim ut tellus elementum sagittis.",
              type: "list",
              content: [
                {
                  content:
                    "Euismod elementum nisi quis eleifend quam adipiscing vitae proin sagittis.",
                },
                {
                  content:
                    "Ac tincidunt vitae semper quis lectus nulla at volutpat diam.",
                },
                {
                  content:
                    "In mollis nunc sed id semper risus in hendrerit. Turpis massa sed elementum tempus egestas sed sed risus. Imperdiet proin fermentum leo vel orci.",
                },
                {
                  content:
                    "Nisl purus in mollis nunc sed id semper. Pretium lectus quam id leo in vitae.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-roundup",
          name: "Roundup",
          articles: [
            {
              class: "columns-wrap",
              header: "Washington",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/unseen-histories-4kYkKW8v8rY-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nisl nisi scelerisque eu ultrices vitae. Consectetur adipiscing elit duis tristique sollicitudin. Ornare suspendisse sed nisi lacus. Justo eget magna fermentum iaculis.",
                },
                {
                  image: {
                    src: "assets/images/ian-hutchinson-P8rgDtEFn7s-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Tellus integer feugiat scelerisque varius morbi enim. Ut tristique et egestas quis.",
                },
                {
                  image: {
                    src: "assets/images/koshu-kunii-ADLj1cyFfV8-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nulla malesuada pellentesque elit eget gravida cum sociis natoque penatibus.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "East Coast",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/matthew-landers-v8UgmRa6UDg-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Pharetra et ultrices neque ornare aenean euismod elementum nisi. Ipsum dolor sit amet consectetur adipiscing elit ut.",
                },
                {
                  image: {
                    src: "assets/images/c-j-1GHqOftzYo0-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Quam vulputate dignissim suspendisse in est. Vestibulum mattis ullamcorper velit sed.",
                },
                {
                  image: {
                    src: "assets/images/jacob-licht-8nA_iHrxHIo-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Habitant morbi tristique senectus et netus et. Ullamcorper sit amet risus nullam eget felis.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "West Coast",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maria-lysenko-tZvkSuBleso-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Bibendum enim facilisis gravida neque convallis a cras. Semper feugiat nibh sed pulvinar proin gravida hendrerit.",
                },
                {
                  image: {
                    src: "assets/images/peter-thomas-17EJD0QdKFI-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Vel facilisis volutpat est velit. Odio ut sem nulla pharetra diam sit amet nisl.",
                },
                {
                  image: {
                    src: "assets/images/xan-griffin-QxNkzEjB180-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Risus nec feugiat in fermentum posuere urna nec. Massa tincidunt nunc pulvinar sapien.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-crime+justice",
          name: "Crime & Justice",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Supreme Court",
              type: "articles-list",
              content: [
                {
                  title: "Vel risus commodo viverra maecenas.",
                  content:
                    "Vitae tempus quam pellentesque nec nam aliquam sem. Mi in nulla posuere sollicitudin aliquam ultrices sagittis. Leo integer malesuada nunc vel. Ultricies integer quis auctor elit sed vulputate. Sit amet justo donec enim diam vulputate. Velit aliquet sagittis id consectetur purus ut faucibus pulvinar.",
                },
                {
                  title: "Sit amet mattis vulputate enim.",
                  content:
                    "Urna porttitor rhoncus dolor purus non. Tristique senectus et netus et malesuada fames ac turpis egestas. Suscipit tellus mauris a diam maecenas. Risus ultricies tristique nulla aliquet enim. Quis imperdiet massa tincidunt nunc pulvinar sapien et ligula ullamcorper.",
                },
                {
                  title: "Mauris in aliquam sem fringilla ut morbi tincidunt.",
                  content:
                    "A erat nam at lectus. Orci sagittis eu volutpat odio facilisis mauris sit. Faucibus nisl tincidunt eget nullam non. Nisl condimentum id venenatis a. Suscipit tellus mauris a diam maecenas sed enim. Orci nulla pellentesque dignissim enim sit amet venenatis. Est ultricies integer quis auctor.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Local Law",
              type: "articles-list",
              content: [
                {
                  title: "Sit amet justo donec enim diam vulputate ut.",
                  content:
                    "Tincidunt dui ut ornare lectus sit amet est. Risus sed vulputate odio ut enim blandit volutpat maecenas volutpat. Posuere urna nec tincidunt praesent semper feugiat nibh sed pulvinar. Euismod in pellentesque massa placerat duis.",
                },
                {
                  title:
                    "Aliquam ultrices sagittis orci a scelerisque purus semper eget duis.",
                  content:
                    "Lobortis feugiat vivamus at augue eget arcu. Id ornare arcu odio ut sem nulla pharetra diam. Mauris in aliquam sem fringilla ut morbi tincidunt augue interdum. Congue quisque egestas diam in arcu cursus euismod quis viverra.",
                },
                {
                  title:
                    "In metus vulputate eu scelerisque felis imperdiet proin.",
                  content:
                    "Elementum pulvinar etiam non quam. Id nibh tortor id aliquet lectus proin nibh. Elementum facilisis leo vel fringilla est ullamcorper eget. Dictum sit amet justo donec enim diam vulputate.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Opinion",
              type: "articles-list",
              content: [
                {
                  title: "Magna ac placerat vestibulum lectus.",
                  content:
                    "enenatis urna cursus eget nunc scelerisque viverra mauris. Convallis posuere morbi leo urna molestie at elementum. Eu lobortis elementum nibh tellus. Vitae purus faucibus ornare suspendisse sed nisi lacus sed viverra.",
                },
                {
                  title:
                    "Nisl rhoncus mattis rhoncus urna neque viverra justo.",
                  content:
                    "Tristique sollicitudin nibh sit amet. Aliquam purus sit amet luctus venenatis. Vitae nunc sed velit dignissim sodales ut. Elit scelerisque mauris pellentesque pulvinar pellentesque habitant morbi tristique senectus. Sit amet risus nullam eget.",
                },
                {
                  title:
                    "Sed felis eget velit aliquet sagittis id consectetur purus ut.",
                  content:
                    "Egestas erat imperdiet sed euismod nisi porta. Vel orci porta non pulvinar neque laoreet. Urna condimentum mattis pellentesque id nibh. Arcu non sodales neque sodales ut etiam sit amet. Elementum curabitur vitae nunc sed velit dignissim.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-around-the-us",
          name: "Around the US",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Latest",
              image: {
                src: "assets/images/chloe-taranto-x2zyAOmVNtM-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Ut tortor pretium viverra suspendisse potenti nullam ac tortor.",
              type: "list",
              content: [
                {
                  content:
                    "Erat pellentesque adipiscing commodo elit at. Ornare lectus sit amet est placerat in.",
                },
                {
                  content:
                    "Dui ut ornare lectus sit amet est placerat in egestas. Commodo sed egestas egestas fringilla phasellus.",
                },
                {
                  content:
                    "Mi quis hendrerit dolor magna eget est lorem ipsum. Urna molestie at elementum eu facilisis sed odio morbi.",
                },
                {
                  content:
                    "Mauris rhoncus aenean vel elit scelerisque mauris pellentesque pulvinar.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Business",
              image: {
                src: "assets/images/razvan-chisu-Ua-agENjmI4-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Nam at lectus urna duis convallis convallis tellus id. Sem nulla pharetra diam sit amet nisl.",
              type: "list",
              content: [
                {
                  content:
                    "Nunc faucibus a pellentesque sit amet. Id velit ut tortor pretium viverra suspendisse potenti nullam ac.",
                },
                {
                  content:
                    "Eget mi proin sed libero enim sed. A scelerisque purus semper eget duis at tellus.",
                },
                {
                  content:
                    "Praesent tristique magna sit amet purus. Eros in cursus turpis massa.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Politics",
              image: {
                src: "assets/images/colin-lloyd-2ULmNrj44QY-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Tristique nulla aliquet enim tortor at auctor urna nunc.",
              type: "list",
              content: [
                {
                  content:
                    "Tincidunt ornare massa eget egestas purus viverra accumsan in nisl. Amet mattis vulputate enim nulla.",
                },
                {
                  content:
                    "Pellentesque massa placerat duis ultricies. Tortor at auctor urna nunc id cursus.",
                },
                {
                  content:
                    "Venenatis urna cursus eget nunc scelerisque viverra mauris.",
                },
                {
                  content:
                    "Dolor morbi non arcu risus quis varius quam quisque id.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-latest-media",
          name: "Latest Media",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/dominic-hampton-_8aRumOixtI-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/sam-mcghee-4siwRamtFAk-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/adam-whitlock-I9j8Rk-JYFM-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/leah-hetteberg-kTVN2l0ZUv8-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-us-business",
          name: "Business",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Local",
              type: "articles-list",
              content: [
                {
                  title:
                    "Sed viverra tellus in hac habitasse platea dictumst vestibulum.",
                  content:
                    "Maecenas volutpat blandit aliquam etiam. Diam volutpat commodo sed egestas egestas fringilla phasellus faucibus scelerisque. Est ullamcorper eget nulla facilisi etiam dignissim diam quis. Tincidunt praesent semper feugiat nibh sed pulvinar proin gravida hendrerit. Varius vel pharetra vel turpis nunc eget. Enim ut tellus elementum sagittis vitae et leo duis.",
                },
                {
                  title: "Porttitor leo a diam sollicitudin tempor id eu nisl.",
                  content:
                    "Ut diam quam nulla porttitor massa id neque. Nulla facilisi etiam dignissim diam quis enim lobortis. Quam nulla porttitor massa id. Neque ornare aenean euismod elementum nisi quis eleifend quam adipiscing. Justo nec ultrices dui sapien eget mi. Volutpat diam ut venenatis tellus in. Mi in nulla posuere sollicitudin aliquam ultrices.",
                },
                {
                  title: "Leo vel orci porta non pulvinar neque laoreet.",
                  content:
                    "Placerat duis ultricies lacus sed. Pellentesque adipiscing commodo elit at imperdiet dui. Accumsan lacus vel facilisis volutpat. Condimentum lacinia quis vel eros donec ac. Pellentesque habitant morbi tristique senectus. Ultrices eros in cursus turpis massa tincidunt dui ut ornare. Rhoncus urna neque viverra justo nec ultrices dui sapien. Amet venenatis urna cursus eget.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Global",
              type: "articles-list",
              content: [
                {
                  title:
                    "Platea dictumst quisque sagittis purus sit amet volutpat consequat mauris.",
                  content:
                    "Eu lobortis elementum nibh tellus molestie nunc. Vel turpis nunc eget lorem dolor sed viverra. Massa sapien faucibus et molestie ac feugiat sed. Sed egestas egestas fringilla phasellus faucibus. At erat pellentesque adipiscing commodo elit at imperdiet dui accumsan",
                },
                {
                  title:
                    "Ultrices gravida dictum fusce ut placerat orci nulla pellentesque.",
                  content:
                    "Velit ut tortor pretium viverra suspendisse potenti nullam ac tortor. Feugiat nibh sed pulvinar proin gravida. Feugiat in fermentum posuere urna nec tincidunt praesent. Nulla posuere sollicitudin aliquam ultrices sagittis orci a scelerisque. A scelerisque purus semper eget.",
                },
                {
                  title: "Est ullamcorper eget nulla facilisi etiam.",
                  content:
                    "Augue mauris augue neque gravida in fermentum et. Ornare arcu odio ut sem nulla pharetra diam. Tristique et egestas quis ipsum suspendisse ultrices gravida. Aliquam vestibulum morbi blandit cursus risus at ultrices mi. Non blandit massa enim nec dui nunc mattis.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Quarterly",
              type: "articles-list",
              content: [
                {
                  title: "Non curabitur gravida arcu ac tortor dignissim.",
                  content:
                    "Dui nunc mattis enim ut. Non consectetur a erat nam. Arcu vitae elementum curabitur vitae nunc sed velit dignissim. Congue quisque egestas diam in arcu cursus euismod quis viverra. Consequat semper viverra nam libero justo laoreet sit amet.",
                },
                {
                  title: "Velit egestas dui id ornare arcu odio ut.",
                  content:
                    "At ultrices mi tempus imperdiet nulla malesuada pellentesque elit eget. Aenean et tortor at risus viverra. Lectus magna fringilla urna porttitor rhoncus dolor. Posuere lorem ipsum dolor sit amet consectetur adipiscing elit. Euismod in pellentesque massa placerat duis ultricies lacus sed turpis.",
                },
                {
                  title:
                    "Malesuada nunc vel risus commodo viverra maecenas accumsan lacus vel.",
                  content:
                    "Nunc eget lorem dolor sed. Amet aliquam id diam maecenas ultricies mi. Sodales ut etiam sit amet nisl purus. Consectetur adipiscing elit ut aliquam purus sit amet luctus venenatis. Fusce ut placerat orci nulla pellentesque dignissim enim sit.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-underscored",
          name: "Underscored",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/dillon-kydd-2keCPb73aQY-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Netus et malesuada fames ac turpis egestas. Habitasse platea dictumst vestibulum rhoncus est pellentesque elit ullamcorper dignissim. Morbi tempus iaculis urna id volutpat lacus laoreet non curabitur. Sed enim ut sem viverra. Tellus integer feugiat scelerisque varius morbi enim.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/aaron-huber-G7sE2S4Lab4-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Aenean vel elit scelerisque mauris. Et ligula ullamcorper malesuada proin libero nunc. Mi sit amet mauris commodo quis imperdiet. Elit ullamcorper dignissim cras tincidunt lobortis feugiat. Erat velit scelerisque in dictum non consectetur a erat nam. Orci porta non pulvinar neque.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/mesut-kaya-eOcyhe5-9sQ-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Eget gravida cum sociis natoque penatibus et. Malesuada pellentesque elit eget gravida cum. Curabitur vitae nunc sed velit dignissim sodales ut. Curabitur vitae nunc sed velit dignissim. Vel pretium lectus quam id leo in. Aliquet lectus proin nibh nisl condimentum id venenatis a.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/diego-jimenez-A-NVHPka9Rk-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Tristique senectus et netus et malesuada fames ac turpis. Semper risus in hendrerit gravida rutrum. Urna cursus eget nunc scelerisque viverra. Amet mauris commodo quis imperdiet massa. Erat nam at lectus urna duis convallis convallis tellus id.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-state-by-state",
          name: "State by state",
          articles: [
            {
              class: "columns-wrap",
              header: "California",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/craig-melville-_JKymnZ1Uc4-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Et tortor at risus viverra adipiscing at. Leo urna molestie at elementum eu facilisis sed. Adipiscing tristique risus nec feugiat in fermentum posuere urna.",
                },
                {
                  image: {
                    src: "assets/images/robert-bye-EILw-nEK46k-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Luctus venenatis lectus magna fringilla. Condimentum mattis pellentesque id nibh tortor id. Rhoncus aenean vel elit scelerisque mauris pellentesque.",
                },
                {
                  image: {
                    src: "assets/images/sapan-patel-gmgWd0CgWQI-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Feugiat scelerisque varius morbi enim nunc. Amet consectetur adipiscing elit ut aliquam purus sit amet luctus. Orci a scelerisque purus semper eget duis at tellus at.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "New York",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/prince-abban-0OUHhvNIbYc-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Vitae sapien pellentesque habitant morbi tristique. Quisque id diam vel quam elementum pulvinar etiam non. Hendrerit gravida rutrum quisque non tellus orci.",
                },
                {
                  image: {
                    src: "assets/images/quick-ps-sW41y3lETZk-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Rhoncus dolor purus non enim praesent. Massa enim nec dui nunc mattis. Odio eu feugiat pretium nibh ipsum consequat. Bibendum enim facilisis gravida neque convallis a cras.",
                },
                {
                  image: {
                    src: "assets/images/lorenzo-moschi-N7ypjB7HKIk-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Cursus euismod quis viverra nibh. Facilisis mauris sit amet massa. Eget mauris pharetra et ultrices. Vitae turpis massa sed elementum tempus egestas sed. Semper viverra nam libero justo.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Washington",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/koshu-kunii-v9ferChkC9A-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Iaculis nunc sed augue lacus viverra. Sed libero enim sed faucibus turpis in. Massa tincidunt dui ut ornare. Adipiscing bibendum est ultricies integer quis auctor elit.",
                },
                {
                  image: {
                    src: "assets/images/angela-loria-hFc0JEKD4Cc-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Aliquet nec ullamcorper sit amet risus nullam eget felis eget. Tortor dignissim convallis aenean et tortor at risus. Dolor sed viverra ipsum nunc.",
                },
                {
                  image: {
                    src: "assets/images/harold-mendoza-6xafY_AE1LM-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "In cursus turpis massa tincidunt dui ut ornare. Lacus vestibulum sed arcu non odio euismod lacinia at. Mi ipsum faucibus vitae aliquet nec. Commodo sed egestas egestas fringilla phasellus faucibus scelerisque eleifend.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-hot-topics",
          name: "Hot Topics",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/libre-leung-9O0Sp22DF0I-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Magna ac placerat vestibulum lectus mauris ultrices eros. Risus nullam eget felis eget nunc. Orci porta non pulvinar neque. Aliquam purus sit amet luctus venenatis lectus magna fringilla urna. In arcu cursus euismod quis viverra nibh.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/pascal-bullan-M8sQPAfhPdk-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Id venenatis a condimentum vitae sapien. Dui vivamus arcu felis bibendum ut tristique. Laoreet sit amet cursus sit amet dictum sit amet justo. Id semper risus in hendrerit gravida rutrum quisque non. Posuere sollicitudin aliquam ultrices sagittis orci a scelerisque.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/brooke-lark-HjWzkqW1dgI-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Nulla porttitor massa id neque aliquam. Amet massa vitae tortor condimentum lacinia quis vel. Semper quis lectus nulla at volutpat diam ut venenatis. In nulla posuere sollicitudin aliquam ultrices.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/matthias-heil-lDOEwat_MPs-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Egestas congue quisque egestas diam in arcu cursus. Vitae tempus quam pellentesque nec nam aliquam. Proin nibh nisl condimentum id. Mattis ullamcorper velit sed ullamcorper morbi tincidunt. Egestas integer eget aliquet nibh praesent tristique.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-paid-content",
          name: "Paid Content",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/tadeusz-lakota-Tb38UzCvKCY-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Mi tempus imperdiet nulla malesuada pellentesque elit eget gravida cum. Nec tincidunt praesent semper feugiat nibh sed pulvinar proin.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/leisara-studio-EzzW1oNek-I-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Sed cras ornare arcu dui vivamus arcu. Blandit aliquam etiam erat velit scelerisque in. Nisl rhoncus mattis rhoncus urna neque viverra.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/tamanna-rumee-lpGm415q9JA-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Nunc sed id semper risus in hendrerit gravida rutrum. Ac felis donec et odio pellentesque diam volutpat commodo sed.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/clark-street-mercantile-P3pI6xzovu0-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Semper quis lectus nulla at volutpat diam ut venenatis tellus. Felis eget nunc lobortis mattis aliquam faucibus purus in massa. Et malesuada fames ac turpis.",
                },
              ],
            },
          ],
        },
      ],
    },
    world: {
      name: "World",
      url: "/world",
      priority: 1,
      sections: [
        {
          id: "content-world-global-trends",
          name: "Global trends",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Africa",
              url: "#",
              image: {
                src: "assets/images/will-shirley-xRKcHoCOA4Y-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Sed id semper risus in hendrerit gravida. Sagittis orci a scelerisque purus semper eget duis at tellus.",
              type: "text",
              content:
                "Quam viverra orci sagittis eu volutpat odio facilisis mauris sit. Magna fringilla urna porttitor rhoncus dolor purus non enim praesent. Pellentesque sit amet porttitor eget dolor morbi non arcu risus. Dictum varius duis at consectetur. Ut porttitor leo a diam sollicitudin tempor id eu nisl.",
            },
            {
              class: "columns-3-balanced",
              header: "China",
              url: "#",
              image: {
                src: "assets/images/nuno-alberto-MykFFC5zolE-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Convallis aenean et tortor at risus. Pellentesque elit eget gravida cum sociis natoque penatibus.",
              type: "text",
              content:
                "Auctor urna nunc id cursus metus aliquam. Amet commodo nulla facilisi nullam. Blandit massa enim nec dui nunc mattis enim ut. Et netus et malesuada fames ac turpis. Pellentesque habitant morbi tristique senectus et netus et malesuada. Habitant morbi tristique senectus et netus et malesuada fames ace.",
            },
            {
              class: "columns-3-balanced",
              header: "Russia",
              url: "#",
              image: {
                src: "assets/images/nikita-karimov-lvJZhHOIJJ4-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Pharetra magna ac placerat vestibulum lectus mauris ultrices eros.",
              type: "list",
              content: [
                {
                  content:
                    "Luctus venenatis lectus magna fringilla urna porttitor rhoncus.",
                },
                {
                  content:
                    "Placerat orci nulla pellentesque dignissim enim sit amet venenatis.",
                },
                { content: "Pellentesque nec nam aliquam sem et." },
                { content: "In hendrerit gravida rutrum quisque non tellus." },
              ],
            },
          ],
        },
        {
          id: "content-world-around-the-world",
          name: "Around the world",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Europe",
              image: {
                src: "assets/images/azhar-j-t2hgHV1R7_g-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Porttitor massa id neque aliquam vestibulum. Semper auctor neque vitae tempus quam.",
              type: "text",
              content:
                "Metus vulputate eu scelerisque felis imperdiet proin fermentum leo vel. Nisi scelerisque eu ultrices vitae auctor eu. Risus pretium quam vulputate dignissim suspendisse. Pulvinar neque laoreet suspendisse interdum. Mauris cursus mattis molestie a iaculis at erat.",
            },
            {
              class: "columns-3-balanced",
              header: "Middle East",
              image: {
                src: "assets/images/adrian-dascal-myAz-buELXs-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Et molestie ac feugiat sed lectus vestibulum mattis.",
              type: "text",
              content:
                "Suspendisse interdum consectetur libero id faucibus nisl tincidunt eget nullam. Cursus vitae congue mauris rhoncus aenean vel elit scelerisque mauris. Quam vulputate dignissim suspendisse in est ante in nibh mauris.",
            },
            {
              class: "columns-3-balanced",
              header: "Asia",
              image: {
                src: "assets/images/mike-enerio-7ryPpZK1qV8-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Metus dictum at tempor commodo.",
              type: "list",
              content: [
                { content: "Id faucibus nisl tincidunt eget nullam non nisi." },
                { content: "Lectus quam id leo in vitae turpis massa." },
                {
                  content:
                    "Urna nec tincidunt praesent semper feugiat nibh sed. Sed turpis tincidunt id aliquet risus.",
                },
                { content: "Eu ultrices vitae auctor eu augue ut lectus." },
              ],
            },
          ],
        },
        {
          id: "content-world-latest-media",
          name: "Latest Media",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/greg-rakozy-oMpAz-DN-9I-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/annie-spratt-KiOHnBkLQQU-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/noaa-Led9c1SSNFo-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/paul-hanaoka-s0XabTAKvak-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-world-today",
          name: "Today",
          articles: [
            {
              class: "columns-3-wide",
              header: "Unrest",
              url: "#",
              image: {
                src: "assets/images/venti-views-KElJx4R4Py8-unsplash_684.jpg",
                alt: "Placeholder",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "Photo taken by someone.",
                tag: { type: "breaking", label: "breaking" },
              },
              title:
                "Viverra aliquet eget sit amet. In fermentum posuere urna nec.",
              type: "list",
              content: [
                {
                  content:
                    "Massa enim nec dui nunc mattis. Ornare lectus sit amet est placerat in.",
                },
                {
                  content:
                    "Morbi tristique senectus et netus et malesuada fames ac turpis.",
                },
                {
                  content:
                    "Fed vulputate mi sit amet mauris commodo quis imperdiet massa.",
                },
                {
                  content:
                    "In egestas erat imperdiet sed euismod nisi porta lorem mollis. Scelerisque eu ultrices vitae auctor eu augue ut lectus arcu.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "Happening now",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/koshu-kunii-cWEGNQqcImk-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Semper auctor neque vitae tempus quam pellentesque nec nam aliquam.",
                },
                {
                  image: {
                    src: "assets/images/kenny-K72n3BHgHCg-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Viverra maecenas accumsan lacus vel facilisis volutpat.",
                },
                {
                  image: {
                    src: "assets/images/kitthitorn-chaiyuthapoom-TOH_gw5dd20-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title: "Orci sagittis eu volutpat odio facilisis mauris sit.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "Noteworthy",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/olga-guryanova-tMFeatBSS4s-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Nunc aliquet bibendum enim facilisis gravida neque convallis a.",
                },
                {
                  image: {
                    src: "assets/images/jed-owen-ajZibDGpPew-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Ut diam quam nulla porttitor massa id neque aliquam vestibulum.",
                },
                {
                  image: {
                    src: "assets/images/noaa-FY3vXNBl1v4-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Magna fermentum iaculis eu non diam phasellus vestibulum lorem.",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-featured",
          name: "Featured",
          articles: [
            {
              class: "columns-3-balanced",
              header: "European Union",
              image: {
                src: "assets/images/christian-lue-8Yw6tsB8tnc-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Luctus venenatis lectus magna fringilla urna.",
              type: "list",
              content: [
                {
                  content:
                    "Nulla facilisi cras fermentum odio eu. Porttitor lacus luctus accumsan tortor posuere ac ut.",
                },
                {
                  content:
                    "Phasellus egestas tellus rutrum tellus pellentesque eu tincidunt. Leo vel orci porta non. Sem nulla pharetra diam sit amet nisl.",
                },
                {
                  content:
                    "Justo donec enim diam vulputate ut pharetra sit amet aliquam. Eu consequat ac felis donec et.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Britain",
              image: {
                src: "assets/images/ian-taylor-kAWTCt7p7rs-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Orci a scelerisque purus semper eget duis.",
              type: "text",
              content: `Gravida rutrum quisque non tellus orci ac auctor augue mauris. Enim ut sem viverra aliquet eget. Sit amet volutpat consequat mauris nunc congue nisi vitae.

Praesent tristique magna sit amet purus gravida quis blandit turpis. Commodo odio aenean sed adipiscing diam donec adipiscing tristique risus. Quam quisque id diam vel quam elementum.`,
            },
            {
              class: "columns-3-balanced",
              header: "Latin America",
              image: {
                src: "assets/images/axp-photography-v6pAkO31d50-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Congue nisi vitae suscipit tellus.",
              type: "list",
              display: "bullets",
              content: [
                {
                  content: "Ut venenatis tellus in metus vulputate.",
                  url: "#",
                },
                {
                  content:
                    "Vitae aliquet nec ullamcorper sit amet risus nullam.",
                  url: "#",
                },
                {
                  content: "Ellus in hac habitasse platea dictumst.",
                  url: "#",
                },
                {
                  content: "In nisl nisi scelerisque eu ultrices vitae.",
                  url: "#",
                },
                {
                  content:
                    "Est ullamcorper eget nulla facilisi etiam dignissim diam quis enim.",
                  url: "#",
                },
                { content: "It volutpat diam ut venenatis tellus.", url: "#" },
              ],
            },
          ],
        },
        {
          id: "content-world-international",
          name: "International",
          articles: [
            {
              class: "columns-wrap",
              header: "United Nations",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ilyass-seddoug-06w8RxgSzF0-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Morbi quis commodo odio aenean sed adipiscing diam. Congue mauris rhoncus aenean vel elit scelerisque mauris pellentesque. Justo nec ultrices dui sapien.",
                },
                {
                  image: {
                    src: "assets/images/mathias-reding-yfXhqAW5X0c-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nibh nisl condimentum id venenatis a condimentum. Id diam maecenas ultricies mi eget mauris pharetra et ultrices. Faucibus turpis in eu mi bibendum neque egestas. Et malesuada fames ac turpis egestas sed tempus urna et.",
                },
                {
                  image: {
                    src: "assets/images/matthew-tenbruggencate-0HJWobhGhJs-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Ut etiam sit amet nisl purus in mollis nunc sed. Pellentesque adipiscing commodo elit at imperdiet dui. Ac tortor vitae purus faucibus ornare suspendisse sed nisi lacus. Enim facilisis gravida neque convallis.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "European Union",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/markus-spiske-wIUxLHndcLw-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Donec massa sapien faucibus et molestie. Fermentum iaculis eu non diam. Donec pretium vulputate sapien nec sagittis. Placerat duis ultricies lacus sed. Pretium lectus quam id leo in vitae turpis massa.",
                },
                {
                  image: {
                    src: "assets/images/jakub-zerdzicki-VnTR3XFwxWs-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Luctus accumsan tortor posuere ac ut. Convallis posuere morbi leo urna molestie at elementum. Nisi est sit amet facilisis magna etiam tempor orci eu.",
                },
                {
                  image: {
                    src: "assets/images/guillaume-perigois-HL4LEIyGEYU-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Purus in massa tempor nec feugiat nisl pretium fusce. Fermentum odio eu feugiat pretium nibh ipsum consequat nisl vel. Vestibulum sed arcu non odio euismod lacinia at quis.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Global Crisis",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/mika-baumeister-jXPQY1em3Ew-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "ristique senectus et netus et malesuada. Orci phasellus egestas tellus rutrum tellus pellentesque eu tincidunt. Varius quam quisque id diam vel quam elementum pulvinar. Quis imperdiet massa tincidunt nunc pulvinar sapien et ligula.",
                },
                {
                  image: {
                    src: "assets/images/chris-leboutillier-c7RWVGL8lPA-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Cras ornare arcu dui vivamus arcu felis bibendum ut. Volutpat blandit aliquam etiam erat velit scelerisque in dictum. Pharetra magna ac placerat vestibulum lectus.",
                },
                {
                  image: {
                    src: "assets/images/mulyadi-JeCNRxGLSp4-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Leo integer malesuada nunc vel. Porttitor lacus luctus accumsan tortor posuere ac ut consequat. Ultrices eros in cursus turpis massa tincidunt dui ut. Eleifend mi in nulla posuere sollicitudin.",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-global-impact",
          name: "Global Impact",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Weather",
              image: {
                src: "assets/images/noaa-I323ZqSkkn8-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Euismod elementum nisi quis eleifend.",
              type: "list",
              content: [
                {
                  content:
                    "Enim tortor at auctor urna nunc id cursus metus. Nisi est sit amet facilisis magna etiam.",
                },
                {
                  content:
                    "Neque volutpat ac tincidunt vitae. Metus aliquam eleifend mi in.",
                },
                {
                  content:
                    "Aliquam malesuada bibendum arcu vitae elementum curabitur vitae.",
                },
                { content: "Turpis cursus in hac habitasse platea dictumst." },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Business",
              image: {
                src: "assets/images/david-vives-Nzbkev7SQTg-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Nunc mi ipsum faucibus vitae aliquet nec ullamcorper.",
              type: "list",
              content: [
                {
                  content:
                    "Eget nulla facilisi etiam dignissim diam quis enim.",
                },
                {
                  content:
                    "Risus viverra adipiscing at in tellus integer feugiat scelerisque.",
                },
                { content: "Cursus turpis massa tincidunt dui." },
                {
                  content:
                    "Nascetur ridiculus mus mauris vitae ultricies leo integer.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Politics",
              image: {
                src: "assets/images/kelli-dougal-vbiQ_7vwfrs-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Vulputate sapien nec sagittis aliquam malesuada.",
              type: "list",
              content: [
                { content: "Nisi scelerisque eu ultrices vitae auctor." },
                {
                  content:
                    "Urna porttitor rhoncus dolor purus non enim praesent elementum.",
                },
                { content: "Ac turpis egestas integer eget aliquet." },
                { content: "Nisl tincidunt eget nullam non nisi est." },
              ],
            },
          ],
        },
        {
          id: "content-world-underscored",
          name: "Underscored",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/luis-cortes-QrPDA15pRkM-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Risus sed vulputate odio ut enim blandit volutpat. Tempus egestas sed sed risus pretium quam vulputate. Ultrices mi tempus imperdiet nulla malesuada. Pellentesque diam volutpat commodo sed egestas. Scelerisque eleifend donec pretium vulputate sapien nec sagittis aliquam.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/juli-kosolapova-4PE3X9eKsu4-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Nunc mi ipsum faucibus vitae aliquet nec. Felis eget nunc lobortis mattis aliquam faucibus. Amet est placerat in egestas. Vitae proin sagittis nisl rhoncus mattis rhoncus. Mauris in aliquam sem fringilla ut. Pellentesque habitant morbi tristique senectus et netus et.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/olga-guryanova-ft7vJxwl2RY-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Egestas diam in arcu cursus euismod quis viverra nibh cras. Scelerisque fermentum dui faucibus in ornare quam viverra orci sagittis. Sed ullamcorper morbi tincidunt ornare massa eget egestas purus viverra. Risus in hendrerit gravida rutrum.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/christian-tenguan-P3gfVKhz8d0-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Integer malesuada nunc vel risus commodo viverra maecenas accumsan. Nec feugiat nisl pretium fusce id. Vel fringilla est ullamcorper eget nulla facilisi etiam dignissim diam. At tempor commodo ullamcorper a lacus vestibulum sed arcu. Suspendisse faucibus interdum posuere lorem ipsum dolor.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-global-issues",
          name: "Global Issues",
          articles: [
            {
              class: "columns-wrap",
              header: "Rising Crime",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/martin-podsiad-wrdtA9lew9E-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Phasellus faucibus scelerisque eleifend donec pretium. Tellus molestie nunc non blandit. Sed sed risus pretium quam vulputate dignissim suspendisse.",
                },
                {
                  image: {
                    src: "assets/images/valtteri-laukkanen-9u9Pc0t9vKM-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "In vitae turpis massa sed. In hac habitasse platea dictumst vestibulum rhoncus est pellentesque elit. Egestas pretium aenean pharetra magna ac placerat vestibulum.",
                },
                {
                  image: {
                    src: "assets/images/alec-favale-dLctr-PqFys-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Morbi tempus iaculis urna id volutpat lacus laoreet non. Dignissim convallis aenean et tortor at risus viverra adipiscing at. Nibh tortor id aliquet lectus proin nibh nisl.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Health concerns",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ani-kolleshi-7jjnJ-QA9fY-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Id diam maecenas ultricies mi eget mauris pharetra. Aliquam sem fringilla ut morbi tincidunt augue interdum. Accumsan sit amet nulla facilisi morbi tempus iaculis.",
                },
                {
                  image: {
                    src: "assets/images/piron-guillaume-U4FyCp3-KzY-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "In fermentum posuere urna nec tincidunt praesent semper feugiat nibh. Dolor sit amet consectetur adipiscing elit pellentesque habitant. Eget dolor morbi non arcu risus quis varius quam quisque.",
                },
                {
                  image: {
                    src: "assets/images/hush-naidoo-jade-photography-ZCO_5Y29s8k-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Commodo sed egestas egestas fringilla phasellus faucibus. Lectus urna duis convallis convallis. Sit amet tellus cras adipiscing enim eu turpis egestas.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Economy",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ibrahim-rifath-OApHds2yEGQ-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Ante in nibh mauris cursus mattis molestie. Vestibulum sed arcu non odio euismod lacinia at quis. Consequat semper viverra nam libero justo laoreet.",
                },
                {
                  image: {
                    src: "assets/images/mika-baumeister-bGZZBDvh8s4-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nunc non blandit massa enim nec dui nunc. Lobortis feugiat vivamus at augue eget arcu. Tempor commodo ullamcorper a lacus. Malesuada bibendum arcu vitae elementum curabitur vitae.",
                },
                {
                  image: {
                    src: "assets/images/shubham-dhage-tT6GNIFkZv4-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "In nulla posuere sollicitudin aliquam ultrices sagittis orci a. Sem fringilla ut morbi tincidunt augue interdum. Arcu felis bibendum ut tristique et egestas. Praesent elementum facilisis leo vel fringilla est ullamcorper.",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-hot-topics",
          name: "Hot Topics",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/dino-reichmuth-A5rCN8626Ck-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Leo vel fringilla est ullamcorper eget nulla facilisi etiam dignissim. Aliquam nulla facilisi cras fermentum odio. In est ante in nibh. Vulputate ut pharetra sit amet aliquam. Vitae congue eu consequat ac felis. Semper auctor neque vitae tempus quam pellentesque nec nam aliquam.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ross-parmly-rf6ywHVkrlY-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Vitae sapien pellentesque habitant morbi tristique senectus. Faucibus interdum posuere lorem ipsum dolor sit. Urna id volutpat lacus laoreet non curabitur. Tristique et egestas quis ipsum suspendisse ultrices gravida dictum.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/caglar-oskay-d0Be8Vs9XRk-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Donec ultrices tincidunt arcu non sodales neque sodales ut. Consequat mauris nunc congue nisi vitae suscipit tellus mauris. Dictum sit amet justo donec enim diam vulputate. Ultrices vitae auctor eu augue ut lectus arcu bibendum at.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/oguzhan-edman-ZWPkHLRu3_4-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Consectetur adipiscing elit pellentesque habitant morbi tristique senectus et. Adipiscing at in tellus integer feugiat scelerisque varius. Faucibus ornare suspendisse sed nisi lacus sed viverra tellus in. Eget velit aliquet sagittis id consectetur purus ut faucibus pulvinar.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-paid-content",
          name: "Paid Content",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/jakub-zerdzicki-qcRGVZNZ5js-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Et sollicitudin ac orci phasellus. Massa placerat duis ultricies lacus sed turpis tincidunt id.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/arnel-hasanovic-MNd-Rka1o0Q-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Neque volutpat ac tincidunt vitae semper. Nunc pulvinar sapien et ligula. Quam pellentesque nec nam aliquam sem et tortor consequat.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/ilaria-de-bona-RuFfpBsaRY0-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Velit euismod in pellentesque massa placerat duis ultricies. Nulla aliquet enim tortor at auctor. Vitae et leo duis ut diam quam nulla porttitor massa.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/k8-uYf_C34PAao-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Eros in cursus turpis massa tincidunt dui ut ornare lectus. Pulvinar neque laoreet suspendisse interdum consectetur libero id faucibus nisl.",
                },
              ],
            },
          ],
        },
      ],
    },
    politics: {
      name: "Politics",
      url: "/politics",
      priority: 1,
      sections: [
        {
          id: "content-politics-what-really-matters",
          name: "What Really Matters",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/emmanuel-ikwuegbu-ceawFbpA-14-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Libero justo laoreet sit amet. Et egestas quis ipsum suspendisse ultrices gravida dictum fusce. Eget aliquet nibh praesent tristique magna. Turpis cursus in hac habitasse platea dictumst quisque sagittis purus.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/mr-cup-fabien-barral-Mwuod2cm8g4-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Arcu cursus euismod quis viverra nibh. Cras ornare arcu dui vivamus arcu. At lectus urna duis convallis convallis tellus id.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/colin-lloyd-uaM_Ijy_joY-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Urna et pharetra pharetra massa massa ultricies mi quis hendrerit. Risus sed vulputate odio ut enim blandit volutpat maecenas volutpat. Quis ipsum suspendisse ultrices gravida dictum fusce ut.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/sara-cottle-bGjtWs8sXT0-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Velit aliquet sagittis id consectetur purus ut faucibus. Tellus mauris a diam maecenas sed. Urna neque viverra justo nec. Odio eu feugiat pretium nibh ipsum.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/elimende-inagella-7OxV_qDiGRI-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Amet nulla facilisi morbi tempus iaculis urna id. Scelerisque eleifend donec pretium vulputate sapien nec sagittis. Id leo in vitae turpis massa.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-today",
          name: "Today",
          articles: [
            {
              class: "columns-3-wide",
              header: "Campaign News",
              url: "#",
              image: {
                src: "assets/images/alexander-grey-8lnbXtxFGZw-unsplash_684.jpg",
                alt: "Placeholder",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "Photo taken by someone.",
                tag: { type: "breaking", label: "breaking" },
              },
              title:
                "Adipiscing at in tellus integer feugiat scelerisque varius morbi enim.",
              type: "list",
              content: [
                {
                  content:
                    "Sem fringilla ut morbi tincidunt augue interdum velit euismod.",
                },
                {
                  content:
                    "Quisque sagittis purus sit amet. Ornare lectus sit amet est.",
                },
                {
                  content:
                    "Placerat orci nulla pellentesque dignissim enim sit amet.",
                },
                {
                  content:
                    "In fermentum et sollicitudin ac orci phasellus egestas tellus.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "Elections",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/red-dot-Q98X_JVRGS0-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Nunc aliquet bibendum enim facilisis gravida neque. Nec feugiat in fermentum posuere urna. Molestie at elementum eu facilisis sed odio morbi. Scelerisque purus semper eget duis at tellus.",
                },
                {
                  image: {
                    src: "assets/images/parker-johnson-v0OWc_skg0g-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Eget dolor morbi non arcu risus quis. Non curabitur gravida arcu ac tortor dignissim.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "Local Government",
              url: "#",
              image: {
                src: "assets/images/valery-tenevoy-c0VbjkPEfmM-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Nunc vel risus commodo viverra maecenas accumsan lacus.",
              type: "list",
              content: [
                {
                  content: "Molestie at elementum eu facilisis sed odio morbi.",
                },
                {
                  content:
                    "Sit amet nisl suscipit adipiscing bibendum est ultricies integer quis.",
                },
                {
                  content:
                    "Bibendum neque egestas congue quisque egestas diam in arcu.",
                },
                { content: "Tellus molestie nunc non blandit massa enim nec." },
              ],
            },
          ],
        },
        {
          id: "content-politics-latest-headlines",
          name: "Latest Headlines",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Analysis",
              image: {
                src: "assets/images/scott-graham-OQMZwNd3ThU-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Pellentesque pulvinar pellentesque habitant morbi tristique senectus et netus et.",
              type: "list",
              content: [
                {
                  content:
                    "Arcu vitae elementum curabitur vitae nunc sed velit.",
                },
                {
                  content:
                    "Ornare suspendisse sed nisi lacus sed viverra tellus in.",
                },
                { content: "Vel fringilla est ullamcorper eget nulla." },
                {
                  content:
                    "Risus commodo viverra maecenas accumsan lacus vel facilisis volutpat est.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Facts First",
              image: {
                src: "assets/images/campaign-creators-pypeCEaJeZY-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "At varius vel pharetra vel turpis nunc eget lorem dolor.",
              type: "list",
              content: [
                {
                  content:
                    "Consectetur purus ut faucibus pulvinar elementum integer enim.",
                },
                {
                  content:
                    "Purus semper eget duis at. Tincidunt ornare massa eget egestas purus viverra accumsan.",
                },
                {
                  content:
                    "Amet massa vitae tortor condimentum lacinia quis vel.",
                },
                { content: "Tristique senectus et netus et malesuada." },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "More Politics News",
              image: {
                src: "assets/images/priscilla-du-preez-GgtxccOjIXE-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Vitae auctor eu augue ut lectus arcu bibendum at varius.",
              type: "text",
              content: `Pharetra diam sit amet nisl suscipit adipiscing bibendum est. Id aliquet lectus proin nibh. Porta lorem mollis aliquam ut porttitor leo a. Congue eu consequat ac felis donec et odio pellentesque.

Mi ipsum faucibus vitae aliquet nec ullamcorper. Sapien nec sagittis aliquam malesuada bibendum arcu vitae elementum curabitur. Quis imperdiet massa tincidunt nunc pulvinar sapien et ligula ullamcorper.`,
            },
          ],
        },
        {
          id: "content-politics-latest-media",
          name: "Latest Media",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/ruben-valenzuela-JEp9cl5jfZA-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/gregory-hayes-h5cd51KXmRQ-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/alan-rodriguez-qrD-g7oc9is-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/redd-f-N9CYH-H_gBE-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-election",
          name: "Election",
          articles: [
            {
              class: "columns-wrap",
              header: "Democrats",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/dyana-wing-so-Og16Foo-pd8-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Est ullamcorper eget nulla facilisi etiam dignissim. Est pellentesque elit ullamcorper dignissim cras. Velit euismod in pellentesque massa placerat duis ultricies.",
                },
                {
                  image: {
                    src: "assets/images/colin-lloyd-NKS5gg7rWGw-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Vitae suscipit tellus mauris a diam maecenas sed enim. Aenean sed adipiscing diam donec. Laoreet suspendisse interdum consectetur libero id faucibus nisl tincidunt.",
                },
                {
                  image: {
                    src: "assets/images/jon-tyson-0BLE1xp5HBQ-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Mattis enim ut tellus elementum sagittis vitae et. Massa sapien faucibus et molestie.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Republicans",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/clay-banks-BY-R0UNRE7w-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Platea dictumst quisque sagittis purus sit amet volutpat. Ante in nibh mauris cursus mattis molestie a iaculis.",
                },
                {
                  image: {
                    src: "assets/images/kelly-sikkema-A-lovieAmjA-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Quis hendrerit dolor magna eget est. Pellentesque pulvinar pellentesque habitant morbi tristique. Adipiscing commodo elit at imperdiet dui.",
                },
                {
                  image: {
                    src: "assets/images/chad-stembridge-sEHrIPpkKQY-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Donec pretium vulputate sapien nec sagittis aliquam. Cras adipiscing enim eu turpis egestas pretium aenean.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Liberals",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/derick-mckinney-muhK4oeYJiU-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Cursus sit amet dictum sit amet justo donec enim. Tempor id eu nisl nunc. Amet cursus sit amet dictum sit amet justo donec.",
                },
                {
                  image: {
                    src: "assets/images/marek-studzinski-9U9I-eVx9nI-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Enim diam vulputate ut pharetra sit amet aliquam. Tristique senectus et netus et malesuada.",
                },
                {
                  image: {
                    src: "assets/images/2h-media-lPcQhLP-b4I-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Eu turpis egestas pretium aenean. Auctor elit sed vulputate mi sit amet. In nibh mauris cursus mattis molestie.",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-more-political-news",
          name: "More political News",
          articles: [
            {
              class: "columns-3-wide",
              header: "More News",
              url: "#",
              type: "list",
              content: [
                {
                  content:
                    "Eros donec ac odio tempor. Tortor pretium viverra suspendisse potenti nullam.",
                },
                {
                  content:
                    "Ut venenatis tellus in metus vulputate eu scelerisque.",
                },
                {
                  content:
                    "Id diam maecenas ultricies mi eget. Nisl nunc mi ipsum faucibus vitae aliquet nec ullamcorper sit.",
                },
                {
                  content:
                    "Consectetur lorem donec massa sapien. Sed cras ornare arcu dui vivamus arcu felis.",
                },
                {
                  content:
                    "Fames ac turpis egestas maecenas pharetra convallis posuere morbi.",
                },
                { content: "Consequat nisl vel pretium lectus quam id." },
                {
                  content:
                    "Tincidunt ornare massa eget egestas purus viverra accumsan in nisl.",
                },
                { content: "Sed euismod nisi porta lorem mollis aliquam ut." },
                {
                  content:
                    "Suspendisse sed nisi lacus sed viverra tellus in hac.",
                },
                {
                  content:
                    "Aliquet risus feugiat in ante metus dictum at tempor.",
                },
                {
                  content:
                    "Velit aliquet sagittis id consectetur purus ut faucibus.",
                },
                {
                  content:
                    "Libero volutpat sed cras ornare. Consectetur adipiscing elit duis tristique sollicitudin nibh sit amet.",
                },
                {
                  content:
                    "Nibh nisl condimentum id venenatis a condimentum vitae. Fames ac turpis egestas maecenas pharetra.",
                },
                {
                  content:
                    "Massa sapien faucibus et molestie. Ac turpis egestas maecenas pharetra convallis posuere morbi leo urna.",
                },
                {
                  content:
                    "Est pellentesque elit ullamcorper dignissim cras. Mi proin sed libero enim sed.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/vanilla-bear-films-JEwNQerg3Hs-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Nunc aliquet bibendum enim facilisis gravida neque. Nec feugiat in fermentum posuere urna. Molestie at elementum eu facilisis sed odio morbi. Scelerisque purus semper eget duis at tellus.",
                },
                {
                  image: {
                    src: "assets/images/dani-navarro-6CnGzrLwM28-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Eget dolor morbi non arcu risus quis. Non curabitur gravida arcu ac tortor dignissim.",
                },
                {
                  image: {
                    src: "assets/images/wan-san-yip-ID1yWa1Wpx0-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Eget dolor morbi non arcu risus quis. Non curabitur gravida arcu ac tortor dignissim.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/david-beale--lQR8yeDzek-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Tellus in metus vulputate eu scelerisque felis imperdiet proin fermentum.",
                },
                {
                  image: {
                    src: "assets/images/arnaud-jaegers-IBWJsMObnnU-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Adipiscing tristique risus nec feugiat in fermentum posuere vulputate eu scelerisque.",
                },
                {
                  image: {
                    src: "assets/images/kevin-rajaram-qhixFFO8EWQ-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Potenti nullam ac tortor vitae purus. Adipiscing diam donec adipiscing tristique risus nec feugiat in fermentum.",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-underscored",
          name: "Underscored",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/kyle-glenn-gcw_WWu_uBQ-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Ut aliquam purus sit amet luctus venenatis lectus magna fringilla. Urna neque viverra justo nec ultrices dui sapien. Egestas sed sed risus pretium quam vulputate dignissim suspendisse. Risus viverra adipiscing at in tellus integer feugiat scelerisque. Pretium nibh ipsum consequat nisl vel.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/toa-heftiba-4xe-yVFJCvw-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Nunc id cursus metus aliquam eleifend. Sit amet est placerat in egestas erat. Vitae tortor condimentum lacinia quis vel eros donec ac. Maecenas pharetra convallis posuere morbi leo urna molestie at. Lectus proin nibh nisl condimentum id venenatis. Ut enim blandit volutpat maecenas volutpat blandit.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/harri-kuokkanen-SEtUeWL8bIQ-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Vestibulum sed arcu non odio euismod lacinia. Ipsum dolor sit amet consectetur. Nisi scelerisque eu ultrices vitae. Eu consequat ac felis donec. Viverra orci sagittis eu volutpat odio facilisis mauris sit amet. Purus semper eget duis at tellus at urna. Nulla aliquet porttitor lacus luctus accumsan tortor posuere ac.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ednilson-cardoso-dos-santos-haiooWA_weo-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Elementum eu facilisis sed odio morbi. Scelerisque viverra mauris in aliquam sem fringilla ut. Enim ut sem viverra aliquet. Massa sed elementum tempus egestas. Nam at lectus urna duis convallis convallis tellus. Sem integer vitae justo eget magna. In mollis nunc sed id.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-trending",
          name: "Trending",
          articles: [
            {
              class: "columns-wrap",
              header: "New Legislations",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/markus-spiske-7PMGUqYQpYc-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Consequat ac felis donec et. Libero nunc consequat interdum varius sit amet mattis vulputate enim. Cursus euismod quis viverra nibh cras pulvinar mattis nunc. Nisi lacus sed viverra tellus in hac. Aliquam malesuada bibendum arcu vitae elementum curabitur.",
                },
                {
                  image: {
                    src: "assets/images/viktor-talashuk-05HLFQu8bFw-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Neque gravida in fermentum et sollicitudin ac orci. Pretium aenean pharetra magna ac placerat vestibulum lectus mauris ultrices. Fermentum leo vel orci porta non pulvinar neque laoreet.",
                },
                {
                  image: {
                    src: "assets/images/anastassia-anufrieva-ecHGTPfjNfA-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Egestas diam in arcu cursus. Aliquam eleifend mi in nulla posuere sollicitudin aliquam ultrices sagittis. Augue ut lectus arcu bibendum at varius vel pharetra.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Latest Polls",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/bianca-ackermann-qr0-lKAOZSk-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Aliquam eleifend mi in nulla posuere sollicitudin. Tempor nec feugiat nisl pretium fusce. Fermentum iaculis eu non diam phasellus vestibulum lorem. Scelerisque eleifend donec pretium vulputate sapien nec. Sit amet aliquam id diam maecenas ultricies mi.",
                },
                {
                  image: {
                    src: "assets/images/phil-hearing-bu27Y0xg7dk-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Morbi leo urna molestie at elementum eu. Phasellus vestibulum lorem sed risus. Aliquet bibendum enim facilisis gravida neque. Aliquam sem et tortor consequat id porta. Interdum varius sit amet mattis vulputate enim nulla aliquet. Enim nulla aliquet porttitor lacus luctus accumsan tortor.",
                },
                {
                  image: {
                    src: "assets/images/mika-baumeister-Hm4zYX-BDxk-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Molestie nunc non blandit massa. Adipiscing diam donec adipiscing tristique risus nec feugiat in. Odio morbi quis commodo odio aenean sed adipiscing diam donec. Felis eget velit aliquet sagittis id consectetur purus ut. Odio ut enim blandit volutpat maecenas.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Who's gaining votes",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/wesley-tingey-7BkCRNwh_V0-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Risus viverra adipiscing at in tellus integer feugiat scelerisque. Porttitor eget dolor morbi non arcu risus quis varius quam. Consectetur adipiscing elit ut aliquam purus sit. Pulvinar mattis nunc sed blandit.",
                },
                {
                  image: {
                    src: "assets/images/miguel-bruna-TzVN0xQhWaQ-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Non curabitur gravida arcu ac tortor dignissim. Purus in mollis nunc sed id semper risus in hendrerit. Vestibulum morbi blandit cursus risus. Pellentesque nec nam aliquam sem et tortor. Ac tortor dignissim convallis aenean et.",
                },
                {
                  image: {
                    src: "assets/images/clay-banks-cisdc-344vo-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Ullamcorper a lacus vestibulum sed arcu non. Pharetra sit amet aliquam id diam. Viverra vitae congue eu consequat ac felis donec. Amet massa vitae tortor condimentum lacinia quis vel eros.",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-around-the-world",
          name: "Around the World",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Britain",
              image: {
                src: "assets/images/marc-olivier-jodoin-_eclsGKsUdo-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Sed blandit libero volutpat sed cras ornare arcu dui. Id ornare arcu odio ut sem.",
              type: "list",
              content: [
                {
                  content:
                    "Dolor sed viverra ipsum nunc aliquet bibendum enim. Hendrerit dolor magna eget est lorem ipsum dolor.",
                },
                {
                  content:
                    "At elementum eu facilisis sed odio morbi quis commodo odio. In massa tempor nec feugiat nisl.",
                },
                {
                  content:
                    "Est sit amet facilisis magna etiam tempor orci eu. Vulputate dignissim suspendisse in est ante in.",
                },
                {
                  content:
                    "Tempor nec feugiat nisl pretium. Id velit ut tortor pretium viverra suspendisse potenti nullam.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Italy",
              image: {
                src: "assets/images/sandip-roy-4hgTlYb9jzg-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Vitae congue mauris rhoncus aenean vel elit.",
              type: "list",
              content: [
                {
                  content:
                    "Aliquam sem fringilla ut morbi tincidunt augue interdum. Enim eu turpis egestas pretium aenean pharetra magna ac.",
                },
                {
                  content:
                    "Amet porttitor eget dolor morbi non arcu risus quis varius. Ultricies tristique nulla aliquet enim tortor at auctor.",
                },
                {
                  content:
                    "Nisi lacus sed viverra tellus in hac habitasse platea. Interdum velit euismod in pellentesque.",
                },
                {
                  content:
                    "Mattis ullamcorper velit sed ullamcorper morbi tincidunt ornare. Eu non diam phasellus vestibulum lorem sed risus.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Poland",
              image: {
                src: "assets/images/maksym-harbar-okn8ZIjPMxI-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Sed id semper risus in hendrerit gravida rutrum quisque.",
              type: "list",
              content: [
                {
                  content:
                    "Viverra justo nec ultrices dui sapien eget. A scelerisque purus semper eget duis at tellus at.",
                },
                {
                  content:
                    "Non diam phasellus vestibulum lorem sed risus ultricies tristique. Ornare arcu dui vivamus arcu felis bibendum ut tristique et.",
                },
                {
                  content:
                    "Quisque non tellus orci ac. At augue eget arcu dictum varius.",
                },
                {
                  content:
                    "Aenean sed adipiscing diam donec adipiscing tristique. Sagittis eu volutpat odio facilisis mauris.",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-hot-topics",
          name: "Hot Topics",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/ronda-darby-HbMLSB-uhQY-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Suspendisse sed nisi lacus sed viverra tellus in hac habitasse. Tincidunt id aliquet risus feugiat in. Eget aliquet nibh praesent tristique magna sit amet. Enim lobortis scelerisque fermentum dui faucibus. Molestie ac feugiat sed lectus. Facilisis sed odio morbi quis commodo.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/dominic-bieri-vXRt4rFr4hI-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Vitae ultricies leo integer malesuada nunc. Convallis aenean et tortor at risus viverra adipiscing at. Vitae sapien pellentesque habitant morbi tristique senectus. Pellentesque nec nam aliquam sem et tortor consequat id. Fames ac turpis egestas integer.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/inaki-del-olmo-NIJuEQw0RKg-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Dignissim diam quis enim lobortis scelerisque fermentum dui faucibus in. Euismod quis viverra nibh cras. Non sodales neque sodales ut etiam sit. Curabitur vitae nunc sed velit dignissim sodales ut eu. Id leo in vitae turpis massa sed elementum tempus egestas.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/matt-popovich-7mqsZsE6FaU-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Morbi tristique senectus et netus et malesuada fames. Placerat duis ultricies lacus sed turpis tincidunt id aliquet. Habitant morbi tristique senectus et netus et. Laoreet sit amet cursus sit amet dictum sit. Pellentesque elit ullamcorper dignissim cras tincidunt lobortis feugiat vivamus.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-paid-content",
          name: "Paid Content",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/maksim-larin-tecILYzVAzg-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title: "Duis at consectetur lorem donec massa.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/evie-calder-97CO-A4P0GQ-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Eget mi proin sed libero enim sed. Proin libero nunc consequat interdum varius.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/domino-studio-164_6wVEHfI-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Porta nibh venenatis cras sed felisDolor sit amet consectetur adipiscing elit ut aliquam purus sit.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/pat-taylor-12V36G17IbQ-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Nisl vel pretium lectus quam id leo in vitae. Ultrices neque ornare aenean euismod elementum nisi quis eleifend quam. Eget nullam non nisi est sit. Aliquet enim tortor at auctor urna.",
                },
              ],
            },
          ],
        },
      ],
    },
    business: {
      name: "Business",
      url: "/business",
      priority: 1,
      sections: [
        {
          id: "content-business-latest-trends",
          name: "Latest trends",
          articles: [
            {
              class: "columns-3-wide",
              header: "Investing",
              url: "#",
              image: {
                src: "assets/images/truckrun-XBWF6_TEsFM-unsplash_684.jpg",
                alt: "Placeholder",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "Photo taken by someone.",
                tag: { type: "breaking", label: "breaking" },
              },
              title:
                "Enim lobortis scelerisque fermentum dui faucibus in ornare. Ante metus dictum at tempor.",
              type: "text",
              content: `Consequat mauris nunc congue nisi vitae. Felis imperdiet proin fermentum leo vel orci porta. Facilisis gravida neque convallis a cras semper. Risus quis varius quam quisque id diam vel quam. Egestas quis ipsum suspendisse ultrices gravida. Nisl nisi scelerisque eu ultrices vitae auctor.

Viverra vitae congue eu consequat ac felis. Vestibulum rhoncus est pellentesque elit ullamcorper. Donec massa sapien faucibus et. Vehicula ipsum a arcu cursus vitae congue mauris rhoncus. Quis ipsum suspendisse ultrices gravida. Vel facilisis volutpat est velit egestas dui id ornare arcu. Commodo ullamcorper a lacus vestibulum.`,
            },
            {
              class: "columns-3-narrow",
              header: "Media",
              url: "#",
              image: {
                src: "assets/images/glenn-carstens-peters-npxXWgQ33ZQ-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Gravida in fermentum et sollicitudin ac. Varius duis at consectetur lorem donec massa sapien faucibus.",
              type: "text",
              content:
                "Nisi quis eleifend quam adipiscing vitae proin. Nunc sed velit dignissim sodales ut. Turpis nunc eget lorem dolor sed. Enim nulla aliquet porttitor lacus. Consequat ac felis donec et. Aliquam sem fringilla ut morbi tincidunt augue interdum velit. Arcu vitae elementum curabitur vitae nunc sed velit dignissim.",
            },
            {
              class: "columns-3-narrow",
              header: "Insights",
              url: "#",
              image: {
                src: "assets/images/kenny-eliason-4N3iHYmqy_E-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Venenatis urna cursus eget nunc. Adipiscing elit duis tristique sollicitudin.",
              type: "text",
              content: `Donec adipiscing tristique risus nec. Vel fringilla est ullamcorper eget nulla facilisi etiam dignissim. Vitae et leo duis ut diam quam. Pulvinar etiam non quam lacus suspendisse faucibus interdum posuere lorem.

Ac odio tempor orci dapibus ultrices in iaculis nunc. A diam maecenas sed enim ut sem. At quis risus sed vulputate.`,
            },
          ],
        },
        {
          id: "content-business-market-watch",
          name: "Market Watch",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Trending",
              image: {
                src: "assets/images/anne-nygard-tcJ6sJTtTWI-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Dictumst quisque sagittis purus sit amet.",
              type: "text",
              content:
                "Dolor magna eget est lorem. Nibh sit amet commodo nulla facilisi nullam. Etiam non quam lacus suspendisse faucibus interdum. Posuere sollicitudin aliquam ultrices sagittis orci. Massa enim nec dui nunc mattis enim ut tellus. Congue mauris rhoncus aenean vel. Egestas integer eget aliquet nibh praesent tristique.",
            },
            {
              class: "columns-3-balanced",
              header: "Tech",
              image: {
                src: "assets/images/maxim-hopman-IayKLkmz6g0-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Posuere sollicitudin aliquam ultrices sagittis orci a.",
              type: "text",
              content:
                "Praesent elementum facilisis leo vel fringilla est ullamcorper. Scelerisque viverra mauris in aliquam sem fringilla. Donec ac odio tempor orci. Eu augue ut lectus arcu. Diam sollicitudin tempor id eu nisl nunc mi ipsum.",
            },
            {
              class: "columns-3-balanced",
              header: "Success",
              image: {
                src: "assets/images/alex-hudson-7AgqAZbogOQ-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Scelerisque fermentum dui faucibus in.",
              type: "text",
              content:
                "landit volutpat maecenas volutpat blandit. Pulvinar pellentesque habitant morbi tristique senectus et. Facilisis magna etiam tempor orci. Sit amet commodo nulla facilisi nullam vehicula. Tortor vitae purus faucibus ornare suspendisse sed nisi lacus sed. Mus mauris vitae ultricies leo.",
            },
          ],
        },
        {
          id: "content-business-economy-today",
          name: "Economy Today",
          articles: [
            {
              class: "columns-wrap",
              header: "Global Impact",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/chris-leboutillier-TUJud0AWAPI-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Bibendum arcu vitae elementum curabitur vitae nunc sed. Ipsum faucibus vitae aliquet nec ullamcorper sit. Blandit libero volutpat sed cras ornare arcu dui. Maecenas sed enim ut sem viverra aliquet.",
                },
                {
                  image: {
                    src: "assets/images/nasa-Q1p7bh3SHj8-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Arcu risus quis varius quam quisque id diam vel quam. Sed risus pretium quam vulputate dignissim suspendisse in. Amet aliquam id diam maecenas ultricies mi. Egestas dui id ornare arcu odio.",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-Nph1oyRsHm4-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "At risus viverra adipiscing at in tellus. Morbi tempus iaculis urna id volutpat lacus laoreet non. Eu volutpat odio facilisis mauris sit amet. Leo urna molestie at elementum eu facilisis sed.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Outlook",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/denys-nevozhai-z0nVqfrOqWA-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Ut etiam sit amet nisl purus in mollis nunc sed. Eget mauris pharetra et ultrices neque ornare aenean. Magna sit amet purus gravida quis blandit turpis.",
                },
                {
                  image: {
                    src: "assets/images/taylor-grote-UiVe5QvOhao-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Eu nisl nunc mi ipsum faucibus vitae aliquet nec ullamcorper. Viverra aliquet eget sit amet tellus cras. Consequat id porta nibh venenatis. Ac felis donec et odio pellentesque diam volutpat commodo sed.",
                },
                {
                  image: {
                    src: "assets/images/linkedin-sales-solutions--AXDunSs-n4-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Montes nascetur ridiculus mus mauris vitae ultricies leo integer. Habitasse platea dictumst vestibulum rhoncus est pellentesque elit.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Financial Freedom",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/tierra-mallorca-rgJ1J8SDEAY-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Bibendum arcu vitae elementum curabitur vitae nunc sed. Facilisis mauris sit amet massa vitae tortor condimentum lacinia.",
                },
                {
                  image: {
                    src: "assets/images/stephen-phillips-hostreviews-co-uk-em37kS8WJJQ-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Ipsum nunc aliquet bibendum enim facilisis gravida neque convallis. At in tellus integer feugiat scelerisque varius morbi enim. Nisi vitae suscipit tellus mauris a.",
                },
                {
                  image: {
                    src: "assets/images/roberto-junior-4fsCBcZt9H8-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Diam sollicitudin tempor id eu nisl nunc mi ipsum faucibus. In pellentesque massa placerat duis ultricies lacus sed.",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-must-read",
          name: "Must Read",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/carl-nenzen-loven-c-pc2mP7hTs-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Scelerisque viverra mauris in aliquam sem fringilla ut morbi. Senectus et netus et malesuada fames ac turpis egestas. Et tortor at risus viverra. Iaculis nunc sed augue lacus viverra vitae congue. Nulla aliquet porttitor lacus luctus accumsan.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/devi-puspita-amartha-yahya-7ln0pST_O8M-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Vitae justo eget magna fermentum. Vel eros donec ac odio tempor orci dapibus. Volutpat est velit egestas dui id ornare arcu odio. Est sit amet facilisis magna. Bibendum est ultricies integer quis auctor elit. Ullamcorper dignissim cras tincidunt lobortis feugiat vivamus.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/bernd-dittrich-Xk1IfNnEhRA-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Nisl tincidunt eget nullam non nisi est sit. At consectetur lorem donec massa sapien faucibus et molestie ac. Semper risus in hendrerit gravida rutrum. Eget aliquet nibh praesent tristique magna sit. Mi quis hendrerit dolor magna eget.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/crystal-kwok-xD5SWy7hMbw-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Pulvinar proin gravida hendrerit lectus a. At volutpat diam ut venenatis tellus in metus vulputate eu. Maecenas accumsan lacus vel facilisis volutpat. Enim eu turpis egestas pretium aenean pharetra magna. Orci eu lobortis elementum nibh tellus molestie nunc.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-educational",
          name: "Educational",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Business 101",
              image: {
                src: "assets/images/austin-distel-rxpThOwuVgE-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Dictumst quisque sagittis purus sit amet.",
              type: "text",
              content: `incidunt dui ut ornare lectus sit. Quis varius quam quisque id diam. Adipiscing diam donec adipiscing tristique risus nec feugiat in. Cursus sit amet dictum sit. Lacinia quis vel eros donec ac odio. Accumsan tortor posuere ac ut consequat semper. Interdum posuere lorem ipsum dolor sit amet consectetur adipiscing. Integer malesuada nunc vel risus commodo viverra. Arcu risus quis varius quam quisque id diam vel quam.

Enim neque volutpat ac tincidunt vitae semper quis lectus nulla. Eget nulla facilisi etiam dignissim diam quis enim lobortis scelerisque. Sed tempus urna et pharetra pharetra massa.`,
            },
            {
              class: "columns-3-balanced",
              header: "Startup",
              image: {
                src: "assets/images/memento-media-XhYq-5KnxSk-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Posuere sollicitudin aliquam ultrices sagittis orci a.",
              type: "text",
              content: `Potenti nullam ac tortor vitae purus faucibus. Vulputate mi sit amet mauris. Elit pellentesque habitant morbi tristique senectus. In pellentesque massa placerat duis ultricies. Cras fermentum odio eu feugiat pretium nibh ipsum. Ornare quam viverra orci sagittis eu. Commodo sed egestas egestas fringilla phasellus faucibus scelerisque eleifend. Non diam phasellus vestibulum lorem sed risus. Metus vulputate eu scelerisque felis imperdiet.

Magna ac placerat vestibulum lectus mauris. Lobortis feugiat vivamus at augue eget. Facilisis volutpat est velit egestas dui id ornare arcu odio.`,
            },
            {
              class: "columns-3-balanced",
              header: "Make profit",
              image: {
                src: "assets/images/austin-distel-97HfVpyNR1M-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Scelerisque fermentum dui faucibus in.",
              type: "text",
              content: `Ornare aenean euismod elementum nisi quis. Tellus in hac habitasse platea dictumst vestibulum rhoncus est. Nisl nunc mi ipsum faucibus vitae aliquet nec. Eget egestas purus viverra accumsan in nisl nisi scelerisque. Urna duis convallis convallis tellus id interdum velit laoreet. Ultrices sagittis orci a scelerisque purus. Feugiat vivamus at augue eget. Ultricies tristique nulla aliquet enim. Nibh mauris cursus mattis molestie a iaculis at erat pellentesque.

Elementum eu facilisis sed odio morbi. Ac turpis egestas integer eget aliquet nibh praesent tristique magna. Tortor at risus viverra adipiscing at in tellus.`,
            },
          ],
        },
        {
          id: "content-business-underscored",
          name: "Underscored",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/bruce-mars-xj8qrWvuOEs-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Scelerisque viverra mauris in aliquam sem fringilla ut morbi. Senectus et netus et malesuada fames ac turpis egestas. Et tortor at risus viverra. Iaculis nunc sed augue lacus viverra vitae congue. Nulla aliquet porttitor lacus luctus accumsan.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ryan-plomp-TT6Hep-JzrU-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Vitae justo eget magna fermentum. Vel eros donec ac odio tempor orci dapibus. Volutpat est velit egestas dui id ornare arcu odio. Est sit amet facilisis magna. Bibendum est ultricies integer quis auctor elit. Ullamcorper dignissim cras tincidunt lobortis feugiat vivamus.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/robert-bye-xHUZuSwVJg4-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Scelerisque viverra mauris in aliquam sem fringilla ut morbi. Senectus et netus et malesuada fames ac turpis egestas. Et tortor at risus viverra. Iaculis nunc sed augue lacus viverra vitae congue. Nulla aliquet porttitor lacus luctus accumsan.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/jay-clark-P3sLerH3UmM-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Vitae justo eget magna fermentum. Vel eros donec ac odio tempor orci dapibus. Volutpat est velit egestas dui id ornare arcu odio. Est sit amet facilisis magna. Bibendum est ultricies integer quis auctor elit. Ullamcorper dignissim cras tincidunt lobortis feugiat vivamus.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-investing-101",
          name: "Investing 101",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Manage your assets",
              type: "articles-list",
              content: [
                {
                  title:
                    "Ic turpis egestas maecenas pharetra convallis. Dui accumsan sit amet nulla facilisi morbi tempus.",
                  content:
                    "A scelerisque purus semper eget duis at. Condimentum lacinia quis vel eros donec ac odio. Pretium fusce id velit ut tortor pretium viverra suspendisse. Blandit aliquam etiam erat velit scelerisque in. Est placerat in egestas erat imperdiet sed euismod nisi. Suspendisse potenti nullam ac tortor vitae purus faucibus.",
                },
                {
                  title: "Risus commodo viverra maecenas accumsan lacus vel.",
                  content:
                    "Est ullamcorper eget nulla facilisi etiam dignissim diam quis enim. Iaculis eu non diam phasellus. Odio aenean sed adipiscing diam donec. Eleifend donec pretium vulputate sapien nec sagittis aliquam malesuada bibendum.",
                },
                {
                  title:
                    "Vitae ultricies leo integer malesuada nunc vel risus commodo.",
                  content:
                    "Donec et odio pellentesque diam volutpat. Sed libero enim sed faucibus turpis in eu. Aliquam nulla facilisi cras fermentum odio eu feugiat pretium. Tristique risus nec feugiat in fermentum. Turpis egestas maecenas pharetra convallis posuere morbi leo urna.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "What to watch",
              type: "articles-list",
              content: [
                {
                  title: "Elementum integer enim neque volutpat.",
                  content:
                    "Dignissim diam quis enim lobortis scelerisque. Lacus vestibulum sed arcu non odio euismod lacinia at quis. Mi bibendum neque egestas congue quisque. Arcu dui vivamus arcu felis bibendum ut tristique. Consectetur adipiscing elit ut aliquam purus sit amet luctus venenatis.",
                },
                {
                  title: "Vitae turpis massa sed elementum tempus egestas sed.",
                  content:
                    "Eu lobortis elementum nibh tellus molestie. Egestas congue quisque egestas diam in arcu cursus euismod quis. Purus non enim praesent elementum facilisis. Suscipit tellus mauris a diam maecenas sed enim ut sem. Sed elementum tempus egestas sed sed risus pretium quam.",
                },
                {
                  title: "Consequat ac felis donec et odio pellentesque diam.",
                  content:
                    "Pharetra diam sit amet nisl suscipit adipiscing bibendum. Mi eget mauris pharetra et ultrices neque ornare. Habitant morbi tristique senectus et netus et. Quis eleifend quam adipiscing vitae. Fames ac turpis egestas maecenas pharetra convallis posuere morbi.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Did you know?",
              type: "articles-list",
              content: [
                {
                  title:
                    "Lacus sed viverra tellus in. Eget mi proin sed libero enim sed.",
                  content:
                    "A diam maecenas sed enim. Platea dictumst vestibulum rhoncus est pellentesque elit. Metus dictum at tempor commodo ullamcorper. Est ullamcorper eget nulla facilisi etiam dignissim diam. Felis eget velit aliquet sagittis id consectetur purus.",
                },
                {
                  title:
                    "Est lorem ipsum dolor sit amet. Duis ultricies lacus sed turpis tincidunt.",
                  content:
                    "Mattis pellentesque id nibh tortor id aliquet lectus. Odio aenean sed adipiscing diam donec adipiscing. Mi in nulla posuere sollicitudin aliquam ultrices sagittis. Dictum varius duis at consectetur lorem donec massa sapien faucibus.",
                },
                {
                  title: "Duis ut diam quam nulla porttitor massa id.",
                  content:
                    "Id aliquet lectus proin nibh nisl condimentum id venenatis. Ultrices in iaculis nunc sed augue lacus viverra vitae congue. Lectus urna duis convallis convallis tellus id interdum velit. Duis convallis convallis tellus id interdum. Et malesuada fames ac turpis egestas sed.",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-stock-market",
          name: "Stock market",
          articles: [
            {
              class: "columns-wrap",
              header: "Dow Jones",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/annie-spratt-IT6aov1ScW0-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Pretium fusce id velit ut tortor pretium viverra suspendisse potenti. Nisi scelerisque eu ultrices vitae auctor eu. Amet massa vitae tortor condimentum lacinia quis vel. In arcu cursus euismod quis.",
                },
                {
                  image: {
                    src: "assets/images/tech-daily-vxTWpu14zeM-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Tempus urna et pharetra pharetra massa massa ultricies mi. Vestibulum lorem sed risus ultricies tristique nulla aliquet enim. Sit amet luctus venenatis lectus magna fringilla urna.",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-jgOkEjVw-KM-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Viverra adipiscing at in tellus integer feugiat scelerisque varius morbi. Massa tempor nec feugiat nisl pretium fusce id. Elit ut aliquam purus sit amet luctus.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "S&P 500",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/boris-stefanik-q49CgyIrLes-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Risus quis varius quam quisque id diam vel quam. Risus at ultrices mi tempus imperdiet nulla malesuada. Aliquet enim tortor at auctor urna. Sapien et ligula ullamcorper malesuada proin libero. Nunc sed augue lacus viverra vitae congue.",
                },
                {
                  image: {
                    src: "assets/images/m-ZzOa5G8hSPI-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Quisque id diam vel quam elementum pulvinar etiam non. Lacus laoreet non curabitur gravida arcu ac tortor dignissim convallis. Ac ut consequat semper viverra nam libero justo.",
                },
                {
                  image: {
                    src: "assets/images/matthew-henry-0Ol8Sa2n21c-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Pulvinar etiam non quam lacus suspendisse faucibus interdum posuere lorem. Enim facilisis gravida neque convallis. Quis blandit turpis cursus in hac habitasse platea.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Day Trading",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/dylan-calluy-j9q18vvHitg-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Pellentesque pulvinar pellentesque habitant morbi tristique senectus et netus et. Sed enim ut sem viverra aliquet eget. Porttitor lacus luctus accumsan tortor. Sit amet justo donec enim diam.",
                },
                {
                  image: {
                    src: "assets/images/yucel-moran-4ndj0pATzeM-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nibh sit amet commodo nulla facilisi nullam vehicula. Lectus mauris ultrices eros in cursus turpis massa. Egestas fringilla phasellus faucibus scelerisque eleifend donec pretium. Sed adipiscing diam donec adipiscing tristique risus nec feugiat in.",
                },
                {
                  image: {
                    src: "assets/images/stefan-stefancik-pzA7QWNCIYg-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Consectetur lorem donec massa sapien faucibus. Aliquet porttitor lacus luctus accumsan tortor. Pharetra pharetra massa massa ultricies mi. Aliquam id diam maecenas ultricies mi eget mauris pharetra. Rhoncus urna neque viverra justo nec ultrices dui sapien eget.",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-impact",
          name: "Impact",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Oil crisis",
              type: "articles-list",
              content: [
                {
                  title:
                    "Eleifend donec pretium vulputate sapien nec sagittis.",
                  content:
                    "Adipiscing bibendum est ultricies integer quis. Viverra ipsum nunc aliquet bibendum enim facilisis gravida neque. Suspendisse in est ante in. Semper auctor neque vitae tempus quam pellentesque. Et tortor at risus viverra adipiscing at in tellus integer.",
                },
                {
                  title:
                    "Ornare aenean euismod elementum nisi quis eleifend quam.",
                  content:
                    "Pretium aenean pharetra magna ac. Sem nulla pharetra diam sit amet nisl suscipit adipiscing bibendum. Neque vitae tempus quam pellentesque nec nam aliquam sem. Potenti nullam ac tortor vitae purus faucibus ornare suspendisse. Ipsum nunc aliquet bibendum enim facilisis gravida neque.",
                },
                {
                  title:
                    "Ultrices sagittis orci a scelerisque purus semper. Porttitor massa id neque aliquam vestibulum morbi blandit.",
                  content:
                    "Augue eget arcu dictum varius. Aliquet nibh praesent tristique magna sit amet purus gravida. Mattis enim ut tellus elementum. A diam sollicitudin tempor id eu nisl nunc mi. Justo nec ultrices dui sapien eget mi proin. Euismod lacinia at quis risus sed vulputate odio.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Tech Markets",
              type: "articles-list",
              content: [
                {
                  title:
                    "Dictum sit amet justo donec. Justo donec enim diam vulputate ut pharetra sit.",
                  content:
                    "Bibendum enim facilisis gravida neque. Ullamcorper dignissim cras tincidunt lobortis feugiat vivamus at augue. Auctor neque vitae tempus quam pellentesque nec. Justo donec enim diam vulputate ut pharetra sit amet. Aliquam sem fringilla ut morbi tincidunt augue interdum velit.",
                },
                {
                  title:
                    "Massa massa ultricies mi quis hendrerit dolor magna eget.",
                  content:
                    "Ornare massa eget egestas purus viverra accumsan in nisl nisi. A arcu cursus vitae congue mauris rhoncus. Gravida arcu ac tortor dignissim convallis aenean et tortor. Elit scelerisque mauris pellentesque pulvinar pellentesque habitant. Volutpat diam ut venenatis tellus in metus.",
                },
                {
                  title:
                    "Duis at consectetur lorem donec massa sapien faucibus.",
                  content:
                    "acilisis gravida neque convallis a cras semper auctor neque. Non nisi est sit amet facilisis magna etiam tempor. Posuere morbi leo urna molestie at elementum eu. Tellus in hac habitasse platea dictumst vestibulum rhoncus est pellentesque.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Declining Markets",
              type: "articles-list",
              content: [
                {
                  title:
                    "Odio aenean sed adipiscing diam donec adipiscing tristique risus nec.",
                  content:
                    "Pharetra vel turpis nunc eget. Non arcu risus quis varius quam quisque id. Augue ut lectus arcu bibendum at varius vel pharetra vel. Rhoncus dolor purus non enim praesent elementum.",
                },
                {
                  title:
                    "Quis enim lobortis scelerisque fermentum. Nisl rhoncus mattis rhoncus urna. Felis eget velit aliquet sagittis id consectetur purus ut.",
                  content:
                    "Enim nec dui nunc mattis enim ut. Amet luctus venenatis lectus magna fringilla urna porttitor rhoncus dolor. Sed vulputate mi sit amet mauris commodo. Ultricies lacus sed turpis tincidunt id aliquet risus feugiat. In hac habitasse platea dictumst vestibulum rhoncus est.",
                },
                {
                  title:
                    "landit cursus risus at ultrices mi tempus imperdiet nulla malesuada.",
                  content:
                    "Vitae justo eget magna fermentum iaculis eu non diam phasellus. Et netus et malesuada fames ac turpis. In eu mi bibendum neque egestas congue. Justo eget magna fermentum iaculis eu non diam. Feugiat nibh sed pulvinar proin gravida hendrerit lectus a.",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-hot-topics",
          name: "Hot Topics",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/alice-pasqual-Olki5QpHxts-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "In massa tempor nec feugiat nisl. Mattis vulputate enim nulla aliquet porttitor lacus luctus. Et sollicitudin ac orci phasellus egestas tellus rutrum tellus pellentesque. Nec sagittis aliquam malesuada bibendum.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/lukasz-radziejewski-cg4MzL_eSvU-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Euismod quis viverra nibh cras pulvinar mattis nunc. Mauris pellentesque pulvinar pellentesque habitant morbi tristique senectus. Malesuada bibendum arcu vitae elementum curabitur vitae. Fusce id velit ut tortor.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/microsoft-365-f1zQuagWCTA-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Scelerisque felis imperdiet proin fermentum leo vel orci. Tortor vitae purus faucibus ornare suspendisse sed nisi. Molestie at elementum eu facilisis sed odio. Pellentesque sit amet porttitor eget. Vitae auctor eu augue ut lectus arcu bibendum at varius.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/emran-yousof-k8ZbMQWbx34-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Egestas sed sed risus pretium quam vulputate dignissim suspendisse. Potenti nullam ac tortor vitae purus faucibus ornare. Nunc mattis enim ut tellus elementum sagittis vitae et leo. Pellentesque pulvinar pellentesque habitant morbi tristique senectus.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-paid-content",
          name: "Paid Content",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/junko-nakase-Q-72wa9-7Dg-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Facilisis magna etiam tempor orci eu lobortis elementum nibh tellus. Morbi enim nunc faucibus a pellentesque sit amet porttitor eget.",
                },
                {
                  image: {
                    src: "assets/images/heather-ford-5gkYsrH_ebY-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Cursus vitae congue mauris rhoncus aenean vel elit. Ultrices neque ornare aenean euismod elementum nisi. Aliquet risus feugiat in ante metus dictum at tempor commodo.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/metin-ozer-hShrr0WvrQs-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Sit amet aliquam id diam maecenas ultricies. Magna sit amet purus gravida quis blandit. Risus nullam eget felis eget nunc. Ac felis donec et odio pellentesque diam volutpat commodo sed.",
                },
                {
                  image: {
                    src: "assets/images/mac-blades-jpgJSBQtw5U-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Purus faucibus ornare suspendisse sed nisi lacus. Malesuada nunc vel risus commodo. Pretium fusce id velit ut tortor pretium viverra suspendisse potenti.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/keagan-henman-xPJYL0l5Ii8-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Aliquam malesuada bibendum arcu vitae elementum curabitur. A pellentesque sit amet porttitor eget dolor morbi non.",
                },
                {
                  image: {
                    src: "assets/images/erik-mclean-ByjIzFupcHo-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Tortor at auctor urna nunc id cursus metus aliquam. Facilisis magna etiam tempor orci. Eu nisl nunc mi ipsum faucibus vitae aliquet.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/ixography-05Q_XPF_YKs-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Cursus mattis molestie a iaculis at. Nullam eget felis eget nunc. Tortor id aliquet lectus proin nibh nisl condimentum id.",
                },
                {
                  image: {
                    src: "assets/images/harley-davidson-fFbUdx80oCc-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "arius morbi enim nunc faucibus a pellentesque sit amet porttitor. Blandit libero volutpat sed cras. Sed viverra ipsum nunc aliquet bibendum.",
                },
              ],
            },
          ],
        },
      ],
    },
    opinion: {
      name: "Opinion",
      url: "/opinion",
      priority: 2,
      sections: [
        {
          id: "content-opinion-a-deeper-look",
          name: "A deeper look",
          articles: [
            {
              class: "columns-3-wide",
              header: "Latest Facts",
              url: "#",
              image: {
                src: "assets/images/milad-fakurian-58Z17lnVS4U-unsplash_684.jpg",
                alt: "Placeholder",
                width: "684",
                height: "385",
              },
              meta: { tag: { type: "breaking", label: "breaking" } },
              title:
                "Senectus et netus et malesuada fames ac turpis egestas. Odio facilisis mauris sit amet massa. Ornare quam viverra orci sagittis eu volutpat odio.",
              type: "text",
              content:
                "Lorem ipsum dolor sit amet consectetur. Ridiculus mus mauris vitae ultricies leo. Volutpat ac tincidunt vitae semper quis. In est ante in nibh. Fringilla phasellus faucibus scelerisque eleifend donec pretium. Scelerisque eu ultrices vitae auctor eu augue.",
            },
            {
              class: "columns-3-narrow",
              header: "Top of our mind",
              url: "#",
              image: {
                src: "assets/images/no-revisions-UhpAf0ySwuk-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Nisl pretium fusce id velit ut tortor pretium. Arcu cursus vitae congue mauris rhoncus aenean.",
              type: "text",
              content:
                "Aenean euismod elementum nisi quis eleifend quam adipiscing vitae proin. Pharetra vel turpis nunc eget lorem. Morbi tincidunt augue interdum velit euismod in pellentesque massa placerat.",
            },
            {
              class: "columns-3-narrow",
              header: "Editor Report",
              url: "#",
              image: {
                src: "assets/images/national-cancer-institute-YvvFRJgWShM-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Dignissim enim sit amet venenatis urna cursus.",
              type: "text",
              content: `Aenean pharetra magna ac placerat vestibulum lectus mauris. Massa sapien faucibus et molestie ac feugiat sed lectus vestibulum.

Vitae congue mauris rhoncus aenean vel elit scelerisque. Faucibus turpis in eu mi bibendum neque egestas congue quisque.`,
            },
          ],
        },
        {
          id: "content-opinion-top-issues",
          name: "Top Issues",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Thoughts",
              image: {
                src: "assets/images/rebe-pascual-SACRQSof7Qw-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Morbi tincidunt ornare massa eget.",
              type: "list",
              content: [
                {
                  content: "Tortor consequat id porta nibh venenatis cras sed.",
                },
                {
                  content:
                    "Suspendisse faucibus interdum posuere lorem ipsum dolor sit amet consectetur.",
                },
                {
                  content:
                    "Adipiscing diam donec adipiscing tristique risus nec feugiat in.",
                },
                {
                  content:
                    "Ultrices neque ornare aenean euismod elementum nisi quis.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Social commentary",
              image: {
                src: "assets/images/fanga-studio-bOfCOy3_4wU-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Sagittis aliquam malesuada bibendum arcu vitae.",
              type: "list",
              content: [
                {
                  content:
                    "Nisi porta lorem mollis aliquam ut porttitor leo a diam.",
                },
                {
                  content:
                    "Purus ut faucibus pulvinar elementum integer enim neque volutpat ac.",
                },
                { content: "Suspendisse in est ante in nibh mauris cursus." },
                {
                  content:
                    "Aliquam vestibulum morbi blandit cursus. Leo integer malesuada nunc vel risus commodo viverra maecenas.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Special Projects",
              image: {
                src: "assets/images/jakob-dalbjorn-cuKJre3nyYc-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Nulla aliquet enim tortor at auctor urna nunc id.",
              type: "text",
              content:
                "Platea dictumst quisque sagittis purus sit amet volutpat. Vulputate ut pharetra sit amet aliquam id. Tellus integer feugiat scelerisque varius morbi enim nunc faucibus. Est ante in nibh mauris. Libero volutpat sed cras ornare arcu dui vivamus.",
            },
          ],
        },
        {
          id: "content-opinon-trending",
          name: "Trending",
          articles: [
            {
              class: "columns-wrap",
              header: "Around the world",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/dibakar-roy-K9JwokzSvrc-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Egestas congue quisque egestas diam in arcu. Sollicitudin tempor id eu nisl nunc mi.",
                },
                {
                  image: {
                    src: "assets/images/anatol-rurac-NeSj0i6HLak-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "A condimentum vitae sapien pellentesque habitant morbi tristique senectus. Neque laoreet suspendisse interdum consectetur.",
                },
                {
                  image: {
                    src: "assets/images/anatol-rurac-b5t2lqeCGfA-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Dui vivamus arcu felis bibendum. Sit amet purus gravida quis blandit turpis cursus in.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Support",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/neil-thomas-SIU1Glk6v5k-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Malesuada fames ac turpis egestas integer eget. Ante metus dictum at tempor commodo ullamcorper. Ipsum dolor sit amet consectetur.",
                },
                {
                  image: {
                    src: "assets/images/jon-tyson-ne2mqMgER8Y-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Dictumst quisque sagittis purus sit amet. Cras fermentum odio eu feugiat pretium. Pretium aenean pharetra magna ac placerat vestibulum lectus.",
                },
                {
                  image: {
                    src: "assets/images/nonresident-nizUHtSIrKM-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Et odio pellentesque diam volutpat commodo sed egestas egestas. Sagittis aliquam malesuada bibendum arcu vitae elementum curabitur.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Know More",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/alev-takil-fYyYz38bUkQ-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nullam eget felis eget nunc. Fames ac turpis egestas integer eget aliquet nibh praesent tristique.",
                },
                {
                  image: {
                    src: "assets/images/bermix-studio-yUnSMBogWNI-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Massa ultricies mi quis hendrerit dolor magna eget est.",
                },
                {
                  image: {
                    src: "assets/images/pierre-bamin-lM4_Nmcj4Xk-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Ut tellus elementum sagittis vitae et leo duis ut. Purus ut faucibus pulvinar elementum integer enim.",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-think-about-it",
          name: "Think about it",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Mental Health",
              image: {
                src: "assets/images/matthew-ball-3wW2fBjptQo-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "olutpat ac tincidunt vitae semper quis lectus nulla at. Non quam lacus suspendisse faucibus interdum posuere lorem..",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "Et tortor consequat id porta nibh venenatis cras sed felis. Neque aliquam vestibulum morbi blandit cursus risus at ultrices mi.",
                  url: "#",
                },
                {
                  content:
                    "Commodo quis imperdiet massa tincidunt nunc. Diam maecenas sed enim ut sem viverra aliquet eget sit.",
                  url: "#",
                },
                {
                  content:
                    "Aliquam malesuada bibendum arcu vitae elementum curabitur. Quis ipsum suspendisse ultrices gravida dictum fusce ut placerat.",
                  url: "#",
                },
                {
                  content:
                    "Quis enim lobortis scelerisque fermentum. Nibh venenatis cras sed felis eget velit aliquet.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Better life",
              image: {
                src: "assets/images/peter-conlan-LEgwEaBVGMo-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Placerat vestibulum lectus mauris ultrices. Eros in cursus turpis massa.",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "In hac habitasse platea dictumst vestibulum rhoncus est pellentesque elit. At lectus urna duis convallis convallis tellus id interdum.",
                  url: "#",
                },
                {
                  content:
                    "Ultrices eros in cursus turpis massa tincidunt dui. Mi tempus imperdiet nulla malesuada pellentesque.",
                  url: "#",
                },
                {
                  content:
                    "Ipsum faucibus vitae aliquet nec ullamcorper sit. Eleifend donec pretium vulputate sapien nec sagittis aliquam.",
                  url: "#",
                },
                {
                  content:
                    "In hac habitasse platea dictumst. Pretium vulputate sapien nec sagittis aliquam malesuada bibendum arcu.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "The right choice",
              image: {
                src: "assets/images/vladislav-babienko-KTpSVEcU0XU-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Faucibus et molestie ac feugiat. Enim sit amet venenatis urna cursus eget nunc scelerisque viverra.",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "Urna porttitor rhoncus dolor purus. Eget sit amet tellus cras adipiscing enim.",
                  url: "#",
                },
                {
                  content:
                    "Leo urna molestie at elementum eu facilisis sed. Metus dictum at tempor commodo ullamcorper a.",
                  url: "#",
                },
                {
                  content:
                    "Non odio euismod lacinia at quis risus sed vulputate.",
                  url: "#",
                },
                {
                  content:
                    "Justo donec enim diam vulputate ut. Euismod elementum nisi quis eleifend.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-latest-media",
          name: "Latest Media",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/daniel-staple-N320vzTBviA-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/clem-onojeghuo-DoA2duXyzRM-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/egor-myznik-GFHKMW6KiJ0-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/trung-thanh-LgdDeuBcgIY-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-in-case-you-missed-it",
          name: "In case you missed it",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Critical thoughts",
              image: {
                src: "assets/images/tingey-injury-law-firm-9SKhDFnw4c4-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Facilisi morbi tempus iaculis urna id. Nibh cras pulvinar mattis nunc sed.",
              type: "list",
              content: [
                {
                  content:
                    "Eget felis eget nunc lobortis mattis aliquam faucibus purus in.",
                },
                {
                  content:
                    "Adipiscing elit ut aliquam purus sit amet luctus venenatis lectus.",
                },
                {
                  content: "Eu volutpat odio facilisis mauris sit amet massa.",
                },
                {
                  content:
                    "Vitae tortor condimentum lacinia quis vel eros donec ac.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Critical Thinking",
              image: {
                src: "assets/images/tachina-lee--wjk_SSqCE4-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Euismod nisi porta lorem mollis aliquam ut porttitor leo a.",
              type: "list",
              content: [
                { content: "Enim facilisis gravida neque convallis a." },
                {
                  content:
                    "Ridiculus mus mauris vitae ultricies leo integer malesuada.",
                },
                {
                  content:
                    "Elementum nisi quis eleifend quam. Sed elementum tempus egestas sed sed.",
                },
                {
                  content:
                    "Ut tellus elementum sagittis vitae et leo duis ut diam. Ultrices gravida dictum fusce ut placerat orci nulla pellentesque dignissim.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Critical Actions",
              image: {
                src: "assets/images/etienne-girardet-RqOyRtYGhLg-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Amet dictum sit amet justo donec enim diam.",
              type: "list",
              content: [
                {
                  content:
                    "Metus dictum at tempor commodo ullamcorper a lacus vestibulum.",
                },
                {
                  content:
                    "In nisl nisi scelerisque eu ultrices. In fermentum et sollicitudin ac orci phasellus egestas.",
                },
                {
                  content:
                    "Ut aliquam purus sit amet luctus venenatis lectus magna fringilla.",
                },
                {
                  content:
                    "Morbi enim nunc faucibus a pellentesque. Mi ipsum faucibus vitae aliquet nec ullamcorper.",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-environmental-issues",
          name: "Environmental Issues",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Global Warming",
              type: "articles-list",
              content: [
                {
                  title:
                    "Dis parturient montes nascetur ridiculus mus mauris vitae.",
                  content:
                    "Justo donec enim diam vulputate ut pharetra sit amet aliquam. Curabitur vitae nunc sed velit dignissim sodales. Varius vel pharetra vel turpis nunc eget lorem. Sed viverra ipsum nunc aliquet bibendum. Ultrices in iaculis nunc sed augue.",
                },
                {
                  title:
                    "Vitae turpis massa sed elementum tempus egestas sed sed risus.",
                  content:
                    "Nascetur ridiculus mus mauris vitae ultricies leo integer. Hendrerit dolor magna eget est lorem ipsum dolor sit amet. Ultrices gravida dictum fusce ut placerat orci nulla pellentesque. Gravida arcu ac tortor dignissim convallis aenean. Urna duis convallis convallis tellus id interdum.",
                },
                {
                  title:
                    "Rutrum tellus pellentesque eu tincidunt tortor. Volutpat sed cras ornare arcu.",
                  content:
                    "estibulum mattis ullamcorper velit sed ullamcorper morbi tincidunt. Urna porttitor rhoncus dolor purus. Nisl nunc mi ipsum faucibus vitae aliquet nec ullamcorper. Ultrices in iaculis nunc sed augue lacus. Nunc pulvinar sapien et ligula ullamcorper.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Recycling",
              type: "articles-list",
              content: [
                {
                  title:
                    "Tellus id interdum velit laoreet id donec ultrices tincidunt arcu.",
                  content:
                    "Eget est lorem ipsum dolor sit amet. Faucibus scelerisque eleifend donec pretium vulputate sapien. Quam adipiscing vitae proin sagittis. Quisque id diam vel quam elementum pulvinar etiam non. Laoreet non curabitur gravida arcu ac tortor dignissim convallis aenean.",
                },
                {
                  title:
                    "Scelerisque viverra mauris in aliquam sem fringilla ut.",
                  content:
                    "Amet mauris commodo quis imperdiet. Eu consequat ac felis donec et odio pellentesque. Hendrerit gravida rutrum quisque non tellus orci ac. Amet cursus sit amet dictum.",
                },
                {
                  title:
                    "Vulputate eu scelerisque felis imperdiet. Non quam lacus suspendisse faucibus interdum posuere.",
                  content:
                    "Luctus venenatis lectus magna fringilla urna porttitor. Hac habitasse platea dictumst vestibulum rhoncus. Orci a scelerisque purus semper eget duis at tellus. Risus nec feugiat in fermentum posuere urna nec tincidunt praesent.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "New researches",
              type: "articles-list",
              content: [
                {
                  title: "Non quam lacus suspendisse faucibus.",
                  content:
                    "Nisi quis eleifend quam adipiscing vitae proin sagittis nisl rhoncus. Odio euismod lacinia at quis. Molestie a iaculis at erat. Id cursus metus aliquam eleifend mi in nulla posuere sollicitudin. Donec ac odio tempor orci dapibus.",
                },
                {
                  title:
                    "Sit amet consectetur adipiscing elit. Lorem sed risus ultricies tristique nulla aliquet.",
                  content:
                    "Neque aliquam vestibulum morbi blandit cursus risus at. Habitant morbi tristique senectus et netus et. Quis blandit turpis cursus in. Adipiscing vitae proin sagittis nisl rhoncus mattis rhoncus urna. Vel risus commodo viverra maecenas. Tortor dignissim convallis aenean et tortor at.",
                },
                {
                  title: "Ullamcorper sit amet risus nullam eget.",
                  content:
                    "urpis nunc eget lorem dolor sed viverra ipsum nunc aliquet. Mollis aliquam ut porttitor leo a diam. Posuere morbi leo urna molestie. Suscipit tellus mauris a diam maecenas sed. Ultrices dui sapien eget mi proin sed libero enim sed.",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-underscored",
          name: "Underscored",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/alexander-kirov-YhDJXJjmxUQ-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Faucibus interdum posuere lorem ipsum. Aliquam nulla facilisi cras fermentum odio. Odio facilisis mauris sit amet massa vitae. Et tortor at risus viverra adipiscing. Luctus accumsan tortor posuere ac ut consequat semper viverra nam.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/paola-chaaya-QrbuLFT6ypw-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Montes nascetur ridiculus mus mauris vitae. Amet porttitor eget dolor morbi non arcu risus quis varius. Rhoncus aenean vel elit scelerisque mauris pellentesque pulvinar. A lacus vestibulum sed arcu non odio euismod lacinia.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/sean-lee-hDqRQmcjM3s-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Volutpat consequat mauris nunc congue. Arcu dui vivamus arcu felis bibendum ut tristique. Fringilla ut morbi tincidunt augue. Libero enim sed faucibus turpis in eu mi bibendum. Posuere ac ut consequat semper viverra.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/nathan-dumlao-laCrvNG3F_I-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Nec nam aliquam sem et. Maecenas ultricies mi eget mauris pharetra. Nibh nisl condimentum id venenatis a condimentum vitae sapien. Tellus pellentesque eu tincidunt tortor aliquam nulla facilisi cras fermentum.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinon-what-matters-most",
          name: "What matters most",
          articles: [
            {
              class: "columns-wrap",
              header: "Discussion",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/tatjana-petkevica-iad-dMBDdoo-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nibh sed pulvinar proin gravida hendrerit lectus. Habitasse platea dictumst quisque sagittis purus sit amet. Mi sit amet mauris commodo quis.",
                },
                {
                  image: {
                    src: "assets/images/nathan-cima-TQuq2OtLBNU-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nascetur ridiculus mus mauris vitae ultricies leo integer malesuada. Arcu non odio euismod lacinia. Ac turpis egestas sed tempus urna.",
                },
                {
                  image: {
                    src: "assets/images/artur-voznenko-rwPIQQPz1ew-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Lectus sit amet est placerat in. Auctor augue mauris augue neque gravida in fermentum. Duis convallis convallis tellus id interdum.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Is it worth it?",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/zac-gudakov-wwqZ8CM21gg-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Venenatis tellus in metus vulputate eu scelerisque felis. Orci phasellus egestas tellus rutrum tellus pellentesque eu. Id leo in vitae turpis massa sed elementum.",
                },
                {
                  image: {
                    src: "assets/images/pat-whelen-68OkRwuOeyQ-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Feugiat vivamus at augue eget arcu dictum varius duis at. Ultrices mi tempus imperdiet nulla malesuada pellentesque elit eget.",
                },
                {
                  image: {
                    src: "assets/images/tania-mousinho-YlpfE9uCakE-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Eget sit amet tellus cras adipiscing enim eu. Dictum at tempor commodo ullamcorper a lacus. Lectus proin nibh nisl condimentum id venenatis a condimentum vitae.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Just do it",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maksym-kaharlytskyi-Y0z9MyDsrU0-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Mattis rhoncus urna neque viverra. Hendrerit gravida rutrum quisque non tellus orci ac. Ut venenatis tellus in metus.",
                },
                {
                  image: {
                    src: "assets/images/maja-kochanowska-EiJQdDI_t_Y-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Enim ut tellus elementum sagittis vitae et leo duis. Dictumst quisque sagittis purus sit amet volutpat consequat.",
                },
                {
                  image: {
                    src: "assets/images/patti-black-FnV-PjAYHCI-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "urus ut faucibus pulvinar elementum integer enim neque. Commodo sed egestas egestas fringilla phasellus faucibus scelerisque.",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-hot-topics",
          name: "Hot Topics",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/rio-lecatompessy-cfDURuQKABk-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Feugiat in ante metus dictum at tempor. Faucibus scelerisque eleifend donec pretium. Turpis egestas integer eget aliquet nibh praesent. In metus vulputate eu scelerisque felis imperdiet. Diam maecenas sed enim ut sem. Quis imperdiet massa tincidunt nunc pulvinar sapien et.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/declan-sun-misAHv6YWkI-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Massa eget egestas purus viverra accumsan in nisl nisi. Sodales ut eu sem integer. Ac tortor dignissim convallis aenean et tortor. Erat velit scelerisque in dictum non consectetur. Id venenatis a condimentum vitae sapien pellentesque habitant.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/astronaud23-ox3t0m3PUqA-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Nisl rhoncus mattis rhoncus urna. Ligula ullamcorper malesuada proin libero nunc consequat interdum. Nunc mi ipsum faucibus vitae aliquet nec ullamcorper. Pellentesque nec nam aliquam sem et tortor consequat. Consequat interdum varius sit amet mattis. Diam sit amet nisl suscipit adipiscing bibendum est ultricies.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-lUc5pRFB25s-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Fermentum odio eu feugiat pretium nibh ipsum consequat nisl. Non enim praesent elementum facilisis leo vel fringilla est ullamcorper. Nulla aliquet enim tortor at auctor urna. In arcu cursus euismod quis viverra nibh cras pulvinar mattis.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-paid-content",
          name: "Paid Content",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/sabri-tuzcu-kxR3hh0IRHU-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Nulla facilisi nullam vehicula ipsum. Sit amet tellus cras adipiscing enim eu turpis egestas pretium. Diam phasellus vestibulum lorem sed risus ultricies.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/cardmapr-nl-s8F8yglbpjo-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Dictum fusce ut placerat orci nulla. Quis ipsum suspendisse ultrices gravida dictum fusce ut placerat.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/leon-seibert-Xs3al4NpIFQ-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Sed cras ornare arcu dui vivamus. Eget nunc lobortis mattis aliquam faucibus purus in. Nulla facilisi nullam vehicula ipsum a. Sed faucibus turpis in eu mi bibendum.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/sheelah-brennan-UOfERQF_pr4-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Mauris nunc congue nisi vitae suscipit tellus. Auctor augue mauris augue neque gravida in. Phasellus vestibulum lorem sed risus ultricies.",
                },
              ],
            },
          ],
        },
      ],
    },
    health: {
      name: "Health",
      url: "/health",
      priority: 2,
      sections: [
        {
          id: "content-health-trending",
          name: "Trending",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Mindfulness",
              url: "#",
              image: {
                src: "assets/images/benjamin-child-rOn57CBgyMo-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Consectetur lorem donec massa sapien faucibus et.",
              type: "list",
              content: [
                {
                  content:
                    "Eu turpis egestas pretium aenean pharetra. Nisl condimentum id venenatis a condimentum vitae sapien pellentesque habitant.",
                },
                {
                  content:
                    "Bibendum arcu vitae elementum curabitur vitae nunc sed velit dignissim.",
                },
                {
                  content:
                    "Eu non diam phasellus vestibulum lorem. Fermentum dui faucibus in ornare quam viverra orci sagittis.",
                },
                {
                  content:
                    "Et malesuada fames ac turpis. Ornare massa eget egestas purus viverra accumsan.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Latest research",
              url: "#",
              image: {
                src: "assets/images/louis-reed-pwcKF7L4-no-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Sed velit dignissim sodales ut eu sem integer vitae.",
              type: "list",
              content: [
                { content: "Metus vulputate eu scelerisque felis." },
                {
                  content:
                    "Aliquam sem et tortor consequat id. Feugiat nibh sed pulvinar proin.",
                },
                { content: "Quisque non tellus orci ac auctor augue." },
                {
                  content:
                    "Sed risus pretium quam vulputate dignissim. Vitae tortor condimentum lacinia quis vel eros.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Healthy Senior",
              url: "#",
              image: {
                src: "assets/images/esther-ann-glpYh1cWf0o-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Scelerisque in dictum non consectetur a.",
              type: "list",
              content: [
                {
                  content:
                    "Odio euismod lacinia at quis risus sed vulputate odio. Ullamcorper eget nulla facilisi etiam.",
                },
                {
                  content:
                    "Ipsum consequat nisl vel pretium. Nisi vitae suscipit tellus mauris a diam.",
                },
                {
                  content:
                    "Laoreet id donec ultrices tincidunt arcu non sodales neque sodales.",
                },
                {
                  content:
                    "At volutpat diam ut venenatis tellus in metus vulputate eu.",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-latest-facts",
          name: "Latest Facts",
          articles: [
            {
              class: "columns-3-balanced",
              header: "More Life, But Better",
              image: {
                src: "assets/images/melissa-askew-8n00CqwnqO8-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Sed tempus urna et pharetra pharetra massa massa ultricies mi.",
              type: "list",
              content: [
                {
                  content:
                    "Pharetra vel turpis nunc eget. Eu feugiat pretium nibh ipsum consequat.",
                },
                {
                  content:
                    "Velit dignissim sodales ut eu sem. Viverra accumsan in nisl nisi scelerisque eu ultrices.",
                },
                {
                  content:
                    "Arcu dictum varius duis at consectetur lorem donec massa sapien.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "In case you missed it",
              image: {
                src: "assets/images/marcelo-leal-6pcGTJDuf6M-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Egestas pretium aenean pharetra magna ac.",
              type: "text",
              content: `Lectus proin nibh nisl condimentum id venenatis a condimentum vitae. Tincidunt praesent semper feugiat nibh sed pulvinar proin.

Quis ipsum suspendisse ultrices gravida dictum fusce. Id donec ultrices tincidunt arcu non. Pellentesque habitant morbi tristique senectus et netus et malesuada fames.`,
            },
            {
              class: "columns-3-balanced",
              header: "Space and science",
              image: {
                src: "assets/images/nasa-cIX5TlQ_FgM-unsplash_448.jpg",
                alt: "Placeholder",
                width: "448",
                height: "252",
              },
              meta: { captions: "Photo taken by someone." },
              title: "Vitae ultricies leo integer malesuada nunc vel risus.",
              type: "list",
              display: "bullets",
              content: [
                {
                  content: "Semper eget duis at tellus at urna condimentum.",
                  url: "#",
                },
                {
                  content:
                    "Aliquet lectus proin nibh nisl condimentum id. Velit scelerisque in dictum non.",
                  url: "#",
                },
                {
                  content:
                    "Nulla posuere sollicitudin aliquam ultrices sagittis orci.",
                  url: "#",
                },
                {
                  content:
                    "Condimentum vitae sapien pellentesque habitant. Iaculis at erat pellentesque adipiscing commodo elit at imperdiet.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-medical-breakthroughs",
          name: "Medical Breakthroughs",
          articles: [
            {
              class: "columns-3-wide",
              header: "Surgical Inventions",
              url: "#",
              image: {
                src: "assets/images/national-cancer-institute-A2CK97sS0ns-unsplash_684.jpg",
                alt: "Placeholder",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "Photo taken by someone.",
                tag: { type: "breaking", label: "breaking" },
              },
              title:
                "Nisi est sit amet facilisis magna etiam tempor. Cursus eget nunc scelerisque viverra mauris in aliquam sem fringilla.",
              type: "text",
              content:
                "Ut eu sem integer vitae justo eget. Ut aliquam purus sit amet luctus. Sit amet mauris commodo quis imperdiet massa tincidunt. Tellus rutrum tellus pellentesque eu tincidunt tortor aliquam nulla facilisi. Turpis nunc eget lorem dolor sed. Ultrices in iaculis nunc sed augue lacus. Quam elementum pulvinar etiam non. Urna cursus eget nunc scelerisque. Nisl purus in mollis nunc sed.",
            },
            {
              class: "columns-3-narrow",
              header: "Medicare",
              url: "#",
              image: {
                src: "assets/images/national-cancer-institute-NFvdKIhxYlU-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Cras semper auctor neque vitae. Vel turpis nunc eget lorem dolor sed viverra ipsum nunc.",
              type: "text",
              content: `Lacus sed viverra tellus in hac habitasse. Sapien faucibus et molestie ac feugiat sed lectus. Pretium aenean pharetra magna ac. Volutpat odio facilisis mauris sit amet massa vitae tortor condimentum. Pellentesque massa placerat duis ultricies lacus sed turpis tincidunt id.

Parturient montes nascetur ridiculus mus mauris. Ultrices eros in cursus turpis. Bibendum at varius vel pharetra vel turpis. Luctus venenatis lectus magna fringilla urna porttitor rhoncus dolor.`,
            },
            {
              class: "columns-3-narrow",
              header: "Medication",
              url: "#",
              image: {
                src: "assets/images/myriam-zilles-KltoLK6Mk-g-unsplash_336.jpg",
                alt: "Placeholder",
                width: "336",
                height: "189",
              },
              meta: { captions: "Photo taken by someone." },
              title:
                "Ipsum dolor sit amet consectetur adipiscing elit. Velit scelerisque in dictum non consectetur a erat nam.",
              type: "text",
              content: `Mattis molestie a iaculis at erat pellentesque adipiscing. Sed augue lacus viverra vitae congue. Volutpat consequat mauris nunc congue nisi vitae suscipit tellus. Lacus laoreet non curabitur gravida arcu. Nisl nisi scelerisque eu ultrices vitae auctor.

Integer vitae justo eget magna fermentum iaculis eu non. Sollicitudin ac orci phasellus egestas. Ligula ullamcorper malesuada proin libero nunc consequat interdum.`,
            },
          ],
        },
        {
          id: "content-health-latest-videos",
          name: "Latest Videos",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/mufid-majnun-J12RfFH-2ZE-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/irwan-rbDE93-0hHs-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/hyttalo-souza-a1p0Z7RSkL8-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
                {
                  image: {
                    src: "assets/images/jaron-nix-7wWRXewYCH4-unsplash_684.jpg",
                    alt: "Placeholder",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "watch" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-health-educational",
          name: "Educational",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/bruno-nascimento-PHIgYUGQPvU-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Orci phasellus egestas tellus rutrum tellus pellentesque eu. Pulvinar neque laoreet suspendisse interdum consectetur. Viverra maecenas accumsan lacus vel facilisis volutpat. Nibh ipsum consequat nisl vel pretium lectus quam id. Leo integer malesuada nunc vel risus commodo viverra.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/brooke-lark-lcZ9NxhOSlo-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Proin libero nunc consequat interdum varius sit amet. Convallis posuere morbi leo urna molestie at. Consectetur lorem donec massa sapien faucibus et molestie ac feugiat. Egestas diam in arcu cursus euismod quis viverra nibh.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/kelly-sikkema-WIYtZU3PxsI-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Elit sed vulputate mi sit. Ullamcorper a lacus vestibulum sed arcu non odio euismod lacinia. Magna eget est lorem ipsum dolor sit amet consectetur. In tellus integer feugiat scelerisque varius morbi enim nunc faucibus. Nam libero justo laoreet sit.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/robina-weermeijer-Pw9aFhc92P8-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Nam aliquam sem et tortor consequat. Non sodales neque sodales ut etiam sit amet nisl purus. Viverra mauris in aliquam sem. Leo vel fringilla est ullamcorper. Tellus at urna condimentum mattis pellentesque id nibh tortor. Lacus laoreet non curabitur gravida. Ut morbi tincidunt augue interdum velit euismod in pellentesque.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/sj-objio-8hHxO3iYuU0-unsplash_448.jpg",
                    alt: "Placeholder",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Egestas integer eget aliquet nibh praesent tristique magna sit. Id consectetur purus ut faucibus. Molestie a iaculis at erat pellentesque adipiscing commodo elit at. Nulla facilisi etiam dignissim diam quis enim lobortis scelerisque. Lectus proin nibh nisl condimentum id. Ornare quam viverra orci sagittis eu volutpat odio facilisis mauris.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-fitness",
          name: "Fitness",
          articles: [
            {
              class: "columns-wrap",
              header: "Burn your calories",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/scott-webb-U5kQvbQWoG0-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Dictumst quisque sagittis purus sit amet volutpat consequat. At imperdiet dui accumsan sit amet nulla facilisi. Felis bibendum ut tristique et egestas. Mus mauris vitae ultricies leo integer malesuada. Adipiscing at in tellus integer feugiat.",
                },
                {
                  image: {
                    src: "assets/images/sven-mieke-Lx_GDv7VA9M-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Morbi non arcu risus quis varius quam quisque id. Enim nulla aliquet porttitor lacus luctus. Quis imperdiet massa tincidunt nunc pulvinar sapien et ligula ullamcorper. Tempor id eu nisl nunc mi ipsum faucibus vitae aliquet. Consequat semper viverra nam libero justo laoreet sit.",
                },
                {
                  image: {
                    src: "assets/images/geert-pieters-NbpUM86Jo8Y-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Suscipit adipiscing bibendum est ultricies integer quis auctor elit. Gravida quis blandit turpis cursus in hac habitasse platea. Maecenas ultricies mi eget mauris pharetra et ultrices. Massa sed elementum tempus egestas sed.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Gym favorites",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/boxed-water-is-better-y-TpYAlcBYM-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nulla facilisi nullam vehicula ipsum a arcu cursus. Et ultrices neque ornare aenean euismod elementum nisi quis. Velit euismod in pellentesque massa. In fermentum posuere urna nec tincidunt praesent semper.",
                },
                {
                  image: {
                    src: "assets/images/jonathan-borba-lrQPTQs7nQQ-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Sit amet consectetur adipiscing elit duis tristique sollicitudin. Ante metus dictum at tempor commodo ullamcorper. Tincidunt eget nullam non nisi est sit. Platea dictumst quisque sagittis purus sit amet volutpat consequat.",
                },
                {
                  image: {
                    src: "assets/images/mr-lee-f4RBYsY2hxA-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Sed vulputate odio ut enim blandit volutpat maecenas. Risus viverra adipiscing at in. Fusce id velit ut tortor pretium viverra. Sem nulla pharetra diam sit amet nisl. Posuere urna nec tincidunt praesent semper feugiat nibh.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Pilates",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ahmet-kurt-WviyUzOg4RU-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Massa massa ultricies mi quis hendrerit dolor magna. Cursus vitae congue mauris rhoncus aenean vel elit scelerisque. Vestibulum lorem sed risus ultricies tristique. Egestas fringilla phasellus faucibus scelerisque eleifend donec pretium vulputate.",
                },
                {
                  image: {
                    src: "assets/images/stan-georgiev-pvNxRUq7O7U-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Massa enim nec dui nunc mattis enim ut tellus elementum. Eros in cursus turpis massa tincidunt dui. Sit amet consectetur adipiscing elit ut aliquam purus sit amet. Eget nullam non nisi est sit amet facilisis magna.",
                },
                {
                  image: {
                    src: "assets/images/ahmet-kurt-5BGg2L5nhlU-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "enenatis tellus in metus vulputate eu scelerisque felis imperdiet proin. In eu mi bibendum neque egestas congue quisque egestas. Bibendum est ultricies integer quis auctor elit. Ipsum nunc aliquet bibendum enim facilisis. Magna fringilla urna porttitor rhoncus dolor purus non enim praesent.",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-guides",
          name: "Guides",
          articles: [
            {
              class: "columns-3-balanced",
              header: "Health after 50",
              type: "articles-list",
              content: [
                {
                  title: "Ac ut consequat semper viverra nam libero justo.",
                  content:
                    "A lacus vestibulum sed arcu non odio euismod lacinia at. Viverra mauris in aliquam sem fringilla ut morbi tincidunt augue. Enim nec dui nunc mattis enim ut tellus. Congue eu consequat ac felis donec et odio. Vitae sapien pellentesque habitant morbi tristique senectus.",
                },
                {
                  title:
                    "Sit amet porttitor eget dolor morbi non arcu risus quis.",
                  content:
                    "Gravida in fermentum et sollicitudin. Diam sollicitudin tempor id eu nisl. Proin libero nunc consequat interdum varius sit amet. Nunc pulvinar sapien et ligula ullamcorper malesuada proin libero. Lacinia quis vel eros donec ac.",
                },
                {
                  title: "Faucibus nisl tincidunt eget nullam non nisi.",
                  content:
                    "Diam ut venenatis tellus in metus. Luctus accumsan tortor posuere ac. Eget aliquet nibh praesent tristique magna. Diam donec adipiscing tristique risus nec feugiat in fermentum posuere. Dolor morbi non arcu risus quis varius quam quisque.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Healthy Heart",
              type: "articles-list",
              content: [
                {
                  title:
                    "Gravida cum sociis natoque penatibus et magnis dis parturient montes.",
                  content:
                    "Nulla porttitor massa id neque aliquam vestibulum morbi. Nullam non nisi est sit amet facilisis. Vitae turpis massa sed elementum tempus. Varius duis at consectetur lorem. Consequat semper viverra nam libero justo laoreet sit.",
                },
                {
                  title:
                    "Non nisi est sit amet facilisis magna etiam tempor orci.",
                  content:
                    "At augue eget arcu dictum varius duis at. Arcu felis bibendum ut tristique et egestas. Elementum tempus egestas sed sed risus pretium quam vulputate. Cursus euismod quis viverra nibh cras pulvinar. Praesent tristique magna sit amet purus gravida quis.",
                },
                {
                  title:
                    "Sit amet justo donec enim diam vulputate ut pharetra.",
                  content:
                    "Nulla at volutpat diam ut venenatis tellus. Pulvinar mattis nunc sed blandit libero volutpat. Sit amet justo donec enim diam vulputate. Condimentum id venenatis a condimentum vitae sapien pellentesque habitant.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "Healthy Digestive",
              type: "articles-list",
              content: [
                {
                  title:
                    "Metus aliquam eleifend mi in nulla posuere sollicitudin.",
                  content:
                    "Sodales ut etiam sit amet nisl purus in. Lorem ipsum dolor sit amet consectetur. Tincidunt ornare massa eget egestas purus viverra accumsan in. Orci eu lobortis elementum nibh tellus molestie nunc non. Ut faucibus pulvinar elementum integer enim neque.",
                },
                {
                  title:
                    "Placerat duis ultricies lacus sed. Donec enim diam vulputate ut.",
                  content:
                    "Condimentum id venenatis a condimentum vitae sapien. Eu ultrices vitae auctor eu augue ut lectus. Fermentum iaculis eu non diam phasellus. Urna nunc id cursus metus aliquam eleifend mi. Venenatis cras sed felis eget velit aliquet sagittis.",
                },
                {
                  title:
                    "Rhoncus dolor purus non enim praesent elementum facilisis.",
                  content:
                    "Nunc consequat interdum varius sit. Non diam phasellus vestibulum lorem sed risus ultricies. Feugiat nibh sed pulvinar proin gravida hendrerit lectus a. Eget egestas purus viverra accumsan in nisl nisi scelerisque.",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-underscored",
          name: "Underscored",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/drew-hays-tGYrlchfObE-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Lectus arcu bibendum at varius. Sed id semper risus in hendrerit gravida rutrum. Bibendum ut tristique et egestas quis ipsum suspendisse ultrices gravida. Euismod nisi porta lorem mollis. At varius vel pharetra vel turpis.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/hush-naidoo-jade-photography-Zp7ebyti3MU-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Pretium aenean pharetra magna ac placerat vestibulum lectus mauris ultrices. Lacus sed turpis tincidunt id. Eget nunc scelerisque viverra mauris in aliquam sem fringilla ut. Dapibus ultrices in iaculis nunc sed.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/mathurin-napoly-matnapo-ejWJ3a92FEs-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Tempus iaculis urna id volutpat lacus laoreet non. Elementum nisi quis eleifend quam adipiscing vitae proin. Vel pretium lectus quam id leo. Eget sit amet tellus cras adipiscing enim eu turpis.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/national-cancer-institute-KrsoedfRAf4-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Sapien nec sagittis aliquam malesuada bibendum arcu vitae. Adipiscing vitae proin sagittis nisl rhoncus. Euismod in pellentesque massa placerat duis. Nec tincidunt praesent semper feugiat nibh sed pulvinar proin. Quam nulla porttitor massa id neque.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-what-to-eat",
          name: "What to eat",
          articles: [
            {
              class: "columns-wrap",
              header: "Low carbs",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/kenny-eliason-5ddH9Y2accI-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Nec feugiat in fermentum posuere urna. Odio ut sem nulla pharetra. Est ultricies integer quis auctor elit sed. Dignissim cras tincidunt lobortis feugiat vivamus at augue eget.",
                },
                {
                  image: {
                    src: "assets/images/brigitte-tohm-iIupxcq-yH4-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Egestas sed tempus urna et. Lorem ipsum dolor sit amet consectetur adipiscing elit pellentesque habitant.",
                },
                {
                  image: {
                    src: "assets/images/brooke-lark-oaz0raysASk-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Sapien pellentesque habitant morbi tristique senectus et netus et malesuada. Dictum non consectetur a erat. Duis ut diam quam nulla porttitor.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Vegetarian",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/christina-rumpf-gUU4MF87Ipw-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Sed viverra tellus in hac habitasse platea dictumst vestibulum. Nisi est sit amet facilisis magna etiam.",
                },
                {
                  image: {
                    src: "assets/images/nathan-dumlao-bRdRUUtbxO0-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Convallis a cras semper auctor neque vitae tempus. Cursus risus at ultrices mi tempus imperdiet nulla.",
                },
                {
                  image: {
                    src: "assets/images/maddi-bazzocco-qKbHvzXb85A-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Ut faucibus pulvinar elementum integer enim neque volutpat. Netus et malesuada fames ac turpis egestas sed tempus urna.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "Breakfast",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/brooke-lark-IDTEXXXfS44-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Eget mauris pharetra et ultrices. In ante metus dictum at tempor commodo ullamcorper a. Ut sem nulla pharetra diam sit.",
                },
                {
                  image: {
                    src: "assets/images/joseph-gonzalez-QaGDmf5tMiE-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Lacus sed turpis tincidunt id aliquet risus. Nulla facilisi etiam dignissim diam quis enim. Non curabitur gravida arcu ac tortor dignissim convallis aenean.",
                },
                {
                  image: {
                    src: "assets/images/brooke-lark-GJMlSBS0FhU-unsplash_150.jpg",
                    alt: "Placeholder",
                    width: "150",
                    height: "84",
                  },
                  text: "Aliquam etiam erat velit scelerisque in dictum non. Pretium fusce id velit ut tortor pretium viverra.",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-hot-topics",
          name: "Hot Topics",
          articles: [
            {
              class: "columns-2-balanced",
              header: "This First",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/national-cancer-institute-cw2Zn2ZQ9YQ-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Augue ut lectus arcu bibendum at varius. Cursus turpis massa tincidunt dui. Feugiat scelerisque varius morbi enim. Vel orci porta non pulvinar. Est velit egestas dui id ornare arcu odio. Amet porttitor eget dolor morbi non arcu risus quis. Turpis in eu mi bibendum neque egestas.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/national-cancer-institute-GcrSgHDrniY-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "new" } },
                  text: "Et pharetra pharetra massa massa. Commodo odio aenean sed adipiscing diam donec adipiscing. In mollis nunc sed id semper risus in hendrerit. A diam sollicitudin tempor id eu nisl nunc. Sit amet consectetur adipiscing elit duis tristique.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "This Second",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/national-cancer-institute-SMxzEaidR20-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Ac tincidunt vitae semper quis lectus nulla. Porttitor massa id neque aliquam. Sed faucibus turpis in eu mi bibendum neque egestas congue. Tincidunt id aliquet risus feugiat in ante metus. Hendrerit gravida rutrum quisque non tellus orci ac auctor augue. Augue eget arcu dictum varius duis at.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/national-cancer-institute-L7en7Lb-Ovc-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "breaking" } },
                  text: "Feugiat pretium nibh ipsum consequat nisl vel pretium lectus quam. Ipsum dolor sit amet consectetur. Non diam phasellus vestibulum lorem sed risus. Porttitor lacus luctus accumsan tortor. Morbi enim nunc faucibus a pellentesque sit amet porttitor. Vel turpis nunc eget lorem. Ligula ullamcorper malesuada proin libero.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-paid-content",
          name: "Paid Content",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/dom-hill-nimElTcTNyY-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Eu sem integer vitae justo eget magna fermentum iaculis. Aenean pharetra magna ac placerat vestibulum lectus. Amet commodo nulla facilisi nullam.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/sarah-dorweiler-gUPiTDBdRe4-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Nullam vehicula ipsum a arcu cursus vitae congue. Enim ut tellus elementum sagittis vitae et leo duis. Nulla malesuada pellentesque elit eget.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/icons8-team-k5fUTay0ghw-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Est velit egestas dui id ornare arcu odio. Urna nunc id cursus metus. Pellentesque adipiscing commodo elit at imperdiet dui accumsan sit. At ultrices mi tempus imperdiet nulla malesuada pellentesque elit.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/jessica-weiller-So4eFi-d1nc-unsplash_336.jpg",
                    alt: "Placeholder",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "Erat imperdiet sed euismod nisi porta. Nullam ac tortor vitae purus faucibus ornare. Feugiat nisl pretium fusce id. Massa enim nec dui nunc mattis enim ut tellus elementum.",
                },
              ],
            },
          ],
        },
      ],
    },
  },
  O_ = {
    home: {
      name: "表紙",
      url: "/",
      priority: 0,
      notification: {
        name: "cookies",
        title: "このウェブサイトは Cookie を使用しています 🍪",
        description:
          "Cookieを使用して、サイトでのエクスペリエンスを向上させ、可能な限り最も関連性の高いコンテンツを表示します。詳細については、プライバシーポリシーとCookieポリシーをお読みください。",
        actions: [
          { name: "キャンセル", priority: "secondary", type: "reject" },
          { name: "受け入れる", priority: "primary", type: "accept" },
        ],
      },
      sections: [
        {
          id: "content-frontpage-breaking-news",
          name: "ニュース速報",
          articles: [
            {
              class: "columns-3-narrow",
              header: "無修正",
              url: "#",
              image: {
                src: "assets/images/isai-ramos-Sp70YIWtuM8-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "今ターゲットに、私の非常に喉の命のバナナ。",
              type: "text",
              content: `サッカーとしてサッカーのメンバーをお願いします。しかし、時間は都市と震えです。チリの非ポートティターの範囲。今の生活の要素ですが、屋外サッカーのメンバー。臨床矢印には注意が払われていません。

最新のサラダのエコロジーサッカー開発者プロパガンダは時々望んでいます。今、ターゲットに、私の喉の寿命バナナまたは。さて、私の非常に喉の人生のバナナ。は、補給の学部開発者栄養を強化しました。開発者の著者である整数。開発者の著者ですが、Vulputateは私の愛する人です。病気のようなサラダ生態学の漫画モーリス。必須の滅菌ポットランニングは、今すぐチョコレートが必要です。`,
            },
            {
              class: "columns-3-wide",
              header: "より多くのトップストーリー",
              url: "#",
              image: {
                src: "assets/images/nasa-dCgbRAQmTQA-unsplash_684.jpg",
                alt: "プレースホルダー",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "誰かが撮影した写真。",
                tag: { type: "breaking", label: "速報" },
              },
              title:
                "優れたウォームアップサッカー、直径の栄養は必要ありません。",
              type: "text",
              content: `プッシュは、時々セットアップされるマッサージジョーのパッチにすぎません。チャンピオンシップは、さまざまなものや震え、またはチケットで飲みます。並んでいる賢い病気や笑顔の弧が必要です。臨床プロテインバスケットボールでの憎しみ。

人生の人生のチャンピオンですが、今では彼はサッカーのメンバーを望んでいます。ストレスバスケットボールの妊娠は、臨床に投資するための臨床的です。明日からのブラビダまたはバレーは常にauctorです。サッカーの抗酸化物質ですが、サッカーは利便性を嫌います.`,
            },
            {
              class: "columns-3-narrow",
              header: "犯罪と正義",
              url: "#",
              image: {
                src: "assets/images/jordhan-madec-AD5ylD2T0UY-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "フットボールサラダ整数のライフセラピーには、大規模なウォームアップワークが必要です.",
              type: "text",
              content: `プロパガンダは、アークがさまざまな宿題をする必要があります。アルコールコースの乗り物の車両最大のジャスミン。しかし、時間は都市と震えです。従業員とEUですが、製造業に選ばれました

価格矢印、一部の男性はアルコールを飲みます。栄養径の週末の利便性ですが、貧困。マクロ写真は明日のゲートニブネナティスですが、。喉または従業員とEUですが、選択された製造ロット。人生の要素と生命の要素。または開発者委託もちろん醜い大衆開発者はそれをduiします。ですが、それはいつもです。プロパガンダ・モーリス・オージューの著者。しかし、今走っているポットの著者。`,
            },
          ],
        },
        {
          id: "content-frontpage-latest-news",
          name: "最新ニュース",
          articles: [
            {
              class: "columns-3-balanced",
              header: "今起こってる",
              type: "articles-list",
              content: [
                {
                  title: "ロレム非常にニンジン.",
                  content:
                    "従業員は今では座りません。しかし、サッカーのプール、しかし湖であるが漫画が入っている場合。これが喉の主な唐辛子です。しかし、恐怖を起こしているポットの著者。現在ののふもとにある。サッカーは悲しい老人とネトゥスとマレスアダの飢er。",
                },
                {
                  title: "強化された監視手順.",
                  content:
                    "いくつかの唐辛子を手に入れるための主流の補給学部開発者。選択されたレシピプレーヤーまたは価格。トマト唐辛子が喉のバレーボール要素に綴られた矢印。マレスアダの一部の矢はアルコールを飲むこともありません。",
                },
                {
                  title: "しかし、私は事件の時にイニコッドでやっています.",
                  content:
                    "プルまたはラリートマッサージは、無料のIDスロートプレーヤーを強化することがあります。飲み物は、ですが、Miを履行する超整形整数です。明日サッカーアークドゥイライブアルコールフットボール。機能的な現在、ロボルティスの喉がたくさんあります。開発者の資金調達は、妊娠する必要があります。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "注目に値します",
              image: {
                src: "assets/images/peter-lawrence-rXZa4ufjoGw-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "プロパガンダまたはウォームアップとケアと臨床で妊娠.",
              type: "list",
              content: [
                {
                  content:
                    "私はジャスミンが嫌いなサッカーが嫌いですが、学部の直径まで。",
                },
                {
                  content:
                    "レシピは常に無料で引っ張ってください。著者の温度のためにバナナはありません。",
                },
                {
                  content:
                    "たとえば、サッカーのugいの喉。たくさんの楽しみがない限り、何もありません",
                },
                {
                  content:
                    "人生は栄養や一部のサラダよりも時です。臨床フットボールカートンエレメント楽しいテレビでさえも.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "世界中に",
              image: {
                src: "assets/images/rufinochka-XonjCOZZN_w-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "今、フェリス・アース、バスケットボールにはマスAC、カートン・ラリート・ロレムが必要です。",
              type: "list",
              content: [
                {
                  content:
                    "ニブ・モーリスレーサーマティスの従業員。さまざまな震えまたはターピスがロアムの痛みが必要になりました。",
                },
                {
                  content:
                    "をキャンセルします。それはマクロ価格を取得してください。アースペレンテスクのサッカー開発者はゼロを温める.",
                },
                { content: "週末の座りの週末にはしばらくしてください。" },
                {
                  content:
                    "サッカーのジョーのトラブルシューティング。このコースでは、は栄養開発者です。",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-latest-media",
          name: "最新のメディア",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/steven-van-bTPP3jBnOb8-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-WUehAgqO5hE-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/david-everett-strickler-igCBFrMd11I-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/marco-oriolesi-wqLGlhjr6Og-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-highlights",
          name: "ハイライト",
          articles: [
            {
              class: "columns-wrap",
              header: "国内のハイライト",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/samuel-schroth-hyPt63Df3Dw-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ニスルまたはライオンではなくベッドの価格。アルコールを就寝するプロパガンダ。メキシコの妊娠中の男性を縁します。臨床チリのバスケットボールの矢は常に宿題が必要です。現在のタンクは、多くの妊娠中の男性にとって重要です。ドライバーもあまりいません。妊娠は、投資または臨床栄養の臨床的です。コンビニエンスゼロ。アークランニングパフォーマンスのバニーの直径。",
                },
                {
                  image: {
                    src: "assets/images/denys-nevozhai-7nrsVjvALnA-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "喉のカートンチョコレートウォームアップの場合。人生は常に週末に選ばれた人です。チョコレートフットボールバスケットボールのキャリア著者を除いて選手で。",
                },
                {
                  image: {
                    src: "assets/images/mattia-bericchia-xkD79yf4tb8-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ロレムは大量のサピエンの喉と従業員とEUまで。誰もがすべてのIDの直径よりも変化している人。タンクと執行として推奨されます。発酵ポットまたは開発者が常にEUを設定します。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "グローバルハイライト",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maximilian-bungart-nwqfl_HtJjk-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "プロパガンダは、メインでさまざまな宿題をする必要があります。DUIのアルコールに弓をすごい、飲み物を引き起こします。ほとんどの場合、顧客のニンジンが必要です。は現在バレーボールサピエンであり、の履歴書を予約しました。",
                },
                {
                  image: {
                    src: "assets/images/gaku-suyama-VyiLZUcdJv0-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ベッドのナムプッシュホームワークバレーテルスID。醜いバニーですが、時間はです。チョコレートチョコレートチョコレートチョコレートファイナンスライオン。マクロID調査が選択されました。各矢印の唐辛子の通りを少しずつ循環します。ライフサスシピットテラスモーリスは直径です。",
                },
                {
                  image: {
                    src: "assets/images/paul-bill-HLuPjCa6IYw-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "チョコレートフットボールバスケットボールのキャリア著者の場合。一つの笑顔ですが、紫色の憎しみは嫌いです。栄養ニンジンの革新的な化学サッカーはそうではありません。ヌラムはサッカーが必要な今、カートンロットが必要です。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "ローカルハイライト",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maarten-van-den-heuvel-gZXx8lKAb7Y-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "今は無料の週末に座ってください。笑い、サッカーの必要はありません。開発者プロパガンダタンパク質のパフォーマンス。週末には、恐怖の紫色で滅菌された領域としての直径があります。来たのはバニーに投資するために重要です。モーリスランニングの前でサッカーマッサージ。",
                },
                {
                  image: {
                    src: "assets/images/quino-al-KydWCDJe9s0-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "しかし醜い。明日、しかし、サッカーはメインチリである屋外バナナの矢を必要としています。明日、しかしサッカーにはゲームが必要です。必須のミネアポリスの学部開発者栄養。笑い声を上げる学部生。しかし、笑顔は温度のためにバナナを悲しみません。バスケットボール開発者はメンバーではなく、。",
                },
                {
                  image: {
                    src: "assets/images/mathurin-napoly-matnapo-pIJ34ZrZEEw-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "人生は栄養や一部のサラダよりも時です。生態学的なポットの革新的な痛みは、現在の要素ではありません。家畜の生活をしない限り、面白いモーリスは直径のメーセナスです。直径の変数である男性。",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-top-stories",
          name: "トップストーリー",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/andrew-solok-LbckXdUVOlY-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "のリスクで地域で滅菌される。いいえ、中国のチョコレートの世話をしてください。今はマティスですが、週末は無料ですが、明日のサッカーアーク。サッカープロパガンダのチョコレートフットボールバスケットボールのキャリア著者。無料のtは、最初のランニングニンジンです。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/hassan-kibwana-fmXLB_uHIh4-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "バルプタートMIニンジンが始まります。控除可能な場合を除き、ジャスミンのパフォーマンス要素。チョコレートチョコレートの紫色。人生の開発者もこれまででもありません。今ではソフトの多くのプレイヤーでさえ。検査は、牛肉だけでなく、どんな笑顔でもパフォーマンススカートを嫌います。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/craig-manners-LvJCFOW3Ma8-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "顧客のチームのニンジンが必要です。プロパガンダでのライブアークは、メインでさまざまな宿題を言っています。サッカーの喉に熱顎を感じ、臨床矢を引っ張ります。ライフサピエン栄養居住者サッカー悲しい老年。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/albert-stoynov-fEdf0fig3os-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "アークランニングパフォーマンスQuisプルモーレーバレーボールのダイアム。シナリオの軽を飾りたいという願いです。テクノロジーは常に著者です。非常にマッサージバスケットボール妊娠臨床順序。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ehimetalor-akhere-unuabona-yS0uBoF4xDo-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "EUチョコレート全体のサッカー全体の伯爵。学部生の悲しい笑顔または発酵におけるFeugiatまでの直径。キャンセル抗酸化物質のキャンセルモーリスは生命の香りを座らせます。ターゲットを絞った臨床タンパク質バスケットボールに尋問されましたが、。ジャスミンまたは開発者チョコレートモーリス栄養バレーボール。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-international",
          name: "国際的",
          articles: [
            {
              class: "columns-3-balanced",
              header: "ヨーロッパ",
              type: "articles-list",
              content: [
                {
                  title:
                    "レイヤーのニンジンに融資している開発者をお願いします。ストリートディクタムの製造範囲の種類。",
                  content:
                    "Orciとプロパガンダまたは妊娠の著者。キュラビトゥールは大規模な一時停止ですが、鍋の大きな生態学的な痛みではありません。アルコールの弓の矢印。栄養居住者のサッカー悲しい老人とネタス。また、声明にチョコレートを持っているか、強化された.",
                },
                {
                  title:
                    "ストレスバレーは、引き金と不動産になります。明日の喉のバスケットボールソース。",
                  content:
                    "またはEcological。醜い開発者ID調査は、恐怖の前でEUを笑顔にします。Viverra自体は現在、抗酸化物質のためにバナナを飲みます。非常に素晴らしいバスケットボールの妊娠しているタムと執行。晴れた老人とマレスアダの飢えと醜いニーズ。",
                },
                {
                  title:
                    "しかし、サッカーのプール、しかし湖であるが漫画が入っている場合。",
                  content:
                    "DUIライブアルコールトリガードリンク。パキスタン全体または週末のバレーボール要素の喉から。サッカーのプロパガンダの著者は、アルコール飲料を寝かせます。ダイアムウィークエンドの利便性ですが、執行執行生態学的.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "南アメリカ",
              type: "articles-list",
              content: [
                {
                  title: "プロパガンダの必要性は、さまざまな宿題です。",
                  content:
                    "プールを耐えてくださいが、パッチではありません。マクロチリテイストフットボール。それは時々、バスケットボールのかしいアルコールの願いです。",
                },
                {
                  title:
                    "サッカー開発者としての生態学的なプロパガンダタンパク質の子供たち。",
                  content:
                    "ライオンの病気を設定する醜い執行子供の直径の週末の利便性が嫌いです。痛みの質量は、プレイヤーの唐辛子漫画層を執行する必要があります。今のところ、チョコレートチョコレートバリエッドサッカーの伯爵。声明の屋外チョコレートまたは強化されたものでした。",
                },
                {
                  title: "はそれぞれを飲むか欲しいです。",
                  content:
                    "Sapienには電子レンジが必要ですが、無料です。重要な開発者の宿題。支払われたサッカーサーマル電話。マクロジャスミンと温度ですが、笑顔。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "アジア",
              type: "articles-list",
              content: [
                {
                  title:
                    "メンバーはあなたのプレイヤーチリでもあります。たとえば、サッカーの醜い人の喉、私の飲み物、そして",
                  content:
                    "DUI層のニンジンの資金調達において、ファシリシ病はありません。ロボルティスEUはプロパガンダに住んでいます。このコースでは、上記の製造範囲の通り。しかし、地球、ウルナソースの不動産ニブ温度で。ニブの前でのサッカーマッサージが始まります。",
                },
                {
                  title: "デートライオンポットを設定するためのハレトラバレー。",
                  content:
                    "ランニングパフォーマンスメールの。そして、醜い執行整数はバナナ・ニーブ・プレゼントが悲しい偉大なものを必要としています。要素フットボールの抗酸化物質での痴漢ですが、病気が嫌いです。さまざまなアルコールドリンクでのチャンピオンシップ。醜い大量開発者を実行するピーナッツ。",
                },
                {
                  title:
                    "さまざまなまたは震えまたはターピスでは、ロアムの痛みが必要になりました.",
                  content:
                    "ビューロースマートビッグケミカルロレム。サッカー明日開発者カートンEU。ライフマクロソースラシニアまたはエロスが嫌いになるまで。ニブの前でサッカーマッサージ。また、屋外チョコレートが強化されていませんでした。大量のヒントまでメインロレムで。しかし、カブトムシのニンジン。",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-featured",
          name: "特徴",
          articles: [
            {
              class: "columns-3-balanced",
              header: "ワシントン",
              image: {
                src: "assets/images/heidi-kaden-L_U4jhwZ6hY-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "臨床グラブは、最大のものであり、最大の資金を調達しています。",
              type: "list",
              display: "bullets",
              content: [
                { content: "正面にサッカーマッサージを添加します。", url: "#" },
                { content: "この十分の道路で醜い走りを設定します。", url: "#" },
                {
                  content:
                    "は、妊娠したアークとマクロサッカーバレーのチャットではありません。",
                  url: "#",
                },
                {
                  content: "チリは今ソフトですが、それはいつも笑顔です。 ",
                  url: "#",
                },
                { content: "タンクの入り口をセットアップする方法.", url: "#" },
                {
                  content:
                    "バナナの航空会社のストレスがないため、たくさんの紫色がたくさんあります。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "ニューヨーク",
              image: {
                src: "assets/images/hannah-busing-0V6DmTuJaIk-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "製造サッカーの生態学的、ID結果は航空会社のサッカーを控えています。",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "それは常に局の妊娠中の化粧の笑顔です。誰もがそうではありません。",
                  url: "#",
                },
                {
                  content:
                    "法執行機関に投資することは、資金調達でしたが、パフォーマンスでした。",
                  url: "#",
                },
                {
                  content:
                    "一部のは今、アルコールライフエレメントチャットライフを飲んでいます。",
                  url: "#",
                },
                {
                  content:
                    "gそして彼のパートナーを妊娠しています。タンクとバニーとして飲む。",
                  url: "#",
                },
                {
                  content: "はマティステレビをターゲットにしています。",
                  url: "#",
                },
                { content: "多くのライフマクロソーススカート。", url: "#" },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "天使たち",
              image: {
                src: "assets/images/martin-jernberg-jVNWCFwdjZU-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "ムーアはマウスを逃すために生まれます。",
              type: "list",
              display: "bullets",
              content: [
                {
                  content: "マティスは、要素の矢印をからかうことです。",
                  url: "#",
                },
                {
                  content:
                    "ニンジンの滅菌ポットランニングは、チョコレート漫画のモーリスになりました。",
                  url: "#",
                },
                {
                  content:
                    "Miは、それぞれの貧困を飲むか、パスポートを執行します。",
                  url: "#",
                },
                { content: "今、チョコレート漫画モーリス。", url: "#" },
                {
                  content:
                    "資金調達ですが、パフォーマンスが必要ですが、ゲートウェイロアムは柔らかくなります。",
                  url: "#",
                },
                {
                  content: "整数または週末のバレーボール要素の喉に.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-underscored",
          name: "強調されています",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/kevin-wang-t7vEVxwGGm0-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "しかし、サッカー開発者フットボール。サッカーの抗酸化物質ですが、便利なサッカーがジャスミンを嫌うサッカーは嫌いですが、しかし。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/claudio-schwarz-3cWxxW2ggKE-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "アース・オルシとプロパガンダ・モーリス・プロパガンダまたは妊娠中の著者。は週末ではありません。ただし、学部課程の直径。ドゥイ顎のカートンチョコレート発酵.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/braden-collum-9HI8UJMSdZA-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "しかし、便利なサッカーはジャスミンを嫌いますが、学部生の直径を嫌います。ニブ地域のテレビは現在、ミサに座っていません。が綴られた、または週末に綴られ、人生の開発者.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/geoff-scott-8lUTnkZXZSA-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "男性とモンテスを引っ張るとき。義人の生活のシナリオと、素晴らしいウォームアップワークの権利を活性化するため。必須の滅菌ポットランニングは、チョコレートの漫画が必要になりました。直径または要素の各ID。速いお金の気性はありません。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-happening-now",
          name: "今起こってる",
          articles: [
            {
              class: "columns-wrap",
              header: "政治的",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/jonathan-simcoe-S9J1HqoL9ns-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "明日は常に子供の頃の著者または生涯です。あなたがあなたの子供を憎むまでレシピとトリガー。私があなたの子供の直径の週末を嫌うまで、サッカーの写真とサッカー。地域のモーリスに直径を伝える可能性がありますが、の場合。",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-p2Xor4Lbrrk-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "しかし、サッカー、私の飲み物、そしてugいの喉はそうです。ソース栄養の居住者であるソースが病気の栄養供給の居住者であるソース。ターゲットを絞ったが、プロパガンダ車が引っ張っている。一部のサラダや。アース・モーリスは直径のメーセナスですが。",
                },
                {
                  image: {
                    src: "assets/images/marius-oprea-ySA9uj7zSmw-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "不動産のはバナナではありません。マクロサッカーのコンバリスジャスミン。中国のチョコレートの世話をセットアップするものは何もありません。あなたがあなたの栄養の直径を嫌うまでレシピとトリガー。ロレムは非常にニンジンのセクテトゥールの学部生。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "健康",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/jannis-brandt-mmsQUgMLqUo-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ライフマクロソーススカートメール。NISLしかし、チョコレートフットボールバスケットボールライフ。それは、多くのサラダの漫画マッサージのマクロ価格を確保してください。チョコレートフットボールのバスケットボールライフを除く選手の漫画層。",
                },
                {
                  image: {
                    src: "assets/images/martha-dominguez-de-gouveia-k-NnVZ-z26w-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "マイクロ波無料レシピ。資金調達ですが、パフォーマンスはドアだけです。人生のコースのチャンピオンシップ、ジャスミンの最大の範囲。今のところ、子供たちからの喉。ウォームアップとケアと臨床の専門家で妊娠しています。",
                },
                {
                  image: {
                    src: "assets/images/freestocks-nss2eRzQwgw-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "サッカーは悲しい老人とネトゥスとマレスアダの飢er。多くのランニングニンジン。直径としての寿命とライオンの宿題。それはベッドでの補償ではありません。ヘンドレリット消費者がそれを必要としています。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "仕事",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/little-plant-TZw891-oMio-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "整数は週末ではありません。しかし、選択された製造ロット。マイクロ波無料撮影写真は、時々異質なニンジンになります。マティステレビ栄養をターゲットにした。重要な開発者宿題タンクダイビング.",
                },
                {
                  image: {
                    src: "assets/images/allan-wadsworth-Lp78NT-mf9o-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "サッカーサラダなどのサッカーメンバー。はマティステレビをターゲットにしています。栄養として、またはサラダと温度の時間。この直径またはバレーボールの要素も同様ではありません。",
                },
                {
                  image: {
                    src: "assets/images/ant-rozetsky-SLIFI67jv5k-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ライフマクロソーススカートまたはエロの塊。栄養のストリートディクタムの製造範囲。今、そのサッカー選手を活性化する時間、私の非常に喉の生活。しかし、微笑むウルトレシーはバナナを悲しみません。偉大なニンジン唐辛子妊娠中の男が走り回って座っています。",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-hot-topics",
          name: "ホットな話題",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/alexandre-debieve-FO7JIlwjOtU-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "あなたが直径のになるまで必須。チョコレートフットボールのバスケットボールライフを除く選手の漫画層。プールですが、醜い開発者ID。ストーリーのこのコースで実行されます。人生のスケジュールである車.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/thisisengineering-ZPeXrWxOjRQ-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "漫画のスマートフォンとマクロ。地球は時々をお願いします。はパッケージの不動産になりました。のニンジンは微笑むことはありません。サッカーは必要ありません。たくさんの顎とマクロマッサージで引っ張られます。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/isaw-company-Oqv_bQbZgS8-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "湖に耐えてくださいが、アークはパフォーマンスを嫌いではありません。はい、プールにすぎません。これは、通りの製造範囲の通りに丸くなっています。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/aditya-chinchure-ZhQCZjr9fHo-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "パッケージのUNC不動産。抗酸化物質ですが、サッカーは利便性を嫌います。しかし、明日サッカーはアルコールです。今または笑顔で、メーセナス層のラクスを引いてください。オールドとネトゥスとマレスアダ。テレビは標的にされています.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-paid-content",
          name: "有料コンテンツ",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/tamara-bellis-IwVRO3TLjLc-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "今、バナナは妊娠抗酸化物質のために飲みます。酵母のFeugiatは鍋に置かれませんでした。要素フットボールの抗酸化物質での痴漢ですが、病気が嫌いです。チョコレートチリは常にパッケージで宿題が必要です。",
                },
                {
                  image: {
                    src: "assets/images/david-lezcano-NfZiOJzZgcg-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "スマートサッカーや笑顔の弧が必要です。妊娠中の弓と温度資産をチャットしないでください。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/heidi-fin-2TLREZi7BUg-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "マッサージジョーのパッチよりも。はまだプールです。明日の谷は常に著者または人生の時代です。震えるには、いくつかのID径がたくさんあります。",
                },
                {
                  image: {
                    src: "assets/images/joshua-rawson-harris-YNaSz-E7Qss-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "または生態学的なは、サッカーの直径でさえもファシリシを必要としません。フットボールネットワークレシピ。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/arturo-rey-5yP83RhaFGA-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "オルシ地域とプロパガンダ・モーリス・アウグーの著者も妊娠していません。自体は、アルコールコースからのものではありません。航空会社の大量IDやサッカーの一部がいかない方法。カートンチョコレートである人。",
                },
                {
                  image: {
                    src: "assets/images/clem-onojeghuo-RLJnH4Mt9A0-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "ハレトラの直径ニンジンプレイヤーはナゲットの箱を受け取ります。オールドとネトゥスとマレスアダの飢er。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/ashim-d-silva-ZmgJiztRHXE-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "警察はマティスの臨床または開発者でした。谷の端にある恐怖の利便性。サラダ製造の場合、酵母、私または範囲質量。",
                },
                {
                  image: {
                    src: "assets/images/toa-heftiba--abWByT3yg4-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "直径からのフェリーライオンの柔らかい層。今、バナナは妊娠中または谷抗酸化物質のために飲みます。",
                },
              ],
            },
          ],
        },
      ],
    },
    us: {
      name: "私たち。",
      url: "/us",
      priority: 1,
      message: {
        title: "ニュースブレイキングをご覧ください！",
        description: "重要なことが起こったので、あなたはそれを見るべきです！",
      },
      sections: [
        {
          id: "content-us-world-news",
          name: "世界のニュース",
          articles: [
            {
              class: "columns-3-wide",
              header: "今日起こっています",
              url: "#",
              image: {
                src: "assets/images/todd-trapani-vS54KomBEJU-unsplash_684.jpg",
                alt: "プレースホルダー",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "誰かが撮影した写真。",
                tag: { type: "breaking", label: "速報" },
              },
              title: "しかし、執行環境生態学的な栄養価の高い熱控除可能。",
              type: "text",
              content:
                "フットボールポットIDキャリー。さまざまな栄養開発者。メインロレムでさまざまな宿題をするまで。しかし、ウルナソース不動産栄養の地球。ミネアポリス・ロレムは大規模なサピエンの喉と従業員まで。私の時間のバスケットボールでの笑い。",
            },
            {
              class: "columns-3-narrow",
              header: "トレンド",
              url: "#",
              image: {
                src: "assets/images/mufid-majnun-tJJIGh703I4-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "偉大なものが必要な人生のシナリオを活性化するために。",
              type: "text",
              content: `それは、バスケットボールで笑いを走らせる特定の製造サッカーではありません。チャンピオンシップDUIライブアルコールトリガードリンクはタンクとして飲みます。直径牛肉まで.

デッキを卒業するための大量開発者. ソースマティス栄養IDニブ温度。 資金調達層のデュオ開発者は、なし。しかし、要素サッカーの抗酸化物質は誰の病気を嫌いますか。`,
            },
            {
              class: "columns-3-narrow",
              header: "天気",
              url: "#",
              image: {
                src: "assets/images/noaa--urO88VoCRE-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "喉のバレーボールの要素へのトマト唐辛子が綴られました。",
              type: "list",
              content: [
                {
                  content:
                    "栄養居住者サッカー悲しい老年。またはピーナッツティルと憎しみのオルシプロテインバスケットボール。",
                },
                {
                  content:
                    "私は栄養の直径の週末の利便性が嫌いですが、執行執行生態学的です。",
                },
                {
                  content:
                    "子供たちが革新的な化学的痛みを引き出します。矢を味わうための矢印。中国のレースは漫画をパフォーマンスします。",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-around-the-nation",
          name: "全国の周り",
          articles: [
            {
              class: "columns-3-balanced",
              header: "最新",
              image: {
                src: "assets/images/fons-heijnsbroek-vBfEZdpEr-E-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "ヌラムはサッカーが必要な今、カートンロットが必要です。",
              type: "list",
              content: [
                {
                  content:
                    "ニブ・アルコール写真選手または。オールドとネトゥスとマレスアダの飢er。",
                },
                { content: "サッカーサッカーが卒業してすみません。" },
                {
                  content:
                    "サッカーの写真とサッカーを計画するヴィタエまで. は最新の座りた週末に座っています。",
                },
                {
                  content:
                    "妊娠中のアルコールをチャットしません。それは重要な抗酸化物質であり、臨床的なサッカーのポリシーが大きいことです.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "仕事",
              image: {
                src: "assets/images/bram-naus-oqnVnI5ixHg-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "栄養開発者の製造範囲。首でカートンチョコレートのウォームアップ。",
              type: "list",
              content: [
                {
                  content:
                    "正面にある紫色のサッカーマッサージ。彼は人生計画のスケジュールを.",
                },
                {
                  content:
                    "現在、妊娠中の抗酸化物質や谷の調査飲み物です。ファレトラバレーは、サッカーライオンポットの従業員を要素に設定しました.",
                },
                {
                  content:
                    "またはバスケットボールDUI SAPIEN。しかし、学部生の悲しい笑顔またはEUまでの学部生の直径。残念ながら、バスケットボールの開発者は、メンバーのメンバーのメンバーではなく、同様です。臨床臨床臨床臨床臨床.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "政治",
              image: {
                src: "assets/images/hansjorg-keller-CQqyv5uldW4-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "局の痛みは非常に機能的です。また、今では不動産ではありません。",
              type: "list",
              content: [
                {
                  content:
                    "学部生のマイクロ波矢印を控除できる限り、パフォーマンス要素。",
                },
                {
                  content:
                    "そして、人生の開発者は、常に週末の直径でいつでも卒業した男です。",
                },
                {
                  content:
                    "今はソフトですが、それは常に局の笑顔です。大衆の質量ですが、ツイッターの要素ですが、笑顔です。",
                },
                {
                  content:
                    "それぞれまたは土壌臨床局妊娠中の化粧に常に微笑んでください。",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-roundup",
          name: "切り上げする",
          articles: [
            {
              class: "columns-wrap",
              header: "ワシントン",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/unseen-histories-4kYkKW8v8rY-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "しかし、チョコレートフットボールバスケットボールライフ。ミネアポリスの学部開発者宿題の悲しいケア。マッサージをデッキしますが、パッチだけです。素晴らしいウォームアップワークが必要です。",
                },
                {
                  image: {
                    src: "assets/images/ian-hutchinson-P8rgDtEFn7s-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "チョコレート全体のサッカー全体の伯爵。タンクとバニーのメールとして。サッカーを卒業したニンジンは、資金調達の執行の不動産です。",
                },
                {
                  image: {
                    src: "assets/images/koshu-kunii-ADLj1cyFfV8-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "彼の仲間を妊娠させる必要はありません。モーリス・モーリス・モーリス栄養サッカーサッカー悲しい老年.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "東海岸",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/matthew-landers-v8UgmRa6UDg-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "しかし、チョコレートフットボールバスケットボールライフ。施行生態学的栄養吸気熱熱。投資臨床ヌラ栄養サッカーは、滅菌されたポットがたくさんあります。タイム臨床サッカーカートンエレメントニブ楽しいテレビは今ではありません。悲しいビッグマンは偉大な​​妊娠中の男性で、走り回って座っています。",
                },
                {
                  image: {
                    src: "assets/images/c-j-1GHqOftzYo0-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "そして、非常にマッサージバスケットボールを執行します。レイヤーのニンジンの開発者にファシリシはありません。ランニングパフォーマンスquisは明日バレーボールロットを引きます。抗酸化物質ですが、病気が嫌いです。このコースでugい走り回っている強大な唐辛子妊娠。",
                },
                {
                  image: {
                    src: "assets/images/jacob-licht-8nA_iHrxHIo-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "地域の整数の学部生をプルします。控除可能な顧客と顧客がいない限り、サッカージャスミンのパフォーマンス要素もいません。無料の週末ですが、明日サッカーはアルコールです。質量をロックしますが、タイムバニーの要素ですが、笑顔の価格。サッカーの価格NIBHテクニックチュートリアルプレーヤーまたは価格。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "西海岸",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maria-lysenko-tZvkSuBleso-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "リンゴ、しかしこの十分の一の漫画地域。製造のプールですが、アークはパフォーマンススカートを嫌いません。明日ニブ・ヴェネナティス、しかしサッカーにはゲームが必要です。バレーボールの要素以上のものは、喉をマッサージするだけではありません。",
                },
                {
                  image: {
                    src: "assets/images/peter-thomas-17EJD0QdKFI-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "チョコレートを除くプレイヤーの漫画層。バレーはデートライオンポットの従業員を設定しました。しかし、都合の良い時点で湖があります。現在、大衆開発者に資金を提供している最大の利便性。フットボールの週末の憎しみを撃つ.",
                },
                {
                  image: {
                    src: "assets/images/xan-griffin-QxNkzEjB180-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "大衆不動産の宿題の子供たちのパフォーマンス。miはそれぞれを飲むか欲しいです。速いお金契約。さまざまなまたはファレトラまたは醜い.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-crime+justice",
          name: "犯罪と正義",
          articles: [
            {
              class: "columns-3-balanced",
              header: "最高裁判所",
              type: "articles-list",
              content: [
                {
                  title: "または笑顔や便利さを引っ張ります。",
                  content:
                    "Ollicitudinおよび臨床的ナグの執行Tellusメイクアップ。ミネアポリスAは宿題渓谷の鍋のベッドにいました。どちらのバスケットボールもいません。学部の悲しい笑顔またはEU。今、あなたは塊を座る必要はありません。明日マティスは今、座っています。",
                },
                {
                  title:
                    "臨床グラブは、最大のものであり、最大の資金を調達しています。",
                  content:
                    "消費者ニンジン補給学部開発者の栄養居住者。明日ニブ・ヴェネナティス、しかしサッカーにはゲームが必要です。のトーマスは、地域の整数の学部生をプルします。フェリスはタンクとして飲み、誰でもマッサージバスケットボールをします。",
                },
                {
                  title:
                    "サッカー開発者のように、いくつかのサラダ生態学のモーリス。",
                  content:
                    "エレメントフットボールの抗酸化物質での痴漢ですが、サッカーは利便性を嫌います。アースメイクアップセラミック栄養サッカー開発者の温度。醜い執従業員は今、座るミサがありません.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "地方法",
              type: "articles-list",
              content: [
                {
                  title: "直径の紫色のまでの多くの治療。",
                  content:
                    "開発者DUIは、段階的なニンジンを飾ります。しかし、私はBlandit Weekend Maecenasの週末に憎んでいます。ポットまたは開発者を入れて、常にであるがバレーボールをしてください。大衆不動産の宿題の子供たちのパフォーマンス。",
                },
                {
                  title:
                    "「チリの最新のバスケットボール矢印は常に宿題が必要です。",
                  content:
                    "ロボルティスEUはプロパガンダ化学アークに住んでいます。憎しみの弧の軽cであり、サラダが震えの直径のないことです。サッカー開発者のプロパガンダとして、サラダの生態学的なモーリス。パスポートアークランニングパフォーマンスの各執行の直径すべての漫画。",
                },
                {
                  title: "ご飯の中のチョコレートチョコレートチョコレート。",
                  content:
                    "塊で座ったり、価値があるのではありません。今、チョコレート漫画モーリス。トマトチリを喉の枕に調査してください。それは執行の不動産です。地域では、今のところは喉のバリエッドサッカーを整理する地域で.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "意見",
              type: "articles-list",
              content: [
                {
                  title: "優れていて、不動産製造が選択されています。",
                  content:
                    "エネナティス・ウルナランニングは今、チョコレート漫画モーリスを必要としています。谷は、デートライオンポットの従業員を要素に設定しました。フットボールカートンエレメントニブ地域。サッカーマッサージのチリのチップの人生、しかしプールだけですが、漫画。",
                },
                {
                  title:
                    "警察はマティスの臨床または開発者でした。谷の端にある恐怖の利便性。",
                  content:
                    "タンクニンジン。最新の唐辛子ニンジンは滅菌しました。今は人生ですが、サッカーのメンバーにお願いします。モーリス・モーリス・モーリス栄養サッカーサッカー悲しい老年。たくさんのスマイリー化学物質。",
                },
                {
                  title: "サラダ製造の場合、酵母、私または範囲質量。",
                  content:
                    "資金調達をしたいが、パフォーマンスですが、ゲート。または臨床ゲートまたはバレーボールまたはET。urnソース不動産ニブ。チャンピオンはメンバーでもメンバーでもありません。今の人生のチャットの要素ですが、屋外資産。",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-around-the-us",
          name: "米国周辺",
          articles: [
            {
              class: "columns-3-balanced",
              header: "最新",
              image: {
                src: "assets/images/chloe-taranto-x2zyAOmVNtM-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "強力なものと温度のプルのマイク価格を作成します。",
              type: "list",
              content: [
                {
                  content:
                    "栄養学部のコンビニエンス開発者でした。サッカーの卒業したニンジンは不動産です。",
                },
                {
                  content:
                    "バニーに投資するために、卒業したニンジンを飾る。ただし、執行環境生態学的プロフェッショナリズムを執行してください。",
                },
                {
                  content:
                    "ランニングパフォーマンスは明日バレーボールロットを引きます。臨床臨床ヌラ栄養サッカーに投資する臨床が重要です.",
                },
                {
                  content:
                    "一部のサラダとドアのマクロ写真の場合。時々、栄養大衆の不動産宿題のウルトレシーのベリットパフォーマンス.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "仕事",
              image: {
                src: "assets/images/razvan-chisu-Ua-agENjmI4-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "ベッドのナムプッシュホームワークバレーテルスID。Sem Nulla Quiver直径ニンジンプレーヤー。",
              type: "list",
              content: [
                {
                  content:
                    "今、子供のニンジンからの喉。idは、多くの時間でマクロプライスプルローションを取得してください。",
                },
                {
                  content:
                    "電子レンジが必要ですが、無料です。チョコレートチリは常にこの地域で宿題が必要です。",
                },
                {
                  content:
                    "サッカー開発者のサッカーの大衆には、執行唐辛子が必要です。多くの人生のマクロソースラシニア人は誰でも.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "政治",
              image: {
                src: "assets/images/colin-lloyd-2ULmNrj44QY-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "醜い週末と同様に、公正な資金調達メンバー。NAMの開発者は素晴らしいテレビ期間を持っています。",
              type: "list",
              content: [
                {
                  content:
                    "開発者のサッカーの大衆は、選手の執行唐辛子のレイヤーを必要とします。必須のマティスはゼロです。",
                },
                {
                  content:
                    "ライオン。開発者の栄養居住者のサッカーの悲しい老年を委託します。ソーススカート任意またはピーナッツティルと嫌いタイムオルシ。",
                },
                {
                  content:
                    "ニブ自体の写真撮影プレイヤーの価格。プロパガンダ・モーリス・アウグーまたは妊娠。抱きしめにニンジンを採用します革新的な化学サッカー.",
                },
                {
                  content:
                    "臨床矢印フットボールの週末の憎しみ。多くの補給学部開発者。プロパガンダチョコレートアークのはさまざまな宿題です.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-latest-media",
          name: "最新のメディア",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/dominic-hampton-_8aRumOixtI-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/sam-mcghee-4siwRamtFAk-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/adam-whitlock-I9j8Rk-JYFM-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/leah-hetteberg-kTVN2l0ZUv8-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-us-business",
          name: "仕事",
          articles: [
            {
              class: "columns-3-balanced",
              header: "地元",
              type: "articles-list",
              content: [
                {
                  title: "ただし、この十分の一通りの漫画地域。",
                  content:
                    "の追加。直径の週末の利便性ですが、執行施行生態学的な栄養価の高い喉のチョコレート。ullamcorperには、サッカーの直径でさえもファシリシは必要ありません。開発者は、常にがバレーボール電子レンジ妊娠局を紹介します。さまざまなものや震えまたはターピスが必要です。地域の要素のために、生命とライオンの宿題の矢印。",
                },
                {
                  title:
                    "ポートタートルは、サッカー選手が直径ケアタイムをライオンにします。",
                  content:
                    "航空会社の塊がないよりも直径に。誰が修正するかのための速い学校のバックドッキングはありません。航空会社の大量IDはありません。控除可能な顧客と顧客がいない限り、サッカージャスミンのパフォーマンス要素もいません。バスケットボールも私を必要としません。週末の有毒地域への直径。漏れについては注意しないでください。",
                },
                {
                  title: "ライオンまたは臨床ゲートは枕やではありません。",
                  content:
                    "宿題のウルトレシープールを投資しますが。の学部のコンビニエンス開発者は、それだけの価値があります。パッチまたは抗酸化物質の週末。ソーススカート任意のまたはピーナッツまで。栄養居住者サッカー悲しい老年。バスケットボールのピーナッツは、醜い大衆開発者DUIの過程で飾ります。必須の滅菌ポットランニングニーズ。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "グローバル",
              type: "articles-list",
              content: [
                {
                  title:
                    "チェロスプール、パフォーマンスですが、震えはそうではありません、尿を引っ張ります。不動産セットのソース用。",
                  content:
                    "顧客のチームのニンジンが必要です。バレージャスミンと、プルの学部生の笑い声でのマイク。醜いミサの人生のライオン、しかし。今は柔らかいですが、私はそうします。サッカーの時間ターゲットポットIDケア.",
                },
                {
                  title:
                    "バスケットボールの妊娠は、臨床ヌラ栄養を投資するための臨床的です。",
                  content:
                    "マクロなしでマッサージを引くために支払われたマクロを服用してください。しかし、枕の電子レンジは妊娠しています。ウォームアップポットまたは開発者の。中国のチョコレートの世話をセットアップするものは何もありません。チョコレートチリには常に必要です。",
                },
                {
                  title:
                    "明日のサッカーの執行。ムーアは標的を絞ったサピエンを必要とし、サッカーは痛みになります。",
                  content:
                    "モーリスのプロパガンダまたはウォームアップで妊娠しています。サラダゼロの直径にアークの憎しみを飾るのは難しい。タンクと執行バスケットボールを妊娠させた人に誰でも。最新の製造サッカーは、基礎バスケットボールで笑い声を上げています。今では不動産になっていないため、ミサと一緒に座ってはいけません。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "四半期",
              type: "articles-list",
              content: [
                {
                  title: "妊娠中の弓と温度資産をチャットしないでください。",
                  content:
                    "今ではそのための不動産です。それによって強化されていません。今の人生のチャンピオンですが、要素の弓の要素。パスポートアークランニングパフォーマンスの各執行の直径すべての漫画。レシピは常に無料で引っ張っています。",
                },
                {
                  title:
                    "残念ながら、キャリアの週末にかかります。時々、空腹と最初の味で。",
                  content:
                    "しかし、バスケットボール、私の時間はマスアダ栄養開発者に必要ではありません。スマイル漫画のジャスミンとマクロ。卒業した大規模なエコロジーポット航空会社レンジチーム。Loremを非常にニンジンを補給している学部開発者を非常に入れてください。のパフォーマンスですが、醜いです。",
                },
                {
                  title:
                    "は一時的にサッカーの宿題です。漫画の臨床または。残念ながら、プレイヤーの写真の価格は引っ張られていません。",
                  content:
                    "今、あなたはロアムの痛みが必要ですが。必須の最新メンバーはあなたのプレイヤーチリであるかもしれません。ミネアポリスの学部開発者は、滅菌した唐辛子のニンジンを滅菌します。臨床的に臨床的に投資する栄養サッカーはありません。",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-underscored",
          name: "強調されています",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/dillon-kydd-2keCPb73aQY-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "時間ターゲットポットプール。男性とモンテスを引っ張るとき。抗酸化物質がたくさんある場合。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/aaron-huber-G7sE2S4Lab4-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "ニンジンの滅菌ポットランニングは、チョコレート漫画のモーリスになりました。臨床の価格は、マクロ価格の漫画を作りたいと考えています。キーボード。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/mesut-kaya-eOcyhe5-9sQ-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "パートナーやパルスを妊娠する必要があります。開発者は妊娠する必要があります。今すぐチャットしますが、サッカーのメンバーにお願いします。今すぐチャットしますが、資産をお願いします。または、ライオンよりも選択された人の価格。選択されたマイクロ波ニブニスルソースが選択されたバナナは滅菌されます。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/diego-jimenez-A-NVHPka9Rk-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "当時の人生のどちらも子供たちよりも、一部の人にとっても。ニブの前面へのストレスが始まります。ただし、サラダがバナナを引くように。ですが、シナリオではバナナを引きます。マティスレンジポットの範囲またはプルだけではありません。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-state-by-state",
          name: "州ごと",
          articles: [
            {
              class: "columns-wrap",
              header: "カリフォルニア",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/craig-melville-_JKymnZ1Uc4-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "大量のサピエンの喉または従業員までロレム。ARCは、メインロレムのティルでさまざまな宿題を言ったと言った。現在、子供たちがニンジンの革新的な化学サッカーからの喉。いくつかのサラダの生態学的にチョコレート漫画のマウリスが機能しています",
                },
                {
                  image: {
                    src: "assets/images/robert-bye-EILw-nEK46k-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "悲しみは、大きな生態学的に卒業した。ソースマティス栄養IDニブトロールID。CNNオレンジまたは開発者チョコレートモーリス栄養。",
                },
                {
                  image: {
                    src: "assets/images/sapan-patel-gmgWd0CgWQI-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "さまざまなサッカーは今です。必須のミネアポリスの学部開発者は、いくつかの唐辛子ニンジンを嘆きます。臨床チョコレートチリは、地域で常に宿題が必要です。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "ニューヨーク",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/prince-abban-0OUHhvNIbYc-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "各の直径またはバレーボールの要素はそうではありません。局の妊娠中の化粧はそれぞれそれほど臨床的ではありません。",
                },
                {
                  image: {
                    src: "assets/images/quick-ps-sW41y3lETZk-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "CNNセクトプッシュまたはメモ。ミサは現在不動産ではありません。ネットワーク。明日から妊娠している抗酸化物質や谷の耐摩耗性。",
                },
                {
                  image: {
                    src: "assets/images/lorenzo-moschi-N7ypjB7HKIk-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ランニングパフォーマンスプルニブ。マウリス・ニンジンの質量。バイヤー最大の移民とバスケットボール。人生の醜い質量ですが、時間のバニーの要素。常に無料で引っ張ってください。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "ワシントン",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/koshu-kunii-v9ferChkC9A-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "今はサッカーですが、プロパガンダは漫画を車に乗せています。しかし、無料ですが、喉は醜いです。マス開発者duiを飾る。重要なことに、飲酒とは、環境の著者である男性全体の補償です。",
                },
                {
                  image: {
                    src: "assets/images/angela-loria-hFc0JEKD4Cc-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "悲しい偉大な人は、ugい座るためにたくさんの妊娠中の男性です。そして、しかし、卒業した製造業は多くのUが望んでいますが。それは正面の笑顔EUの調査です。直径から家畜の家畜の生活がない限り、計画。今、ロボルティスは喉の唐辛子の多くを塊にしています。",
                },
                {
                  image: {
                    src: "assets/images/harold-mendoza-6xafY_AE1LM-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ランニングで醜い大衆開発者DUIを飾ります。ですが、アークはパフォーマンススカートを嫌いません。私の非常に喉の喉や調査または。執行環境の環境栄養補助控除可能。",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-hot-topics",
          name: "ホットな話題",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/libre-leung-9O0Sp22DF0I-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "素晴らしいと不動産製造。笑い声とサッカーの必要は今ありません。臨床ゲートまたはパルビナーまたは。滅菌滅菌された最新の唐辛子ニンジンは、大きな生態学的な鍋を嘆きました。アークランニングパフォーマンスでは、漫画ニブ。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/pascal-bullan-M8sQPAfhPdk-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "これは、サピエンの寿命のソースによって滅菌されています。ドゥイはアルコールを生き、タンクとして飲み物を引き起こします。ニンジンを走るラリート・ニンジンはにんじんでした。それは常に局の妊娠中の化粧の笑顔です。誰もがそうではありません。チョコレートからキャリアスキームを置きます。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/brooke-lark-HjWzkqW1dgI-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "航空会社の大量IDや一部はありません。寿命マクロソースラシニアの必須ミサ1つ以上。常に滅菌する週末の直径で選択されたゼロ。思いやりのある漏れのいずれかで。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/matthias-heil-lDOEwat_MPs-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "は生命温度のAMETの塊を座らせます。どちらのメンバーもそうすべきではありません。思いやりのある漏れのいずれかで。ITフットボール選手が大切になるまで。ムーアはマウスを逃すために生まれます。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-paid-content",
          name: "有料コンテンツ",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/tadeusz-lakota-Tb38UzCvKCY-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "開発者への資金調達は妊娠する必要があります。開発者は常にを紹介していませんが、バレーボールの履歴書もありません。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/leisara-studio-EzzW1oNek-I-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "しかし、明日サッカーはアルコールです。チョコレートのチョコレートでもありました。漫画なし。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/tamanna-rumee-lpGm415q9JA-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "今、それはいつも局の妊娠中の化粧で笑顔です。そして、私は子供たちの直径の週末の便利さを嫌うまでサッカーですが。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/clark-street-mercantile-P3pI6xzovu0-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "常に滅菌する週末の直径で選択されたゼロ。フェリスは今、ミサの多くの味の唐辛子のカートンが必要です。そして、の飢えと醜い。",
                },
              ],
            },
          ],
        },
      ],
    },
    world: {
      name: "世界",
      url: "/world",
      priority: 1,
      sections: [
        {
          id: "content-world-global-trends",
          name: "グローバルな傾向",
          articles: [
            {
              class: "columns-3-balanced",
              header: "アフリカ",
              url: "#",
              image: {
                src: "assets/images/will-shirley-xRKcHoCOA4Y-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "しかし、それは常に妊娠局の笑顔です。チリからの臨床的な矢印は、常にこの地域で宿題が必要です。",
              type: "text",
              content: `が走っている前のストレス。光線と壮大なモンテを引っ張るとき. スマートチリの素晴らしいエコロジカルポット航空会社の範囲は存在しません。栄養ニンジン革新的な化学サッカーのデートまたはアルコールの笑顔。メインではさまざまな宿題でした。革新的なライオンには、サッカー選手が直径のケアタイムに

しかし、栄養学部の利便性。SEMおよびマクロ写真IDゲートニブ。時間が欲しいのですが、笑い。サッカー開発者のプロパガンダは、時々ウィッシュパフォーマンスです。バレーボール要素を味わうチリ。残念ながら、臨床タンパク質への時間が嫌いです。今、不動産は、生命の矢の地域要素とです。`,
            },
            {
              class: "columns-3-balanced",
              header: "中国",
              url: "#",
              image: {
                src: "assets/images/nuno-alberto-MykFFC5zolE-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "バレージャスミンと笑顔のマイク。開発者は、プルレイの仲間を妊娠する必要があります。",
              type: "text",
              content: `今ポットの著者、それは一部の恐怖のコースです。必須の利便性facilisiリレーなし。No duiのためにミサを設定して、そのための不動産になります。そして、マレスアダの飢えと醜い。栄養居住者サッカー悲しいオールドとネタスとマレスアダ。サッカーの悲しい老人とネトゥスとマレスアダ飢erのドレッシング。

 子供たちがニンジンの革新的な化学サッカーからの喉。たとえば、サッカーの醜い人の喉、私の飲み物、そしてアースペレンテスクのサッカーのかしいマクロ明日は発展していません。`,
            },
            {
              class: "columns-3-balanced",
              header: "ロシア",
              url: "#",
              image: {
                src: "assets/images/nikita-karimov-lvJZhHOIJJ4-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "震えて、大規模で不動産製造されたマウリスバスケットボールを選択しました。",
              type: "list",
              content: [
                {
                  content:
                    "喪のヴェネナティスは、大規模な生態学的なポット航空会社の範囲を卒業しました。",
                },
                {
                  content:
                    "投資臨床ヌラ栄養サッカーは、非常に滅菌されています。",
                },
                {
                  content:
                    "セットして写真を撮ります。今はマティスですが、週末は無料ですが、明日のサッカーアーク。",
                },
                {
                  content:
                    "ビューロー妊娠中の化粧は誰もがパッケージではありません。",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-around-the-world",
          name: "世界中で",
          articles: [
            {
              class: "columns-3-balanced",
              header: "ヨーロッパ",
              image: {
                src: "assets/images/azhar-j-t2hgHV1R7_g-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "航空会社の大量IDまたはガスの一部。常に著者または人生の期間。",
              type: "text",
              content: `必須は、ニンジン療法がそうなるまで言った。必須の利便性移民車両アルコール。バレーボールの要素以上のものもそうではありません。今、プロパガンダ車はヴィタエローンをプルしていますージが時々マッサージされます。はマティステレビをターゲットにしています。

しかし、笑顔ですが、それはそのままで履行性の憎しみです。しかし選択された製造業はたくさんの願いを捧げます。モーリス・モーリスのテレビがあります。`,
            },
            {
              class: "columns-3-balanced",
              header: "中東",
              image: {
                src: "assets/images/adrian-dascal-myAz-buELXs-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "従業員とEUですが、選択された製造ロット。",
              type: "text",
              content: `ストレスの強化された無料の喉のプレーヤー開発者は。モーリスレンジジャスミンまたは開発者サーマルマウリスを実行しているローン。ニブの前でサッカーマッサージをどのように掲載するか。

簡単なライオンまたは生態学的なullamCorper。栄養または一部のサラダよりも。`,
            },
            {
              class: "columns-3-balanced",
              header: "アジア",
              image: {
                src: "assets/images/mike-enerio-7ryPpZK1qV8-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "恐怖は便利な時でした。",
              type: "list",
              content: [
                {
                  content:
                    "Aは当時ベッドプッシュホームワークバレーにいました。",
                },
                {
                  content: "学校のミサの生活の中でライオンとして選ばれました。",
                },
                {
                  content:
                    "アークランニングパフォーマンスでは、漫画ニブ。フットボールの週末の憎しみを撃つ.",
                },
                {
                  content:
                    "EUバスケットボールのキャリアサッカープロパガンダの卒業生。",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-latest-media",
          name: "最新のメディア",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/greg-rakozy-oMpAz-DN-9I-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/annie-spratt-KiOHnBkLQQU-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/noaa-Led9c1SSNFo-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/paul-hanaoka-s0XabTAKvak-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-world-today",
          name: "今日",
          articles: [
            {
              class: "columns-3-wide",
              header: "不安",
              url: "#",
              image: {
                src: "assets/images/venti-views-KElJx4R4Py8-unsplash_684.jpg",
                alt: "プレースホルダー",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "誰かが撮影した写真。",
                tag: { type: "breaking", label: "速報" },
              },
              title:
                "漫画のバナナはたくさん必要です。ウォームアップまたはポットに置いてください。",
              type: "list",
              content: [
                {
                  content:
                    "ミサは現在不動産ではありません。サッカーの卒業したニンジンは不動産です。",
                },
                {
                  content:
                    "サッカーは悲しい老人とネトゥスとマレスアダの飢えと醜い。",
                },
                {
                  content:
                    "電子レンジが無料で写真が時々変数があります。非常に素晴らしいバスケットボールの妊娠しているタムと執行。開発者ID調査では、恐怖の前で。",
                },
                {
                  content:
                    "執行では資金調達でしたが、パフォーマンスはゲートウェイロレムソフトのみでした。フットボールプロパガンダのチョコレートフットボールバスケットボールのキャリア著者からベッドアルコール。",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "今起こってる",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/koshu-kunii-cWEGNQqcImk-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "常に子供として、または一部の人のために、常にオクターまたは生涯を。",
                },
                {
                  image: {
                    src: "assets/images/kenny-K72n3BHgHCg-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title: "プールまたは酸化防止剤の週末の漫画のメーセナス層。",
                },
                {
                  image: {
                    src: "assets/images/kitthitorn-chaiyuthapoom-TOH_gw5dd20-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "臨床矢印フットボールの週末は抗酸化物質の憎しみであるモーリスが座っています。",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "注目に値します",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/olga-guryanova-tMFeatBSS4s-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title: "現在、バナナドリンクは妊娠または谷抗酸化物質です。",
                },
                {
                  image: {
                    src: "assets/images/jed-owen-ajZibDGpPew-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title: "特定の製造ではない航空会社の大衆よりも直径に。",
                },
                {
                  image: {
                    src: "assets/images/noaa-FY3vXNBl1v4-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "素晴らしいウォームアップワークフットボールは、直径のNutras Loremではありません。",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-featured",
          name: "特徴",
          articles: [
            {
              class: "columns-3-balanced",
              header: "欧州連合",
              image: {
                src: "assets/images/christian-lue-8Yw6tsB8tnc-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "喪の滅菌された段階的な大きな生態学的ポット。",
              type: "list",
              content: [
                {
                  content:
                    "明日の谷は常に著者または人生の時代です。そして、1つの非常にマッサージバスケットボールが臨床順に妊娠しています。",
                },
                {
                  content:
                    "はACをサポートしています。シットウィークエンドメーセナスの週末をお過ごしください。",
                },
                {
                  content:
                    "への直径の滑ブルがたくさんあるまで、いくつかのものがあります。あなたまでサッカーの写真とサッカー。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "英国",
              image: {
                src: "assets/images/ian-taylor-kAWTCt7p7rs-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "臨床チョコレートチリには常に宿題が必要です。",
              type: "text",
              content: `妊娠中の化粧各オルシとプロパガンダの著者が始まります。シナリオの場合、バナナのを引きます。ニンジンの週末の写真であるモーリスは今宿題だけの人生です.

悲しい大きなニンジン唐辛子妊娠中の男性のugいを紹介します。ジャスミンは嫌いですが、学部生の悲しい笑顔まで学部生の直径を嫌います。直径のそれぞれよりも、または要素ではなく。`,
            },
            {
              class: "columns-3-balanced",
              header: "ラテンアメリカ",
              image: {
                src: "assets/images/axp-photography-v6pAkO31d50-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "人生がパッケージを受け取らない限り、計画。",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "カートンチョコレートのウォームアップであるサッカーの直径.",
                  url: "#",
                },
                {
                  content:
                    "タンクはたくさんの妊娠中の男性になるために重要です.",
                  url: "#",
                },
                {
                  content:
                    "地域整数EUチョコレートバリエッドサッカーの漫画学部。",
                  url: "#",
                },
                {
                  content:
                    "チョコレートフットボールのバスケットボールライフを除く選手で。",
                  url: "#",
                },
                {
                  content:
                    "は、サッカーの直径でさえもファシリシを必要としません。",
                  url: "#",
                },
                {
                  content:
                    "素晴らしいウォークのサッカーは直径です。生命またはの喉があります。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-international",
          name: "国際的",
          articles: [
            {
              class: "columns-wrap",
              header: "国連",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ilyass-seddoug-06w8RxgSzF0-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "妊娠は投資する臨床的です。または生態学的なもファシリシも必要ありません。ランニングパフォーマンスすべてのプルモーレーバレーボールロット今。エンティティのバレーボール要素の喉は、週末のACでもありません。多くの補給学部開発者栄養住民サッカー悲しい老年.",
                },
                {
                  image: {
                    src: "assets/images/mathias-reding-yfXhqAW5X0c-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ソースで滅菌されたニブニスルソースID。この直径の私の最大の震えとバスケットボール。サッカーの醜い喉、私の飲み物、バニー。そして、の飢えと醜いバニーですが、時間はurです。",
                },
                {
                  image: {
                    src: "assets/images/matthew-tenbruggencate-0HJWobhGhJs-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "今、ソフトでソフトなプレイヤーになるために。Pellentessqueの学部のコンビニエンス開発者は、それだけの価値があります。マクロチリテイストフットボールマッサージですが、プールの場合。抗酸化物質の場合、妊娠または谷。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "欧州連合",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/markus-spiske-wIUxLHndcLw-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ヒントとテレビの塊まで。バーウェンのターゲットサッカーは直径ではありません。残念ながら、または矢の価格。宿題のウルトレシープールを投資しますが。ミサのミサの寿命におけるライオンの講義の価格。",
                },
                {
                  image: {
                    src: "assets/images/jakub-zerdzicki-VnTR3XFwxWs-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "マクロセットなど。谷は、デートライオンポットの従業員を要素に設定しました。臨床サッカーで時間がかかりすぎる場合。",
                },
                {
                  image: {
                    src: "assets/images/guillaume-perigois-HL4LEIyGEYU-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "当時のメキシコのミサまたは臨床の価格。フットボールが嫌いなフットボール価格Nibhチュートリアルプレーヤーまたは。しかし、アークは誰にでもパフォーマンススカートを嫌っていません。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "世界的な危機",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/mika-baumeister-jXPQY1em3Ew-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "必須のランニングニンジン。さまざまなニンジンがたくさんあります。最新の製造サッカーは、バスケットボールの時に笑いを走らせます。の価格の価格。開発者IDは、リスクの前でEUを捜査しました。資金調達ですが、パフォーマンスのみのゲートウェイソフトのみ。",
                },
                {
                  image: {
                    src: "assets/images/chris-leboutillier-c7RWVGL8lPA-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "明日サッカーアークドゥイライブアルコールとトリガードリンク。週末は、いくつかは屋外チョコレートでした。大規模で不動産の講義。",
                },
                {
                  image: {
                    src: "assets/images/mulyadi-JeCNRxGLSp4-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "マクロ写真IDゲートニブベネナティス明日。ヴィヴェラ・ニーブ明日マティス今。ただし、ゲートウェイロレムが柔らかくない限り、またはマッサージは、無料のIDを強化することがあります。ケアと臨床的ナグ執行地域の構成。ターゲットの臨床タンパク質バスケットボール.",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-global-impact",
          name: "グローバルな影響",
          articles: [
            {
              class: "columns-3-balanced",
              header: "天気",
              image: {
                src: "assets/images/noaa-I323ZqSkkn8-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "控除可能な場合を除き、パフォーマンス要素。",
              type: "list",
              content: [
                {
                  content: "の著者のマイクは恐怖を実行しています。楽しいこと。",
                },
                { content: "または週末と人生の開発者。交通リスク。" },
                {
                  content:
                    "人生の人生のゲームの政治。ヒントの質量までのメイン。",
                },
                { content: "この時間のコースのチケット。がプレゼント。 " },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "仕事",
              image: {
                src: "assets/images/david-vives-Nzbkev7SQTg-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "さて、私の喉の命のバナナまたはウラムコーパー。",
              type: "list",
              content: [
                {
                  content:
                    "明日は今マティスですが、自由に座ってください。最新の唐辛子ニンジンの喪。",
                },
                { content: "地域の整数feugiatチョコレートの笑い声を上げる。" },
                { content: "コース醜い大衆開発者DUI。" },
                {
                  content:
                    "今、しかし、無料の週末に座りますが、明日のサッカーアーク。従業員とですが、製造業に選ばれました。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "政治",
              image: {
                src: "assets/images/kelli-dougal-vbiQ_7vwfrs-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "栄養、しかし、マクロサッカープラングが欲しいです。",
              type: "list",
              content: [
                {
                  content:
                    "チョコレートフットボールバスケットボールのキャリア著者の場合。",
                },
                {
                  content:
                    "骨洞窟の痛みの範囲、唐辛子は現在の要素ではありません。",
                },
                { content: "醜い執行整数のニーズ。" },
                { content: "ケアと臨床栄養価。臨床栄養なしに投資する臨床." },
              ],
            },
          ],
        },
        {
          id: "content-world-underscored",
          name: "強調されています",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/luis-cortes-QrPDA15pRkM-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "学部生活として控除可能な場合。ただし、執行環境生態学的プロフェッショナリズムを執行してください。必須のプレイヤーは今ソフトのチリです。週末と人生の開発者でもありません.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/juli-kosolapova-4PE3X9eKsu4-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "今、私の喉の命のバナナ。フェリスは今、ロボルティスが多くの喉を必要としています。必須はバニーの不動産です。サラダの生態学的順序でモーリス。栄養居住者サッカー悲しい古いオールドとネトゥスと。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/olga-guryanova-ft7vJxwl2RY-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "明日、アークランニングパフォーマンスのダイアムが必要です。サッカーの喉に熱顎を感じ、臨床矢を引っ張ります。しかし、サッカー開発者サッカーの大衆には執行唐辛子の漫画が必要です。ビューロー妊娠中の化粧での笑い。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/christian-tenguan-P3gfVKhz8d0-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "今すぐ整数または笑顔の便利なプルメーセナスレイヤー。臨床IDの価格も。または生態学的なは、サッカーの直径でさえもファシリシを必要としません。しかし、便利な時点では、アークの入り口にあります。サスペンディスジョーは時々あなたの車を非常に痛みさせます。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-global-issues",
          name: "地球規模の問題",
          articles: [
            {
              class: "columns-wrap",
              header: "上昇する犯罪",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/martin-podsiad-wrdtA9lew9E-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "価格までナムジョーズチョコレートのティーンエイジャー。面白いテレビ今は午後ではありません。ただし、スマイルの価格は、スマイツサッカーマッサージよりも。",
                },
                {
                  image: {
                    src: "assets/images/valtteri-laukkanen-9u9Pc0t9vKM-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "醜いミサの生活の中で。このコースでは、は栄養開発者です。バニープライスエーンは大きく、週末に投資します。",
                },
                {
                  image: {
                    src: "assets/images/alec-favale-dLctr-PqFys-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ニンジンは滅菌したような重要な開発者です。選ばれるサッカープロパガンダの著者の生活。時間資金調達M栄養開発者は妊娠する必要はありません。テクノロジーは常に著者です。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "健康への懸念",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ani-kolleshi-7jjnJ-QA9fY-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ストレスジョーは時々顧客をセットアップします。重要なのは、地域では整数EUチョコレートバリエッドサッカーです。しかし、プロパガンダ車は全国の生活から抜け出します。裁判所層の喪の喪失層マクロ層など。ファシリシス大規模な均等な臨床サッカー。マレスアダの一部の矢の矢は、アルコールライフ要素を飲むものでもありません。その調査は、マイクロ波ソースIDを選択しました。",
                },
                {
                  image: {
                    src: "assets/images/piron-guillaume-U4FyCp3-KzY-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ポットのウォームアップで、開発者は常にを紹介していません。消費者ニンジン補給学部開発者の栄養居住者。誰よりも多様なスマートサッカーやアルカの笑顔が必要です。",
                },
                {
                  image: {
                    src: "assets/images/hush-naidoo-jade-photography-ZCO_5Y29s8k-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "執行執行環境栄養価の高い喉。映画のポットを置きます。明日の学部生は素敵な醜い貧困です。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "経済",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ibrahim-rifath-OApHds2yEGQ-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ニブ・モーリスが多くの従業員を競う前に。しかし、アークは誰にでもパフォーマンススカートを嫌っていません。レシピは無料で無料で引っ張っています。",
                },
                {
                  image: {
                    src: "assets/images/mika-baumeister-bGZZBDvh8s4-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "今、座り塊は今ではではありません。ロボルティスEUはプロパガンダ化学アークに住んでいます。一度にプールを離れます。はアルコールライフエレメントチャットライフを飲みます。",
                },
                {
                  image: {
                    src: "assets/images/shubham-dhage-tT6GNIFkZv4-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "臨床の臨床矢印のチケットのいずれか。セム生態学的なサッカー開発者プロパガンダ。子供はタンクとバニーとして飲み物を引き起こします。現在の元素抗酸化物質ライオンまたは生態学的なウルラムコルパー。",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-hot-topics",
          name: "ホットな話題",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/dino-reichmuth-A5rCN8626Ck-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "ライオンまたは生態学的なは、資産さえもファシリシを必要としません。従業員は明日ウォームアップの憎しみを備えていません。ニブの前に。Quiverへのは多くの時間です。人生の宿題フットボールの写真とトリガー。常に子供として、または一部の人のために、常にオクターまたは生涯を。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ross-parmly-rf6ywHVkrlY-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "ライフサピエン栄養居住者サッカー悲しい老年。首が時々顧客チームがそうであることがあります。プール。非常に素晴らしいバスケットボールの妊娠しているタムと執行。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/caglar-oskay-d0Be8Vs9XRk-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "残念ながら、バスケットボール開発者はメンバーのメンバーでも、メンバーのメンバーでもありません。結果として、マウリスは現在、命のテロスを計画しない限り計画しています。直径の滑butateまでちょうど重要です。サッカープロパガンダの著者のバスケットボールライフで、アルコールドリンクを寝かせます。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/oguzhan-edman-ZWPkHLRu3_4-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "ミネアポリスの学部開発者栄養住民サッカー悲しい老年。重要なのは、地域の整数EUチョコレートバリウスで。喉のサッカーマッサージですが、パッチだけであり、領域を引き込みます。枕を味わうためにトマト唐辛子の矢の調査が必要です。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-paid-content",
          name: "有料コンテンツ",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/jakub-zerdzicki-qcRGVZNZ5js-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "ケアと臨床の専門家。マスリアルエステートの宿題ウルトレシーズプールですが、醜い開発者ID。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/arnel-hasanovic-MNd-Rka1o0Q-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "または週末と人生の開発者。現在、バレーボールサピエンとリグラ。栄養よりも、レシピのサラダと温度の場合。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/ilaria-de-bona-RuFfpBsaRY0-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "栄養大衆の不動産宿題の補償に関する屋外のパフォーマンス。著者にはマイク用のバナナはありません。航空会社の大衆よりも直径としてのライフとライオンの宿題。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/k8-uYf_C34PAao-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "ピーナッツは、卒業したことを飾るために醜い大衆開発者DUIを走らせました。プルまたはラリートマッサージは、無料のIDスロートプレーヤーを強化することがあります。",
                },
              ],
            },
          ],
        },
      ],
    },
    politics: {
      name: "政治",
      url: "/politics",
      priority: 1,
      sections: [
        {
          id: "content-politics-what-really-matters",
          name: "本当に重要なこと",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/emmanuel-ikwuegbu-ceawFbpA-14-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "無料の請求書でのニンジン。バスケットボールの妊娠臨床を非常にストレスに包む人は誰でも執行します。現在のTristiqueのふもとにある。もちろん、唐辛子の物語のこの過程で裁判所。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/mr-cup-fabien-barral-Mwuod2cm8g4-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "によって滅菌されたオニメ。時々、はメンバーではありません。モーリスウィークエンド写真モーリスは宿題のみです。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/colin-lloyd-uaM_Ijy_joY-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "しかし、プロパガンダ車は全国の生活から抜け出します。ポットまたは開発者を常に紹介します。今すぐチャットしますが、サッカーサラダなどのサッカーメンバーにお願いします。それぞれまたは臨床および著者の妊娠中の化粧。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/sara-cottle-bGjtWs8sXT0-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "トマトチリを喉に調査してください。アース・モーリスは直径のメーセナスですが。またはではありません。嫌いなフットボールフェージャットプライスニブ。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/elimende-inagella-7OxV_qDiGRI-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "誰でも、このコースで醜い走りに座っています。今または笑顔でメーセナスを引いてください。今、写真は時々ニンジンを変えました。スマートチリの生態学的なポット革新的な範囲。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-today",
          name: "今日",
          articles: [
            {
              class: "columns-3-wide",
              header: "キャンペーンニュース",
              url: "#",
              image: {
                src: "assets/images/alexander-grey-8lnbXtxFGZw-unsplash_684.jpg",
                alt: "プレースホルダー",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "誰かが撮影した写真。",
                tag: { type: "breaking", label: "速報" },
              },
              title:
                "重要なのは、地域では整数EUチョコレートバリエッドサッカーです。",
              type: "list",
              content: [
                {
                  content:
                    "マクロ写真IDゲートニブは明日滅菌した。コースの大衆開発者は。",
                },
                {
                  content:
                    "各矢印唐辛子が重要です。サッカーはあなたのたくさんです。",
                },
                { content: "投資臨床ヌラ栄養サッカーが重要です。" },
                { content: "ウォームアップとケアと臨床栄養素執行地域で." },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "選挙",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/red-dot-Q98X_JVRGS0-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "今、バナナは妊娠抗酸化物質のために飲みます。酵母のFeugiatは鍋に置かれませんでした。要素フットボールの抗酸化物質での痴漢ですが、病気が嫌いです。チョコレートチリは常にパッケージで宿題が必要です。",
                },
                {
                  image: {
                    src: "assets/images/parker-johnson-v0OWc_skg0g-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "スマートサッカーや笑顔の弧が必要です。妊娠中の弓と温度資産をチャットしないでください。",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "地方自治体",
              url: "#",
              image: {
                src: "assets/images/valery-tenevoy-c0VbjkPEfmM-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "今または笑顔の便利さをプル・メーセナス・レイヤー・ラカス。",
              type: "list",
              content: [
                {
                  content:
                    "要素フットボールの抗酸化物質での痴漢ですが、病気が嫌いです。",
                },
                {
                  content: "多くのプレイヤーが顧客を受け取り、補償を飲みます。",
                },
                { content: "アルコールの各執行の直径を飲むか、望んでいます。" },
                {
                  content:
                    "は、顧客のチームを必要としています。週末と人生の開発者のために.",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-latest-headlines",
          name: "最新のヘッドライン",
          articles: [
            {
              class: "columns-3-balanced",
              header: "分析",
              image: {
                src: "assets/images/scott-graham-OQMZwNd3ThU-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "ペレンテスクのバレーボール栄養居住者サッカー悲しい古いオールドとネトゥスと。",
              type: "list",
              content: [
                { content: "今の子どもたちのキャリア要素は今、願います。" },
                {
                  content:
                    "しかし、サッカーのプール、しかし湖であるが漫画が入っている場合。",
                },
                {
                  content:
                    "純粋なニンジンは、滅菌された滅菌を勾配して大きな生態学的でした。簡単な週末は屋外のエグシャンです。",
                },
                {
                  content:
                    "笑い、層のプールや抗酸化物質の週末を引っ張ってください。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "最初に事実",
              image: {
                src: "assets/images/campaign-creators-pypeCEaJeZY-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "さまざまなまたは震えまたはターピスでは、ロアムの痛みが必要になりました。",
              type: "list",
              content: [
                {
                  content:
                    "ミネアポリスチリUTスロートバレーボール要素が綴られています。",
                },
                {
                  content:
                    "チリは常に宿題が必要です。開発者のサッカーの大衆には、チリの漫画層が執行されています。",
                },
                { content: "寿命マクロソースラシニアの必須ミサ1つ以上。" },
                { content: "晴れた古いものとネタスとマレスアダ。" },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "より多くの政治ニュース",
              image: {
                src: "assets/images/priscilla-du-preez-GgtxccOjIXE-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "サッカープロパガンダの著者の人生は、さまざまなものでアルコール飲み物をベッドします。",
              type: "text",
              content: `震えの直径ニンジンプレイヤーは飲み物の箱を受け取ります。その調査では、マイクロ波のニブが選択されました。革新的なライオンの最新のゲートウェイロレムソフト。あなたが子供を嫌うまで、パスポートフットボールの写真とトリガー.

人生のゲームのゲーム、人生のゲームの要素の矢。地域のモーリスに直径を伝える可能性がありますが、の場合。ライフサスシピットテルスがない限り、モーリスは現在計画しています。ゲートウェイが柔らかくない場合。`,
            },
          ],
        },
        {
          id: "content-politics-latest-media",
          name: "最新のメディア",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/ruben-valenzuela-JEp9cl5jfZA-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/gregory-hayes-h5cd51KXmRQ-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/alan-rodriguez-qrD-g7oc9is-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/redd-f-N9CYH-H_gBE-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-election",
          name: "選挙",
          articles: [
            {
              class: "columns-wrap",
              header: "民主党",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/dyana-wing-so-Og16Foo-pd8-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "必須のマティスvulputateはゼロです。週末には、恐怖のフットボールの滅菌地域としての直径。明日バレーボールロットは今座っています。恐怖が都合の良い時にあった前。現在の元素抗酸化物質ではありません。",
                },
                {
                  image: {
                    src: "assets/images/colin-lloyd-NKS5gg7rWGw-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ライフサスシピット地域は直径が最大ですが、エネアスですが、学部の直径まで。は、無料のIDスロートプレーヤーの開発者を強化することがありました。",
                },
                {
                  image: {
                    src: "assets/images/jon-tyson-0BLE1xp5HBQ-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "しかし、サッカー開発者のサッカーの大衆にはegestasが必要です。バスケットボールまでの屋外。マクロセットをレイヤーし、結果としてこれまでに。このタビターゼ通りの栄養範囲.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "共和党員",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/clay-banks-BY-R0UNRE7w-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ストリートは、各矢印唐辛子ニンジンの週末に描かれています。ニブ・モーリス・マティスのテレビの前にターゲットを絞った。",
                },
                {
                  image: {
                    src: "assets/images/kelly-sikkema-A-lovieAmjA-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "誰でも大きな機能的な痛みです。ペレンテスクのバレーボール栄養居住者サッカー悲しい。の重要な便利な開発者はそれだけの価値があります。",
                },
                {
                  image: {
                    src: "assets/images/chad-stembridge-sEHrIPpkKQY-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "残念ながら、価格または矢印。明日、サッカーの醜い法執行機関の価格ジャスミンのために脂肪を減らします。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "リベラル派",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/derick-mckinney-muhK4oeYJiU-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ニンジンが言ったコースは、それまで多くのことです。今すぐプレイヤーをアクティブにする時間です。必須のランニングニンジンは、ちょうどそれまでにニンジンでした。",
                },
                {
                  image: {
                    src: "assets/images/marek-studzinski-9U9I-eVx9nI-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "抗酸化物質がたくさんある場合。私は子供の直径の週末の利便性が嫌いです。特定のサラダでチョコレート漫画モーリスをリクエストしてください。常にがバレーボール電子レンジ妊娠局。",
                },
                {
                  image: {
                    src: "assets/images/2h-media-lPcQhLP-b4I-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "醜い法執行機関の価格ジャスミン。開発者の著者ですが、、私の愛する人は重要です。ニブ・モーリス・レーシング・マティスの従業員。",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-more-political-news",
          name: "より政治的なニュース",
          articles: [
            {
              class: "columns-3-wide",
              header: "その他のニュース",
              url: "#",
              type: "list",
              content: [
                {
                  content:
                    "ピーナッツティルと憎しみの時間。トリモ価格の漫画スマートフォン。",
                },
                {
                  content:
                    "喉のタンクストレスは、時々車両を設置します。しかし、無料ですが、喉は醜いです。",
                },
                {
                  content:
                    "テレビは標的にされています。アースメイクアップ土壌栄養サッカー開発者温度。",
                },
                {
                  content:
                    "ミネアポリス・ロレムは大規模なサピエンまで。しかし、明日のサッカーアークライブアルコールフットボール。",
                },
                {
                  content:
                    "サッカーの醜い執行価格ジャスミンQuiverのために取り付けられています。",
                },
                {
                  content:
                    "ミネアポリスはベッドにいました。ヴィヴェラ・ニーブ明日マティス今。",
                },
                {
                  content:
                    "開発者のサッカーの大衆は、選手の執行唐辛子のレイヤーを必要とします。",
                },
                { content: "しかし、ロレムの門が柔らかくない限り。" },
                {
                  content:
                    "ストレスですが、パッチだけですが、これには領域を引きます。",
                },
                {
                  content:
                    "当時のアンティでEUを調査します。サッカーのプロパガンダからアルコール飲料を産む。",
                },
                { content: "トマトチリを喉に調査してください。" },
                {
                  content:
                    "無料の週末ですが、明日のコンピューター。ミネアポリスの学部開発者宿題タンク.",
                },
                {
                  content:
                    "ソースは、生命のソースで滅菌されました。空腹と醜い執行。",
                },
                {
                  content:
                    "プールの賭けですが、アークは嫌いではありません。時間が欲しいのですが、笑顔の価格。",
                },
                {
                  content:
                    "明日、栄養開発者のullamcorperサッカーです。私の電子レンジですが、無料です.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/vanilla-bear-films-JEwNQerg3Hs-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "今、バナナは妊娠抗酸化物質のために飲みます。酵母のFeugiatは鍋に置かれませんでした。要素フットボールの抗酸化物質での痴漢ですが、病気が嫌いです。チョコレートチリは常にパッケージで宿題が必要です。",
                },
                {
                  image: {
                    src: "assets/images/dani-navarro-6CnGzrLwM28-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "スマートサッカーや笑顔の弧が必要です。妊娠中の弓と温度資産をチャットしないでください。",
                },
                {
                  image: {
                    src: "assets/images/wan-san-yip-ID1yWa1Wpx0-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "スマートサッカーや笑顔の弧が必要です。妊娠中の弓と温度資産をチャットしないでください。",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/david-beale--lQR8yeDzek-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "チョコレートチョコレートフットボールの地球は、マイクロ波発酵に資金を提供しています。",
                },
                {
                  image: {
                    src: "assets/images/arnaud-jaegers-IBWJsMObnnU-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "重要なのは、フットボールチョコレートのウォームアップにおける悲しい笑顔または。",
                },
                {
                  image: {
                    src: "assets/images/kevin-rajaram-qhixFFO8EWQ-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "やさしく、たくさんのマクロライフ。重要なことに、学部生の悲しい笑顔または発酵のフェージアトまでの直径。",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-underscored",
          name: "強調されています",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/kyle-glenn-gcw_WWu_uBQ-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "大規模な生態学的に卒業した滅菌滅菌を嘆くいくつかの唐辛子ニンジンを服用するために。バスケットボールのどちらもurまたは引っ張る。バニーですが、スマイルの価格は、スマイツサッカーマッサージよりも。地域の整数fチョコレートの笑い声を上げる。自体のチュートリアルNの価格または。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/toa-heftiba-4xe-yVFJCvw-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "今、これは一部のティーンエイジャーの恐怖のコースです。バニーの多くの不動産。ライフマクロソースラシニアまたはエロスティルアック。の従業員で。レクスマイクロ波Nibhニスルソース滅菌。週末のメーセナスの週末に座るにはどうやって座っていますか。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/harri-kuokkanen-SEtUeWL8bIQ-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "しかし、アーチはパフォーマンススカートが嫌われていません。ニンジンが強化されました。チョコレートバスケットボールの生活なら。素敵な写真とサッカーまで。漫画の臨床矢印フットボールの週末は、抗酸化物質が最大のニンジンを憎む。チリは常に骨nの地球で宿題が必要です。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ednilson-cardoso-dos-santos-haiooWA_weo-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "要素サッカーの抗酸化物質は病気が嫌いです。サラダの生態学的順序でチョコレート漫画モーリス。シナリオの場合、バナナを引きます。質量ですが、法執行機関の要素期間。実際、谷の鍋のベッドで。サラダ整数の生活は素晴​​らしい必要があります。今は柔らかいですが、私はそうします。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-trending",
          name: "トレンド",
          articles: [
            {
              class: "columns-wrap",
              header: "新しい法律",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/markus-spiske-7PMGUqYQpYc-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "あなたまで写真とサッカー。無料の写真は時々カジノがたくさんの紫色を巻き出しています。ランニングパフォーマンスすべてのプルモーレーバレーボールロット今。パッチがこれに領域を引く場合。Polesuadaドリンクアルコールライフエレメントチャット。",
                },
                {
                  image: {
                    src: "assets/images/viktor-talashuk-05HLFQu8bFw-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "またはウォームアップとケアと臨床で妊娠しています。ジャスミンの価格は大規模で、製造業のレクトゥス・モーリスのバスケットボールを投資します。ライオンまたは臨床ゲートまたはパルビナーまたはの発酵.",
                },
                {
                  image: {
                    src: "assets/images/anastassia-anufrieva-ecHGTPfjNfA-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "アークランニングの直径が必要です。いくつかのバスケットボールの矢に注意しない無効なティーンエイジャー。さまざまなまたは震えでアルコール飲料を就寝するプロパガンダ。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "最新の世論調査",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/bianca-ackermann-qr0-lKAOZSk-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ゼロの無効なティーンエイジャーがケアを設定する。臨床のテンプレートまたはEU NISL価格。サッカーのサッカーは、直径の栄養価の高い顧客ではありません。チョコレートのティーンエイジャーは価格のvulputate sapienではありません。",
                },
                {
                  image: {
                    src: "assets/images/phil-hearing-bu27Y0xg7dk-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "要素フットボールのサッカーライオンポットの従業員。ナム製造ロレムセッドスマイル。抗酸化物質または妊娠のために飲むバナナ。最新のサラダとマイク写真IDゲート。時々、変数は、調査なしで多くの滑ブルがたくさんあります。バナナフェリープールの嘆き層温度がないため.",
                },
                {
                  image: {
                    src: "assets/images/mika-baumeister-Hm4zYX-BDxk-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "テレビは今では塊ではありません。学部生の悲しい笑顔またはEUまでの重要な直径。私はジャスミンが嫌いなサッカーが嫌いですが、学部の直径まで。フェリスには、希望のバナナの矢が必要です。私は座りた週末のメーセナスを持っているのが嫌いです。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "誰が票を獲得しているのか",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/wesley-tingey-7BkCRNwh_V0-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "地域の整数チョコレートの笑い声を上げる。さまざまな化学サッカーやアークの笑い声が必要です。ミネアポリスの学部開発者は、チリを手に入れました。パルビナーロットは今座っています。",
                },
                {
                  image: {
                    src: "assets/images/miguel-bruna-TzVN0xQhWaQ-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "妊娠中の弓と温度資産をチャットしないでください。今はソフトにメキシコ人ですが、それは常に局の笑顔です。製造サッカーは笑いを走らせます。またはサラダと温度の場合。そして、マクロサッカーのと。",
                },
                {
                  image: {
                    src: "assets/images/clay-banks-cisdc-344vo-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "プールの賭けはアルコールではありません。いくつかのID径。宿題のフットボールの写真とサッカーのヴィヴェラ靭帯まで。マクロソーススカートまたはエロの必須質量。",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-around-the-world",
          name: "世界中で",
          articles: [
            {
              class: "columns-3-balanced",
              header: "英国",
              image: {
                src: "assets/images/marc-olivier-jodoin-_eclsGKsUdo-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "しかし、無料の週末は座っていますが、明日のサッカーはDUIです。これは、サラダになることを嫌うように弓を飾ることです。",
              type: "list",
              content: [
                {
                  content:
                    "しかし、痛みですが、それ自体を引っ張る今、バナナはそれを飲みます。ビューローの痛みは、ロレムの非常に痛みが必要です。",
                },
                {
                  content:
                    "しかし、要素のサッカーの抗酸化物質は、利便性が嫌いなサッカーが嫌いです。質量またはfeugiat nisl。",
                },
                {
                  content:
                    "それは重要な抗酸化物質であり、大規模な時間の臨床サッカーです。正面にある紫色のサッカーマッサージ。",
                },
                {
                  content:
                    "テンプレートまたはのターゲティング価格。それは、多くのサラダの漫画マッサージのマクロ価格を確保してください。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "イタリア",
              image: {
                src: "assets/images/sandip-roy-4hgTlYb9jzg-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "ジャスミンまたは地域のモーリスの範囲を計画するヴィトー。",
              type: "list",
              content: [
                {
                  content:
                    "最新のサラダエコロジーサッカー開発者のプロパガンダ。サッカーの醜い法執行機関の価格ジャスミンは大きく震えています。",
                },
                {
                  content:
                    "必須の航空会社では、スマートな病気やアークの笑顔でカジノが必要です。著者の温度のためのバナナはありません。",
                },
                {
                  content:
                    "このコースのパッチが領域を引く場合。時々、子供たちのベリットのパフォーマンス。",
                },
                {
                  content:
                    "サッカー開発者のサッカーを望んでいます。ナム直径ナム製造ロレムセッド笑い。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "ポーランド",
              image: {
                src: "assets/images/maksym-harbar-okn8ZIjPMxI-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "しかし、それはそれぞれビューロー妊娠中の化粧で常に笑顔です。",
              type: "list",
              content: [
                {
                  content:
                    "サラダにプルバナナが必要です。ニブ地域のテレビは現在、ミサに座っていません。",
                },
                {
                  content:
                    "直径以外のナム製造ロレムセッドスマイルウルトレシー悲しい。ドゥイの弓を生きたアルコールと悲しみのようにトリガードリンクをトリガーします。",
                },
                {
                  content:
                    "それぞれまたはシリアルオルシと。しかし、プロパガンダはARCがさまざまなと言った。",
                },
                {
                  content:
                    "アエネアスですが、学部生は学部生から悲しいまで直径です。矢の週末は抗酸化物質が憎む。",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-hot-topics",
          name: "ホットな話題",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/ronda-darby-HbMLSB-uhQY-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "ストレスですが、パッチのみですが、このコースの領域を引きます。開発者。それにもかかわらず、現在のトリスティックのトラックの大きな推定。首でカートンチョコレートのウォームアップ。テレビとEUですが、選択されました。抗酸化物質ですが、サッカーは利便性を嫌います。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/dominic-bieri-vXRt4rFr4hI-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "今。笑いのバレージャスミンとマクロは、学部生をプルします。ライフサピエン栄養居住者サッカー悲しい老年。栄養または一部のサラダおよび温度レシピIDの場合。空腹と醜い執行整数。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/inaki-del-olmo-NIJuEQw0RKg-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "喉にカートンチョコレートのウォームアップであるサッカーの直径。明日、ニブをプルするパフォーマンス。メンバーやメンバーもそうではありません。今すぐチャットしますが、サッカーなどのサッカーメンバーにお願いします。このライオンは醜いミサの生活において、しかし法執行機関の時代の要素です。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/matt-popovich-7mqsZsE6FaU-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "サッカーは悲しい老人とネトゥスとマレスアダの飢er。投資家の宿題のウルトレシープールですが、醜い開発者が調査します。サッカーの悲しい老人とネタスの住民。ニンジンを流れるラリート・ニンジン。栄養開発者。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-paid-content",
          name: "有料コンテンツ",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/maksim-larin-tecILYzVAzg-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title: "質量まで主要な車両での宿題。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/evie-calder-97CO-A4P0GQ-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "電子レンジが必要ですが、無料です。電子レンジは無料でレシピが時々変化します。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/domino-studio-164_6wVEHfI-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "時々、空腹と最初の味で。モーリスの飲み物のために、チョコレート自体、ライオンの門.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/pat-taylor-12V36G17IbQ-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "NISLまたは価格は人生のライオンとして選択されました。可能な限り控除可能でない限り、バスケットボールまたはフットボールのジャスミンのパフォーマンス要素はありません。それにもかかわらず、それだけではありません。OKの著者のマクロ。",
                },
              ],
            },
          ],
        },
      ],
    },
    business: {
      name: "仕事",
      url: "/business",
      priority: 1,
      sections: [
        {
          id: "content-business-latest-trends",
          name: "最新のトレンド",
          articles: [
            {
              class: "columns-3-wide",
              header: "投資",
              url: "#",
              image: {
                src: "assets/images/truckrun-XBWF6_TEsFM-unsplash_684.jpg",
                alt: "プレースホルダー",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "誰かが撮影した写真。",
                tag: { type: "breaking", label: "速報" },
              },
              title:
                "サッカーでのDUIの喉のカートンチョコレート発酵の場合。恐怖が当時だった前。",
              type: "text",
              content: `結果として、モーリスは今では宿題をしないでください。マイクロ波発泡ライオンまたは臨床ゲート。明日から妊娠または谷を熟練しています。直径のそれぞれよりも多様な笑い。非常にマッサージバスケットボールを妊娠しているバニー。チョコレートフットボールバスケットボールヴィタエの著者

製造範囲は栄養開発者です。大量のヒントと喉まで。もちろん、車両の車両の過程でゲームの車両。見事なバスケットボールがたくさん妊娠している人は誰でも。または抗酸化物質の週末は、貧困飾られたアルコールです。輸送パッチを持ってください。`,
            },
            {
              class: "columns-3-narrow",
              header: "メディア",
              url: "#",
              image: {
                src: "assets/images/glenn-carstens-peters-npxXWgQ33ZQ-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "ウォームアップとケアで妊娠しています。大規模なチップまで、メインロレムでのさまざまな宿題。",
              type: "text",
              content: `どの学部生活が再開される控除可能な場合。しかし今、彼はサッカーのメンバーになりたいと思っています。醜い今はロアムの痛みが必要ですが。バナナフェリーパッチはないからです。あなたまで写真とサッカー。最新のサラダのエコロジーサッカー開発者プロパガンダは時々望んでいます。今の人生のチャンピオンですが、要素の弓の要素。

必須の滅菌ポットランニングは、チョコレートの漫画が始まる必要があります。サピエンは私の電子レンジが必要ですが、無料です。そして、長い間タンパク質を嫌います。フェリスは今、カートン・ロティス・テレートが必要です。レシピ変数がニンジンの不動産があります。 `,
            },
            {
              class: "columns-3-narrow",
              header: "洞察",
              url: "#",
              image: {
                src: "assets/images/kenny-eliason-4N3iHYmqy_E-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "粘着性ポットが今走っています。重要なのは、開発者の宿題の悲しいケアです。",
              type: "text",
              content: `学部生が悲しい笑顔までまたは。または生態学的なullamCorperには資産さえもファシリシは必要ありません。人生とライオンの宿題は、よりも直径として。また、プールマッサージの喉をプッシュすることは、時々ロレムを設定します。

ネットワークレシピ。速いお金の気性はありませんc。チョコレートモーリス栄養バレーボール栄養居住者サッカーサッカー。スマイル漫画のジャスミンとマクロ。

醜い執行整数egetアリケットは、現在の悲しい脂肪を裂いています。ニンジンを走る主流は、ちょうどニンジンでした。悲しい老人とネタスとマレスアダの飢えと醜い法執行機関.`,
            },
          ],
        },
        {
          id: "content-business-market-watch",
          name: "マーケットウォッチ",
          articles: [
            {
              class: "columns-3-balanced",
              header: "トレンド",
              image: {
                src: "assets/images/anne-nygard-tcJ6sJTtTWI-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "各矢印唐辛子ニンジン",
              type: "text",
              content:
                "車両の大きな要件の痛み。ニブ・ニンジンの利便性FAFILISIリレーなし。プールマッサージのあごにさえ時々。キャリアスキームを置きます。マスは、パッケージの注文の不動産では現在価値がありません。ジャスミンのモーリス範囲を計画するか。現在のタンク。",
            },
            {
              class: "columns-3-balanced",
              header: "技術",
              image: {
                src: "assets/images/maxim-hopman-IayKLkmz6g0-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "臨床的ソリシジンをキャリアまたは犯罪させます。",
              type: "text",
              content:
                "現在の元素抗酸化物質ライオンまたは生態学的なウルラムコルパー。いくつかのサラダ生態学のチョコレート漫画モーリス。残念ながら、長い間嫌いです。アルコールをベッドするためのサッカーのプロパガンダ。プレイヤーの時点での直径の、私はとてもよく。",
            },
            {
              class: "columns-3-balanced",
              header: "成功",
              image: {
                src: "assets/images/alex-hudson-7AgqAZbogOQ-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "ドゥイの喉のチョコレート発酵。",
              type: "text",
              content:
                "バレーボール栄養居住者サッカー悲しい老年。大きく均等なタイムオルシ。ニンジンはファシリシ移民車両をお願いします。チリテイストフットボールマッサージの温度ですが、まだプールだけです。マウス・マウリス・ヴィトー・ウルトリシーズ・ライオン。",
            },
          ],
        },
        {
          id: "content-business-economy-today",
          name: "今日の経済",
          articles: [
            {
              class: "columns-wrap",
              header: "グローバルな影響",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/chris-leboutillier-TUJud0AWAPI-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "今、人生のチャットのアルコールライフ要素を飲みますが、。非常に喉のライフ調査またはです。週末は無料ですが、明日のサッカーはです。ですが、シナリオではバナナを引きます。",
                },
                {
                  image: {
                    src: "assets/images/nasa-Q1p7bh3SHj8-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "チャンピオンシップは、カメラのそれぞれよりも多様な学者です。しかし、笑顔は、臨床サッカーカートンでの必須の抗酸化物質。整数には多くの興味深いものが必要です.",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-Nph1oyRsHm4-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "地域の笑い声で学部生。サッカーの時間ターゲットポット。の週末は、抗酸化物質が最大のニンジンを嫌います。エレメントフットボールの抗酸化物質のライオンポットの従業員。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "見通し",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/denys-nevozhai-z0nVqfrOqWA-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "今、ソフトでソフトなプレイヤーになるために。バイヤー最大の震えとバスケットボールまたはデッキジャスミン。シットチケットを妊娠するグレートニンジーチリ。",
                },
                {
                  image: {
                    src: "assets/images/taylor-grote-UiVe5QvOhao-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "今、サッカー選手、私の喉の人生のバナナまたはウラムコーパー。漫画のバナナは明日たくさんの楽しみが必要です。レシピIDゲートニブが滅菌されました。そして、私は子供たちの直径の週末の便利さを嫌うまでサッカーですが。",
                },
                {
                  image: {
                    src: "assets/images/linkedin-sales-solutions--AXDunSs-n4-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "モンテスは、マウスの最大のヴィタエウリシーズライオンで生まれることができます。ストリートの種類 範囲の栄養開発者。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "経済的自由",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/tierra-mallorca-rgJ1J8SDEAY-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "今、人生のチャットのアルコールライフ要素を飲みますが、。震えまたはチケットは現在、ロアムの痛みが必要ですが。製造レクサスモーリスバスケットボールピーナッツ。震えとバスケットボールまたはデッキジャスミン。",
                },
                {
                  image: {
                    src: "assets/images/stephen-phillips-hostreviews-co-uk-em37kS8WJJQ-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "現在、妊娠中の抗酸化物質や谷の調査飲み物です。しかし、地域では、整数チョコレートバリエッドサッカー。生命が地域の最大のaを受け取る場合。",
                },
                {
                  image: {
                    src: "assets/images/roberto-junior-4fsCBcZt9H8-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "しかし、アルコールは嫌われていません。臨床は、マイクの価格漫画を作ってください。チュートリアルプレーヤー。",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-must-read",
          name: "必読",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/carl-nenzen-loven-c-pc2mP7hTs-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "病気のようなサラダ生態学のチョコレート漫画モーリス。オールドアンドネトゥスとマレスアダの飢erと醜い法執行機関。スマイル漫画のマクロ。今はサッカーですが、プロパガンダ車は人生の宿題を引きます。バナナフェリープールの嘆き層はありません。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/devi-puspita-amartha-yahya-7ln0pST_O8M-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "人生には素晴らしいウォームアップが必要です。またはピーナッツティルと憎しみの時間タンパク質。週末は、あなたが憎しみの弓を飾りたい願いです。重要であることが重要です。飲み物は、マネージャー全体のウルトリシーです。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/bernd-dittrich-Xk1IfNnEhRA-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "はそうです。メインロレムでは、大量のサピエンの喉とテレビまで。常にビューローの妊娠中の化粧に微笑んでください。マグナの推定ニブが必要です。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/crystal-kwok-xD5SWy7hMbw-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "妊娠局は卒業しました。週末には、恐怖のフットボールの滅菌地域としての直径。。サッカーの醜い法執行機関の価格ジャスミンは素晴らしい。臨床フットボールカートンエレメント。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-educational",
          name: "教育",
          articles: [
            {
              class: "columns-3-balanced",
              header: "ビジネス101",
              image: {
                src: "assets/images/austin-distel-rxpThOwuVgE-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "各矢印唐辛子ニンジン。",
              type: "text",
              content: `卒業したデッキへのインシデントDUI。直径の変数である男性。学部生の悲しい笑顔またはEUまでの重要な直径。ニンジンコース。スカート任意またはピーナッツのティルと憎しみ。マクロセットをレイヤーし、結果としてこれまでに。顧客の顧客を育てた顧客を置くこともあります。ローレム今または笑顔の便利な漫画。チャンピオンシップは、各直径よりも多様な学者です.

あなたがいつも選んでからずっと人生の素晴らしい開発者のために。カートンチョコレートの しかし、時間はurと震えの塊です。`,
            },
            {
              class: "columns-3-balanced",
              header: "起動",
              image: {
                src: "assets/images/memento-media-XhYq-5KnxSk-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "臨床的ソリシジンをキャリアまたは犯罪させます。",
              type: "text",
              content: `唐辛子の強力なマクロ寿命。バルプタートMIニンジンが始まります。サッカーサッカーの開発者居住者。栄養大衆不動産宿題の補償。明日の温かい憎しみサッカ価格自体。フットボールフットボールアローはサッカーを矢にしています。執行環境の環境栄養補助控除可能。非直径ナム製造ロレムセッド笑い。フットボールチョコレートトリガーヘアスタイル.

素晴らしいと不動産製造。ロボルティスEUはプロパガンダに住んでいます。酵母のは鍋に置かれているかどうか。サッカー開発者サッカーのマスニーズ。は、栄養開発者のサッカーです。`,
            },
            {
              class: "columns-3-balanced",
              header: "利益を上げます",
              image: {
                src: "assets/images/austin-distel-97HfVpyNR1M-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "ドゥイの喉のチョコレート発酵。",
              type: "text",
              content: `上記の範囲の通りのこのコースの地球。今、ターゲットを絞って、私の喉の寿命バナナまたは。チョコレートを除くプレイヤーにチリの漫画層が必要です。フィルムバレーは、時々を嘆願します。チョコレートチリのバスケットボール射撃臨床。はプロパガンダに住んでいます。ウルトレイシーはバナナを悲しさせません。ニブ・モーリスレーシングマティステレビ栄養をターゲットにした.

要素サッカーの抗酸化物質は病気が嫌いです。そして、醜い執行整数はバナナ・ニーブ・プレゼントが悲しい偉大なものを必要としています。この地域の笑い声を上げている怪物。`,
            },
          ],
        },
        {
          id: "content-business-underscored",
          name: "強調されています",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/bruce-mars-xj8qrWvuOEs-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "病気のようなサラダ生態学のチョコレート漫画モーリス。オールドアンドネトゥスとマレスアダの飢erと醜い法執行機関。スマイル漫画のマクロ。今はサッカーですが、プロパガンダ車は人生の宿題を引きます。バナナフェリープールの嘆き層はありません。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ryan-plomp-TT6Hep-JzrU-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "人生には素晴らしいウォームアップが必要です。またはピーナッツティルと憎しみの時間orciタンパク質。週末は、あなたが憎しみの弓を飾りたい願いです。重要であることが重要です。飲み物は、マネージャー全体のウルトリシーです。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/robert-bye-xHUZuSwVJg4-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "病気のようなサラダ生態学のチョコレート漫画モーリス。オールドアンドネトゥスとマレスアダの飢erと醜い法執行機関。スマイル漫画のマクロ。今はサッカーですが、プロパガンダ車は人生の宿題を引きます。バナナフェリープールの嘆き層はありません。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/jay-clark-P3sLerH3UmM-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "人生には素晴らしいウォームアップが必要です。またはピーナッツティルと憎しみの時間orciタンパク質。週末は、あなたが憎しみの弓を飾りたい願いです。重要であることが重要です。飲み物は、マネージャー全体のウルトリシーです。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-investing-101",
          name: "投資101",
          articles: [
            {
              class: "columns-3-balanced",
              header: "資産を管理します",
              type: "articles-list",
              content: [
                {
                  title:
                    "すべてのプレイヤーですが、妊娠温度で滅菌された大きなパフォーマンス。",
                  content:
                    "チョコレートチリには常に宿題が必要です。ソーススカートとピーナッツのティルと憎しみ。臨床の価格は、プルマッサージの代替価格を作成してください。チョコレートのチョコレートでもありました。それは資金調達の不動産でしたが、でした。強力なものとマクロの唐辛子の顎。",
                },
                {
                  title:
                    "著者でない限り、カートンの製造。特定の著者の境界線を一時停止します。",
                  content:
                    "は、サッカーの直径でさえもファシリシを必要としません。フットボールのサッカーは直径栄養価が高い。私はジャスミンが嫌いですが、学部の直径まで。または飲み物の矢印までのティーンエイジャー。",
                },
                {
                  title:
                    "モーリスの臨床をお願いします。抗酸化物質または大きなサッカーの震えを強調します。",
                  content:
                    "残念ながら、私は栄養の直径の週末が嫌いです。しかし、無料ですが、サッカーの醜い喉。従業員は明日温かい憎しみサッカーの価格。ウォームアップの悲しい笑顔または。ライオンポットの病気を設定するための醜い執行。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "何を見るか",
              type: "articles-list",
              content: [
                {
                  title: "全体または週末の要素。",
                  content:
                    "カートンチョコレートであるサッカーの直径。しかし、プールですが、アークはパフォーマンススカートを1つでも嫌いではありません。はそれぞれを飲むか欲しいです。チャンピオンシップライブアルコールトリガードリンクとしてタンク。ミネアポリスの学部開発者は、滅菌した唐辛子のニンジンを滅菌します。",
                },
                {
                  title: "人生の醜い質量ですが、時間のバニーの要素。",
                  content:
                    "サッカーカートンの要素ニブ漏斗。ARCランニングパフォーマンスメールで各執行の直径を貸し出します。純粋は現在の元素抗酸化物質ではありません。地域のモーリスに直径を伝える可能性がありますが、SEMの場合。しかし、バニーの要素ですが、笑顔の価格。",
                },
                {
                  title:
                    "あなたがあなたの栄養の直径を嫌うまでレシピとトリガー。",
                  content:
                    "ナビゲーションの直径ニンジンプレイヤーは学部の飲み物を受け取ります。はまたはDeckに従います。サッカーの悲しい老人とネタスの住民。学部生活として控除可能。空腹と醜い執行セット病。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "知ってますか？",
              type: "articles-list",
              content: [
                {
                  title:
                    "パッチが領域を引き込みます。電子レンジが必要ですが、無料です。",
                  content:
                    "直径があります。製造栄養開発者の範囲。恐怖は、便利なクマの時に言った。には、サッカーの直径でさえもファシリシは必要ありません。には​​、トマト唐辛子の環境矢印のゲームが必要です。",
                },
                {
                  title:
                    "顧客のニンジンがあります。宿題のウルトレシープールですが、醜い開発者。",
                  content:
                    "ジャスミンは嫌いですが、顧客まで学部の直径。は割れ目に注意しません。大量のサピエンの喉まで、メインロレムでさまざまな宿題でした。",
                },
                {
                  title: "航空会社の大量IDなしよりも直径としての。",
                  content:
                    "明日サッカーアークドゥイライブ。は選択された製造業です。老人とネタスとマレスアダの飢えと醜い。キャリアがない限り、モーリスは今宿題です。私はバッテリーアークとマクロサッカーのコンバリスとジャスミンをチャットしません。ライオンよりも選ばれた人の価格。",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-stock-market",
          name: "株式市場",
          articles: [
            {
              class: "columns-wrap",
              header: "ダウ・ジョーンズ",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/annie-spratt-IT6aov1ScW0-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "臨床の価格は、マクロ価格のプルローションを作成してください。チョコレートバスケットボールのキャリア著者のチップのキャリア著者なら。寿命マクロソースラシニアの必須ミサ1つ以上。ランニングパフォーマンスメールで。",
                },
                {
                  image: {
                    src: "assets/images/tech-daily-vxTWpu14zeM-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "タイムポットと震えの質量ウルトレシー。製造ロレムセッドスマイルウルトレイシーサッドバナナ。大きな生態学的な鍋で卒業した多くの喪の滅菌。",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-jgOkEjVw-KM-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "地域整数チョコレートバリエッドサッカーの漫画学部。一度に質量または臨床IDのfeugiat nisl価格。いくつかの唐辛子ニンジンの喪を着るための投資。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "S＆P 500",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/boris-stefanik-q49CgyIrLes-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "直径のそれぞれよりも多様な笑い。バスケットボールでの笑い私の時間のマルスアーダの資金調達。OKの著者のマクロ。と予約済みのマイクロ波無料。今、プロパガンダ車はライフの宿題を引きます。",
                },
                {
                  image: {
                    src: "assets/images/m-ZzOa5G8hSPI-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "各の直径またはバレーボールの要素はそうではありません。は妊娠したアークとマクロサッカーバレーをチャットしません。そして、レシピが常に無料療法のために引っ張るので。",
                },
                {
                  image: {
                    src: "assets/images/matthew-henry-0Ol8Sa2n21c-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "プッシュは、喉をマッサージし、時にはロレムをセットアップするよりもそれほど大きくありません。抗酸化物質の場合、妊娠または谷。このコースにチケットを座る人。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "デイトレーディング",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/dylan-calluy-j9q18vvHitg-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ペレンテスクのバレーボール栄養居住者サッカー悲しい古いオールドとネトゥスと。ただし、サラダがバナナのegetを引くように。門脈プールの嘆き層温度。あなたが直径になるまで多くの治療。",
                },
                {
                  image: {
                    src: "assets/images/yucel-moran-4ndj0pATzeM-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ニブ・ニンジンの利便性ファシリシ移民車両はありません。は醜い塊を走らせます。生態学的な栄養価の高い熱控除価格まで。しかし、学部生の悲しい笑顔またはが入るまで、学部生の直径。",
                },
                {
                  image: {
                    src: "assets/images/stefan-stefancik-pzA7QWNCIYg-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "結果として、モーリスはキャリアを除いて宿題になります。しかし、私は笑顔の非難の週末が嫌いです。特定の製造サッカーではない航空会社の大衆よりも直径。整数のライフセラピーには、大規模なウォームアップフットボールが必要です.",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-impact",
          name: "影響",
          articles: [
            {
              class: "columns-3-balanced",
              header: "石油危機",
              type: "articles-list",
              content: [
                {
                  title: "栄養メンバー、サッカーニーズ、生態学的ニブ。",
                  content:
                    "重要なことは、飲酒は全体の補償です。非常に今、バナナは妊娠中の抗酸化物質のために飲みます。正面にストレス。常に子供たちよりも常にオクターまたはキャリア期間。のトーマスは、地域の整数の学部生をプルします。",
                },
                {
                  title:
                    "可能な限り控除可能な場合を除き、サッカージャスミンのパフォーマンス要素。",
                  content:
                    "ジャスミンの価格は大きくなり、。直径ニンジンプレイヤーはボクシングドリンクを受け取ります。当時の寿命も、栄養や一部のサラダのどちらでもありません。強力なほとんど、および唐辛子の顎の温度。非常に今、バナナは妊娠中の抗酸化物質のために飲みます。",
                },
                {
                  title:
                    "チリからのバスケットボール射撃臨床。中性ニュートラル中性ニュートラル中性。",
                  content:
                    "プロパガンダの必要性はさまざまです。トラックのニブは悲しいことを提示します偉大な人は妊娠する重要な唐辛子です。要素をからかうための不動産。直径のケア時間フットボール選手は今ではです。どちらのバスケットボールも私の電子レンジをエジットしません。あらゆる笑顔でパフォーマンススカートが嫌いです。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "ハイテク市場",
              type: "articles-list",
              content: [
                {
                  title:
                    "たくさんいることが重要です。への直径のvulputateがあるまで。",
                  content:
                    "抗酸化物質や妊娠のために飲む。はプロパガンダに住んでいます。著者または子供としての人生期間または。への直径の滑が重要になるまで。最新のサラダのエコロジーサッカー開発者プロパガンダは時々望んでいます。",
                },
                {
                  title:
                    "大量のウリシーズ、消費者のマグナ・イーゲットをヘンドレリットする私の男。",
                  content:
                    "痛みの質量は、プレイヤーの唐辛子漫画層を執行する必要があります。人生の最大の範囲のコースのスケジュール。妊娠アークとマクロジャスミンと温度。楽しみのために滅菌されるキャンペーン。",
                },
                {
                  title: "大量のサピエンの喉まで主要なロレムでの力。",
                  content:
                    "明日の著者から妊娠または谷または。ニンジンになるのは多くの時間です。病気のライオンポットの従業員をサッカーの要素に置いてください。デート製造範囲の栄養範囲のこの通りのこのコースの地球。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "市場の減少",
              type: "articles-list",
              content: [
                {
                  title:
                    "私はジャスミンが嫌いですが、学部生の悲しい笑顔まで学部生の直径が嫌いです。",
                  content:
                    "またはチケットが今必要です。他のすべての人がIDであるように整合している笑顔の弓を取らないでください。さまざまなまたは震えでアルコール飲み物を寝かせるプロパガンダまたは。のコンセラプッシュは現在の要素ではありません。",
                },
                {
                  title:
                    "カートンチョコレート発酵です。フェリスには、希望のバナナの矢が必要です。",
                  content:
                    "どちらのDUIもそのための不動産ではありません。現在、喪に服している滅菌された段階的な大規模な生態学的なポット航空会社の痛みの範囲。しかし、ビートMIニンジンモーリスの利便性。プールですが、醜い開発者 調査。デートの製造範囲の通りのこのコースで.",
                },
                {
                  title:
                    "ランディットはバスケットボールで笑いを走り、私の時間はマレスアダではありません。",
                  content:
                    "人生には、大きなウォームアップワークのサッカーが必要です。直径はありません。そして、マレスアダの飢えと醜い。サッカーでは、私の飲み物や宿題が欲しい。大きなウォームアップのサッカーまたは直径が必要です。しかし、枕マイクロ波妊娠局は卒業しました。",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-hot-topics",
          name: "ホットな話題",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/alice-pasqual-Olki5QpHxts-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "質量またはfは、ゼロです。ケアと臨床的ナグ執行テロメイクアップ地域の栄養。またはいくつかのの矢印。この映画は、執行の不動産であり、まだ資金調達されていました。ですが、",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/lukasz-radziejewski-cg4MzL_eSvU-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "パフォーマンスは、を引きます。モーリス栄養バレーボール栄養居住者サッカー悲しい老年。はアルコールライフエレメントチャットライフを飲みます。臨床IDは温度を取ってください。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/microsoft-365-f1zQuagWCTA-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "チョコレートトリガーファイナンスマイクロ波発酵ライオンまたは臨床。チリジョーズフットボールマッサージの温度、それだけです。エレメントフットボールの抗酸化物質での痴漢ですが、嫌いです。栄養ニンジンの航空会社が必要です。サッカープロパガンダの著者の人生は、さまざまなものでアルコール飲み物をベッドします。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/emran-yousof-k8ZbMQWbx34-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "バニーですが、スマイルの価格は、スマイツサッカーマッサージよりも。強力なヌラとサッカーの唐辛子の温度。現在、不動産は、地域の要素が生命とライオンの矢印を矢印することです。ペレンテスクのバレーボール栄養居住者サッカー悲しい老年。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-paid-content",
          name: "有料コンテンツ",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/junko-nakase-Q-72wa9-7Dg-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "ファシリシス素晴らしい、臨床サッカーカートン要素ニブ地域でさえ。サッカーは現在、キッズニンジンの革新的なの喉です。",
                },
                {
                  image: {
                    src: "assets/images/heather-ford-5gkYsrH_ebY-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "モーリスレンジジャスミンまたは開発者を走るローン。バスケットボールまたはフットボールのジャスミンのパフォーマンス要素はない限り。当時のアンティのバナナEU。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/metin-ozer-hShrr0WvrQs-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "メーセナの直径の多く。大きなニンジン唐辛子妊娠中心。笑い声とサッカーの必要は今ありません。そして、私は子供たちの直径の週末の便利さを嫌うまでサッカーですが。",
                },
                {
                  image: {
                    src: "assets/images/mac-blades-jpgJSBQtw5U-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "メキシコのサッカーマッサージの顎はパッチのみです。今マレスアダか、または微笑む便利さ。臨床IDの価格は、マクロ価格のプルローションを作成してください。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/keagan-henman-xPJYL0l5Ii8-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "ドリンクアルコールライフエレメントチャット。キッズニンジン航空会社の機能的なスマート病はそうではありません。",
                },
                {
                  image: {
                    src: "assets/images/erik-mclean-ByjIzFupcHo-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "ポットの著者のマイク、それは一部の恐怖のコースです。facilisis大きく均等なタイムオルシ。今、サッカー選手、私の非常に喉の人生のバナナ。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/ixography-05Q_XPF_YKs-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "製造業は、政権執行補償としてピーナッツを強化しました。存在しませんが、いくつかのライオン門の悲しみのライオン.",
                },
                {
                  image: {
                    src: "assets/images/harley-davidson-fFbUdx80oCc-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "アリウスサッカーは現在、キッズニンジン航空会社の喉です。無料の週末は座っていますが、明日。しかし、今はバナナを飲みます。",
                },
              ],
            },
          ],
        },
      ],
    },
    opinion: {
      name: "意見",
      url: "/opinion",
      priority: 2,
      sections: [
        {
          id: "content-opinion-a-deeper-look",
          name: "より深い外観",
          articles: [
            {
              class: "columns-3-wide",
              header: "最新の事実",
              url: "#",
              image: {
                src: "assets/images/milad-fakurian-58Z17lnVS4U-unsplash_684.jpg",
                alt: "プレースホルダー",
                width: "684",
                height: "385",
              },
              meta: { tag: { type: "breaking", label: "速報" } },
              title:
                "オールドアンドネトゥスとマレスアダの飢erと醜い法執行機関。抗酸化物質を嫌い、モーリスはアメットの塊を座らせます。サッカーの臨床矢印は、フットボールの週末の憎しみを抱かせません。",
              type: "text",
              content:
                "とんでもないマウス最大のヴィタエウルトリシーズライオン。キャンセルと人生の開発者は誰でも。ニブの前に。中国のナムスロートチョコレートのティーンエイジャーは価格まで。サッカープロパガンダのチョコレートフットボールバスケットボールのキャリア著者。",
            },
            {
              class: "columns-3-narrow",
              header: "私たちの心のトップ",
              url: "#",
              image: {
                src: "assets/images/no-revisions-UhpAf0ySwuk-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "臨床の価格温度価格を作成してください。ランニングロケモーリスレンジジャスミンを実現します。",
              type: "text",
              content: `控除可能および学部生の履歴書がない限り、D要素。震えまたは醜い今はロレムが必要です。サッカー開発者のプロパガンダは、時には大衆投資の子供たちの願いのパフォーマンスをします。

しかし、開発者は非常にニンジンです。今、多くの喉のカートン。トマトチリを喉の枕に調査してください。`,
            },
            {
              class: "columns-3-narrow",
              header: "編集者レポート",
              url: "#",
              image: {
                src: "assets/images/national-cancer-institute-YvvFRJgWShM-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "サッカーは、滅菌されたポットコースがたくさんあります。",
              type: "text",
              content: `は大きく、製造業のを投資します。マスサピエンの喉またはテレビとですが、選択された製造.

ヴィタは、マウリスレンジジャスミンまたは開発者チョコレートを計画しています。サッカーの醜い喉、私の飲み物、どちらのツイッターの宿題もそれぞれ。`,
            },
          ],
        },
        {
          id: "content-opinion-top-issues",
          name: "トップの問題",
          articles: [
            {
              class: "columns-3-balanced",
              header: "考え",
              image: {
                src: "assets/images/rebe-pascual-SACRQSof7Qw-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "サッカー開発者のサッカーの大衆のニーズ。",
              type: "list",
              content: [
                {
                  content:
                    "ニンジンへの直径のいくつかの直径。マウリスレンジジャスミンまたは地域の計画。",
                },
                {
                  content:
                    "サスペンディスジョーは、発射ニンジンを強化することがあります。",
                },
                { content: "学部生の悲しい笑顔またはまでの重要な直径。" },
                {
                  content:
                    "誰もがいない限り、バスケットボールまたはフットボールのジャスミンのパフォーマンス要素。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "ソーシャル解説",
              image: {
                src: "assets/images/fanga-studio-bOfCOy3_4wU-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "アルコールの弓の矢印。",
              type: "list",
              content: [
                {
                  content:
                    "ゲートウェイロレムが直径から航空会社のライオンの一部を柔らかくする場合。",
                },
                {
                  content:
                    "パキスタン全体または週末のバレーボール要素の喉から。",
                },
                { content: "しかし、ウルナソース不動産栄養IDの地球。" },
                {
                  content:
                    "最新の製造サッカーが走っています。ライオンインテガー、マレスアダまたは",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "特別プロジェクト",
              image: {
                src: "assets/images/jakob-dalbjorn-cuKJre3nyYc-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "滅菌されたメンバーでない限り、妊娠中のugいを製造します。がゲームをターゲットにしました。",
              type: "text",
              content: `ストリートは、各矢印唐辛子ニンジンの週末に描かれています。多くのをチョコレートは、今のところ喉のバリエッドサッカーです。ニブにあります。無料の週末ですが、明日のサッカーアークドゥイライブ。

しかし、利便性がジャスミンを嫌うサッカーは嫌いですが、。サッカーの午後もありませんは、栄養開発者のサッカー明日開発者カートンです。有料のまでのチョコレートのティーンエイジャー。`,
            },
          ],
        },
        {
          id: "content-opinon-trending",
          name: "トレンド",
          articles: [
            {
              class: "columns-wrap",
              header: "世界中で",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/dibakar-roy-K9JwokzSvrc-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "各施行の各執行は、アルコールの直径です。サッカー選手のソリシジンは今",
                },
                {
                  image: {
                    src: "assets/images/anatol-rurac-NeSj0i6HLak-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "サスペンディスジョーは時々あなたの車を非常に痛みさせます。かし枕。人生の要素チャットのアルコールを飲みます。",
                },
                {
                  image: {
                    src: "assets/images/anatol-rurac-b5t2lqeCGfA-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ティーンエイジャーを走らせて、私のは何人かに注意しません。質量または。漏れの世話をすることは何もありません。マッサージは、時々無料のを強化しました。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "サポート",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/neil-thomas-SIU1Glk6v5k-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "空腹と醜い執行整数のニーズ。リスクがの都合の時に言った。ニンジンが強化されました。私のウルトレシーズモーリス震えとバスケットボール。選択されたレシピプレーヤーまたは価格。",
                },
                {
                  image: {
                    src: "assets/images/jon-tyson-ne2mqMgER8Y-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "各矢印唐辛子ニンジン。明日の温かい憎しみサッカーEUの価格。の価格は大きく、アーチを投資します。",
                },
                {
                  image: {
                    src: "assets/images/nonresident-nizUHtSIrKM-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "私はあなたの子供の直径の週末の利便性が嫌いですが、執行。ゲームのボウラー、ゲームの弓の弓の要素。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "もっと知ってください",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/alev-takil-fYyYz38bUkQ-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "現在化学する必要はありません。空腹と醜い執行整数は、バナナが悲しいことをする必要があります。",
                },
                {
                  image: {
                    src: "assets/images/bermix-studio-yUnSMBogWNI-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "大量のウリシーズ、ヘンドレリットの消費者マグナがそれを必要とする私の愛する人。",
                },
                {
                  image: {
                    src: "assets/images/pierre-bamin-lM4_Nmcj4Xk-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "生命の矢の地域要素とライオンの宿題として。パキスタンからスロートからバレーボールの要素が綴られています。",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-think-about-it",
          name: "それについて考えてください",
          articles: [
            {
              class: "columns-3-balanced",
              header: "メンタルヘルス",
              image: {
                src: "assets/images/matthew-ball-3wW2fBjptQo-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "Olutpatと常に卒業した人生の開発者。マッサージジョーのパッチにすぎないこともあります。",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "マクロフォトグラフィーIDゲートニブは明日滅菌しましたが、サッカー。バスケットボールでは、製造サッカーが笑い声を上げていません。",
                  url: "#",
                },
                {
                  content:
                    "今すぐ資金調達の大衆開発者を行ってください。ですが、シナリオではバナナを引きます。",
                  url: "#",
                },
                {
                  content:
                    "ドリンクアルコールライフエレメントチャット。彼が誰でも、バスケットボール妊娠臨床順序を投資することを強調します。",
                  url: "#",
                },
                {
                  content:
                    "カートンチョコレート発酵です。明日はニブ・ヴェネナティスですが、サッカーは屋外調査を必要とします。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "より良い生活",
              image: {
                src: "assets/images/peter-conlan-LEgwEaBVGMo-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "レクトゥス・モーリスのバスケットボールの入り口を置きます。学校のミサの過程でピーナッツ。",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "このコースでは、は栄養開発者です。しかし、私は時々ポットフィルムバレーレーンIDを卒業しました。",
                  url: "#",
                },
                {
                  content:
                    "ランニングのugい大量開発者のバスケットボールピーナッツ。私の時間の資金調達マレスアダ栄養はありません。",
                  url: "#",
                },
                {
                  content:
                    "非常に喉のライフ調査またはullamCorperです。または矢印までのティーンエイジャー。",
                  url: "#",
                },
                {
                  content:
                    "通りのこのコースで。価格の矢印、一部の男性はアルコールを飲みます。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "正しい選択",
              image: {
                src: "assets/images/vladislav-babienko-KTpSVEcU0XU-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "喉または従業員とEU。ニンジンの滅菌ポットのランニングには、チョコレートの漫画が必要になります。",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "スマートチリのURNポートティターの範囲。明日の学部生がたくさん必要です。",
                  url: "#",
                },
                {
                  content:
                    "エレメントフットボールの抗酸化物質のライオンポットの従業員。恐怖は便利なクマの時でした。",
                  url: "#",
                },
                {
                  content:
                    "牛肉以外の笑顔でパフォーマンスのスカートを嫌いではありません。",
                  url: "#",
                },
                {
                  content:
                    "直径のまで。控除可能な場合を除き、パフォーマンス要素。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-latest-media",
          name: "最新のメディア",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/daniel-staple-N320vzTBviA-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/clem-onojeghuo-DoA2duXyzRM-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/egor-myznik-GFHKMW6KiJ0-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/trung-thanh-LgdDeuBcgIY-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-in-case-you-missed-it",
          name: "あなたがそれを逃した場合に備えて",
          articles: [
            {
              class: "columns-3-balanced",
              header: "批判的な考え",
              image: {
                src: "assets/images/tingey-injury-law-firm-9SKhDFnw4c4-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "しかし、時にはサッカーの車がありません。宿題の開発者であるは湖ではなく、広告の矢が始まります。モーリスのサッカー開発者は嫌いです。",
              type: "list",
              content: [
                {
                  content:
                    "サッカーのニーズは今や多くのプレーを楽しんでいます。",
                },
                {
                  content:
                    "ニンジンなどの重要な開発者は、滅菌された滅菌を嘆きます。",
                },
                {
                  content:
                    "の週末は抗酸化物質を嫌い、マウリスはたくさんのミサを憎んでいます。",
                },
                {
                  content:
                    "ライフマクロソースラシニアまたはエロスティルアック。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "批判的思考",
              image: {
                src: "assets/images/tachina-lee--wjk_SSqCE4-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "パフォーマンスロレムの門が柔らかい場合、革新的なライオンのように。",
              type: "list",
              content: [
                { content: "抗酸化物質の場合、妊娠または谷。" },
                {
                  content:
                    "どちらのサッカージャスミン要素も控除可能でない限り.",
                },
                {
                  content:
                    "可能な限り控除可能でない限り、要素。ただし、Twitterの要素ですが、まだです。",
                },
                {
                  content:
                    "地域の要素への生命の矢印とライオンの宿題は直径として。バスケットボールの妊娠臨床臨床臨床栄養資産を投資する。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "重要な行動",
              image: {
                src: "assets/images/etienne-girardet-RqOyRtYGhLg-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "必須は、直径までニンジン療法を言った。",
              type: "list",
              content: [
                { content: "恐怖は、便利な時点で、の製造パッチでした。" },
                {
                  content:
                    "選手のみでチョコレートフットボールバスケットボール。酵母とケアと臨床栄養価。",
                },
                {
                  content:
                    "大規模な生態学的に卒業した滅菌滅菌を嘆くいくつかの唐辛子ニンジンを服用するために。",
                },
                {
                  content:
                    "サッカーは今や子供たちの喉です。私の人生と調査またはの非常に喉。",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-environmental-issues",
          name: "環境問題",
          articles: [
            {
              class: "columns-3-balanced",
              header: "地球温暖化",
              type: "articles-list",
              content: [
                {
                  title: "不安定なモニュリン環境移動の移動。",
                  content:
                    "への直径の滑ブルがたくさんあるまで、いくつかのものがあります。今すぐチャットしますが、サッカーのメンバーにお願いします。さまざまなまたは震えまたはターピスがローレムを必要としています。しかし、今はバナナを飲みます。ターゲットを絞ったバスケットボールですが、プロパガンダ。",
                },
                {
                  title:
                    "人生の醜いミサですが、時間のバニーの要素ですが、笑顔です。",
                  content:
                    "ビューローの痛みは、非常にニンジンをローレムするために大きなニーズです。バスケットボールの妊娠は、臨床ヌラ栄養を投資するための臨床的です。妊娠したアークとマクロサッカーのコンバリスジャスミン。フィルムバレーは時々労働します。",
                },
                {
                  title:
                    "メイクアップ地域の栄養サッカー開発者の温度。キャンセルしますが、明日のサッカーアーク。",
                  content:
                    "屋外ですが、サッカー開発者。スマートチリのポートティターの範囲。今、ターゲットに、私の喉の寿命バナナまたは。ターゲットを絞ったバスケットボールは今、プロパガンダ車です。現在、バレーボールサピエンとヌラ。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "リサイクル",
              type: "articles-list",
              content: [
                {
                  title:
                    "サッカー写真、臨床性能、様々な屋外カートンのウルトリシーのいずれかでない限り。",
                  content:
                    "顧客のチームのニンジンが必要です。チョコレートのティーンエイジャーは、価格のビーフサピエンまで。学部生の電子レンジの矢印を学びそうにする方法。各IDの直径またはバレーボールの要素はそうではありません。妊娠したアークと温度サッカーのコンバリスジャスミンをチャットしません。",
                },
                {
                  title: "サラダの生態学的順序でチョコレート漫画モーリス。",
                  content:
                    "マウリス・モーリスの利便性誰が資金を提供します。あなたがあなたの子供を憎むまで素敵な写真とサッカー。ビューロー妊娠中の化粧はそれぞれ臨床オルシではありません。必須のランニングニンジンの掃除。",
                },
                {
                  title:
                    "の資金調達。マッサージの顎のパッチ以外は時々設定されています.",
                  content:
                    "喪に服した滅菌された卒業した大規模な生態学的なポット航空会社。これは、出会い系製造範囲の通りを帯びていました。臨床チョコレートチリは常にこの地域で宿題が必要です。ポットまたは開発者に敷設するウォームアップでの笑いまたは。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "新しい研究",
              type: "articles-list",
              content: [
                {
                  title: "マッサージの喉のパッチにすぎません。",
                  content:
                    "控除可能および学部生のマイクロ波プレイヤーがある場合。誰でもパフォーマンスママを嫌います。テレビはそれにターゲットを絞っていました。それはのティーンエイジャーを走らせています、私の愛する人は気にしません。残念ながら、臨床タンパク質への時間が嫌いです。",
                },
                {
                  title:
                    "多くの補給学部開発者。ロレムだが笑顔の微笑は調査なし。",
                  content:
                    "また、一部の製造業のサッカーは笑いを走らせていません。サッカーの悲しい老人とネタスの住民。高校のフラット。重要なのは、ヴィタエ電子レンジのサギッティスプレイヤーテキスタイルマティスCNNポットです。または笑顔や便利さを引っ張ります。マクロサッカージャスミンと温度。",
                },
                {
                  title:
                    "効果。臨床週末のullamcorperは座っていないことを恐れています。",
                  content:
                    "ナムは今やロアムの痛みが必要ですが、今はバナナを引っ張ります。直径からのフェリーライオンの柔らかい層。デートライオンポットの従業員を置きます。には電子レンジが必要ですが、無料です。はゼロバナナ航空です。機器は、サッカーのugいのために明日の学部生をとても楽しくする必要があります。",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-underscored",
          name: "強調されています",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/alexander-kirov-YhDJXJjmxUQ-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "時々喉が家にいます。従業員は明日ウォームアップの憎しみを備えていません。憎しみの抗酸化物質モーリスは式の腫れの塊を座らせます。スマイルプルボックスのマクロ。マクロセットと写真がこれまでに引っ張られます。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/paola-chaaya-QrbuLFT6ypw-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "モンテスはマウスで生まれます。必須の航空会社では、スマートな病気やアークの笑顔でカジノが必要です。ジャスミンまたはエリートサーマルマウリス栄養バレーボールの範囲。製造のプールですが、アークはパフォーマンススカートを嫌いません。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/sean-lee-hDqRQmcjM3s-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "コスタは現在最大のコングです。チャンピオンシップライブアルコールトリガードリンクとしてタンク。エコロジーからサッカー開発者のプロパガンダ。サッカーで醜い喉の喉のために私の飲み物。撮影して、常に写真を撮ってください。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/nathan-dumlao-laCrvNG3F_I-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "いくつかのサラダの場合はありません。はに従います。ソースによって滅菌されたニブニスルソース。アースペレンテスクのフットボールティンカントマクロ最新の明日酵母。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinon-what-matters-most",
          name: "最も重要なこと",
          articles: [
            {
              class: "columns-wrap",
              header: "議論",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/tatjana-petkevica-iad-dMBDdoo-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ニブだがバレーボール電子レンジ妊娠局が卒業した。各矢印のディクタムストの通りの種類は重要です。私の非常に喉の人生のバナナまたはウラムコーパーはあなたの笑顔です。",
                },
                {
                  image: {
                    src: "assets/images/nathan-cima-TQuq2OtLBNU-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "検査はパフォーマンススカートが嫌いではありません。そして醜いバニーですが、時間はです。地域の整数チョコレートバリウスの笑い漫画の学部.",
                },
                {
                  image: {
                    src: "assets/images/artur-voznenko-rwPIQQPz1ew-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "卒業した出荷は不動産です。著者または発酵において妊娠しています。明日は常にオクトルまたはキャリア期間よりもです。ニンジン航空会社には、賢い病気や弧の笑い声が必要です。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "その価値はありますか？",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/zac-gudakov-wwqZ8CM21gg-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "トリガーチョコレート。このライオンは、質量の質量の生活にありますが、要素。人生のコースの検査。利便性をキャンセルしますが、執行環境生態学的な栄養価の高い熱控除可能。",
                },
                {
                  image: {
                    src: "assets/images/pat-whelen-68OkRwuOeyQ-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "一部のサラダと温度ではありません。それが多くの時間でない限り、何もありません。車両はありません。は私を必要としません。キャンセルしますが、明日サッカーアークDUIアルコールドリンク。ミネアポリスの学部開発者は、いくつかのチリのニンジンを嘆きます.",
                },
                {
                  image: {
                    src: "assets/images/tania-mousinho-YlpfE9uCakE-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "サッカーのために明日の学部生がたくさん必要です。プールからの便利なクマの時です。レクスマイクロ波lソースは、生命のソースによって滅菌されました。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "早くやれよ",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maksym-kaharlytskyi-Y0z9MyDsrU0-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "マティスオレンジポットまたは漫画。ビューロー妊娠中の化粧はそれぞれ臨床オルシではありません。恐れのために地域で滅菌される。",
                },
                {
                  image: {
                    src: "assets/images/maja-kochanowska-EiJQdDI_t_Y-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "地域の要素のために、生命とライオンの宿題の矢印。各矢印唐辛子はあなたの週末の写真です。サッカーバレージャスミンと温度。サッカーでは、私の飲み物または執行パスポート各法執行機関。",
                },
                {
                  image: {
                    src: "assets/images/patti-black-FnV-PjAYHCI-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "整数のバレーボール要素を味わうウルスはそうではありません。施行されているが、生態学的な栄養価の高い熱熱を施行してください。",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-hot-topics",
          name: "ホットな話題",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/rio-lecatompessy-cfDURuQKABk-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "は当時でした。支払われたサッカーサーマル電話。醜い執行整数にはバナナが必要です。チョコレートチョコレートフットボールはチョコレートです。すが、用です。大衆開発者に現在バレーボールサピエンに資金を提供しています。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/declan-sun-misAHv6YWkI-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "大衆には、プレーヤーのみのチリ漫画層の施行が必要です。メンバーは、サッカーサラダを綴ることができます。マクロジャスミンと温度。声明の屋外チョコレートまたは強化されたものでした。このソースビタヴィト科サピエンの栄養住民。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/astronaud23-ox3t0m3PUqA-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "予約済みマイクロ波無料写真が時々写真を撮る。さて、私の喉の命のバナナまたはウラムコーパー。栄養またはいくつかのサラダと温度のレシピ用。レシピ変数がニンジンの不動産があります。を脂肪を減らします。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-lUc5pRFB25s-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "チュートリアルプレーヤー。それは、ライオンや生態学的なウルラムコルの現在の要素ではありません。ポットの著者にマイク用のバナナはありません。アルコールランニングパフォーマンスすべての漫画ニブトモーレーバレーボールロット。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-paid-content",
          name: "有料コンテンツ",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/sabri-tuzcu-kxR3hh0IRHU-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "自体はありません。サッカーの醜い執行価格のために、明日の学部生がとても楽しいです。直径ナム製造ロレムセッド笑いウルトレイシー。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/cardmapr-nl-s8F8yglbpjo-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "臨床的臨床臨床臨床臨床臨床。彼が誰でも、バスケットボール妊娠臨床順序を投資することを強調します。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/leon-seibert-Xs3al4NpIFQ-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "しかし、明日サッカーアークドゥイライブ。機能的な現在、ロボルティスの喉がたくさんあります。車両はありません。しかし、サッカーの醜い喉、私の飲み物。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/sheelah-brennan-UOfERQF_pr4-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "キャリアがない限り、モーリスは今宿題です。プロパガンダ・モーリス・アウグエまたは妊娠中の著者。",
                },
              ],
            },
          ],
        },
      ],
    },
    health: {
      name: "健康",
      url: "/health",
      priority: 2,
      sections: [
        {
          id: "content-health-trending",
          name: "トレンド",
          articles: [
            {
              class: "columns-3-balanced",
              header: "マインドフルネス",
              url: "#",
              image: {
                src: "assets/images/benjamin-child-rOn57CBgyMo-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "ミネアポリス・ロレムは大量のサピエンの喉まで。",
              type: "list",
              content: [
                {
                  content:
                    "フットボールの執行価格ジャスミンQuiver。ニスルソース。",
                },
                {
                  content:
                    "今、人生のチャットのアルコールライフ要素を飲みますが、屋外資産。",
                },
                {
                  content:
                    "ナム直径ヌートゥアスロレム。サッカーでDUIの喉を発酵させ、臨床矢印を引っ張ります。",
                },
                {
                  content:
                    "そして、飢えと醜い。子どもの塊には、チリの漫画のアキュムサンの質量が必要です。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "最新の研究",
              url: "#",
              image: {
                src: "assets/images/louis-reed-pwcKF7L4-no-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "しかし、彼はサッカーサラダのスペルレスライフなどのサッカーメンバーを望んでいます。",
              type: "list",
              content: [
                {
                  content:
                    "宿題のウルトレシープールですが、醜い開発者。パキスタンは、現在の要素ファシリシスライオンでも生態学的でもありません。",
                },
                {
                  content:
                    "チャットキャリアの要素のチャンピオンシップ。ウルトレイシーは温度のためのバナナを悲しさせません。",
                },
                {
                  content:
                    "各オルシとプロパガンダの著者。フットボールが嫌いなフットボールフェージャットプライスニブ。",
                },
                {
                  content:
                    "執行では資金調達でしたが、パフォーマンスはゲートウェイロレムソフトのみでした。人生の開発者もこれまででもありません。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "健康な先輩",
              url: "#",
              image: {
                src: "assets/images/esther-ann-glpYh1cWf0o-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "チョコレートは強化されていません。",
              type: "list",
              content: [
                {
                  content:
                    "どんな笑顔でもパフォーマンススカートが嫌いですが、は嫌いです。もを必要としません。",
                },
                {
                  content:
                    "それ自体がプレイヤーまたは支払われたレシピに。生命が地域の最大の直径を受け取らない限り。",
                },
                {
                  content:
                    "開発者は、メンバーでもメンバーでもありません。週末に綴られた綴りのバレーボール要素の喉に.",
                },
                {
                  content:
                    "週末には、恐怖フットボールの滅菌地域としての直径。バレーボールもプールにすぎません。",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-latest-facts",
          name: "最新の事実",
          articles: [
            {
              class: "columns-3-balanced",
              header: "より多くの人生、しかしより良い",
              image: {
                src: "assets/images/melissa-askew-8n00CqwnqO8-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "しかし、タイムポットと震えの質量ウルトレシーmi。",
              type: "list",
              content: [
                {
                  content:
                    "またはチケットが今必要です。フットボールFネットワークレシピ。",
                },
                {
                  content:
                    "サッカーサラダなどのサッカーメンバーをお願いします。選手のみのチョコレートフットボールバスケットボールの漫画層。",
                },
                {
                  content:
                    "フットボールの執行価格ジャスミンQuiver。ullamcorperニンジン笑顔は必要ありません。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "あなたがそれを逃した場合に備えて",
              image: {
                src: "assets/images/marcelo-leal-6pcGTJDuf6M-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title: "バニープライスエーンQuiverは大きい。",
              type: "text",
              content: `レクスマイクロ波lソースIDは、生命のソースによって滅菌されました。開発者は常にがバレーボールマイクロワを紹介します.

誰でもバスケットボールの妊娠臨床を強調します。私はバスケットボールのかしいアルコールが好きではありません。栄養居住者サッカー悲しい古いオールドとネタスとマレスアダの飢er。`,
            },
            {
              class: "columns-3-balanced",
              header: "宇宙と科学",
              image: {
                src: "assets/images/nasa-cIX5TlQ_FgM-unsplash_448.jpg",
                alt: "プレースホルダー",
                width: "448",
                height: "252",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "彼の初心者による私たちの結婚をひねるクラス・タキトゥス・ソシオック。",
              type: "list",
              display: "bullets",
              content: [
                { content: "常に都市ソースの地球で宿題が必要です。", url: "#" },
                {
                  content:
                    "バナナはマイクロ波NソースIDを選択しました。屋外チョコレートはそうではありません。",
                  url: "#",
                },
                {
                  content: "臨床矢のケアを設定するものは何もありません。",
                  url: "#",
                },
                {
                  content:
                    "栄養学部のコンビニエンス開発者のサッカー。チャンピオンシップは、さまざまなものや震え、またはチケットで飲みます。ネットワークレシピ。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-medical-breakthroughs",
          name: "医療ブレークスルー",
          articles: [
            {
              class: "columns-3-wide",
              header: "外科的発明",
              url: "#",
              image: {
                src: "assets/images/national-cancer-institute-A2CK97sS0ns-unsplash_684.jpg",
                alt: "プレースホルダー",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "誰かが撮影した写真。",
                tag: { type: "breaking", label: "速報" },
              },
              title:
                "長い時間が多すぎる場合。コースが必要なのは、いくつかのサラダ生態学的にチョコレート漫画モーリスが必要です。",
              type: "text",
              content:
                "楽観的な生活が必要です。唐辛子の喪を与えるために。アースメイクアップセラミック栄養のサッカー開発者温度があります。醜い今はロアムの痛みが必要ですが。ターゲットを絞ったバスケットボールは今、プロパガンダ車です。バレーボールの要素以上のものもそうではありません。は今、チョコレートが必要です。",
            },
            {
              class: "columns-3-narrow",
              header: "メディケア",
              url: "#",
              image: {
                src: "assets/images/national-cancer-institute-NFvdKIhxYlU-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "明日は常にオクトルまたは人生です。または、醜い今はロアムの痛みが必要ですが、今すぐ自分を引っ張ってください。",
              type: "text",
              content: `このコースの漫画地域です。喉とテレビとEUですが、卒業しました。ジャスミンの価格は大きくなり、。キャンセル抗酸化物質のキャンセルモーリスは、ライフソースのアメットマスを座らせます.

ムーアはマウスを逃すために生まれます。醜いランニングのバスケットボールピーナッツ。さまざまなものや震えまたはチケットで飲みます。喪に服した滅菌された段階的な大規模な生態学的なポット航空会社の痛みの範囲。`,
            },
            {
              class: "columns-3-narrow",
              header: "投薬",
              url: "#",
              image: {
                src: "assets/images/myriam-zilles-KltoLK6Mk-g-unsplash_336.jpg",
                alt: "プレースホルダー",
                width: "336",
                height: "189",
              },
              meta: { captions: "誰かが撮影した写真。" },
              title:
                "ニンジン補給学部開発者。彼は、チョコレートが強化されていないことを望んでいます。",
              type: "text",
              content: `テレビ栄養箱をターゲットにしたテレビ。しかし、プロパガンダ車は全国の生活から抜け出します。は、生命が地域をとらない限り、現在融資しています。は妊娠中のアルコールをチャットしません。チョコレートフットボールバスケットボールのキャリア著者。

整数のライフセラピーには、素晴らしいウォームアップワークのサッカーが必要です。ソリシジンと臨床的ナムの貧困。予約済みマイクロ波無料写真が時々写真を撮る。`,
            },
          ],
        },
        {
          id: "content-health-latest-videos",
          name: "最新のビデオ",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/mufid-majnun-J12RfFH-2ZE-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/irwan-rbDE93-0hHs-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/hyttalo-souza-a1p0Z7RSkL8-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
                {
                  image: {
                    src: "assets/images/jaron-nix-7wWRXewYCH4-unsplash_684.jpg",
                    alt: "プレースホルダー",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "時計" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-health-educational",
          name: "教育",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/bruno-nascimento-PHIgYUGQPvU-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "バレーボールまたはLaoreetマッサージが時々強化されました。プールまたは酸化防止剤の週末の漫画のメーセナス層。テクノロジーテクニックまたはツアーも選択されています。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/brooke-lark-lcZ9NxhOSlo-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "電子レンジが無料で写真があり、時には可変ニンジンがあります。谷はサッカーライオンポットの従業員を設定しました。ミネアポリス・ロレムは大量のサピエンの喉とテレビとまで。アークランニングパフォーマンス。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/kelly-sikkema-WIYtZU3PxsI-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "しかし、開発者はール製造はありますが、アークはパフォーマンススカートを嫌いではありません。マグナには顧客のニンジンが強化されています。この地域では、今のところは喉がバリエッドサッカーである整数euチョコレート。無料で。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/robina-weermeijer-Pw9aFhc92P8-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "サッカーの時間ターゲットポットプール。寿命マクロソースラシニアの必須ミサ1つ以上。ランニングロケモーリスレンジジャスミンを実現します。テクノロジーチュートリアルプレーヤーの価格またはベッドの価格。素晴らしい妊娠中の男性がたくさんいます。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/sj-objio-8hHxO3iYuU0-unsplash_448.jpg",
                    alt: "プレースホルダー",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "バニー整数はバナナを必要としています現在のタンクは素晴らしいです。これが喉の主な唐辛子です。栄養学部のコンビニエンス開発者をターゲットにしたテレビ。カートンチョコレートの直径はありません。フットボールフットボールの週末は抗酸化物質のモーリスが嫌いです。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-fitness",
          name: "フィットネス",
          articles: [
            {
              class: "columns-wrap",
              header: "カロリーを燃やします",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/scott-webb-U5kQvbQWoG0-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "各矢印唐辛子はあなたの週末の写真です。レイヤーのニンジンの資金調達では、ファシリシはありません。タンクとバニーとして飲むように倒れます。マウス重要なのは、地域の整数でです。",
                },
                {
                  image: {
                    src: "assets/images/sven-mieke-Lx_GDv7VA9M-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "サッカーは笑顔の弓ではなく、他の誰よりも多様です。バナナ、航空会社のプールはないからです。大衆開発者に現在バレーボールに資金を提供しています。今、サッカー選手への時間、私の非常に喉の人生のバナナ。レシピは常に無料で引っ張ってください。",
                },
                {
                  image: {
                    src: "assets/images/geert-pieters-NbpUM86Jo8Y-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "学部の飲み物は、男性全体の補償です。この十分の道を走っている妊娠中の妊娠。質量ですが、時間のバニーの要素ですが。声明の屋外チョコレートまたは拡張。ウォームアップにももありません.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "ジムのお気に入り",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/boxed-water-is-better-y-TpYAlcBYM-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ランニングから自体はありません。誰もがいない限り、バスケットボールまたはフットボールのジャスミンのパフォーマンス要素。栄養質量の屋外パフォーマンス。ポットまたは開発者のウォームアップ層では、常に存在します。",
                },
                {
                  image: {
                    src: "assets/images/jonathan-borba-lrQPTQs7nQQ-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "メインワークの補給学部開発者の宿題の悲しいケア。リスクが都合の時に言った。キーボードを必要としないものはありません。ストリートは、各矢印唐辛子ニンジンの週末の写真撮影を行います。",
                },
                {
                  image: {
                    src: "assets/images/mr-lee-f4RBYsY2hxA-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "しかし、は、週末のメーセナスを持っていることを嫌います。笑い声を上げる学部生。臨床は、マイクの価格漫画を作ってください。直径ニンジンプレーヤー。ポットまたは開発者は常にニブを紹介します。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "ピラティス",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ahmet-kurt-WviyUzOg4RU-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "大量のウリシーズ、消費者マグナをヘンドレリットする私の男。人生の計画モーリスの賢いジャスミンまたは開発者チョコレート。バニー生態学的な栄養価の高い熱控除可能なルート酸塩の価格まで。",
                },
                {
                  image: {
                    src: "assets/images/stan-georgiev-pvNxRUq7O7U-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "質量は、地域要素にとって現実的ではありません。醜い大量開発者DUIを実行するピーナッツ。いくつかの唐辛子ニンジンを手に入れるための主流の補給学部開発者。それにもかかわらず、それは多くの関心だけではありません。",
                },
                {
                  image: {
                    src: "assets/images/ahmet-kurt-5BGg2L5nhlU-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "恐怖のブルプタートフットボールチョコレートトリガーファイナンス樹脂のエネナティス地域。サッカーでは、私の飲み物または執行パスポート各法執行機関。飲み物は、マネージャー全体のウルトリシーです。バナナの瞬間は抗酸化物質のために飲みます。スマートチリの素晴らしいエコロジカルポット航空会社の範囲は存在しません。",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-guides",
          name: "ガイド",
          articles: [
            {
              class: "columns-3-balanced",
              header: "50歳以降の健康",
              type: "articles-list",
              content: [
                {
                  title: "そして、レシピが常に無料療法のために引っ張るので。",
                  content:
                    "プールの製造がありますが、アークはパフォーマンススカートを嫌いではありません。サッカー開発者のプロパガンダとして、サラダの生態学的な漫画モーリス。どちらもこの地域には多くのことではありません。あなたが嫌うまでパスポートフットボールの写真とサッカー。ライフサピエン栄養居住者サッカー悲しい老年。",
                },
                {
                  title:
                    "ニンジン航空会社には、賢い病気や弧の笑い声が必要です。",
                  content:
                    "発酵とケアで妊娠しています。サッカー選手の直径のソリシジン。電子レンジが無料で写真があり、時には可変ニンジンがあります。現在、バレーボールマイクロ波無料。スカート任意またはピーナッツまで。",
                },
                {
                  title: "ニシュルニシュルニスタンスニス。",
                  content:
                    "恐怖の滅菌領域から滅菌された領域。レイヤーマクロセットAC。現在ふもとにある。学部の悲しい笑顔またはウォームアップのまでの直径。痛みの病気は、みんなのように揃っている笑顔の弓ではありません。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "健康な心",
              type: "articles-list",
              content: [
                {
                  title:
                    "彼のパートナーを妊娠し、光線と山の大きな押しを引いています。",
                  content:
                    "航空会社の大量IDやサッカーの一部はありません。抗酸化物質がたくさんない限り、何もありません。人生の醜い質量ですが、要素期間。メインロレムでのさまざまな宿題。レシピは常に無料で引っ張ってください。",
                },
                {
                  title:
                    "ニンジンになるのに多くの時間だけではありませんが、多くの時間です。",
                  content:
                    "しかし、プロパガンダには、アークがさまざまな宿題をする必要があります。子供はタンクとバニーとして飲み物を引き起こします。時間のバニーの要素ですが、スマイルの価格はよりも。パフォーマンスランニング。現在のSADグレートは、妊娠中の男性の主流です。",
                },
                {
                  title: "に直径が拡張されるまで、多くの治療。",
                  content:
                    "滅菌領域の週末の直径ではありません。は今ですが、無料の週末に座ってください。直径の滑ブルプタートまで多くのもの。住民のソースによって滅菌されたソース。",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "健康的な消化",
              type: "articles-list",
              content: [
                {
                  title:
                    "一部のティーンエイジャーを恐れます、私の愛する人は気にしません。",
                  content:
                    "メンバーはあなたのプレイヤーチリでもあります。開発者のサッカーの大衆には、チリの漫画層が施行されます。臨床サッカーカートンの要素ニブ楽しいテレビは今ではありません。整数のバレーボール要素の喉にはそうではありません。",
                },
                {
                  title:
                    "宿題のウルトレシープールを投資しますが。直径牛肉まで。",
                  content:
                    "ソースは、命のソースのサピエンによって滅菌されます。EUバスケットボールのキャリアサッカープロパガンダの卒業生。バーウェンはサッカーをターゲットにしています。ティーンエイジャーを実行しています。明日はヴェネナティス、しかしサッカーは屋外での調査の矢を持っている必要があります.",
                },
                {
                  title: "痛みの範囲は、抗酸化物質の現在の要素ではありません。",
                  content:
                    "今、レシピは時々Variusです。非直径ナム製造ロレムセッドスマイルウルトレシー。しかし、枕マイクロ波妊娠局は卒業しました。チョコレートを除くプレイヤーにチリの漫画層が必要です。",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-underscored",
          name: "強調されています",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/drew-hays-tGYrlchfObE-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "さまざまなアルコールドリンクでのチャンピオンシップ。しかし、それは常に局の妊娠中の化粧で笑顔です。非常にマッサージバスケットボールを妊娠しているタンクと執行として飲む。ゲートウェイがソフトな場合のパフォーマンス。さまざまなまたは震えまたはチケットで。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/hush-naidoo-jade-photography-Zp7ebyti3MU-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "ジャスミンの価格は大規模で、製造業のレクトゥス・モーリスのバスケットボールを投資します。パッチが醜い開発者。サラダの生態学的順序でチョコレート漫画モーリスが必要になりました。ターゲットを絞ったプロテインバスケットボールですが。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/mathurin-napoly-matnapo-ejWJ3a92FEs-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "時間ターゲットポットプール。学部生の履歴書として控除可能でない限り、要素。または、ライオンよりも選ばれた人の価格。サッカーのスコアのために明日の学部生がたくさん必要です.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/national-cancer-institute-KrsoedfRAf4-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "矢印いくつかのマレスアダはアルコール寿命を飲みます。重要なのは、ライフマイクロ波プレイヤーCNNです。大衆不動産の宿題の子供たちのパフォーマンス。開発者は常にを紹介していませんが、バレーボールの履歴書もありません。航空会社の大量がないか。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-what-to-eat",
          name: "何を食べれば良いか",
          articles: [
            {
              class: "columns-wrap",
              header: "低炭水化物",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/kenny-eliason-5ddH9Y2accI-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "酵母は鍋に置かれませんでした。震えがないのは嫌いです。これは、著者開発者であっても、超整備です。サッカーの明日の開発者はプロパガンダに住んでいます。",
                },
                {
                  image: {
                    src: "assets/images/brigitte-tohm-iIupxcq-yH4-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "したいのですが、時間はです。は、補給開発者の栄養住民を強化しました。利便性をキャンセルしますが、執行執行生態学的専門家。",
                },
                {
                  image: {
                    src: "assets/images/brooke-lark-oaz0raysASk-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "栄養供与者。トマトではありませんでした。航空会社がないよりも直径として。チリのリレーと温度。チョコレートは病気に沿っています。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "ベジタリアン",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/christina-rumpf-gUU4MF87Ipw-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "ただし、この十分の一通りの漫画地域。楽しいこともあれば。",
                },
                {
                  image: {
                    src: "assets/images/nathan-dumlao-bRdRUUtbxO0-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "明日の谷は常に著者または生涯です。バスケットボールで笑いを走らせて、私の時間の資金調達no。",
                },
                {
                  image: {
                    src: "assets/images/maddi-bazzocco-qKbHvzXb85A-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "エンティティのバレーボール要素の喉に、または週末に。空腹と醜いバニーですが、時間はurです。",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "朝食",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/brooke-lark-IDTEXXXfS44-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "バイヤー最大の移民とバスケットボール。便利なクマの時点で期待して。サラダゼロQuiver直径へ。",
                },
                {
                  image: {
                    src: "assets/images/joseph-gonzalez-QaGDmf5tMiE-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "が醜い開発者ID調査の笑顔。直径はありません。私は妊娠したアークと温度サッカーのコンバリスジャスミンをチャットしません。",
                },
                {
                  image: {
                    src: "assets/images/brooke-lark-GJMlSBS0FhU-unsplash_150.jpg",
                    alt: "プレースホルダー",
                    width: "150",
                    height: "84",
                  },
                  text: "最新のものにも委託がありました。臨床の価格は、マイクの価格漫画を作ってください。",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-hot-topics",
          name: "ホットな話題",
          articles: [
            {
              class: "columns-2-balanced",
              header: "これは最初です",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/national-cancer-institute-cw2Zn2ZQ9YQ-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "プロパガンダはさまざまなものでアルコール飲み物をベッドします。コース醜い大衆開発者DUI。Feugiatチョコレートは病気に沿っています。または臨床ゲートは枕ではありません。それはあなたが憎しみの弓を飾りたいと願うことです。必須の航空会社には、痛みの病気や笑顔の弧が必要です。サッカーのハロウィーン、私の飲み物またはバニー。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/national-cancer-institute-GcrSgHDrniY-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "新しい" } },
                  text: "震えの質量。ジャスミンを嫌いますが、顧客まで学部の直径を嫌います。今はソフトですが、それは常に局の笑顔です。今、それはフットボール選手の直径のケア時間です。トマトの学部開発者の宿題タンクの多く。",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "この秒",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/national-cancer-institute-SMxzEaidR20-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "そして、人生の開発者は常に人のベッドです。どちらでもない大衆の航空会社。しかし、サッカー、私の飲み物、または執行宿題の喉は醜いです。開発者ID調査では、恐怖の前でSmile feugiat。ビューロー妊娠中のメイクアップそれぞれまたはシリアルプロパガンダ。プロパガンダは、アークがさまざまな宿題をする必要があります。",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/national-cancer-institute-L7en7Lb-Ovc-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "速報" } },
                  text: "または支払った選択。ニンジンが強化されました。非直径ナム製造ロレムセッド笑い。門脈プールの嘆き層温度。サッカーは現在、子供のニンジン航空会社の喉です。または醜い今はロレムが必要です。予約済みマイクロ波無料。",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-paid-content",
          name: "有料コンテンツ",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/dom-hill-nimElTcTNyY-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "フットボールサラダ素晴らしいウォームアップワークの人生。は大きく、製造業を卒業しました。必須の利便性リレーなし。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/sarah-dorweiler-gUPiTDBdRe4-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "ヌラム車は、学校のコースからの訪問時の車両。地域の要素のために、生命とライオンの宿題の矢印。栄養開発者のニーズはありません。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/icons8-team-k5fUTay0ghw-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "開発者、および。最大の一部のマスカラニブで。サッカーだが妊娠中のチリ。エロスのプレイヤーでは、サピエンのヘアスタイルですが、恐怖のために時間を嘆きます。",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/jessica-weiller-So4eFi-d1nc-unsplash_336.jpg",
                    alt: "プレースホルダー",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "それは資金調達でしたが、パフォーマンスでしたが、ゲートでした。ヌラムと唐辛子の温度。臨床IDの価格。質量は、地域要素にとって現実的ではありません。",
                },
              ],
            },
          ],
        },
      ],
    },
  },
  U_ = {
    home: {
      name: "الصفحة الأمامية",
      url: "/",
      priority: 0,
      notification: {
        name: "cookies",
        title: "هذاالموقعيستخدمالكوكيز 🍪",
        description:
          "نحن نستخدم ملفات تعريف الارتباط لتحسين تجربتك على موقعنا ولإظهار المحتوى الأكثر ملاءمة لك. لمعرفة المزيد ، يرجى قراءة سياسة الخصوصية وسياسة ملفات تعريف الارتباط الخاصة بنا.",
        actions: [
          { name: "يلغي", priority: "secondary", type: "reject" },
          { name: "يقبل", priority: "primary", type: "accept" },
        ],
      },
      sections: [
        {
          id: "content-frontpage-breaking-news",
          name: "أخبار عاجلة",
          articles: [
            {
              class: "columns-3-narrow",
              header: "غير خاضعة للرقابة",
              url: "#",
              image: {
                src: "assets/images/isai-ramos-Sp70YIWtuM8-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "الاستهداف الآن ، موز الحياة الحلق للغاية.",
              type: "text",
              content: `الرجاء أعضاء كرة القدم كقوة كرة قدم.لكن الوقت هو جرة وجع.مجموعة غير من الفلفل الحار.عنصر الحياة الآن ولكن أعضاء كرة القدم في الهواء الطلق.في أي رعاية لسهم سريري.آخر ما تريد دعاية مطوري كرة القدم البيئية السلطة في بعض الأحيان.الاستهداف الآن ، موز الحياة الحلق للغاية أو .الآن مسح الحياة في الحلق.

بشكل السفن ما يتم. جدول الإمداد أن جُل, هو دون اتفاقية باستحداث الدولارات. فقد في وشعار الأمريكي. اكتوبر بتطويق ليرتفع الى قد, غير بالرغم أفريقيا إستيلاء في. وحرمان باستخدام و لها, أن يعبأ بمباركة ولم, ان ومضى الجوي تحرّك حول.`,
            },
            {
              class: "columns-3-wide",
              header: "المزيد من القصص العليا",
              url: "#",
              image: {
                src: "assets/images/nasa-dCgbRAQmTQA-unsplash_684.jpg",
                alt: "عنصر نائب",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "الصورة التي التقطها شخص ما.",
                tag: { type: "breaking", label: "كسر" },
              },
              title:
                "فقط بحاجة إلى كرة قدم رائعة لأعمال الاحماء ، ولا تتغذى على القطر.",
              type: "text",
              content: `لان وتنصيب والفلبين التبرعات إذ. المواد وهولندا، إذ كلّ, دار السيطرة والكساد لم. عدد لم اللا الثالث استعملت, ذات من ويتّفق معاملة. قائمة معارضة قبل ما. قد الجو وهولندا، فقد. أخذ إذ كانتا وبالرغم, يبق هو مرجع ليركز ويكيبيديا،. جُل الفترة الأرواح ثم, بشكل وصغار غير بل, قام عل وباءت المسرح الجنوبي.

تعد وجهان ميناء غينيا مع, و تجهيز وقدّموا فقد. المسرح الأرواح إذ ضرب, و تلك تسمّى وقامت الأهداف. أسر ثم وسمّيت وتتحمّل الانجليزية. بل قدما حالية مكن, الآلاف المتحدة مع به،. فكانت يعادل أن بلا, تكبّد لتقليعة بريطانيا-فرنسا كان قد.`,
            },
            {
              class: "columns-3-narrow",
              header: "الجريمة والعدالة",
              url: "#",
              image: {
                src: "assets/images/jordhan-madec-AD5ylD2T0UY-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "سلطة كرة القدم حياة مجرد عملية الاحماء العظيمة.",
              type: "text",
              content: `هذا و كانتا مليارات الفرنسي, وبداية وبعدما فعل و. مليون وحرمان مما لم, الا مع تكبّد سبتمبر, جنوب قِبل إجلاء بـ عرض. إبّان مكثّفة واقتصار بعد أن, و ذلك غينيا تكتيكاً. ان أسر أفاق تعداد المسرح, و ببعض لتقليعة لان. في جُل بحشد الدّفاع, بل لهيمنة وتتحمّل بحث. عل وشعار أمدها قتيل، بين, تحت ان أجزاء وقامت المتحدة, دار إختار الأول أي. بـ أدنى وسمّيت الفرنسية شيء.

ولم واندونيسيا، تشيكوسلوفاكيا تم, ما ولم إنطلاق العالم الأثنان, وصل عالمية مشاركة الأوروبية، و. أي دأبوا الطريق الأثناء، حين. وانهاء مساعدة ومن ان, قبل و معارضة ألمانيا. به، مئات وباءت الربيع، في, دون اتّجة كُلفة الجنوب تم. أن هذه عليها اكتوبر العالمي. بحشد فبعد الدمج يبق من, ٣٠ أكثر ولاتّساع كان.`,
            },
          ],
        },
        {
          id: "content-frontpage-latest-news",
          name: "أحدث الأخبار",
          articles: [
            {
              class: "columns-3-balanced",
              header: "يحدث الآن",
              type: "articles-list",
              content: [
                {
                  title: "لوريم الجزر جدا.",
                  content:
                    "كانتا فرنسية استبدال أم فصل, أخر إذ الأراضي باستحداث. تلك أن استبدال لبلجيكا،, كل لكل وسوء الوزراء, وقام وجهان مساعدة وصل كل. كل فكان سياسة أساسي بعض, حيث أم الأخذ الشمل العسكري, ولم في هناك الآلاف. الا السبب الصفحات أم, لم خلاف ٢٠٠٤ عرض. للمجهود الأوربيين بريطانيا-فرنسا بحث قد, حتى لغزو اليها الإقتصادية إذ, وبغطاء الضروري استبدال إذ لكل. و دول دفّة الحرة للجزر, ضرب غرّة، المبرمة ثم, بالرّد البرية الأعمال ٣٠ حيث.",
                },
                {
                  title: "إجراءات المراقبة المحسنة.",
                  content:
                    "تحت وبداية عالمية مسؤولية بل, بشرية نتيجة البولندي عن به،, ومضى الإنزال أسر بل. والكوري الخارجية ما أخذ, ٣٠ للصين أفريقيا ذلك. لكل تم وحتّى بلديهما. بالعمل بتطويق باستحداث أسر إذ. أضف عل الإثنان والفلبين, لدحر بتحدّي وفنلندا وتم ما, والنفيس العمليات الإكتفاء ثم جعل.",
                },
                {
                  title: "لكنني سأكون في.",
                  content:
                    "ماذا باستخدام لم أما, مع لمّ بقسوة المشتّتون. يكن أن وعُرفت والإتحاد. ولاتّساع الرئيسية ما ذلك. كلا ترتيب لعملة لم, الضغوط محاولات استراليا، هو ضرب. الأسيوي, محاولات الباهضة واندونيسيا، مع جعل. عن بعد فقامت الخاسر التخطيط. يكن بهيئة أسابيع ولاتّساع",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "جدير بالملاحظة",
              image: {
                src: "assets/images/peter-lawrence-rXZa4ufjoGw-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "الدعاية أو الحامل في الاحماء والرعاية والسريرية.",
              type: "list",
              content: [
                { content: "معقل الواقعة عن غير, قد ومن صفحة تسمّى الرئيسية." },
                {
                  content:
                    "هو. بعض بقعة قدما عل, جُل دفّة بتخصيص قد. انه تزامناً لتقليعة ٣٠, إذ أثره، أراضي بالجانب فعل, إذ قام الذود مليون البرية.",
                },
                {
                  content:
                    "عض بقعة قدما عل, جُل دفّة بتخصيص قد. انه تزامناً لتقليعة ٣٠, إذ أثره، أراضي بالجانب فعل, إذ قام الذود مليون",
                },
                {
                  content:
                    "وتم و بزمام المتّبعة الدولارات, ان أسابيع الثالث، الثقيلة تلك, دول ٣٠ وجزر مشارف الآلاف. شاسعة النفط ولم ٣٠, مما عل رئيس مسؤولية وقدّموا. أن قائمة",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "حول العالم",
              image: {
                src: "assets/images/rufinochka-XonjCOZZN_w-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "الآن فيليس الأرض ، كرة السلة تحتاج إلى كتلة كبيرة ، كارتون لوريه لوريم.",
              type: "list",
              content: [
                {
                  content:
                    "الخاصّة حدى, أم شاسعة غرّة، لها. بل وشعار مواقعها أضف, وشعار فرنسية المسرح حين بـ. الى الوزراء الشّعبين كل. بعض هاربر بتخصيص بمباركة بل, عل جُل وقبل قدما مسارح",
                },
                {
                  content:
                    "أراض بالحرب الحيلولة بعض ثم, هو أسيا ضمنها فعل. قتيل، والنفيس غير بـ, هامش شواطيء الإتحاد ثم حدى, أي قبل هناك الأولى.",
                },
                {
                  content:
                    "هو الفترة وفرنسا لليابان بحق. الآخر الإنزال ولاتّساع ثم حتى. أخر ما بسبب تصرّف مشاركة, كل حقول بوابة طوكيو ضرب, مرمى وزارة السبب دنو ثم. يعبأ القوى الاندونيسية لم يبق,",
                },
                {
                  content:
                    "بـ أخر غينيا بتخصيص, كلا هو إبّان كنقطة  أن. ما بعض يقوم العالمي المنتصر. واُسدل ولكسمبورغ بـ ضرب, تم الا يعبأ السيطرة, بالرّد محاولات جهة أم.",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-latest-media",
          name: "أحدث الوسائط",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/steven-van-bTPP3jBnOb8-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-WUehAgqO5hE-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/david-everett-strickler-igCBFrMd11I-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/marco-oriolesi-wqLGlhjr6Og-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-highlights",
          name: "يسلط الضوء",
          articles: [
            {
              class: "columns-wrap",
              header: "النقاط البارزة المحلية",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/samuel-schroth-hyPt63Df3Dw-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "يبق عن قدما حاملات, وفي عن هناك اعتداء والفلبين. كل تزامناً الجنوبي العالمية عدد, أعلنت بالمحور في دار. وبالرغم أفريقيا ان بال. أمام البولندي تشيكوسلوفاكيا ذلك هو. أساسي وصافرات إيو لم. وصل كردة وصافرات ما.",
                },
                {
                  image: {
                    src: "assets/images/denys-nevozhai-7nrsVjvALnA-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بـ مسارح والفلبين التقليدية وتم, لدحر الشرقية الوزراء عدد ثم. كردة وإيطالي الصفحات بل حيث, بـ بحث بقصف مسرح أخرى. مدن الله وأكثرها ومطالبة بل, زهاء سقوط الأولية قد وصل, يقوم الصفحة مكن ٣٠. جديدة اكتوبر ان لها, تم الا بقصف لكون. قد اوروبا الخارجية عرض. بين كل وترك أمّا وسمّيت.",
                },
                {
                  image: {
                    src: "assets/images/mattia-bericchia-xkD79yf4tb8-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "لكل كُلفة حادثة كل, ما أراض حادثة التنازلي به،. الدمج الساحة تم مما, بشرية إعادة قد الى, به، جمعت بمباركة ما. وفي في مارد تمهيد, كلا تم حلّت أمام مدينة, جعل و الأرواح واشتدّت. من وعلى الصفحات حيث. فعل وصغار الشرقية المزيفة أن.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "النقاط البارزة العالمية",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maximilian-bungart-nwqfl_HtJjk-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "كلّ أمّا الجو الصينية بل. إذ كما جسيمة اليابان، المتساقطة،, حين أملاً يتمكن ثم, لم تحت ميناء بالرغم. عرض للسيطرة لإنعدام من. ان القوى انتباه الإحتفاظ حيث, لان تم احداث الفترة الخاطفة, أوزار اوروبا والديون أضف ان. في منتصف والفلبين جُل, هذا عل الآخر بالحرب, بقعة مارد وسمّيت أضف ثم. مدينة المتاخمة بريطانيا-فرنسا تحت بل, تم كما بالإنزال بالولايات.",
                },
                {
                  image: {
                    src: "assets/images/gaku-suyama-VyiLZUcdJv0-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "كان إحتار اليها السادس و, تم هذا بوابة التقليدية, الحيلولة المتّبعة الولايات ٣٠ دول. يبق جنوب وقرى والحزب أن, ٣٠ الله دأبوا الضروري عدم, وتم رجوعهم وسمّيت المتساقطة، ٣٠. عل لمّ الدول بتحدّي, عجّل أعلنت ابتدعها تم يتم. ما الأخذ المتساقطة، كما. جنوب أكثر انه هو, عل وبعد وسوء يكن.",
                },
                {
                  image: {
                    src: "assets/images/paul-bill-HLuPjCa6IYw-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "فعل أي كثيرة والروسية, عن خطّة إحكام فرنسية وصل. هذه ٣٠ إعمار إبّان الانجليزية. مسرح عملية حول أم, كل قِبل سياسة مدينة لكل. تم يبق حقول تاريخ للجزر. أن كانت الأوروبية، بعض, عل بعض هامش لفشل بشرية, غينيا الأحمر السادس أي حدى. إذ وتم ثانية تجهيز, ٠٨٠٤ بقسوة عالمية حين في, بالحرب بأضرار مع كما.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "أبرز الأحداث المحلية",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maarten-van-den-heuvel-gZXx8lKAb7Y-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ان لليابان الحكومة يبق. كل أثره، المضي الأهداف كلّ, أما تم غريمه الشمل. أم أهّل اوروبا شموليةً نفس, للصين ارتكبها بال تم. من كان المسرح يتعلّق, العالمي اتفاقية يكن تم. حيث عملية النفط الحدود لم. وأزيز وتزويده أي فعل, ٣٠ ومن احداث يتبقّ الخطّة. الجو تنفّس المتحدة في قام, حول قد سقوط للسيطرة والنرويج.",
                },
                {
                  image: {
                    src: "assets/images/quino-al-KydWCDJe9s0-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "قبل للمجهود بمباركة ٣٠, ثم ولم الذود الساحة إستيلاء. عدد و بالعمل وبالرغم الكونجرس, هذه المحيط استطاعوا التقليدية ان. دار الشهير وفرنسا بالولايات في. ثم كلا كردة فقامت الفرنسي, تم نفس الأمم الأولى الساحل. تكبّد الصينية وقد بل, ثم إحتار إستعمل التخطيط جهة, ذات أن تنفّس فكانت الشتاء،. الوراء الشرقية ماليزيا، قد فقد, يعبأ مشاركة الإنزال أن بعد.",
                },
                {
                  image: {
                    src: "assets/images/mathurin-napoly-matnapo-pIJ34ZrZEEw-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "جهة التنازلي المتساقطة، ان, به، حقول ويتّفق معارضة إذ. أم كلا الخاطفة الشهيرة الأولية. أوروبا المجتمع وقد مع, تاريخ العالمية المشتّتون ما وتم, فسقط شدّت ان دول. يذكر تكبّد من قام. شمال دأبوا الكونجرس ٣٠ مدن, لكل مع شمال لدحر دأبوا. هذا و تعديل تجهيز الوزراء, ما يذكر لغزو دون.",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-top-stories",
          name: "أهم الأخبار",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/andrew-solok-LbckXdUVOlY-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "ما عشوائية سنغافورة غير, أي جهة الجنوب ابتدعها, بـ الا بزمام فشكّل يونيو. كل كان أمام وأزيز الحيلولة, دفّة الإمتعاض واندونيسيا، وتم ان. الأرض الحكومة المعاهدات مع أخذ. قد لفشل ابتدعها لان, لم يكن واعتلاء وبلجيكا،. في فعل يطول أسيا وأزيز, ان تعديل وزارة هذه.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/hassan-kibwana-fmXLB_uHIh4-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "عجّل أراض الواقعة عدد في, طوكيو الأراضي التخطيط الا إذ. قبل ان ليركز مقاطعة. عدم اتّجة الأجل معزّزة ثم. نهاية الإتحاد ومن بـ, تحت أم الشرق، الأثنان الإمتعاض. أم مشروط بمعارضة والنرويج بحق, ان المضي بمعارضة حين. و أمام العظمى عدم, عل فصل نقطة حكومة, قامت تطوير الساحلية كل تلك.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/craig-manners-LvJCFOW3Ma8-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "تم خلاف الأسيوي أسر, ضرب ووصف علاقة استبدال عل, وترك إعلان ما أما. التكاليف المشتّتون كل هذا, بعد وباءت الضغوط الثالث قد, و شعار وبريطانيا حدى. فصل ما لفشل جديداً, قد أمام استرجاع بحث. عن مدن تُصب ووصف, تلك أي واُسدل ارتكبها تشيكوسلوفاكيا, ذلك الحرة لمحاكم الأهداف تم. ديسمبر الخاصّة الإثنان أسر أم, يبق ألمّ بالمطالبة أن, تشكيل إتفاقية تلك ثم. و عليها ممثّلة مكثّفة أسر.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/albert-stoynov-fEdf0fig3os-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "عدد لعدم وفرنسا أي, وتزويده الأرضية يتم هو. جورج مسارح يكن ٣٠. نفس أملاً الشرقي بالولايات مع, تصرّف فرنسية مع الى, ثم الا .",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ehimetalor-akhere-unuabona-yS0uBoF4xDo-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "عجّل الشطر النزاع ثم دنو, ومن عل لفرنسا الباهضة. مئات الأجل واشتدّت بحق كل, غضون وعُرفت نفس أي. انه وحتى أوزار والقرى قد. الخاصّة الأوروبية، من بها, مسرح علاقة الرئيسية ان حين, أن العاصمة ويكيبيديا الا.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-international",
          name: "دولي",
          articles: [
            {
              class: "columns-3-balanced",
              header: "أوروبا",
              type: "articles-list",
              content: [
                {
                  title:
                    "يرجى المطور في تمويل الجزر طبقة وثيقة الهوية الوحيدة.أنواع نطاق تصنيع الشارع.",
                  content:
                    "الا ان يقوم المشترك بالسيطرة, ثم تسبب وإقامة تغييرات هذا. كان أوسع يتبقّ الكونجرس أي, أسر وجزر بهيئة الأولى كل. أم العناد يتعلّق الخاصّة وصل, بقصف هاربر عدد أن. أي لعدم انتهت الاندونيسية وقد, يتم هو وبدون أملاً الخاطفة. يطول المشترك أن كما. كما وحتّى وقدّموا الوزراء مع, بـ النفط للإتحاد ومن.",
                },
                {
                  title:
                    "هناك حقيقة مثبتة منذ زمن طويل وهي أن المحتوى المقروء لصفحة ما سيلهي القارئ عن التركيز",
                  content:
                    "ببعض الوراء بـ حول. فصل هو الأول مهمّات محاولات. حتى عل إحكام الوراء, أن حول بلاده الشمل المدن, وقام شاسعة واستمرت ٣٠ قبل. بحث بتطويق بمباركة الشتوية ما, عُقر معزّزة أن حيث. مع ضرب حاول جسيمة علاقة. حدى في تعداد الآخر. أسر تم أعلنت القادة, تم لها معاملة الهادي.",
                },
                {
                  title:
                    "ولكن إذا كانت حمامات كرة القدم ، ولكن البحيرة ، ولكن الكاريكاتير في.",
                  content:
                    "حول عل الجنود الشتاء،, لها عليها بتخصيص الجنرال كل. نتيجة الشمل كل لها. الا بـ وبدون الإنزال بالمطالبة, حول لم هناك الإمداد. تحرّك بالعمل هو بين, عل على وسوء بزمام اليها. ٣٠ جعل تعداد وباءت وقوعها،, نتيجة أعلنت مشاركة بها تم, احداث والديون قام أي.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "أمريكا الجنوبية",
              type: "articles-list",
              content: [
                {
                  title: "كانت دعاية الحاجة القوس متنوعة الواجب المنزلي.",
                  content:
                    "وفي مكّن للسيطرة الإطلاق من. في وصل جسيمة المتحدة التجارية, عن ضرب الإمداد ولكسمبورغ, هو بالفشل والعتاد أضف. فبعد وعُرفت إنطلاق في ومن. وترك وبولندا بعض مع, للسيطرة العسكري الأوروبي مدن أن. بـ الجنود الأثناء، لبولندا، كان, مكن عن تحرّكت استطاعوا.",
                },
                {
                  title:
                    "طريقة لوريم إيبسوم لأنها تعطي توزيعاَ طبيعياَ -إلى حد ما- للأحرف عوضاً عن استخدام هنا يوجد",
                  content:
                    "دون ثم بداية حادثة ويكيبيديا. أما لهيمنة استبدال التكاليف عل, أي تحت جدول وبداية اتفاقية. عرض ما تمهيد مسارح وحلفاؤها, ان أثره، فشكّل مدن. صفحة لغات مشارف بل مدن, دون تم الجنود المبرمة. الذود بأضرار الأهداف انه أي, بالعمل وصافرات ومن تم, مئات مشروط بال من.",
                },
                {
                  title: "يشرب أو تريد كل منها.",
                  content:
                    "فعل يونيو انتصارهم في, و أخر تعداد أوزار ليرتفع. لم حتى جسيمة وسمّيت والإتحاد. بفرض الصفحات لم لان, جسيمة السفن يكن كل. أن الفترة بريطانيا مدن.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "آسيا",
              type: "articles-list",
              content: [
                {
                  title:
                    "قد يكون الأعضاء أيضًا لاعبيك الفلفل الحار.على سبيل المثال ، ولكن حلق القبيح في كرة القدم ، مشروب بلدي ، ولا.",
                  content:
                    "الشمال سليمان، ان دول, قد ومضى وقبل عشوائية لمّ. البرية والعتاد وصل من, أن شيء وبدأت حالية. أم كما واحدة اتفاقية, أي بحق أراضي واقتصار. ثم وجزر تكاليف عشوائية الى, عل ولم سكان قائمة. ان حين رئيس مرمى وترك. نفس إعلان واتّجه سنغافورة ٣٠, المبرمة ماليزيا، قد إيو. بـ وقد وجزر قادة لبولندا،, تحت عل الثقيل الربيع،.",
                },
                {
                  title: "وادي هاريترا لتعيين وعاء الأسد الذي يرجع تاريخه.",
                  content:
                    "حتى لم جورج عليها استدعى, جديدة وباءت الساحة ان كما, يبق عن فبعد والنفيس الاندونيسية. يكن بـ يتبقّ تغييرات, عدد بسبب علاقة الدّفاع إذ. أن رجوعهم اسبوعين جعل, أكثر مقاومة في بال. و مساعدة وتزويده العالمية لان, بحث خيار مساعدة عل, من بها بالفشل وإعلان مواقعها. سقطت مساعدة بين أي, أعلنت استرجاع بعد ثم.",
                },
                {
                  title: "في مختلف أو جعبة أو قبيح بحاجة الآن إلى ألم لوريم.",
                  content:
                    "رجوعهم الجنوب أي بحث. بل مرجع وصغار شيء, بسبب جديدة دول بل. ان لإعادة المجتمع وايرلندا بال, مكن و يعبأ محاولات ومطالبة. جهة أم مايو سياسة الدمج. حول سبتمبر وبغطاء ومحاولة بل, فعل أي بلديهما لتقليعة للأراضي.",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-featured",
          name: "متميز",
          articles: [
            {
              class: "columns-3-balanced",
              header: "واشنطن",
              image: {
                src: "assets/images/heidi-kaden-L_U4jhwZ6hY-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "عشوائي أخذتها من نص، لتكوّن كتيّب بمثابة دليل أو مرجع شكلي لهذه الأحرف. خمسة قرون من",
              type: "list",
              display: "bullets",
              content: [
                { content: "تدليك كرة القدم الجذاب في الجبهة.", url: "#" },
                {
                  content: "اضبط الجري القبيح في هذا الشارع العشور.",
                  url: "#",
                },
                {
                  content:
                    "وفي مكّن للسيطرة الإطلاق من. في وصل جسيمة المتحدة التجارية, عن",
                  url: "#",
                },
                {
                  content:
                    "الأداء على مجموعة الموجات فوق الصوتية للواجبات العقارية التغذوية حتى الآن.",
                  url: "#",
                },
                { content: "من تصحيح فكي التدليك في بعض الأحيان.", url: "#" },
                {
                  content: "الكثير من الضعف من أجل عدم وجود ضغوط طيران الموز.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "نيويورك",
              image: {
                src: "assets/images/hannah-busing-0V6DmTuJaIk-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "الزمن لم تقضي على هذا النص، بل انه حتى صار مستخدماً وبشكله الأصلي في الطباعة والتنضيد",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "هذه دائمًا ابتسامة في المكياج حامل المكتب ليس الجميع كذلك.",
                  url: "#",
                },
                {
                  content:
                    "وعُرفت إنطلاق في ومن. وترك وبولندا بعض مع, للسيطرة العسكر",
                  url: "#",
                },
                {
                  content:
                    "دون ثم بداية حادثة ويكيبيديا. أما لهيمنة استبدال التكاليف عل, أي تحت ",
                  url: "#",
                },
                {
                  content:
                    "فعل يونيو انتصارهم في, و أخر تعداد أوزار ليرتفع. لم حتى جسي",
                  url: "#",
                },
                {
                  content:
                    "الشمال سليمان، ان دول, قد ومضى وقبل عشوائية لمّ. البرية والعتاد وصل",
                  url: "#",
                },
                { content: "الكثير من الكتلة من صلصة ماكرو الحياة.", url: "#" },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "الملائكة",
              image: {
                src: "assets/images/martin-jernberg-jVNWCFwdjZU-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "يولد المورس لتفوت الماوس.",
              type: "list",
              display: "bullets",
              content: [
                { content: "ماتيس هو السخرية من الأسهم العناصر.", url: "#" },
                {
                  content:
                    "الجزر المعقمة وعاء الجري تحتاج الآن كرتون الشوكولاتة موريس.",
                  url: "#",
                },
                { content: "حتى لم جورج عليها استدعى, جديدة و", url: "#" },
                { content: "الآن كرتون الشوكولاتة موريس في بعض.", url: "#" },
                {
                  content:
                    "تريد التمويل ولكن الأداء ولكن بوابة لوريم لينة بعض.",
                  url: "#",
                },
                {
                  content:
                    "المبرمة ماليزيا، قد إيو. بـ وقد وجزر قادة لبولندا،, تحت عل الثقيل الربيع،.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-underscored",
          name: "يؤكد",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/kevin-wang-t7vEVxwGGm0-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "تصفح يقوم الشهير عدد و. و وتم بوابة تطوير. إيو قد لعدم يعادل, ذلك لدحر وتنامت كل, قام وسوء أملاً المشتّتون في. بعد يذكر نتيجة اليابان، كل, أحدث أطراف البولندي كان أن. ذات لهيمنة الحدود و, أسابيع الحدود اليابانية ان حتى. دون مارد أجزاء المؤلّفة ٣٠.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/claudio-schwarz-3cWxxW2ggKE-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "وتم بقيادة الواقعة باستحداث ما, أي انه العدّ ممثّلة استدعى. إيو ان وقرى لغزو, فكانت الشطر شموليةً يكن لم. دفّة الأخذ جهة ٣٠, مع بسبب بالفشل حين. أن والحزب والمانيا ضرب, لدحر التي إذ أخر, بين لم وقرى بهيئة. وجزر بالفشل الأوروبيّون قام أي, بل والتي التخطيط تحت.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/braden-collum-9HI8UJMSdZA-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "وعلى أفاق مهمّات من أما. أخذ ضمنها انتهت أفريقيا ان. في ا استمرار الدنمارك ذات, ما بينما معزّزة الا. قِبل وباءت والعتاد ان فعل, ٣٠ ومن وقبل بتخصيص. على حصدت ليبين تكاليف أم, بلا من حكومة الشرق،, لان لإعادة الثقيلة في. بين وبعض حكومة إعادة من, جنوب النفط ٣٠ على.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/geoff-scott-8lUTnkZXZSA-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "وتم عُقر النفط الرئيسية عن, ضمنها ولكسمبورغ وبريطانيا ثم مما. ولم أم أكثر وبالتحديد،, عرض الحرة الطرفين اتفاقية إذ, لغزو التجارية التقليدية عل جعل. أي مدن غينيا وسمّيت, إبّان فرنسا أي فقد, على مع سليمان، والإتحاد بريطانيا-فرنسا. أي الإثنان العمليات جُل, ان سابق معاملة حيث. سكان تشكيل بقيادة هو دنو.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-happening-now",
          name: "يحدث الآن",
          articles: [
            {
              class: "columns-wrap",
              header: "سياسي",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/jonathan-simcoe-S9J1HqoL9ns-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "كنقطة الانجليزية كل حين, ٣٠ الجنوبي ايطاليا، التغييرات انه, عل هذه إنطلاق البرية. هُزم الفرنسي الأمريكية ٣٠ عدد, دون ما منتصف أوروبا بالإنزال. ٣٠ تحرّك الفرنسي فصل, بل كما ا البولندي. نهاية بالمحور حدى إذ, إذ أوسع تجهيز الشمال ذات, قد بلا الحكومة الواقعة الكونجرس. أم فقد وبدون المحيط. ومن بل قدما الشرق، الأمريكي, ٣٠ أواخر تشكيل وانهاء على.",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-p2Xor4Lbrrk-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "كل يعبأ بالرّد الولايات فعل, بلاده وباستثناء الإتفاقية قد على, أحكم لإعادة في تحت. جسيمة جديداً بالسيطرة ثم كان, بل يقوم وجهان واستمر وصل. ووصف واعتلاء و حيث, فقامت المحيط لتقليعة كان بـ. قدما جديداً إيطاليا بلا إذ, قد بأيدي الشرقي أسر. حول تم تحرير الثقيلة. تصرّف شاسعة مع عدم, أم العاصمة الشرقية المعاهدات به،.",
                },
                {
                  image: {
                    src: "assets/images/marius-oprea-ySA9uj7zSmw-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ذات لم فكان فقامت والروسية. لمّ هُزم انتباه بل. دون مع لعدم ويتّفق المواد. كرسي اتّجة أطراف كلا من, بشرية وايرلندا كان ٣٠. ٣٠ وسفن قُدُماً الإيطالية بين, كل النفط يعادل هذا.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "صحة",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/jannis-brandt-mmsQUgMLqUo-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ما جُل هنا؟ وأكثرها, عن انه جزيرتي بالمطالبة. الله حقول كان في, بـ إيو ألمّ أخرى والمعدات, أخر بل تحرّك الضغوط والمانيا. ما سقوط نتيجة ويتّفق ومن. وفي ودول الواقعة ان, ٣٠ ذلك يذكر المتساقطة،, نفس و بقيادة بالحرب. بحث الأخذ وانهاء في, بالحرب وإيطالي لكل من.",
                },
                {
                  image: {
                    src: "assets/images/martha-dominguez-de-gouveia-k-NnVZ-z26w-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "فكانت اعلان وإقامة قد وصل, ان يذكر قبضتهم والفلبين مكن. هو فقد وكسبت العصبة استبدال. أن الطرفين استراليا، انه. كانتا اكتوبر ان قبل, بقيادة وبولندا بريطانيا-فرنسا ما بين, عن أما لهيمنة والمانيا ماليزيا،. مع يبق بفرض وتنامت واستمرت, مع سياسة جديداً ذلك.",
                },
                {
                  image: {
                    src: "assets/images/freestocks-nss2eRzQwgw-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "هامش البرية الساحلية دنو في, مع شاسعة تعديل ومطالبة على. أي تمهيد عليها والكساد دول, أم ذات رئيس وصافرات. دار مرجع الأوروبية، قد, أملاً واُسدل حاملات أم بحث. من بشرية قُدُماً جُل, بها كل سياسة فقامت وانهاء. الستار للحكومة عل مدن, مشروط كُلفة المسرح من يكن, تحت بـ الجو وتتحمّل لبلجيكا،. غضون لفشل تحرّك ما جعل. حيث في قررت وباءت, قبل ثم الآخر بالفشل معزّزة, قد ترتيب استراليا، مكن.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "عمل",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/little-plant-TZw891-oMio-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "حيث عل وقرى بمباركة. لم بعض أمام ولاتّساع, العناد بمعارضة الإمتعاض تلك ما. ثم تمهيد كثيرة الشّعبين ذات, بـ كرسي أصقاع واتّجه لان, بقعة كانت الحرة كان و. مدن وزارة ميناء بمعارضة مع, جديداً تحرّكت باستخدام قد يتم.",
                },
                {
                  image: {
                    src: "assets/images/allan-wadsworth-Lp78NT-mf9o-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بها لم تونس جديدة الدّفاع, تم وصل مارد إجلاء اعلان. يبق في هامش المؤلّفة الدولارات, أم انه مليارات ولاتّساع الكونجرس. ترتيب الأخذ ممثّلة أسر ثم, الشمل الجنود عن كلا. وفنلندا الإثنان الجنوبي ثم حيث. وبعد لليابان ثم بعد. لم يكن الساحل التكاليف, حدى أجزاء الحيلولة عن, وأزيز مقاطعة الفرنسية كلا و. يعادل وإعلان واقتصار دول و.",
                },
                {
                  image: {
                    src: "assets/images/ant-rozetsky-SLIFI67jv5k-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "سقوط إحكام وبولندا أم جعل, جُل بـ فسقط تطوير سليمان،, وقرى الأجل الأوضاع ذلك ثم. أوسع الدنمارك ان أخر. العالمي مواقعها بال إذ, أما المحيط الاندونيسية هو. بل أخذ أواخر الآلاف القادة, أم استرجاع والنرويج غير, استدعى العالم، الأوربيين قد شيء. وجهان الدول وإعلان أما أم, ان رئيس مسارح اعلان يتم. تلك تُصب والنفيس هو, ماشاء الإثنان المتساقطة، عن شيء.",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-hot-topics",
          name: "مواضيع مثيرة",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/alexandre-debieve-FO7JIlwjOtU-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "مع يبق المضي العالمية, كلّ هو جورج الآخر الغالي. انه ومضى القوى أي. كل غريمه بالمطالبة دول, بـ كلا قررت وعُرفت قُدُماً. نتيجة الشهيرة والفرنسي عدد كل, عدم ان وجزر وبدأت. بزمام الحيلولة بـ عرض, الى عُقر أفاق بالجانب هو, مكن أي تصفح التحالف. بال منتصف المؤلّفة قد, وقد وحلفاؤها المشتّتون أي.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/thisisengineering-ZPeXrWxOjRQ-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "أي حدى بزمام العالم, المسرح الأبرياء في لها. ٣٠ ولم بخطوط تشكيل اعتداء, أم الوزراء التقليدي التقليدية دون. من بسبب العالمي كلا, ان يونيو التقليدي حول. كان أسيا وباءت واُسدل إذ, قتيل، شواطيء وتم من.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/isaw-company-Oqv_bQbZgS8-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "دنو صفحة أصقاع والكساد بل. بها ليبين بالولايات من, تعد أملاً وهولندا، ويكيبيديا، ان. كل أضف قِبل وبحلول استبدال, في الوراء للمجهود تلك. وبعض اعلان معاملة حول هو, مسؤولية الثانية في جهة. بها الوراء التّحول الأوروبية، في. الإنزال التّحول جُل من, مكّن مليون العدّ ٣٠ لان.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/aditya-chinchure-ZhQCZjr9fHo-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "دون قد بخطوط الشهير, قد يقوم الهادي والروسية جهة, أدنى الإطلاق الانجليزية عن تحت. بعد مسؤولية ارتكبها ان, لها العصبة المشتّتون بـ, أخرى وتزويده جُل ما. كل سقطت الصفحة وفنلندا إيو, حصدت اعلان وتنامت أم لمّ. وقد بحشد سقوط عليها في, عل تطوير للجزر المشترك فصل. ذات ٣٠ السفن اسبوعين.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-frontpage-paid-content",
          name: "المحتوى المدفوع",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/tamara-bellis-IwVRO3TLjLc-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الآن يشرب الموز لمضادات الأكسدة الحوامل أو.ولا  في الخميرة وضعت على الوعاء.م",
                },
                {
                  image: {
                    src: "assets/images/david-lezcano-NfZiOJzZgcg-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "تحتاج إلى كرة قدم ذكية أو قوس ابتسامة أي شخص.لا الدردشة القوس الحامل وأصول درجة الحرارة.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/heidi-fin-2TLREZi7BUg-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "من رقعة من فكي التدليك في بعض الأحيان.مجموعة التغذية الجماعية للعقارات التجمعات فوق الصوتيات حتى الآن.وادي الغد هو دائما المؤلف أو فترة الحياة.لارتجال الكثير من بعض المعرف.",
                },
                {
                  image: {
                    src: "assets/images/joshua-rawson-harris-YNaSz-E7Qss-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "البيئي لا يحتاج إلى قطر كرة القدم حتى.كرة القد وصفة شبكة.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/arturo-rey-5yP83RhaFGA-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "أو حامل.لا يوجد مركبة من موبيليسي نولام نفسها من مسار طيران أو بعض كرة القدم.أي شخص هو شوكولاتة كرتون.",
                },
                {
                  image: {
                    src: "assets/images/clem-onojeghuo-RLJnH4Mt9A0-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "هنالك العديد من الأنواع المتوفرة لنصوص لوريم إيبسوم، ولكن الغالبية تم تعديلها بشكل ما عبر",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/ashim-d-silva-ZmgJiztRHXE-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "عليك أن تتحقق أولاً أن ليس هناك أي كلمات أو عبارات محرجة أو غير لائقة مخبأة في هذا النص.",
                },
                {
                  image: {
                    src: "assets/images/toa-heftiba--abWByT3yg4-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "طبقة ناعمة من أسد العبارة من قطر.الآن يشرب الموز لمضادات الأكسدة الحوامل أو الوادي.",
                },
              ],
            },
          ],
        },
      ],
    },
    us: {
      name: "نحن",
      url: "/us",
      priority: 1,
      message: {
        title: "شاهد الأخبار العاجلة!",
        description: "حدث شيء مهم ويجب عليك مشاهدته!",
      },
      sections: [
        {
          id: "content-us-world-news",
          name: "اخبار العالم",
          articles: [
            {
              class: "columns-3-wide",
              header: "يحدث اليوم",
              url: "#",
              image: {
                src: "assets/images/todd-trapani-vS54KomBEJU-unsplash_684.jpg",
                alt: "عنصر نائب",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "الصورة التي التقطها شخص ما.",
                tag: { type: "breaking", label: "كسر" },
              },
              title: "لكن إنفاذ التنفيذ البيئي المغذي الحراري القابل للخصم.",
              type: "text",
              content:
                "من غير حاول والفلبين اليابان،, فعل احداث اتفاقية في. في وصل وبعض عقبت وجهان, قائمة السيء ولكسمبورغ ومن أي, هاربر غرّة، هذه إذ. عل كلا الشمال وفنلندا, نفس كل لعملة التّحول ومحاولة, أوسع الجنوب الخاطفة دون هو. نفس مليون نهاية استعملت إذ, من الجنوبي وهولندا، أخر. جهة قد بالجانب بمحاولة ولاتّساع, أسر تم بوابة النفط وبحلول. فعل وبدأت اوروبا إذ. حقول وبعد وفي ٣٠, للجزر الأبرياء و شيء. دفّة العدّ الضروري دار أم, عل التي مقاومة الجنوبي بال. بـ ولم منتصف بالرّغم, عن أفاق الشرق، بريطانيا-فرنسا بال. مع نفس الأحمر الصعداء, بحث يقوم لهذه بـ.",
            },
            {
              class: "columns-3-narrow",
              header: "الشائع",
              url: "#",
              image: {
                src: "assets/images/mufid-majnun-tJJIGh703I4-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "من أجل تنشيط سيناريو حياة فقط تحتاج إلى شيء عظيم.",
              type: "text",
              content: `هذا ليس تصنيعًا معينًا لكرة القدم يركض ضحكًا في كرة السلة.بطولة وثيقة الهوية الوحيدة لايف الكحول تشرب الشراب كدبابات.فقط حتى الضعف القطر.تدليك كرة السلة حامل للغاية سريري للاستثمار.فالي تيلوس معرف البروتين معرف.

لدحر السيء وهولندا، أم ولم, أما وإيطالي ألمانيا بالسيطرة بـ. ما رئيس الواقعة باستحداث حيث, أن الله قتيل، حول. عل الوراء الجنود أما, تعد قد اللازمة الأولية, أم تعديل والمانيا الإيطالية انه. تم لكل تعديل السيطرة ايطاليا،. لدحر والنفيس بريطانيا مع وتم, أوسع مليون أضف ٣٠, من وحرمان المشتّتون وقد.`,
            },
            {
              class: "columns-3-narrow",
              header: "طقس",
              url: "#",
              image: {
                src: "assets/images/noaa--urO88VoCRE-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "هذا الفلفل الحار الطماطم إلى عنصر الكرة الطائرة الحلق.",
              type: "list",
              content: [
                {
                  content:
                    "سكان التغذية كرة القدم الحزين الشيخوخة.أو الفول السوداني Til ووقت الكراهية Orci بروتين كرة السلة في.",
                },
                {
                  content:
                    "أنا أكره الراحة في نهاية الأسبوع في نهاية الأسبوع ، لكن إنفاذ إنفاذ التنفيذ البيئي.",
                },
                {
                  content:
                    "لدحر السيء وهولندا، أم ولم, أما وإيطالي ألمانيا بالسيطرة بـ. ما رئيس الواقعة باستحداث حيث, أن الله قتيل، حول. عل الوراء الجنود أما, تعد قد اللازمة الأولية, أم تعديل والمانيا الإيطالية انه. تم لكل تعديل السيطرة ايطاليا،. لدحر والنفيس بريطانيا مع وتم, أوسع مليون أضف ٣٠, من وحرمان المشتّتون وقد.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-around-the-nation",
          name: "في جميع أنحاء البلاد",
          articles: [
            {
              class: "columns-3-balanced",
              header: "أحدث",
              image: {
                src: "assets/images/fons-heijnsbroek-vBfEZdpEr-E-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "تم تكوين 1 فقرة، 5 كلمة، 27 بايت من نص لوريم إيبسوم",
              type: "list",
              content: [
                {
                  content:
                    "بال بالعمل جديداً ماليزيا، ان. عن بها مليارات المتحدة",
                },
                { content: "آسف على كرة القدم التي تخرجت." },
                {
                  content:
                    "حين. خطّة بشرية أوراقهم حدى في, ما والتي أعمال العالمية دار. مكن",
                },
                {
                  content:
                    "أنا لا الدردشة الكحول الحامل و.من المهم مضادات الأكسدة الكبيرة حتى وقت كرة القدم السريرية.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "عمل",
              image: {
                src: "assets/images/bram-naus-oqnVnI5ixHg-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "نطاق تصنيع مطور التغذية.للكرتون الشوكولاته الاحماء مع الرقبة.",
              type: "list",
              content: [
                {
                  content:
                    "جُل بشرية إستعمل إذ, بـ جعل خيار لغات تسبب. لم الإمتعاض الإتفاقية وتم, دنو كل",
                },
                {
                  content:
                    "لأوربيين ثم ذات, و تشكيل إعلان أضف. جُل الضروري الخاصّة ان, الإمداد عشو",
                },
                {
                  content:
                    "أوروبا ولاتّساع وحلفاؤها تم جهة, عل جعل وعلى وجهان والنرويج",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "سياسة",
              image: {
                src: "assets/images/hansjorg-keller-CQqyv5uldW4-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "آلام المكتب وظيفية رائعة.كما أنها ليست العقارات الآن هي أن أسهم عنصر المنطقة.",
              type: "list",
              content: [
                {
                  content:
                    "دون قد بخطوط الشهير, قد يقوم الهادي والروسية جهة, أدنى الإطلاق",
                },
                {
                  content:
                    "بـ, أخرى وتزويده جُل ما. كل سقطت الصفحة وفنلندا إيو, حصدت ",
                },
                {
                  content:
                    "قبل من الأمور الطرفين ولكسمبورغ, قبل غريمه أطراف إتفاقية عن, و",
                },
                {
                  content:
                    "لم الإمتعاض الإتفاقية وتم, دنو كل وفنلندا وبالتحديد،, بخطوط وبلجيكا، بين بل. مشا",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-roundup",
          name: "جمع الشمل",
          articles: [
            {
              class: "columns-wrap",
              header: "واشنطن",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/unseen-histories-4kYkKW8v8rY-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "مرجع الأوروبيّون عن ذلك. بل لمّ بزمام تجهيز بمحاولة, يكن قد جيوب إستيلاء, ودول بينما الإطلاق ما حتى. بعد بل اسبوعين الإيطالية الإقتصادية. مما ان فرنسا ديسمبر إستيلاء, و منتصف تحرّكت أخذ.",
                },
                {
                  image: {
                    src: "assets/images/ian-hutchinson-P8rgDtEFn7s-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "قبل من الأمور الطرفين ولكسمبورغ, قبل غريمه أطراف إتفاقية عن, و لمّ وجهان مشاركة. إذ لمّ الهادي بالمطالبة, إذ ٢٠٠٤ كنقطة اليها حين. خطّة بشرية أوراقهم حدى في, ما والتي أعمال العالمية دار. مكن ٣٠ ليبين قتيل، عسكرياً. ما الصفحات استرجاع مكن.",
                },
                {
                  image: {
                    src: "assets/images/koshu-kunii-ADLj1cyFfV8-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بال بالعمل جديداً ماليزيا، ان. عن بها مليارات المتحدة, كردة واتّجه قُدُماً قام و. عل مارد الصين مساعدة كما. سابق الإنزال أي أسر, معزّزة بالحرب ضرب في. تم أضف خيار مواقعها.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "الساحل الشرقي",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/matthew-landers-v8UgmRa6UDg-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "جُل بشرية إستعمل إذ, بـ جعل خيار لغات تسبب. لم الإمتعاض الإتفاقية وتم, دنو كل وفنلندا وبالتحديد،, بخطوط وبلجيكا، بين بل. مشارف الأوربيين ثم ذات, و تشكيل إعلان أضف. جُل الضروري الخاصّة ان, الإمداد عشوائية وقد ثم, عرض بل أملاً ديسمبر. مع لغزو بالعمل ذات, فاتّبع الأمور جعل كل.",
                },
                {
                  image: {
                    src: "assets/images/c-j-1GHqOftzYo0-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "إذ قادة المضي النزاع قام, ما كلا وأكثرها ومطالبة الساحلية. قتيل، المضي ولم أم. ثم مما اتّجة الإكتفاء. قبل و أساسي بالسيطرة, حول أن بلاده معزّزة. و بقصف مسارح الإنذار، ذلك, تعد أمّا بهيئة الوراء مع.",
                },
                {
                  image: {
                    src: "assets/images/jacob-licht-8nA_iHrxHIo-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "يكن الفترة وتنامت العالمي بـ, انتباه الربيع، باستحداث فصل عن. بعض ثم خطّة القادة. نقطة مرمى تم جُل, حيث قد تجهيز إستعمل التبرعات, يتبقّ رجوعهم إذ عدد. أسيا الشتاء عل شيء. معارضة وبالتحديد، قد أخذ. لان فكان بلديهما ثم, لدحر فهرست بل نفس.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "الساحل الغربي",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maria-lysenko-tZvkSuBleso-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "لها لغزو للمجهود لم. الخاصّة اقتصادية من حول, بعض قد لإعلان الإنزال الإمتعاض. بعض إذ إحكام الآلاف, أن هذا مقاطعة بالمحور. كل إحتار اعلان الى, بل به، مارد بخطوط المسرح.",
                },
                {
                  image: {
                    src: "assets/images/peter-thomas-17EJD0QdKFI-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "أوروبا ولاتّساع وحلفاؤها تم جهة, عل جعل وعلى وجهان والنرويج. بحشد أكثر هو به،, كان وإقامة استبدال لم. لمّ تصفح وفرنسا إذ, فقد أطراف إتفاقية ما, فصل اعتداء الطرفين ان. قُدُماً التجارية بـ مدن. مقاومة والروسية ايطاليا، كان أم, ان ومحاولة المشتّتون وقد, مما أم وحتى بتخصيص. دنو قد الجو ويتّفق, ثم الأولية الخارجية ويكيبيديا إيو, الأثنان واستمرت باستخدام من لكل.",
                },
                {
                  image: {
                    src: "assets/images/xan-griffin-QxNkzEjB180-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "والنفيس التاريخ، بريطانيا، تم وقد, يذكر علاقة به، أم. أم يبق العدّ الساحة, المضي وكسبت انه قد, إتفاقية والديون الأراضي ما هذا. حول تم خيار الولايات, المجتمع المتحدة الأوروبية، بحق ما, تم للصين الأمور العمليات تحت. مليون هاربر المشتّتون لم عرض. بهيئة الغالي كما مع, ان بتخصيص مشاركة بالإنزال انه, وبعض مشروط ان بلا.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-crime+justice",
          name: "الجريمة والعدالة",
          articles: [
            {
              class: "columns-3-balanced",
              header: "المحكمة العليا",
              type: "articles-list",
              content: [
                {
                  title: "أو ابتسامة أو راحة سحب.",
                  content:
                    "سابق أفريقيا الإتحاد في يكن, شمال لغات الحرة ٣٠ الى, أي بأضرار وحلفاؤها الرئيسية إيو. هو الى قامت البرية تزامناً, أم دفّة الحيلولة يبق. ان خلاف انذار مدن, مايو لإنعدام عل به،. المدن باستحداث لبلجيكا، من بحث, و أعمال الشرق، الجنرال بعض, و مارد بالرغم بالإنزال بعد. كل عقبت أكثر الربيع، شيء, ان أهّل ماشاء مدن. أحدث احداث بـ جُل, الحرة يتعلّق بلا هو.",
                },
                {
                  title: "الكثير من الضعف مات هو.",
                  content:
                    "حتى ان لإعلان التغييرات, بـ وأزيز التجارية بريطانيا، بها. كل وبدأت يعادل لتقليعة حدى. ساعة ضمنها الفرنسي ٣٠ أضف. وصل في هُزم كانتا وإعلان, ما الى شواطيء اللازمة. بال كثيرة واتّجه الهادي ثم, بشكل ووصف الأرضية قام ما.",
                },
                {
                  title: "موريس في بعض السلطة البيئية مثل مطوري كرة القدم.",
                  content:
                    "هو دول المضي الإحتفاظ الإمتعاض, بينما أعلنت قُدُماً جُل لم, الأحمر الخاسرة بل جُل. بل أخذ معقل الأجل الصعداء, الله اوروبا بالجانب أي عدم. الشتاء العظمى حاملات كل ضرب, عدم دخول مئات المزيفة في. بحشد اتفاق شيء أن, بـ أخذ تشكيل قبضتهم اللازمة. انه قد وحرمان الساحلية الأوروبي.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "قانون محلي",
              type: "articles-list",
              content: [
                {
                  title: "الكثير من العلاج حتى الضعف القطر.",
                  content:
                    "مشروط العدّ المتحدة في جُل. مع شدّت تكبّد الإطلاق تلك, والكوري والمانيا الى عل, جعل تم مليون العسكري المجتمع. كلا شرسة البولندي الساحلية مع, الى هو بقعة تنفّس واقتصار. إذ لفشل ديسمبر لكل, و حتى إبّان بريطانيا-فرنسا. للسيطرة العسكري المنتصر أن عرض, وتم وصافرات التقليدي أي.",
                },
                {
                  title:
                    "أحدث أسهم كرة السلة السريرية من الفلفل الحار تحتاج دائمًا إلى الواجب المنزلي.",
                  content:
                    "أم الشرق، معزّزة الحيلولة الى. الصين شواطيء إذ قبل, مرمى الأهداف ثم الا, و بعض أراض والديون. أم كردة بقسوة وبالرغم على. إعادة ماشاء العسكري حتى عن, وإعلان العظمى عل كما, بحشد حقول لهذه أم فقد. لان العصبة والمعدات من, بقصف المضي المواد كل يتم. سكان ويتّفق وإيطالي فعل كل, ثم بال أوزار فشكّل, تكبّد التاريخ، التغييرات جعل عن.",
                },
                {
                  title: "الشوكولاتة الشوكولاتة الشوكولاتة في الأرز.",
                  content:
                    "إحكام للمجهود جهة قد, وصل ووصف أوسع للحكومة عن. وشعار عسكرياً التقليدية لم كما, كما ما ماذا ومضى تطوير, تحرّك بالحرب قد بحق. أن أمّا اتفاق لها. ماذا ٠٨٠٤ وبولندا عن قبل, لها وقام التّحول استرجاع عن. قد سياسة غرّة، كلّ, و جُل أطراف إستيلاء.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "رأي",
              type: "articles-list",
              content: [
                {
                  title: "رائع وتصنيع العقارات المختارة.",
                  content:
                    "ثم الجنرال الأوربيين يكن. تعد الذود تحرير وباءت ٣٠. تم لكل مليون سبتمبر شموليةً. وبعض سبتمبر يتعلّق بلا عن, إيو قائمة أجزاء و, ثم بعد للمجهود وبولندا. و أسر مرجع الخطّة اوروبا, من ذات وعُرفت الإنزال. اتفاق الشمل بمباركة بال أن, تلك أن السيطرة الأعمال, وحتّى السيطرة بريطانيا أن لها.",
                },
                {
                  title:
                    "خلافاَ للإعتقاد السائد فإن لوريم إيبسوم ليس نصاَ عشوائياً،",
                  content:
                    "لمّ إبّان وباءت الإثنان عل, مكن الطريق الشّعبين ٣٠. فقد خطّة وترك يتسنّى ان, الفرنسية ولكسمبورغ بـ حيث, أسر قد الأمريكي الإقتصادية. أي بين تطوير النزاع, المحيط النزاع عسكرياً بعد لم. كما معارضة للسيطرة انتصارهم و, عن وإقامة الإقتصادية أضف. من أسر الأوضاع الدنمارك انتصارهم, اليميني لإنعدام يكن إذ. تشكيل بريطانيا، لمّ بل, كل واُسدل للسيطرة بحث, انه ان هُزم التبرعات.",
                },
                {
                  title: "لكلاسيكي منذ العام 45 قبل الميلاد، مما يجعله أكثر",
                  content:
                    "جمعت الأوروبيّون أخر مع, ترتيب يونيو واُسدل كلّ أن, وفي لم وباءت ديسمبر. ثم اعتداء المنتصر كلا. بل حدى بقصف عشوائية. إذ بالفشل واتّجه بها, ما عرفها للمجهود اتفاقية هذا, وانهاء الكونجرس ٣٠ بال. خطّة الأولى الباهضة لم مما, لم جهة إعلان الباهضة.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-around-the-us",
          name: "حول الولايات المتحدة",
          articles: [
            {
              class: "columns-3-balanced",
              header: "أحدث",
              image: {
                src: "assets/images/chloe-taranto-x2zyAOmVNtM-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "لجعل سعر الميكروفون لسحب القوية ، ودرجة الحرارة.",
              type: "list",
              content: [
                {
                  content:
                    "لقد كان مطور الراحة في التغذية في.الجزر المتخرج لكرة القدم هو عقار في.",
                },
                {
                  content:
                    "وثيقة الهوية الوحيدة لتزيين الجزر المتدربين للاستثمار في الأرنب.من فضلك ومع ذلك ، إنفاذ التنفيذ الاحترافية البيئية.",
                },
                {
                  content:
                    "مع, قبل أي اللا المنتصر والنفيس. أن حتى مئات وإيطالي تشيكوسلوفاكيا, في سابق",
                },
                {
                  content:
                    "عجّل أصقاع الربيع، حيث قد. الخاطفة الأبرياء انه عن, بعض ما لفشل الأحمر",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "عمل",
              image: {
                src: "assets/images/razvan-chisu-Ua-agENjmI4-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "في السرير دفع الواجب المنزلي وادي تيلوس معرف لاعبي الجزر قطر الجزر.",
              type: "list",
              content: [
                {
                  content:
                    "من الى الضغوط البرية الانجليزية, كل لهذه إبّان ضرب, هذا الوراء الإتحاد",
                },
                {
                  content:
                    "أحتاج إلى الميكروويف الخاص بي ولكن مجاني ل.يحتاج الفلفل الحار الشوكولاتة دائمًا إلى الواجب المنزلي في المنطقة.",
                },
                {
                  content:
                    "الحاضر الحزين العظيم هو الفلفل الحار المهم.الفول السوداني في سياق القداس المدرسي.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "سياسة",
              image: {
                src: "assets/images/colin-lloyd-2ULmNrj44QY-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "بينما تعمل جميع مولّدات نصوص لوريم إيبسوم على الإنترنت على",
              type: "list",
              content: [
                {
                  content:
                    "بل إيو وقبل مشروط الدولارات, عل أخرى لفرنسا نفس. أن وسوء الحكم الأمريكي لا",
                },
                {
                  content:
                    "التغذية كتلة العقارات تعويض الواجب المنزلي.الميكروفون في مؤلف الوعاء الآن الذي يعمل.",
                },
                {
                  content:
                    "مبروك الجري تحتاج الآن إلى الشوكولاتة الكرتون موريس.",
                },
                {
                  content:
                    "أمراض الألم ليست قوسًا للابتسامة التي تتماشى لأن أي شخص آخر هو معرف.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-latest-media",
          name: "أحدث الوسائط",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/dominic-hampton-_8aRumOixtI-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/sam-mcghee-4siwRamtFAk-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/adam-whitlock-I9j8Rk-JYFM-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/leah-hetteberg-kTVN2l0ZUv8-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-us-business",
          name: "عمل",
          articles: [
            {
              class: "columns-3-balanced",
              header: "محلي",
              type: "articles-list",
              content: [
                {
                  title: "ومع ذلك ، فإن منطقة الكرتون في هذا الشارع العشور.",
                  content:
                    "وقد في وحتى الانجليزية. تعد أم الصين وانتهاءً. مارد أوزار للحكومة من بعض, ٣٠ أسر تكاليف وأكثرها, قبل شعار لغات وحلفاؤها مع. حتى ما وإقامة وإيطالي, أن إيو والتي الثالث واعتلاء.",
                },
                {
                  title: "وقت رعاية القطر الذي لاعبي كرة القدم.",
                  content:
                    "في حيث وبغطاء الشتاء والعتاد, ولم عن دخول تطوير شواطيء. تحرير إحتار الطريق دار تم, حتى أكثر الربيع، كل. كل اللا ارتكبها الثالث، دار, ٣٠ عرض هاربر العالم, بل وصل سياسة ماشاء الجنود. الشمال والمانيا بـ بعض, انه مع الدول موالية, و المارق وتزويده ضرب. عن الله تُصب كما.",
                },
                {
                  title: "الأسد أو البوابة السريرية ليست وسادة أو.",
                  content:
                    "السبب الشتاء، استمرار فقد هو. بل إيو يتمكن الباهضة, ان ذات اوروبا البرية. تعد الدّفاع المؤلّفة ان. قامت ليبين الأوضاع مع مدن, فكان المنتصر ما مدن.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "عالمي",
              type: "articles-list",
              content: [
                {
                  title:
                    "كل أسهم جزر الفلفل الحار في نهاية الأسبوع التصوير الفوتوغرافي موريس.",
                  content:
                    "الصين محاولات أم بعض, عن حيث خلاف ديسمبر. هو أوسع مقاطعة تشيكوسلوفاكيا بعد, عرض وسفن بقيادة أم. ما تحرّك بالتوقيع بها, دار شرسة احداث من, قبل لم فكانت ابتدعها البشريةً. ببعض تنفّس لم عدم.",
                },
                {
                  title:
                    "كان حامل كرة السلة سريريًا لاستثمار التغذية السريرية.",
                  content:
                    "التي السبب اتفاقية أم وفي. كلا ثم دفّة مايو, لدحر ألمّ الثقيل ما يتم. الا لم أخرى أكثر تعداد. وفي مدينة الغالي بل, بين الأمور لإنعدام المتساقطة، أن. قبل حالية إستعمل مسؤولية ما.",
                },
                {
                  title: "لا يحتاج رات بما تتطلبه",
                  content:
                    "مما تم الأجل لفرنسا الأوربيين. أوسع وزارة ثم بين, هو عرض قتيل، الستار ومحاولة, الى فكان كنقطة لم. هو والتي ويتّفق المؤلّفة وصل. عدد لغات بقسوة البولندي لم. هو بحق يذكر جسيمة وسمّيت, قد مدن هناك غريمه وبالرغم. دار لم ماشاء التقليدي, أسيا العظمى بالمحور تحت بـ. دول عل اللا أوزار الخطّة.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "ربعي",
              type: "articles-list",
              content: [
                {
                  title: "لا الدردشة القوس الحامل وأصول درجة الحرارة.",
                  content:
                    "عجّل أصقاع الربيع، حيث قد. الخاطفة الأبرياء انه عن, بعض ما لفشل الأحمر لإعلان. و أضف شعار قامت, أمدها أوزار الإكتفاء أما مع, قبل أي اللا المنتصر والنفيس. أن حتى مئات وإيطالي تشيكوسلوفاكيا, في سابق انذار الإتفاقية بحق. ان الدول لإعلان للسيطرة بحث, أي بعض الجو وايرلندا, أن بلا غينيا لليابان.",
                },
                {
                  title:
                    "أتمنى لكم معرف فقر وثيقة الهوية الوحيدة لتزيين الكراهية القوس.",
                  content:
                    "وبعض وإيطالي إذ به،. بل الا القادة اعتداء الهادي. ألمّ مساعدة عدد عل, عدد كل التي لإعلان. فعل ما دخول للسيطرة, بـ وإقامة للإتحاد بها. عرض عالمية للإتحاد لم.",
                },
                {
                  title: "الآن أو ابتسامة الراحة سحب طبقة لاكوس أو.",
                  content:
                    "من الى الضغوط البرية الانجليزية, كل لهذه إبّان ضرب, هذا الوراء الإتحاد في. كل منتصف عسكرياً حول, إذ بعض أحكم مسؤولية بالسيطرة. هامش ليرتفع من يبق. كلا أن وعُرفت بمعارضة وحلفاؤها, أي بعد سكان وبعدما الانجليزية. يتم مع لغات سنغافورة.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-underscored",
          name: "يؤكد",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/dillon-kydd-2keCPb73aQY-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "ساعة وتنامت وباستثناء حيث و, جُل عقبت بداية قتيل، عن. هو غير تحرير الخاسر. الدمج واقتصار والمانيا وفي ٣٠. انه في الموسوعة استطاعوا, أن جدول وحرمان تعد. كانتا السادس العالمي قام ما, أن حيث وبعض ليبين اليميني. والقرى الخاسرة بالولايات كما إذ.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/aaron-huber-G7sE2S4Lab4-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "بل إيو وقبل مشروط الدولارات, عل أخرى لفرنسا نفس. أن وسوء الحكم الأمريكي لان, ساعة الخارجية شيء في. ٠٨٠٤ الأمم الولايات في ذات, عن عقبت إحكام ومحاولة يتم, شيء عل بتخصيص سنغافورة. كل شيء بقعة وبغطاء واقتصار, نقطة انتهت عدد تم, إذ الى تونس أحدث. أم وتم اتّجة ومطالبة, الى تُصب فمرّ العالم ما, هو جُل جنوب فهرست الوزراء.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/mesut-kaya-eOcyhe5-9sQ-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "حيث لفرنسا المعاهدات الدولارات و, لها الدنمارك وبريطانيا عل. دون دارت مليون السيء تم, أم بهيئة الأسيوي الساحلية بعد. بـ غريمه انتباه تعد, إذ الحرة انتباه مسؤولية مدن. إجلاء الثالث وقوعها، عن ضرب. بين جمعت سكان والعتاد ما. مدن ثم الساحل العالمي استبدال, دول بل ونتج انتهت.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/diego-jimenez-A-NVHPka9Rk-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "قد ذات أدوات بالحرب ومحاولة, مع جمعت لفشل والروسية ذلك. فصل عل جنوب إعادة وكسبت, عرض عن زهاء المبرمة. و ومن خلاف وحرمان, أن يبق رئيس لتقليعة الأبرياء. كلا لم انتهت شواطيء, أم نفس هاربر الساحلية.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-state-by-state",
          name: "الدولة حسب الدولة",
          articles: [
            {
              class: "columns-wrap",
              header: "كاليفورنيا",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/craig-melville-_JKymnZ1Uc4-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "حقول هنا؟ الموسوعة كان كل, حاول جنوب عن وقد. لها إذ شمال الإنزال الثالث،. لها قد العدّ بمحاولة المتاخمة, سكان الأمور اليابان، أما من. ارتكبها الأولية تعد بـ, وقد مكثّفة وحلفاؤها باستخدام ان, حيث تم الجنود الإنزال.",
                },
                {
                  image: {
                    src: "assets/images/robert-bye-EILw-nEK46k-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ضمنها لإعادة الإتحاد إيو من, فقد ترتيب يتسنّى واقتصار في, إستعمل استمرار العالمية ٣٠ عدم. أي كما ليبين مهمّات الشرقية, إذ جورج للأراضي ضرب. جيوب تعديل بمباركة أسر عن, شعار سقطت إستعمل ذلك ان. بل العناد الإحتفاظ اليابان، دون. أن أما الشتاء ومحاولة, ٣٠ معقل الأسيوي ويكيبيديا، قام.",
                },
                {
                  image: {
                    src: "assets/images/sapan-patel-gmgWd0CgWQI-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ببعض بمحاولة وتم و, تم عدد لإنعدام اقتصادية. لان في وبعض الشهيرة, قد الشمال ماليزيا، الدولارات إيو. يبق قائمة جزيرتي ما. يبق حصدت هامش الأمم ٣٠, و خطّة قادة السادس فقد. عل حيث وقبل ممثّلة الغالي.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "نيويورك",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/prince-abban-0OUHhvNIbYc-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بال ساعة بالفشل وسمّيت ما, تحت بل الله الجو. التي بمباركة ثم كما, بل كلّ دفّة عرفها. أي جُل نتيجة يتعلّق وقدّموا, بها لم أكثر وبلجيكا،. يطول السيطرة ٣٠ أضف, ٣٠ تُصب تحرير الأولى حتى. عدد إجلاء نتيجة الساحة ثم, الدول فقامت يكن ٣٠. من أراض تغييرات الولايات بها, بل القادة جديداً على.",
                },
                {
                  image: {
                    src: "assets/images/quick-ps-sW41y3lETZk-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "فصل جيما اتفاق الهادي أم, تعد عل قبضتهم وتتحمّل. تشكيل حاملات التنازلي حتى تم. حين بـ يتعلّق البرية الحيلولة, أي الأخذ العظمى حين. وعلى كرسي وقوعها، أي الى, الهادي المتّبعة ان هذه, فعل ٣٠ جنوب الشمال.",
                },
                {
                  image: {
                    src: "assets/images/lorenzo-moschi-N7ypjB7HKIk-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ليتسنّى وبالتحديد، دول ٣٠, بمباركة مليارات الشّعبين قام من. ان يعبأ بالحرب وباستثناء جعل, دون بـ ومضى وجهان فرنسية. مع بشكل إعمار حتى. وقبل لإعادة وقد ما, بهيئة تشكيل مما أي, الشرقية بمحاولة تعد ٣٠.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "واشنطن",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/koshu-kunii-v9ferChkC9A-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "قام أم سقوط أواخر. مدن الثقيل الثقيلة الواقعة ٣٠, مع المحيط الصفحة الشتاء، دون, بحق كردة التحالف الإقتصادي مع. يتبقّ الفترة كلّ أم. أم مسرح شمال وسمّيت حيث, يتم في تسبب وحتّى. جدول النفط فقد ٣٠, الستار الباهضة بريطانيا لمّ في.",
                },
                {
                  image: {
                    src: "assets/images/angela-loria-hFc0JEKD4Cc-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "فسقط الثالث عل على. ومن الأجل اليها بل, عرفها وباءت إتفاقية أضف بل. بحث بل إعلان الأبرياء الاندونيسية, بل إنطلاق العالم تزامناً يبق. والحزب بالمطالبة تم ضرب, قد وتم مهمّات بولندا،, جعل تُصب وعلى بالحرب ثم. الى أم وتتحمّل الوزراء, تعداد الانجليزية دنو ٣٠. غينيا وفنلندا فقد عل, كُلفة الحدود العالمي ثم جهة, الحكم بلديهما أي به،.",
                },
                {
                  image: {
                    src: "assets/images/harold-mendoza-6xafY_AE1LM-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "يطول إجلاء تم كلّ, و فصل عالمية بقيادة بمعارضة. عدد تم قدما عالمية الأثنان, بفرض وسوء التّحول دول مع. ثم السيء إستعمل استمرار الا, اعلان كثيرة الثانية من قام. فهرست ويتّفق دنو من. ٣٠ فقد شرسة دأبوا تعديل, أن بتخصيص والإتحاد ولم, أخر بينما الشمل إذ.",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-hot-topics",
          name: "مواضيع مثيرة",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/libre-leung-9O0Sp22DF0I-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "بل أمدها العصبة الإيطالية فصل, قد والفرنسي سنغافورة الأمريكية بعد. ببعض أمّا قد مما, كانتا والكساد بـ حتى. عدد إذ معاملة وسمّيت. أخذ أم قُدُماً بالإنزال, تعد هامش نهاية عالمية بل, بقسوة العدّ بالحرب فصل أم. في الصفحة وقدّموا مدن. عن ضمنها بولندا، دنو, حين وقبل بداية إختار كل, ان بال الحكم اقتصادية. أن تعد مارد الشمال, حيث و أهّل العالمي اقتصادية, إختار التغييرات ولم ثم.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/pascal-bullan-M8sQPAfhPdk-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "Iليبين بتخصيص حين و. كانتا بولندا، واستمرت تعد مع, بل لها ثمّة بتخصيص الثالث،. القوى بريطانيا-فرنسا و كما, و تلك يتسنّى ا اليميني, تلك ثم بالرّد مهمّات الشتوية. ثمّة المشترك ومحاولة هو بها.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/brooke-lark-HjWzkqW1dgI-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "تم وقبل جسيمة وكسبت فقد, ما جهة الأسيوي والمعدات. عل كانت الهجوم يتم, مكن أن أمّا وبحلول المبرمة. عن غير وكسبت عسكرياً, بل شيء أثره، ا. أخذ تشكيل بقيادة المنتصر كل, ذلك أدنى أواخر اقتصادية مع. الطريق العالم، اليميني فقد ثم, لأداء وتتحمّل الأعمال قبل تم. والحزب وفرنسا عسكرياً جهة كل, كل تحت سياسة تحرّك.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/matthias-heil-lDOEwat_MPs-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "بقصف العصبة الوراء وصل و. لهذه المضي أم قبل, هو دول بقصف حادثة. دارت مسرح عقبت تلك عل. حلّت معاملة تحت ما. قد الضروري وصافرات تحت.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-us-paid-content",
          name: "المحتوى المدفوع",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/tadeusz-lakota-Tb38UzCvKCY-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "وقتي تمويل مطور التغذية يحتاج إلى الحمل.كما أن المطورين لا يقدمون دائمًا ولكن السيرة الذاتية للكرة الطائرة.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/leisara-studio-EzzW1oNek-I-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "ومع ذلك ، غد كرة القدم قوس الكحول.وكان أيضا الشوكولاته في الشوكولات لا رسم كاريكاتوري.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/tamanna-rumee-lpGm415q9JA-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الآن ، لكن هذا يبتسم دائمًا في المكياج الحامل للمكتب.وكرة القدم حتى أكره راحة قطر الأطفال في عطلة نهاية الأسبوع ولكن.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/clark-street-mercantile-P3pI6xzovu0-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "دائما صفر الشخص المختار في قطر عطلة نهاية الأسبوع ليتم تعقيمه.فيليس بحاجة الآن إلى كرتون الكثير من تذوق الفلفل الحار في الكتلة. الجوع والقبيح.",
                },
              ],
            },
          ],
        },
      ],
    },
    world: {
      name: "عالم",
      url: "/world",
      priority: 1,
      sections: [
        {
          id: "content-world-global-trends",
          name: "الاتجاهات العالمية",
          articles: [
            {
              class: "columns-3-balanced",
              header: "أفريقيا",
              url: "#",
              image: {
                src: "assets/images/will-shirley-xRKcHoCOA4Y-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "لكنها دائما ابتسامة في مكتب حامل.الأسهم السريرية من الفلفل الحار تحتاج دائمًا إلى الواجب المنزلي في المنطقة.",
              type: "text",
              content: `مدن و تحرير بمباركة الباهضة, الأول اعتداء أخر لم, تلك و الثقيل الحكومة استرجاع. هو العدّ اليابان لان, ٣٠ وقامت مكثّفة مكن, مدن بلاده الآخر الشرقية ٣٠. غير للمجهود المجتمع مع, وشعار ايطاليا، تم كلّ. قد الحيلولة العمليات وصل. سابق والحزب الدّفاع انه و. جهة بـ وترك والحزب تزامناً.

 أفريقيا, شاسعة إعمار به، ما. ولم كرسي وقوعها، من. فصل واستمر التنازلي لم, أم مكن تاريخ تطوير الساحل. أساسي بمحاولة دار هو, لها الأول لإعادة ما.

٣٠ ومطالبة والفلبين جهة, أراض معارضة عرض ان. حكومة إحتار تم ضرب, عل جمعت وعلى أضف, حدى في نهاية الأرضية. الدّفاع العالمية الا أي, بـ طوكيو الساحة حدى. بحق ما اليابانية الإيطالية الأوروبية،, بـ فبعد ترتيب دول. بالحرب بالمطالبة ولم أن.`,
            },
            {
              class: "columns-3-balanced",
              header: "الصين",
              url: "#",
              image: {
                src: "assets/images/nuno-alberto-MykFFC5zolE-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "وادي ياسمين وميكروفون في الابتسامة.يحتاج مطور إلى الحمل مع رفاق أشعة السحب.",
              type: "text",
              content: `تحت لفشل المتّبعة و, دول بأيدي بتخصيص مليارات عل. كل فمرّ شاسعة لعملة أخر, حول مع أوسع الشتاء،, حتى مشارف لمحاكم الشتوية إذ. كل الى جيما النزاع الكونجرس. ما اللا العالم فقد, يبق بـ سقطت أوزار بالولايات. وقوعها، التغييرات عن بحث. عن ميناء واشتدّت جهة, قبل يذكر التنازلي أي.

جعل تم تونس ماذا مسؤولية, جُل ٣٠ بتخصيص وأكثرها والديون, ان بلاده وحلفاؤها وصل. أم بحث الواقعة باستخدام, كان ما فرنسا بالرّد لبلجيكا،, اتفاق وبولندا بالولايات أي حدى. دار القادة وتنامت عن. جُل طوكيو الثانية و, انه السفن العالم، عن. شمال مليون ثم كان, كل مما موالية ألمانيا الأهداف.`,
            },
            {
              class: "columns-3-balanced",
              header: "روسيا",
              url: "#",
              image: {
                src: "assets/images/nikita-karimov-lvJZhHOIJJ4-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "ارتجاع كبير وتصنيع عقاري مختار كرة السلة موريس.",
              type: "list",
              content: [
                {
                  content:
                    "اليها الهادي المتساقطة، لم بال وبغطاء الشمال ايطاليا،, ثم المارق الأبرياء التقليدية حيث, مكن عل ويعزى لبلجيكا،. ترتيب لإعلان هذا قد, مدن عل تعديل الحدود اسبوعين.",
                },
                {
                  content:
                    "وزارة أفريقيا الإقتصادي كما ٣٠, غينيا وبداية اليابان، عل هذه. لها تم بفرض أفاق الثالث،, بال مع مايو للمجهود, بقسوة فهرست الأولية هذا تم. حين في دأبوا",
                },
                {
                  content:
                    "هذا بتخصيص بالرغم أن, الإقتصادية, لان غضون مقاومة تم. ولم إبّان أراضي إذ. مكن لم أفاق وأزيز وبلجيكا،, حقول المزيفة ٣٠ ضرب. وفرنسا المعاهدات عن بحق, فصل مسؤولية كل.",
                },
                {
                  content:
                    "مرجع مليون الخطّة ومن ما, بقيادة وحرمان إيو كل. بتحدّي الأولية يكن لم. هو قام ليرتفع الخاطفة بالمحور, هو يذكر العدّ هذا. يتسنّى الفرنسي وتم هو, و بلا حادثة المواد. حول إذ يعادل",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-around-the-world",
          name: "حول العالم",
          articles: [
            {
              class: "columns-3-balanced",
              header: "أوروبا",
              image: {
                src: "assets/images/azhar-j-t2hgHV1R7_g-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "محايدة محايدة. دائما المؤلف أو فترة الحياة.",
              type: "text",
              content: `مع ولم بقسوة الخطّة. أم أمام الشرقية والنرويج نفس, بتطويق الأبرياء التغييرات في حدى, الطرفين بالمطالبة دار عل. بها ومضى وتنامت مليارات بل. إذ ولم الإطلاق مليارات وحلفاؤها, بخطوط بزمام لها ٣٠.

أي لعملة إستعمل العالم، مما, وباءت الضغوط وبولندا ٣٠ تلك. حول هناك سياسة إذ, إيطاليا تزامناً الانجليزية ٣٠ ولم. قادة فاتّبع الثالث، ان هذه. إذ ووصف بزمام بين, ومن ثم ودول لهذه الموسوعة. الطريق للحكومة الأعمال يكن في, تحت قد الذود انتصارهم. لها واستمر الإطلاق استطاعوا ان. ووصف يتمكن الثالث، كل أسر.`,
            },
            {
              class: "columns-3-balanced",
              header: "الشرق الأوسط",
              image: {
                src: "assets/images/adrian-dascal-myAz-buELXs-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "الموظف والاتحاد الأوروبي ولكن الكثير من التصنيع المحدد.",
              type: "text",
              content: `جعل إحتار الباهضة الواقعة ان, مدن في ودول الساحل الإثنان. بعض بينما مشروط الساحلية ان, عل اتفاقية التاريخ، هذا, قامت يتمكن بالتوقيع أي عرض. حكومة وعُرفت لم وصل. أسر بل نتيجة والحزب.

خطّة وتنامت حين أم, تمهيد وايرلندا قد أخر. لفشل بداية بل بحق, أم وتم أكثر تشيكوسلوفاكيا. كان لم كانت المبرمة. قد أما نقطة مساعدة ابتدعها, تحت قد أخرى ارتكبها, انتباه والفلبين الاندونيسية لمّ عل. ان اللا وبالرغم الاندونيسية أخذ, بين عن لهذه بالفشل.`,
            },
            {
              class: "columns-3-balanced",
              header: "آسيا",
              image: {
                src: "assets/images/mike-enerio-7ryPpZK1qV8-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "كان الخوف في وقت الراحة.",
              type: "list",
              content: [
                {
                  content:
                    "تم وقبل جسيمة وكسبت فقد, ما جهة الأسيوي والمعدات. عل كانت الهجو",
                },
                { content: "تم اختياره كأسد في حياة القداس المدرسي." },
                {
                  content: "والحزب وفرنسا عسكرياً جهة كل, كل تحت سياسة تحرّك.",
                },
                {
                  content:
                    "الاتحاد الأوروبي لكرة السلة مؤلفة الدعاية لكرة القدم لتخرج.",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-latest-media",
          name: "أحدث الوسائط",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/greg-rakozy-oMpAz-DN-9I-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/annie-spratt-KiOHnBkLQQU-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/noaa-Led9c1SSNFo-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/paul-hanaoka-s0XabTAKvak-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-world-today",
          name: "اليوم",
          articles: [
            {
              class: "columns-3-wide",
              header: "الاضطرابات",
              url: "#",
              image: {
                src: "assets/images/venti-views-KElJx4R4Py8-unsplash_684.jpg",
                alt: "عنصر نائب",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "الصورة التي التقطها شخص ما.",
                tag: { type: "breaking", label: "كسر" },
              },
              title:
                "موز الكرتون يتطلب الكثير.في الاحماء أو وضع على الوعاء أم لا.",
              type: "list",
              content: [
                {
                  content:
                    "القداس ليست لا قيمة لها الآن العقارات.الجزر المتخرج لكرة القدم هو عقار في.",
                },
                {
                  content:
                    "إذ. كل الى جيما النزاع الكونجرس. ما اللا العالم فقد, يبق بـ سقطت أوزار بالولايات",
                },
                {
                  content:
                    "مدن و تحرير بمباركة الباهضة, الأول اعتداء أخر لم, تلك و الثقيل الحكومة استرجاع. ه",
                },
                {
                  content:
                    "اليها الهادي المتساقطة، عن لمّ. وفي هو قبضتهم وتنصيب. لم بال وبغطاء الشمال ايطاليا،, ثم المارق الأبرياء التقليدية حيث, مكن عل ويعزى لبلجيكا،. ترتيب لإعلان هذا قد, مدن عل تعديل الحدود اسبوعين. تحت لفشل المتّبعة و, دول بأيدي بتخصيص مليارات عل. كل فمرّ شاسعة لعملة أخر, حول مع",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "يحدث الآن",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/koshu-kunii-cWEGNQqcImk-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title: "دائما أو وقت الحياة كأطفال أو للبعض.",
                },
                {
                  image: {
                    src: "assets/images/kenny-K72n3BHgHCg-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "طب الكرتونية من البلياردو أو مضادات الأكسدة في عطلة نهاية الأسبوع.",
                },
                {
                  image: {
                    src: "assets/images/kitthitorn-chaiyuthapoom-TOH_gw5dd20-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "السهام السريرية كرة القدم عطلة نهاية الأسبوع الكراهية مضادات الأكسدة موريس الجلوس.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "جدير بالملاحظة",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/olga-guryanova-tMFeatBSS4s-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title: "الآن مشروب الموز هو مضادات الأكسدة حامل أو وادي.",
                },
                {
                  image: {
                    src: "assets/images/jed-owen-ajZibDGpPew-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title: "إلى القطر من عدم وجود كتلة طيران ليست تصنيع معين.",
                },
                {
                  image: {
                    src: "assets/images/noaa-FY3vXNBl1v4-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title: "كرة القدم الإحماء العظيمة ليست قطر.",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-featured",
          name: "متميز",
          articles: [
            {
              class: "columns-3-balanced",
              header: "الاتحاد الأوروبي",
              image: {
                src: "assets/images/christian-lue-8Yw6tsB8tnc-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "الحداد تعقيم وعاء بيئي كبير.",
              type: "list",
              content: [
                {
                  content:
                    "كان ان وبدون حادثة النزاع, إذ العصبة واقتصار كما. ثم فعل هاربر أفريقيا.",
                },
                {
                  content:
                    "سنغافورة لم. ولم عل لفشل تحرّكت محاولات, أم كما الساحة الجديدة، الفرنسية, أن دون مرمى أمام مساعدة. ٣٠ جُل حالي",
                },
                {
                  content:
                    "الشرق، الهادي ٣٠ أما, ثم سقوط أمّا ومن. لمّ بل لعدم الأول. إذ أطراف اكتوبر دنو, قِبل الثقيلة الجديدة، حول كل",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "بريطانيا",
              image: {
                src: "assets/images/ian-taylor-kAWTCt7p7rs-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "الشوكولاتة السريرية الفلفل الحار تحتاج دائما الواجب المنزلي.",
              type: "text",
              content:
                "في قتيل، ابتدعها دنو. ضرب شمال للسيطرة استراليا، أم. قادة بقسوة الهادي مع عدم. إذ تطوير علاقة أساسي هذه. عليها الأوروبية الإقتصادية تم انه, بحق أم وسوء أمدها بريطانيا،.",
            },
            {
              class: "columns-3-balanced",
              header: "أمريكا اللاتينية",
              image: {
                src: "assets/images/axp-photography-v6pAkO31d50-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "التخطيط ما لم تأخذ الحياة الحزمة.",
              type: "list",
              display: "bullets",
              content: [
                {
                  content: "ليتم تعقيمها في المنطقة في الخوف من الضعف.",
                  url: "#",
                },
                {
                  content:
                    "خيار اليابان، الأوروبية، أم عدد. بـ لفرنسا واُسدل الى. التي اليميني",
                  url: "#",
                },
                {
                  content: "عرض يقوم أسابيع ديسمبر. سكان وزارة مسؤولية ذلك تم,",
                  url: "#",
                },
                {
                  content: "في اللاعبين باستثناء حياة كرة السلة للشوكولاتة.",
                  url: "#",
                },
                {
                  content:
                    "هذا لعدم القادة الأمريكي أم. حالية السبب قد أسر. عن أخر",
                  url: "#",
                },
                { content: "قطر عطلة نهاية الأسبوع كمنطقة معقمة.", url: "#" },
              ],
            },
          ],
        },
        {
          id: "content-world-international",
          name: "دولي",
          articles: [
            {
              class: "columns-wrap",
              header: "الأمم المتحدة",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ilyass-seddoug-06w8RxgSzF0-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "و بينما مكثّفة الأمريكية يبق, إذ لفشل أفاق استرجاع بعد, جُل أصقاع معزّزة اليابانية ان. وصغار وحلفاؤها هو جعل, كانت تحرّكت التقليدي بلا كل. مكثّفة وهولندا، يكن هو, خطّة وجزر مع بين. هذه ان جيما ليرتفع, دون مع فسقط اتفاق بأيدي. مع يتم اعتداء الأمور وهولندا،, الخاسر المتحدة كل عدد. الخارجية استطاعوا بـ بحق.",
                },
                {
                  image: {
                    src: "assets/images/mathias-reding-yfXhqAW5X0c-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "شواطيء ومطالبة إيو ان. أسيا والمانيا عل تحت, فكانت العالم، الربيع، أخذ أي. كلا و جديدة القوى. غضون أوسع اللازمة حول هو, يتم ما الآخر وبداية السادس, والقرى بريطانيا-فرنسا هذه ٣٠. وفي الحرة الأمور المواد إذ, جهة لم رئيس ميناء. بين أن أدوات التّحول.",
                },
                {
                  image: {
                    src: "assets/images/matthew-tenbruggencate-0HJWobhGhJs-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بـ أضف أوزار البولندي الإكتفاء. دارت الهادي يبق قد, أي ويعزى الساحل لمّ. وتم في أكثر اتفاقية الواقعة, وقبل مسؤولية قد شيء. يتم فمرّ مشارف ان, تلك ان وإقامة الخطّة. كانت الساحلية مدن كل, بل هذه اعتداء الثانية.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "الاتحاد الأوروبي",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/markus-spiske-wIUxLHndcLw-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "خيار اليابان، الأوروبية، أم عدد. بـ لفرنسا واُسدل الى. التي اليميني بـ بحق, هذه الأوضاع المتّبعة و, وعلى محاولات سليمان، الى ان. هو الخطّة ديسمبر الأثنان يتم, لمّ ثم قبضتهم الثالث، وبلجيكا،.",
                },
                {
                  image: {
                    src: "assets/images/jakub-zerdzicki-VnTR3XFwxWs-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "هذا لعدم القادة الأمريكي أم. حالية السبب قد أسر. عن أخر حصدت الفرنسية, الصفحات للإتحاد قبل ثم. ضرب عُقر أحكم لإعادة عل, من عرض يقوم أسابيع ديسمبر. سكان وزارة مسؤولية ذلك تم, غضون إجلاء العالم، ٣٠ تلك, دول كُلفة الخاسرة بل.",
                },
                {
                  image: {
                    src: "assets/images/guillaume-perigois-HL4LEIyGEYU-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ومن عن الشتاء الشرقي, إذ أسر تطوير والإتحاد. ٣٠ الى لفشل بالحرب الإمتعاض. شاسعة تعديل ولم عن, يقوم لبلجيكا، قام و, فصل هو أوروبا الخاصّة. فعل الإثنان اليابانية من.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "أزمة عالمية",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/mika-baumeister-jXPQY1em3Ew-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "مرجع الثانية الانجليزية لم مكن. فكان وبداية استمرار دار عل. الشتاء، الثانية حين مع. وصل أن لمحاكم للحكومة اليابان, تم عُقر وبغطاء شموليةً بها. أواخر الصينية فصل و, ومن أي ليتسنّى بولندا، اليابانية. بحشد عملية أما لم, هو العناد معزّزة بحق.",
                },
                {
                  image: {
                    src: "assets/images/chris-leboutillier-c7RWVGL8lPA-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "أخر في مشروط الهادي, عدم تم ترتيب الشّعبين, من الإنزال البشريةً بعض. عل عدد الصفحات للأراضي, إعلان للجزر الشتاء، جهة من. عن العالمي المبرمة الأبرياء دون. هو الشهير بالتوقيع أخر, تعد عن مايو واستمرت, عرض من جمعت ماشاء المارق.",
                },
                {
                  image: {
                    src: "assets/images/mulyadi-JeCNRxGLSp4-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "حيث فقامت السفن وبلجيكا، في, حين أم وحتّى لليابان, أي أخر هاربر الدنمارك. تلك في سكان التخطيط, غير أراضي إتفاقية وهولندا، ٣٠. ما لها سقوط المضي وقدّموا, أخذ حكومة بقيادة تكاليف قد, ذلك أراض الحكم أي. ومن أم بقصف منتصف الاندونيسية, أن كُلفة ويعزى الواقعة عدم.",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-global-impact",
          name: "التأثير العالمي",
          articles: [
            {
              class: "columns-3-balanced",
              header: "طقس",
              image: { alt: "عنصر نائب", width: "448", height: "252" },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "عنصر الأداء ما لم يكن أي خصم.",
              type: "list",
              content: [
                {
                  content:
                    "الميكروفون في مؤلف كتاب الوعاء الآن معرف يدير المخاوف.إذا كان هناك الكثير من المرح أيضا.",
                },
                {
                  content:
                    "أو عطلة نهاية الأسبوع ومطوري الحياة.مخاطر حركة المرور في.",
                },
                {
                  content:
                    "مرجع الثانية الانجليزية لم مكن. فكان وبداية استمرار دار عل.",
                },
                { content: "التذاكر في هذا الوقت من الوقت." },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "عمل",
              image: {
                src: "assets/images/david-vives-Nzbkev7SQTg-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "الآن ، موز حياتي الحلق أو.",
              type: "list",
              content: [
                {
                  content: "حيث فقامت السفن وبلجيكا، في, حين أم وحتّى لليابان,",
                },
                {
                  content:
                    "الضحك سحب المرحلة الجامعية الأولى في المنطقة عدد صحيح الشوكولاتة.",
                },
                {
                  content:
                    "الدورة التدريبية المطورين كتلة القبيحة وثيقة الهوية الوحيدة.",
                },
                {
                  content:
                    ". ما لها سقوط المضي وقدّموا, أخذ حكومة بقيادة تكاليف",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "سياسة",
              image: {
                src: "assets/images/kelli-dougal-vbiQ_7vwfrs-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "ن حتى جورج الحرة, ثم جيوب أواخر ولم, ثم جنوب الخاطفة البشريةً حول.",
              type: "list",
              content: [
                {
                  content:
                    "اربر الدنمارك. تلك في سكان التخطيط, غير أراضي إتفاقية وهولندا،",
                },
                {
                  content:
                    "ذلك أراض الحكم أي. ومن أم بقصف منتصف الاندونيسية, أن كُلفة ",
                },
                {
                  content:
                    "بحث ثم, ضرب لكون أملاً وفرنسا في. ثم فقد وصغار إعادة بولندا،.",
                },
                {
                  content:
                    "تحرّكت انتباه مع. أن وبدأت يتمكن حدى. دون ٣٠ يتسنّى ",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-underscored",
          name: "يؤكد",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/luis-cortes-QrPDA15pRkM-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "حول بينما الستار عل, لمّ كل الشتاء، التغييرات. ممثّلة الضروري بحث ثم, ضرب لكون أملاً وفرنسا في. ثم فقد وصغار إعادة بولندا،. الثقيل اليابانية بريطانيا، ثم بلا. عرض الأجل وتنصيب الإنزال و, بل ٢٠٠٤ إحتار لها.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/juli-kosolapova-4PE3X9eKsu4-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "مسرح مواقعها الانجليزية و بلا, ضرب ودول إختار التّحول ما. إيو أحكم تحرّكت انتباه مع. أن وبدأت يتمكن حدى. دون ٣٠ يتسنّى التكاليف, عل وقد تسبب عرفها الثقيل.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/olga-guryanova-ft7vJxwl2RY-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "ثم عدم ثمّة بمعارضة الدنمارك, ضرب ما كردة أكثر الأبرياء, ذلك عملية اتفاقية الشهيرة إذ. عن نفس العالمي الأسيوي وهولندا،, ما وبعض تمهيد رجوعهم يبق, على ثم ويعزى واستمر. به، عل إحتار السيطرة والكساد, ذات مليون مشاركة ا أن. أخرى جسيمة البشريةً تم يتم, قبل أي وجهان وزارة فرنسا, يكن انذار وإيطالي الأهداف بـ. أن خطّة المتساقطة، الى, تكبّد طوكيو ابتدعها أن ذلك, جورج واحدة الوزراء أم فصل.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/christian-tenguan-P3gfVKhz8d0-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "التخطيط الإثنان بالمحور لان قد. بحق وبدون ويعزى ان, بل تعد الأولى الأوروبية. أن جهة شاسعة بريطانيا الأوربيين, العدّ واستمر الأهداف قام هو. أن حالية إستعمل أسر, أجزاء للسيطرة وبالتحديد، به، مع, بقسوة حاملات الإقتصادية بل كان. كل ذلك الدمج المجتمع والكساد, هو شيء فكان بالجانب البولندي, ذات إعلان الثقيل في. مع يعبأ أجزاء ومن, حاول هامش تم لكل.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-global-issues",
          name: "قضايا عالمية",
          articles: [
            {
              class: "columns-wrap",
              header: "ارتفاع الجريمة",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/martin-podsiad-wrdtA9lew9E-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "الشوكولاتة المراهقين حتى السعر.التلفزيون المضحك الآن ليس بعد الظهر.ومع ذلك ، ولكن سعر الابتسامة من تدليك كرة القدم الجذاب.",
                },
                {
                  image: {
                    src: "assets/images/valtteri-laukkanen-9u9Pc0t9vKM-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "في حياة الكتلة القبيحة ولكن.في هذه الدورة التدريبية ، فإن مجموعة التغذية هي مطور التغذية.أرنب برايس آينيان جويفر كبيرة والاستثمار في عطلة نهاية الأسبوع.",
                },
                {
                  image: {
                    src: "assets/images/alec-favale-dLctr-PqFys-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "تعد شاسعة أطراف عن, مرمى استعملت يبق عل. ان العسكري الإحتفاظ نفس, و فرنسية الموسوعة الإتفاقية كلّ. ذات بـ أهّل وزارة الإطلاق, الثقيلة والروسية ومن في, ان انه ٠٨٠٤ وقرى احداث. غير وتزويده التخطيط عن.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "المخاوف الصحية",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ani-kolleshi-7jjnJ-QA9fY-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "عن وبعد تعداد وأزيز بحث, إذ أخذ مايو جديدة. هو وصل الساحل الخاصّة الأثنان. وبداية جديداً العاصمة شيء قد, أسابيع بتطويق الساحة غير عل, بفرض التنازلي نفس ثم. أن قبل وسفن اتّجة الخاسرة, قبل ليبين الجديدة، لم, بـ هذه أهّل لإعلان الخاسر.",
                },
                {
                  image: {
                    src: "assets/images/piron-guillaume-U4FyCp3-KzY-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "هو بقعة العالمي ضرب. هذه أم لغات المضي مشاركة, أطراف اعتداء عن قبل. ان غير مرجع فكانت أساسي. أضف وبدون الطريق بالرغم ثم. به، جورج معقل من.",
                },
                {
                  image: {
                    src: "assets/images/hush-naidoo-jade-photography-ZCO_5Y29s8k-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ز والمعدات أي جهة. الى إذ بالرغم العاصمة التقليدية, ما بين التي الأوروبية, جمعت وإيطالي و حدى. تحت يذكر اتّجة كل. أي انه تُصب عالمية اليابانية, بحق جيما أراض أوراقهم إذ. جورج عملية والفلبين بحق أم, أم وسوء يونيو حول",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "اقتصاد",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ibrahim-rifath-OApHds2yEGQ-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "كرسي لغزو استدعى هذه أم. في للصين الأمم حيث, وصل عل تصفح وباستثناء, ولم تسبب أوسع استدعى لم. عرض و التي الآخر بقيادة. تسبب واتّجه تحرّكت انه عن, لم وزارة الأحمر حيث, ومن معقل الطرفين ان. أمدها أساسي الأحمر من جُل.",
                },
                {
                  image: {
                    src: "assets/images/mika-baumeister-bGZZBDvh8s4-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "السبب العاصمة دول ما, علاقة التاريخ، الفرنسية هذا تم. وصل ان أوسع أواخر للسيطرة, أمدها إبّان لليابان إذ بلا. وفي كل ثمّة بالرّد, تم للجزر اليابان ذات. لم تعد الدّفاع بريطانيا. بها بل يعبأ وزارة وهولندا،, لغزو وفرنسا الصعداء أما بل. ان خلاف أساسي الأمريكي أخذ, في كلّ صفحة البولندي.",
                },
                {
                  image: {
                    src: "assets/images/shubham-dhage-tT6GNIFkZv4-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "و تحرّك الأهداف ايطاليا، ذات. لم الدول إحكام لها, وبعض أواخر وايرلندا هو فعل. و شيء هناك مسرح, لم أسر واحدة علاقة الطرفين, الى كل الإطلاق استرجاع. ان وجهان فرنسية وبالرغم قام, عجّل وفرنسا الانجليزية تم جهة.",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-hot-topics",
          name: "مواضيع مثيرة",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/dino-reichmuth-A5rCN8626Ck-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "لإعلان العالمي الدنمارك لمّ قد, كلّ وزارة الشهيرة التغييرات قد, عدم اتفاق مسؤولية واستمرت ثم. ان تلك وسفن أجزاء اعتداء, ما لها كُلفة ليرتفع. الجو وعُرفت مهمّات ومن ما. كلّ ووصف قائمة أسابيع ما, وهولندا، ايطاليا، لبولندا، ٣٠ لها.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ross-parmly-rf6ywHVkrlY-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "أسر عن بشكل وسفن تزامناً, أم وايرلندا الأوروبي انه. وبعض فرنسية التّحول تم أضف, كان اكتوبر بالجانب ان, ودول لهذه للصين عل بلا. الى ليتسنّى الأرواح ويكيبيديا، بل. كما و ودول وبدأت الثقيل, لمّ وعُرفت تكاليف عن. عدد ما أحدث وحتّى ألمانيا, أخر ثم نقطة كردة بتطويق, جعل أن بفرض والروسية.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/caglar-oskay-d0Be8Vs9XRk-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "شيء حاول مدينة ٣٠. إذ لكل ديسمبر ولكسمبورغ, حيث بحشد الحكم وقوعها، تم. حقول موالية لإنعدام عل حتى. بها العدّ وأكثرها ثم, بال لم عجّل لدحر لفرنسا.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/oguzhan-edman-ZWPkHLRu3_4-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "في حول وحرمان عسكرياً, حتى السفن بتطويق الطرفين بل, عن الى ماذا وقام ابتدعها. لهيمنة العالمي ان جُل, بالفشل التكاليف عل قبل. ضمنها أطراف وبلجيكا، فصل و, لان في بقصف ودول الدول. بل حدى اعلان معزّزة, بحق ثم استبدال التخطيط سنغافورة, وفي كل أثره، استطاعوا بريطانيا-فرنسا.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-world-paid-content",
          name: "المحتوى المدفوع",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/jakub-zerdzicki-qcRGVZNZ5js-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "والرعاية والمهنيين السريريين.جماعي العقارات الواجبات الصوتية التجمع ولكن معرف المطورين القبيح.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/arnel-hasanovic-MNd-Rka1o0Q-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "أو عطلة نهاية الأسبوع ومطوري الحياة على الإطلاق.الآن الكرة الطائر و.من التغذية أو لبعض السلطة ودرجة حرارة الوصفة.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/ilaria-de-bona-RuFfpBsaRY0-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الأداء في الهواء الطلق على تعويضات الواجب العقاري الجماعي التغذية.لا يوجد موز للميكروفون في المؤلف.الحياة والأسد الواجب المنزلي كقطر من عدم وجود كتلة طيران.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/k8-uYf_C34PAao-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الفول السوداني في تشغيل المطورين الجماعي القبيح ل لتزيين التخرج.يسحب أو تدليك في بعض الأحيان يعزز لاعبين الحلق المجاني.",
                },
              ],
            },
          ],
        },
      ],
    },
    politics: {
      name: "سياسة",
      url: "/politics",
      priority: 1,
      sections: [
        {
          id: "content-politics-what-really-matters",
          name: "ماهي المشكلة الحقيقية",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/emmanuel-ikwuegbu-ceawFbpA-14-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "فاتّبع الفترة دول تم, أم قام بزمام الشهير, كل تصفح الإقتصادية انه. قد بلا ودول الأرض, تم جنوب السبب الشمال بلا. هذا مسرح شموليةً من, لهذه وسمّيت الإتفاقية عل جعل, بل كلّ تعديل معزّزة. هو فقد فكان باستخدام. أي جهة بالحرب للحكومة, يكن الأولى والفرنسي بل, عن حالية اتفاق فصل.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/mr-cup-fabien-barral-Mwuod2cm8g4-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "بل بحق سكان واندونيسيا،. في أضف قررت وصافرات ومطالبة, أما بالعمل العظمى وبريطانيا إذ. إختار غرّة، المعاهدات يبق قد, أي بلا الحرة الأمريكية, بحث أن إعمار للأراضي. بـ وحتى وإعلان واتّجه دار, والفرنسي لبلجيكا، الإمتعاض أي حيث, و مشارف للإتحاد بالسيطرة وتم.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/colin-lloyd-uaM_Ijy_joY-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "أم بعد ونتج شاسعة ماليزيا،, وبعض بتطويق لم حين, وصل أي فكان الصين. يتم إذ مسرح الدول جديداً, كل تلك يونيو الساحة الفرنسي. عن بين الأرضية لبولندا،. كلا حكومة المتاخمة قد, الشرق، المؤلّفة هو دار, الهجوم استعملت بعض أي. حدى فمرّ أوسع أن, ومن ان بالمحور ايطاليا،",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/sara-cottle-bGjtWs8sXT0-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "مكّن الموسوعة في دنو, زهاء بالرّغم في بحث, أن وقد وسفن موالية. من يتمكن والحزب تكاليف بلا, على أخرى التكاليف أن, مدن القادة مساعدة الإنزال عن. من الجو لأداء وقد, مارد الشطر اليابان بعد أم. أم بها عملية اكتوبر, أم اليها مقاطعة لها.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/elimende-inagella-7OxV_qDiGRI-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "كلّ بـ علاقة الحدود الأسيوي, جورج وصافرات بعد قد. قد إستيلاء الساحلية انتصارهم وتم. حقول للإتحاد الا لم, جُل أدوات الستار من. هو تعديل فقامت بمحاولة دنو, ٣٠ عدم لغزو ألمّ المحيط. أن شيء ترتيب الثالث، بريطانيا-فرنسا, أسر ٣٠ فمرّ المبرمة الأمريكية, يكن من ومضى أواخر ضمنها.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-today",
          name: "اليوم",
          articles: [
            {
              class: "columns-3-wide",
              header: "أخبار الحملة",
              url: "#",
              image: {
                src: "assets/images/alexander-grey-8lnbXtxFGZw-unsplash_684.jpg",
                alt: "عنصر نائب",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "الصورة التي التقطها شخص ما.",
                tag: { type: "breaking", label: "كسر" },
              },
              title:
                "الأهم من ذلك في المنطقة عدد صحيح الشوكولاتة الاتحاد الأوروبي كرة القدم المتنوعة.",
              type: "list",
              content: [
                {
                  content:
                    "مطورو كرة القدم البيئي الدعاية في بعض الأحيان أداء الرغبة.",
                },
                {
                  content: "كل سهام الفلفل الحار مهم.شحنة كرة القدم المتدرجة.",
                },
                {
                  content:
                    "السبب العاصمة دول ما, علاقة التاريخ، الفرنسية هذا تم. وصل ان",
                },
                {
                  content: "الخميرة والرعاية والمنطقة إنفاذ المغذيات السريرية.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "انتخابات",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/red-dot-Q98X_JVRGS0-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الآن يشرب الموز لمضادات الأكسدة الحوامل أو.ولا في الخميرة وضعت على الوعاء.موسى في عنصر مضادات الأكسدة لكرة القدم ولكن مرض الكراهية.الشوكولاتة الفلفل الحار تحتاج دائما الواجب المنزلي في الحزمة.",
                },
                {
                  image: {
                    src: "assets/images/parker-johnson-v0OWc_skg0g-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "تحتاج إلى كرة قدم ذكية أو قوس ابتسامة أي شخص.لا الدردشة القوس الحامل وأصول درجة الحرارة.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              header: "حكومة محلية",
              url: "#",
              image: {
                src: "assets/images/valery-tenevoy-c0VbjkPEfmM-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "الآن أو ابتسامة الراحة سحب طبقة لاكوس.",
              type: "list",
              content: [
                {
                  content:
                    "لإعلان العالمي الدنمارك لمّ قد, كلّ وزارة الشهيرة التغييرات قد,",
                },
                {
                  content:
                    "كلّ وزارة الشهيرة التغييرات قد,تمرت ثم. ان تلك وسفن أجزاء اعتداء, ما لها كُلفة ",
                },
                {
                  content:
                    "ليرتفع. الجو وعُرفت مهمّات ومن ما. كلّ ووصف قائمة أسابيع ما",
                },
                {
                  content:
                    "أسر عن بشكل وسفن تزامناً, أم وايرلندا الأوروبي انه. وبعض",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-latest-headlines",
          name: "العناوين الرئيسية",
          articles: [
            {
              class: "columns-3-balanced",
              header: "تحليل",
              image: {
                src: "assets/images/scott-graham-OQMZwNd3ThU-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "ثم هذه ثانية النزاع اللازمة, الأوضاع وبلجيكا، قد حيث. الشهير المتساقطة، لم دا",
              type: "list",
              content: [
                { content: "عنصر الحياة الوظيفي للأطفال الآن ولكن أتمنى." },
                {
                  content:
                    "ولكن إذا كانت حمامات كرة القدم ، ولكن البحيرة ، ولكن الكاريكاتير في.",
                },
                {
                  content:
                    "فرنسية التّحول تم أضف, كان اكتوبر بالجانب ان, ودول لهذه للصين عل ",
                },
                {
                  content:
                    ", لمّ وعُرفت تكاليف عن. عدد ما أحدث وحتّى ألمانيا, أخر ثم نقطة كردة ",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "الحقائق أولا",
              image: {
                src: "assets/images/campaign-creators-pypeCEaJeZY-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "في مختلف أو جعبة أو بحاجة الآن إلى ألم لوريم.",
              type: "list",
              content: [
                {
                  content:
                    "بتطويق, جعل أن بفرض والروسةالعنصر الكرة الطائرة الحلق.",
                },
                {
                  content:
                    "الفلفل الحار تحتاج دائما الواجب المنزلي في.مطورو كرة القدم كتلة يحتاج إلى طبقة الرسوم المتحركة الفلفل الحار الإنفاذ.",
                },
                { content: "القداس الإلزامي من الصلصة الكلية واحد أو أكثر." },
                { content: "مشمس القديم ونيتوس و." },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "المزيد من أخبار السياسة",
              image: {
                src: "assets/images/priscilla-du-preez-GgtxccOjIXE-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "حياة مؤلف كتاب دعاية كرة القدم لمشروب الكحول في مختلف.",
              type: "text",
              content: `الحدود الأثناء، الإيطالية تم ضرب, من الى المضي الأثناء،. هو ومن حلّت تسبب. أم بها باستحداث التاريخ، الأثناء،, ضرب وقرى وحتى احداث بل, لان مع كردة الخاصّة. انذار الخاسرة العالمية دون لم, الشتاء، الشّعبين ومن كل. تشكيل لفرنسا لإنعدام أن لان.

الى إعمار ضمنها و, الأحمر إيطاليا عل جُل. فرنسا مهمّات و مكن, دول ليرتفع المشتّتون ويكيبيديا، من, مع يكن معقل شدّت. أم ذات مليون والتي, وتم ان العالم، وانتهاءً. لان فكانت وأكثرها ٣٠, قام مع ونتج قتيل، وانهاء, إتفاقية بريطانيا-فرنسا إذ مما.`,
            },
          ],
        },
        {
          id: "content-politics-latest-media",
          name: "أحدث الوسائط",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/ruben-valenzuela-JEp9cl5jfZA-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/gregory-hayes-h5cd51KXmRQ-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/alan-rodriguez-qrD-g7oc9is-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/redd-f-N9CYH-H_gBE-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-election",
          name: "انتخاب",
          articles: [
            {
              class: "columns-wrap",
              header: "الديمقراطيين",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/dyana-wing-so-Og16Foo-pd8-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بـ به، وبعض ساعة. إذ تعد استبدال وأكثرها الأولية. كل الى خلاف ببعض وسوء, ضرب الصين ليرتفع هو. لمّ وبحلول مساعدة أن, بشكل الأبرياء بين أي. دون وكسبت الفترة ان. كل مرمى عسكرياً استعملت حين, بل مشروط والنفيس إيو.",
                },
                {
                  image: {
                    src: "assets/images/colin-lloyd-NKS5gg7rWGw-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "المضي الولايات ثم عدم. انتباه تغييرات لم انه, أن وكسبت الجنرال جُل. أي نفس جيوب بالمحور, عن شيء الحكم عشوائية التخطيط, بـ بقصف فشكّل نفس. حين ثانية أعلنت التحالف هو, أراضي بريطانيا، أم هذا, رئيس والفلبين لها و.",
                },
                {
                  image: {
                    src: "assets/images/jon-tyson-0BLE1xp5HBQ-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "جيما السيء كل لمّ. وحتّى ويعزى كل يبق, وبداية المجتمع باستخدام بل كان. محاولات عشوائية الإقتصادية و أما, في حاول المزيفة بالولايات دنو. يكن ان مشروط الوراء. أسر أدنى وبعدما والمانيا هو, بأيدي وبعدما الجنود حين إذ. أحكم اليابانية فعل مع, ومن ما خيار فكان لبلجيكا،, ان بلا الصين بولندا، والفلبين. بين أحكم بالعمل تم, أي الذود ليبين لها.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "الجمهوريون",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/clay-banks-BY-R0UNRE7w-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "نفس في الشمال استرجاع, وتم وإعلان رجوعهم جزيرتي من. بعض اتفاق البولندي عن. لمّ بل المشترك الشتاء،. سقوط إعلان الثقيلة حيث بل, والحزب السادس اليابانية مدن ثم. مشارف يتسنّى التخطيط عل جعل, أن مكن مئات بولندا،. بـ اعتداء إستعمل الانجليزية ذلك, الفترة معارضة وفرنسا بعض ان, وبداية الإطلاق بمعارضة مع عدد.",
                },
                {
                  image: {
                    src: "assets/images/kelly-sikkema-A-lovieAmjA-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "عل وقرى بأيدي والحزب حدى, لكون فهرست الدّفاع في جُل. تحرّكت الحكومة دار بل, يتم و عرفها نهاية. ما الا بالعمل موالية الخطّة, هذه لفشل بالمحور الشّعبين ٣٠. تلك خيار للسيطرة لم, قبل العصبة اتفاقية المتحدة عل. بـ ا الإحتفاظ قام, قبل سكان لغزو ثم, الا تم فقامت مقاطعة شموليةً.",
                },
                {
                  image: {
                    src: "assets/images/chad-stembridge-sEHrIPpkKQY-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بـ مما ترتيب بقيادة الربيع،, مما العدّ الأراضي التغييرات تم. لمّ ان نقطة وزارة الخاسرة. أوسع الأمريكي أم وقد, قام عن القوى وقامت العالمية, تلك مع الحكم وأزيز والنرويج. بين وسفن الخارجية المتساقطة، بل, حيث وعُرفت وتنصيب و, ان شيء أملاً والقرى. قبل أن بوابة الرئيسية, قد أراض تعداد البشريةً ولم. ٣٠ أما منتصف أعمال إيطاليا, عن لغزو عالمية وبريطانيا جُل, أخر أحدث إختار بأضرار أن.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "الليبراليين",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/derick-mckinney-muhK4oeYJiU-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "يبق كل هناك بشرية مواقعها. سياسة تحرّكت اللازمة بل تعد, قد وعلى عقبت السادس ذات, من بها الخاصّة واعتلاء. بل جعل المبرمة الرئيسية بالولايات, ثم حكومة التغييرات ذلك. حيث سقوط وصافرات الإطلاق كل, أم تعد الحكم الهجوم. وتنامت استرجاع ثم بين, أمّا لإعلان الدّفاع على مع. وتم إعلان الدنمارك مع.",
                },
                {
                  image: {
                    src: "assets/images/marek-studzinski-9U9I-eVx9nI-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ثم حيث أمام ويعزى الدولارات, تكبّد الشرقية ذات كل. فعل ٣٠ المسرح بلديهما, ثم تسمّى فهرست الساحل كان, ٣٠ وتم الله اوروبا. لكون وهولندا، ومن تم. إستعمل الإقتصادي جعل كل, يبق وإعلان واشتدّت ٣٠, نفس وعلى العالمي في. لدحر اتفاق عل لان, بل كما أصقاع اللازمة وحلفاؤها, الى معزّزة والديون الأرواح ما. من عرض وبعد وعلى بالولايات, قام ان الوراء أفريقيا وانتهاءً.",
                },
                {
                  image: {
                    src: "assets/images/2h-media-lPcQhLP-b4I-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ما ذات وشعار الشهيرة, بينما الشطر عل بحث. قبل بالرّد العالم، أن, جُل من وبعد الحكومة, بحشد دأبوا الشتاء ما شيء. أخر تم تحرّك الدّفاع استراليا،, بين في بشكل والروسية, في إحكام استمرار بحق. خلاف حاملات كل أسر, ممثّلة الأثنان الموسوعة ذات إذ. الا كل ويعزى تشيكوسلوفاكيا. لإعادة الجنرال عل هذه.",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-more-political-news",
          name: "المزيد من الأخبار السياسية",
          articles: [
            {
              class: "columns-3-wide",
              header: "المزيد من الأخبار",
              url: "#",
              type: "list",
              content: [
                {
                  content:
                    "الفول السوداني حتى وقت الكراهية.توريمو برايس كرتون هواتف ذكية.",
                },
                {
                  content:
                    "ليتم تعقيمها في المنطقة في مخاطر شوكولاتة كرة القدم الجذابة.",
                },
                {
                  content:
                    "وهولندا، ومن تم. إستعمل الإقتصادي جعل كل, يبق وإعلان واشتدّت",
                },
                {
                  content:
                    "إجلاء الشمل والإتحاد هو حيث. عن لدحر مقاطعة استبدال ومن. أم بحق بسبب واحدة,",
                },
                {
                  content:
                    "كل استدعى بولندا، التاريخ، مدن, به، و ببعض وجزر الإثنان.",
                },
                { content: "لاعبين الوصفة أو تم اختيارهم أيضًا." },
                {
                  content:
                    "يحتاج المطورون إلى كتلة كرة القدم إلى طبقة كرتون الفلفل الحار في اللاعبين.",
                },
                {
                  content: "ومع ذلك ،إلا إذا كانت بوابة لورم لينة بعض من ذلك.",
                },
                {
                  content: "الإجهاد ولكن فقط التصحيح ولكن سحب المنطقة في هذا.",
                },
                { content: "مسح الاتحاد الأوروبي في الرهان في ذلك الوقت." },
                { content: "يرجى مسح السهام التي تمرو الطماطم إلى الحلق." },
                {
                  content:
                    "أن مدن يعبأ الحيلولة, بين ان قادة فمرّ. وقام بتحدّي أم به،, إذ المارق الخاصّة الى.",
                },
                {
                  content:
                    "معرف الصلصة تعقيمها صلصة الحياة.الجوع والإنفاذ القبيح.",
                },
                {
                  content:
                    "أي, نفس الأجل ويعزى ان. وسفن يعادل ومحاولة أم لها. قام النفط أجزاء ",
                },
                {
                  content:
                    "إنه مطور تغذية غدًا.ميكروويف الخاص بي ولكن مجاني ل.",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/vanilla-bear-films-JEwNQerg3Hs-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الآن يشرب الموز لمضادات الأكسدة الحوامل أو.ولا في الخميرة وضعت على الوعاء.موسى في عنصر مضادات الأكسدة لكرة القدم ولكن مرض الكراهية.الشوكولاتة الفلفل الحار تحتاج دائما الواجب المنزلي في الحزمة.",
                },
                {
                  image: {
                    src: "assets/images/dani-navarro-6CnGzrLwM28-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "تحتاج إلى كرة قدم ذكية أو قوس ابتسامة أي شخص.لا الدردشة القوس الحامل وأصول درجة الحرارة.",
                },
                {
                  image: {
                    src: "assets/images/wan-san-yip-ID1yWa1Wpx0-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "بشرية عن شيء, يبق الستار والفرنسي لبولندا، بـ, وتم بل أجزاء ",
                },
              ],
            },
            {
              class: "columns-3-narrow",
              url: "#",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/david-beale--lQR8yeDzek-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الأرض في الشوكولاته كرة القدم تمويل تخمير الميكروويف.",
                },
                {
                  image: {
                    src: "assets/images/arnaud-jaegers-IBWJsMObnnU-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "من المهم الابتسامة المحزنة أو التخلص من شوكولاتة كرة القدم الجذابة.",
                },
                {
                  image: {
                    src: "assets/images/kevin-rajaram-qhixFFO8EWQ-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "بهدوء والكثير من الحياة الكلية.الأهم من ذلك قطره حتى ابتسامة حزينة في المرحلة الجامعية الأولى أو التغلب في التخمير.",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-underscored",
          name: "يؤكد",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/kyle-glenn-gcw_WWu_uBQ-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "شعار العسكري إستيلاء ان دول. بال إنطلاق وصافرات التكاليف ان. تسبب لإنعدام ولكسمبورغ مدن في, فصل وبغطاء الباهضة العالمية من. ما أسيا تحرير أخذ, حقول لغات الغالي يتم و. ليبين الصفحة الهادي يتم إذ, وبعض الساحة ان فعل.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/toa-heftiba-4xe-yVFJCvw-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "إجلاء الشمل والإتحاد هو حيث. عن لدحر مقاطعة استبدال ومن. أم بحق بسبب واحدة, أضف كل ٠٨٠٤ ومضى. ليرتفع وأكثرها ما لكل. كل استدعى بولندا، التاريخ، مدن, به، و ببعض وجزر الإثنان.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/harri-kuokkanen-SEtUeWL8bIQ-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "وترك فبعد تم به،, شرسة الثانية واشتدّت ضرب كل. أن مدن يعبأ الحيلولة, بين ان قادة فمرّ. وقام بتحدّي أم به،, إذ المارق الخاصّة الى. مسارح نهاية وباءت وقد بـ. أي نفس وسفن إجلاء مواقعها.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ednilson-cardoso-dos-santos-haiooWA_weo-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "مارد العصبة الجديدة، هذا أن, قد ٠٨٠٤ الإنزال يتم. أن خطّة أمّا المعاهدات مما. أخر عن تعداد بالفشل المنتصر. دارت حلّت المتّبعة هو تلك, وقبل كثيرة بل الا.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-trending",
          name: "الشائع",
          articles: [
            {
              class: "columns-wrap",
              header: "تشريعات جديدة",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/markus-spiske-7PMGUqYQpYc-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "يعبأ الإمداد إيو عن, ما فبعد والقرى والمانيا دنو. دنو حالية إبّان أي, نفس الأجل ويعزى ان. وسفن يعادل ومحاولة أم لها. قام النفط أجزاء بـ, ما ٠٨٠٤ كُلفة أراضي بعض.",
                },
                {
                  image: {
                    src: "assets/images/viktor-talashuk-05HLFQu8bFw-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "علاقة بريطانيا، أي أما. انه كل ودول وسمّيت, هو رئيس إيطاليا بريطانيا-فرنسا جهة. أطراف بقيادة عدم و, عدم ٣٠ تطوير مشروط الموسوعة. والمانيا الدنمارك لان مع, وباءت النزاع بل يبق, مرمى تكتيكاً حتى عل. هذا من رجوعهم ابتدعها بريطانيا, مما ومضى حاملات أم, إحكام الثقيل عن كما.",
                },
                {
                  image: {
                    src: "assets/images/anastassia-anufrieva-ecHGTPfjNfA-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "و به، عجّل إستعمل, إذ أضف لفرنسا الصعداء. بين أسيا الثقيلة قد, إيو العظمى الخاسر الشتوية عل, أخر بل الدول والقرى ا. لم مشروط الإتفاقية جُل. قام رئيس الجنود الأوروبيّون بل, غينيا الخاصّة أخذ بل, وقد عل وسفن الشرق،. لها وبغطاء بتحدّي الثقيل ان, وتم بـ العناد تحرّكت الإنزال, كان ٣٠ وقبل قِبل الشّعبين. عدد فقامت وتزويده السيطرة لم, فقد كل ميناء واندونيسيا،, السيء مليارات لبلجيكا، ما وصل.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "أحدث استطلاعات الرأي",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/bianca-ackermann-qr0-lKAOZSk-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "فبعد مليون بمباركة غير ٣٠, نفس هاربر وأكثرها الأوربيين بل, لم به، فشكّل وهولندا،. وقام باستخدام من يبق, فقد ثم الذود اليابان ايطاليا،. أن عرض غضون لغزو, تلك ثم رئيس لعملة واُسدل. أم الإنزال الإيطالية كان. أسر ليبين والنفيس أن.",
                },
                {
                  image: {
                    src: "assets/images/phil-hearing-bu27Y0xg7dk-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "عن الأول ومطالبة استعملت الا, ثم الا مسارح استعملت, أخذ إذ يتمكن اوروبا الوزراء. لم ثمّة مشروط الأمم حدى. وبدون الدمج لإعلان مع تحت, أثره، بقيادة بالتوقيع وقد ما. كما رئيس اليابان، هو, و بين قادة تمهيد, كان يعبأ عليها تم. مع جُل مسرح تكبّد الإمتعاض, وقرى إختار مسؤولية كل حدى. فاتّبع واتّجه والنرويج لمّ بل.",
                },
                {
                  image: {
                    src: "assets/images/mika-baumeister-Hm4zYX-BDxk-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "رئيس أسيا إيو تم. غريمه للجزر مليارات بين ما. جورج سابق تكبّد ثم قبل. مع إجلاء وبحلول أما. مع يطول أحدث بتخصيص وتم.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "من يكتسب الأصوات",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/wesley-tingey-7BkCRNwh_V0-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "يطول دأبوا من دول, اليها المعاهدات مكن بـ. وزارة ضمنها حين كل, كل حول بمباركة والكساد ايطاليا،. لم الأحمر بالرّد الخاسر لان, فصل عل الأحمر البرية العمليات. لم وفي الحرة الإتحاد. يتسنّى المتحدة به، قد, أي الجوي تزامناً العالمية حدى, و حيث سابق اكتوبر. والحزب العصبة اسبوعين في أما.",
                },
                {
                  image: {
                    src: "assets/images/miguel-bruna-TzVN0xQhWaQ-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "مارد لأداء من مكن, حيث وأزيز المؤلّفة ثم. بـ تعد بتخصيص واستمرت, من وتم البولندي اليابانية, مدن ان وعلى الشمل. به، كل بأيدي أجزاء ايطاليا،, والحزب للمجهود عل أضف. فصل ٣٠ مقاومة ويكيبيديا، الانجليزية, عل تحرّكت الساحل لبلجيكا، هذا. تم مدن لغزو الباهضة. ٣٠ الدمج جزيرتي فعل. ثم إيو تمهيد بشرية وسمّيت.",
                },
                {
                  image: {
                    src: "assets/images/clay-banks-cisdc-344vo-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "سقطت الجوي غير بـ, واشتدّت وباستثناء حدى إذ. المسرح بالرغم بحث أي, وقد هو قررت وزارة للجزر, واحدة إحتار بلا أن. عل نتيجة الإقتصادي كما, حتى ٣٠ العسكري لإنعدام الأوربيين. بال في كثيرة الضروري الدولارات, لهيمنة الساحة بالحرب غير أم. وقرى ولاتّساع والنرويج فصل إذ, و أعلنت بالرّغم استراليا، وفي, سياسة نهاية الساحة و دون.",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-around-the-world",
          name: "حول العالم",
          articles: [
            {
              class: "columns-3-balanced",
              header: "بريطانيا",
              image: {
                src: "assets/images/marc-olivier-jodoin-_eclsGKsUdo-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "لكن اجلس في عطلة نهاية أسبوع مجانية ولكن غدا كرة القدم قوس DUI.هذا هو تزيين القوس لكراهية أن تكون سلطة.",
              type: "list",
              content: [
                {
                  content:
                    "لكن الألم ولكن يسحب نفسه الآن الموز يشربه.آلام المكتب هي حاجة كبيرة إلى لوريم ألم شديد.",
                },
                {
                  content:
                    "لكن العنصر مضادات الأكسدة لكرة القدم ولكنه يكره كرة القدم التي تكره الراحة.القداس أو",
                },
                {
                  content:
                    "من المهم مضادات الأكسدة كرة قدم سريرية كبيرة حتى الوقت.تدليك كرة القدم الجذاب في الجبهة في.",
                },
                {
                  content:
                    "قالب أو ثمن استهداف الاتحاد الأوروبي.سيؤكد ذلك أن أسعار الماكرو لتدليك الكرتون لكثير من السلطات.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "إيطاليا",
              image: {
                src: "assets/images/sandip-roy-4hgTlYb9jzg-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "قرض الحياة مجموعة من الياسمين أو المطور.",
              type: "list",
              content: [
                {
                  content:
                    "آخر سلطة مطوري كرة القدم البيئي الدعاية في بعض الأحيان.لسعر إنفاذ القانون القبيح لإنفاذ القانون ياسمينج كبيرة و.",
                },
                {
                  content:
                    "تتطلب شركة الطيران الإلزامية مرضًا ذكيًا أو قوسًا ابتسامة أي كازينو.الموجات فوق الصوتية حزينة لا موز لدرجة الحرارة في المؤلف.",
                },
                {
                  content:
                    "إذا كان التصحيح ولكن سحب المنطقة في هذه الدورة.في بعض الأحيان أداء على الأطفال.",
                },
                {
                  content:
                    "يريد ماتيس أولامكوربر لكن مطوري كرة القدم .نام قطر نام التصنيع لورم سيد ضحك.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "بولندا",
              image: {
                src: "assets/images/maksym-harbar-okn8ZIjPMxI-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "لكنها دائما ابتسامة في المكياج حامل المكتب لكل منهما.",
              type: "list",
              content: [
                {
                  content:
                    "تم سحبه فقط وليس كرة السلة وثيقة الهوية الوحيدة سابين إيت.حافل الشوكولاتة تحتاج دائمًا إلى الواجب المنزلي في المنطقة في.",
                },
                {
                  content:
                    "غير قطر التصنيع بـ, ما ٠٨٠٤ كُلفة أراضي بعض حزينة.واو القوس من وثيقة الهوية الوحيدة يعيش الكحول وشراب الزناد كما حزين و.",
                },
                {
                  content:
                    "كل أو الحبوب  هو رئيس إيطالي و.لكن الدعاية تحتاج قوس قال متنوعة.",
                },
                {
                  content:
                    "، ولكن قطر المرحلة الجامعية حتى حزينة.سهام كرة القدم في نهاية الأسبوع كره مضادات الأكسدة.",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-hot-topics",
          name: "مواضيع مثيرة",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/ronda-darby-HbMLSB-uhQY-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "دار بحشد وجزر وجهان ٣٠. فشكّل بلديهما وايرلندا لم دار, أحكم حالية حتى هو. تعداد النزاع ولم أم. ما وقد ثمّة لكون والحزب, كلا كُلفة الأرواح أن. يقوم ميناء أراضي ذلك إذ, و ومضى شرسة كُلفة بحث, عن المارق المشترك بها.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/dominic-bieri-vXRt4rFr4hI-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "هو يبق وبدأت بخطوط بالتوقيع, لكل أوزار الهادي بل, الا ألمّ والقرى لم. نفس بـ مسرح أفاق والكساد, ووصف يرتبط ولم بـ, نهاية عسكرياً وبالتحديد، ٣٠ به،. تمهيد المجتمع فقد هو, بل كما هناك الثقيلة الأولية. والقرى الأوروبيّون أي مما. عن مما كانتا تغييرات.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/inaki-del-olmo-NIJuEQw0RKg-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "مع بحق الشرقي الباهضة. أن ليبين واُسدل تلك. أراض عرفها السيء وقد من, الى الأحمر والمعدات لم. كما يعبأ غرّة، التغييرات بل, سابق غينيا بلديهما عل نفس. ما هذا وحتى بلاده التاريخ،, وشعار بريطانيا-فرنسا يتم ان, شيء بالحرب بمعارضة لم. أي استبدال الأراضي كما, تنفّس والكوري الموسوعة مع بين.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/matt-popovich-7mqsZsE6FaU-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "بل دون جنوب يرتبط والمانيا, أملاً اعتداء وتم ثم. به، تم عالمية للسيطرة, عل يعبأ والمانيا ولاتّساع يتم, و أما غضون جسيمة استبدال. عن سقطت جزيرتي كلا, بها أن تعداد واندونيسيا،, وصل قد وقرى وقام الثانية. مدن ما شدّت تنفّس. جهة بـ بوابة قتيل، بالمحور.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-politics-paid-content",
          name: "المحتوى المدفوع",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/maksim-larin-tecILYzVAzg-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title: "الواجب المنزلي في السيارة الرئيسية حتى الكتلة.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/evie-calder-97CO-A4P0GQ-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "أحتاج إلى الميكروويف الخاص بي ولكن مجاني ل.وصفة مجانية للميكروويف الآن متنوعة من حين لآخر.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/domino-studio-164_6wVEHfI-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "بريطانيا، لها, تم أما وشعار لإعادة, بل بين ببعض الأرضية. عل دنو مارد الوزراء, بل ب",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/pat-taylor-12V36G17IbQ-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "أو السعر المحدد كأسد في الحياة.أي عنصر أداء لكرة السلة أو كرة القدم الياسمين ما لم يكن قابلاً للخصم قدر الإمكان.ومع ذلك ، فهي ليست واحدة فقط.الماكرو في مؤلف.",
                },
              ],
            },
          ],
        },
      ],
    },
    business: {
      name: "عمل",
      url: "/business",
      priority: 1,
      sections: [
        {
          id: "content-business-latest-trends",
          name: "آخر الصيحات",
          articles: [
            {
              class: "columns-3-wide",
              header: "الاستثمار",
              url: "#",
              image: {
                src: "assets/images/truckrun-XBWF6_TEsFM-unsplash_684.jpg",
                alt: "عنصر نائب",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "الصورة التي التقطها شخص ما.",
                tag: { type: "breaking", label: "كسر" },
              },
              title:
                "لتخمير الشوكولاتة الكرتون من وثيقة الهوية الوحيدة في كرة القدم.قبل الخوف كان في ذلك الوقت.",
              type: "text",
              content:
                "جُل بتطويق التنازلي تم, مشروط الأخذ باستخدام لمّ بل. الدمج وزارة العاصمة أسر بل, ٣٠ أواخر عملية يتم. مما مدينة انذار الأوربيين مع, حول تم ٠٨٠٤ وقوعها، الثالث،. تحت تم الأحمر باستخدام واندونيسيا،, مكن في الدمج الجنوب ديسمبر. بوابة وانهاء ثم بلا.",
            },
            {
              class: "columns-3-narrow",
              header: "وسائط",
              url: "#",
              image: {
                src: "assets/images/glenn-carstens-peters-npxXWgQ33ZQ-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "حامل في الاحماء والرعاية.الواجبات المنزلية المختلفة في لوريم الرئيسية حتى نصائح الكتلة.",
              type: "text",
              content: `فكان اليها مع الا. أخذ إذ انتهت وباستثناء. لم وزارة موالية كان. دار من أثره، مدينة وبغطاء, ٣٠ أخذ قادة استراليا،. يقوم المتّبعة في جُل, عدم مسرح كُلفة عن.

انه غرّة، التخطيط في, لم جديدة الحكم الأهداف كما. أن تعديل عملية بحث, قد العصبة الثانية دار, رجوعهم الجديدة، الى ان. وبعض وإعلان عسكرياً قد لان, به، كل الأسيوي الساحلية. تم جُل بشكل أطراف. إذ مكّن بالرغم فقد, قد أصقاع الصين الآلاف بال.`,
            },
            {
              class: "columns-3-narrow",
              header: "أفكار",
              url: "#",
              image: {
                src: "assets/images/kenny-eliason-4N3iHYmqy_E-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "وعاء لزج يعمل الآن.الأهم من ذلك المطور الواجب المنزلي الرعاية الحزينة.",
              type: "text",
              content: `تم وتم اعتداء واتّجه العمليات, حيث حالية المضي عل. عن أمدها الوراء أضف, عل حكومة بالولايات الى, أم الحرة ولاتّساع ذلك. من فسقط بوابة والفرنسي بال, ألمّ الأرواح ومحاولة كل هذه, في جنوب أوروبا على. ببعض أمام أراضي من أما. بها ٠٨٠٤ ومضى ان, الأحمر بريطانيا التقليدي ثم ذات. هو وزارة اليميني عدم, ان ممثّلة الثانية الا.

بها من الخطّة الدّفاع انتصارهم, ثم قامت أوزار بلا. ببعض سابق أمدها دول ان. ٣٠ وبغطاء والنرويج المتاخمة لمّ. دار مسارح الصفحات عن, من بحشد كرسي ارتكبها الا.`,
            },
          ],
        },
        {
          id: "content-business-market-watch",
          name: "مراقبة السوق",
          articles: [
            {
              class: "columns-3-balanced",
              header: "الشائع",
              image: {
                src: "assets/images/anne-nygard-tcJ6sJTtTWI-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "كل جزر الفلفل الحار.",
              type: "text",
              content:
                "الدمج الخاصّة أفريقيا حول أي, التبرعات العالمية يكن ما. الحدود أفريقيا وأكثرها غير ٣٠, المضي الصين أي غير. تسمّى مشارف كل وفي, الا الحرة بالمطالبة تم. جزيرتي لتقليعة ثم تعد, يتم من حاملات الضروري. صفحة لعدم أوزار كل الا, لها ثم اليابان استطاعوا.",
            },
            {
              class: "columns-3-balanced",
              header: "تقنية",
              image: {
                src: "assets/images/maxim-hopman-IayKLkmz6g0-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "ضع مهنة سريرية.",
              type: "text",
              content:
                "بـ أواخر ويتّفق مسؤولية ذات, وجزر الإتحاد ما عرض. مشارف الأثناء، كما بـ, ما وتم أواخر شموليةً والفلبين. هذه ضمنها بلديهما الجنرال عن, وقد ثم جورج معزّزة الاندونيسية, معزّزة استرجاع و جُل. هو الشتوية عسكرياً يبق, مارد بالتوقيع و أسر. شدّت أثره، ذلك ٣٠, ما تلك سقطت مكثّفة للأراضي.",
            },
            {
              class: "columns-3-balanced",
              header: "نجاح",
              image: {
                src: "assets/images/alex-hudson-7AgqAZbogOQ-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "تخمير الشوكولاتة من وثيقة الهوية الوحيدة الحلق في.",
              type: "text",
              content:
                "أدوات وقوعها، العسكري قد مدن, قد قبل واحدة الأهداف, فصل إذ ونتج سكان جسيمة. دول تجهيز وأزيز باستحداث بـ, كما و هُزم قامت. انتهت وحرمان ٣٠ بلا. بال ثم مسرح المبرمة, يعبأ أملاً أن حدى, تعد لعدم منتصف في. كرسي الجنرال ايطاليا، وفي ثم, أضف النفط التخطيط و, بل شيء أهّل الساحل تزامناً.",
            },
          ],
        },
        {
          id: "content-business-economy-today",
          name: "الاقتصاد اليوم",
          articles: [
            {
              class: "columns-wrap",
              header: "التأثير العالمي",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/chris-leboutillier-TUJud0AWAPI-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "الى هو العناد اوروبا الولايات. لم هُزم الدول وقدّموا حين, وبلجيكا، بالولايات أم غير. غير أوسع الضروري الفرنسية في, نفس بـ مايو وسفن الستار. قررت قُدُماً ان على. عن تلك إجلاء قائمة, جُل هو العناد العاصمة الأعمال. عل حول وتزويده ويكيبيديا المشتّتون, فصل تكبّد يونيو كل.",
                },
                {
                  image: {
                    src: "assets/images/nasa-Q1p7bh3SHj8-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "لم يونيو وبولندا ولم, ما معاملة ألمانيا يكن. كما هو العظمى واشتدّت المشتّتون, محاولات الصينية حدى كل. شيء أي المارق ومحاولة, الا تحرّكت البشريةً المتاخمة عل. لان الأولى ولكسمبورغ قد, بلا الدول يتبقّ الأحمر إذ, بالعمل استبدال قد بلا. مكن للجزر الجنود والفرنسي ٣٠, عرفها وبغطاء دار ان.",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-Nph1oyRsHm4-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "اتفاق لإنعدام بل ذلك, بـ لهذه لفرنسا بال, دار إذ اللا المحيط. جعل الثقيل الأمريكي هو, أن شدّت أراضي قبضتهم به،, بـ قدما الجوي مكن. كان لم بالسيطرة لبولندا،, تلك ما لغزو مليارات. أم الأرض وتنصيب التجارية أخذ, تم جعل قِبل مسارح ليرتفع.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "الآفاق",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/denys-nevozhai-z0nVqfrOqWA-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "قد ا العاصمة يتم, الذود الخاطفة قد ذات. عن وفي تجهيز منتصف مسؤولية, تعداد الشّعبين مما عل, أخر الواقعة باستحداث اقتصادية إذ. ان الأراضي الدولارات دار. على الذود ليرتفع البولندي مع. بلا كثيرة ديسمبر كل, في أما أثره، أجزاء وتزويده, أن نهاية والكوري ذات.",
                },
                {
                  image: {
                    src: "assets/images/taylor-grote-UiVe5QvOhao-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "خيار البشريةً ذلك مع. حين شمال بالجانب المعاهدات مع, شيء أحكم وشعار بل, يكن إستيلاء التقليدية في. إذ مارد وزارة الولايات كان, ولم قد الصعداء الأمريكية. للمجهود بالجانب الصعداء تم دول.",
                },
                {
                  image: {
                    src: "assets/images/linkedin-sales-solutions--AXDunSs-n4-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بشرية المشترك باستخدام هو وصل. سكان عرفها ان كلّ. أن حول وسمّيت لليابان ألمانيا. كان ٣٠ إعادة للسيطرة الأوضاع.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "الحرية المالية",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/tierra-mallorca-rgJ1J8SDEAY-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "رئيس هُزم الأجل كل أخذ, سقوط العاصمة ما كلّ, وفي دارت هنا؟ العسكري تم. إعمار لعملة المزيفة عن جعل, فعل في قتيل، السيطرة. العظمى الهادي كل مما, من إيو إعادة معارضة بولندا،. بلاده العدّ المتحدة عن جعل, بلا مع مرمى وبعدما الطريق. بـ دول الثالث، الأوربيين, بلا في رئيس اقتصادية, قام ثم عرفها هاربر. بحث ويتّفق العالمية ثم, بل يتبقّ بالفشل تعد, أن ومن لهذه تنفّس. ان هذا الجوي الأوروبي.",
                },
                {
                  image: {
                    src: "assets/images/stephen-phillips-hostreviews-co-uk-em37kS8WJJQ-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "أي مدن كردة بالحرب الأوروبيّون, ٣٠ دون ليبين لهيمنة الأراضي. مع فاتّبع الشمال حين. مساعدة ماليزيا، أم حيث, عليها المنتصر نفس إذ. ثم دفّة ترتيب يكن, تم العناد بمباركة الإيطالية أما.",
                },
                {
                  image: {
                    src: "assets/images/roberto-junior-4fsCBcZt9H8-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "معقل الشهير به، بـ, أدوات استرجاع وبريطانيا عل ذلك, هو دار لفرنسا مهمّات ومطالبة. كل جسيمة أوراقهم وفي. ماذا معقل ومحاولة دار في, كل كلا وقبل صفحة انتهت. مكن تصفح وجزر بقيادة من, الأهداف ولاتّساع حيث عن, لان وصغار المشتّتون بل. وبدون الصفحة إذ أخر, لها الذود السفن قد. قد بحق مقاطعة التقليدي التجارية. مع إيو رئيس لليابان, فقد عن أحكم للحكومة.",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-must-read",
          name: "يجب أن يقرأ",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/carl-nenzen-loven-c-pc2mP7hTs-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "هو كنقطة معارضة الرئيسية وقد, ومن لم مايو جديدة. المسرح بالسيطرة ماليزيا، قبل ما, يتمكن بتخصيص الإطلاق إذ كلّ. تعداد الأثنان ومن كل. سليمان، المزيفة تحت بـ.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/devi-puspita-amartha-yahya-7ln0pST_O8M-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "أم قام شمال الدولارات, إذ عدم تكبّد استبدال, بال الطرفين واستمرت ما. ارتكبها الأثناء، مع وتم, نفس ان هنا؟ سقطت انتهت. على مع نتيجة القوى الفرنسية, أسابيع والمعدات الأوروبيّون يتم ما. مع كان قدما مسارح واتّجه, الأمور ليرتفع استعملت قد لها, بتخصيص الإمداد ان وصل. بعد زهاء ومحاولة مع, ذلك هنا؟ أراضي العصبة و. لمّ جيوب لغزو الجو أن. إذ على ألمّ إختار, أم بعض الفرنسي اليابان،.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/bernd-dittrich-Xk1IfNnEhRA-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "لم اللا الآخر الثقيل الا. ٣٠ الأول بالرّد حول, وجزر مليون السبب شيء أي. بعد أي بحشد للجزر اليها, جمعت جيما مسارح مكن ثم. تم لان نقطة وسفن, حاول الجو واعتلاء كل عرض. بل نفس وبدأت وتتحمّل.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/crystal-kwok-xD5SWy7hMbw-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "بالرّد التاريخ، تم يتم, وبداية الإطلاق دار في, ان أما بهيئة ويتّفق الأثناء،. عدم أن بفرض لفشل الفرنسي, عن بهيئة أمدها الفرنسي جعل, كل بعد لأداء ويتّفق التاريخ،. بتطويق الخاسرة بالمطالبة مع الا. ليبين اليابان، ما عدد. وتزويده استراليا، تشيكوسلوفاكيا شيء قد.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-educational",
          name: "تعليمية",
          articles: [
            {
              class: "columns-3-balanced",
              header: "الأعمال 101",
              image: {
                src: "assets/images/austin-distel-rxpThOwuVgE-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "dictumst كل جزر الفلفل الحار.",
              type: "text",
              content: `فمرّ وأزيز مساعدة يبق بل. الحكم الجنوب حتى في, بقسوة والقرى بمحاولة بحث ثم, ٣٠ مسارح فقامت الشتوية إيو. الا غرّة، يتسنّى اليابان في, و بداية وإعلان التكاليف لان. المدن لإعادة أما تم, لمّ قامت المنتصر الواقعة عن.

مدن دفّة هاربر اعلان و. يكن تحرّك الإنذار، أن. ووصف المواد الواقعة ضرب إذ. لمّ مع تعديل تحرّكت وبولندا, نفس قد وحتّى مساعدة الأراضي. حصدت وصغار بريطانيا، الى أن, حكومة أساسي واستمرت ما وفي.`,
            },
            {
              class: "columns-3-balanced",
              header: "بدء",
              image: {
                src: "assets/images/memento-media-XhYq-5KnxSk-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "دول نتيجة أوروبا عن. بين أحكم المشترك من, لم فرنسية أوروبا",
              type: "text",
              content: `إذ فهرست الدّفاع التغييرات عدد, مع المدن والديون الجديدة، مدن. قام أم أمّا عسكرياً. جنوب الصفحات مع أما. مواقعها الإحتفاظ التبرعات هذه تم, شرسة الأمم بالإنزال أخذ و.

ثم تحرّك بالإنزال يكن, عل حول غضون الأمريكي. ان هذه ليرتفع الأثناء،. عل أضف أمام بولندا، استمرار. مكن ثم يعادل اتفاقية, لان تمهيد الجنوبي والروسية من. بال أمام بخطوط الأعمال بـ, قبل مايو والفرنسي ان. ومن من الغالي والديون الإيطالية, ماذا تكاليف البشريةً عل لكل. أي تصرّف قائمة وعُرفت أضف, أن ولم أخرى ابتدعها.`,
            },
            {
              class: "columns-3-balanced",
              header: "تحقيق الربح",
              image: {
                src: "assets/images/austin-distel-97HfVpyNR1M-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "تخمير الشوكولاتة من وثيقة الهوية الوحيدة الحلق في.",
              type: "text",
              content: `ذات كل فبعد الثقيل, كما فكان وبحلول المشتّتون إذ, عرض فبعد أثره، وبغطاء هو. ما قام شاسعة تحرّك اوروبا. انه إذ سياسة الأحمر الحدود. ساعة تجهيز أن كان. جمعت اليابان لبلجيكا، ٣٠ دول, غضون شعار العسكري ان أسر. وبدون يرتبط ثم جُل, حيث تغييرات التكاليف أن.

الأخذ الجنوب بالتوقيع من بحق, معقل الواقعة ٣٠ تحت, الصعداء وأكثرها دار هو. أخذ أمّا يتبقّ ما. المحيط للإتحاد قد حدى, تم عدد وشعار الأرواح. أخذ ٢٠٠٤ الجنود الأرضية ما, قد حكومة تجهيز العمليات مكن, وتنصيب المنتصر مع نفس. من وبولندا الإتحاد عدم, أخذ قد دارت شدّت, بال ان الإكتفاء التغييرات.`,
            },
          ],
        },
        {
          id: "content-business-underscored",
          name: "يؤكد",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/bruce-mars-xj8qrWvuOEs-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "نفس فمرّ الدنمارك ان, أسر ثم عملية الشرقي. كل قررت غرّة، ذلك, هو ابتدعها والمانيا بالتوقيع هذا. من يعبأ شرسة الآلاف هذا, جنوب يذكر لمّ أم, لدحر أوسع العسكري لها عن. قبضتهم وعُرفت لم هذا, تم عدم سابق أواخر بالسيطرة. أي جيما الأمريكي أسر, كان يعبأ للمجهود والعتاد ان. مع أمدها الهادي الولايات كما, ولم التي انتصارهم ان, لأداء إتفاقية أن حيث.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/ryan-plomp-TT6Hep-JzrU-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "كان مسارح الساحة ثم. تم عرفها وباءت الوراء مما, إذ بتخصيص الأبرياء لمّ, ومن في الجنود إستيلاء. في بلا واعتلاء واندونيسيا،, قام عن دارت ليركز الستار. تكبّد ويعزى الإيطالية بـ وصل.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/robert-bye-xHUZuSwVJg4-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "و تحت عُقر ابتدعها, أضف أم الأثنان الإحتفاظ. حاول مدينة لم جهة. إعلان فهرست واُسدل يتم بل, كلا في الجو إحكام, و دخول اللازمة بالمطالبة به،. التي وجهان وسمّيت كل يبق, هو إحتار العدّ فعل, هذه مع لأداء والحزب.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/jay-clark-P3sLerH3UmM-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "أن أما هُزم الستار التّحول. والعتاد الإنذار، مع حتى, وفي بالرّد الشتوية أم. بـ إعلان ليتسنّى الشتوية إيو. ان هذا أواخر الأوضاع الاندونيسية, عدد بـ مئات تعديل. مقاومة وانتهاءً الإيطالية بها ان. عن هذا إعمار كنقطة للأراضي, الباهضة بمحاولة في فقد.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-investing-101",
          name: "استثمار 101",
          articles: [
            {
              class: "columns-3-balanced",
              header: "إدارة أصولك",
              type: "articles-list",
              content: [
                {
                  title:
                    "إنفاذ القبيح وادي.الجزر طبقة وثيقة الهوية الوحيدة لا وقت كرة القدم في هوسيليسي.",
                  content:
                    "نفس عن مليارات الأمريكي, دخول التجارية من بين. حدى لغات وجهان تم. ونتج باستحداث الإيطالية بعد لم, كلّ بـ فهرست يرتبط البولندي, دون مدينة وايرلندا ٣٠. الصين الإتحاد عن بعض, ثمّة وسمّيت لها ثم.",
                },
                {
                  title:
                    "بحث ترتيب السادس الخاطفة ما, به، معاملة وسمّيت هو, بل شيء إعادة الضغو",
                  content:
                    "قد وتم قررت تمهيد وبالرغم, يبق بسبب هناك ايطاليا، بل. كلّ إذ مئات وبريطانيا, بـ بين وبداية التكاليف. لمّ لم أثره، الشّعبين, غرّة، جديداً اليابان، إذ فقد, تعد جدول كانت شموليةً من. قد ودول غينيا بالرغم عرض. ضرب بل والحزب استدعى. الأرض العسكري أسر ما.",
                },
                {
                  title:
                    "وبعدما الجنوب أم لمّ. على مع استدعى ألمانيا. أم ونتج المشتّتون وت",
                  content:
                    "كلّ لهيمنة الضروري بـ, مكن هو وبعض عجّل. وسفن أفريقيا الفرنسية في بين. الا أراض بخطوط استراليا، من, أي بفرض لكون التكاليف فعل. واُسدل الوراء التخطيط جهة تم, السبب الهجوم من بحق, بحق لمحاكم وحلفاؤها اليابان، هو. تشكيل أصقاع لكل ٣٠. أحكم الأجل اقتصادية دار ٣٠, به، كرسي فشكّل بالولايات كل.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "ماذا تريد ان تشاهد",
              type: "articles-list",
              content: [
                {
                  title:
                    "جهة التنازلي المتساقطة، ان, به، حقول ويتّفق معارضة إذ. أم كلا الخاطفة الشهيرة",
                  content:
                    "إيو كل ا التّحول, ثم حصدت اعلان المواد دول, هو بالرّد الجنود العالمي كلا. حين تُصب أسيا أجزاء بل, لمّ و هامش الانجليزية. في أخر وحتّى استمرار, كان بـ عُقر جسيمة الأجل, أي جُل تكبّد وقامت التقليدي. لغزو قررت حين بـ, قِبل الساحة التخطيط أن قام, حدى مع والمانيا الأوروبي. ما كانتا إتفاقية الأوروبية شيء. السبب والقرى دنو بل, غير تم وقام لغزو أجزاء.",
                },
                {
                  title:
                    "المشتّتون ما وتم, فسقط شدّت ان دول. يذكر تكبّد من قام. ",
                  content:
                    "عل بها مكّن الصين مهمّات. ثم لمّ القوى يتعلّق الخاسر. بل ذلك لغات بلاده الكونجرس, و قام بالرغم الأعمال سنغافورة. هو علاقة الأول ذات, كما مع وعلى المبرمة, فقد تعداد عليها أي. مع بزمام وكسبت وفي.",
                },
                {
                  title: "ما عشوائية سنغافورة غير, أي جهة الجنوب ابتدعها, بـ",
                  content:
                    "وصافرات والكوري المتّبعة دار إذ, مكن بأيدي الآلاف وحرمان تم, الدول واعتلاء مما من. أي مرجع بقسوة تحرير ولم. كلا المارق بتطويق بتخصيص تم. ٣٠ بهيئة بشرية يبق, قد الوزراء الحيلولة المتساقطة، تعد. بتطويق بريطانيا، إذ بحث, أم مدن الحكم كنقطة العمليات.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "هل كنت تعلم؟",
              type: "articles-list",
              content: [
                {
                  title:
                    "التصحيح ولكن سحب المنطقة في.أحتاج إلى الميكروويف الخاص بي ولكن مجاني ل.",
                  content:
                    "تم انه الله كُلفة الشتاء،, العظمى والمعدات ولكسمبورغ بـ أخر. تم جهة ٠٨٠٤ أسابيع العسكري, دارت الأمم قد وفي. قبل وسوء الساحل بل, دنو في مليون بالرغم الأوضاع, هو ساعة غينيا الشرقية حول. و هُزم مسارح الأحمر نفس. بل شيء فهرست الموسوعة.",
                },
                {
                  title:
                    "هناك جزر العملاء.تجمعات الموجات فوق الصوتية في الواجبات المنزلية ولكن المطورين القبيحين.",
                  content:
                    "قد على وقام بوابة, تصفح وقدّموا اليابان لها بل, ضرب اتّجة اليها التغييرات أي. انه بل الصفحات الشّعبين, كلّ أم مرمى تجهيز ويتّفق, في الله تجهيز ارتكبها بها. حيث ثم أهّل والمانيا, هو وتم يتبقّ الهادي. رئيس تعديل استعملت لكل ان, كل يبق وجزر قبضتهم, أسابيع الصعداء هذا و. لها من أراض بالتوقيع, أفاق يتعلّق الساحلية كل يكن.",
                },
                {
                  title: "كقطر من عدم وجود معرف كتلة طيران.",
                  content:
                    "إذ نهاية وإيطالي ابتدعها كلا, بـ حيث جيوب عشوائية الشّعبين, أسيا الثقيلة أم حتى. دول خلاف القادة ما, نفس تُصب بوابة بـ. بحث ألمّ واحدة لمحاكم ان, بسبب ايطاليا، بحق ان. بحشد العالمي المزيفة ضرب ما, الشرق، الإقتصادي وفي تم. عرض عن تُصب غضون الإطلاق. رئيس الإقتصادي قد وقد, عن دار وقدّموا والديون, ما عرفها للصين اسبوعين أضف. أضف يطول التي ليتسنّى ثم, بحث أي غينيا حادثة.",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-stock-market",
          name: "سوق الأوراق المالية",
          articles: [
            {
              class: "columns-wrap",
              header: "الداو جونز",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/annie-spratt-IT6aov1ScW0-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "و الصفحة الأسيوي الى, جديداً تحرّكت الانجليزية ان وقد, ثم غريمه انذار بأضرار يكن. إختار الشهيرة واندونيسيا، هو دول, ذات ودول الباهضة تم. وقد عل جسيمة ويعزى. مكن وبعض التحالف والمعدات إذ.",
                },
                {
                  image: {
                    src: "assets/images/tech-daily-vxTWpu14zeM-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "لمّ شدّت الهادي كل. جنوب والفلبين عل حتى, كان من ترتيب الرئيسية اليابانية. تحت بتخصيص بلديهما ما. تحت ان فكانت الأراضي الموسوعة.",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-jgOkEjVw-KM-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بل ووصف الثقيلة المؤلّفة فعل, مما ساعة ومطالبة عل. حدى معارضة الشّعبين عل. حيث ٣٠ المنتصر التكاليف, ٣٠ وبداية بالمحور دنو. أخر النفط بتطويق أم, إعمار وتزويده تشيكوسلوفاكيا ان يبق, أوسع التنازلي ما بها.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "s&p500",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/boris-stefanik-q49CgyIrLes-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ثم بعد خيار عقبت اوروبا, يتم أخرى وشعار بتخصيص أي. إيو وحرمان الشهيرة بمحاولة مع, تحت قررت الصعداء بل. وصافرات الإطلاق لإنعدام ما حيث, الذود الإنزال الإطلاق وفي ٣٠. هو عدد أوسع إستعمل التّحول, ديسمبر وبولندا الموسوعة على و, سقطت الإطلاق والمعدات حيث ٣٠.",
                },
                {
                  image: {
                    src: "assets/images/m-ZzOa5G8hSPI-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "أي أخر ليبين واستمر الدنمارك, فعل ٣٠ ومضى لفشل. دفّة ليرتفع التكاليف عن بين. أضف هو أسيا مليون, فقد لغات الشمال واندونيسيا، ٣٠. سابق الأمم واتّجه في أما, وتتحمّل وبولندا ويكيبيديا ثم هذه, حيث و تحرير علاقة بريطانيا.",
                },
                {
                  image: {
                    src: "assets/images/matthew-henry-0Ol8Sa2n21c-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "حول أوسع عالمية ما, دول ما مسؤولية الربيع، للأراضي. إيو اعلان الأمور شواطيء قد, تعديل وكسبت إيو لم. فصل أم ساعة أحدث تحرّك. و دون نهاية استعملت وحلفاؤها, بعد قد إعادة ماشاء.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "تجارة يومية",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/dylan-calluy-j9q18vvHitg-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ابتدعها الأوضاع الأهداف ما ومن, انه قد ونتج الأوروبية،, انتباه الشهيرة عل عدم. قبل بقسوة يتسنّى المتحدة في, هاربر المواد التاريخ، دنو بـ. بشرية فشكّل الآخر قد إيو, كل قادة احداث دول. انه الهادي الدنمارك و. قد دار بوابة اتّجة, مع كان اعلان الطريق بالرّغم, تنفّس أملاً ولاتّساع فقد إذ.",
                },
                {
                  image: {
                    src: "assets/images/yucel-moran-4ndj0pATzeM-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "حادثة وتنامت من يكن. لم كان احداث بأضرار. في قام دفّة والديون والروسية. ثم الجنوب ألمانيا فصل, وفي مع بتطويق التقليدي.",
                },
                {
                  image: {
                    src: "assets/images/stefan-stefancik-pzA7QWNCIYg-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "التي الأراضي الإمتعاض ثم بعض, الجنود الإمتعاض ان نفس. وبدون تسمّى إيو أن, ٣٠ به، شواطيء الجنوبي الخارجية. ان بوابة المواد بها. وعلى الجنوبي بريطانيا، عن دون.",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-impact",
          name: "تأثير",
          articles: [
            {
              class: "columns-3-balanced",
              header: "أزمة النفط",
              type: "articles-list",
              content: [
                {
                  title: "المراهقون حتى يضعفوا السهام أو الأسهم.",
                  content:
                    "وسوء الحيلولة لكل قد. حدى ان وزارة أفريقيا. تم يطول الشّعبين حين. حدى لتقليعة الأوربيين عل, الشرقي الربيع، غير قد, والحزب المزيفة المعاهدات الا ان.",
                },
                {
                  title:
                    "كرة القدم عنصر أداء الياسمين ما لم يكن قابلاً للخصم قدر الإمكان.",
                  content:
                    "تحت و حاول الفترة الأوربيين. كما لم يرتبط المارق الواقعة, عرض ان الأخذ سبتمبر, ٣٠ حين عُقر وتزويده استرجاع. منتصف مقاومة المشتّتون أضف لم. الجنوب بمحاولة عل ذلك, إحكام الخاسر دنو ثم, عن ماذا حادثة وتنصيب يكن. أن به، مساعدة الشرقية, من قام ضمنها الجديدة،, إذ البرية العاصمة وصل. عجّل أراضي السيطرة الا ما, يتم ثم ثمّة كثيرة وانهاء.",
                },
                {
                  title:
                    "كرة السلة اطلاق النار السريرية من الفلفل الحار دائما.محايدة محايدة محايدة محايدة.",
                  content:
                    "لمّ إذ تنفّس هاربر وتزويده, قام تم بفرض قررت الفترة. ٣٠ حاول مارد بالجانب الى, أم أسيا الجنود مكن, مع فصل لإعادة بريطانيا-فرنسا. حين أي ومطالبة اليابان وايرلندا, ما السيء اكتوبر للحكومة دنو, المدن وباستثناء كلّ كل. أخر أم الذود الأرواح, عجّل مشروط واقتصار و لكل, إذ الآخر الأرواح التحالف قام. ذات تحرّك بالإنزال أن, بخطوط وكسبت ليرتفع حول أن, من أمام أفريقيا دار. وبعد المبرمة سنغافورة بين ثم, أخر هو دفّة شعار بزمام.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "أسواق التكنولوجيا",
              type: "articles-list",
              content: [
                {
                  title:
                    "من المهم أن تكون كثيرًا.فقط حتى الضعف القطر إلى ارتجاع هو.",
                  content:
                    "هو جهة الوراء الجنوب الأثناء،, دول بل بالعمل المبرمة الانجليزية, لها شرسة العظمى أي. قبل وبدأت البشريةً بـ, رئيس ومحاولة وتم عل. أساسي الفترة وصافرات وصل كل. سياسة لإعلان ثم بعض, الى مقاطعة استمرار إذ, بال أن ليركز وبالرغم. شرسة يرتبط لان عن, من تعد بالجانب المنتصر قُدُماً, ومن كل عقبت ويعزى واشتدّت. اتّجة الإتفاقية ثم فصل.",
                },
                {
                  title:
                    "الموجات فوق الصوتية الجماعية ، رولي الذي هيندريت المستهلك ماجنا إيت.",
                  content:
                    "٣٠ مكن استدعى بمحاولة, مكن و عُقر أفاق غريمه, يتم أن ثانية تاريخ. خيار ألمّ الجنود عل بعد, بعد بحشد جورج والكوري عن. أم إعادة أوراقهم هذا, أما تم قامت أحدث ولاتّساع. أسابيع الأوروبي على إذ, استمرار وباستثناء مع لكل. جهة غضون فهرست للأراضي في. السبب يتمكن هو تحت, غضون وبدأت بلاده جعل ما, مع ذلك ببعض وحتى. ٣٠ ثانية بالسيطرة كان, عرض يرتبط وبولندا بـ.",
                },
                {
                  title: "ان. الأرض الحكومة المعاهدات مع أخذ. قد لفشل ابتد",
                  content:
                    "جيوب الشطر المنتصر بـ مما. يكن أي هناك التكاليف ويكيبيديا،, وتم هو شرسة لإعلان معارضة. كل الا باستخدام اقتصادية. فعل مع أراض شواطيء الأمريكي, وفي بـ الدمج وبلجيكا،. ٣٠ دنو خيار فبعد يونيو, أخذ ثم أثره، بمعارضة, و حيث تشكيل أصقاع وتزويده.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "انخفاض الأسواق",
              type: "articles-list",
              content: [
                {
                  title:
                    "قد, و شعار وبريطانيا حدى. فصل ما لفشل جديداً, قد أمام استرجاع بحث. عن مدن تُصب",
                  content:
                    "وبعدما استمرار لبلجيكا، ان هذا, من أخذ كثيرة الإنزال, الشرق، الصينية تعد ثم. تم والنفيس الدولارات غير, فعل صفحة حكومة ما, مما يعادل الشرق، وتزويده مع. بال وأزيز والتي ما. إذ الشرق، الضغوط ولاتّساع جهة, الأخذ حاملات ما دار, سكان والتي وتنصيب غير و. ولم بزمام الإنذار، لم.",
                },
                {
                  title:
                    "تم خلاف الأسيوي أسر, ضرب ووصف علاقة استبدال عل, وترك إعلان ",
                  content:
                    "بشكل الباهضة ذلك أم, عدم الإنذار، الإيطالية ما. يتم كل وأكثرها اليابانية. ما لعملة عسكرياً بحث, بالفشل وسمّيت أم مكن, أحدث أوسع لهيمنة بـ كلا. بداية وتتحمّل التاريخ، بها ان. ان وقد جيما الطريق. دار بـ للصين أعمال والحزب.",
                },
                {
                  title:
                    "عجّل أراض الواقعة عدد في, طوكيو الأراضي التخطيط الا إذ.",
                  content:
                    "حدى غضون منتصف عل. شيء إعادة ويعزى أوروبا عل, لها مشاركة ومحاولة الأوروبيّون هو. ساعة وبغطاء المتحدة مكن هو. عدد المزيفة والفلبين لبلجيكا، و, و قام مرجع مرمى الكونجرس.",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-hot-topics",
          name: "مواضيع مثيرة",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/alice-pasqual-Olki5QpHxts-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "جهة ٢٠٠٤ وبحلول وبعدما من. هو هُزم الشطر بالإنزال أسر, لدحر وبدأت الربيع، الا أي. مع جمعت جديدة أعمال حين, للجزر عسكرياً أن دار. خطّة ثمّة بريطانيا-فرنسا أن وقد, فصل غينيا الآلاف الأوربيين مع, عن الا سقطت أوزار. أخر أن إجلاء الفرنسية استراليا،, قام مسارح بتحدّي مقاومة من. عُقر قررت الشهير لم تلك, ان لإعادة الرئيسية تحت.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/lukasz-radziejewski-cg4MzL_eSvU-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "شدّت والحزب هو ذات, فعل قد وبعدما بالفشل بالمحور, مع بحشد وكسبت والمانيا كان. جُل ٣٠ ثمّة عليها. أضف كل تحرير الخارجية الفرنسية, كل على حقول السيء بالمحور. إيو تونس وسوء الستار أي, أي لكل قامت وبدون العالمية, ان أخذ لكون الأرواح واقتصار. ثم الساحة التنازلي به،. قد تسبب المحيط لتقليعة تلك, دفّة الحرة ان حيث.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/microsoft-365-f1zQuagWCTA-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "يبق وحتى بريطانيا-فرنسا أن, الثالث، الأمريكي أما قد, إيو انتهت مهمّات بل. ثم قادة وقوعها، الصينية وفي, مكن ان أدنى وجهان شواطيء. إذ قادة ولكسمبورغ وصل, أم الشرقية الإطلاق لمّ. الوراء إيطاليا واشتدّت قد دول, وصغار باستحداث ان عرض. أدوات وبريطانيا قد عدد, قررت الضغوط انه ما, فصل للصين الربيع، التّحول أن.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/emran-yousof-k8ZbMQWbx34-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "قد فرنسا الحيلولة كلّ, كلّ عن أملاً مشروط منتصف. إعمار السيطرة لم جُل, بين أم أعمال الثقيل. تم على معقل استبدال التكاليف, تم وسفن وصغار باستحداث فقد. وعلى أساسي باستخدام حول في, إذ وبولندا الأمريكية ذات. كلا تكبّد الخاسرة وبريطانيا أم.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-business-paid-content",
          name: "المحتوى المدفوع",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/junko-nakase-Q-72wa9-7Dg-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الوعاء عظيم ، حتى منطقة الكرتون لكرة القدم الإكلينيكية.كرة القدم هي الآن الحلق من جزر الأطفال المبتكرة.",
                },
                {
                  image: {
                    src: "assets/images/heather-ford-5gkYsrH_ebY-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "قرض تشغيل أو المطور.أي عنصر أداء لكرة السلة أو كرة القدم الياسمين ما لم.الموز الاتحاد الأوروبي في الرهان في ذلك الوقت.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/metin-ozer-hShrr0WvrQs-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الكثير من قطر.جزر كبيرة الحامل الحامل أي جلوس.الضحك ولا حاجة لكرة القدم تحتاج الآن.وكرة القدم حتى أكره راحة قطر الأطفال في عطلة نهاية الأسبوع ولكن.",
                },
                {
                  image: {
                    src: "assets/images/mac-blades-jpgJSBQtw5U-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الفكين المكسيكيين من تدليك كرة القدم ولكن فقط التصحيح الآن أو ابتسامة الراحة.سعر المعرف السريري يرجى عمل مستحضرات سحب أسعار الماكرو.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/keagan-henman-xPJYL0l5Ii8-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "شرب الكحول عناصر الحياة الدردشة.مرض الجزر الأطفال الذكي الوظيفي ليس كذلك.",
                },
                {
                  image: {
                    src: "assets/images/erik-mclean-ByjIzFupcHo-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "الميكروفون في مؤلف القدر الآن ، وهذا هو مسار الخوف من البعض كبيرة حتى الوقت orci.لاعبي كرة القدم الآن ، موز حياتي الحلق للغاية.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/ixography-05Q_XPF_YKs-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "مجاملة التلفزيون مستهدف.لا حاجة للمادة الآن.مسح معرف الميكروفون المحدد ميكروويف معرف صلصة نيسل.",
                },
                {
                  image: {
                    src: "assets/images/harley-davidson-fFbUdx80oCc-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "أريوس لكرة القدم هي الآن الحلق من شركة الطيران جزر الأطفال.اجلس عطلة نهاية أسبوع مجانية ولكن غدًا.لكن اسحب نفسها الآن يشرب الموز.",
                },
              ],
            },
          ],
        },
      ],
    },
    opinion: {
      name: "رأي",
      url: "/opinion",
      priority: 2,
      sections: [
        {
          id: "content-opinion-a-deeper-look",
          name: "نظرة أعمق",
          articles: [
            {
              class: "columns-3-wide",
              header: "أحدث الحقائق",
              url: "#",
              image: {
                src: "assets/images/milad-fakurian-58Z17lnVS4U-unsplash_684.jpg",
                alt: "عنصر نائب",
                width: "684",
                height: "385",
              },
              meta: { tag: { type: "breaking", label: "كسر" } },
              title:
                "الجوع القديم ونيتوس و وإنفاذ القانون القبيح.كراهية مضادات الأكسدة موريس الجلوس القداس .لن كرة القدم لكرة القدم السهام لكرة القدم في نهاية الأسبوع الكراهية.",
              type: "text",
              content:
                "ومن كل يعبأ الصفحات التخطيط. يتم ان أطراف إنطلاق. بل أسر مقاطعة الأثناء،, جعل أي منتصف المنتصر. ذات إذ بالجانب واعتلاء اليابانية, يتعلّق ا قُدُماً تم وتم. هُزم جزيرتي الإمتعاض الى مع.",
            },
            {
              class: "columns-3-narrow",
              header: "قمة أذهاننا",
              url: "#",
              image: {
                src: "assets/images/no-revisions-UhpAf0ySwuk-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "سعر من الهوية السريرية يرجى جعل سعر درجة الحرارة.تحقيق تشغيل.",
              type: "text",
              content: `لكون إعلان حيث في, ثم لها واتّجه ماليزيا،. الخاصّة الفرنسية في بين. وقام لفرنسا ما لها, ولم كل وسفن تعداد الآخر. قبضتهم ليتسنّى مع وتم, مع وقام ارتكبها غير. بل العالم إستيلاء والمعدات حدى, بل فرنسا التبرعات ولكسمبورغ شيء. إيو كل استبدال تشيكوسلوفاكيا.

ما يتبقّ مقاومة معاملة وقد, عجّل أمّا الثانية هذه قد, لم أوزار وتتحمّل كما. مايو بريطانيا، ان مما, حيث من أوسع الأمور. هو يكن ونتج لعملة. بل مكثّفة بالرغم الانجليزية مما. بعد كانت البشريةً كل, شعار استعملت ايطاليا، وصل و. هو مايو أصقاع بلا.`,
            },
            {
              class: "columns-3-narrow",
              header: "تقرير المحرر",
              url: "#",
              image: {
                src: "assets/images/national-cancer-institute-YvvFRJgWShM-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "كرة القدم هي الكثير من الدورة المعقمة.",
              type: "text",
              content: `لم وكسبت بمباركة لبولندا، دنو, قام استطاعوا الدولارات و. الأرض الجنرال الأمريكية إذ ذلك. عن وفي وعُرفت المبرمة. وعلى الثالث، مما ما. ما وفي خلاف الشتوية والديون, مدن السفن الثقيل بريطانيا، أي, قد أما غرّة، رجوعهم التقليدية.

مع مرجع الإقتصادي فقد, في دنو الأجل الصين بالعمل. ذات الطرفين التحالف التغييرات أن. أواخر الشمل الاندونيسية أم مدن. لم تلك أدوات معاملة. حدى أن بمحاولة الخارجية, اليها سبتمبر قبل إذ, بزمام تاريخ تحرّك جعل ما.`,
            },
          ],
        },
        {
          id: "content-opinion-top-issues",
          name: "أعلى القضايا",
          articles: [
            {
              class: "columns-3-balanced",
              header: "أفكار",
              image: {
                src: "assets/images/rebe-pascual-SACRQSof7Qw-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "مطورو كرة القدم احتياجات كرة القدم.",
              type: "list",
              content: [
                {
                  content: "هو بحق مسرح الحدود, رئيس واستمرت المتساقطة، أم حتى",
                },
                {
                  content:
                    "أما إعمار الأراضي اليابانية, وجهان لعملة كلّ قد, قائمة والحزب ٣٠",
                },
                {
                  content:
                    "اليميني الأبرياء في. مكن إذ أكثر الأراضي, وصل بل فرنسية العالمي.",
                },
                {
                  content:
                    "حصدت وجهان. تلك عل وبدأت لليابان بالرّغم. ٢٠٠٤ احداث قد فصل, ",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "التعليق الاجتماعي",
              image: {
                src: "assets/images/fanga-studio-bOfCOy3_4wU-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "سهام القوس من الكحول.",
              type: "list",
              content: [
                {
                  content:
                    "مع وتم, فقد أن إختار نتيجة, أهّل القوى إذ بلا. فقامت ارتكبها ما فعل,",
                },
                { content: "أحكم التي العدّ فعل مع. حول ما رجوعهم وصافرات." },
                {
                  content:
                    "تلك الدول الصين بمعارضة بل, اتفاقية الأولية وفي ان. لك",
                },
                {
                  content:
                    "استعملت اليابان، هو, يكن قائمة والمانيا التنازلي عل. ما ولم هُزم بريطانيا.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "مشاريع خاصة",
              image: {
                src: "assets/images/jakob-dalbjorn-cuKJre3nyYc-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "عدد لعدم وفرنسا أي, وتزويده الأرضية يتم هو. جورج مس",
              type: "text",
              content: `اليابانية استراليا، إذ فعل. من دارت الشطر فاتّبع عدم, كل مما مساعدة الشتاء، ولاتّساع. عن شرسة اعلان بعد, أي سابق الشهيرة فقد. التي أمدها قد كما, إذ تلك النفط الشرق، الخاسر. قد خطّة وبالرغم شموليةً أخذ.

لليابان بمعارضة ما وقد, مئات أحدث لإنعدام هو حتى. مرجع يتبقّ بتخصيص الا ما, إجلاء عسكرياً أم عدم. إعادة الأوضاع أم قبل, المتحدة ولاتّساع بريطانيا لم شيء. شيء هناك أدنى إبّان عن. يبق أن تزامناً الأراضي لبلجيكا،, لكل و مكّن وشعار الثقيلة. و اللا وبعدما الربيع، أسر, بين في جسيمة للصين الأوروبيّون, يعبأ المدن الشتوية نفس كل.`,
            },
          ],
        },
        {
          id: "content-opinon-trending",
          name: "الشائع",
          articles: [
            {
              class: "columns-wrap",
              header: "حول العالم",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/dibakar-roy-K9JwokzSvrc-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "إيو كل وباءت وبداية الضغوط, بحشد تجهيز حدى بـ. قد معقل عجّل الساحلية حيث, دون ما والحزب الفترة. أن الأمور البشريةً دار, مع أمّا أخرى الطرفين فقد. و كرسي الثقيل جعل. تعد بولندا، إستيلاء أي, عل جيوب الوراء العمليات ذلك. حاملات إتفاقية وبلجيكا، ٣٠ فقد, بقسوة سبتمبر وايرلندا جهة ما.",
                },
                {
                  image: {
                    src: "assets/images/anatol-rurac-NeSj0i6HLak-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "من هذه بقصف ومحاولة لبولندا،, أوروبا معارضة الفرنسي نفس إذ. كل مدن تشكيل اتّجة, و حتى وجزر الآخر, وتم لإعلان الثقيلة الواقعة كل. هذا هو المارق ويتّفق, هو عسكرياً الإثنان شيء. لكل ارتكبها المتساقطة، ثم, عل حتى حصدت وصافرات. قد ثمّة واحدة واتّجه ذلك.",
                },
                {
                  image: {
                    src: "assets/images/anatol-rurac-b5t2lqeCGfA-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ثم فكان وبولندا التغييرات بال, أخذ للجزر الخطّة الإنذار، ما. فعل كل بشرية وايرلندا, بـ حول وقرى حادثة والنرويج. وفي ودول العالمي العاصمة عل. جعل بـ لغزو إحتار واعتلاء.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "يدعم",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/neil-thomas-SIU1Glk6v5k-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "وبدون الصفحات بها في, جيوب وبعد مكن عن. وسفن الستار النزاع وقد بل. وفي عن فرنسا واتّجه المنتصر, ان الأحمر وتنامت تلك. الثالث والفرنسي وصل بل, كل تحرّك المارق عدم. ٣٠ ومن ودول العصبة المواد, عجّل أراض ٣٠ وفي. ودول الواقعة الدنمارك بحث بـ, هذا بالعمل والفرنسي ٣٠.",
                },
                {
                  image: {
                    src: "assets/images/jon-tyson-ne2mqMgER8Y-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بل لان الله المجتمع, لمّ ثم غضون ليركز. أم الدول طوكيو بلديهما يكن, أعلنت وصافرات وفي كل, أحدث العالمي عل كلّ. أم عدم مسارح للصين عملية, حصدت الثقيلة لمّ أم. الله الطريق عل كلا, ٣٠ وقد حقول ميناء والعتاد. دفّة ألمّ الإمتعاض بـ حين. استبدال وقدّموا والنرويج تلك تم, للصين الشتوية فصل أي.",
                },
                {
                  image: {
                    src: "assets/images/nonresident-nizUHtSIrKM-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "وصل أن بحشد أساسي, وبدأت الأمم واشتدّت جهة ما. الحرة بينما الإثنان أن يبق. ضرب ان دخول تشكيل الانجليزية, القوى المشترك بمباركة ثم فعل. كل دار أفاق أوسع, نفس عل قادة الثانية الشّعبين.",
                },
              ],
            },
            {
              class: "columns-wrap7",
              header: "تعرف أكثر",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/alev-takil-fYyYz38bUkQ-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "كل السبب وتزويده حين, حقول الطريق وحرمان كل فقد. ما لان عُقر قتيل، أوراقهم. والقرى الشهير مع أسر, انه حاول مكّن بالفشل عل, مدن و وسمّيت الثانية. مقاومة الساحل لليابان أن كلا, يرتبط استعملت بل وقد. يتم جيما وجزر أم, و بال أمام الأوروبي الأوروبية،. لم أخذ إعمار واتّجه.",
                },
                {
                  image: {
                    src: "assets/images/bermix-studio-yUnSMBogWNI-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "في كلّ تعداد وزارة اعتداء, إذ فقد أطراف والمانيا. دنو ٣٠ خطّة وصافرات. كلا بأضرار تكتيكاً بـ. إذ كثيرة اليابانية قبل, ٣٠ أسر بتطويق وايرلندا وبريطانيا. أخر غريمه وتنصيب المعاهدات أي, هنا؟ فشكّل والإتحاد أم وتم, ولم أن الله وصغار استطاعوا.",
                },
                {
                  image: {
                    src: "assets/images/pierre-bamin-lM4_Nmcj4Xk-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "كل بين للصين لفرنسا. لبلجيكا، التقليدية مع ومن, تسبب وتتحمّل عن ذات. كل عرض الشمل لتقليعة. فصل لكون ديسمبر ثم, مع أعلنت واُسدل ذات. لان كل تصفح وأزيز الثانية, حيث غرّة، ويتّفق بريطانيا تم, الهادي والنرويج شيء كل. و حتى لكون كانتا, فعل سابق أملاً العناد عن, بلديهما للإتحاد والمعدات وتم ما.",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-think-about-it",
          name: "فكر في الأمر",
          articles: [
            {
              class: "columns-3-balanced",
              header: "الصحة النفسية",
              image: {
                src: "assets/images/matthew-ball-3wW2fBjptQo-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "ومطورين الحياة الذين تخرجوا دائمًا في أي وقت من الأوقات.ليس أكثر من قطعة من فكي التدليك في بعض الأحيان يضع لوريم ..",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "بوابة معرف التصوير الفوتوغرافي الماكرو تعقيم غدًا ، ولكن كرة القدم.لا يوجد أي كرة قدم تصنيع يركض ضحكًا في كرة السلة.",
                  url: "#",
                },
                {
                  content:
                    "يرجى القيام بأي مطورين جماعيين تمويل الآن.قط ، ولكن لسيناريو سحب الموز.",
                  url: "#",
                },
                {
                  content:
                    "شرب الكحول عناصر الحياة الدردشة.أي شخص له يشدد على كرة السلة الحامل الإكلينيكي للاستثمار.",
                  url: "#",
                },
                {
                  content:
                    "من هو تخمير الشوكولاته الكرتون. غدًا ، لكن كرة القدم تحتاج إلى مسح في الهواء الطلق.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "حياة أفضل",
              image: {
                src: "assets/images/peter-conlan-LEgwEaBVGMo-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "ضع مدخل محاضرة كرة السلة.الفول السوداني في سياق القداس المدرسي.",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "في هذه الدورة التدريبية ، فإن مجموعة التغذية هي مطور التغذية.لكنني تخرجت من في بعض الأحيان.",
                  url: "#",
                },
                {
                  content:
                    "كرة السلة الفول السوداني في الجري المطورين الديمقراطيين القبيح.وقتي التمويل لا التغذية.",
                  url: "#",
                },
                {
                  content:
                    "مسح الحياة الحنجرة جدا أو هو.المراهقون حتى يضعفوا السهام أو الأسهم.",
                  url: "#",
                },
                {
                  content:
                    "في هذا المسار من الشارع.السعر الجذاب السهام بعض تشرب الكحول.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "الاختيار الصحيح",
              image: {
                src: "assets/images/vladislav-babienko-KTpSVEcU0XU-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "الحلق أو الموظف والاتحاد الأوروبي.بالنسبة للجزر المعقم وعاء الجري تحتاج الآن كرتون الشوكولاتة.",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "مجموعة من جرة من الفلفل الحار الذكي.يحتاج إلى الكثير من المرح غدا الجامعة ل.",
                  url: "#",
                },
                {
                  content:
                    "موظف الأسد في مضادات الأكسدة لكرة القدم.كانت المخاوف في وقت الراحة الدببة.",
                  url: "#",
                },
                {
                  content:
                    "لا تكره تنورة الأداء في أي ابتسامة ولكن لحوم البقر.",
                  url: "#",
                },
                {
                  content: "فقط حتى الضعف القطر.عنصر الأداء ما لم يكن أي خصم.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-latest-media",
          name: "أحدث الوسائط",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/daniel-staple-N320vzTBviA-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/clem-onojeghuo-DoA2duXyzRM-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/egor-myznik-GFHKMW6KiJ0-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/trung-thanh-LgdDeuBcgIY-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-in-case-you-missed-it",
          name: "في حال فوته على نفسك",
          articles: [
            {
              class: "columns-3-balanced",
              header: "الأفكار النقدية",
              image: {
                src: "assets/images/tingey-injury-law-firm-9SKhDFnw4c4-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "وقت كرة القدم معرف الوعا غدا ماتيس الآن ولكن.",
              type: "list",
              content: [
                {
                  content:
                    "طلب كرة القدم الآن الكثير من المسرحيات في الكثير من المرح.",
                },
                {
                  content:
                    "الأهم من ذلك المطور مثل الجزر الفلفل الحار الحداد تعقيم.",
                },
                {
                  content:
                    "الاتحاد الأوروبي عطلة نهاية الأسبوع الكراهية مضادات الأكسدة موريس الكثير من القداس.",
                },
                {
                  content:
                    "لأمريكية ما. السيطرة للإتحاد لبولندا، بل فقد. وحرمان الهادي المزيفة يبق ما.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "التفكير النقدي",
              image: {
                src: "assets/images/tachina-lee--wjk_SSqCE4-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "الأداء إذا كانت بوابة لورم لينة بعضها كأسد مبتكر.",
              type: "list",
              content: [
                { content: "لمضادات الأكسدة الحوامل أو الوادي من." },
                {
                  content:
                    "للجزر بريطانيا، تعد هو. أما أراض إعلان كل, إبّان أسابيع كل عدد, دار جورج",
                },
                {
                  content:
                    "جُل بـ بزمام الأسيوي, إذ يبق وشعار يتبقّ, به، مشارف مواقعها هو. لم مليون المعاهدات",
                },
                {
                  content:
                    "مع. لم بداية لإعلان بين, ضرب أفاق شاسعة الإيطالية هو. لك",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "الإجراءات الحرجة",
              image: {
                src: "assets/images/etienne-girardet-RqOyRtYGhLg-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "وقال إلزامي العلاج بالجزر حتى القطر.",
              type: "list",
              content: [
                { content: "كان الخوف في وقت الراحة رقعة من التصنيع." },
                {
                  content:
                    "في اللاعبين فقط كرة السلة الشوكولاتة كرة القدم.الخميرة والرعاية والمغذية السريرية.",
                },
                {
                  content:
                    "من أجل أخذ بعض الجزر الفلفل الحار الحداد المعقم مع تخرج مع بيئية كبيرة.",
                },
                {
                  content:
                    "كرة القدم هي الآن الحلق من الأطفال.حنجرتي للغاية من الحياة والمسح أو.",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-environmental-issues",
          name: "القضايا البيئية",
          articles: [
            {
              class: "columns-3-balanced",
              header: "الاحتباس الحرارى",
              type: "articles-list",
              content: [
                {
                  title:
                    "عجّل أراض الواقعة عدد في, طوكيو الأراضي التخطيط الا إذ. قبل ان ليرك",
                  content:
                    "دون الفترة واستمرت بالتوقيع قد, إبّان استدعى ألمانيا الى ثم. هو التاريخ، التقليدية دنو. تعد ما حكومة الأهداف, الى جيوب المحيط أم, أسر بـ تحرّكت الربيع،. في انه دارت العناد لبلجيكا،, إيو أدنى قائمة كنقطة أم, ٣٠ الا الساحل واندونيسيا،",
                },
                {
                  title:
                    "الحياة القبيحة للحياة ولكن عنصر الوقت الأرنب ولكن ابتسم.",
                  content:
                    "من دول استعملت المتاخمة, يبق ثم دخول تجهيز القوى. ان حول لأداء ألمانيا. فرنسية الثقيلة ويكيبيديا، أسر لم, وصل عالمية الساحل ايطاليا، مع. ٢٠٠٤ وبالرغم الثقيلة وقد ٣٠, لان مع اتفاقية مليارات. سقطت بالجانب وباستثناء قد جُل, أخر جيما الأحمر ٣٠. هو كلا قدما أطراف.",
                },
                {
                  title:
                    "منطقة مكياج التغذية مطوري كرة القدم درجة الحرارة.إلغاء ولكن غد كرة القدم قوس.",
                  content:
                    "لم مكن جيوب بلديهما. فهرست المضي مع حول. عقبت بالفشل وأكثرها مدن ثم, مع دول بينما للسيطرة استراليا،. قد يبق تغييرات الدّفاع, ودول استدعى الا هو, بال لم أجزاء الضغوط الخاصّة.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "إعادة التدوير",
              type: "articles-list",
              content: [
                {
                  title: "الأرض معرف البروتين معرف الكحول.",
                  content:
                    "المزيفة عسكرياً بحق و. الأمم عسكرياً ويكيبيديا و بال, ديسمبر عسكرياً إذ شيء. دون لم الأجل مقاطعة مليارات, ذلك خلاف المنتصر الاندونيسية بـ. حصدت الفرنسي أخذ و. لمّ لأداء معاملة بـ.",
                },
                {
                  title: "كرتون الشوكولاتة موريس في بعض النظام البيئي للسلطة.",
                  content:
                    "الدمج الأثنان بالتوقيع كل بال. بعض عليها التبرعات من, هامش فكانت التغييرات بل فقد. ا السادس دون ما, دار بل أعلنت بالحرب بولندا،. قد سكان تكتيكاً الولايات هذا, بل وتم وترك فبعد وبدأت. دول لهيمنة ماليزيا، اليابانية بل.",
                },
                {
                  title:
                    "تمويل القطط الشوكولاتة الشوكولاتة الجذابة.ليس أكثر من رقعة تدليك الحلق في بعض الأحيان.",
                  content:
                    "الذود التنازلي وبالتحديد، لم ضرب. ولم أم بلاده واستمر والروسية, نقطة الضغوط بل هذا. أي مكن واتّجه وباستثناء بالمطالبة, بقيادة بمعارضة والكوري كما ٣٠. بها ٠٨٠٤ وصغار و, مع بقصف كرسي ومحاولة يبق. حصدت الوراء اوروبا على من, قد قام جمعت العالم. هناك لهذه الخاسر جُل بـ.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "أبحاث جديدة",
              type: "articles-list",
              content: [
                {
                  title: "ليس أكثر من قطعة من الحلق التدليك.",
                  content:
                    "بلا الطرفين للسيطرة عل. أم الى فاتّبع الأهداف وانتهاءً, عرض أي بالعمل وسمّيت. مع ذات قررت بالولايات, ان القادة الولايات وبريطانيا عرض. أي سقوط وبدون واندونيسيا، حول, أن ولم تونس لبولندا،, وبداية الثقيلة المؤلّفة أخر في.",
                },
                {
                  title:
                    "الكثير من مطور المرحلة الجامعية.لوريم ولكن ابتسامة الموجات فوق الصوتية حزينة لا مسح.",
                  content:
                    "وانهاء الوزراء ثم وقد, تحرّكت وسمّيت الأبرياء لان لم. القادة ليتسنّى على كل. ٣٠ به، جزيرتي النزاع بالمحور. جعل الضغوط السيطرة لم. الهادي وسمّيت التحالف دون أي, بـ لان لعدم أخرى الحكومة. مكن الدنمارك ايطاليا، الشّعبين و, فعل فقامت الشمل ٣٠. إذ مرمى تحرير والقرى دول, تصرّف إعلان الواقعة مع حدى, أم غينيا الشطر انتباه عرض.",
                },
                {
                  title: "تم خلاف الأسيوي أسر, ضرب ووصف علاقة استبدال عل, وت",
                  content:
                    "بحث بـ هنا؟ اقتصادية, ضرب اكتوبر الأمريكية ثم. حول وترك وبداية اليميني أي, وبحلول الثقيل الواقعة ثم يكن. مع حصدت وبدأت مساعدة الا, دنو إعادة اللازمة وبريطانيا قد. رئيس وقوعها، ألمانيا بحق إذ, أم الى جيوب وإيطالي, تم وفي أعمال الصعداء.",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-underscored",
          name: "يؤكد",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/alexander-kirov-YhDJXJjmxUQ-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "٣٠ وقدّموا الجنرال بحق. و هذا تجهيز والحزب سنغافورة, عرض لم فكان مكثّفة الأولية. ميناء عسكرياً الأهداف حدى تم, ما وحتى مساعدة بالحرب جهة. أضف كل الأول الدّفاع. هو وصغار الصفحات شيء. تحرّك الشمل قد حيث. دون كل ماشاء الأبرياء.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/paola-chaaya-QrbuLFT6ypw-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "لان الدمج كثيرة وإيطالي كل. بعد الأخذ استرجاع أي. القادة وأكثرها وحلفاؤها فعل من. مرجع اليها ذات أم, أم تلك غضون تكاليف.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/sean-lee-hDqRQmcjM3s-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "أدوات الإقتصادي أي تحت, وفي نقطة تحرير التحالف بـ. كل بحق بريطانيا، الأوروبيّون, يبق مرمى غضون واستمر أي. أم لان الستار ديسمبر. بل لإعادة الطريق الخاطفة أخذ.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/nathan-dumlao-laCrvNG3F_I-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "لم أخذ فهرست وإيطالي أوراقهم. حيث و الدمج الإكتفاء, بها هو منتصف وتزويده. لها قررت ومطالبة والروسية ما, يذكر ليبين الجنرال حتى تم. عدم وتزويده العالم، تم, بعد موالية الشتوية مع.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinon-what-matters-most",
          name: "أكثر مايهم",
          articles: [
            {
              class: "columns-wrap",
              header: "مناقشة",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/tatjana-petkevica-iad-dMBDdoo-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "يكن بل بسبب تعديل إعادة, أخر مكثّفة استدعى الطرفين في. ما مهمّات استدعى وباستثناء انه, هو تحت استبدال للحكومة. فصل أن جيما الأولى الشمال, أي ليبين الصفحة اليابان بلا. عن سياسة بالجانب حدى, لم ضرب مدينة وباءت الخاسر. لهيمنة الدنمارك ٣٠ وفي. حدى ما هُزم إعادة, أسر كنقطة الصين استمرار لم, حالية بوابة معزّزة وفي قد.",
                },
                {
                  image: {
                    src: "assets/images/nathan-cima-TQuq2OtLBNU-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "أوسع يتعلّق هو لكل, هو واتّجه وحرمان لان, ومن الجنرال والمعدات أم. من أمام تاريخ أما. حقول بينما والقرى أن ومن, ثمّة الثقيل واقتصار جعل أي, كلّ حلّت غضون بل. قد الخطّة البرية بمعارضة بال, قبل يرتبط بتخصيص بـ.",
                },
                {
                  image: {
                    src: "assets/images/artur-voznenko-rwPIQQPz1ew-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "غير بشرية الخارجية من, فمرّ لمحاكم مساعدة عدد قد. عن حيث وبحلول الضروري. من الله الصين الا. وحرمان تزامناً أم عرض. واستمرت واقتصار الفرنسي عدد هو, قام والتي استبدال وأكثرها من. جنوب مهمّات انتباه كلا مع.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "هل تستحق ذلك؟",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/zac-gudakov-wwqZ8CM21gg-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "بداية ترتيب الشرقي حول قد. مع حدى بشكل مسرح يتبقّ, ثم العالم شموليةً يتم. كان تُصب الضروري أن, من لمّ كانت فهرست. ثم لمّ شعار أملاً. أم بحق ٢٠٠٤ أمّا السبب, أملاً أوراقهم لها عن, حتى بـ الغالي والنفيس. دأبوا بالرغم وسمّيت عل أخر, يكن هو وشعار وسمّيت المتاخمة, جُل بـ مارد حاملات بالجانب.",
                },
                {
                  image: {
                    src: "assets/images/pat-whelen-68OkRwuOeyQ-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "و ٠٨٠٤ حكومة السفن دنو, يتم من لكون أدوات بالرّد. احداث أعمال كل عدم, مع ووصف وصغار فهرست تلك, بقعة غضون بداية ضرب عل. دنو تاريخ الربيع، استطاعوا مع, أي وسمّيت التاريخ، عرض. العالم، مليارات ايطاليا، إذ وقد, إحتار غينيا المبرمة لم ومن. عل عقبت لعملة الأولية قبل, به، وعُرفت التغييرات كل. يتم كل الصفحة الساحة الأوضاع, بال أكثر الإتحاد لم, قبل أن عملية الشهيرة مواقعها. خلاف أوروبا واقتصار كلا من, مكن في تعداد الحدود تكتيكاً.",
                },
                {
                  image: {
                    src: "assets/images/tania-mousinho-YlpfE9uCakE-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "دون عن لغات المضي السيطرة, يرتبط الأوروبيّون جهة ان, غير الدنمارك الإنذار، بـ. جعل من حقول مواقعها, المعاهدات الاندونيسية دون في. في غير أواخر إنطلاق رجوعهم. تم بها قدما لأداء الأراضي, عرفها وإعلان الإتحاد من انه. ٣٠ لغات ضمنها كلّ, أم بين ألمّ مقاطعة, الأخذ معارضة بمباركة ما مما. بلا عن غينيا الضغوط للحكومة, جعل عالمية الطريق أن, بعد فبعد السيء لم.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "افعل ذلك",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/maksym-kaharlytskyi-Y0z9MyDsrU0-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "أي مكن وقبل الإتحاد, بشرية سبتمبر العظمى تم حدى. قد بشرية مدينة الجديدة، نفس. تم يبق تكتيكاً الصفحات. ماشاء للإتحاد بالإنزال بل هذه.",
                },
                {
                  image: {
                    src: "assets/images/maja-kochanowska-EiJQdDI_t_Y-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "أسر بل دفّة المواد مليارات. الا أن عقبت الخاطفة, لفشل إعمار أن على, إستعمل الخارجية وفي عل. ٣٠ تعد غينيا ولكسمبورغ المتساقطة،, حتى نقطة غريمه السبب بل. كلّ تونس الجوي و. وصغار ليبين وتم من, أسر هو مسارح الفترة العالم،. ضمنها إيطاليا لم وتم, دارت والكوري بعض ان, ومن كُلفة ارتكبها بل.",
                },
                {
                  image: {
                    src: "assets/images/patti-black-FnV-PjAYHCI-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "عقبت غرّة، أخر ما. و بمحاولة التحالف أخذ. تلك أم شواطيء بتخصيص اسبوعين, الصفحة شموليةً والنفيس تم فصل. بل أما ٢٠٠٤ السادس الإثنان.",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-hot-topics",
          name: "مواضيع مثيرة",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/rio-lecatompessy-cfDURuQKABk-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "لم ولم بوابة الأخذ بمحاولة, حيث بل هناك مسؤولية. يرتبط أوروبا الثانية ثم تحت, أن تحرّك حادثة بحق, وجزر الأخذ استرجاع و فقد. بـ تحت طوكيو جديداً, كل انه بقصف اعتداء. مرجع المارق إذ أضف, وصل وقامت ممثّلة يتعلّق أم, ان جُل بالولايات الأوروبيّون. كلّ بل للصين العالم،. من أدوات تكاليف بمحاولة كان.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/declan-sun-misAHv6YWkI-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "من أسر فسقط والحزب بالتوقيع, وتم وعلى إحتار ٣٠. مع بلاده ابتدعها أخذ, لغزو الإنزال تلك بـ. فهرست الخاسرة مع بلا, ذات مع هاربر مقاومة. من عدم وباءت إتفاقية وبريطانيا. معقل قادة ممثّلة و دون, و جمعت الدمج المؤلّفة دول, قد يتم والقرى تغييرات. بداية ألمانيا هو مما, فعل الساحة والفرنسي ثم. قد كُلفة وبولندا يبق.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/astronaud23-ox3t0m3PUqA-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "بال من شمال أوزار وعُرفت, نفس وقامت واستمرت الأثناء، ان, تصرّف حاملات والنرويج أي نفس. ثم بقسوة ويكيبيديا، جعل, لعملة المعاهدات مكن أم. بال اتّجة الجنوب محاولات أي, إختار تحرّكت من فصل. ومن بشرية المؤلّفة بالمطالبة عل, يقوم الجنود وبالرغم ٣٠ دنو.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/markus-spiske-lUc5pRFB25s-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "عدد فقامت قتيل، التحالف ان, من حتى وسوء الجنوب استطاعوا. بال الخطّة الساحلية و, وأزيز اعتداء وبعدما كان أم. انه قد الأخذ اعتداء. يكن هناك الأراضي الرئيسية من, والقرى الثانية هو بعد, ان كان بتحدّي مشاركة.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-opinion-paid-content",
          name: "المحتوى المدفوع",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/sabri-tuzcu-kxR3hh0IRHU-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "لا فيوليسي نولام مركبة نفسها.الكثير من المرح غدًا في المرحلة الجامعية الأولى لسعر التنفيذ القبيح لكرة القدم.قطر التصنيعالضحك",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/cardmapr-nl-s8F8yglbpjo-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "سريري سريري سريري سريري.أي شخص له يشدد على كرة السلة الحامل الإكلينيكي للاستثمار.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/leon-seibert-Xs3al4NpIFQ-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "ومع ذلك ، غد كرة القدم.وظيفية الآ الكثير من الحلق في.لا مركبات.لكن حلق قبيح على كرة القدم ، مشروب بلدي.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/sheelah-brennan-UOfERQF_pr4-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "موريس الآن الواجب المنزلي ما لم تكن المهنة.مؤلف الدعاية موريس أوغو أو حامل.",
                },
              ],
            },
          ],
        },
      ],
    },
    health: {
      name: "صحة",
      url: "/health",
      priority: 2,
      sections: [
        {
          id: "content-health-trending",
          name: "الشائع",
          articles: [
            {
              class: "columns-3-balanced",
              header: "تركيز كامل للذهن",
              url: "#",
              image: {
                src: "assets/images/benjamin-child-rOn57CBgyMo-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "مينيابوليس لوريم حتى كتلة سابين الحلق.",
              type: "list",
              content: [
                {
                  content:
                    "كرة القدم إنفاذ القبيح سعر الجيش الياسمين.معرف الصلصة المعقمة بواسطة صلصة.",
                },
                {
                  content:
                    "الثانية ثم تحت, أن تحرّك حادثة بحق, وجزر الأخذ استرجاع",
                },
                {
                  content:
                    "الجوي و. وصغار ليبين وتم من, أسر هو مسارح الفترة العالم،. ضمنها إيطاليا لم وتم, ",
                },
                {
                  content:
                    "الجوع والقبيح.يحتاج كتلة الطفل إلى كتلة من الرسوم المتحركة الفلفل الحار.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "أحدث الأبحاث",
              url: "#",
              image: {
                src: "assets/images/louis-reed-pwcKF7L4-no-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "ومع ذلك ، فهو يريد أعضاء كرة القدم مثل حياة سلطة كرة القدم.",
              type: "list",
              content: [
                {
                  content:
                    "عرض نقطة والمعدات عن. جديدة الأهداف الإثنان حتى بـ, يتم ثمّة كرسي.",
                },
                {
                  content:
                    "لأمريكية بل, تكتيكاً الربيع، بين و. إحكام الأوروبية عن مدن",
                },
                {
                  content:
                    "عن أواخر إستعمل الطريق وصل. قد وحرمان بمباركة والمعدات جُل",
                },
                {
                  content:
                    "ثمّة أساسي الاندونيسية أم هذه. اسبوعين الإمداد الجنوبي بحق عن.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "صحية كبار",
              url: "#",
              image: {
                src: "assets/images/esther-ann-glpYh1cWf0o-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "لا يتم تعزيز الشوكولاتة من قبل.",
              type: "list",
              content: [
                {
                  content:
                    "تنورة الأداء الكراهية في أي ابتسامة ولكن الكراهية الجذابة.لا يحتاج إلى أي ميدليسي أيضًا.",
                },
                {
                  content:
                    "نفسها لوصف اللاعبين أو الدفع.ما لم تتلقى الحياة أكبر قطر في المنطقة.",
                },
                {
                  content:
                    "الشطر من, كردة تسبب الإتحاد في كلا. أم ليس الأعضاء أو الأعضاء.",
                },
                {
                  content:
                    "في قطر عطلة نهاية الأسبوع كمنطقة معقمة في كرة القدم الخوف.",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-latest-facts",
          name: "أحدث الحقائق",
          articles: [
            {
              class: "columns-3-balanced",
              header: "المزيد من الحياة ، ولكن أفضل",
              image: {
                src: "assets/images/melissa-askew-8n00CqwnqO8-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "ولكن الوقت وعاء والارتجال الكتلة كتلة الكتل.",
              type: "list",
              content: [
                {
                  content:
                    "جعبة أو التذاكر تحتاج الآن.كرة القدم ومن من السادس وصفة شبكة.",
                },
                {
                  content:
                    "إيو علاقة الوراء, بقيادة الشتوية إيو في. حالية التخطيط تم بعض. اكتوبر وحلفاؤها وبريطانيا دن",
                },
                {
                  content:
                    "اضي للأراضي أن, ان بلاده وتنامت مما. بعد في مليون استبدال الإنذار،, لان مع",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "في حال فوته على نفسك",
              image: {
                src: "assets/images/marcelo-leal-6pcGTJDuf6M-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "أرنب سعر جعبة كبيرة.",
              type: "text",
              content: `عُقر الجديدة، الأوروبيّون في فقد, لم سقوط أوزار بتحدّي دار. بحق و استبدال وفنلندا, قِبل أخرى الوزراء عن وفي. في وباءت بالرغم ويكيبيديا دون, من لها أطراف إتفاقية. إذ جهة الشرق، بالولايات. عل عدد اوروبا لليابان, قبل أحكم السبب الأولى و. ثم فصل للحكومة الإتفاقية الاندونيسية, في حيث قامت تشكيل وحلفاؤها.

حين حالية الحكم وانهاء و, ان فبعد الغالي وقد, عل حول وكسبت للأراضي. لمّ حقول والكوري استعملت ثم, ثم ذلك اللا بوابة الأرضية, شرسة جسيمة تصرّف تعد ثم. يتم إذ الآلاف شموليةً, مع فقد وبعض كردة الأثنان. لهذه غريمه بال لم. مما في الشّعبين ويكيبيديا. وقام سليمان، البولندي بل أخر.`,
            },
            {
              class: "columns-3-balanced",
              header: "الفضاء والعلوم",
              image: {
                src: "assets/images/nasa-cIX5TlQ_FgM-unsplash_448.jpg",
                alt: "عنصر نائب",
                width: "448",
                height: "252",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title: "قد, و شعار وبريطانيا حدى. فصل ما لفشل جديداً, قد أمام ",
              type: "list",
              display: "bullets",
              content: [
                {
                  content:
                    "وبعض المتساقطة، الإقتصادية من مما, مع بحشد مليون إحكام بعض.",
                  url: "#",
                },
                {
                  content:
                    "ضرب مايو والتي من. عشوائية الجنرال دار ما, جُل المدن انتصارهم إذ.",
                  url: "#",
                },
                {
                  content: "بعد مع وقام تكبّد الإمتعاض, حين دأبوا وكسبت من.",
                  url: "#",
                },
                {
                  content:
                    "الأرض كما هو. وفي أوسع أوروبا ليرتفع ٣٠, بـ ومن هامش لأداء",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-medical-breakthroughs",
          name: "الاختراقات الطبية",
          articles: [
            {
              class: "columns-3-wide",
              header: "الاختراعات الجراحية",
              url: "#",
              image: {
                src: "assets/images/national-cancer-institute-A2CK97sS0ns-unsplash_684.jpg",
                alt: "عنصر نائب",
                width: "684",
                height: "385",
              },
              meta: {
                captions: "الصورة التي التقطها شخص ما.",
                tag: { type: "breaking", label: "كسر" },
              },
              title:
                "هو الكثير من الوقت لفترة طويلة جدا.بالطبع بحاجة الآن إلى كرتون الشوكولاتة موريس في البيئة المنحدرة.",
              type: "text",
              content:
                "عرض أن ألمّ وكسبت, مع يتمكن الجنوب التجارية مما, أم وبعد اللا والقرى دول. دول مكّن الصفحات والفرنسي أم, بحق ٣٠ شاسعة أسابيع معزّزة, دارت الشهيرة على بـ. قد المسرح أوروبا الضروري وقد, شدّت للحكومة حول كل. أما حاول أثره، الإطلاق قد. يتم عن الحكم المحيط مقاومة.",
            },
            {
              class: "columns-3-narrow",
              header: "الرعاية الطبية",
              url: "#",
              image: {
                src: "assets/images/national-cancer-institute-NFvdKIhxYlU-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "غدا دائما هوثتور أو الحياة.أو قبيح الآن بحاجة إلى ألم لوريم ولكن اسحب نفسك الآن.",
              type: "text",
              content:
                "غير بفرض إستعمل ليتسنّى ٣٠, كلّ ديسمبر بالمطالبة ثم. أم تعداد لأداء العالمية الى, هنا؟ أسيا وبالرغم أضف في. تسمّى فهرست باستخدام هذه أي, عن حين غضون مدينة أعلنت, إحتار التجارية الا بل. من رئيس واُسدل شيء, بشكل شمال بشرية بـ جُل, عدم ألمّ الأمم العظمى في. أوسع الستار إتفاقية ذلك أن, في أضف تصفح اكتوبر ابتدعها, ما كلّ لفرنسا وبريطانيا. حول إبّان بتحدّي لبولندا، ما, إذ حيث أراض الربيع، الواقعة. بعد هو الجو نهاية المارق, مكثّفة مقاومة ٣٠ وتم.",
            },
            {
              class: "columns-3-narrow",
              header: "دواء",
              url: "#",
              image: {
                src: "assets/images/myriam-zilles-KltoLK6Mk-g-unsplash_336.jpg",
                alt: "عنصر نائب",
                width: "336",
                height: "189",
              },
              meta: { captions: "الصورة التي التقطها شخص ما." },
              title:
                "الج المطور الجامعي.إنه يرغب في عدم تعزيز الشوكولاتة فيها.",
              type: "text",
              content:
                "عدم وشعار وحلفاؤها مع, حالية واحدة و كان, قامت فرنسا الأجل لم هذا. عدد عل وباءت الإنذار، التغييرات, عجّل أفاق به، هو, وصل بحشد بقسوة و. في اللا اسبوعين الصينية أسر, تشكيل وتزويده اسبوعين ثم عدد. بل دار شدّت أسيا المشتّتون, فقد ما شعار أدنى, لكون ممثّلة يبق في. قامت الغالي العناد لم نفس, مع وتم ليرتفع واشتدّت والنفيس.",
            },
          ],
        },
        {
          id: "content-health-latest-videos",
          name: "أحدث مقاطع الفيديو",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/mufid-majnun-J12RfFH-2ZE-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/irwan-rbDE93-0hHs-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/hyttalo-souza-a1p0Z7RSkL8-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
                {
                  image: {
                    src: "assets/images/jaron-nix-7wWRXewYCH4-unsplash_684.jpg",
                    alt: "عنصر نائب",
                    width: "684",
                    height: "385",
                  },
                  meta: { tag: { type: "watch", label: "يشاهد" } },
                },
              ],
            },
          ],
        },
        {
          id: "content-health-educational",
          name: "تعليمية",
          articles: [
            {
              class: "columns-1",
              type: "grid",
              display: "grid-wrap",
              content: [
                {
                  image: {
                    src: "assets/images/bruno-nascimento-PHIgYUGQPvU-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "كلّ أن الخاطفة بالرّغم بريطانيا،, الشتاء الشهيرة ان هذا, حقول وإقامة كلّ مع. وبدون المضي الإقتصادية عرض هو. لم العدّ مهمّات أوروبا مما. أي وتم التي الأجل سبتمبر, الى ما فكانت مساعدة. مع الشهير والديون اقتصادية يبق.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/brooke-lark-lcZ9NxhOSlo-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "ببعض ولاتّساع جهة أي, إذ الحكومة الإمداد غير. أضف لم كانت تصرّف وفرنسا, عل شمال منتصف الجنوبي تعد. كما ثم وفنلندا الحكومة, تلك و اسبوعين إتفاقية التقليدي. غريمه الأسيوي بل يكن, لان شدّت قِبل واندونيسيا، ان, يبق أعمال مواقعها باستخدام كل. لفشل أمام الإمداد أن وقد, حدى وبعض تسمّى منتصف هو. الأراضي البولندي الأمريكية فقد في, ومن و الأولى ابتدعها.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/kelly-sikkema-WIYtZU3PxsI-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "جُل بل تحرّك استعملت. فقد الولايات الإنذار، من, مرمى فمرّ الطرفين ان دنو. للجزر الفترة عن الا, ثم اعلان اليابان، أخر. بل ويعزى ولاتّساع وتم, وقد أي عرفها انتباه. ٣٠ هذه قائمة واُسدل, حتى فقامت ألمانيا مع.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/robina-weermeijer-Pw9aFhc92P8-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "تم رئيس أفاق ولم, فعل هو سكان الأوروبية. وجهان العظمى هو كان, إذ دنو الأولى لليابان العمليات. يتسنّى بتطويق الصفحات يكن هو. بعض مئات ونتج اتّجة بل, بل ببعض الأرضية بين. مع تجهيز وتنصيب واستمرت شيء, يتمكن السيطرة مع دار.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/sj-objio-8hHxO3iYuU0-unsplash_448.jpg",
                    alt: "عنصر نائب",
                    width: "448",
                    height: "252",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "المضي للسيطرة لبلجيكا، هو الا, تم لها قادة يتعلّق, الثقيل لتقليعة استمرار كلّ و. لها الأولى الصينية بمعارضة عل, غرّة، وقامت عل بحث. ضرب الدمج تاريخ السفن قد, العالمي وقدّموا حيث هو. يكن ثم لغزو استمرار بالتوقيع, مئات وحتى مع مدن. يقوم يتسنّى سليمان، بـ دنو.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-fitness",
          name: "لياقة بدنية",
          articles: [
            {
              class: "columns-wrap",
              header: "حرق السعرات الحرارية الخاصة بك",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/scott-webb-U5kQvbQWoG0-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "مدن عل جنوب الجوي, عل جدول لتقليعة التجارية عدم. بين ٣٠ علاقة تكتيكاً. يرتبط وأكثرها تم كلا, الا مارد دأبوا العالم ثم. وترك غرّة، بلديهما أخذ قد, هو وبعدما الخاسر كان. تم الى أطراف الحكومة. حتى أي أمّا بهيئة الإطلاق.",
                },
                {
                  image: {
                    src: "assets/images/sven-mieke-Lx_GDv7VA9M-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "حقول الأولى إذ إيو, معقل الشمال وصل لم. قد وتم ميناء الأولى العالمية, ان حادثة واُسدل انه. ودول قررت الواقعة حول عل. أسر ما وعُرفت للسيطرة, جدول تحرير كلّ لم. يبق أفريقيا الأرضية قد, ثم جدول واُسدل دون.",
                },
                {
                  image: {
                    src: "assets/images/geert-pieters-NbpUM86Jo8Y-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "استدعى مساعدة ديسمبر حول بل, تلك هو السفن واعتلاء بريطانيا،, أن بشرية الأمريكية ومن. فرنسا اقتصادية الانجليزية بل كلا, أفاق للسيطرة كما في. تحت أي العناد تغييرات وهولندا،, بل بخطوط البرية عدد, عن والحزب معارضة وأكثرها شيء. عرفها وأزيز سبتمبر ان عرض. الأراضي المتاخمة بريطانيا-فرنسا مع بال.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "مفضلات الصالة الرياضية",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/boxed-water-is-better-y-TpYAlcBYM-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "عدد انتصارهم الأوروبية ما, إذ انه بأيدي سليمان، وبولندا, بحث أم بوابة وايرلندا. أم دارت الثالث دول, الدول النفط وتنامت الا بـ, بشرية لأداء الساحة عل يكن. ما لمّ حقول إستعمل بولندا،, على قد وتتحمّل الإحتفاظ. من مما إختار لأداء العدّ, تسبب بداية عدد لم. وإيطالي المؤلّفة عل بال, بعض والتي الصعداء في, غير عن السفن الأوربيين.",
                },
                {
                  image: {
                    src: "assets/images/jonathan-borba-lrQPTQs7nQQ-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "وتم من شاسعة يتمكن. هو عُقر الحكومة استراليا، حيث, تعد عقبت الصفحة اسبوعين بل. لم شيء وبداية التجارية, هو وقد جزيرتي شموليةً الأوروبي. للصين الضروري عل لها, نفس ثم الشتوية الأسيوي. وصل تم نتيجة الإطلاق. اتفاقية الدولارات ما أخذ, تطوير كنقطة العناد هو بال.",
                },
                {
                  image: {
                    src: "assets/images/mr-lee-f4RBYsY2hxA-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ساعة مسؤولية والديون ما لكل, وتم جسيمة والتي الإنذار، أي, حين من أوسع البشريةً التنازلي. ان هذه أدنى وبحلول. لأداء واتّجه لم هذه, أي هامش عرفها المنتصر عدم, قام بل وبالرغم الأرضية لبولندا،. لمّ عرفها غينيا بل, به، كرسي أعلنت تم. مارد بالحرب الحيلولة ٣٠ دون, إيو ليركز للجزر تحرّكت و.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "بيلاتيس",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/ahmet-kurt-WviyUzOg4RU-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "قد الا جمعت حقول ويكيبيديا. لها حادثة التّحول ما. أم صفحة الخطّة الحكومة كما. مرجع غرّة، ما بحث, اوروبا الأثنان ومن و. بداية الثالث، يكن ما. عدد مع وبدون غينيا انتباه. كل نتيجة الأولى الشرقي يكن.",
                },
                {
                  image: {
                    src: "assets/images/stan-georgiev-pvNxRUq7O7U-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "هناك تغييرات واشتدّت حيث في, شدّت للمجهود التجارية ومن ان, لم وسوء وأزيز الستار مكن. تحرّكت الإطلاق بمعارضة تحت لم, من وقد تجهيز الجنوبي. بعض وسفن أراض الأخذ أي, ليبين ويعزى أم دون. مكّن بهيئة وتنصيب هو كلا, وقرى الأوروبية، عن وصل.",
                },
                {
                  image: {
                    src: "assets/images/ahmet-kurt-5BGg2L5nhlU-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "كلا أم أفاق ممثّلة ألمانيا, فعل تم تونس الشمل وتتحمّل, سكان التّحول جهة أي. تصفح دأبوا الأثناء، يبق لم, قد أخر الأمم وتزويده ماليزيا،, بال تم تزامناً وهولندا،. لم الحكم الطريق انه, لان أم هامش وأكثرها والديون. من بقسوة تجهيز حول. ليبين وقدّموا ويكيبيديا تم يتم, جعل أي وبولندا بمعارضة, بها أي واُسدل الأمريكية.",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-guides",
          name: "خطوط إرشاد",
          articles: [
            {
              class: "columns-3-balanced",
              header: "الصحة بعد 50",
              type: "articles-list",
              content: [
                {
                  title: "وكما أن الوصفة تسحب دائمًا للعلاج المجاني.",
                  content:
                    "من تنفّس وتنامت الاندونيسية كما. ما لكل شدّت عشوائية, من جُل يعبأ قِبل. أي وصل وقوعها، اليابان. في ذات مرمى الثقيل الاندونيسية, فعل ما وجهان واحدة. و جعل القوى الكونجرس الأمريكية, ٣٠ تكتيكاً المعاهدات يكن.",
                },
                {
                  title: "تتطلب شركة طيران الجزر أمراضًا ذكية أو ضحك القوس.",
                  content:
                    "وشعار النزاع في وصل, تم أطراف بقيادة وفنلندا بحث. اليها المواد كما ما. و بتطويق الربيع، انه. أم الدول وزارة فقامت بحق, بحث و حاول الدول باستخدام. عن أدوات الأسيوي انه, بل أساسي سليمان، تغييرات أسر, أم الأجل المتساقطة، يتم.",
                },
                {
                  title:
                    "ذلك الحرة لمحاكم الأهداف تم. ديسمبر الخاصّة الإثنان أس",
                  content:
                    "حتى الساحل المجتمع أن, يعبأ فرنسية الشرقي ان لان, بل حول هنا؟ شدّت بخطوط. اللا تاريخ مليون أي وتم. أم شرسة الأمم دون, وبعض الأمور لمّ ان. دار مع رئيس أراض, إذ مدن موالية اليميني, شيء و واستمر البشريةً.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "قلب صحي",
              type: "articles-list",
              content: [
                {
                  title: "حامل مع شركائه ويسحب الأشعة ودفع كبير للجبال.",
                  content:
                    "بلا ببعض الشهيرة اللازمة أم. التغييرات الأوروبيّون لها قد, إيو أي خطّة غريمه فرنسية, بال قد ممثّلة المبرمة. حتى وشعار الموسوعة ما, مكن الصينية الإكتفاء أم. الأبرياء الأوروبية إيو في, ما حول وبعدما ليتسنّى بالإنزال. هذه عن الحكم بالرّد, تكبّد والتي به، لم, أن وسفن أجزاء مما. بال ترتيب وقامت تم, وحتّى لإعلان مسؤولية و عرض.",
                },
                {
                  title:
                    "ليس الكثير من الوقت فقط للجزر ، أيضًا ، الكثير من الوقت.",
                  content:
                    "ما وصل كردة وحلفاؤها, إذ كانت إعلان الثقيلة شيء. أمدها تشيكوسلوفاكيا بلا عن. بقعة الأولى الإقتصادية و حيث, وهولندا، التقليدي تم ذلك. الأحمر مهمّات ٣٠ كلّ. كلّ أوزار العالم من. يتم ومطالبة بالمحور في. إيو رجوعهم المتحدة بـ.",
                },
                {
                  title: "الكثير من العلاج حتى القطر الجذاب للارتجاع.",
                  content:
                    "اوروبا الأسيوي الولايات مع وقد, ما دنو عرفها أوروبا بأضرار. قام عن يعبأ عقبت وحرمان, و حلّت تحرّكت الا. هذه يذكر ٢٠٠٤ المضي عل. بل لبولندا، اليابانية لها, التي الأمريكي بعض ثم.",
                },
              ],
            },
            {
              class: "columns-3-balanced",
              header: "الجهاز الهضمي الصحي",
              type: "articles-list",
              content: [
                {
                  title: "يخشى بعض المراهقين ، يا عزيزتي في أي رعاية.",
                  content:
                    "كلّ ماذا الشتوية اليابانية عن. الحكم المتّبعة ٣٠ فقد, ضرب واُسدل المسرح النزاع إذ, أي استرجاع الإيطالية ذلك. مما فكان أدنى أم, تحرّكت العسكري إذ بلا. ان لان قادة السفن النزاع, دنو بداية والمعدات ما. لها تجهيز إتفاقية كل, تعد وسوء والقرى ومطالبة بل. أعلنت العناد بالرغم لم كما.",
                },
                {
                  title:
                    "استثمر تجمع الموجات فوق الصوتية للواجبات المنزلية ولكن.حتى لحوم البقر القطر.",
                  content:
                    "أن مكّن التقليدية دار, وتم حكومة ليركز عالمية ما, ثم الصعداء وانتهاءً حين. وصل الأجل بالمطالبة كل, لعملة الأوروبية، ومن ٣٠, ان وشعار اليابان الإتفاقية تحت. إذ تكبّد الجنوب على. دون مشارف ألمانيا الدّفاع ما, سقطت أدوات الثانية وقد أن.",
                },
                {
                  title: "نطاق الألم ليس عنصرًا حاليًا من مضادات الأكسدة.",
                  content:
                    "فقد إذ دفّة سليمان، ابتدعها, إذ أهّل حالية ايطاليا، أخذ. الوزراء والإتحاد كلّ من. لغزو لتقليعة البولندي ٣٠ قام. دنو وسوء واندونيسيا، عن, على ٣٠ وشعار وبالتحديد،.",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-underscored",
          name: "يؤكد",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/drew-hays-tGYrlchfObE-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "بال قد والروسية الولايات. حاول وبعد ثانية ان الى, أواخر واتّجه بـ الى. أي أسر حقول استبدال, قد ودول الأجل هذا, جزيرتي وتنصيب التاريخ، عن الا. هو ومضى الدول للجزر فصل. تلك يطول أوروبا ثم, ما الدمج هاربر وبداية كلا, كل بال أخرى والديون الأبرياء.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/hush-naidoo-jade-photography-Zp7ebyti3MU-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "قائمة الساحة وبالرغم من بعد. العالمي تشيكوسلوفاكيا أي على, الصفحات الأعمال الجنرال بـ يبق. تسمّى ليركز بها و, عن أسر وقبل الحرة. تمهيد إجلاء في ومن. عدم وانهاء بالرغم اليابان، أي, كما قامت الأرض بأيدي ثم.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/mathurin-napoly-matnapo-ejWJ3a92FEs-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "دار الدنمارك ولكسمبورغ بـ, تم بحث الإنزال الرئيسية. حصدت فرنسية المنتصر بحث ان. ان حالية الأولية دول, إعمار اتّجة جعل بل. لم وحتّى قائمة اعلان على. سليمان، التاريخ، مما مع. بخطوط وأزيز وبعدما أي الى, في خيار فبعد والفرنسي به،.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/national-cancer-institute-KrsoedfRAf4-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "لكل عن ألمانيا والروسية, بـ وكسبت الشرقي الخاطفة تحت. ٣٠ أضف أهّل الوراء الأوروبية, جيوب الوزراء سنغافورة أم انه, ليركز إبّان أم لها. ومن قد الأحمر محاولات وانتهاءً, لها من مشارف وبحلول لإنعدام. تم كان البرية المبرمة التبرعات. بلا بـ الغالي الثقيلة ولكسمبورغ. بمعارضة شموليةً و أضف, كما فكانت يتمكن وتنصيب بـ.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-what-to-eat",
          name: "ما الذي تريد أن تأكله",
          articles: [
            {
              class: "columns-wrap",
              header: "الكربوهيدرات منخفضة",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/kenny-eliason-5ddH9Y2accI-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "حدى عن يعبأ الفترة حاملات, وسوء الأثنان ثم انه, الى ان والقرى حاملات. قام ان تمهيد العناد بالتوقيع, ذلك بهيئة والمعدات بـ. جعل الشتاء الأوربيين هو, قدما تصرّف إبّان يتم ان, بـ كلّ أمدها كانتا المنتصر. ضرب عل سقوط المتساقطة،, لمّ و تصرّف الوراء بمحاولة. تسبب الأثناء، وصل ثم, بـ أعلنت والمانيا به،, حدى بمباركة الأسيوي التنازلي من.",
                },
                {
                  image: {
                    src: "assets/images/brigitte-tohm-iIupxcq-yH4-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "عن مكن لغات الإمداد, ٣٠ تلك مسرح الجنوبي, إذ الإكتفاء الأوربيين تلك. من مكن واتّجه والكوري عشوائية, بلاده بمباركة الرئيسية مع عدم. أراض ألمانيا عل مكن, بـ بداية الخاسر يبق. دون أي الأعمال الدّفاع والفلبين. بل عملية استرجاع تحت, ما مما مايو والتي الإقتصادي. مما من احداث اتفاق.",
                },
                {
                  image: {
                    src: "assets/images/brooke-lark-oaz0raysASk-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "قام لم تغييرات الجنوبي. أم أراضي الثقيل نفس, جمعت السيء مع أخذ. إذ لان ويعزى الوراء ومطالبة, فعل بفرض حالية أطراف ما, تحرير يتعلّق ومطالبة عن تحت. دنو والحزب معاملة العمليات ما, في حتى هنا؟ بتحدّي, وأزيز وصافرات الصفحات على في. من بعد الأجل اليميني البولندي, هذا لعدم والإتحاد بالتوقيع مع. بالعمل الجنود إذ بحق.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "نباتي",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/christina-rumpf-gUU4MF87Ipw-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "في ولم جمعت عشوائية. لان ماذا ٢٠٠٤ وفنلندا ثم. يتم جمعت وقبل اللا تم, كما وصغار السادس ان. فكان واستمر تعد بل. عن جديدة الساحل دنو. مما بل خطّة معزّزة الأوروبي, بداية وأزيز الخارجية وصل مع.",
                },
                {
                  image: {
                    src: "assets/images/nathan-dumlao-bRdRUUtbxO0-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "٣٠ كما أفاق والروسية الإقتصادية, أم مسرح وترك اسبوعين يتم, الا وبحلول يتعلّق ان. تصفح الخطّة الشتاء، في بين, عن الخاطفة الخارجية دار. ان ذلك زهاء مشروط. كلا مع قادة العظمى, كلا مليون وبالرغم بل, حدى و كردة بالعمل الفرنسي. يذكر الأراضي عن دول, ٣٠ لأداء اسبوعين ذلك, بال أثره، السيء إذ.",
                },
                {
                  image: {
                    src: "assets/images/maddi-bazzocco-qKbHvzXb85A-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "مكّن المدن لم جعل. بعد أم أدوات ويكيبيديا وبريطانيا. مع بعض الوزراء تشيكوسلوفاكيا, لم حول تونس الضغوط. بـ الحكم مهمّات بحق, سياسة بالعمل تم تحت. مساعدة وتنصيب ويكيبيديا كما أم, فعل بقصف المزيفة التقليدي من.",
                },
              ],
            },
            {
              class: "columns-wrap",
              header: "إفطار",
              type: "excerpt",
              content: [
                {
                  image: {
                    src: "assets/images/brooke-lark-IDTEXXXfS44-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "ما وقد كانت تسبب الإنذار،, اتّجة الاندونيسية بعض قد. تم الى عليها وسمّيت. عل لكون بالفشل عرض, اسبوعين والنرويج قد حول. بها الأمور رجوعهم الإمتعاض عل.",
                },
                {
                  image: {
                    src: "assets/images/joseph-gonzalez-QaGDmf5tMiE-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "هناك والمانيا الا مع. هو تلك تكبّد نتيجة ويتّفق, كانت الشرق، الرئيسية لكل كل. كل المبرمة الفرنسي ذات, زهاء الثالث وبولندا عرض و. ان تحت ليبين وبغطاء الهجوم. غضون الأرواح قُدُماً ٣٠ عدد, حتى مشارف والحزب أي. قائمة الأوربيين لمّ عل.",
                },
                {
                  image: {
                    src: "assets/images/brooke-lark-GJMlSBS0FhU-unsplash_150.jpg",
                    alt: "عنصر نائب",
                    width: "150",
                    height: "84",
                  },
                  text: "وصغار أطراف أي ذات. هذه عن واحدة أوزار التخطيط, لم وجزر بالحرب مدن. لان ان ثانية والإتحاد. نهاية التقليدي حدى عل, مكن السيطرة الثالث، الأرضية لم.",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-hot-topics",
          name: "مواضيع مثيرة",
          articles: [
            {
              class: "columns-2-balanced",
              header: "هذا أولا",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/national-cancer-institute-cw2Zn2ZQ9YQ-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "ان الجو للسيطرة الأثنان شيء. مما عن أدوات يتعلّق المنتصر, شاسعة الصين سنغافورة ولم ثم. بعض بقعة والحزب التجارية عن. فقد وإيطالي الشتاء، وايرلندا أن, من يطول فقامت الشتوية كلا, إذ كانتا اكتوبر أسر.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/national-cancer-institute-GcrSgHDrniY-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "new", label: "جديد" } },
                  text: "مليارات الأراضي عن بعض. بال سقطت دأبوا من, أن بوابة بالسيطرة باستحداث كان. بعد لعدم بالولايات أم. احداث أعمال القوى أسر و.",
                  url: "#",
                },
              ],
            },
            {
              class: "columns-2-balanced",
              header: "هذا الثاني",
              type: "grid",
              content: [
                {
                  image: {
                    src: "assets/images/national-cancer-institute-SMxzEaidR20-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "حصدت وترك بقيادة ذلك أن, ذلك مايو فرنسية قد. حكومة وحلفاؤها الأوربيين بل مما, وفي عن هُزم بالفشل العالمية. المتحدة والمعدات من بعد, مدن من الصين الإقتصادي. تعد أي غريمه النفط استدعى, بل المارق الشهير العمليات قبل. لكون الشطر عدم أن, أخرى كُلفة إستعمل عن ومن, مع ضرب عقبت الهادي.",
                  url: "#",
                },
                {
                  image: {
                    src: "assets/images/national-cancer-institute-L7en7Lb-Ovc-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  meta: { tag: { type: "breaking", label: "كسر" } },
                  text: "بعد كل انتباه بالرّغم بالولايات, مدن بـ وبعد إستعمل. مع العدّ استدعى الأمريكي وفي, بعض وقامت بتحدّي والعتاد في. وأزيز قائمة أم تحت. أمام وباءت مواقعها كل الى, حقول بخطوط بأضرار أي مكن. سكان استدعى ذات مع, وقد ٣٠ خيار الفترة الرئيسية, و أما اتفاق مكثّفة إستعمل.",
                  url: "#",
                },
              ],
            },
          ],
        },
        {
          id: "content-health-paid-content",
          name: "المحتوى المدفوع",
          articles: [
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/dom-hill-nimElTcTNyY-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "سلطة كرة القدم حياة مجرد عملية الاحماء العظيمة.جعبة كبيرة واستثمر تصنيع تخرج.الراحة الإلزامية لا تتابع الذروة.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/sarah-dorweiler-gUPiTDBdRe4-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "مركبات في زيارة من دورة الحياة المدرسية.بالنسبة إلى أسهم عنصر الحياة في المنطقة والواجب المنزلي الأسد.لا احتياجات مطور التغذية ماليسوادا.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/icons8-team-k5fUTay0ghw-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "إنها أتمنى أن ترغب في إزالة القوس الكراهية.جرة هي الآن الخوف.مطور الراحة الجامعي في في تمويل هو.في كرة السلة ، وقتي تمويل أي مطور التغذية.",
                },
              ],
            },
            {
              class: "columns-4-balanced",
              type: "preview",
              content: [
                {
                  image: {
                    src: "assets/images/jessica-weiller-So4eFi-d1nc-unsplash_336.jpg",
                    alt: "عنصر نائب",
                    width: "336",
                    height: "189",
                  },
                  title:
                    "كان التمويل ولكن الأداء ولكن البوابة.نولام ودرجة حرارة الحلق الفلفل الحار سعر المعرف السريري.الكتلة ليست واقعية الآن لعنصر المنطقة.",
                },
              ],
            },
          ],
        },
      ],
    },
  },
  L_ = {
    header: "Settings",
    items: {
      motion: { label: "Reduced Motion" },
      contrast: { label: "High Contrast Mode" },
    },
  },
  H_ = {
    header: "設定",
    items: {
      motion: { label: "モーションの減少" },
      contrast: { label: "ハイコントラストモード" },
    },
  },
  F_ = {
    header: "إعدادات",
    items: {
      motion: { label: "انخفاض الحركة" },
      contrast: { label: "وضع التباين العاليs" },
    },
  },
  D_ = { copyright: { label: "all rights reserved!" } },
  z_ = { copyright: { label: "全著作権所有！" } },
  B_ = { copyright: { label: "كل الحقوق محفوظة" } },
  Q_ = { label: "Log In", href: "#", target: "internal" },
  V_ = { label: "More", href: "#", target: "internal" },
  W_ = Object.freeze(
    Object.defineProperty(
      { __proto__: null, login: Q_, more: V_ },
      Symbol.toStringTag,
      { value: "Module" }
    )
  ),
  Y_ = { label: "ログイン", href: "#", target: "internal" },
  $_ = { label: "もっと", href: "#", target: "internal" },
  K_ = Object.freeze(
    Object.defineProperty(
      { __proto__: null, login: Y_, more: $_ },
      Symbol.toStringTag,
      { value: "Module" }
    )
  ),
  J_ = { label: "تسجيل الدخول", href: "#", target: "internal" },
  Z_ = { label: "أكثر", href: "#", target: "internal" },
  G_ = Object.freeze(
    Object.defineProperty(
      { __proto__: null, login: J_, more: Z_ },
      Symbol.toStringTag,
      { value: "Module" }
    )
  ),
  X_ = {
    facebook: { label: "Facebook", href: "#", target: "external" },
    instagram: { label: "Instagram", href: "#", target: "external" },
    twitter: { label: "Twitter", href: "#", target: "external" },
  },
  ey = {
    terms: { label: "Terms of Use", href: "#", target: "external" },
    privacy: { label: "Privacy Policy", href: "#", target: "external" },
    sell: {
      label: "Do Not Sell Or Share My Personal Information",
      href: "#",
      target: "external",
    },
    choices: { label: "Ad Choices", href: "#", target: "external" },
  },
  ty = { skip: { label: "Skip to content" } },
  sy = Object.freeze(
    Object.defineProperty(
      { __proto__: null, a11y: ty, legal: ey, social: X_ },
      Symbol.toStringTag,
      { value: "Module" }
    )
  ),
  ay = {
    facebook: { label: "Facebook", href: "#", target: "external" },
    instagram: { label: "Instagram", href: "#", target: "external" },
    twitter: { label: "Twitter", href: "#", target: "external" },
  },
  iy = {
    terms: { label: "利用規約", href: "#", target: "external" },
    privacy: { label: "プライバシーポリシー", href: "#", target: "external" },
    sell: {
      label: "私の個人情報を販売したり共有したりしないでください",
      href: "#",
      target: "external",
    },
    choices: { label: "広告の選択", href: "#", target: "external" },
  },
  ny = { skip: { label: "コンテンツにスキップします" } },
  ly = Object.freeze(
    Object.defineProperty(
      { __proto__: null, a11y: ny, legal: iy, social: ay },
      Symbol.toStringTag,
      { value: "Module" }
    )
  ),
  ry = {
    facebook: { label: "Facebook", href: "#", target: "external" },
    instagram: { label: "Instagram", href: "#", target: "external" },
    twitter: { label: "Twitter", href: "#", target: "external" },
  },
  cy = {
    terms: { label: "شروط الاستخدام", href: "#", target: "external" },
    privacy: { label: "سياسة الخصوصية", href: "#", target: "external" },
    sell: {
      label: "لا تبيع أو تشارك معلوماتي الشخصية",
      href: "#",
      target: "external",
    },
    choices: { label: "اختيارات الإعلان", href: "#", target: "external" },
  },
  oy = { skip: { label: "تخطى الى المحتوى" } },
  uy = Object.freeze(
    Object.defineProperty(
      { __proto__: null, a11y: oy, legal: cy, social: ry },
      Symbol.toStringTag,
      { value: "Module" }
    )
  ),
  Pl = {
    en: { content: N_, settings: L_, footer: D_, buttons: W_, links: sy },
    jp: { content: O_, settings: H_, footer: z_, buttons: K_, links: ly },
    ar: { content: U_, settings: F_, footer: B_, buttons: G_, links: uy },
  },
  my = ["ar", "he", "fa", "ps", "ur"],
  hy = "en",
  dy = "ltr";
function gy() {
  var n;
  const t =
      (n = new URLSearchParams(window.location.search).get("lang")) == null
        ? void 0
        : n.toLowerCase(),
    s = t && t in Pl ? t : hy,
    a = s && my.includes(s) ? "rtl" : dy;
  cd({ htmlAttrs: { dir: a, lang: s } });
  const i = { lang: s, dir: a, ...Pl[s] };
  Nt("data", i);
}
function py() {
  const e = us();
  Rt(
    e,
    t => {
      if (document.getElementById("page"))
        if (!e.hash) document.getElementById("page").scrollTo(0, 0);
        else {
          const s = e.hash.split("#")[1];
          Lt(() => {
            document.getElementById(s).scrollIntoView();
          });
        }
    },
    { deep: !0, immediate: !0 }
  );
}
(history.replaceState = function (e) {
  return null;
}),
  (window.requestAnimationFrame = e => window.setTimeout(e, 0)),
  (window.cancelAnimationFrame = window.clearTimeout),
  (window.requestIdleCallback = void 0),
  (window.cancelIdleCallback = void 0);
const fy = {
    __name: "app",
    setup(e) {
      return (
        gy(),
        py(),
        (t, s) => {
          const a = _f,
            i = M_;
          return T(), _e(i, null, { default: Xe(() => [Q(a)]), _: 1 });
        }
      );
    },
  },
  El = {
    __name: "nuxt-root",
    setup(e) {
      const t = $o(() =>
          ec(
            () => import("./error-component.98713fee.js"),
            [],
            import.meta.url
          ).then(r => r.default || r)
        ),
        s = () => null,
        a = we(),
        i = a.deferHydration(),
        n = !1;
      Nt("_route", us()),
        a.hooks.callHookWith(r => r.map(o => o()), "vue:setup");
      const l = Ta();
      _r((r, o, u) => {
        if (
          (a.hooks
            .callHook("vue:error", r, o, u)
            .catch(m => console.error("[nuxt] Error in `vue:error` hook", m)),
          kd(r) && (r.fatal || r.unhandled))
        )
          return a.runWithContext(() => Qt(r)), !1;
      });
      const { islandContext: c } = !1;
      return (r, o) => (
        T(),
        _e(
          rr,
          { onResolve: pe(i) },
          {
            default: Xe(() => [
              pe(l)
                ? (T(), _e(pe(t), { key: 0, error: pe(l) }, null, 8, ["error"]))
                : pe(c)
                  ? (T(),
                    _e(pe(s), { key: 1, context: pe(c) }, null, 8, ["context"]))
                  : pe(n)
                    ? (T(), _e(vr(pe(n)), { key: 2 }))
                    : (T(), _e(pe(fy), { key: 3 })),
            ]),
            _: 1,
          },
          8,
          ["onResolve"]
        )
      );
    },
  };
globalThis.$fetch || (globalThis.$fetch = Wm.create({ baseURL: $m() }));
let Al;
const by = uh(df);
{
  let e;
  (Al = async function () {
    var n, l;
    if (e) return e;
    const a = !!(
        ((n = window.__NUXT__) != null && n.serverRendered) ||
        ((l = document.getElementById("__NUXT_DATA__")) == null
          ? void 0
          : l.dataset.ssr) === "true"
      )
        ? cm(El)
        : rm(El),
      i = rh({ vueApp: a });
    try {
      await oh(i, by);
    } catch (c) {
      await i.callHook("app:error", c),
        (i.payload.error = i.payload.error || c);
    }
    try {
      await i.hooks.callHook("app:created", a),
        await i.hooks.callHook("app:beforeMount", a),
        a.mount("#" + md),
        await i.hooks.callHook("app:mounted", a),
        await Lt();
    } catch (c) {
      await i.callHook("app:error", c),
        (i.payload.error = i.payload.error || c);
    }
    return a;
  }),
    (e = Al().catch(t => {
      console.error("Error while mounting app:", t);
    }));
}
export {
  ec as _,
  re as a,
  cd as b,
  _e as c,
  $o as d,
  H as e,
  k as f,
  ku as g,
  Q as h,
  cs as i,
  Oa as j,
  vy as k,
  _y as n,
  T as o,
  yy as p,
  ye as t,
  pe as u,
  Xe as w,
};
