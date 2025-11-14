var express = require('express');
var router = express.Router();
var fs = require('fs');
var path = require('path');
var FormData = require('form-data');
var axios = require('axios');

// ML Service Configuration
var ML_SERVICE_URL = process.env.ML_SERVICE_URL || 'http://localhost:5000';

/**
 * Middleware to handle raw image data
 */
var rawBodyMiddleware = function(req, res, next) {
  if (req.headers['content-type'] && req.headers['content-type'].startsWith('image/')) {
    var data = [];
    req.on('data', function(chunk) {
      data.push(chunk);
    });
    req.on('end', function() {
      req.rawBody = Buffer.concat(data);
      next();
    });
  } else {
    next();
  }
};

/**
 * ESP32 sends frame to this endpoint (DEPRECATED - Now using Socket.IO only)
 * Kept for backward compatibility or testing purposes
 */
router.post('/frame', rawBodyMiddleware, function(req, res) {
  try {
    var app = req.app;
    var frameData = req.body;
    var latestFrame = null;
    
    // Optional: Detailed frame logging (comment out if too verbose)
    // console.log('📸 Frame received from ESP32, size:', req.headers['content-length'], 'bytes');
    
    // Check for raw image data first (from middleware)
    if (req.rawBody) {
      latestFrame = req.rawBody;
    } else if (Buffer.isBuffer(frameData)) {
      latestFrame = frameData;
    } else if (typeof frameData === 'string') {
      // Base64 encoded
      latestFrame = Buffer.from(frameData, 'base64');
    } else if (frameData && frameData.image) {
      // JSON with base64 image
      latestFrame = Buffer.from(frameData.image, 'base64');
    }

    if (latestFrame) {
      app.set('latestFrame', latestFrame);
      
      var frameCount = app.get('frameCount') + 1;
      app.set('frameCount', frameCount);
      
      // Increment frames for FPS calculation
      var incrementFrameCount = app.get('incrementFrameCount');
      if (incrementFrameCount) {
        incrementFrameCount();
      }
      
      var esp32Status = app.get('esp32Status');
      esp32Status.connected = true;
      esp32Status.lastUpdate = new Date();
      esp32Status.frameCount = frameCount;
      app.set('esp32Status', esp32Status);

      // Broadcast to all connected WebSocket clients
      var wss = app.get('wss');
      if (wss) {
        var frameMessage = JSON.stringify({
          type: 'frame',
          image: latestFrame.toString('base64'),
          timestamp: Date.now(),
        });
        wss.clients.forEach(function(client) {
          if (client.readyState === 1) { // WebSocket.OPEN = 1
            client.send(frameMessage);
          }
        });
      }

      res.status(200).json({ success: true, frameCount: frameCount });
    } else {
      res.status(400).json({ error: 'Invalid frame data' });
    }
  } catch (error) {
    console.error('Error processing frame:', error);
    res.status(500).json({ error: 'Failed to process frame' });
  }
});

/**
 * Get latest frame (for testing)
 */
router.get('/snapshot', function(req, res) {
  var latestFrame = req.app.get('latestFrame');
  
  if (!latestFrame) {
    return res.status(404).json({ error: 'No frame available' });
  }

  res.set('Content-Type', 'image/jpeg');
  res.send(latestFrame);
});

/**
 * Save captured image and predict disease
 */
router.post('/capture', async function(req, res) {
  try {
    var latestFrame = req.app.get('latestFrame');
    
    if (!latestFrame) {
      return res.status(404).json({ error: 'No frame available' });
    }

    var timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    var filename = 'ESP32_CAM_' + timestamp + '.jpg';
    var uploadsDir = path.join(__dirname, '../uploads');
    var filepath = path.join(uploadsDir, filename);

    // Save the image
    fs.writeFileSync(filepath, latestFrame);

    // Predict disease using ML service
    var prediction = null;
    try {
      console.log('🔍 Sending image to ML service for prediction...');
      
      var formData = new FormData();
      formData.append('image', latestFrame, {
        filename: filename,
        contentType: 'image/jpeg',
      });

      var mlResponse = await axios.post(
        ML_SERVICE_URL + '/predict',
        formData,
        {
          headers: formData.getHeaders(),
          timeout: 10000, // 10 second timeout
        }
      );

      console.log('📊 ML Service Response Status:', mlResponse.status);
      console.log('📦 Full ML Response Data:', JSON.stringify(mlResponse.data, null, 2));

      if (mlResponse.data && mlResponse.data.success) {
        prediction = mlResponse.data.prediction;
        console.log('✅ Prediction extracted:', JSON.stringify(prediction, null, 2));
        console.log('🌱 Disease:', prediction.disease);
        console.log('🎯 Confidence:', (prediction.confidence * 100).toFixed(2) + '%');
        console.log('⚠️  Severity:', prediction.severity);
        console.log('💊 Remedies:', prediction.remedies);
      } else {
        console.log('⚠️  ML service returned unsuccessful response');
      }
    } catch (mlError) {
      console.error('❌ ML Service error:', mlError.message);
      if (mlError.response) {
        console.error('   Response status:', mlError.response.status);
        console.error('   Response data:', mlError.response.data);
      }
      // Continue even if ML service fails
    }

    var response = {
      success: true,
      filename: filename,
      path: filepath,
      url: '/uploads/' + filename,
      size: latestFrame.length,
      timestamp: timestamp,
    };

    // Add prediction if available
    if (prediction) {
      response.disease_prediction = prediction;
      console.log('✨ Final response includes disease prediction!');
    } else {
      console.log('⚠️  No prediction available, sending response without prediction');
    }

    console.log('📤 Sending response to client:', JSON.stringify(response, null, 2));
    res.json(response);
  } catch (error) {
    console.error('Error saving image:', error);
    res.status(500).json({ error: 'Failed to save image' });
  }
});

/**
 * Get saved images list
 */
router.get('/images', function(req, res) {
  try {
    var uploadsDir = path.join(__dirname, '../uploads');
    var files = fs.readdirSync(uploadsDir);
    var images = files
      .filter(function(f) {
        return f.endsWith('.jpg') || f.endsWith('.jpeg');
      })
      .map(function(filename) {
        var stats = fs.statSync(path.join(uploadsDir, filename));
        return {
          filename: filename,
          path: '/uploads/' + filename,
          size: stats.size,
          created: stats.mtime,
        };
      });

    res.json({ images: images });
  } catch (error) {
    console.error('Error listing images:', error);
    res.status(500).json({ error: 'Failed to list images' });
  }
});

/**
 * Delete a saved image
 */
router.delete('/images/:filename', function(req, res) {
  try {
    var filename = req.params.filename;
    var uploadsDir = path.join(__dirname, '../uploads');
    var filepath = path.join(uploadsDir, filename);

    if (fs.existsSync(filepath)) {
      fs.unlinkSync(filepath);
      res.json({ success: true, message: 'Image deleted' });
    } else {
      res.status(404).json({ error: 'Image not found' });
    }
  } catch (error) {
    console.error('Error deleting image:', error);
    res.status(500).json({ error: 'Failed to delete image' });
  }
});

/**
 * Predict disease from a saved image
 */
router.post('/predict/:filename', async function(req, res) {
  try {
    var filename = req.params.filename;
    var uploadsDir = path.join(__dirname, '../uploads');
    var filepath = path.join(uploadsDir, filename);

    if (!fs.existsSync(filepath)) {
      return res.status(404).json({ error: 'Image not found' });
    }

    var imageBuffer = fs.readFileSync(filepath);
    console.log('🔍 Predicting disease for saved image:', filename);

    // Predict disease using ML service
    try {
      var formData = new FormData();
      formData.append('image', imageBuffer, {
        filename: filename,
        contentType: 'image/jpeg',
      });

      var mlResponse = await axios.post(
        ML_SERVICE_URL + '/predict',
        formData,
        {
          headers: formData.getHeaders(),
          timeout: 10000,
        }
      );

      console.log('📊 ML Response for', filename + ':', JSON.stringify(mlResponse.data, null, 2));

      if (mlResponse.data && mlResponse.data.success) {
        console.log('✅ Prediction successful for', filename);
        console.log('🌱 Disease:', mlResponse.data.prediction.disease);
        console.log('🎯 Confidence:', (mlResponse.data.prediction.confidence * 100).toFixed(2) + '%');
        
        res.json({
          success: true,
          filename: filename,
          prediction: mlResponse.data.prediction,
          top_predictions: mlResponse.data.top_predictions,
        });
      } else {
        console.error('❌ Prediction failed for', filename);
        res.status(500).json({ error: 'Prediction failed' });
      }
    } catch (mlError) {
      console.error('❌ ML Service error for', filename + ':', mlError.message);
      if (mlError.response) {
        console.error('   Response status:', mlError.response.status);
        console.error('   Response data:', mlError.response.data);
      }
      res.status(503).json({
        error: 'ML service unavailable',
        message: mlError.message,
      });
    }
  } catch (error) {
    console.error('Error predicting disease:', error);
    res.status(500).json({ error: 'Failed to predict disease' });
  }
});

/**
 * Get ESP32 status
 */
router.get('/status', function(req, res) {
  var esp32Status = req.app.get('esp32Status');
  var wss = req.app.get('wss');
  var connectedClients = wss ? wss.clients.size : 0;
  
  res.json({
    esp32: esp32Status,
    clients: connectedClients,
    uptime: process.uptime(),
  });
});

module.exports = router;

