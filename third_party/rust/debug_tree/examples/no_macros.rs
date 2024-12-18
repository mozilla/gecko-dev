use debug_tree::TreeBuilder;

fn main() {
    // Make a new tree.
    let tree = TreeBuilder::new();

    // Add a scoped branch. The next item added will belong to the branch.
    let mut branch = tree.add_branch("1 Branch");

    // Add a leaf to the current branch
    tree.add_leaf("1.1 Child");

    // Leave scope early
    branch.release();
    tree.add_leaf("2 Sibling");
    // output to file
    tree.write("examples/out/no_macros.txt").ok(); // Write and flush.
}
