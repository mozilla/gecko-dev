#[cfg(test)]
mod test {
    use crate::*;
    use futures::future::join5;
    use std::cmp::{max, min};
    use std::fs::{create_dir, read_to_string, remove_file};

    #[test]
    fn test_branch() {
        let d: TreeBuilder = TreeBuilder::new();
        d.add_leaf("1");
        {
            let _l = d.enter_scoped();
            d.add_leaf("1.1");
            d.add_leaf("1.2");
        }
        d.add_leaf("2");
        d.add_leaf("3");
        let _l = d.enter_scoped();
        d.add_leaf("3.1");
        d.add_leaf("3.2");
        d.peek_print();
        assert_eq!(
            "\
1
├╼ 1.1
└╼ 1.2
2
3
├╼ 3.1
└╼ 3.2",
            d.string()
        );
    }

    #[test]
    fn test_branch2() {
        let d = TreeBuilder::new();
        d.add_leaf("1");
        {
            let _scope = d.enter_scoped();
            d.add_leaf("1.1");
            {
                let _scope = d.enter_scoped();
                d.add_leaf("1.1.1");
            }
        }

        d.add_leaf("2");
        d.enter();
        d.add_leaf("2.1");
        d.enter();
        d.add_leaf("2.1.1");
        d.peek_print();
        assert_eq!(
            "\
1
└╼ 1.1
  └╼ 1.1.1
2
└╼ 2.1
  └╼ 2.1.1",
            d.string()
        );
    }

    #[test]
    fn simple() {
        let d = TreeBuilder::new();
        d.add_leaf("Hi");
        assert_eq!("Hi", d.string());
    }

    #[test]
    fn depth() {
        let d = TreeBuilder::new();
        assert_eq!(0, d.depth());
        d.add_leaf("Hi");
        assert_eq!(0, d.depth());
        let _b = d.add_branch("Hi");
        assert_eq!(1, d.depth());
        d.add_leaf("Hi");
        assert_eq!(1, d.depth());
    }

    #[test]
    fn indent() {
        let d = TreeBuilder::new();
        d.add_leaf("1");
        add_branch_to!(d);
        d.add_leaf("1.1");
        {
            add_branch_to!(d);
            d.add_leaf("1.1.1");
        }
        d.set_config_override(TreeConfig::new().indent(4));
        d.peek_print();
        assert_eq!(
            "\
1
└──╼ 1.1
    └──╼ 1.1.1",
            d.string()
        );
    }

    #[test]
    fn macros() {
        let d = TreeBuilder::new();
        add_leaf_to!(d, "1");
        {
            add_branch_to!(d);
            add_leaf_to!(d, "1.1")
        }
        d.peek_print();
        assert_eq!(
            "\
1
└╼ 1.1",
            d.string()
        );
    }

    #[test]
    fn macros_with_fn() {
        let d = TreeBuilder::new();
        let tree = || d.clone();
        add_leaf_to!(tree(), "1");
        {
            add_branch_to!(tree());
            add_leaf_to!(tree(), "1.1")
        }
        tree().peek_print();
        assert_eq!(
            "\
1
└╼ 1.1",
            d.string()
        );
    }

    #[test]
    fn leaf_with_value() {
        let d = TreeBuilder::new();
        let value = add_leaf_value_to!(d, 1);
        d.peek_print();
        assert_eq!("1", d.string());
        assert_eq!(1, value);
    }

    #[test]
    fn macros2() {
        let d = TreeBuilder::new();
        add_branch_to!(d, "1");
        add_leaf_to!(d, "1.1");
        d.peek_print();
        assert_eq!(
            "\
1
└╼ 1.1",
            d.string()
        );
    }

    #[test]
    fn mid() {
        let d = TreeBuilder::new();
        d.add_leaf(&format!("{}{}", "1", "0"));
        d.enter();
        d.add_leaf("10.1");
        d.add_leaf("10.2");
        d.enter();
        d.add_leaf("10.1.1");
        d.add_leaf("10.1.2\nNext line");
        d.exit();
        d.add_leaf(&format!("10.3"));
        d.peek_print();
        assert_eq!(
            "\
10
├╼ 10.1
├╼ 10.2
│ ├╼ 10.1.1
│ └╼ 10.1.2
│    Next line
└╼ 10.3",
            d.string()
        );
    }

    fn factors(x: usize) {
        add_branch!("{}", x);
        for i in 1..x {
            if x % i == 0 {
                factors(i);
            }
        }
    }

    #[test]
    fn recursive() {
        factors(6);
        default_tree().peek_print();
        assert_eq!(
            "\
6
├╼ 1
├╼ 2
│ └╼ 1
└╼ 3
  └╼ 1",
            default_tree().string()
        );
    }

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

    #[test]
    fn nested() {
        a();
        default_tree().peek_print();
        assert_eq!(
            "\
a
├╼ b
│ └╼ c
│   └╼ Nothing to see here
└╼ c
  └╼ Nothing to see here",
            default_tree().string()
        );
    }

    #[test]
    fn disabled_output() {
        let tree = TreeBuilder::new();
        tree.set_enabled(false);
        add_leaf_to!(tree, "Leaf");
        tree.add_leaf("Leaf");

        add_branch_to!(tree, "Branch");
        tree.add_branch("Branch");
        assert_eq!("", tree.string());
    }

    #[test]
    fn enabled_output() {
        let tree = TreeBuilder::new();
        tree.set_enabled(false);
        add_branch_to!(tree, "Ignored branch");
        add_leaf_to!(tree, "Ignored leaf");
        tree.set_enabled(true);
        add_leaf_to!(tree, "Leaf");
        tree.add_leaf("Leaf");

        add_branch_to!(tree, "Branch");
        tree.add_branch("Branch");
        assert_eq!(
            "Leaf
Leaf
Branch
└╼ Branch",
            tree.string()
        );
    }

    #[test]
    fn tree_by_name() {
        clear("A");
        let b = tree("B");
        b.clear();
        {
            add_branch_to!("A", "1");
            add_branch_to!(b, "3");
            add_leaf_to!("A", "1.1");
            add_leaf_to!("B", "3.1");
        }
        add_leaf_to!("A", "2");
        peek_print("A");
        b.peek_print();
        assert_eq!(
            "1
└╼ 1.1
2",
            string("A")
        );
        assert_eq!(
            "3
└╼ 3.1",
            b.string()
        );
    }

    #[test]
    fn tree_by_name_disabled() {
        let d = tree("D");
        d.clear();
        d.set_enabled(true);
        clear("C");
        set_enabled("C", false);
        {
            add_branch_to!("C", "1");
            set_enabled("C", true);
            add_branch_to!(d, "3");
            add_leaf_to!("C", "1.1");
            d.set_enabled(false);
            add_leaf_to!("D", "3.1");
        }
        add_leaf_to!("C", "2");
        peek_print("C");
        d.peek_print();
        assert_eq!(
            "1.1
2",
            string("C")
        );
        assert_eq!("3", d.string());
    }

    #[test]
    fn defer_write() {
        let tree = TreeBuilder::new();
        {
            create_dir("test_out").ok();
            remove_file("test_out/defer_write.txt").ok();
            File::create("test_out/defer_write.txt").unwrap();
            defer_write!(tree, "test_out/defer_write.txt");
            tree.add_leaf("Branch");
            assert_eq!(read_to_string("test_out/defer_write.txt").unwrap(), "");
            assert_eq!(tree.peek_string(), "Branch");
        }
        assert_eq!(tree.peek_string(), "");
        assert_eq!(
            read_to_string("test_out/defer_write.txt").unwrap(),
            "Branch"
        );
    }

    #[test]
    fn defer_peek_write() {
        let tree = TreeBuilder::new();
        {
            create_dir("test_out").ok();
            remove_file("test_out/defer_peek_write.txt").ok();
            File::create("test_out/defer_peek_write.txt").unwrap();
            defer_peek_write!(tree, "test_out/defer_peek_write.txt");
            tree.add_leaf("Branch");
            assert_eq!(read_to_string("test_out/defer_peek_write.txt").unwrap(), "");
            assert_eq!(tree.peek_string(), "Branch");
        }
        assert_eq!(tree.peek_string(), "Branch");
        assert_eq!(
            read_to_string("test_out/defer_peek_write.txt").unwrap(),
            "Branch"
        );
    }

    #[test]
    #[should_panic]
    #[allow(unreachable_code)]
    fn defer_peek_write_panic() {
        let tree = TreeBuilder::new();
        {
            create_dir("test_out").ok();
            remove_file("test_out/defer_peek_write_panic.txt").ok();
            File::create("test_out/defer_peek_write_panic.txt").unwrap();
            defer_peek_write!(tree, "test_out/defer_peek_write_panic.txt");
            tree.add_leaf("This should be the only line in this file");
            assert_eq!(read_to_string("test_out/defer_peek_write.txt").unwrap(), "");
            assert_eq!(
                tree.peek_string(),
                "This should be the only line in this file"
            );
            panic!();
            tree.add_leaf("This line should not exist");
        }
    }

    fn example_tree() -> TreeBuilder {
        let tree = TreeBuilder::new();
        {
            add_branch_to!(tree, "1");
            {
                add_branch_to!(tree, "1.1");
                add_leaf_to!(tree, "1.1.1");
                add_leaf_to!(tree, "1.1.2\nWith two\nextra lines");
                add_leaf_to!(tree, "1.1.3");
            }
            add_branch_to!(tree, "1.2");
            add_leaf_to!(tree, "1.2.1");
        }
        {
            add_branch_to!(tree, "2");
            add_leaf_to!(tree, "2.1");
            add_leaf_to!(tree, "2.2");
        }
        add_leaf_to!(tree, "3");
        tree
    }

    #[test]
    fn format_output() {
        let tree = example_tree();
        tree.set_config_override(
            TreeConfig::new()
                .indent(8)
                .symbols(TreeSymbols {
                    continued: "| |",
                    join_first: "|A|",
                    join_last: "|Z|",
                    join_inner: "|N|",
                    join_only: "|O|",
                    branch: "123456[NOT SHOWN]",
                    leaf: ")}>",
                    multiline_first: Some(")}MULTI>"),
                    multiline_continued: Some(".. CONTINUED: "),
                })
                .show_first_level(),
        );
        tree.peek_print();
        assert_eq!(
            tree.string(),
            "\
|A|123456)}>1
| |       |A|123456)}>1.1
| |       | |       |A|123456)}>1.1.1
| |       | |       |N|123456)}MULTI>1.1.2
| |       | |       | |      .. CONTINUED: With two
| |       | |       | |      .. CONTINUED: extra lines
| |       | |       |Z|123456)}>1.1.3
| |       |Z|123456)}>1.2
| |               |O|123456)}>1.2.1
|N|123456)}>2
| |       |A|123456)}>2.1
| |       |Z|123456)}>2.2
|Z|123456)}>3"
        );
    }

    #[test]
    fn format_output_thick() {
        let tree = example_tree();
        tree.set_config_override(
            TreeConfig::new()
                .symbols(TreeSymbols::with_thick())
                .indent(4)
                .show_first_level(),
        );
        tree.peek_print();
        assert_eq!(
            tree.string(),
            "\
┣━━╼ 1
┃   ┣━━╼ 1.1
┃   ┃   ┣━━╼ 1.1.1
┃   ┃   ┣━━╼ 1.1.2
┃   ┃   ┃    With two
┃   ┃   ┃    extra lines
┃   ┃   ┗━━╼ 1.1.3
┃   ┗━━╼ 1.2
┃       ┗━━╼ 1.2.1
┣━━╼ 2
┃   ┣━━╼ 2.1
┃   ┗━━╼ 2.2
┗━━╼ 3"
        );
    }

    #[test]
    fn format_output_pipes() {
        let tree = example_tree();
        tree.set_config_override(
            TreeConfig::new()
                .symbols(TreeSymbols::with_pipes())
                .indent(3)
                .show_first_level(),
        );
        tree.peek_print();
        assert_eq!(
            tree.string(),
            "\
╠═╼ 1
║  ╠═╼ 1.1
║  ║  ╠═╼ 1.1.1
║  ║  ╠═╼ 1.1.2
║  ║  ║   With two
║  ║  ║   extra lines
║  ║  ╚═╼ 1.1.3
║  ╚═╼ 1.2
║     ╚═╼ 1.2.1
╠═╼ 2
║  ╠═╼ 2.1
║  ╚═╼ 2.2
╚═╼ 3"
        );
    }

    #[test]
    fn format_output_dashed() {
        let tree = example_tree();
        tree.set_config_override(
            TreeConfig::new()
                .symbols(TreeSymbols::with_dashed().multiline_continued("  > "))
                .indent(4)
                .show_first_level(),
        );
        tree.peek_print();
        assert_eq!(
            tree.string(),
            "\
┊╌╌- 1
┊   ┊╌╌- 1.1
┊   ┊   ┊╌╌- 1.1.1
┊   ┊   ┊╌╌- 1.1.2
┊   ┊   ┊    > With two
┊   ┊   ┊    > extra lines
┊   ┊   '╌╌- 1.1.3
┊   '╌╌- 1.2
┊       '╌╌- 1.2.1
┊╌╌- 2
┊   ┊╌╌- 2.1
┊   '╌╌- 2.2
'╌╌- 3"
        );
    }

    #[test]
    fn format_output_rounded() {
        let tree = example_tree();
        tree.set_config_override(
            TreeConfig::new()
                .symbols(TreeSymbols::with_rounded())
                .indent(4),
        );
        tree.peek_print();
        assert_eq!(
            tree.string(),
            "\
1
├──╼ 1.1
│   ├──╼ 1.1.1
│   ├──╼ 1.1.2
│   │    With two
│   │    extra lines
│   ╰──╼ 1.1.3
╰──╼ 1.2
    ╰──╼ 1.2.1
2
├──╼ 2.1
╰──╼ 2.2
3"
        );
    }

    async fn wait_a_bit(tree: TreeBuilder, index: usize) {
        tree.print();
        add_branch_to!(tree, "inside async branch {}", index);
        tree.print();
        add_leaf_to!(tree, "inside async leaf {}", index);
        tree.print();
    }

    #[tokio::test]
    async fn async_barrier() {
        let tree = TreeBuilder::new();
        defer_peek_print!(tree);
        add_branch_to!(tree, "root");
        add_leaf_to!(tree, "before async");

        let x2 = wait_a_bit(tree.clone(), 4);
        let x1 = wait_a_bit(tree.clone(), 5);
        let x3 = wait_a_bit(tree.clone(), 3);
        let x4 = wait_a_bit(tree.clone(), 2);
        let x5 = wait_a_bit(tree.clone(), 1);

        add_leaf_to!(tree, "before join async");

        join5(x1, x2, x3, x4, x5).await;
        add_leaf_to!(tree, "after join async");
        assert_eq!(tree.peek_string(), "after join async");
    }
}
