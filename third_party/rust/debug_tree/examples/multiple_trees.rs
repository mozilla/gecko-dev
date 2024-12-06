use debug_tree::*;

fn populate(tree_name: &str, n_children: usize) {
    add_branch_to!(tree_name, "{} TREE", tree_name);
    for _ in 0..n_children {
        populate(tree_name, n_children / 2);
    }
}
fn main() {
    // Override tree config (just for "B")
    let b_tree = tree("B");
    b_tree.set_config_override(
        TreeConfig::new()
            .indent(4)
            .symbols(TreeSymbols::with_rounded().leaf("> ")),
    );
    defer_write!(b_tree, "examples/out/multiple_trees_B.txt");
    defer_write!("A", "examples/out/multiple_trees_A.txt");

    populate("A", 2);
    populate("B", 3);
}
