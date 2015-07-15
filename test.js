var Threx = require('threx');
var work = require('./build/Release/worky.node').eval(function () {console.log(arguments);},'test')
var thread = new Threx();
thread.spawn();
console.log('queueing');
thread.enqueue(work);
thread.join();
console.log('joined');
