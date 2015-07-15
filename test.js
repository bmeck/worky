var Threx = require('threx');
function log() {
  console.log('log:', arguments);
  process.exit(0);
}
var worky = require('./build/Release/worky.node');

// eval(cb, string)
var work_parts = worky.eval(log, 'tess')

var thread = new Threx();
thread.spawn();
console.log('queueing');
thread.enqueue(work_parts[0], work_parts[1]);

// something to keep loop alive while we wait on log()
var test_blocking = Boolean(process.env.BLOCK);
if (test_blocking) {
 for(;;){}
 console.log('NEVER SHOULD GET HERE');
}
else {
  setInterval(Function.prototype, 1e5);
}
