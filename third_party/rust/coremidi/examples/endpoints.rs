use coremidi::{Destinations, Endpoint, Sources};

fn main() {
    println!("System destinations:");

    for (i, destination) in Destinations.into_iter().enumerate() {
        let display_name = get_display_name(&destination);
        println!("[{}] {}", i, display_name);
    }

    println!();
    println!("System sources:");

    for (i, source) in Sources.into_iter().enumerate() {
        let display_name = get_display_name(&source);
        println!("[{}] {}", i, display_name);
    }
}

fn get_display_name(endpoint: &Endpoint) -> String {
    endpoint
        .display_name()
        .unwrap_or_else(|| "[Unknown Display Name]".to_string())
}
