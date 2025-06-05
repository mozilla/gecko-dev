use coremidi::{Client, EventList, Properties, Protocol};

fn main() {
    let client = Client::new("Example Client").unwrap();

    let callback = |event_list: &EventList| {
        println!("{:?}", event_list);
    };

    // Creates a virtual destination, then gets its properties
    let destination = client
        .virtual_destination_with_protocol("Example Destination", Protocol::Midi20, callback)
        .unwrap();

    // All coremidi structs dereference to an Object, so you can directly
    // ask for some of the supported properties invoking a method like:

    println!("Created Virtual Destination:");
    println!("  Display Name: {}", destination.display_name().unwrap());

    // The rest of the supported properties can be accessed like:

    println!(
        "  Protocol ID: {}",
        destination
            .get_property::<i32>(&Properties::protocol_id())
            .unwrap()
    );

    destination
        .set_property(&Properties::private(), true)
        .unwrap();
    println!(
        "  Private: {}",
        destination
            .get_property::<bool>(&Properties::private())
            .unwrap()
    );

    // You can also set/get your own properties like:

    destination
        .set_property_string("my-own-string-property", "my-value")
        .unwrap();
    println!(
        "  My own string property: {}",
        destination
            .get_property_string("my-own-string-property")
            .unwrap()
    )
}
