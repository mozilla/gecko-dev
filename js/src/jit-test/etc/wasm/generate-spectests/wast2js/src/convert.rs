/* Copyright 2021 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use anyhow::{anyhow, bail, Context as _, Result};
use regex::Regex;
use std::fmt::Write;
use std::path::Path;
use std::str;

use super::out::*;

const HARNESS: &'static str = include_str!("./harness.js");
const LICENSE: &'static str = include_str!("./license.js");

pub fn harness() -> String {
    HARNESS.to_string()
}

pub fn convert<P: AsRef<Path>>(path: P, wast: &str) -> Result<String> {
    let filename = path.as_ref();
    let adjust_wast = |mut err: wast::Error| {
        err.set_path(filename);
        err.set_text(wast);
        err
    };

    let mut lexer = wast::lexer::Lexer::new(wast);
    // The 'names.wast' spec test has confusable unicode -- disable detection.
    lexer.allow_confusing_unicode(filename.ends_with("names.wast"));
    let buf = wast::parser::ParseBuffer::new_with_lexer(lexer).map_err(adjust_wast)?;
    let ast = wast::parser::parse::<wast::Wast>(&buf).map_err(adjust_wast)?;

    let mut out = String::new();

    writeln!(&mut out, "{}", LICENSE)?;
    writeln!(&mut out, "// {}", filename.display())?;

    let mut current_instance: Option<usize> = None;

    for directive in ast.directives {
        let sp = directive.span();
        let (line, col) = sp.linecol_in(wast);
        writeln!(&mut out, "")?;
        convert_directive(
            directive,
            &mut current_instance,
            filename,
            line,
            col,
            wast,
            &mut out,
        )
        .with_context(|| {
            format!(
                "failed to convert directive on {}:{}:{}",
                filename.display(),
                line + 1,
                col
            )
        })?;
    }

    Ok(out)
}

fn convert_directive(
    directive: wast::WastDirective,
    current_instance: &mut Option<usize>,
    filename: &Path,
    line: usize,
    col: usize,
    wast: &str,
    out: &mut String,
) -> Result<()> {
    use wast::WastDirective::*;

    if col == 1 {
        writeln!(out, "// {}:{}", filename.display(), line + 1)?;
    } else {
        writeln!(out, "// {}:{}:{}", filename.display(), line + 1, col)?;
    }

    // For full definitions of these directives, see
    // https://github.com/WebAssembly/spec/blob/main/interpreter/README.md#scripts
    match directive {
        // (module $M? ...): Instantiate a module with an optional identifier.
        Module(wast::QuoteWat::Wat(wast::Wat::Module(module))) => {
            let next_instance = current_instance.map(|x| x + 1).unwrap_or(0);
            let module_text = module_to_js_string(&module, wast)?;

            writeln!(
                out,
                "let ${} = instantiate(`{}`);",
                next_instance, module_text
            )?;
            if let Some(id) = module.id {
                writeln!(out, "let ${} = ${};", id.name(), next_instance)?;
            }

            *current_instance = Some(next_instance);
        }
        // (module quote "..."): Instantiate a module using the given literal text.
        // Identifiers are not supported.
        Module(wast::QuoteWat::QuoteModule(_, source)) => {
            let next_instance = current_instance.map(|x| x + 1).unwrap_or(0);
            let module_text = quote_module_to_js_string(source)?;

            writeln!(
                out,
                "let ${} = instantiate(`{}`);",
                next_instance, module_text
            )?;

            *current_instance = Some(next_instance);
        }
        Module(..) => bail!("unsupported module definition"),

        // (module definition $M ...): Define and validate, but do not instantiate, a module
        ModuleDefinition(wast::QuoteWat::Wat(wast::Wat::Module(module))) => {
            let name = module
                .id
                .ok_or(anyhow!("`module define` directives must have IDs"))?
                .name();
            let module_text = module_definition_to_js_string(&module, wast)?;

            writeln!(out, "let ${} = module(`{}`);", name, module_text)?;
        }
        ModuleDefinition(..) => bail!("unsupported module definition...definition"),

        // (module instance $I $M): Instantiate a specific module
        ModuleInstance {
            span: _,
            instance: Some(instance),
            module: Some(module),
        } => {
            writeln!(
                out,
                "let ${} = instantiateFromModule(${});",
                instance.name(),
                module.name()
            )?;
        }
        ModuleInstance { .. } => bail!("unsupported module instance definition"),

        // (register "a"): Register the current instance
        Register {
            span: _,
            name,
            module: None,
        } => {
            writeln!(
                out,
                "register(${}, `{}`);",
                current_instance.unwrap(),
                escape_template_name_string(name)
            )?;
        }
        // (register "a" $a): Register an item by identifier
        Register {
            span: _,
            name,
            module: Some(thing),
        } => {
            writeln!(
                out,
                "register(${}, `{}`);",
                thing.name(),
                escape_template_name_string(name)
            )?;
        }

        // (invoke $M? "a" ...args): Call an exported function
        Invoke(i) => {
            let invoke_node = invoke_to_js(current_instance, i)?;
            writeln!(out, "{};", invoke_node.output(0))?;
        }

        // (assert_return ...)
        AssertReturn {
            span: _,
            exec,
            results,
        } => {
            let exec_node = execute_to_js(current_instance, exec, wast)?;
            let expected_node = to_js_value_array(&results, assert_expression_to_js_value)?;
            writeln!(
                out,
                "{};",
                JSNode::Assert {
                    name: "assert_return".to_string(),
                    exec: exec_node,
                    expected: expected_node,
                }
                .output(0),
            )?;
        }

        // (assert_trap ...)
        AssertTrap {
            span: _,
            exec,
            message,
        } => {
            let exec_node = execute_to_js(current_instance, exec, wast)?;
            let expected_node = Box::new(JSNode::Raw(format!(
                "`{}`",
                escape_template_name_string(message)
            )));
            writeln!(
                out,
                "{};",
                JSNode::Assert {
                    name: "assert_trap".to_string(),
                    exec: exec_node,
                    expected: expected_node,
                }
                .output(0),
            )?;
        }

        // (assert_exhaustion ...)
        AssertExhaustion {
            span: _,
            call,
            message,
        } => {
            let exec_node = invoke_to_js(current_instance, call)?;
            let expected_node = Box::new(JSNode::Raw(format!(
                "`{}`",
                escape_template_name_string(message)
            )));
            writeln!(
                out,
                "{};",
                JSNode::Assert {
                    name: "assert_exhaustion".to_string(),
                    exec: exec_node,
                    expected: expected_node,
                }
                .output(0),
            )?;
        }

        // (assert_invalid ...)
        AssertInvalid {
            span: _,
            module,
            message,
        } => {
            let text = match module {
                wast::QuoteWat::Wat(wast::Wat::Module(m)) => module_to_js_string(&m, wast)?,
                wast::QuoteWat::QuoteModule(_, source) => quote_module_to_js_string(source)?,
                other => bail!("unsupported {:?} in assert_invalid", other),
            };
            let exec = Box::new(JSNode::Raw(format!("instantiate(`{}`)", text)));
            let expected_node = Box::new(JSNode::Raw(format!(
                "`{}`",
                escape_template_name_string(message)
            )));
            writeln!(
                out,
                "{};",
                JSNode::Assert {
                    name: "assert_invalid".to_string(),
                    exec: exec,
                    expected: expected_node,
                }
                .output(0),
            )?;
        }

        // (assert_malformed ...)
        AssertMalformed {
            module,
            span: _,
            message,
        } => {
            let text = match module {
                wast::QuoteWat::Wat(wast::Wat::Module(m)) => module_to_js_string(&m, wast)?,
                wast::QuoteWat::QuoteModule(_, source) => match quote_module_to_js_string(source) {
                    Ok(t) => t,
                    Err(err) => {
                        writeln!(out, "// ignoring badly-encoded assert_malformed: {:?}", err)?;
                        return Ok(());
                    }
                },
                other => bail!("unsupported {:?} in assert_malformed", other),
            };
            let exec = Box::new(JSNode::Raw(format!("instantiate(`{}`)", text)));
            let expected_node = Box::new(JSNode::Raw(format!(
                "`{}`",
                escape_template_name_string(message)
            )));
            writeln!(
                out,
                "{};",
                JSNode::Assert {
                    name: "assert_malformed".to_string(),
                    exec: exec,
                    expected: expected_node,
                }
                .output(0),
            )?;
        }

        // (assert_unlinkable ...)
        AssertUnlinkable {
            span: _,
            module,
            message,
        } => {
            let text = match module {
                wast::Wat::Module(module) => module_to_js_string(&module, wast)?,
                other => bail!("unsupported {:?} in assert_unlinkable", other),
            };
            let exec = Box::new(JSNode::Raw(format!("instantiate(`{}`)", text)));
            let expected_node = Box::new(JSNode::Raw(format!(
                "`{}`",
                escape_template_name_string(message)
            )));
            writeln!(
                out,
                "{};",
                JSNode::Assert {
                    name: "assert_unlinkable".to_string(),
                    exec: exec,
                    expected: expected_node,
                }
                .output(0),
            )?;
        }

        AssertException { span: _, exec } => {
            // This assert doesn't have a second parameter, so we don't bother
            // formatting it using an Assert node.
            let exec_node = execute_to_js(current_instance, exec, wast)?;
            writeln!(out, "assert_exception(() => {});", exec_node.output(0))?;
        }
        AssertSuspension { .. } => unimplemented!(),
        Thread(..) => unimplemented!(),
        Wait { .. } => unimplemented!(),
    }

    Ok(())
}

fn escape_template_string(text: &str, escape_ascii_lf_tab: bool) -> String {
    let mut escaped = String::new();
    for c in text.chars() {
        match c {
            '$' => escaped.push_str("$$"),
            '\\' => escaped.push_str("\\\\"),
            '`' => escaped.push_str("\\`"),
            c if c.is_ascii_control() && escape_ascii_lf_tab && c != '\n' && c != '\t' => {
                escaped.push_str(&format!("\\x{:02x}", c as u32))
            }
            c if !c.is_ascii() => escaped.push_str(&c.escape_unicode().to_string()),
            c => escaped.push(c),
        }
    }
    escaped
}

// Escape a module for use in a JS template string.
fn escape_template_module_string(text: &str) -> String {
    // Modules may have comments, where line-feeds must be passed through as-is
    // to preserve their meaning
    escape_template_string(text, false)
}

// Escape a name for use in a JS template string.
fn escape_template_name_string(text: &str) -> String {
    // Names don't care about line-feeds or tabs, just need equality
    escape_template_string(text, true)
}

fn span_to_offset(span: wast::token::Span, text: &str) -> Result<usize> {
    let (span_line, span_col) = span.linecol_in(text);
    let mut cur = 0;
    // Use split_terminator instead of lines so that if there is a `\r`,
    // it is included in the offset calculation. The `+1` values below
    // account for the `\n`.
    for (i, line) in text.split_terminator('\n').enumerate() {
        if span_line == i {
            assert!(span_col < line.len());
            return Ok(cur + span_col);
        }
        cur += line.len() + 1;
    }
    bail!("invalid line/col");
}

fn closed_module(module: &str) -> Result<&str> {
    if !module.starts_with("module") {
        bail!("expected module source");
    }

    enum State {
        Module,
        QStr,
        EscapeQStr,
    }

    let mut i = 0;
    let mut level = 1;
    let mut state = State::Module;

    let mut chars = module.chars();
    while level != 0 {
        let next = chars.next().ok_or(anyhow!("unable to close module"))?;
        match state {
            State::Module => match next {
                '(' => level += 1,
                ')' => level -= 1,
                '"' => state = State::QStr,
                _ => {}
            },
            State::QStr => match next {
                '"' => state = State::Module,
                '\\' => state = State::EscapeQStr,
                _ => {}
            },
            State::EscapeQStr => match next {
                _ => state = State::QStr,
            },
        }
        i += next.len_utf8();
    }
    return Ok(&module[0..i]);
}

fn module_to_js_string(module: &wast::core::Module, wast: &str) -> Result<String> {
    let offset = span_to_offset(module.span, wast)?;
    let opened_module = &wast[offset..];
    if !opened_module.starts_with("module") {
        return Ok(escape_template_module_string(opened_module));
    }
    Ok(escape_template_module_string(&format!(
        "({}",
        closed_module(opened_module)?
    )))
}

fn quote_module_to_js_string(quotes: Vec<(wast::token::Span, &[u8])>) -> Result<String> {
    let mut text = String::new();
    for (_, src) in quotes {
        text.push_str(str::from_utf8(src)?);
        text.push_str(" ");
    }
    let escaped = escape_template_module_string(&text);
    Ok(escaped)
}

fn module_definition_to_js_string(module: &wast::core::Module, wast: &str) -> Result<String> {
    let offset = span_to_offset(module.span, wast)?;
    let opened_module = &wast[offset..];

    // strip "module definition $M" down to just "module"
    let pattern = r"^module definition \$[a-zA-Z_$][a-zA-Z0-9_$]*(.*)";
    let re = Regex::new(pattern).expect("Invalid regex pattern");
    let without_definition = re.replace(opened_module, "module $1");

    Ok(escape_template_module_string(&format!(
        "({}",
        closed_module(&without_definition)?
    )))
}

fn invoke_to_js(current_instance: &Option<usize>, i: wast::WastInvoke) -> Result<Box<JSNode>> {
    let instance = i
        .module
        .map(|x| format!("${}", x.name()))
        .unwrap_or_else(|| format!("${}", current_instance.unwrap()));
    let body = to_js_value_array(&i.args, arg_to_js_value)?;

    Ok(Box::new(JSNode::Invoke {
        instance: instance,
        name: escape_template_name_string(i.name),
        body: body,
    }))
}

fn execute_to_js(
    current_instance: &Option<usize>,
    exec: wast::WastExecute,
    wast: &str,
) -> Result<Box<JSNode>> {
    match exec {
        wast::WastExecute::Invoke(invoke) => invoke_to_js(current_instance, invoke),
        wast::WastExecute::Wat(module) => {
            let text = match module {
                wast::Wat::Module(module) => module_to_js_string(&module, wast)?,
                other => bail!("unsupported {:?} at execute_to_js", other),
            };
            Ok(Box::new(JSNode::Raw(format!("instantiate(`{}`)", text))))
        }
        wast::WastExecute::Get { module, global, .. } => {
            let instanceish = module
                .map(|x| format!("{}", x.name()))
                .unwrap_or_else(|| format!("{}", current_instance.unwrap()));
            Ok(Box::new(JSNode::Raw(format!(
                "get(${}, `{}`)",
                instanceish, global
            ))))
        }
    }
}

fn to_js_value_array<T, F: Fn(&T) -> Result<String>>(vals: &[T], func: F) -> Result<Box<JSNode>> {
    let mut value_nodes: Vec<Box<JSNode>> = vec![];
    for val in vals {
        let converted_value = (func)(val)?;
        value_nodes.push(Box::new(JSNode::Raw(converted_value)));
    }

    Ok(Box::new(JSNode::Array(value_nodes)))
}

fn to_js_value_array_string<T, F: Fn(&T) -> Result<String>>(vals: &[T], func: F) -> Result<String> {
    let array = to_js_value_array(vals, func)?;
    Ok(array.output(NOWRAP))
}

fn f32_needs_bits(a: f32) -> bool {
    if a.is_infinite() {
        return false;
    }
    return a.is_nan()
        || ((a as f64) as f32).to_bits() != a.to_bits()
        || (format!("{:.}", a).parse::<f64>().unwrap() as f32).to_bits() != a.to_bits();
}

fn f64_needs_bits(a: f64) -> bool {
    return a.is_nan();
}

fn f32x4_needs_bits(a: &[wast::token::F32; 4]) -> bool {
    a.iter().any(|x| {
        let as_f32 = f32::from_bits(x.bits);
        f32_needs_bits(as_f32)
    })
}

fn f64x2_needs_bits(a: &[wast::token::F64; 2]) -> bool {
    a.iter().any(|x| {
        let as_f64 = f64::from_bits(x.bits);
        f64_needs_bits(as_f64)
    })
}

fn f32_to_js_value(val: f32) -> String {
    if val == f32::INFINITY {
        format!("Infinity")
    } else if val == f32::NEG_INFINITY {
        format!("-Infinity")
    } else if val.is_sign_negative() && val == 0f32 {
        format!("-0")
    } else {
        format!("{:.}", val)
    }
}

fn f64_to_js_value(val: f64) -> String {
    if val == f64::INFINITY {
        format!("Infinity")
    } else if val == f64::NEG_INFINITY {
        format!("-Infinity")
    } else if val.is_sign_negative() && val == 0f64 {
        format!("-0")
    } else {
        format!("{:.}", val)
    }
}

fn float32_to_js_value(val: &wast::token::F32) -> String {
    let as_f32 = f32::from_bits(val.bits);
    if f32_needs_bits(as_f32) {
        format!(
            "bytes(\"f32\", {})",
            to_js_value_array_string(&val.bits.to_le_bytes(), |x| Ok(format!("0x{:x}", x)))
                .unwrap(),
        )
    } else {
        format!("value(\"f32\", {})", f32_to_js_value(as_f32))
    }
}

fn float64_to_js_value(val: &wast::token::F64) -> String {
    let as_f64 = f64::from_bits(val.bits);
    if f64_needs_bits(as_f64) {
        format!(
            "bytes(\"f64\", {})",
            to_js_value_array_string(&val.bits.to_le_bytes(), |x| Ok(format!("0x{:x}", x)))
                .unwrap(),
        )
    } else {
        format!("value(\"f64\", {})", f64_to_js_value(as_f64))
    }
}

fn f32_pattern_to_js_value(pattern: &wast::core::NanPattern<wast::token::F32>) -> String {
    use wast::core::NanPattern::*;
    match pattern {
        CanonicalNan => format!("`canonical_nan`"),
        ArithmeticNan => format!("`arithmetic_nan`"),
        Value(x) => float32_to_js_value(x),
    }
}

fn f64_pattern_to_js_value(pattern: &wast::core::NanPattern<wast::token::F64>) -> String {
    use wast::core::NanPattern::*;
    match pattern {
        CanonicalNan => format!("`canonical_nan`"),
        ArithmeticNan => format!("`arithmetic_nan`"),
        Value(x) => float64_to_js_value(x),
    }
}

fn return_value_to_js_value(v: &wast::core::WastRetCore<'_>) -> Result<String> {
    use wast::core::WastRetCore::*;
    Ok(match v {
        I32(x) => format!("value(\"i32\", {})", x),
        I64(x) => format!("value(\"i64\", {}n)", x),
        F32(x) => f32_pattern_to_js_value(x),
        F64(x) => f64_pattern_to_js_value(x),
        RefNull(x) => match x {
            Some(wast::core::HeapType::Abstract { shared: false, ty }) => match ty {
                wast::core::AbstractHeapType::Any => format!("value('anyref', null)"),
                wast::core::AbstractHeapType::Exn => format!("value('exnref', null)"),
                wast::core::AbstractHeapType::Eq => format!("value('eqref', null)"),
                wast::core::AbstractHeapType::Array => format!("value('arrayref', null)"),
                wast::core::AbstractHeapType::Struct => format!("value('structref', null)"),
                wast::core::AbstractHeapType::I31 => format!("value('i31ref', null)"),
                wast::core::AbstractHeapType::None => format!("value('nullref', null)"),
                wast::core::AbstractHeapType::Func => format!("value('anyfunc', null)"),
                wast::core::AbstractHeapType::NoFunc => format!("value('nullfuncref', null)"),
                wast::core::AbstractHeapType::Extern => format!("value('externref', null)"),
                wast::core::AbstractHeapType::NoExtern => format!("value('nullexternref', null)"),
                wast::core::AbstractHeapType::Exn => format!("value('exnref', null)"),
                wast::core::AbstractHeapType::NoExn => format!("value('nullexnref', null)"),
                other => bail!(
                    "couldn't convert ref.null {:?} to a js assertion value",
                    other
                ),
            },
            None => "null".to_string(),
            other => bail!(
                "couldn't convert ref.null {:?} to a js assertion value",
                other
            ),
        },
        RefFunc(_) => format!("new RefWithType('funcref')"),
        RefExtern(x) => match x {
            Some(v) => format!("new ExternRefResult({})", v),
            None => format!("new RefWithType('externref')"),
        },
        RefHost(x) => format!("new HostRefResult({})", x),
        RefAny => format!("new RefWithType('anyref')"),
        RefEq => format!("new RefWithType('eqref')"),
        RefArray => format!("new RefWithType('arrayref')"),
        RefStruct => format!("new RefWithType('structref')"),
        RefI31 => format!("new RefWithType('i31ref')"),
        V128(x) => {
            use wast::core::V128Pattern::*;
            match x {
                I8x16(elements) => format!(
                    "i8x16({})",
                    to_js_value_array_string(elements, |x| Ok(format!("0x{:x}", x)))?,
                ),
                I16x8(elements) => format!(
                    "i16x8({})",
                    to_js_value_array_string(elements, |x| Ok(format!("0x{:x}", x)))?,
                ),
                I32x4(elements) => format!(
                    "i32x4({})",
                    to_js_value_array_string(elements, |x| Ok(format!("0x{:x}", x)))?,
                ),
                I64x2(elements) => format!(
                    "i64x2({})",
                    to_js_value_array_string(elements, |x| Ok(format!("0x{:x}n", x)))?,
                ),
                F32x4(elements) => {
                    let elements: Vec<String> = elements
                        .iter()
                        .map(|x| f32_pattern_to_js_value(x))
                        .collect();
                    let elements = strings_to_raws(elements);
                    JSNode::Call {
                        name: "new F32x4Pattern".to_string(),
                        args: elements,
                    }
                    .output(0)
                }
                F64x2(elements) => {
                    let elements: Vec<String> = elements
                        .iter()
                        .map(|x| f64_pattern_to_js_value(x))
                        .collect();
                    let elements = strings_to_raws(elements);
                    JSNode::Call {
                        name: "new F64x2Pattern".to_string(),
                        args: elements,
                    }
                    .output(0)
                }
            }
        }
        Either(v) => {
            let args = v
                .iter()
                .map(|v| return_value_to_js_value(v).unwrap())
                .collect();
            let args = strings_to_raws(args);
            JSNode::Call {
                name: "either".to_string(),
                args: args,
            }
            .output(0)
        }
        other => bail!("couldn't convert Core({:?}) to a js assertion value", other),
    })
}

fn assert_expression_to_js_value(v: &wast::WastRet<'_>) -> Result<String> {
    use wast::WastRet::*;

    Ok(match &v {
        Core(x) => return_value_to_js_value(x)?,
        other => bail!("couldn't convert {:?} to a js assertion value", other),
    })
}

fn arg_to_js_value(v: &wast::WastArg<'_>) -> Result<String> {
    use wast::core::WastArgCore::*;
    use wast::WastArg::Core;

    Ok(match &v {
        Core(I32(x)) => format!("{}", *x),
        Core(I64(x)) => format!("{}n", *x),
        Core(F32(x)) => float32_to_js_value(x),
        Core(F64(x)) => float64_to_js_value(x),
        Core(V128(x)) => {
            use wast::core::V128Const::*;
            match x {
                I8x16(elements) => format!(
                    "i8x16({})",
                    to_js_value_array_string(elements, |x| Ok(format!("0x{:x}", x)))?,
                ),
                I16x8(elements) => format!(
                    "i16x8({})",
                    to_js_value_array_string(elements, |x| Ok(format!("0x{:x}", x)))?,
                ),
                I32x4(elements) => format!(
                    "i32x4({})",
                    to_js_value_array_string(elements, |x| Ok(format!("0x{:x}", x)))?,
                ),
                I64x2(elements) => format!(
                    "i64x2({})",
                    to_js_value_array_string(elements, |x| Ok(format!("0x{:x}n", x)))?,
                ),
                F32x4(elements) => {
                    if f32x4_needs_bits(elements) {
                        let bytes =
                            to_js_value_array(&x.to_le_bytes(), |x| Ok(format!("0x{:x}", x)))?;
                        format!("bytes('v128', {})", bytes.output(0))
                    } else {
                        let vals = to_js_value_array(elements, |x| {
                            Ok(f32_to_js_value(f32::from_bits(x.bits)))
                        })?;
                        format!("f32x4({})", vals.output(0))
                    }
                }
                F64x2(elements) => {
                    if f64x2_needs_bits(elements) {
                        let bytes =
                            to_js_value_array(&x.to_le_bytes(), |x| Ok(format!("0x{:x}", x)))?;
                        format!("bytes('v128', {})", bytes.output(0))
                    } else {
                        let vals = to_js_value_array(elements, |x| {
                            Ok(f64_to_js_value(f64::from_bits(x.bits)))
                        })?;
                        format!("f64x2({})", vals.output(0))
                    }
                }
            }
        }
        Core(RefNull(_)) => format!("null"),
        Core(RefExtern(x)) => format!("externref({})", x),
        Core(RefHost(x)) => format!("hostref({})", x),
        other => bail!("couldn't convert {:?} to a js value", other),
    })
}
