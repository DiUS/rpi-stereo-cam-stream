var express = require('express');
var app = express();
var http = require('http').Server(app);
var io = require('socket.io')(http);
var fs = require('fs');
var path = require('path');
var spawn = require('child_process').spawn;
var proc;

app.use('/', express.static(path.join(__dirname, 'stream')));

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
  var interval = 3000;

  // if already watching file
  if (app.get('watchingFile')) {
    io.sockets.emit('liveStream', 'stream/image_stream.jpg?_t=' + (Math.random() * 100000));
    return;
  }

  // start new watch
  //var args = ["-tl", interval, "-t", 0, "-sh", 100, "-q", 100, "-th", "none", "-n", "-w", 5184, "-h", 1944, "-3d", "sbs", "-o", "/tmp/image_stream.jpg"];
  var args = ["-tl", interval, "-t", 0, "-q", 100, "-th", "none", "-n", "-w", 5184, "-h", 1944, "-3d", "sbs", "-o", "/tmp/image_stream.jpg"];
  //var args = ["-tl", interval, "-t", 0, "-sh", 100, "-cs", 0, "-q", 100, "-th", "none", "-n", "-w", 2592, "-h", 1944, "-o", "/tmp/image_stream.jpg"];
  //var args = ["-tl", interval, "-t", 0, "-sh", 100, "-cs", 1, "-q", 100, "-th", "none", "-n", "-w", 2592, "-h", 1944, "-o", "/tmp/image_stream.jpg"];
  //var args = ["-tl", interval, "-t", 0, "-cs", 0, "-q", 100, "-th", "none", "-n", "-w", 2592, "-h", 1944, "-o", "/tmp/image_stream.jpg"];
  //var args = ["-tl", interval, "-t", 0, "-cs", 1, "-q", 100, "-th", "none", "-n", "-w", 2592, "-h", 1944, "-o", "/tmp/image_stream.jpg"];
  proc = spawn('raspistill', args);
  console.log('Watching for changes...');
  app.set('watchingFile', true);
  fs.watchFile('/tmp/image_stream.jpg', {interval: interval}, function(current, previous) {
    io.sockets.emit('liveStream', 'stream/image_stream.jpg?_t=' + (Math.random() * 100000));
  });
}
