use libtest_mimic::{run, Arguments, Failed, Trial};
use std::fs::{self, DirEntry};
use std::process::ExitCode;

use yaml_rust2::{
    parser::{Event, EventReceiver, Parser, Tag},
    scanner::TScalarStyle,
    yaml, ScanError, Yaml, YamlLoader,
};

type Result<T, E = Box<dyn std::error::Error>> = std::result::Result<T, E>;

struct YamlTest {
    yaml_visual: String,
    yaml: String,
    expected_events: String,
    expected_error: bool,
}

fn main() -> Result<ExitCode> {
    if !std::path::Path::new("tests/yaml-test-suite").is_dir() {
        eprintln!("===================================================================");
        eprintln!("/!\\ yaml-test-suite directory not found, Skipping tests /!\\");
        eprintln!("If you intend to contribute to the library, restore the test suite.");
        eprintln!("===================================================================");
        return Ok(ExitCode::SUCCESS);
    }

    let mut arguments = Arguments::from_args();
    if arguments.test_threads.is_none() {
        arguments.test_threads = Some(1);
    }
    let tests: Vec<Vec<_>> = std::fs::read_dir("tests/yaml-test-suite/src")?
        .map(|entry| -> Result<_> {
            let entry = entry?;
            let tests = load_tests_from_file(&entry)?;
            Ok(tests)
        })
        .collect::<Result<_>>()?;
    let tests: Vec<_> = tests.into_iter().flatten().collect();

    Ok(run(&arguments, tests).exit_code())
}

fn run_yaml_test(data: YamlTest) -> Result<(), Failed> {
    let actual_events = parse_to_events(&data.yaml);
    let events_diff = actual_events.map(|events| events_differ(&events, &data.expected_events));
    let mut error_text = match (&events_diff, data.expected_error) {
        (Ok(x), true) => Some(format!("no error when expected: {x:#?}")),
        (Err(_), true) | (Ok(None), false) => None,
        (Err(e), false) => Some(format!("unexpected error {e:?}")),
        (Ok(Some(diff)), false) => Some(format!("events differ: {diff}")),
    };

    // Show a caret on error.
    if let Some(text) = &mut error_text {
        use std::fmt::Write;
        let _ = writeln!(text, "\n### Input:\n{}\n### End", data.yaml_visual);
        if let Err(err) = &events_diff {
            writeln!(text, "### Error position").unwrap();
            let mut lines = data.yaml.lines();
            for _ in 0..(err.marker().line() - 1) {
                let l = lines.next().unwrap();
                writeln!(text, "{l}").unwrap();
            }
            writeln!(text, "\x1B[91;1m{}", lines.next().unwrap()).unwrap();
            for _ in 0..err.marker().col() {
                write!(text, " ").unwrap();
            }
            writeln!(text, "^\x1b[m").unwrap();
            for l in lines {
                writeln!(text, "{l}").unwrap();
            }
            writeln!(text, "### End error position").unwrap();
        }
    }

    match error_text {
        None => Ok(()),
        Some(txt) => Err(txt.into()),
    }
}

fn load_tests_from_file(entry: &DirEntry) -> Result<Vec<Trial>> {
    let file_name = entry.file_name().to_string_lossy().to_string();
    let test_name = file_name
        .strip_suffix(".yaml")
        .ok_or("unexpected filename")?;
    let tests = YamlLoader::load_from_str(&fs::read_to_string(entry.path())?)?;
    let tests = tests[0].as_vec().ok_or("no test list found in file")?;

    let mut result = vec![];
    let mut current_test = yaml::Hash::new();
    for (idx, test_data) in tests.iter().enumerate() {
        let name = if tests.len() > 1 {
            format!("{test_name}-{idx:02}")
        } else {
            test_name.to_string()
        };

        // Test fields except `fail` are "inherited"
        let test_data = test_data.as_hash().unwrap();
        current_test.remove(&Yaml::String("fail".into()));
        for (key, value) in test_data.clone() {
            current_test.insert(key, value);
        }

        let current_test = Yaml::Hash(current_test.clone()); // Much better indexing

        if current_test["skip"] != Yaml::BadValue {
            continue;
        }

        result.push(Trial::test(name, move || {
            run_yaml_test(YamlTest {
                yaml_visual: current_test["yaml"].as_str().unwrap().to_string(),
                yaml: visual_to_raw(current_test["yaml"].as_str().unwrap()),
                expected_events: visual_to_raw(current_test["tree"].as_str().unwrap()),
                expected_error: current_test["fail"].as_bool() == Some(true),
            })
        }));
    }
    Ok(result)
}

fn parse_to_events(source: &str) -> Result<Vec<String>, ScanError> {
    let mut reporter = EventReporter::new();
    Parser::new_from_str(source).load(&mut reporter, true)?;
    Ok(reporter.events)
}

struct EventReporter {
    events: Vec<String>,
}

impl EventReporter {
    fn new() -> Self {
        Self { events: vec![] }
    }
}

impl EventReceiver for EventReporter {
    fn on_event(&mut self, ev: Event) {
        let line: String = match ev {
            Event::StreamStart => "+STR".into(),
            Event::StreamEnd => "-STR".into(),

            Event::DocumentStart => "+DOC".into(),
            Event::DocumentEnd => "-DOC".into(),

            Event::SequenceStart(idx, tag) => {
                format!("+SEQ{}{}", format_index(idx), format_tag(tag.as_ref()))
            }
            Event::SequenceEnd => "-SEQ".into(),

            Event::MappingStart(idx, tag) => {
                format!("+MAP{}{}", format_index(idx), format_tag(tag.as_ref()))
            }
            Event::MappingEnd => "-MAP".into(),

            Event::Scalar(ref text, style, idx, ref tag) => {
                let kind = match style {
                    TScalarStyle::Plain => ":",
                    TScalarStyle::SingleQuoted => "'",
                    TScalarStyle::DoubleQuoted => r#"""#,
                    TScalarStyle::Literal => "|",
                    TScalarStyle::Folded => ">",
                };
                format!(
                    "=VAL{}{} {}{}",
                    format_index(idx),
                    format_tag(tag.as_ref()),
                    kind,
                    escape_text(text)
                )
            }
            Event::Alias(idx) => format!("=ALI *{idx}"),
            Event::Nothing => return,
        };
        self.events.push(line);
    }
}

fn format_index(idx: usize) -> String {
    if idx > 0 {
        format!(" &{idx}")
    } else {
        String::new()
    }
}

fn escape_text(text: &str) -> String {
    let mut text = text.to_owned();
    for (ch, replacement) in [
        ('\\', r"\\"),
        ('\n', "\\n"),
        ('\r', "\\r"),
        ('\x08', "\\b"),
        ('\t', "\\t"),
    ] {
        text = text.replace(ch, replacement);
    }
    text
}

fn format_tag(tag: Option<&Tag>) -> String {
    if let Some(tag) = tag {
        format!(" <{}{}>", tag.handle, tag.suffix)
    } else {
        String::new()
    }
}

fn events_differ(actual: &[String], expected: &str) -> Option<String> {
    let actual = actual.iter().map(Some).chain(std::iter::repeat(None));
    let expected = expected_events(expected);
    let expected = expected.iter().map(Some).chain(std::iter::repeat(None));
    for (idx, (act, exp)) in actual.zip(expected).enumerate() {
        return match (act, exp) {
            (Some(act), Some(exp)) => {
                if act == exp {
                    continue;
                } else {
                    Some(format!(
                        "line {idx} differs: \n=> expected `{exp}`\n=>    found `{act}`",
                    ))
                }
            }
            (Some(a), None) => Some(format!("extra actual line: {a:?}")),
            (None, Some(e)) => Some(format!("extra expected line: {e:?}")),
            (None, None) => None,
        };
    }
    unreachable!()
}

/// Convert the snippets from "visual" to "actual" representation
fn visual_to_raw(yaml: &str) -> String {
    let mut yaml = yaml.to_owned();
    for (pat, replacement) in [
        ("␣", " "),
        ("»", "\t"),
        ("—", ""), // Tab line continuation ——»
        ("←", "\r"),
        ("⇔", "\u{FEFF}"),
        ("↵", ""), // Trailing newline marker
        ("∎\n", ""),
    ] {
        yaml = yaml.replace(pat, replacement);
    }
    yaml
}

/// Adapt the expectations to the yaml-rust reasonable limitations
///
/// Drop information on node styles (flow/block) and anchor names.
/// Both are things that can be omitted according to spec.
fn expected_events(expected_tree: &str) -> Vec<String> {
    let mut anchors = vec![];
    expected_tree
        .split('\n')
        .map(|s| s.trim_start().to_owned())
        .filter(|s| !s.is_empty())
        .map(|mut s| {
            // Anchor name-to-number conversion
            if let Some(start) = s.find('&') {
                if s[..start].find(':').is_none() {
                    let len = s[start..].find(' ').unwrap_or(s[start..].len());
                    anchors.push(s[start + 1..start + len].to_owned());
                    s = s.replace(&s[start..start + len], &format!("&{}", anchors.len()));
                }
            }
            // Alias nodes name-to-number
            if s.starts_with("=ALI") {
                let start = s.find('*').unwrap();
                let name = &s[start + 1..];
                let idx = anchors
                    .iter()
                    .enumerate()
                    .filter(|(_, v)| v == &name)
                    .next_back()
                    .unwrap()
                    .0;
                s = s.replace(&s[start..], &format!("*{}", idx + 1));
            }
            // Dropping style information
            match &*s {
                "+DOC ---" => "+DOC".into(),
                "-DOC ..." => "-DOC".into(),
                s if s.starts_with("+SEQ []") => s.replacen("+SEQ []", "+SEQ", 1),
                s if s.starts_with("+MAP {}") => s.replacen("+MAP {}", "+MAP", 1),
                s => s.into(),
            }
        })
        .collect()
}
