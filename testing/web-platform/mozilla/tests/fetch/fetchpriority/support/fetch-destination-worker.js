self.addEventListener('fetch', function(event) {
    if (event.request.url.includes('priority-dependent-content.py')) {
        let params = new URL(event.request.url).searchParams;
        let destination = params.get("destination");
        let fetchpriority = params.get("fetchpriority");
        if (event.request.destination == destination ||
            (event.request.destination == "empty" && destination == "")) {
            let options = fetchpriority ? { priority: fetchpriority } : null;
            event.respondWith(fetch(event.request, options));
        } else {
            event.respondWith(Response.error());
        }
    }
});
