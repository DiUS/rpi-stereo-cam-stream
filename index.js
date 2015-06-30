var express = require('express');
var app = express();
var http = require('http').Server(app);
var io = require('socket.io')(http);
var tar = require('tar-fs');
var fs = require('fs');
var path = require('path');
var spawn = require('child_process').spawn;
var exec = require('child_process').exec;
var bodyParser = require('body-parser');

var proc;
var pollTimer;
var mode = 'test';
var prevModTime;
var stream_dir = '/tmp/';
var capture_partition = '/storage';
var capture_dir = capture_partition + '/photos/';
var pollInterval = 5000;
var raspistill_args = {
  "tl"  : 1000,
  "be"  : null,
  "t"   : 0,
  "ISO" : 100,
  "ss"  : 800,
  "awb" : "off",
  "awbg": "1,1",
  "q"   : 100,
  "th"  : "none",
  "gps" : null,
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


app.get('/download', function(req, res) {
  var pack = tar.pack(capture_dir, {
    ignore: function(name) {
      return name.substr(-4) !== '.jpg';
    }
  });
  var d = new Date().toISOString().slice(0, 19).replace(/[-T:]/g, "");
  var fname = "images_" + d + ".tar";
  res.setHeader('Content-disposition', 'attachment; filename=' + fname);
  res.setHeader('content-type', 'application/octet-stream');
  pack.pipe(res);
});


app.get('/', function(req, res) {
  res.sendfile(__dirname + '/index.html');
});


exec("kill `pidof raspistill`");
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
  socket.on('images', function(data) {
    if (data === 'ls')
      listImages();
    else if (data === 'rm')
      deleteImages();
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
      if (!err) {
        var latest = files.filter(function(file) { return file === 'image_stream.jpg'; }).shift();
        if (latest) {
          var currModTime = fs.statSync(stream_dir + latest).mtime.getTime();
          if (currModTime != prevModTime) {
            prevModTime = currModTime;
            io.sockets.emit('liveStream', 'stream/'+latest+'?_t=' + (Math.random() * 100000));
          }
        }
      }
    });
  } else if (mode === 'capture') {
    fs.readdir(capture_dir, function(err, files) {
      if (!err) {
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


function emit_image_list(images) {
  var image_list = {};
  image_list.dir = 'capture';
  image_list.images = images;
  io.sockets.emit('image-list', image_list);
}


function emit_diskfree() {
  diskfree(capture_partition, function (error, total, used, free) {
    if (!error) {
      var diskinfo = {};
      diskinfo.total = total;
      diskinfo.used = used;
      diskinfo.free = free;
      io.sockets.emit('diskinfo', diskinfo);
    }
  });
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


function diskfree(drive, callback) {
  var total = 0;
  var free = 0;
  var used = 0;
  exec("df -k '" + drive.replace(/'/g,"'\\''") + "'", function(error, stdout, stderr) {
    if (error) {
      callback ? callback(error, total, used, free)
      : console.error(stderr);
    } else {
      var lines = stdout.trim().split("\n");
      var str_disk_info = lines[lines.length - 1].replace( /[\s\n\r]+/g,' ');
      var disk_info = str_disk_info.split(' ');
      total = disk_info[1] * 1024;
      used = disk_info[2] * 1024;
      free = disk_info[3] * 1024;
      callback && callback(null, total, used, free);
    }
  });
}


function listImages() {
  fs.readdir(capture_dir, function(err, files) {
    if (!err) {
      var images = files.filter(function(file) { return file.substr(-4) === '.jpg'; })
                   .sort(function(a, b) {
                    return fs.statSync(capture_dir + b).mtime.getTime() -
                           fs.statSync(capture_dir + a).mtime.getTime();
                   });
      if (images)
        emit_image_list(images);
    }
  });
  emit_diskfree();
}


function deleteFiles(files, callback) {
  var i = files.length;
  files.forEach(function(filepath) {
    fs.unlink(capture_dir + filepath, function(err) {
      i--;
      if (err) {
        callback(err);
        return;
      } else if (i <= 0) {
        callback(null);
      }
    });
  });
}


function deleteImages(){
  fs.readdir(capture_dir, function(err, files) {
    if (!err) {
      var images = files.filter(function(file) {
                    return (file.substr(-4) === '.jpg') || (file.substr(-5) === '.jpg~');
                   });
      if (images) {
        deleteFiles(images, function(err) {
            if (err) {
              console.log(err);
            } else {
              emit_image_list([]);
              emit_diskfree();
            }
          });
      }
    }
  });
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
    if (pollTimer) {
      console.log('stop poll timer');
      clearInterval(pollTimer);
      pollTimer = null;
    }
    if (mode === 'test') {
      killChild();
      app.set('pollDir', false);
    }
  }
}


function startStreaming(io) {
  // if already polling
  if (app.get('pollDir')) {
    emit_mode();
    emit_latest_image();
    if (!pollTimer) {
      console.log('start poll timer');
      pollTimer = setInterval(emit_latest_image, pollInterval);
    }
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
