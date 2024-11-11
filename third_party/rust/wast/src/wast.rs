#[cfg(feature = "component-model")]
use crate::component::WastVal;
use crate::core::{WastArgCore, WastRetCore};
use crate::kw;
use crate::parser::{self, Cursor, Parse, ParseBuffer, Parser, Peek, Result};
use crate::token::{Id, Span};
use crate::{Error, Wat};

/// A parsed representation of a `*.wast` file.
///
/// WAST files are not officially specified but are used in the official test
/// suite to write official spec tests for wasm. This type represents a parsed
/// `*.wast` file which parses a list of directives in a file.
#[derive(Debug)]
pub struct Wast<'a> {
    #[allow(missing_docs)]
    pub directives: Vec<WastDirective<'a>>,
}

impl<'a> Parse<'a> for Wast<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut directives = Vec::new();

        parser.with_standard_annotations_registered(|parser| {
            // If it looks like a directive token is in the stream then we parse a
            // bunch of directives, otherwise assume this is an inline module.
            if parser.peek2::<WastDirectiveToken>()? {
                while !parser.is_empty() {
                    directives.push(parser.parens(|p| p.parse())?);
                }
            } else {
                let module = parser.parse::<Wat>()?;
                directives.push(WastDirective::Module(QuoteWat::Wat(module)));
            }
            Ok(Wast { directives })
        })
    }
}

struct WastDirectiveToken;

impl Peek for WastDirectiveToken {
    fn peek(cursor: Cursor<'_>) -> Result<bool> {
        let kw = match cursor.keyword()? {
            Some((kw, _)) => kw,
            None => return Ok(false),
        };
        Ok(kw.starts_with("assert_")
            || kw == "module"
            || kw == "component"
            || kw == "register"
            || kw == "invoke")
    }

    fn display() -> &'static str {
        unimplemented!()
    }
}

/// The different kinds of directives found in a `*.wast` file.
///
///
/// Some more information about these various branches can be found at
/// <https://github.com/WebAssembly/spec/blob/main/interpreter/README.md#scripts>.
#[allow(missing_docs)]
#[derive(Debug)]
pub enum WastDirective<'a> {
    /// The provided module is defined, validated, and then instantiated.
    Module(QuoteWat<'a>),

    /// The provided module is defined and validated.
    ///
    /// This module is not instantiated automatically.
    ModuleDefinition(QuoteWat<'a>),

    /// The named module is instantiated under the instance name provided.
    ModuleInstance {
        span: Span,
        instance: Option<Id<'a>>,
        module: Option<Id<'a>>,
    },

    /// Asserts the module cannot be decoded with the given error.
    AssertMalformed {
        span: Span,
        module: QuoteWat<'a>,
        message: &'a str,
    },

    /// Asserts the module cannot be validated with the given error.
    AssertInvalid {
        span: Span,
        module: QuoteWat<'a>,
        message: &'a str,
    },

    /// Registers the `module` instance with the given `name` to be available
    /// for importing in future module instances.
    Register {
        span: Span,
        name: &'a str,
        module: Option<Id<'a>>,
    },

    /// Invokes the specified export.
    Invoke(WastInvoke<'a>),

    /// The invocation provided should trap with the specified error.
    AssertTrap {
        span: Span,
        exec: WastExecute<'a>,
        message: &'a str,
    },

    /// The invocation provided should succeed with the specified results.
    AssertReturn {
        span: Span,
        exec: WastExecute<'a>,
        results: Vec<WastRet<'a>>,
    },

    /// The invocation provided should exhaust system resources (e.g. stack
    /// overflow).
    AssertExhaustion {
        span: Span,
        call: WastInvoke<'a>,
        message: &'a str,
    },

    /// The provided module should fail to link when instantiation is attempted.
    AssertUnlinkable {
        span: Span,
        module: Wat<'a>,
        message: &'a str,
    },

    /// The invocation provided should throw an exception.
    AssertException { span: Span, exec: WastExecute<'a> },

    /// The invocation should fail to handle a suspension.
    AssertSuspension {
        span: Span,
        exec: WastExecute<'a>,
        message: &'a str,
    },

    /// Creates a new system thread which executes the given commands.
    Thread(WastThread<'a>),

    /// Waits for the specified thread to exit.
    Wait { span: Span, thread: Id<'a> },
}

impl WastDirective<'_> {
    /// Returns the location in the source that this directive was defined at
    pub fn span(&self) -> Span {
        match self {
            WastDirective::Module(QuoteWat::Wat(w))
            | WastDirective::ModuleDefinition(QuoteWat::Wat(w)) => w.span(),
            WastDirective::Module(QuoteWat::QuoteModule(span, _))
            | WastDirective::ModuleDefinition(QuoteWat::QuoteModule(span, _)) => *span,
            WastDirective::Module(QuoteWat::QuoteComponent(span, _))
            | WastDirective::ModuleDefinition(QuoteWat::QuoteComponent(span, _)) => *span,
            WastDirective::ModuleInstance { span, .. }
            | WastDirective::AssertMalformed { span, .. }
            | WastDirective::Register { span, .. }
            | WastDirective::AssertTrap { span, .. }
            | WastDirective::AssertReturn { span, .. }
            | WastDirective::AssertExhaustion { span, .. }
            | WastDirective::AssertUnlinkable { span, .. }
            | WastDirective::AssertInvalid { span, .. }
            | WastDirective::AssertException { span, .. }
            | WastDirective::AssertSuspension { span, .. }
            | WastDirective::Wait { span, .. } => *span,
            WastDirective::Invoke(i) => i.span,
            WastDirective::Thread(t) => t.span,
        }
    }
}

impl<'a> Parse<'a> for WastDirective<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::module>()? || l.peek::<kw::component>()? {
            parse_wast_module(parser)
        } else if l.peek::<kw::assert_malformed>()? {
            let span = parser.parse::<kw::assert_malformed>()?.0;
            Ok(WastDirective::AssertMalformed {
                span,
                module: parser.parens(|p| p.parse())?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::assert_invalid>()? {
            let span = parser.parse::<kw::assert_invalid>()?.0;
            Ok(WastDirective::AssertInvalid {
                span,
                module: parser.parens(|p| p.parse())?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::register>()? {
            let span = parser.parse::<kw::register>()?.0;
            Ok(WastDirective::Register {
                span,
                name: parser.parse()?,
                module: parser.parse()?,
            })
        } else if l.peek::<kw::invoke>()? {
            Ok(WastDirective::Invoke(parser.parse()?))
        } else if l.peek::<kw::assert_trap>()? {
            let span = parser.parse::<kw::assert_trap>()?.0;
            Ok(WastDirective::AssertTrap {
                span,
                exec: parser.parens(|p| p.parse())?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::assert_return>()? {
            let span = parser.parse::<kw::assert_return>()?.0;
            let exec = parser.parens(|p| p.parse())?;
            let mut results = Vec::new();
            while !parser.is_empty() {
                results.push(parser.parens(|p| p.parse())?);
            }
            Ok(WastDirective::AssertReturn {
                span,
                exec,
                results,
            })
        } else if l.peek::<kw::assert_exhaustion>()? {
            let span = parser.parse::<kw::assert_exhaustion>()?.0;
            Ok(WastDirective::AssertExhaustion {
                span,
                call: parser.parens(|p| p.parse())?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::assert_unlinkable>()? {
            let span = parser.parse::<kw::assert_unlinkable>()?.0;
            Ok(WastDirective::AssertUnlinkable {
                span,
                module: parser.parens(parse_wat)?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::assert_exception>()? {
            let span = parser.parse::<kw::assert_exception>()?.0;
            Ok(WastDirective::AssertException {
                span,
                exec: parser.parens(|p| p.parse())?,
            })
        } else if l.peek::<kw::assert_suspension>()? {
            let span = parser.parse::<kw::assert_suspension>()?.0;
            Ok(WastDirective::AssertSuspension {
                span,
                exec: parser.parens(|p| p.parse())?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::thread>()? {
            Ok(WastDirective::Thread(parser.parse()?))
        } else if l.peek::<kw::wait>()? {
            let span = parser.parse::<kw::wait>()?.0;
            Ok(WastDirective::Wait {
                span,
                thread: parser.parse()?,
            })
        } else {
            Err(l.error())
        }
    }
}

#[allow(missing_docs)]
#[derive(Debug)]
pub enum WastExecute<'a> {
    Invoke(WastInvoke<'a>),
    Wat(Wat<'a>),
    Get {
        span: Span,
        module: Option<Id<'a>>,
        global: &'a str,
    },
}

impl<'a> WastExecute<'a> {
    /// Returns the first span for this execute statement.
    pub fn span(&self) -> Span {
        match self {
            WastExecute::Invoke(i) => i.span,
            WastExecute::Wat(i) => i.span(),
            WastExecute::Get { span, .. } => *span,
        }
    }
}

impl<'a> Parse<'a> for WastExecute<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::invoke>()? {
            Ok(WastExecute::Invoke(parser.parse()?))
        } else if l.peek::<kw::module>()? || l.peek::<kw::component>()? {
            Ok(WastExecute::Wat(parse_wat(parser)?))
        } else if l.peek::<kw::get>()? {
            let span = parser.parse::<kw::get>()?.0;
            Ok(WastExecute::Get {
                span,
                module: parser.parse()?,
                global: parser.parse()?,
            })
        } else {
            Err(l.error())
        }
    }
}

fn parse_wat(parser: Parser) -> Result<Wat> {
    // Note that this doesn't use `Parse for Wat` since the `parser` provided
    // has already peeled back the first layer of parentheses while `Parse for
    // Wat` expects to be the top layer which means it also tries to peel off
    // the parens. Instead we can skip the sugar that `Wat` has for simply a
    // list of fields (no `(module ...)` container) and just parse the `Module`
    // itself.
    if parser.peek::<kw::component>()? {
        Ok(Wat::Component(parser.parse()?))
    } else {
        Ok(Wat::Module(parser.parse()?))
    }
}

#[allow(missing_docs)]
#[derive(Debug)]
pub struct WastInvoke<'a> {
    pub span: Span,
    pub module: Option<Id<'a>>,
    pub name: &'a str,
    pub args: Vec<WastArg<'a>>,
}

impl<'a> Parse<'a> for WastInvoke<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::invoke>()?.0;
        let module = parser.parse()?;
        let name = parser.parse()?;
        let mut args = Vec::new();
        while !parser.is_empty() {
            args.push(parser.parens(|p| p.parse())?);
        }
        Ok(WastInvoke {
            span,
            module,
            name,
            args,
        })
    }
}

fn parse_wast_module<'a>(parser: Parser<'a>) -> Result<WastDirective<'a>> {
    if parser.peek2::<kw::quote>()? {
        QuoteWat::parse(parser).map(WastDirective::Module)
    } else if parser.peek2::<kw::definition>()? {
        fn parse_module(span: Span, parser: Parser<'_>) -> Result<Wat<'_>> {
            Ok(Wat::Module(
                crate::core::Module::parse_without_module_keyword(span, parser)?,
            ))
        }
        fn parse_component(_span: Span, parser: Parser<'_>) -> Result<Wat<'_>> {
            #[cfg(feature = "component-model")]
            return Ok(Wat::Component(
                crate::component::Component::parse_without_component_keyword(_span, parser)?,
            ));
            #[cfg(not(feature = "component-model"))]
            return Err(parser.error("component model support disabled at compile time"));
        }
        let (span, ctor) = if parser.peek::<kw::component>()? {
            (
                parser.parse::<kw::component>()?.0,
                parse_component as fn(_, _) -> _,
            )
        } else {
            (
                parser.parse::<kw::module>()?.0,
                parse_module as fn(_, _) -> _,
            )
        };
        parser.parse::<kw::definition>()?;
        Ok(WastDirective::ModuleDefinition(QuoteWat::Wat(ctor(
            span, parser,
        )?)))
    } else if parser.peek2::<kw::instance>()? {
        let span = if parser.peek::<kw::component>()? {
            parser.parse::<kw::component>()?.0
        } else {
            parser.parse::<kw::module>()?.0
        };
        parser.parse::<kw::instance>()?;
        Ok(WastDirective::ModuleInstance {
            span,
            instance: parser.parse()?,
            module: parser.parse()?,
        })
    } else {
        QuoteWat::parse(parser).map(WastDirective::Module)
    }
}

#[allow(missing_docs)]
#[derive(Debug)]
pub enum QuoteWat<'a> {
    Wat(Wat<'a>),
    QuoteModule(Span, Vec<(Span, &'a [u8])>),
    QuoteComponent(Span, Vec<(Span, &'a [u8])>),
}

impl<'a> QuoteWat<'a> {
    /// Encodes this module to bytes, either by encoding the module directly or
    /// parsing the contents and then encoding it.
    pub fn encode(&mut self) -> Result<Vec<u8>, Error> {
        match self.to_test()? {
            QuoteWatTest::Binary(bytes) => Ok(bytes),
            QuoteWatTest::Text(text) => {
                let text = std::str::from_utf8(&text).map_err(|_| {
                    let span = self.span();
                    Error::new(span, "malformed UTF-8 encoding".to_string())
                })?;
                let buf = ParseBuffer::new(&text)?;
                let mut wat = parser::parse::<Wat<'_>>(&buf)?;
                wat.encode()
            }
        }
    }

    /// Converts this to either a `QuoteWatTest::Binary` or
    /// `QuoteWatTest::Text` depending on what it is internally.
    pub fn to_test(&mut self) -> Result<QuoteWatTest, Error> {
        let (source, prefix) = match self {
            QuoteWat::Wat(m) => return m.encode().map(QuoteWatTest::Binary),
            QuoteWat::QuoteModule(_, source) => (source, None),
            QuoteWat::QuoteComponent(_, source) => (source, Some("(component")),
        };
        let mut ret = Vec::new();
        for (_, src) in source {
            ret.extend_from_slice(src);
            ret.push(b' ');
        }
        if let Some(prefix) = prefix {
            ret.splice(0..0, prefix.as_bytes().iter().copied());
            ret.push(b')');
        }
        Ok(QuoteWatTest::Text(ret))
    }

    /// Returns the identifier, if registered, for this module.
    pub fn name(&self) -> Option<Id<'a>> {
        match self {
            QuoteWat::Wat(Wat::Module(m)) => m.id,
            QuoteWat::Wat(Wat::Component(m)) => m.id,
            QuoteWat::QuoteModule(..) | QuoteWat::QuoteComponent(..) => None,
        }
    }

    /// Returns the defining span of this module.
    pub fn span(&self) -> Span {
        match self {
            QuoteWat::Wat(w) => w.span(),
            QuoteWat::QuoteModule(span, _) => *span,
            QuoteWat::QuoteComponent(span, _) => *span,
        }
    }
}

impl<'a> Parse<'a> for QuoteWat<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek2::<kw::quote>()? {
            let ctor = if parser.peek::<kw::component>()? {
                parser.parse::<kw::component>()?;
                QuoteWat::QuoteComponent
            } else {
                parser.parse::<kw::module>()?;
                QuoteWat::QuoteModule
            };
            let span = parser.parse::<kw::quote>()?.0;
            let mut src = Vec::new();
            while !parser.is_empty() {
                let span = parser.cur_span();
                let string = parser.parse()?;
                src.push((span, string));
            }
            Ok(ctor(span, src))
        } else {
            Ok(QuoteWat::Wat(parse_wat(parser)?))
        }
    }
}

/// Returned from [`QuoteWat::to_test`].
#[allow(missing_docs)]
#[derive(Debug)]
pub enum QuoteWatTest {
    Binary(Vec<u8>),
    Text(Vec<u8>),
}

#[derive(Debug)]
#[allow(missing_docs)]
pub enum WastArg<'a> {
    Core(WastArgCore<'a>),
    // TODO: technically this isn't cargo-compliant since it means that this
    // isn't and additive feature by defining this conditionally. That being
    // said this seems unlikely to break many in practice so this isn't a shared
    // type, so fixing this is left to a future commit.
    #[cfg(feature = "component-model")]
    Component(WastVal<'a>),
}

impl<'a> Parse<'a> for WastArg<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        #[cfg(feature = "component-model")]
        if parser.peek::<WastArgCore<'_>>()? {
            Ok(WastArg::Core(parser.parse()?))
        } else {
            Ok(WastArg::Component(parser.parse()?))
        }

        #[cfg(not(feature = "component-model"))]
        Ok(WastArg::Core(parser.parse()?))
    }
}

#[derive(Debug)]
#[allow(missing_docs)]
pub enum WastRet<'a> {
    Core(WastRetCore<'a>),
    #[cfg(feature = "component-model")]
    Component(WastVal<'a>),
}

impl<'a> Parse<'a> for WastRet<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        #[cfg(feature = "component-model")]
        if parser.peek::<WastRetCore<'_>>()? {
            Ok(WastRet::Core(parser.parse()?))
        } else {
            Ok(WastRet::Component(parser.parse()?))
        }

        #[cfg(not(feature = "component-model"))]
        Ok(WastRet::Core(parser.parse()?))
    }
}

#[derive(Debug)]
#[allow(missing_docs)]
pub struct WastThread<'a> {
    pub span: Span,
    pub name: Id<'a>,
    pub shared_module: Option<Id<'a>>,
    pub directives: Vec<WastDirective<'a>>,
}

impl<'a> Parse<'a> for WastThread<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.depth_check()?;
        let span = parser.parse::<kw::thread>()?.0;
        let name = parser.parse()?;

        let shared_module = if parser.peek2::<kw::shared>()? {
            let name = parser.parens(|p| {
                p.parse::<kw::shared>()?;
                p.parens(|p| {
                    p.parse::<kw::module>()?;
                    p.parse()
                })
            })?;
            Some(name)
        } else {
            None
        };
        let mut directives = Vec::new();
        while !parser.is_empty() {
            directives.push(parser.parens(|p| p.parse())?);
        }
        Ok(WastThread {
            span,
            name,
            shared_module,
            directives,
        })
    }
}
