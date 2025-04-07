use std::convert::Infallible;
use std::fmt;

use humansize::{DECIMAL, ISizeFormatter, ToF64};

/// Returns adequate string representation (in KB, ..) of number of bytes
///
/// ## Example
/// ```
/// # use rinja::Template;
/// #[derive(Template)]
/// #[template(
///     source = "Filesize: {{ size_in_bytes|filesizeformat }}.",
///     ext = "html"
/// )]
/// struct Example {
///     size_in_bytes: u64,
/// }
///
/// let tmpl = Example { size_in_bytes: 1_234_567 };
/// assert_eq!(tmpl.to_string(),  "Filesize: 1.23 MB.");
/// ```
#[inline]
pub fn filesizeformat(b: &impl ToF64) -> Result<FilesizeFormatFilter, Infallible> {
    Ok(FilesizeFormatFilter(b.to_f64()))
}

#[derive(Debug, Clone, Copy)]
pub struct FilesizeFormatFilter(f64);

impl fmt::Display for FilesizeFormatFilter {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_fmt(format_args!("{}", ISizeFormatter::new(self.0, &DECIMAL)))
    }
}

#[test]
fn test_filesizeformat() {
    assert_eq!(filesizeformat(&0).unwrap().to_string(), "0 B");
    assert_eq!(filesizeformat(&999u64).unwrap().to_string(), "999 B");
    assert_eq!(filesizeformat(&1000i32).unwrap().to_string(), "1 kB");
    assert_eq!(filesizeformat(&1023).unwrap().to_string(), "1.02 kB");
    assert_eq!(filesizeformat(&1024usize).unwrap().to_string(), "1.02 kB");
}
