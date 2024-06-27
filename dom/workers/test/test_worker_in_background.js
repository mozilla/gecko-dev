let childworker = new Worker("test_worker_in_background_child.js");

onmessage = async e => {
  childworker.postMessage(e.data);
};

childworker.onmessage = async e => {
  postMessage(e.data);
};
