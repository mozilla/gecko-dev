use coremidi::{Client, EventList, Protocol};

fn main() {
    let client = Client::new("Example Client").unwrap();

    let callback = |event_list: &EventList| {
        print!("{:?}", event_list);
    };

    let _destination = client
        .virtual_destination_with_protocol("Example Destination", Protocol::Midi10, callback)
        .unwrap();

    let mut input_line = String::new();
    println!("Created Virtual Destination \"Example Destination\"");
    println!("Press Enter to Finish");
    std::io::stdin()
        .read_line(&mut input_line)
        .expect("Failed to read line");
}
