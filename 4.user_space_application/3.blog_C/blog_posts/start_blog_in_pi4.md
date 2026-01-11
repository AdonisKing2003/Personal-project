✅ Step-by-step: Auto-start blog server at boot
Assumptions (adjust if needed)

Binary:

/home/pi/myblog/blog_server


Blog posts directory:

/home/pi/myblog/blog_posts


Port: 8080

User: pi

1️⃣ Make sure binary is executable
chmod +x /home/pi/myblog/blog_server


Test it manually:

/home/pi/myblog/blog_server 8080 /home/pi/myblog/blog_posts


Stop with Ctrl+C after confirming it works.

2️⃣ Create systemd service file
sudo nano /etc/systemd/system/blog-server.service

Paste exactly this
[Unit]
Description=C Markdown Blog Server
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/myblog
ExecStart=/home/pi/myblog/blog_server 8080 /home/pi/myblog/blog_posts
Restart=always
RestartSec=3

# Safety limits (recommended)
Nice=10
MemoryMax=50M
CPUQuota=20%

[Install]
WantedBy=multi-user.target


Save and exit (Ctrl+O, Enter, Ctrl+X).

3️⃣ Reload systemd and enable service
sudo systemctl daemon-reload
sudo systemctl enable blog-server

4️⃣ Start it now (no reboot needed)
sudo systemctl start blog-server

5️⃣ Verify status
sudo systemctl status blog-server


You should see:

Active: active (running)

6️⃣ Test in browser

From Pi:

http://localhost:8080


From another device:

http://<PI_IP>:8080


Find IP:

hostname -I

🔄 Reboot test (important)
sudo reboot


After boot:

systemctl status blog-server


✔ If running → auto-start is working.

📜 View logs (very useful)
sudo journalctl -u blog-server -f

🧠 Common mistakes (avoid these)

❌ Using rc.local
❌ Running as root
❌ Hardcoding relative paths
❌ Forgetting daemon-reload

You did none of those 👍

🛡️ Optional hardening (recommended if internet-exposed)
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full
ProtectHome=true


(Add inside [Service])

✅ Final result

Starts automatically at boot

Restarts if it crashes

Limited CPU & memory

Does not slow down Pi 4

Production-grade setup

If you want next:

nginx reverse proxy

HTTPS (Let’s Encrypt)

Rate limiting

Markdown cache

Graceful shutdown handler


---
✅ Step-by-step: Auto-start blog server at boot
Assumptions (adjust if needed)

Binary:

/home/pi/myblog/blog_server


Blog posts directory:

/home/pi/myblog/blog_posts


Port: 8080

User: pi

1️⃣ Make sure binary is executable
chmod +x /home/pi/myblog/blog_server


Test it manually:

/home/pi/myblog/blog_server 8080 /home/pi/myblog/blog_posts


Stop with Ctrl+C after confirming it works.

2️⃣ Create systemd service file
sudo nano /etc/systemd/system/blog-server.service

Paste exactly this
[Unit]
Description=C Markdown Blog Server
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/myblog
ExecStart=/home/pi/myblog/blog_server 8080 /home/pi/myblog/blog_posts
Restart=always
RestartSec=3

# Safety limits (recommended)
Nice=10
MemoryMax=50M
CPUQuota=20%

[Install]
WantedBy=multi-user.target


Save and exit (Ctrl+O, Enter, Ctrl+X).

3️⃣ Reload systemd and enable service
sudo systemctl daemon-reload
sudo systemctl enable blog-server

4️⃣ Start it now (no reboot needed)
sudo systemctl start blog-server

5️⃣ Verify status
sudo systemctl status blog-server


You should see:

Active: active (running)

6️⃣ Test in browser

From Pi:

http://localhost:8080


From another device:

http://<PI_IP>:8080


Find IP:

hostname -I

🔄 Reboot test (important)
sudo reboot


After boot:

systemctl status blog-server


✔ If running → auto-start is working.

📜 View logs (very useful)
sudo journalctl -u blog-server -f

🧠 Common mistakes (avoid these)

❌ Using rc.local
❌ Running as root
❌ Hardcoding relative paths
❌ Forgetting daemon-reload

You did none of those 👍

🛡️ Optional hardening (recommended if internet-exposed)
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full
ProtectHome=true


(Add inside [Service])

✅ Final result

Starts automatically at boot

Restarts if it crashes

Limited CPU & memory

Does not slow down Pi 4

Production-grade setup

If you want next:

nginx reverse proxy

HTTPS (Let’s Encrypt)

Rate limiting

Markdown cache

Graceful shutdown handler