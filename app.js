var createError = require('http-errors');
var express = require('express');
var path = require('path');
var cookieParser = require('cookie-parser');
var logger = require('morgan');
var cors = require('cors');
var fs = require('fs');

var indexRouter = require('./routes/index');
var usersRouter = require('./routes/users');
var cameraRouter = require('./routes/camera');

var app = express();

// view engine setup
app.set('views', path.join(__dirname, 'views'));
app.set('view engine', 'jade');

// Middleware
// CORS configuration - Allow all origins for local development (mobile devices)
app.use(cors({
  origin: true, // Allow all origins for local network access
  credentials: true,
  methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
  allowedHeaders: ['Content-Type', 'Authorization']
}));
app.use(logger('dev'));
app.use(express.json({ limit: '50mb' }));
app.use(express.urlencoded({ extended: true, limit: '50mb' }));
app.use(cookieParser());
app.use(express.static(path.join(__dirname, 'public')));

// Ensure uploads directory exists
var uploadsDir = path.join(__dirname, 'uploads');
if (!fs.existsSync(uploadsDir)) {
  fs.mkdirSync(uploadsDir, { recursive: true });
}

// Serve uploaded images
app.use('/uploads', express.static(uploadsDir));

// Initialize ESP32 status and frame storage
app.set('latestFrame', null);
app.set('frameCount', 0);
app.set('esp32Status', {
  connected: false,
  lastUpdate: null,
  frameCount: 0,
  fps: 0,
});

// Calculate FPS
var lastFpsCheck = Date.now();
var framesSinceLastCheck = 0;

setInterval(function() {
  var now = Date.now();
  var elapsed = (now - lastFpsCheck) / 1000;
  var esp32Status = app.get('esp32Status');
  esp32Status.fps = Math.round(framesSinceLastCheck / elapsed);
  app.set('esp32Status', esp32Status);
  framesSinceLastCheck = 0;
  lastFpsCheck = now;
}, 1000);

// Monitor ESP32 connection
setInterval(function() {
  var esp32Status = app.get('esp32Status');
  if (esp32Status.lastUpdate) {
    var timeSinceUpdate = Date.now() - new Date(esp32Status.lastUpdate).getTime();
    if (timeSinceUpdate > 5000) {
      esp32Status.connected = false;
      esp32Status.fps = 0;
      app.set('esp32Status', esp32Status);
    }
  }
}, 2000);

// Make framesSinceLastCheck accessible globally
app.set('framesSinceLastCheck', 0);
app.set('incrementFrameCount', function() {
  framesSinceLastCheck++;
});

// Routes
app.use('/', indexRouter);
app.use('/users', usersRouter);
app.use('/', cameraRouter);

// catch 404 and forward to error handler
app.use(function(req, res, next) {
  next(createError(404));
});

// error handler
app.use(function(err, req, res, next) {
  // set locals, only providing error in development
  res.locals.message = err.message;
  res.locals.error = req.app.get('env') === 'development' ? err : {};

  // render the error page
  res.status(err.status || 500);
  res.render('error');
});

module.exports = app;
