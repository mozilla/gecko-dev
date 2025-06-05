use coremidi::{Client, Destination, Destinations, EventBuffer, Protocol};
use std::env;
use std::thread;
use std::time::Duration;

fn main() {
    let destination_index = get_destination_index();
    println!("Destination index: {}", destination_index);

    let destination = Destination::from_index(destination_index).unwrap();
    println!(
        "Destination display name: {}",
        destination.display_name().unwrap()
    );

    let client = Client::new("Example Client").unwrap();
    let output_port = client.output_port("Example Port").unwrap();

    let note_on = EventBuffer::new(Protocol::Midi10).with_packet(0, &[0x2090407f]);

    let note_off = EventBuffer::new(Protocol::Midi10).with_packet(0, &[0x2080407f]);

    for i in 0..10 {
        println!("[{}] Sending note ...", i);

        output_port.send(&destination, &note_on).unwrap();
        thread::sleep(Duration::from_millis(1000));

        output_port.send(&destination, &note_off).unwrap();
        thread::sleep(Duration::from_millis(100));
    }
}

fn get_destination_index() -> usize {
    let mut args_iter = env::args();
    let tool_name = args_iter
        .next()
        .and_then(|path| {
            path.split(std::path::MAIN_SEPARATOR)
                .last()
                .map(|v| v.to_string())
        })
        .unwrap_or_else(|| "send".to_string());

    match args_iter.next() {
        Some(arg) => match arg.parse::<usize>() {
            Ok(index) => {
                if index >= Destinations::count() {
                    println!("Destination index out of range: {}", index);
                    std::process::exit(-1);
                }
                index
            }
            Err(_) => {
                println!("Wrong destination index: {}", arg);
                std::process::exit(-1);
            }
        },
        None => {
            println!("Usage: {} <destination-index>", tool_name);
            println!();
            println!("Available Destinations:");
            print_destinations();
            std::process::exit(-1);
        }
    }
}

fn print_destinations() {
    for (i, destination) in Destinations.into_iter().enumerate() {
        if let Some(display_name) = destination.display_name() {
            println!("[{}] {}", i, display_name)
        }
    }
}
