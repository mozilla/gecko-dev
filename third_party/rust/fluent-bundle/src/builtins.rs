use crate::{FluentArgs, FluentValue};

#[allow(non_snake_case)]
pub fn NUMBER<'a>(positional: &[FluentValue<'a>], named: &FluentArgs) -> FluentValue<'a> {
    let Some(FluentValue::Number(n)) = positional.first() else {
        return FluentValue::Error;
    };

    let mut n = n.clone();
    n.options.merge(named);

    FluentValue::Number(n)
}
