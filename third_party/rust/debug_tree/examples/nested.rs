use debug_tree::*;
fn a() {
    add_branch!("a");
    b();
    c();
}
fn b() {
    add_branch!("b");
    c();
}
fn c() {
    add_branch!("c");
    add_leaf!("Nothing to see here");
}

fn main() {
    defer_write!("examples/out/nested.txt");
    a();
}
