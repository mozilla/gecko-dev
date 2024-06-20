for (var i = 0; i < 15; i++) {
    new Promise(function() { throw 1; }).catch(e => null).finally(Object);
}
