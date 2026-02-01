# 1. Setup Wifi connect automatically (Yocto/Linux)

1. Create config file:

```bash
vi /etc/wpa_supplicant.conf
```

2. Add wifi information

```bash
ctrl_interface=/var/run/wpa_supplicant
update_config=1
network={
    ssid="Your Wifi name"
    psk="Wifi password"
}
```

3. Setup wifi start with the system
```bash
systemctl enable wpa_supplicant
```

4. Setup Static IP for Pi4 on the Router (Static DHCP) (Option)
- Go to the Router, find Static DHCP or Address Reservation or DHCP Reservation. Enter the MAC Address of Pi4 and static IP you want for Pi4 (example: 192.168.1.115).

# 2. Setup Tailscale in Raspberry Pi 4 (Yocto)

1. Config Service
   
Config file in: /lib/systemd/system/tailscaled.service

```bash
[Service]
ExecStart=/usr/bin/tailscaled --tun=user-networking --state=/home/root/tailscaled.state
Restart=on-failure
```

2. Start tailscale first time
```bash
# 1. Update new config
systemctl daemon-reload
systemctl enable tailscaled
systemctl start tailscaled

# 2. Sign in with Auth Key (get from Admin Tailscale)
# Use --accept-dns=false for avoid error DNS on Yoccto
tailscale up --authkey=tskey-auth-xxxx --accept-dns=false
```

3. Check for save data for tailscale after reboot
- Check the saved file (must have size > 0):
```bash
ls -l /home/root/tailscaled.state
```
- Restart service:
```bash
systemctl restart tailscaled
```
- Check the tailscale state (must show the ip of the pi4 without login again):
```bash
tailscale status
```

4. Setup Tailscale for Laptop (Client)

- Download application
    - Windows/macOS: download at: https://tailscale.com/download.
    - Linux (Ubuntu/Debian): curl -fsSL https://tailscale.com/install.sh | sh
- Login:
    - Must login the account use for get Auth Key for pi4.
- Connect:
  - After login, laptop will get the IP with the format: 100.x.y.z
  - Open terminal and check: `tailscale status`
  - Then, you will see the `raspberrypi4-64` and it's IP.

5. Use daily:
- Both the Laptop and Pi4 open the Tailscale, then you can connect to Pi4 when use the different network.
- Note: When rebuild the Pi, go to the Admin Tailscale and delete the old machine information.