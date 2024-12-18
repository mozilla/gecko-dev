use debug_tree::*;

fn i_will_panic() {
    add_branch!("Here are my last words");
    add_leaf!("Stay calm, and try not to panic");
    panic!("I told you so...")
}

fn main() {
    // output to file at the end of this block
    defer_write!("examples/out/panic.txt");
    // print at the end of this block
    {
        add_branch!("By using the 'defer_' functions");
        add_branch!("Output will still be generated");
        add_branch!("Otherwise you might lose your valuable tree!");
    }
    add_branch!("Now for something crazy...");
    i_will_panic();
}
