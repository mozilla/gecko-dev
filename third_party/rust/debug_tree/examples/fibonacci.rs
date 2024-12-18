use debug_tree::*;

fn factors(x: usize) {
    add_branch!("{}", x); // <~ THE MAGIC LINE
    for i in 1..x {
        if x % i == 0 {
            factors(i);
        }
    }
}

fn main() {
    // output to file at the end of this block
    defer_write!("examples/out/fibonacci.txt");
    add_branch!("A Fibonacci Tree");
    factors(6);
    add_leaf!("That's All Folks!");
}
