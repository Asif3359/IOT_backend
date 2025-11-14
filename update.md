# React Native App Update Guide: Socket.IO → WebSocket

## Overview
The backend has been updated from Socket.IO to plain WebSocket for better performance and lower overhead. This guide shows how to update your React Native app.

## Changes Required

### 1. Remove Socket.IO Client

**Before:**
```bash
npm install socket.io-client
```

**After:**
```bash
npm uninstall socket.io-client
```

React Native has built-in WebSocket support - no additional package needed!

---

## 2. Update Connection Code

### Old Code (Socket.IO):
```typescript
import { io } from 'socket.io-client';

const socket = io('http://192.168.0.115:3000', {
  transports: ['websocket'],
  reconnection: true,
});

socket.on('connect', () => {
  console.log('Connected');
});

socket.on('frame', (data) => {
  setFrameData(`data:image/jpeg;base64,${data.image}`);
});

socket.on('status', (status) => {
  setEsp32Status(status);
});
```

### New Code (WebSocket):
```typescript
import { useState, useEffect, useRef } from 'react';

const SERVER_URL = 'ws://192.168.0.115:3000/ws'; // Note: ws:// not http://

function useWebSocket() {
  const [frameData, setFrameData] = useState<string | null>(null);
  const [esp32Status, setEsp32Status] = useState<any>(null);
  const [isConnected, setIsConnected] = useState(false);
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<NodeJS.Timeout | null>(null);

  useEffect(() => {
    connectWebSocket();

    return () => {
      if (wsRef.current) {
        wsRef.current.close();
      }
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current);
      }
    };
  }, []);

  const connectWebSocket = () => {
    try {
      const ws = new WebSocket(SERVER_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        console.log('✅ WebSocket Connected');
        setIsConnected(true);
        
        // Clear any pending reconnection
        if (reconnectTimeoutRef.current) {
          clearTimeout(reconnectTimeoutRef.current);
          reconnectTimeoutRef.current = null;
        }
      };

      ws.onmessage = (event) => {
        try {
          // Server sends JSON messages
          const message = JSON.parse(event.data);
          
          if (message.type === 'frame') {
            // Frame data with base64 image
            setFrameData(`data:image/jpeg;base64,${message.image}`);
          } else if (message.type === 'status') {
            // ESP32 status update
            setEsp32Status(message.data);
          }
        } catch (error) {
          console.error('Failed to parse message:', error);
        }
      };

      ws.onerror = (error) => {
        console.error('❌ WebSocket Error:', error);
        setIsConnected(false);
      };

      ws.onclose = () => {
        console.log('❌ WebSocket Disconnected');
        setIsConnected(false);
        
        // Auto-reconnect after 2 seconds
        reconnectTimeoutRef.current = setTimeout(() => {
          console.log('🔄 Reconnecting...');
          connectWebSocket();
        }, 2000);
      };
    } catch (error) {
      console.error('Failed to create WebSocket:', error);
      setIsConnected(false);
    }
  };

  return { frameData, esp32Status, isConnected, reconnect: connectWebSocket };
}
```

---

## 3. Complete React Native Component Example

```typescript
import React, { useState, useEffect, useRef } from 'react';
import { View, Image, Text, StyleSheet, ActivityIndicator } from 'react-native';

const SERVER_URL = 'ws://192.168.0.115:3000/ws'; // Change to your server IP

interface Esp32Status {
  connected: boolean;
  lastUpdate: string | null;
  frameCount: number;
  fps: number;
}

export default function CameraStream() {
  const [frameData, setFrameData] = useState<string | null>(null);
  const [status, setStatus] = useState<Esp32Status | null>(null);
  const [isConnected, setIsConnected] = useState(false);
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<NodeJS.Timeout | null>(null);

  useEffect(() => {
    connectWebSocket();

    return () => {
      // Cleanup on unmount
      if (wsRef.current) {
        wsRef.current.close();
      }
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current);
      }
    };
  }, []);

  const connectWebSocket = () => {
    try {
      const ws = new WebSocket(SERVER_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        console.log('✅ Connected to WebSocket server');
        setIsConnected(true);
        
        if (reconnectTimeoutRef.current) {
          clearTimeout(reconnectTimeoutRef.current);
          reconnectTimeoutRef.current = null;
        }
      };

      ws.onmessage = (event) => {
        try {
          const message = JSON.parse(event.data);
          
          switch (message.type) {
            case 'frame':
              // Update frame image
              setFrameData(`data:image/jpeg;base64,${message.image}`);
              break;
              
            case 'status':
              // Update ESP32 status
              setStatus(message.data);
              break;
              
            default:
              console.log('Unknown message type:', message.type);
          }
        } catch (error) {
          console.error('Failed to parse message:', error);
        }
      };

      ws.onerror = (error) => {
        console.error('WebSocket error:', error);
        setIsConnected(false);
      };

      ws.onclose = () => {
        console.log('WebSocket closed');
        setIsConnected(false);
        
        // Auto-reconnect
        reconnectTimeoutRef.current = setTimeout(() => {
          console.log('Reconnecting...');
          connectWebSocket();
        }, 2000);
      };
    } catch (error) {
      console.error('Failed to create WebSocket:', error);
      setIsConnected(false);
    }
  };

  return (
    <View style={styles.container}>
      {/* Connection Status */}
      <View style={styles.statusBar}>
        <View style={[styles.statusIndicator, { backgroundColor: isConnected ? '#4CAF50' : '#F44336' }]} />
        <Text style={styles.statusText}>
          {isConnected ? 'Connected' : 'Disconnected'}
        </Text>
        {status && (
          <Text style={styles.statusText}>
            | Frames: {status.frameCount} | FPS: {status.fps.toFixed(1)}
          </Text>
        )}
      </View>

      {/* Camera Frame */}
      {frameData ? (
        <Image
          source={{ uri: frameData }}
          style={styles.cameraFrame}
          resizeMode="contain"
        />
      ) : (
        <View style={styles.placeholder}>
          <ActivityIndicator size="large" color="#2196F3" />
          <Text style={styles.placeholderText}>Waiting for frames...</Text>
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000',
  },
  statusBar: {
    flexDirection: 'row',
    alignItems: 'center',
    padding: 10,
    backgroundColor: '#1a1a1a',
  },
  statusIndicator: {
    width: 10,
    height: 10,
    borderRadius: 5,
    marginRight: 8,
  },
  statusText: {
    color: '#fff',
    fontSize: 12,
  },
  cameraFrame: {
    flex: 1,
    width: '100%',
  },
  placeholder: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  placeholderText: {
    color: '#fff',
    marginTop: 10,
  },
});
```

---

## 4. Key Differences

| Feature | Socket.IO | WebSocket |
|---------|-----------|-----------|
| **Package** | `socket.io-client` | Built-in (no package needed) |
| **Connection** | `io('http://...')` | `new WebSocket('ws://...')` |
| **Events** | `socket.on('frame', ...)` | `ws.onmessage` (parse JSON) |
| **Message Format** | Named events | JSON with `type` field |
| **Reconnection** | Automatic | Manual (shown above) |
| **Binary Data** | Handled automatically | Parse JSON messages |

---

## 5. Message Format

The server sends JSON messages with a `type` field:

### Frame Message:
```json
{
  "type": "frame",
  "image": "base64_encoded_jpeg_string",
  "timestamp": 1234567890
}
```

### Status Message:
```json
{
  "type": "status",
  "data": {
    "connected": true,
    "lastUpdate": "2025-01-14T19:18:15.270Z",
    "frameCount": 141,
    "fps": 2.5
  }
}
```

---

## 6. Testing

1. **Start your backend server:**
   ```bash
   npm run dev
   ```

2. **Update SERVER_URL in your React Native app:**
   ```typescript
   const SERVER_URL = 'ws://YOUR_COMPUTER_IP:3000/ws';
   ```

3. **Run your React Native app:**
   ```bash
   npx react-native run-android
   # or
   npx react-native run-ios
   ```

4. **Expected behavior:**
   - App connects to WebSocket
   - Receives frames and displays them
   - Shows connection status
   - Auto-reconnects if disconnected

---

## 7. Troubleshooting

### Issue: Connection fails
- **Check IP address**: Make sure `SERVER_URL` uses your computer's local IP
- **Check server**: Ensure backend is running on port 3000
- **Check network**: ESP32 and phone must be on same WiFi network

### Issue: No frames received
- **Check ESP32**: Verify ESP32 is connected and sending frames
- **Check server logs**: Look for `📷 ESP32 Camera connected!`
- **Check message parsing**: Add console.log to see received messages

### Issue: Reconnection not working
- **Check timeout**: Adjust reconnection delay (currently 2 seconds)
- **Check cleanup**: Ensure WebSocket is properly closed on unmount

---

## 8. Production Considerations

### For Production (Render.com or similar):
```typescript
// Use wss:// for secure WebSocket
const SERVER_URL = 'wss://your-backend.onrender.com/ws';
```

### Error Handling:
```typescript
ws.onerror = (error) => {
  console.error('WebSocket error:', error);
  // Show user-friendly error message
  Alert.alert('Connection Error', 'Failed to connect to server');
};
```

### Performance Optimization:
- Use `React.memo` for frame rendering component
- Debounce status updates
- Consider using `react-native-fast-image` for better image performance

---

## Summary

✅ **Removed**: `socket.io-client` package  
✅ **Added**: Native WebSocket (built-in)  
✅ **Updated**: Message handling to parse JSON with `type` field  
✅ **Added**: Manual reconnection logic  
✅ **Updated**: Connection URL to use `ws://` protocol  

Your React Native app should now work with the new WebSocket backend!

