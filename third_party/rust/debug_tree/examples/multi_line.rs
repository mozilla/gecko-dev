use debug_tree::*;
fn main() {
    // output to file at the end of this block
    defer_write!("examples/out/multi_line.txt");
    add_branch!("1");
    add_leaf!("1.1\nAnother line...\n... and one more line");
    add_leaf!("1.2");
}
