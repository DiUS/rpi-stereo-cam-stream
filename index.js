var express = require('express');
var app = express();
var http = require('http').Server(app);
var io = require('socket.io')(http);
var fs = require('fs');
var path = require('path');
var spawn = require('child_process').spawn;
var bodyParser = require('body-parser');

var proc;
var pollTimer;
var mode = 'test';
var prevModTime;
var stream_dir = '/tmp/';
var capture_dir = '/storage/photos/';
var pollInterval = 5000;
var raspistill_args = {
    "tl"  : 1000,
    "t"   : 0,
    "ISO" : 100,
    "ss"  : 800,
    "awb" : "off",
    "awbg": "1,1",
    "q"   : 100,
    "th"  : "none",
    "bm"  : null,
    "n"   : null,
    "w"   : 5184,
    "h"   : 1944,
    "3d"  : "sbs"
  };
var stream_args = {
    "o"   : stream_dir + "image_stream.jpg"
};
var capture_args = {
    "ts"  : null,
    "o"   : capture_dir + "image_%d.jpg"
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
  static_file = stream_dir + req.params.file;
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


app.get('/capture/:file', function(req, res) {
  var static_file;
  static_file = capture_dir + req.params.file;
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


process.on('exit', killChild);

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
  socket.on('new-cam-config', function(data) {
    if (app.get('pollDir'))
      update_cam_config(data);
  });
  socket.on('capture', function(data) {
    if (app.get('pollDir'))
      start_stop_capture(data);
  });
});


http.listen(3000, function() {
  console.log('listening on *:3000');
});


function emit_latest_image() {
  emit_mode();
  emit_cam_config();
  if (mode === 'test') {
    fs.readdir(stream_dir, function(err, files) {
      var latest = files.filter(function(file) { return file === 'image_stream.jpg'; }).shift();
      if (latest) {
        var currModTime = fs.statSync(stream_dir + latest).mtime.getTime();
        if (currModTime != prevModTime) {
          prevModTime = currModTime;
          io.sockets.emit('liveStream', 'stream/'+latest+'?_t=' + (Math.random() * 100000));
        }
      }
    });
  } else if (mode === 'capture') {
    fs.readdir(capture_dir, function(err, files) {
      var latest = files.filter(function(file) { return file.substr(-4) === '.jpg'; })
                    .sort(function(a, b) {
                     return fs.statSync(capture_dir + b).mtime.getTime() -
                            fs.statSync(capture_dir + a).mtime.getTime();
                   }).shift();
      if (latest) {
        var currModTime = fs.statSync(capture_dir + latest).mtime.getTime();
        if (currModTime != prevModTime) {
          prevModTime = currModTime;
          io.sockets.emit('liveStream', 'capture/'+latest+'?_t=' + (Math.random() * 100000));
        }
      }
    });
  }
}


function emit_mode() {
  io.sockets.emit('mode', mode);
}


function emit_cam_config() {
  io.sockets.emit('current-cam-config', raspistill_args);
}


function serializeRaspistillArgs(optional_args) {
  var args = [];
  for (var k in raspistill_args) {
    args.push('-'+k)
    if (raspistill_args[k] !== null)
      args.push(raspistill_args[k]);
  }
  if (optional_args) {
    for (var k in optional_args) {
      args.push('-'+k)
      if (optional_args[k] !== null)
        args.push(optional_args[k]);
    }
  }
  return args;
}


function update_cam_config(new_config) {
  if (mode === 'test') {
    for (var k in new_config)
      raspistill_args[k] = new_config[k];
    console.log(JSON.stringify(serializeRaspistillArgs(stream_args)));
    emit_cam_config();
    killChild();
    proc = spawn('raspistill', serializeRaspistillArgs(stream_args));
  } else {
    responseString = 'Not allowed';
  }
}


function start_stop_capture(action) {
  if ((action === 'start') && (mode === 'test')) {
    console.log('Capturing ...');
    killChild();
    console.log(JSON.stringify(serializeRaspistillArgs(capture_args)));
    proc = spawn('raspistill', serializeRaspistillArgs(capture_args));
    mode = 'capture';
  } else if ((action === 'stop') && (mode === 'capture')) {
    console.log('Stopped capturing');
    killChild();
    console.log(JSON.stringify(serializeRaspistillArgs(stream_args)));
    proc = spawn('raspistill', serializeRaspistillArgs(stream_args));
    mode = 'test';
  }
  emit_mode();
}


function numClients() {
  return Object.keys(sockets).length;
}


function killChild() {
  if (proc) {
    proc.kill();
    proc = null;
  }
}


function stopStreaming() {
  if (Object.keys(sockets).length == 0) {
    if (mode === 'test') {
      if (pollTimer)
        clearInterval(pollTimer);
      killChild();
      app.set('pollDir', false);
    }
  }
}


function startStreaming(io) {
  // if already polling
  if (app.get('pollDir')) {
    emit_latest_image();
    return;
  }

  // start new poll
  app.set('pollDir', true);
  mode = 'test';
  proc = spawn('raspistill', serializeRaspistillArgs(stream_args));

  console.log('Polling directories for changes...');
  pollTimer = setInterval(emit_latest_image, pollInterval);
  emit_mode();
}
