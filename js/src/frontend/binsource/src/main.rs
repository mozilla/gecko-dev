extern crate binjs_meta;
extern crate clap;
extern crate env_logger;
extern crate itertools;
#[macro_use] extern crate log;
extern crate webidl;
extern crate yaml_rust;

use binjs_meta::export::{ ToWebidl, TypeDeanonymizer, TypeName };
use binjs_meta::import::Importer;
use binjs_meta::spec::*;
use binjs_meta::util:: { Reindentable, ToCases, ToStr };

mod refgraph;

use refgraph::{ ReferenceGraph };

use std::collections::{ HashMap, HashSet };
use std::fs::*;
use std::io::{ Read, Write };
use std::path::Path;
use std::rc::Rc;

use clap::{ App, Arg };

use itertools::Itertools;

/// Rules for generating the code for parsing a single field
/// of a node.
///
/// Extracted from the yaml file.
#[derive(Clone, Default)]
struct FieldRules {
    /// Declaring the variable to hold the contents of that field.
    declare: Option<String>,

    /// Replace the declaration and assignation.
    replace: Option<String>,

    /// Things to add before the field, typically for checking invariants.
    before_field: Option<String>,

    /// Things to add after the field, typically for checking invariants.
    after_field: Option<String>,

    /// Things to add before the field, as part of a block, typically for
    /// putting guard values on the stack.
    block_before_field: Option<String>,

    /// Things to add before the field, as part of a block, typically for
    /// cleanup.
    block_after_field: Option<String>,

    /// Extra arguments passed to the method when parsing this field.
    extra_args: Option<Rc<String>>,
}

#[derive(Clone, Default)]
struct SumRules {
    after_arm: Option<String>,

    // Disable this arm (false by default).
    disabled: bool,
}


/// Rules for generating the code for parsing a full node
/// of a node.
///
/// Extracted from the yaml file.
#[derive(Clone, Default)]
struct NodeRules {
    /// This node inherits from another node.
    inherits: Option<NodeName>,

    /// Override the result type for the method.
    type_ok: Option<Rc<String>>,

    /// Default value for the optional field.
    default_value: Option<Rc<String>>,

    /// Extra parameters for the method.
    extra_params: Option<Rc<String>>,

    /// Extra arguments passed to the method when parsing this interface.
    /// For ListOf* interfaces, this arguments are passed to each item.
    /// For sum interface, this arguments are passed to each interface.
    extra_args: Option<Rc<String>>,

    /// Things to add before calling the method when the optional field has
    /// value.
    some_before: Option<Rc<String>>,

    /// Things to add after calling the method when the optional field has
    /// value.
    some_after: Option<Rc<String>>,

    /// Replace the assignment when the option field has no value.
    none_replace: Option<Rc<String>>,

    /// Stuff to add at start.
    init: Option<String>,

    /// How to append to a list. Used only for lists.
    append: Option<String>,

    /// Custom per-field treatment. Used only for interfaces.
    by_field: HashMap<FieldName, FieldRules>,

    by_sum: HashMap<NodeName, SumRules>,

    /// How to build the result, eventually.
    build_result: Option<String>,
}

/// Rules for generating entire files.
///
/// Extracted from the yaml file.
#[derive(Default)]
struct GlobalRules {
    /// C++ class name of the parser class.
    parser_class_name: Rc<String>,

    /// The template part of the parser class.
    parser_class_template: Rc<String>,

    /// The return value of each method.
    parser_type_ok: Rc<String>,

    /// Default value for the optional field.
    parser_default_value: Rc<String>,

    /// Code to append the list item.
    parser_list_append: Option<Rc<String>>,

    /// Header to add at the start of the .cpp file.
    cpp_header: Option<String>,

    /// Header to add at the end of the .cpp file.
    cpp_footer: Option<String>,

    /// Header to add at the start of the .hpp file.
    /// defining the class.
    hpp_class_header: Option<String>,

    /// Footer to add at the end of the .hpp file
    /// defining the class.
    hpp_class_footer: Option<String>,

    /// Header to add at the start of the .hpp file.
    /// defining the enums.
    hpp_enums_header: Option<String>,

    /// Footer to add at the end of the .hpp file
    /// defining the enums.
    hpp_enums_footer: Option<String>,

    /// Header to add at the start of the .hpp file.
    /// defining the tokens.
    hpp_tokens_header: Option<String>,

    /// Footer to add at the start of the .hpp file.
    /// defining the tokens.
    hpp_tokens_footer: Option<String>,

    /// Documentation for the `BinKind` class enum.
    hpp_tokens_kind_doc: Option<String>,

    /// Documentation for the `BinField` class enum.
    hpp_tokens_field_doc: Option<String>,

    /// Documentation for the `BinVariant` class enum.
    hpp_tokens_variants_doc: Option<String>,

    /// Per-node rules.
    per_node: HashMap<NodeName, NodeRules>,
}
impl GlobalRules {
    fn new(syntax: &Spec, yaml: &yaml_rust::yaml::Yaml) -> Self {
        let rules = yaml.as_hash()
            .expect("Rules are not a dictionary");

        let mut parser_class_name = None;
        let mut parser_class_template = None;
        let mut parser_type_ok = None;
        let mut parser_default_value = None;
        let mut parser_list_append = None;
        let mut cpp_header = None;
        let mut cpp_footer = None;
        let mut hpp_class_header = None;
        let mut hpp_class_footer = None;
        let mut hpp_enums_header = None;
        let mut hpp_enums_footer = None;
        let mut hpp_tokens_header = None;
        let mut hpp_tokens_footer = None;
        let mut hpp_tokens_kind_doc = None;
        let mut hpp_tokens_field_doc = None;
        let mut hpp_tokens_variants_doc = None;
        let mut per_node = HashMap::new();

        for (node_key, node_entries) in rules.iter() {
            let node_key = node_key.as_str()
                .expect("Could not convert node_key to string");

            match node_key {
                "parser" => {
                    update_rule_rc(&mut parser_class_name, &node_entries["class-name"])
                        .unwrap_or_else(|_| panic!("Rule parser.class-name must be a string"));
                    update_rule(&mut parser_class_template, &node_entries["class-template"])
                        .unwrap_or_else(|_| panic!("Rule parser.class-template must be a string"));
                    update_rule_rc(&mut parser_type_ok, &node_entries["type-ok"])
                        .unwrap_or_else(|_| panic!("Rule parser.type-ok must be a string"));
                    update_rule_rc(&mut parser_default_value, &node_entries["default-value"])
                        .unwrap_or_else(|_| panic!("Rule parser.default-value must be a string"));
                    update_rule_rc(&mut parser_list_append, &node_entries["list"]["append"])
                        .unwrap_or_else(|_| panic!("Rule parser.list.append must be a string"));
                    continue;
                }
                "cpp" => {
                    update_rule(&mut cpp_header, &node_entries["header"])
                        .unwrap_or_else(|_| panic!("Rule cpp.header must be a string"));
                    update_rule(&mut cpp_footer, &node_entries["footer"])
                        .unwrap_or_else(|_| panic!("Rule cpp.footer must be a string"));
                    continue;
                }
                "hpp" => {
                    update_rule(&mut hpp_class_header, &node_entries["class"]["header"])
                        .unwrap_or_else(|_| panic!("Rule hpp.class.header must be a string"));
                    update_rule(&mut hpp_class_footer, &node_entries["class"]["footer"])
                        .unwrap_or_else(|_| panic!("Rule hpp.class.footer must be a string"));
                    update_rule(&mut hpp_enums_header, &node_entries["enums"]["header"])
                        .unwrap_or_else(|_| panic!("Rule hpp.enum.header must be a string"));
                    update_rule(&mut hpp_enums_footer, &node_entries["enums"]["footer"])
                        .unwrap_or_else(|_| panic!("Rule hpp.enum.footer must be a string"));
                    update_rule(&mut hpp_tokens_header, &node_entries["tokens"]["header"])
                        .unwrap_or_else(|_| panic!("Rule hpp.tokens.header must be a string"));
                    update_rule(&mut hpp_tokens_footer, &node_entries["tokens"]["footer"])
                        .unwrap_or_else(|_| panic!("Rule hpp.tokens.footer must be a string"));
                    update_rule(&mut hpp_tokens_kind_doc, &node_entries["tokens"]["kind"]["doc"])
                        .unwrap_or_else(|_| panic!("Rule hpp.tokens.kind.doc must be a string"));
                    update_rule(&mut hpp_tokens_field_doc, &node_entries["tokens"]["field"]["doc"])
                        .unwrap_or_else(|_| panic!("Rule hpp.tokens.field.doc must be a string"));
                    update_rule(&mut hpp_tokens_variants_doc, &node_entries["tokens"]["variants"]["doc"])
                        .unwrap_or_else(|_| panic!("Rule hpp.tokens.variants.doc must be a string"));
                    continue;
                }
                _ => {}
            }


            let node_name = syntax.get_node_name(&node_key)
                .unwrap_or_else(|| panic!("Unknown node name {}", node_key));

            let hash = node_entries.as_hash()
                .unwrap_or_else(|| panic!("Node {} isn't a dictionary"));

            let mut node_rule = NodeRules::default();
            for (node_item_key, node_item_entry) in hash {
                let as_string = node_item_key.as_str()
                    .unwrap_or_else(|| panic!("Keys for rule {} must be strings", node_key));
                match as_string {
                    "inherits" => {
                        let name = node_item_entry.as_str()
                            .unwrap_or_else(|| panic!("Rule {}.{} must be a string", node_key, as_string));
                        let inherits = syntax.get_node_name(name)
                            .unwrap_or_else(|| panic!("Unknown node name {}", name));
                        node_rule.inherits = Some(inherits).cloned();
                    }
                    "extra-params" => {
                        update_rule_rc(&mut node_rule.extra_params, node_item_entry)
                            .unwrap_or_else(|()| panic!("Rule {}.{} must be a string", node_key, as_string));
                    }
                    "extra-args" => {
                        update_rule_rc(&mut node_rule.extra_args, node_item_entry)
                            .unwrap_or_else(|()| panic!("Rule {}.{} must be a string", node_key, as_string));
                    }
                    "some" => {
                        update_rule_rc(&mut node_rule.some_before, &node_item_entry["before"])
                            .unwrap_or_else(|()| panic!("Rule {}.{}.before must be a string", node_key, as_string));
                        update_rule_rc(&mut node_rule.some_after, &node_item_entry["after"])
                            .unwrap_or_else(|()| panic!("Rule {}.{}.after must be a string", node_key, as_string));
                    }
                    "none" => {
                        update_rule_rc(&mut node_rule.none_replace, &node_item_entry["replace"])
                            .unwrap_or_else(|()| panic!("Rule {}.{}.replace must be a string", node_key, as_string));
                    }
                    "init" => {
                        update_rule(&mut node_rule.init, node_item_entry)
                            .unwrap_or_else(|()| panic!("Rule {}.{} must be a string", node_key, as_string));
                    }
                    "build" => {
                        update_rule(&mut node_rule.build_result, node_item_entry)
                            .unwrap_or_else(|()| panic!("Rule {}.{} must be a string", node_key, as_string));
                    }
                    "append" => {
                        update_rule(&mut node_rule.append, node_item_entry)
                            .unwrap_or_else(|()| panic!("Rule {}.{} must be a string", node_key, as_string));
                    }
                    "type-ok" => {
                        update_rule_rc(&mut node_rule.type_ok, node_item_entry)
                            .unwrap_or_else(|()| panic!("Rule {}.{} must be a string", node_key, as_string));
                    }
                    "default-value" => {
                        update_rule_rc(&mut node_rule.default_value, node_item_entry)
                            .unwrap_or_else(|()| panic!("Rule {}.{} must be a string", node_key, as_string));
                    }
                    "fields" => {
                        let fields = node_item_entry.as_hash()
                            .unwrap_or_else(|| panic!("Rule {}.fields must be a hash, got {:?}", node_key, node_entries["fields"]));
                        for (field_key, field_entry) in fields {
                            let field_key = field_key.as_str()
                                .unwrap_or_else(|| panic!("In rule {}, field entries must be field names",
                                    node_key))
                                .to_string();
                            let field_name = syntax.get_field_name(&field_key)
                                .unwrap_or_else(|| panic!("In rule {}, can't find field {}",
                                    node_key,
                                    field_key));

                            let mut field_rule = FieldRules::default();
                            for (field_config_key, field_config_entry) in field_entry.as_hash()
                                .unwrap_or_else(|| panic!("Rule {}.fields.{} must be a hash", node_key, field_key))
                            {
                                let field_config_key = field_config_key.as_str()
                                    .expect("Expected a string as a key");
                                match field_config_key
                                {
                                    "block" => {
                                        update_rule(&mut field_rule.declare, &field_config_entry["declare"])
                                            .unwrap_or_else(|()| panic!("Rule {}.fields.{}.{}.{} must be a string", node_key, field_key, field_config_key, "declare"));

                                        update_rule(&mut field_rule.replace, &field_config_entry["replace"])
                                            .unwrap_or_else(|()| panic!("Rule {}.fields.{}.{}.{} must be a string", node_key, field_key, field_config_key, "replace"));

                                        update_rule(&mut field_rule.block_before_field, &field_config_entry["before"])
                                            .unwrap_or_else(|()| panic!("Rule {}.fields.{}.{}.{} must be a string", node_key, field_key, field_config_key, "before"));

                                        update_rule(&mut field_rule.block_after_field, &field_config_entry["after"])
                                            .unwrap_or_else(|()| panic!("Rule {}.fields.{}.{}.{} must be a string", node_key, field_key, field_config_key, "after"));
                                    }
                                    "before" => {
                                        update_rule(&mut field_rule.before_field, &field_config_entry)
                                            .unwrap_or_else(|()| panic!("Rule {}.fields.{}.{} must be a string", node_key, field_key, field_config_key));
                                    }
                                    "after" => {
                                        update_rule(&mut field_rule.after_field, &field_config_entry)
                                            .unwrap_or_else(|()| panic!("Rule {}.fields.{}.{} must be a string", node_key, field_key, field_config_key));
                                    }
                                    "extra-args" => {
                                        update_rule_rc(&mut field_rule.extra_args, &field_config_entry)
                                            .unwrap_or_else(|()| panic!("Rule {}.fields.{}.{} must be a string", node_key, field_key, field_config_key));
                                    }
                                    _ => {
                                        panic!("Unexpected {}.fields.{}.{}", node_key, field_key, field_config_key)
                                    }
                                }
                            }
                            node_rule.by_field.insert(field_name.clone(), field_rule);
                        }
                    }
                    "sum-arms" => {
                        let arms = node_item_entry.as_hash()
                            .unwrap_or_else(|| panic!("Rule {}.sum-arms must be a hash, got {:?}", node_key, node_entries["sum-arms"]));
                        for (sum_arm_key, sum_arm_entry) in arms {
                            let sum_arm_key = sum_arm_key.as_str()
                                .unwrap_or_else(|| panic!("In rule {}, sum arms must be interface names"));
                            let sum_arm_name = syntax.get_node_name(&sum_arm_key)
                                .unwrap_or_else(|| panic!("In rule {}. cannot find interface {}", node_key, sum_arm_key));

                            let mut sum_rule = SumRules::default();
                            for (arm_config_key, arm_config_entry) in sum_arm_entry.as_hash()
                                .unwrap_or_else(|| panic!("Rule {}.sum-arms.{} must be a hash", node_key, sum_arm_key))
                            {
                                let arm_config_key = arm_config_key.as_str()
                                    .expect("Expected a string as a key");
                                match arm_config_key
                                {
                                    "after" => {
                                        update_rule(&mut sum_rule.after_arm, arm_config_entry)
                                            .unwrap_or_else(|()| panic!("Rule {}.sum-arms.{}.{} must be a string", node_key, sum_arm_key, arm_config_key));
                                    }
                                    "disabled" => {
                                        if let Some(disabled) = arm_config_entry.as_bool() {
                                            if disabled {
                                                sum_rule.disabled = true;
                                            }
                                        } else {
                                            panic!("Rule {}.sum-arms.{}.{} must be a bool", node_key, sum_arm_key, arm_config_key);
                                        }
                                    }
                                    _ => {
                                        panic!("Unexpected {}.sum-arms.{}.{}", node_key, sum_arm_key, arm_config_key);
                                    }
                                }
                            }
                            node_rule.by_sum.insert(sum_arm_name.clone(), sum_rule);
                        }
                    }
                    _ => panic!("Unexpected node_item_key {}.{}", node_key, as_string)
                }
            }

            per_node.insert(node_name.clone(), node_rule);
        }

        Self {
            parser_class_name: parser_class_name
                .expect("parser.class-name should be specified"),
            parser_class_template: Rc::new(if parser_class_template.is_some() {
                format!("{} ", parser_class_template.unwrap())
            } else {
                "".to_string()
            }),
            parser_type_ok: parser_type_ok
                .expect("parser.type-ok should be specified"),
            parser_default_value: parser_default_value
                .expect("parser.default-value should be specified"),
            parser_list_append,
            cpp_header,
            cpp_footer,
            hpp_class_header,
            hpp_class_footer,
            hpp_enums_header,
            hpp_enums_footer,
            hpp_tokens_header,
            hpp_tokens_footer,
            hpp_tokens_kind_doc,
            hpp_tokens_field_doc,
            hpp_tokens_variants_doc,
            per_node,
        }
    }
    fn get(&self, name: &NodeName) -> NodeRules {
        let mut rules = self.per_node.get(name)
            .cloned()
            .unwrap_or_default();
        if let Some(ref parent) = rules.inherits {
            let NodeRules {
                inherits: _,
                type_ok,
                default_value,
                extra_params,
                extra_args,
                some_before,
                some_after,
                none_replace,
                init,
                append,
                by_field,
                by_sum,
                build_result,
            } = self.get(parent);
            if rules.type_ok.is_none() {
                rules.type_ok = type_ok;
            }
            if rules.default_value.is_none() {
                rules.default_value = default_value;
            }
            if rules.extra_params.is_none() {
                rules.extra_params = extra_params;
            }
            if rules.extra_args.is_none() {
                rules.extra_args = extra_args;
            }
            if rules.some_before.is_none() {
                rules.some_before = some_before;
            }
            if rules.some_after.is_none() {
                rules.some_after = some_after;
            }
            if rules.none_replace.is_none() {
                rules.none_replace = none_replace;
            }
            if rules.init.is_none() {
                rules.init = init;
            }
            if rules.append.is_none() {
                rules.append = append;
            }
            if rules.build_result.is_none() {
                rules.build_result = build_result;
            }
            for (key, value) in by_field {
                rules.by_field.entry(key)
                    .or_insert(value);
            }
            for (key, value) in by_sum {
                rules.by_sum.entry(key)
                    .or_insert(value);
            }
        }
        rules
    }
}

/// The inforamtion used to generate a list parser.
struct ListParserData {
    /// Name of the node.
    name: NodeName,

    /// If `true`, supports empty lists.
    supports_empty: bool,

    /// Name of the elements in the list.
    elements: NodeName,
}

/// The inforamtion used to generate a parser for an optional data structure.
struct OptionParserData {
    /// Name of the node.
    name: NodeName,

    /// Name of the element that may be contained.
    elements: NodeName,
}

/// What to use when calling the method to store the result value.
enum MethodCallKind {
    /// Use BINJS_MOZ_TRY_DECL if the result type is not "Ok",
    /// use MOZ_TRY otherwise.
    Decl,

    /// Always use MOZ_TRY_DECL regardless of the result type.
    AlwaysDecl,

    /// Use MOZ_TRY_VAR if the result type is not "Ok",
    /// use MOZ_TRY otherwise.
    Var,

    /// Always use MOZ_TRY_VAR regardless of the result type.
    AlwaysVar,
}

/// Fixed parameter of interface method.
const INTERFACE_PARAMS: &str =
    "const size_t start, const BinKind kind, const BinFields& fields";

/// Fixed arguments of interface method.
const INTERFACE_ARGS: &str =
    "start, kind, fields";

/// The name of the toplevel interface for the script.
const TOPLEVEL_INTERFACE: &str =
    "Program";

/// The actual exporter.
struct CPPExporter {
    /// The syntax to export.
    syntax: Spec,

    /// Rules, as specified in yaml.
    rules: GlobalRules,

    /// Reference graph of the method call.
    refgraph: ReferenceGraph,

    /// All parsers of lists.
    list_parsers_to_generate: Vec<ListParserData>,

    /// All parsers of options.
    option_parsers_to_generate: Vec<OptionParserData>,

    /// A mapping from symbol (e.g. `+`, `-`, `instanceof`, ...) to the
    /// name of the symbol as part of `enum class BinVariant`
    /// (e.g. `UnaryOperatorDelete`).
    variants_by_symbol: HashMap<String, String>,

    // A map from enum class names to the type.
    enum_types: HashMap<NodeName, Rc<String>>,
}

impl CPPExporter {
    fn new(syntax: Spec, rules: GlobalRules) -> Self {
        let mut list_parsers_to_generate = vec![];
        let mut option_parsers_to_generate = vec![];
        for (parser_node_name, typedef) in syntax.typedefs_by_name() {
            if typedef.is_optional() {
                let content_name = TypeName::type_spec(typedef.spec());
                let content_node_name = syntax.get_node_name(&content_name)
                    .unwrap_or_else(|| panic!("While generating an option parser, could not find node name \"{}\"", content_name))
                    .clone();
                debug!(target: "generate_spidermonkey", "CPPExporter::new adding optional typedef {:?} => {:?} => {:?}",
                    parser_node_name,
                    content_name,
                    content_node_name);
                option_parsers_to_generate.push(OptionParserData {
                    name: parser_node_name.clone(),
                    elements: content_node_name
                });
            } else if let TypeSpec::Array { ref contents, ref supports_empty } = *typedef.spec() {
                let content_name = TypeName::type_(&**contents);
                let content_node_name = syntax.get_node_name(&content_name)
                    .unwrap_or_else(|| panic!("While generating an array parser, could not find node name {}", content_name))
                    .clone();
                list_parsers_to_generate.push(ListParserData {
                    name: parser_node_name.clone(),
                    supports_empty: *supports_empty,
                    elements: content_node_name
                });
            }
        }
        list_parsers_to_generate.sort_by(|a, b| str::cmp(a.name.to_str(), b.name.to_str()));
        option_parsers_to_generate.sort_by(|a, b| str::cmp(a.name.to_str(), b.name.to_str()));

        // Prepare variant_by_symbol, which will let us lookup the BinVariant name of
        // a symbol. Since some symbols can appear in several enums (e.g. "+"
        // is both a unary and a binary operator), we need to collect all the
        // string enums that contain each symbol and come up with a unique name
        // (note that there is no guarantee of unicity – if collisions show up,
        // we may need to tweak the name generation algorithm).
        let mut enum_by_string : HashMap<String, Vec<NodeName>> = HashMap::new();
        let mut enum_types : HashMap<NodeName, Rc<String>> = HashMap::new();
        for (name, enum_) in syntax.string_enums_by_name().iter() {
            let type_ = format!("typename {parser_class_name}::{kind}",
                                parser_class_name = rules.parser_class_name,
                                kind = name.to_class_cases());
            enum_types.insert(name.clone(), Rc::new(type_));
            for string in enum_.strings().iter() {
                let vec = enum_by_string.entry(string.clone())
                    .or_insert_with(|| vec![]);
                vec.push(name.clone());
            }
        }
        let variants_by_symbol = enum_by_string.drain()
            .map(|(string, names)| {
                let expanded = format!("{names}{symbol}",
                    names = names.iter()
                        .map(NodeName::to_str)
                        .sorted()
                        .into_iter()
                        .format("Or"),
                    symbol = string.to_cpp_enum_case());
                (string, expanded)
            })
            .collect();

        // This is just a placeholder to instantiate the CPPExporter struct.
        // The field will be overwritten later in generate_reference_graph.
        let refgraph = ReferenceGraph::new();

        CPPExporter {
            syntax,
            rules,
            refgraph,
            list_parsers_to_generate,
            option_parsers_to_generate,
            variants_by_symbol,
            enum_types,
        }
    }

    /// Generate a reference graph of methods.
    fn generate_reference_graph(&mut self) {
        let mut refgraph = ReferenceGraph::new();

        // FIXME: Reflect `replace` rule in yaml file for each interface to
        //        the reference (bug 1504595).

        // 1. Typesums
        let sums_of_interfaces = self.syntax.resolved_sums_of_interfaces_by_name();
        for (name, nodes) in sums_of_interfaces {
            let rules_for_this_sum = self.rules.get(name);

            let mut edges: HashSet<Rc<String>> = HashSet::new();
            edges.insert(Rc::new(format!("Sum{}", name)));
            refgraph.insert(name.to_rc_string().clone(), edges);

            let mut sum_edges: HashSet<Rc<String>> = HashSet::new();
            for node in nodes {
                let rule_for_this_arm = rules_for_this_sum.by_sum.get(&node)
                    .cloned()
                    .unwrap_or_default();

                // If this arm is disabled, we emit raiseError instead of
                // call to parseInterface*.  Do not add edge in that case.
                if rule_for_this_arm.disabled {
                    continue;
                }

                sum_edges.insert(Rc::new(format!("Interface{}", node.to_string())));
            }
            refgraph.insert(Rc::new(format!("Sum{}", name.to_string())), sum_edges);
        }

        // 2. Single interfaces
        let interfaces_by_name = self.syntax.interfaces_by_name();
        for (name, interface) in interfaces_by_name {
            let rules_for_this_interface = self.rules.get(name);
            let is_implemented = rules_for_this_interface.build_result.is_some();
            // If this interafce is not implemented, parse* method should
            // not be called nor referenced in the graph.
            if is_implemented {
                let mut edges: HashSet<Rc<String>> = HashSet::new();
                edges.insert(Rc::new(format!("Interface{}", name)));
                refgraph.insert(name.to_rc_string().clone(), edges);
            }

            let mut interface_edges: HashSet<Rc<String>> = HashSet::new();
            // If this interface is not implemented, we emit raiseError in
            // parseInterface* method, instead of parse* for each fields.
            // There can be reference to parseInterface* of this interface
            // from sum interface, and this node needs to be represented in
            // the reference graph.
            if is_implemented {
                for field in interface.contents().fields() {
                    match field.type_().get_primitive(&self.syntax) {
                        Some(IsNullable { is_nullable: _, content: Primitive::Interface(_) })
                            | None => {
                                let typename = TypeName::type_(field.type_());
                                interface_edges.insert(Rc::new(typename.to_string()));
                            },

                        // Don't have to handle other type of fields (string,
                        // number, bool, etc).
                        _ => {}
                    }
                }
            }
            refgraph.insert(Rc::new(format!("Interface{}", name)), interface_edges);
        }

        // 3. String Enums
        for (kind, _) in self.syntax.string_enums_by_name() {
            refgraph.insert(kind.to_rc_string().clone(), HashSet::new());
        }

        // 4. Lists
        for parser in &self.list_parsers_to_generate {
            let name = &parser.name;
            let rules_for_this_list = self.rules.get(name);
            let is_implemented = rules_for_this_list.init.is_some();
            // If this list is not implemented, this method should not be
            // called nor referenced in the graph.
            if !is_implemented {
                continue;
            }

            let mut edges: HashSet<Rc<String>> = HashSet::new();
            edges.insert(parser.elements.to_rc_string().clone());
            refgraph.insert(name.to_rc_string().clone(), edges);
        }

        // 5. Optional values
        for parser in &self.option_parsers_to_generate {
            let mut edges: HashSet<Rc<String>> = HashSet::new();
            let named_implementation =
                if let Some(NamedType::Typedef(ref typedef)) = self.syntax.get_type_by_name(&parser.name) {
                    assert!(typedef.is_optional());
                    if let TypeSpec::NamedType(ref named) = *typedef.spec() {
                        self.syntax.get_type_by_name(named)
                            .unwrap_or_else(|| panic!("Internal error: Could not find type {}, which should have been generated.", named.to_str()))
                    } else {
                        panic!("Internal error: In {}, type {:?} should have been a named type",
                               parser.name.to_str(),
                               typedef);
                    }
                } else {
                    panic!("Internal error: In {}, there should be a type with that name",
                           parser.name.to_str());
                };
            match named_implementation {
                NamedType::Interface(_) => {
                    edges.insert(Rc::new(format!("Interface{}", parser.elements.to_string())));
                },
                NamedType::Typedef(ref type_) => {
                    match type_.spec() {
                        &TypeSpec::TypeSum(_) => {
                            edges.insert(Rc::new(format!("Sum{}", parser.elements.to_string())));
                        },
                        _ => {}
                    }
                },
                _ => {}
            }
            refgraph.insert(parser.name.to_rc_string().clone(), edges);
        }

        // 6. Primitive values.
        refgraph.insert(Rc::new("IdentifierName".to_string()), HashSet::new());
        refgraph.insert(Rc::new("PropertyKey".to_string()), HashSet::new());

        self.refgraph = refgraph;
    }

    /// Trace the reference graph from the node with `name and mark all nodes
    /// as used. `name` is the name of the method, without leading "parse".
    fn trace(&mut self, name: Rc<String>) {
        self.refgraph.trace(name)
    }

// ----- Generating the header

    /// Get the type representing a success for parsing this node.
    fn get_type_ok(&self, name: &NodeName) -> Rc<String> {
        // enum has its own rule.
        if self.enum_types.contains_key(name) {
            return self.enum_types.get(name).unwrap().clone();
        }

        let rules_for_this_interface = self.rules.get(name);
        // If the override is provided, use it.
        if let Some(type_ok) = rules_for_this_interface.type_ok {
            return type_ok;
        }
        self.rules.parser_type_ok.clone()
    }
    fn get_default_value(&self, name: &NodeName) -> Rc<String> {
        let rules_for_this_interface = self.rules.get(name);
        // If the override is provided, use it.
        if let Some(default_value) = rules_for_this_interface.default_value {
            return default_value;
        }
        if let Some(type_ok) = rules_for_this_interface.type_ok {
            if type_ok.as_str() == "Ok" {
                return Rc::new("Ok()".to_string());
            }
        }
        self.rules.parser_default_value.clone()
    }

    fn get_method_signature(&self, name: &NodeName, prefix: &str, args: &str,
                            extra_params: &Option<Rc<String>>) -> String {
        let type_ok = self.get_type_ok(name);
        let kind = name.to_class_cases();
        let extra = match extra_params {
            Some(s) => {
                format!("{}\n{}",
                        if args.len() > 0 {
                            ","
                        } else {
                            ""
                        },
                        s.reindent("        "))
            }
            _ => {
                "".to_string()
            }
        };
        format!("    JS::Result<{type_ok}> parse{prefix}{kind}({args}{extra});\n",
            prefix = prefix,
            type_ok = type_ok,
            kind = kind,
            args = args,
            extra = extra,
        )
    }

    fn get_method_definition_start(&self, name: &NodeName, prefix: &str, args: &str,
                                   extra_params: &Option<Rc<String>>) -> String {
        let type_ok = self.get_type_ok(name);
        let kind = name.to_class_cases();
        let extra = match extra_params {
            Some(s) => {
                format!("{}\n{}",
                        if args.len() > 0 {
                            ","
                        } else {
                            ""
                        },
                        s.reindent("        "))
            }
            _ => {
                "".to_string()
            }
        };
        format!("{parser_class_template}JS::Result<{type_ok}>\n{parser_class_name}::parse{prefix}{kind}({args}{extra})",
            parser_class_template = self.rules.parser_class_template,
            prefix = prefix,
            type_ok = type_ok,
            parser_class_name = self.rules.parser_class_name,
            kind = kind,
            args = args,
            extra = extra,
        )
    }

    fn get_method_call(&self, var_name: &str, name: &NodeName,
                       prefix: &str, args: &str,
                       extra_params: &Option<Rc<String>>,
                       call_kind: MethodCallKind) -> String {
        let type_ok_is_ok = match call_kind {
            MethodCallKind::Decl | MethodCallKind::Var => {
                self.get_type_ok(name).to_str() == "Ok"
            }
            MethodCallKind::AlwaysDecl | MethodCallKind::AlwaysVar => {
                false
            }
        };
        let extra = match extra_params {
            Some(s) => {
                format!("{}\n{}",
                        if args.len() > 0 {
                            ","
                        } else {
                            ""
                        },
                        s.reindent("    "))
            }
            _ => {
                "".to_string()
            }
        };
        let call = format!("parse{prefix}{name}({args}{extra})",
                           prefix = prefix,
                           name = name.to_class_cases(),
                           args = args,
                           extra = extra);

        if type_ok_is_ok {
            // Special case: `Ok` means that we shouldn't bind the return value.
            format!("MOZ_TRY({call});",
                    call = call)
        } else {
            match call_kind {
                MethodCallKind::Decl | MethodCallKind::AlwaysDecl => {
                    format!("BINJS_MOZ_TRY_DECL({var_name}, {call});",
                            var_name = var_name, call = call)
                }
                MethodCallKind::Var | MethodCallKind::AlwaysVar => {
                    format!("MOZ_TRY_VAR({var_name}, {call});",
                            var_name = var_name, call = call)
                }
            }
        }
    }

    /// Declaring enums for kinds and fields.
    fn export_declare_kinds_and_fields_enums(&self, buffer: &mut String) {
        buffer.push_str(&self.rules.hpp_tokens_header.reindent(""));

        buffer.push_str("\n\n");
        if self.rules.hpp_tokens_kind_doc.is_some() {
            buffer.push_str(&self.rules.hpp_tokens_kind_doc.reindent(""));
        }

        let node_names = self.syntax.node_names()
            .keys()
            .sorted();
        buffer.push_str(&format!("\n#define FOR_EACH_BIN_KIND(F) \\\n{nodes}\n",
            nodes = node_names.iter()
                .map(|name| format!("    F({enum_name}, \"{spec_name}\")",
                    enum_name = name.to_cpp_enum_case(),
                    spec_name = name))
                .format(" \\\n")));
        buffer.push_str("
enum class BinKind {
#define EMIT_ENUM(name, _) name,
    FOR_EACH_BIN_KIND(EMIT_ENUM)
#undef EMIT_ENUM
};
");

        buffer.push_str(&format!("\n// The number of distinct values of BinKind.\nconst size_t BINKIND_LIMIT = {};\n\n\n", self.syntax.node_names().len()));
        buffer.push_str("\n\n");
        if self.rules.hpp_tokens_field_doc.is_some() {
            buffer.push_str(&self.rules.hpp_tokens_field_doc.reindent(""));
        }

        let field_names = self.syntax.field_names()
            .keys()
            .sorted();
        buffer.push_str(&format!("\n#define FOR_EACH_BIN_FIELD(F) \\\n{nodes}\n",
            nodes = field_names.iter()
                .map(|name| format!("    F({enum_name}, \"{spec_name}\")",
                    spec_name = name,
                    enum_name = name.to_cpp_enum_case()))
                .format(" \\\n")));
        buffer.push_str("
enum class BinField {
#define EMIT_ENUM(name, _) name,
    FOR_EACH_BIN_FIELD(EMIT_ENUM)
#undef EMIT_ENUM
};
");
        buffer.push_str(&format!("\n// The number of distinct values of BinField.\nconst size_t BINFIELD_LIMIT = {};\n\n\n", self.syntax.field_names().len()));

        if self.rules.hpp_tokens_variants_doc.is_some() {
            buffer.push_str(&self.rules.hpp_tokens_variants_doc.reindent(""));
        }
        let enum_variants : Vec<_> = self.variants_by_symbol
            .iter()
            .sorted_by(|&(ref symbol_1, ref name_1), &(ref symbol_2, ref name_2)| {
                Ord::cmp(name_1, name_2)
                    .then_with(|| Ord::cmp(symbol_1, symbol_2))
            });

        buffer.push_str(&format!("\n#define FOR_EACH_BIN_VARIANT(F) \\\n{nodes}\n",
            nodes = enum_variants.into_iter()
                .map(|(symbol, name)| format!("    F({variant_name}, \"{spec_name}\")",
                    spec_name = symbol,
                    variant_name = name))
                .format(" \\\n")));

        buffer.push_str("
enum class BinVariant {
#define EMIT_ENUM(name, _) name,
    FOR_EACH_BIN_VARIANT(EMIT_ENUM)
#undef EMIT_ENUM
};
");
        buffer.push_str(&format!("\n// The number of distinct values of BinVariant.\nconst size_t BINVARIANT_LIMIT = {};\n\n\n",
            self.variants_by_symbol.len()));

        buffer.push_str(&self.rules.hpp_tokens_footer.reindent(""));
        buffer.push_str("\n");
    }

    /// Declare string enums
    fn export_declare_string_enums(&self, buffer: &mut String) {
        buffer.push_str("
// ----- Declaring string enums (by lexicographical order)
");
        let string_enums_by_name = self.syntax.string_enums_by_name()
            .iter()
            .sorted_by(|a, b| str::cmp(a.0.to_str(), b.0.to_str()));
        for (name, enum_) in string_enums_by_name {
            let rendered_cases = enum_.strings()
                .iter()
                .map(|str| format!("    // \"{original}\"
    {case},",
                    case = str.to_cpp_enum_case(),
                    original = str))
                .format("\n");
            let rendered = format!("
enum class {name} {{
{cases}
}};
",
                cases = rendered_cases,
                name = name.to_class_cases());
            buffer.push_str(&rendered);
        }
    }

    fn export_declare_sums_of_interface_methods(&self, buffer: &mut String) {
        let sums_of_interfaces = self.syntax.resolved_sums_of_interfaces_by_name()
            .iter()
            .sorted_by(|a, b| a.0.cmp(&b.0));
        buffer.push_str("
    // ----- Sums of interfaces (by lexicographical order)
    // `ParseNode*` may never be nullptr
");
        for &(ref name, _) in &sums_of_interfaces {
            if !self.refgraph.is_used(name.to_rc_string().clone()) {
                continue;
            }

            let rules_for_this_sum = self.rules.get(name);
            let extra_params = rules_for_this_sum.extra_params;
            let rendered = self.get_method_signature(name, "", "",
                                                     &extra_params);
            buffer.push_str(&rendered.reindent("    ")
                            .newline_if_not_empty());
        }
        for (name, _) in sums_of_interfaces {
            let prefix = "Sum";
            if !self.refgraph.is_used(Rc::new(format!("{}{}", prefix, name))) {
                continue;
            }

            let rules_for_this_sum = self.rules.get(name);
            let extra_params = rules_for_this_sum.extra_params;
            let rendered = self.get_method_signature(name, prefix,
                                                     INTERFACE_PARAMS,
                                                     &extra_params);
            buffer.push_str(&rendered.reindent("    ")
                            .newline_if_not_empty());
        }
    }

    fn export_declare_single_interface_methods(&self, buffer: &mut String) {
        buffer.push_str("
    // ----- Interfaces (by lexicographical order)
    // `ParseNode*` may never be nullptr
");
        let interfaces_by_name = self.syntax.interfaces_by_name()
            .iter()
            .sorted_by(|a, b| str::cmp(a.0.to_str(), b.0.to_str()));

        let mut outer_parsers = Vec::with_capacity(interfaces_by_name.len());
        let mut inner_parsers = Vec::with_capacity(interfaces_by_name.len());

        for &(name, _) in &interfaces_by_name {
            let rules_for_this_interface = self.rules.get(name);
            let extra_params = rules_for_this_interface.extra_params;

            if self.refgraph.is_used(name.to_rc_string().clone()) {
                let outer = self.get_method_signature(name, "", "", &extra_params);
                outer_parsers.push(outer.reindent("    "));
            }

            let inner_prefix = "Interface";
            if !self.refgraph.is_used(Rc::new(format!("{}{}", inner_prefix, name))) {
                continue;
            }
            let inner = self.get_method_signature(name, inner_prefix,
                                                  INTERFACE_PARAMS,
                                                  &extra_params);
            inner_parsers.push(inner.reindent("    "));
        }

        for parser in outer_parsers.drain(..) {
            buffer.push_str(&parser);
            buffer.push_str("\n");
        }

        for parser in inner_parsers.drain(..) {
            buffer.push_str(&parser);
            buffer.push_str("\n");
        }
    }

    fn export_declare_string_enums_methods(&self, buffer: &mut String) {
        buffer.push_str("
    // ----- String enums (by lexicographical order)
");
        let string_enums_by_name = self.syntax.string_enums_by_name()
            .iter()
            .sorted_by(|a, b| str::cmp(a.0.to_str(), b.0.to_str()));
        for (kind, _) in string_enums_by_name {
            if !self.refgraph.is_used(kind.to_rc_string().clone()) {
                continue;
            }

            let rendered = self.get_method_signature(kind, "", "", &None);
            buffer.push_str(&rendered.reindent("    "));
            buffer.push_str("\n");
        }
    }

    fn export_declare_list_methods(&self, buffer: &mut String) {
        buffer.push_str("
    // ----- Lists (by lexicographical order)
");
        for parser in &self.list_parsers_to_generate {
            if !self.refgraph.is_used(parser.name.to_rc_string().clone()) {
                continue;
            }

            let rules_for_this_node = self.rules.get(&parser.name);
            let extra_params = rules_for_this_node.extra_params;
            let rendered = self.get_method_signature(&parser.name, "", "",
                                                     &extra_params);
            buffer.push_str(&rendered.reindent("    "));
            buffer.push_str("\n");
        }
    }

    fn export_declare_option_methods(&self, buffer: &mut String) {
        buffer.push_str("
    // ----- Default values (by lexicographical order)
");
        for parser in &self.option_parsers_to_generate {
            if !self.refgraph.is_used(parser.name.to_rc_string().clone()) {
                continue;
            }

            let rules_for_this_node = self.rules.get(&parser.name);
            let extra_params = rules_for_this_node.extra_params;
            let rendered = self.get_method_signature(&parser.name, "", "",
                                                     &extra_params);
            buffer.push_str(&rendered.reindent("    "));
            buffer.push_str("\n");
        }
    }

    fn generate_autogenerated_warning(&self) -> String {
        let warning = format!("// This file was autogenerated by binjs_generate_spidermonkey,
// please DO NOT EDIT BY HAND.
");
        warning
    }

    /// Generate C++ headers for SpiderMonkey
    fn to_spidermonkey_token_hpp(&self) -> String {
        let mut buffer = String::new();

        buffer.push_str(&self.generate_autogenerated_warning());

        self.export_declare_kinds_and_fields_enums(&mut buffer);

        buffer.push_str("\n");
        buffer
    }
    fn to_spidermonkey_class_hpp(&self) -> String {
        let mut buffer = String::new();

        buffer.push_str(&self.generate_autogenerated_warning());

        buffer.push_str(&self.rules.hpp_class_header.reindent(""));
        buffer.push_str("\n");

        self.export_declare_sums_of_interface_methods(&mut buffer);
        self.export_declare_single_interface_methods(&mut buffer);
        self.export_declare_string_enums_methods(&mut buffer);
        self.export_declare_list_methods(&mut buffer);
        self.export_declare_option_methods(&mut buffer);

        buffer.push_str("\n");
        buffer.push_str(&self.rules.hpp_class_footer.reindent(""));
        buffer.push_str("\n");
        buffer
    }

    fn to_spidermonkey_enum_hpp(&self) -> String {
        let mut buffer = String::new();

        buffer.push_str(&self.generate_autogenerated_warning());

        buffer.push_str(&self.rules.hpp_enums_header.reindent(""));
        buffer.push_str("\n");

        self.export_declare_string_enums(&mut buffer);

        buffer.push_str("\n");
        buffer.push_str(&self.rules.hpp_enums_footer.reindent(""));
        buffer.push_str("\n");
        buffer
    }
}

impl CPPExporter {
    /// Generate implementation of a single typesum.
    fn generate_implement_sum(&self, buffer: &mut String, name: &NodeName, nodes: &HashSet<NodeName>) {
        // Generate comments (FIXME: We should use the actual webidl, not the resolved sum)
        let rules_for_this_sum = self.rules.get(name);
        let extra_params = rules_for_this_sum.extra_params;
        let extra_args = rules_for_this_sum.extra_args;
        let nodes = nodes.iter()
            .sorted();
        let kind = name.to_class_cases();

        if self.refgraph.is_used(name.to_rc_string().clone()) {
            let rendered_bnf = format!("/*\n{name} ::= {nodes}\n*/",
                nodes = nodes.iter()
                    .format("\n    "),
                name = name.to_str());

            // Generate outer method
            buffer.push_str(&format!("{bnf}
{first_line}
{{
    BinKind kind;
    BinFields fields(cx_);
    AutoTaggedTuple guard(*tokenizer_);
    const auto start = tokenizer_->offset();

    MOZ_TRY(tokenizer_->enterTaggedTuple(kind, fields, guard));

{call}

    MOZ_TRY(guard.done());
    return result;
}}\n",
                bnf = rendered_bnf,
                call = self.get_method_call("result", name,
                                            "Sum", INTERFACE_ARGS,
                                            &extra_args,
                                            MethodCallKind::AlwaysDecl)
                    .reindent("    "),
                first_line = self.get_method_definition_start(name, "", "",
                                                              &extra_params)
            ));
        }

        let inner_prefix = "Sum";
        if !self.refgraph.is_used(Rc::new(format!("{}{}", inner_prefix, name))) {
            return;
        }

        // Generate inner method
        let mut buffer_cases = String::new();
        for node in nodes {
            let rule_for_this_arm = rules_for_this_sum.by_sum.get(&node)
                .cloned()
                .unwrap_or_default();

            if rule_for_this_arm.disabled {
                buffer_cases.push_str(&format!("
      case BinKind::{variant_name}:
        return raiseError(\"FIXME: Not implemented yet in this preview release ({variant_name})\");",
                    variant_name = node.to_cpp_enum_case()));
                continue;
            }

            buffer_cases.push_str(&format!("
      case BinKind::{variant_name}:
{call}
{arm_after}        break;",
                call = self.get_method_call("result", node,
                                            "Interface", INTERFACE_ARGS,
                                            &extra_args,
                                            MethodCallKind::AlwaysVar)
                    .reindent("        "),
                variant_name = node.to_cpp_enum_case(),
                arm_after = rule_for_this_arm.after_arm.reindent("        ")
                    .newline_if_not_empty()));
        }
        buffer.push_str(&format!("\n{first_line}
{{
    {type_ok} result;
    switch (kind) {{{cases}
      default:
        return raiseInvalidKind(\"{kind}\", kind);
    }}
    return result;
}}

",
            kind = kind,
            cases = buffer_cases,
            first_line = self.get_method_definition_start(name, inner_prefix,
                                                          INTERFACE_PARAMS,
                                                          &extra_params),
            type_ok = self.get_type_ok(name)
        ));
    }

    /// Generate the implementation of a single list parser
    fn generate_implement_list(&self, buffer: &mut String, parser: &ListParserData) {
        if !self.refgraph.is_used(parser.name.to_rc_string().clone()) {
            return;
        }

        let rules_for_this_list = self.rules.get(&parser.name);
        let extra_params = rules_for_this_list.extra_params;
        let extra_args = rules_for_this_list.extra_args;

        // Warn if some rules are unused.
        for &(condition, name) in &[
            (rules_for_this_list.build_result.is_some(), "build:"),
            (rules_for_this_list.type_ok.is_some(), "type-ok:"),
            (rules_for_this_list.by_field.len() > 0, "fields:"),
            (rules_for_this_list.by_sum.len() > 0, "sum-arms:"),
        ] {
            if condition {
                warn!("In {}, rule `{}` was specified but is ignored.", parser.name, name);
            }
        }

        let kind = parser.name.to_class_cases();
        let first_line = self.get_method_definition_start(&parser.name, "", "",
                                                          &extra_params);

        let init = match rules_for_this_list.init {
            Some(str) => str.reindent("    "),
            None => {
                // We cannot generate the method if we don't know how to initialize the list.
                let rendered = format!("
{first_line}
{{
    return raiseError(\"FIXME: Not implemented yet in this preview release ({kind})\");
}}\n",
                    first_line = first_line,
                    kind = kind,
                );
                buffer.push_str(&rendered);
                return;
            }
        };
        let append = match rules_for_this_list.append {
            Some(str) => str.reindent("        ").newline_if_not_empty(),
            None => {
                match self.rules.parser_list_append {
                    Some(ref str) => str.reindent("        ").newline_if_not_empty(),
                    None => "".to_string(),
                }
            }
        };


        let rendered = format!("\n{first_line}
{{
    uint32_t length;
    AutoList guard(*tokenizer_);

    const auto start = tokenizer_->offset();
    MOZ_TRY(tokenizer_->enterList(length, guard));{empty_check}
{init}

    for (uint32_t i = 0; i < length; ++i) {{
{call}
{append}    }}

    MOZ_TRY(guard.done());
    return result;
}}\n",
            first_line = first_line,
            empty_check =
                if parser.supports_empty {
                    "".to_string()
                } else {
                    format!("
    if (length == 0) {{
        return raiseEmpty(\"{kind}\");
    }}
",
                        kind = kind)
                },
            call = self.get_method_call("item",
                                        &parser.elements, "", "",
                                        &extra_args,
                                        MethodCallKind::Decl)
                .reindent("        "),
            init = init,
            append = append);
        buffer.push_str(&rendered);
    }

    fn generate_implement_option(&self, buffer: &mut String, parser: &OptionParserData) {
        debug!(target: "generate_spidermonkey", "Implementing optional value {} backed by {}",
            parser.name.to_str(), parser.elements.to_str());

        if !self.refgraph.is_used(parser.name.to_rc_string().clone()) {
            return;
        }

        let rules_for_this_node = self.rules.get(&parser.name);
        let extra_params = rules_for_this_node.extra_params;
        let extra_args = rules_for_this_node.extra_args;

        // Warn if some rules are unused.
        for &(condition, name) in &[
            (rules_for_this_node.build_result.is_some(), "build:"),
            (rules_for_this_node.append.is_some(), "append:"),
            (rules_for_this_node.by_field.len() > 0, "fields:"),
            (rules_for_this_node.by_sum.len() > 0, "sum-arms:"),
        ] {
            if condition {
                warn!("In {}, rule `{}` was specified but is ignored.", parser.name, name);
            }
        }

        let type_ok = self.get_type_ok(&parser.name);
        let default_value = self.get_default_value(&parser.name);

        // At this stage, thanks to deanonymization, `contents`
        // is something like `OptionalFooBar`.
        let named_implementation =
            if let Some(NamedType::Typedef(ref typedef)) = self.syntax.get_type_by_name(&parser.name) {
                assert!(typedef.is_optional());
                if let TypeSpec::NamedType(ref named) = *typedef.spec() {
                    self.syntax.get_type_by_name(named)
                        .unwrap_or_else(|| panic!("Internal error: Could not find type {}, which should have been generated.", named.to_str()))
                } else {
                    panic!("Internal error: In {}, type {:?} should have been a named type",
                        parser.name.to_str(),
                        typedef);
                }
            } else {
                panic!("Internal error: In {}, there should be a type with that name",
                    parser.name.to_str());
            };
        match named_implementation {
            NamedType::Interface(_) => {
                buffer.push_str(&format!("{first_line}
{{
    BinKind kind;
    BinFields fields(cx_);
    AutoTaggedTuple guard(*tokenizer_);

    MOZ_TRY(tokenizer_->enterTaggedTuple(kind, fields, guard));
    {type_ok} result;
    if (kind == BinKind::{null}) {{
{none_block}
    }} else if (kind == BinKind::{kind}) {{
        const auto start = tokenizer_->offset();
{before}{call}{after}
    }} else {{
        return raiseInvalidKind(\"{kind}\", kind);
    }}
    MOZ_TRY(guard.done());

    return result;
}}

",
                    first_line = self.get_method_definition_start(&parser.name, "", "",
                                                                  &extra_params),
                    null = self.syntax.get_null_name().to_cpp_enum_case(),
                    call = self.get_method_call("result",
                                                &parser.elements,
                                                "Interface", INTERFACE_ARGS,
                                                &extra_args,
                                                MethodCallKind::AlwaysVar)
                        .reindent("        "),
                    before = rules_for_this_node.some_before
                        .map_or_else(|| "".to_string(),
                                     |s| s
                                     .reindent("        ")
                                     .newline_if_not_empty()),
                    after = rules_for_this_node.some_after
                        .map_or_else(|| "".to_string(),
                                     |s| s
                                     .reindent("        ")
                                     .newline_if_not_empty()),
                    none_block = rules_for_this_node.none_replace
                        .map_or_else(|| format!("result = {default_value};",
                                                default_value = default_value)
                                            .reindent("        "),
                                     |s| s.reindent("        ")),
                    type_ok = type_ok,
                    kind = parser.elements.to_cpp_enum_case(),
                ));
            }
            NamedType::Typedef(ref type_) => {
                match type_.spec() {
                    &TypeSpec::TypeSum(_) => {
                buffer.push_str(&format!("{first_line}
{{
    BinKind kind;
    BinFields fields(cx_);
    AutoTaggedTuple guard(*tokenizer_);

    MOZ_TRY(tokenizer_->enterTaggedTuple(kind, fields, guard));
    {type_ok} result;
    if (kind == BinKind::{null}) {{
{none_block}
    }} else {{
        const auto start = tokenizer_->offset();
{before}{call}{after}
    }}
    MOZ_TRY(guard.done());

    return result;
}}

",
                            first_line = self.get_method_definition_start(&parser.name, "", "",
                                                                          &extra_params),
                            call = self.get_method_call("result", &parser.elements,
                                                        "Sum", INTERFACE_ARGS,
                                                        &extra_args,
                                                        MethodCallKind::AlwaysVar)
                                .reindent("        "),
                            before = rules_for_this_node.some_before
                                .map_or_else(|| "".to_string(),
                                             |s| s
                                             .reindent("        ")
                                             .newline_if_not_empty()),
                            after = rules_for_this_node.some_after
                                .map_or_else(|| "".to_string(),
                                             |s| s
                                             .reindent("        ")
                                             .newline_if_not_empty()),
                            none_block = rules_for_this_node.none_replace
                                .map_or_else(|| format!("result = {default_value};",
                                                        default_value = default_value)
                                                    .reindent("        "),
                                             |s| s.reindent("        ")),
                            type_ok = type_ok,
                            null = self.syntax.get_null_name().to_cpp_enum_case(),
                        ));
                    }
                    &TypeSpec::String => {
                        let build_result = rules_for_this_node.init.reindent("    ");
                        let first_line = self.get_method_definition_start(&parser.name, "", "",
                                                                          &extra_params);
                        if build_result.len() == 0 {
                            buffer.push_str(&format!("{first_line}
{{
    return raiseError(\"FIXME: Not implemented yet in this preview release ({kind})\");
}}

",
                                first_line = first_line,
                                kind = parser.name.to_str()));
                        } else {
                            buffer.push_str(&format!("{first_line}
{{
    BINJS_MOZ_TRY_DECL(result, tokenizer_->readMaybeAtom());

{build}

    return result;
}}

",
                                first_line = first_line,
                                build = build_result,
                            ));
                        }
                    }
                    &TypeSpec::IdentifierName => {
                        let build_result = rules_for_this_node.init.reindent("    ");
                        let first_line = self.get_method_definition_start(&parser.name, "", "",
                                                                          &extra_params);
                        if build_result.len() == 0 {
                            buffer.push_str(&format!("{first_line}
{{
    return raiseError(\"FIXME: Not implemented yet in this preview release ({kind})\");
}}

",
                                first_line = first_line,
                                kind = parser.name.to_str()));
                        } else {
                            buffer.push_str(&format!("{first_line}
{{
    BINJS_MOZ_TRY_DECL(result, tokenizer_->readMaybeIdentifierName());

{build}

    return result;
}}

",
                                first_line = first_line,
                                build = build_result,
                            ));
                        }
                    }
                    &TypeSpec::PropertyKey => {
                        debug!(target: "generate_spidermonkey", "Generating method for PropertyKey: {:?}", parser.name);
                        let build_result = rules_for_this_node.init.reindent("    ");
                        let first_line = self.get_method_definition_start(&parser.name, "", "",
                                                                          &extra_params);
                        if build_result.len() == 0 {
                            buffer.push_str(&format!("{first_line}
{{
    return raiseError(\"FIXME: Not implemented yet in this preview release ({kind})\");
}}

",
                                first_line = first_line,
                                kind = parser.name.to_str()));
                        } else {
                            buffer.push_str(&format!("{first_line}
{{
    BINJS_MOZ_TRY_DECL(result, tokenizer_->readMaybePropertyKey());

{build}

    return result;
}}

",
                                first_line = first_line,
                                build = build_result,
                            ));
                        }
                    }
                    _else => unimplemented!("{:?}", _else)
                }
            }
            NamedType::StringEnum(_) => {
                unimplemented!()
            }
        }
    }

    fn generate_implement_interface(&self, buffer: &mut String, name: &NodeName, interface: &Interface) {
        let rules_for_this_interface = self.rules.get(name);
        let extra_params = rules_for_this_interface.extra_params;
        let extra_args = rules_for_this_interface.extra_args;

        for &(condition, rule_name) in &[
            (rules_for_this_interface.append.is_some(), "build:"),
        ] {
            if condition {
                warn!("In {}, rule `{}` was specified but is ignored.", name, rule_name);
            }
        }

        if self.refgraph.is_used(name.to_rc_string().clone()) {
            // Generate comments
            let comment = format!("\n/*\n{}*/\n", ToWebidl::interface(interface, "", "    "));
            buffer.push_str(&comment);

            // Generate public method
            buffer.push_str(&format!("{first_line}
{{
    BinKind kind;
    BinFields fields(cx_);
    AutoTaggedTuple guard(*tokenizer_);

    MOZ_TRY(tokenizer_->enterTaggedTuple(kind, fields, guard));
    if (kind != BinKind::{kind}) {{
        return raiseInvalidKind(\"{kind}\", kind);
    }}
    const auto start = tokenizer_->offset();
{call}
    MOZ_TRY(guard.done());

    return result;
}}

",
                first_line = self.get_method_definition_start(name, "", "",
                                                              &extra_params),
                kind = name.to_cpp_enum_case(),
                call = self.get_method_call("result", name,
                                            "Interface", INTERFACE_ARGS,
                                            &extra_args,
                                            MethodCallKind::AlwaysDecl)
                    .reindent("    ")
            ));
        }

        let inner_prefix = "Interface";
        if !self.refgraph.is_used(Rc::new(format!("{}{}", inner_prefix, name))) {
            return;
        }

        // Generate aux method
        let number_of_fields = interface.contents().fields().len();
        let first_line = self.get_method_definition_start(name, inner_prefix,
                                                          INTERFACE_PARAMS,
                                                          &extra_params);

        let fields_type_list = format!("{{ {} }}", interface.contents()
            .fields()
            .iter()
            .map(|field| format!("BinField::{}", field.name().to_cpp_enum_case()))
            .format(", "));

        let mut fields_implem = String::new();
        for field in interface.contents().fields() {
            let rules_for_this_field = rules_for_this_interface.by_field.get(field.name())
                .cloned()
                .unwrap_or_default();
            let needs_block = rules_for_this_field.block_before_field.is_some() || rules_for_this_field.block_after_field.is_some();

            let var_name = field.name().to_cpp_field_case();
            let (decl_var, parse_var) = match field.type_().get_primitive(&self.syntax) {
                Some(IsNullable { is_nullable: false, content: Primitive::Number }) => {
                    if needs_block {
                        (Some(format!("double {var_name};", var_name = var_name)),
                            Some(format!("MOZ_TRY_VAR({var_name}, tokenizer_->readDouble());", var_name = var_name)))
                    } else {
                        (None,
                            Some(format!("BINJS_MOZ_TRY_DECL({var_name}, tokenizer_->readDouble());", var_name = var_name)))
                    }
                }
                Some(IsNullable { is_nullable: false, content: Primitive::UnsignedLong }) => {
                    if needs_block {
                        (Some(format!("uint32_t {var_name};", var_name = var_name)),
                            Some(format!("MOZ_TRY_VAR({var_name}, tokenizer_->readUnsignedLong());", var_name = var_name)))
                    } else {
                        (None,
                            Some(format!("BINJS_MOZ_TRY_DECL({var_name}, tokenizer_->readUnsignedLong());", var_name = var_name)))
                    }
                }
                Some(IsNullable { is_nullable: false, content: Primitive::Boolean }) => {
                    if needs_block {
                        (Some(format!("bool {var_name};", var_name = var_name)),
                        Some(format!("MOZ_TRY_VAR({var_name}, tokenizer_->readBool());", var_name = var_name)))
                    } else {
                        (None,
                        Some(format!("BINJS_MOZ_TRY_DECL({var_name}, tokenizer_->readBool());", var_name = var_name)))
                    }
                }
                Some(IsNullable { is_nullable: false, content: Primitive::Offset }) => {
                    if needs_block {
                        (Some(format!("BinTokenReaderBase::SkippableSubTree {var_name};", var_name = var_name)),
                        Some(format!("MOZ_TRY_VAR({var_name}, tokenizer_->readSkippableSubTree());", var_name = var_name)))
                    } else {
                        (None,
                        Some(format!("BINJS_MOZ_TRY_DECL({var_name}, tokenizer_->readSkippableSubTree());", var_name = var_name)))
                    }
                }
                Some(IsNullable { content: Primitive::Void, .. }) => {
                    warn!("Internal error: We shouldn't have any `void` types at this stage.");
                    (Some(format!("// Skipping void field {}", field.name().to_str())),
                        None)
                }
                Some(IsNullable { is_nullable: false, content: Primitive::String }) => {
                    (Some(format!("RootedAtom {var_name}(cx_);", var_name = var_name)),
                        Some(format!("MOZ_TRY_VAR({var_name}, tokenizer_->readAtom());", var_name = var_name)))
                }
                Some(IsNullable { is_nullable: false, content: Primitive::IdentifierName }) => {
                    (Some(format!("RootedAtom {var_name}(cx_);", var_name = var_name)),
                        Some(format!("MOZ_TRY_VAR({var_name}, tokenizer_->readIdentifierName());", var_name = var_name)))
                }
                Some(IsNullable { is_nullable: false, content: Primitive::PropertyKey }) => {
                    (Some(format!("RootedAtom {var_name}(cx_);", var_name = var_name)),
                        Some(format!("MOZ_TRY_VAR({var_name}, tokenizer_->readPropertyKey());", var_name = var_name)))
                }
                Some(IsNullable { is_nullable: true, content: Primitive::String }) => {
                    (Some(format!("RootedAtom {var_name}(cx_);", var_name = var_name)),
                        Some(format!("MOZ_TRY_VAR({var_name}, tokenizer_->readMaybeAtom());", var_name = var_name)))
                }
                Some(IsNullable { is_nullable: true, content: Primitive::IdentifierName }) => {
                    (Some(format!("RootedAtom {var_name}(cx_);", var_name = var_name)),
                        Some(format!("MOZ_TRY_VAR({var_name}, tokenizer_->readMaybeIdentifierName());", var_name = var_name)))
                }
                Some(IsNullable { is_nullable: true, content: Primitive::PropertyKey }) => {
                    (Some(format!("RootedAtom {var_name}(cx_);", var_name = var_name)),
                        Some(format!("MOZ_TRY_VAR({var_name}, tokenizer_->readMaybePropertyKey());", var_name = var_name)))
                }
                _else => {
                    let typename = TypeName::type_(field.type_());
                    let name = self.syntax.get_node_name(typename.to_str())
                        .expect("NodeName for the field type should exist.");
                    let field_extra_args = rules_for_this_field.extra_args;

                    let (decl_var, call_kind) = if needs_block {
                        (Some(format!("{typename} {var_name};",
                                      var_name = var_name,
                                      typename = typename)),
                         MethodCallKind::Var)
                    } else {
                        (None,
                         MethodCallKind::Decl)
                    };

                    (decl_var,
                     Some(self.get_method_call(var_name.to_str(),
                                               &name, "", "", &field_extra_args,
                                               call_kind)))
                }
            };
            let rendered = {
                if rules_for_this_field.replace.is_some() {
                    for &(condition, rule_name) in &[
                        (rules_for_this_field.before_field.is_some(), "before:"),
                        (rules_for_this_field.after_field.is_some(), "after:"),
                        (rules_for_this_field.declare.is_some(), "declare:"),
                    ] {
                        if condition {
                            warn!("In {}, rule `{}` was specified but is ignored because `replace:` is also specified.", name, rule_name);
                        }
                    }
                    rules_for_this_field.replace.reindent("    ")
                        .newline()
                } else {
                    let before_field = rules_for_this_field.before_field.reindent("    ");
                    let after_field = rules_for_this_field.after_field.reindent("    ");
                    let decl_var = if rules_for_this_field.declare.is_some() {
                        rules_for_this_field.declare.reindent("    ")
                    } else {
                        decl_var.reindent("    ")
                    };
                    if needs_block {
                        let parse_var = parse_var.reindent("        ");
                        format!("{before_field}{decl_var}    {{
{block_before_field}{parse_var}{block_after_field}
    }}
{after_field}",
                            before_field = before_field.reindent("    ").newline_if_not_empty(),
                            decl_var = decl_var.reindent("    ").newline_if_not_empty(),
                            block_before_field = rules_for_this_field.block_before_field.reindent("        ").newline_if_not_empty(),
                            parse_var = parse_var.reindent("        ").newline_if_not_empty(),
                            block_after_field = rules_for_this_field.block_after_field.reindent("        "),
                            after_field = after_field.reindent("    "))
                    } else {
                        // We have a before_field and an after_field. This will create newlines
                        // for them.
                        format!("
{before_field}{decl_var}{parse_var}{after_field}",
                            before_field = before_field.reindent("    ").newline_if_not_empty(),
                            decl_var = decl_var.reindent("    ").newline_if_not_empty(),
                            parse_var = parse_var.reindent("    ").newline_if_not_empty(),
                            after_field = after_field.reindent("    "))
                    }
                }
            };
            fields_implem.push_str(&rendered);
        }

        let init = rules_for_this_interface.init.reindent("    ");
        let build_result = rules_for_this_interface.build_result.reindent("    ");

        if build_result == "" {
            buffer.push_str(&format!("{first_line}
{{
    return raiseError(\"FIXME: Not implemented yet in this preview release ({class_name})\");
}}

",
                class_name = name.to_class_cases(),
                first_line = first_line,
            ));
        } else {
            let check_fields = if number_of_fields == 0 {
                format!("MOZ_TRY(tokenizer_->checkFields0(kind, fields));")
            } else {
                // The following strategy is designed for old versions of clang.
                format!("
#if defined(DEBUG)
    const BinField expected_fields[{number_of_fields}] = {fields_type_list};
    MOZ_TRY(tokenizer_->checkFields(kind, fields, expected_fields));
#endif // defined(DEBUG)",
                    fields_type_list = fields_type_list,
                    number_of_fields = number_of_fields)
            };
            buffer.push_str(&format!("{first_line}
{{
    MOZ_ASSERT(kind == BinKind::{kind});
    BINJS_TRY(CheckRecursionLimit(cx_));
{check_fields}
{pre}{fields_implem}
{post}    return result;
}}

",
                check_fields = check_fields,
                fields_implem = fields_implem,
                pre = init.newline_if_not_empty(),
                post = build_result.newline_if_not_empty(),
                kind = name.to_cpp_enum_case(),
                first_line = first_line,
            ));
        }
    }

    /// Generate C++ code for SpiderMonkey
    fn to_spidermonkey_cpp(&self) -> String {
        let mut buffer = String::new();

        buffer.push_str(&self.generate_autogenerated_warning());

        // 0. Header
        buffer.push_str(&self.rules.cpp_header.reindent(""));
        buffer.push_str("\n");

        // 1. Typesums
        buffer.push_str("\n\n// ----- Sums of interfaces (autogenerated, by lexicographical order)\n");
        buffer.push_str("// Sums of sums are flattened.\n");

        let sums_of_interfaces = self.syntax.resolved_sums_of_interfaces_by_name()
            .iter()
            .sorted_by(|a, b| a.0.cmp(&b.0));

        for (name, nodes) in sums_of_interfaces {
            self.generate_implement_sum(&mut buffer, name, nodes);
        }

        // 2. Single interfaces
        buffer.push_str("\n\n// ----- Interfaces (autogenerated, by lexicographical order)\n");
        buffer.push_str("// When fields have a non-trivial type, implementation is deanonymized and delegated to another parser.\n");
        let interfaces_by_name = self.syntax.interfaces_by_name()
            .iter()
            .sorted_by(|a, b| str::cmp(a.0.to_str(), b.0.to_str()));

        for (name, interface) in interfaces_by_name {
            self.generate_implement_interface(&mut buffer, name, interface);
        }

        // 3. String Enums
        buffer.push_str("\n\n// ----- String enums (autogenerated, by lexicographical order)\n");
        {
            let string_enums_by_name = self.syntax.string_enums_by_name()
                .iter()
                .sorted_by(|a, b| str::cmp(a.0.to_str(), b.0.to_str()));
            for (kind, enum_) in string_enums_by_name {
                if !self.refgraph.is_used(kind.to_rc_string().clone()) {
                    continue;
                }

                let convert = format!("    switch (variant) {{
{cases}
      default:
        return raiseInvalidVariant(\"{kind}\", variant);
    }}",
                    kind = kind,
                    cases = enum_.strings()
                        .iter()
                        .map(|symbol| {
                            format!("    case BinVariant::{binvariant_variant}:
        return {kind}::{specialized_variant};",
                                kind = kind,
                                specialized_variant = symbol.to_cpp_enum_case(),
                                binvariant_variant  = self.variants_by_symbol.get(symbol)
                                    .unwrap()
                            )
                        })
                        .format("\n")
                );

                let rendered_doc = format!("/*\nenum {kind} {{\n{cases}\n}};\n*/\n",
                    kind = kind,
                    cases = enum_.strings()
                            .iter()
                            .map(|s| format!("    \"{}\"", s))
                            .format(",\n")
                );
                buffer.push_str(&format!("{rendered_doc}{first_line}
{{
    BINJS_MOZ_TRY_DECL(variant, tokenizer_->readVariant());

{convert}
}}

",
                    rendered_doc = rendered_doc,
                    convert = convert,
                    first_line = self.get_method_definition_start(kind, "", "",
                                                                  &None)
                ));
            }
        }

        // 4. Lists
        buffer.push_str("\n\n// ----- Lists (autogenerated, by lexicographical order)\n");
        for parser in &self.list_parsers_to_generate {
            self.generate_implement_list(&mut buffer, parser);
        }

        // 5. Optional values
        buffer.push_str("\n\n    // ----- Default values (by lexicographical order)\n");
        for parser in &self.option_parsers_to_generate {
            self.generate_implement_option(&mut buffer, parser);
        }

        buffer.push_str("\n");
        buffer.push_str(&self.rules.cpp_footer.reindent(""));
        buffer.push_str("\n");

        buffer
    }
}

fn update_rule(rule: &mut Option<String>, entry: &yaml_rust::Yaml) -> Result<Option<()>, ()> {
    if entry.is_badvalue() {
        return Ok(None)
    } else if let Some(as_str) = entry.as_str() {
        *rule = Some(as_str.to_string());
        Ok(Some(()))
    } else {
        Err(())
    }
}
fn update_rule_rc(rule: &mut Option<Rc<String>>, entry: &yaml_rust::Yaml) -> Result<Option<()>, ()> {
    let mut value = None;
    let ret = update_rule(&mut value, entry)?;
    if let Some(s) = value {
        *rule = Some(Rc::new(s));
    }
    Ok(ret)
}

fn main() {
    env_logger::init();

    let matches = App::new("BinAST C++ parser generator")
        .author("David Teller, <dteller@mozilla.com>")
        .about("Converts an webidl syntax definition and a yaml set of rules into the C++ source code of a parser.")
        .args(&[
            Arg::with_name("INPUT.webidl")
                .required(true)
                .help("Input webidl file to use. Must be a webidl source file."),
            Arg::with_name("INPUT.yaml")
                .required(true)
                .help("Input rules file to use. Must be a yaml source file."),
            Arg::with_name("OUT_HEADER_CLASS_FILE")
                .long("out-class")
                .required(true)
                .takes_value(true)
                .help("Output header file for class (.h)"),
            Arg::with_name("OUT_HEADER_ENUM_FILE")
                .long("out-enum")
                .required(true)
                .takes_value(true)
                .help("Output header file for enum (.h)"),
            Arg::with_name("OUT_TOKEN_FILE")
                .long("out-token")
                .required(true)
                .takes_value(true)
                .help("Output token file (.h)"),
            Arg::with_name("OUT_IMPL_FILE")
                .long("out-impl")
                .required(true)
                .takes_value(true)
                .help("Output implementation file (.cpp)"),
        ])
    .get_matches();

    let source_path = matches.value_of("INPUT.webidl")
        .expect("Expected INPUT.webidl");

    let mut file = File::open(source_path)
        .expect("Could not open source");
    let mut source = String::new();
    file.read_to_string(&mut source)
        .expect("Could not read source");

    println!("...parsing webidl");
    let ast = webidl::parse_string(&source)
        .expect("Could not parse source");

    println!("...verifying grammar");
    let mut builder = Importer::import(&ast);
    let fake_root = builder.node_name("@@ROOT@@"); // Unused
    let null = builder.node_name(""); // Used
    builder.add_interface(&null)
        .unwrap();
    let syntax = builder.into_spec(SpecOptions {
        root: &fake_root,
        null: &null,
    });

    let deanonymizer = TypeDeanonymizer::new(&syntax);
    let syntax_options = SpecOptions {
        root: &fake_root,
        null: &null,
    };
    let new_syntax = deanonymizer.into_spec(syntax_options);

    let rules_source_path = matches.value_of("INPUT.yaml").unwrap();
    println!("...generating rules");
    let mut file = File::open(rules_source_path)
        .expect("Could not open rules");
    let mut data = String::new();
    file.read_to_string(&mut data)
        .expect("Could not read rules");

    let yaml = yaml_rust::YamlLoader::load_from_str(&data)
        .expect("Could not parse rules");
    assert_eq!(yaml.len(), 1);

    let global_rules = GlobalRules::new(&new_syntax, &yaml[0]);
    let mut exporter = CPPExporter::new(new_syntax, global_rules);

    exporter.generate_reference_graph();
    exporter.trace(Rc::new(TOPLEVEL_INTERFACE.to_string()));

    let get_file_content = |path: &str| {
        if !Path::new(path).is_file() {
            return None;
        }

        let mut f = File::open(path)
            .expect("File not found");
        let mut contents = String::new();
        f.read_to_string(&mut contents)
            .expect("Failed to read file");
        Some(contents)
    };
    let write_to = |description, arg, data: &String| {
        let dest_path = matches.value_of(arg)
            .unwrap();
        print!("...exporting {description}: {path} ... ",
            description = description,
            path = dest_path);

        if let Some(old_data) = get_file_content(dest_path) {
            if old_data == *data {
                // To avoid unnecessary rebuild, do not touch the file if the
                // content is not updated.
                println!("skip");
                return;
            }
        };

        let mut dest = File::create(&dest_path)
            .unwrap_or_else(|e| panic!("Could not create {description} at {path}: {error}",
                            description = description,
                            path = dest_path,
                            error = e));
        dest.write_all(data.as_bytes())
            .unwrap_or_else(|e| panic!("Could not write {description} at {path}: {error}",
                            description = description,
                            path = dest_path,
                            error = e));

        println!("done");
    };

    write_to("C++ class header code", "OUT_HEADER_CLASS_FILE",
        &exporter.to_spidermonkey_class_hpp());
    write_to("C++ enum header code", "OUT_HEADER_ENUM_FILE",
        &exporter.to_spidermonkey_enum_hpp());
    write_to("C++ token header code", "OUT_TOKEN_FILE",
        &exporter.to_spidermonkey_token_hpp());
    write_to("C++ token implementation code", "OUT_IMPL_FILE",
        &exporter.to_spidermonkey_cpp());

    println!("...done");
}
