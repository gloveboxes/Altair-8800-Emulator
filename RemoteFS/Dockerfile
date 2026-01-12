FROM python:3.11-alpine

WORKDIR /app

# Create directories
RUN mkdir -p /app/clients /app/disks /app/files

# Copy server scripts
COPY RemoteFS/remote_fs_server.py /app/
COPY RemoteFS/remote_ft_server.py /app/

# Copy disk templates
COPY disks/*.dsk /app/disks/

# Expose ports
EXPOSE 8085 8090 80

# Install Nginx
RUN apk add --no-cache nginx

# Copy entrypoint
COPY RemoteFS/entrypoint.sh /app/
RUN chmod +x /app/entrypoint.sh

# Run via entrypoint
CMD ["/app/entrypoint.sh"]
