#!/bin/sh

# Start Nginx
echo "Starting Nginx..."
mkdir -p /run/nginx

# Enable autoindex for file listing
# Alpine Nginx uses /etc/nginx/http.d/
mkdir -p /etc/nginx/http.d
cat > /etc/nginx/http.d/default.conf <<EOF
server {
    listen       80;
    server_name  localhost;
    location / {
        root   /usr/share/nginx/html;
        index  index.html index.htm;
        autoindex on;
    }
}
EOF

nginx

# Start RFS Server
echo "Starting RFS Server..."
exec python3 -u remote_fs_server.py --host 0.0.0.0 --port 8085 --clients-dir /app/clients --template-dir /app/disks
