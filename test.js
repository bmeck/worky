var Threx = require('threx');
function log() {
  console.log('log:', arguments);
}
var worky = require('./build/Release/worky.node');

// eval(cb, string)
var work_parts = worky.eval(log, 'tess')

var thread = new Threx();
thread.spawn();
console.log('queueing');
thread.enqueue(work_parts[0], work_parts[1]);
// should printf here for parse?
thread.join();
console.log('joined');
// should log: here
