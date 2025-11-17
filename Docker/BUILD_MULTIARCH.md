# Building Multi-Architecture Docker Images for Altair Everywhere

This guide shows how to build Docker images that work on both x64 (AMD64) and ARM64 architectures using Docker Buildx.

## Prerequisites

- Docker Desktop installed with Buildx support
- Docker Hub account (or another container registry)
- Access to the Altair Everywhere repository

## Step-by-Step Instructions

### 1. Navigate to the Docker Directory

```bash
cd Docker
```

### 2. Create and Configure Multi-Architecture Builder

Create a new buildx builder instance that supports multi-platform builds:

```bash
docker buildx create --name multiarch --use
```

Bootstrap the builder to ensure it's ready:

```bash
docker buildx inspect --bootstrap
```

### 3. Clear Build Cache (Optional)

If you want to start with a clean build cache:

```bash
docker buildx prune -a
```

⚠️ **Warning**: This will remove all build cache and may slow down subsequent builds.

### 4. Login to Docker Registry

Make sure you're authenticated with your container registry:

```bash
docker login
```

### 5. Build Multi-Architecture Image

Replace `YOUR_DOCKER_ID` with your actual Docker Hub username:

```bash
docker buildx build . --platform linux/arm64,linux/amd64 --tag YOUR_DOCKER_ID/altair8800:latest --push
```

This command will:
- Build for both ARM64 and AMD64 architectures
- Tag the image as `latest`
- Push directly to Docker Hub

## Switching Back to Regular Docker Build

### Option 1: Switch to Default Builder

```bash
docker context use default
```

### Option 2: Remove Custom Builder

```bash
docker buildx rm multiarch
```

This automatically switches back to the default builder.

### Verify Current Builder

Check which builder is currently active:

```bash
docker buildx ls
```

The active builder is marked with an asterisk (*).

## Running the Multi-Architecture Image

Once built and pushed, you can run the image on any supported architecture:

```bash
# Basic run
docker run -p 8088:8080 -p 8082:8082 YOUR_DOCKER_ID/altair8800:latest

# With environment variables
docker run -p 8088:8080 -p 8082:8082 \
  -e MQTT_HOST=your-mqtt-broker \
  -e OPEN_WEATHER_MAP_API_KEY=your-api-key \
  YOUR_DOCKER_ID/altair8800:latest
```

## Troubleshooting

### Builder Issues

If you encounter builder issues, try recreating it:

```bash
docker buildx rm multiarch
docker buildx create --name multiarch --use --bootstrap
```

### Platform-Specific Builds

To build for a single platform:

```bash
# ARM64 only
docker buildx build . --platform linux/arm64 --tag YOUR_DOCKER_ID/altair8800:arm64 --push

# AMD64 only
docker buildx build . --platform linux/amd64 --tag YOUR_DOCKER_ID/altair8800:amd64 --push
```

### Local Testing

To build for local testing without pushing:

```bash
docker buildx build . --platform linux/amd64 --tag altair8800:local --load
```

## Environment Variables

The container supports the following environment variables:

| Variable | Description | Example |
|----------|-------------|---------|
| `MQTT_HOST` | MQTT broker hostname | `mqtt.example.com` |
| `MQTT_PORT` | MQTT broker port | `1883` |
| `MQTT_CLIENT_ID` | MQTT client identifier | `altair-001` |
| `MQTT_USERNAME` | MQTT username | `altair` |
| `MQTT_PASSWORD` | MQTT password | `secret123` |
| `NETWORK_INTERFACE` | Network interface to use | `eth0` |
| `OPEN_WEATHER_MAP_API_KEY` | OpenWeatherMap API key | `your-api-key` |
| `COPYX_URL` | CopyX service URL | `http://copyx.example.com` |
| `OPENAI_API_KEY` | OpenAI API key | `sk-...` |

## Ports

The container exposes the following ports:

- **Port 80**: Web terminal interface
- **Port 8082**: Altair emulator interface

Make sure to map these ports when running the container.