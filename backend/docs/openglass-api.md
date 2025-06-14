# OmiGlass API Documentation

## Overview

The OmiGlass API provides dedicated endpoints for processing OmiGlass device images with AI-powered description generation and seamless conversation integration. This API has been separated from the general chat API to provide specialized functionality for OmiGlass devices.

## Migration Notice

**⚠️ Breaking Change**: OmiGlass image uploads have been moved from `/v2/files` to a dedicated endpoint.

### Before (Deprecated)
```bash
POST /v2/files  # Mixed chat files AND OmiGlass images
```

### After (Current)
```bash
POST /v2/files              # Chat files only
POST /omiglass/v1/images    # OmiGlass images only
```

## Endpoints

### POST /omiglass/v1/images

Process OmiGlass images with AI-powered description generation and conversation integration.

#### Features
- 🤖 **AI Image Descriptions**: Uses GPT-4o Vision to generate detailed descriptions
- 🚫 **Duplicate Detection**: Automatically identifies and removes duplicate images  
- ☁️ **Cloud Storage**: Uploads images and generates thumbnails in cloud storage
- 💬 **Conversation Integration**: Links images to existing conversations or creates new ones
- ⚡ **Concurrent Processing**: Processes multiple images in parallel for performance

#### Request

**Headers:**
- `Authorization: Bearer {token}` (required)
- `Content-Type: multipart/form-data`

**Body:**
- `files`: Array of image files (multipart/form-data)
  - Must contain 'omiglass' in filename (case-insensitive)
  - Supported formats: JPEG, JPG, PNG
  - Recommended size: < 10MB per image

#### Response

**Success (200):**
```json
[
  {
    "id": "omiglass_1703123456789",
    "name": "omiglass_1703123456789.jpg", 
    "description": "A person sitting at a desk with a laptop, typing in what appears to be an office environment. There are books visible on shelves in the background.",
    "mime_type": "application/octet-stream",
    "created_at": "2023-12-21T10:30:56.789Z",
    "thumbnail": "https://storage.googleapis.com/...",
    "url": "https://storage.googleapis.com/...",
    "is_interesting": true,
    "upload_success": true,
    "linked_conversation_id": "conv_abc123"
  }
]
```

**Error (400) - Invalid Files:**
```json
{
  "detail": "Non-OmiGlass files detected: ['regular_file.jpg']. Use /v2/files for regular file uploads."
}
```

**Error (500) - Processing Failed:**
```json
{
  "detail": "Failed to process OmiGlass images"
}
```

## Usage Scenarios

### 1. Audio + Images
Images are automatically added to existing audio conversations when a user is actively recording.

### 2. Image-only Sessions  
Creates new conversations for standalone image uploads when no active conversation exists.

### 3. Extended Sessions
Adds images to existing image-only conversations within the timeout window.

## Client Integration

### Flutter/Dart Example

**Before (Deprecated):**
```dart
// Old approach - mixed endpoint
final response = await dio.post(
  '${baseUrl}v2/files',
  data: formData,
  options: Options(headers: headers),
);
```

**After (Current):**
```dart
// New approach - dedicated endpoint
final response = await dio.post(
  '${baseUrl}omiglass/v1/images',  // ✅ Use dedicated endpoint
  data: formData,
  options: Options(headers: headers),
);
```

### JavaScript/TypeScript Example

```typescript
const uploadOmiGlassImages = async (files: File[], authToken: string) => {
  const formData = new FormData();
  
  files.forEach(file => {
    formData.append('files', file);
  });
  
  const response = await fetch('/omiglass/v1/images', {
    method: 'POST',
    headers: {
      'Authorization': `Bearer ${authToken}`,
    },
    body: formData,
  });
  
  if (!response.ok) {
    throw new Error(`Upload failed: ${response.statusText}`);
  }
  
  return await response.json();
};
```

### Python Example

```python
import requests

def upload_omiglass_images(files, auth_token, base_url):
    headers = {
        'Authorization': f'Bearer {auth_token}'
    }
    
    files_data = [
        ('files', (file.name, file, 'image/jpeg')) 
        for file in files
    ]
    
    response = requests.post(
        f'{base_url}/omiglass/v1/images',
        headers=headers,
        files=files_data
    )
    
    response.raise_for_status()
    return response.json()
```

## Error Handling

### File Validation Errors
- **Non-OmiGlass files**: Files without 'omiglass' in filename are rejected
- **No files provided**: Empty request returns 400 error
- **Invalid file format**: Unsupported formats are handled gracefully

### Processing Errors
- **AI description failures**: Falls back to default descriptions
- **Upload failures**: Images are still processed with descriptions
- **Conversation integration failures**: Logged but don't block response

## Performance Considerations

- **Concurrent Processing**: Up to 3 images processed simultaneously
- **Timeout Handling**: 60-second total timeout, 30-second per image
- **Duplicate Detection**: Uses Jaccard similarity with 0.9 threshold
- **Caching**: 5-minute cache prevents reprocessing identical images

## Architecture Benefits

### Separation of Concerns
- **Clean API Design**: OmiGlass images have dedicated endpoint
- **Dedicated Storage**: OmiGlass images stored in separate bucket from chat files
- **Specialized Processing**: AI descriptions, duplicate detection, conversation integration
- **Independent Scaling**: OmiGlass endpoint can be scaled independently

### Backward Compatibility
- **Graceful Migration**: Clear error messages guide users to correct endpoint
- **Chat API Unchanged**: Regular file uploads continue using `/v2/files`
- **Type Safety**: Dedicated response models for better API documentation

## Testing

### Manual Testing
```bash
# Test OmiGlass image upload
curl -X POST \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -F "files=@omiglass_123456789.jpg" \
  https://api.omi.com/omiglass/v1/images

# Test error handling with non-OmiGlass file
curl -X POST \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -F "files=@regular_image.jpg" \
  https://api.omi.com/omiglass/v1/images

# Test health check
curl -X GET \
  -H "Authorization: Bearer YOUR_TOKEN" \
  https://api.omi.com/omiglass/health

# Test manual cleanup
curl -X POST \
  -H "Authorization: Bearer YOUR_TOKEN" \
  https://api.omi.com/omiglass/cleanup
```

### Expected Behaviors
- ✅ OmiGlass files process successfully with AI descriptions
- ✅ Non-OmiGlass files return clear error messages
- ✅ Duplicate images are automatically filtered
- ✅ Images integrate with active conversations
- ✅ Thumbnails and full-size URLs are generated
- ✅ System remains stable under high load
- ✅ Graceful degradation during service failures

## 24-Hour Operation Robustness

### **Critical Robustness Features**

The OmiGlass API is designed for continuous 24-hour operation with comprehensive robustness features:

#### **1. Memory Management & Resource Cleanup**
- **Bounded Cache**: Image processing cache limited to 1,000 entries with automatic cleanup
- **Shared Thread Pool**: Adaptive worker count with proper lifecycle management
- **Resource Cleanup**: Automatic cleanup on module unload and periodic maintenance
- **Memory Leak Prevention**: Thread-safe operations with proper resource disposal

#### **2. Error Handling & Recovery**
- **Circuit Breaker Pattern**: Automatic service isolation during failures
  - OpenAI API: 3 failures trigger 2-minute circuit break
  - Storage: 5 failures trigger 1-minute circuit break
- **Exponential Backoff**: Intelligent retry logic with increasing delays
- **Graceful Degradation**: Continue processing with fallback descriptions when AI fails
- **Error Isolation**: Individual image failures don't affect batch processing

#### **3. Concurrency & Performance**
- **Backpressure Handling**: Reject requests with >50 images to prevent overload
- **Adaptive Processing**: CPU-based worker count scaling
- **Timeout Management**: 
  - Total batch: 2 minutes
  - Per image: 45 seconds
  - Upload: 30 seconds per file
- **Progress Tracking**: Detailed logging for long-running batches

#### **4. Session Management**
- **TTL Management**: Automatic expiration of Redis keys
- **Stale Session Cleanup**: Remove sessions older than 24 hours
- **Session Validation**: Robust parsing with error recovery
- **State Synchronization**: Thread-safe conversation state management

#### **5. Storage & Database**
- **Dedicated Bucket**: Separate OmiGlass storage from chat files
- **Connection Pooling**: Efficient database connection reuse
- **Bulk Operations**: Optimized for large image sets
- **Cleanup Procedures**: Remove failed uploads and orphaned data

#### **6. Health Monitoring**
- **Health Check Endpoint**: `/omiglass/health` - Real-time service status
- **Service Monitoring**: Redis, OpenAI, Storage, Thread Pool, Cache
- **Circuit Breaker Status**: Monitor service isolation states
- **Performance Metrics**: Cache size, active threads, response times

### **Production Deployment Considerations**

#### **Required Environment Variables**
```bash
# Storage
BUCKET_OMIGLASS=your-omiglass-bucket-name
BUCKET_CHAT_FILES=your-chat-files-bucket-name

# OpenAI
OPENAI_API_KEY=your-openai-api-key

# Redis (with connection pooling)
REDIS_DB_HOST=your-redis-host
REDIS_DB_PORT=6379
REDIS_DB_PASSWORD=your-redis-password
```

#### **Monitoring Setup**
1. **Health Checks**: Monitor `/omiglass/health` every 30 seconds
2. **Log Monitoring**: Watch for circuit breaker state changes
3. **Resource Usage**: Monitor CPU, memory, and Redis usage
4. **Error Rates**: Track API failures and recovery patterns

#### **Maintenance Tasks**
```bash
# Periodic cleanup (run every hour via cron)
curl -X POST https://api.omi.com/omiglass/cleanup

# Monitor Redis health
redis-cli info memory
redis-cli info clients

# Check circuit breaker states
curl https://api.omi.com/omiglass/health | jq '.services'
```

#### **Performance Tuning**
- **Worker Count**: Adjust based on CPU cores and load patterns
- **Cache Size**: Increase `_MAX_CACHE_SIZE` for high-volume deployments
- **Timeout Values**: Tune based on network latency and processing requirements
- **Circuit Breaker Thresholds**: Adjust failure counts based on service reliability

### **Disaster Recovery**

#### **Service Failure Scenarios**
1. **OpenAI API Down**: System continues with fallback descriptions
2. **Storage Unavailable**: Images processed but not uploaded (retry later)
3. **Redis Down**: Direct database fallback for conversation state
4. **High Load**: Backpressure rejection with clear error messages

#### **Recovery Procedures**
1. **Circuit Breaker Reset**: Automatic after 5-minute cooldown
2. **Failed Upload Retry**: Manual cleanup endpoint available
3. **Cache Rebuild**: Automatic on service restart
4. **Session Recovery**: Stale session cleanup and rebuild

### **Load Testing Recommendations**

```bash
# Test concurrent image uploads
for i in {1..10}; do
  curl -X POST \
    -H "Authorization: Bearer $TOKEN" \
    -F "files=@test_omiglass_$i.jpg" \
    https://api.omi.com/omiglass/v1/images &
done

# Monitor health during load
watch -n 5 'curl -s https://api.omi.com/omiglass/health | jq .status'
```

## Performance Characteristics

- **Throughput**: 50+ concurrent images per request
- **Latency**: <45 seconds per image (including AI processing)
- **Memory Usage**: Bounded cache prevents memory leaks
- **CPU Usage**: Scales with available cores
- **Network**: Handles intermittent connectivity issues
- **Storage**: Resilient to temporary storage failures

The system is designed to maintain these characteristics continuously for 24+ hour operation periods.

## Monitoring and Debugging

### Logs to Monitor
- `Successfully uploaded OmiGlass image to cloud`
- `Error processing OmiGlass images: {error}`
- `BLOCKING image upload - conversation completed`
- `Received description for image {id}: {description}`

### Common Issues
1. **Missing API Key**: Check `OPENAI_API_KEY` environment variable
2. **Storage Failures**: Monitor cloud storage bucket permissions
3. **Conversation Linking**: Check Redis connection for active conversation tracking
4. **Duplicate Detection**: Monitor similarity threshold effectiveness

## Storage Architecture

### Dedicated Bucket Separation
OmiGlass images are stored in a **dedicated bucket** separate from chat files to ensure:

- **Data Organization**: Clear separation between chat files and device images
- **Access Control**: Different permissions and policies for different data types
- **Cost Management**: Separate billing and storage optimization policies
- **Backup Strategies**: Independent backup and retention policies

### Storage Structure
```
BUCKET_OMIGLASS/
├── {uid}/
│   ├── images/
│   │   ├── omiglass_1703123456789.jpg
│   │   └── omiglass_1703123456790.jpg
│   └── thumbnails/
│       ├── uuid_thumb_omiglass_1703123456789.jpg
│       └── uuid_thumb_omiglass_1703123456790.jpg
```

### Environment Configuration
```bash
# Required: Dedicated OmiGlass bucket
BUCKET_OMIGLASS=your-omiglass-bucket-name

# Chat files use separate bucket
BUCKET_CHAT_FILES=your-chat-files-bucket-name
```

## Security Considerations

- **Authentication Required**: All endpoints require valid bearer token
- **File Validation**: Only processes files with 'openglass' in filename
- **Rate Limiting**: Inherits from FastAPI middleware settings
- **Signed URLs**: Cloud storage URLs expire after 24 hours
- **Privacy**: AI descriptions are processed securely through OpenAI API
- **Bucket Isolation**: OmiGlass images isolated from other data types 