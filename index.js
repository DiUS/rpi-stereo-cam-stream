var express = require('express');
var app = express();
var http = require('http').Server(app);
var io = require('socket.io')(http);
var fs = require('fs');
var path = require('path');
var spawn = require('child_process').spawn;
var bodyParser = require('body-parser');
var proc;
var interval = 3000;
var raspistill_args = {
    "-tl"  : interval,
    "-t"   : 0,
    "-ISO" : 100,
    "-ss"  : 800,
    "-awb" : "off",
    "-awbg": "1,1",
    "-q"   : 100,
    "-th"  : "none",
    "-n"   : null,
    "-w"   : 5184,
    "-h"   : 1944,
    "-3d"  : "sbs",
    "-o"   : "/tmp/image_stream.jpg"
  };

app.use(bodyParser.json());

app.get('/static/:file', function(req, res) {
  var static_file;
  static_file = 'static/' + req.params.file;
  return fs.exists(static_file, function(exists) {
    var params;
    if (exists) {
      return res.sendfile(static_file);
    } else {
      res.status(404);
      return res.render('error', params = {
        title: 'File error',
        description: 'File not found'
      });
    }
  });
});

app.get('/stream/:file', function(req, res) {
  var static_file;
  static_file = '/tmp/' + req.params.file;
  return fs.exists(static_file, function(exists) {
    var params;
    if (exists) {
      return res.sendfile(static_file);
    } else {
      res.status(404);
      return res.render('error', params = {
        title: 'File error',
        description: 'File not found'
      });
    }
  });
});

app.get('/', function(req, res) {
  res.sendfile(__dirname + '/index.html');
});

app.post('/update', function(req, res){
  var responseString = 'OK';
  console.log('body: ' + JSON.stringify(req.body));
  for (var k in req.body)
    raspistill_args['-'+k] = req.body[k];
  console.log(JSON.stringify(raspistill_args));
  if (proc) {
    proc.kill();
    proc = spawn('raspistill', serializeRaspistillArgs());
  }
  res.send(responseString);
});

var sockets = {};
io.on('connection', function(socket) {
  sockets[socket.id] = socket;
  console.log("Total clients connected : ", numClients());

  socket.on('disconnect', function() {
    delete sockets[socket.id];

    console.log("Total clients connected : ", numClients());
    if (numClients() === 0) {
      stopStreaming();
    }
  });
  socket.on('start-stream', function() {
    startStreaming(io);
  });
});

http.listen(3000, function() {
  console.log('listening on *:3000');
});

function serializeRaspistillArgs() {
  var args = [];
  for (var k in raspistill_args) {
    args.push(k)
    if (raspistill_args[k] !== null)
      args.push(raspistill_args[k]);
  }
  return args;
}

function numClients() {
  return Object.keys(sockets).length;
}

function stopStreaming() {
  if (Object.keys(sockets).length == 0) {
    app.set('watchingFile', false);
    if (proc) proc.kill();
    fs.unwatchFile('/tmp/image_stream.jpg');
  }
}

function startStreaming(io) {
  // if already watching file
  if (app.get('watchingFile')) {
    io.sockets.emit('liveStream', 'stream/image_stream.jpg?_t=' + (Math.random() * 100000));
    return;
  }

  // start new watch
  proc = spawn('raspistill', serializeRaspistillArgs());
  console.log('Watching for changes...');
  app.set('watchingFile', true);
  fs.watchFile('/tmp/image_stream.jpg', {interval: interval}, function(current, previous) {
    io.sockets.emit('liveStream', 'stream/image_stream.jpg?_t=' + (Math.random() * 100000));
  });
}
