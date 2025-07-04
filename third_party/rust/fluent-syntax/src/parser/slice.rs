use std::ops::Range;

pub(crate) fn matches_fluent_ws(c: char) -> bool {
    c == ' ' || c == '\r' || c == '\n'
}

pub trait Slice<'s>: AsRef<str> + Clone + PartialEq {
    fn slice(&self, range: Range<usize>) -> Self;
    fn trim(&mut self);
}

impl Slice<'_> for String {
    fn slice(&self, range: Range<usize>) -> Self {
        self[range].to_string()
    }

    fn trim(&mut self) {
        *self = self.trim_end_matches(matches_fluent_ws).to_string();
    }
}

impl<'s> Slice<'s> for &'s str {
    fn slice(&self, range: Range<usize>) -> Self {
        &self[range]
    }

    fn trim(&mut self) {
        *self = self.trim_end_matches(matches_fluent_ws);
    }
}
