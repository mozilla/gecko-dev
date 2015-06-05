this.EXPORTED_SYMBOLS = ['foco'];
var self = this;
(function (){
  var foco = {};
  var root = this;

  foco.each = function (items, handler, finished){
    var index = 0;
    function done(){
      index++;
      if (index < items.length) {
        handler(index, items[index], done);
      } else {
        finished();
      }
    }

    handler(index, items[index], done);
  };

  foco.priorityQueue = function (handler, concurrency){
    var listQueuing = [];
    var listProcessing = [];

    function linearSearch(list, compare){
      for(var i in list){
        if(compare(list[i])){
          return i;
        }
      }
      return -1;
    }
    function move(list, old_index, new_index) {
      if (new_index >= list.length) {
        var k = new_index - list.length;
        while ((k--) + 1) {
          list.push(undefined);
        }
      }
      list.splice(new_index, 0, list.splice(old_index, 1)[0]);
    }
    function removeProcessing(id){
      var index = linearSearch(listProcessing, function (item){
        return item.id === id;
      });
      if(index != -1){
        listProcessing.splice(index, 1);
      }
    }
    function done(){
      removeProcessing(this.id);
      processTask();
    }
    function processTask(){
      //console.log('listQueuing.length ' + listQueuing.length);
      if(listQueuing.length === 0){
        //console.log('queue empty');
      } else if(listProcessing.length < concurrency){
        //console.log('task aval ' + listProcessing.length + ' ' + concurrency);
        var task = listQueuing.shift();
        listProcessing.push(task);
        setTimeout(function (){
          handler(task.id, task.data, done.bind(task));
        }, 0);
      } else {
        //console.log('task full ' + listProcessing.length + ' ' + concurrency);
      }
    }
    var queue = {
      push: function (id, priority, data){
        var task = {
          id: id,
          priority: priority,
          data: data
        };
        var index = linearSearch(listQueuing, function (item){
          return item.priority > priority;
        });
        if (index === -1) { // Not found
          listQueuing.push(task);
        } else {
          listQueuing.splice(index, 0, task);
        }
        processTask();
      },
      priorityChange: function (id, priority){
        var indexById = linearSearch(listQueuing, function (item){
          return item.id === id;
        });
        if (indexById === -1) {
          return ;
        }
        var indexByPriority = linearSearch(listQueuing, function (item){
          return item.priority > priority;
        });
        move(listQueuing, indexById, indexByPriority);
      }
    };
    return queue;
  };

  if (typeof module !== 'undefined' && module.exports) { // Node.js support
    module.exports = foco;
  } else { // browser support
    root.foco = foco;
  }
  self.foco = foco;
})();
