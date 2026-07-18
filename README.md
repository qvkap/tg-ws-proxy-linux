# Telegram WebSocket Proxy (C Edition)

A high-performance, incredibly lightweight, and robust C-port of the original Python `tg-ws-proxy`. It masks MTProto traffic as standard HTTPS traffic via Cloudflare Workers (WebSocket over TLS), allowing you to bypass DPI and strict censorship completely.

Unlike the Python version which requires megabytes of RAM and heavy interpreters, this C version is lightning fast and uses only a few megabytes, making it ideal for weak VPS servers or home routers (OpenWrt).

## Features

- **Ultra-low footprint**: Uses minimal RAM and CPU because it is written in pure C.
- **Protocol Multiplexing**: Automatically detects the connection type by inspecting the first byte. 
  - Routes `SOCKS5` traffic to a direct proxy tunnel.
  - Routes `MTProto FakeTLS` traffic through a highly secure Cloudflare WebSockets (`WSS`) tunnel.
- **Dynamic Worker Rotation**: Contains an embedded list of 20 encrypted Cloudflare Worker domains. It automatically decrypts them and randomly selects a different domain for each connection to balance the load and prevent blocking.
- **Auto-Installation (Init Scripts)**: Fully automates the creation of system services. Supports `Systemd`, `OpenRC`, and `Dinit`.
- **Daemon Mode**: Can detach from the console and run in the background with full `syslog` integration.
- **Automatic Secret Generation**: Automatically generates a secure 16-byte cryptographic secret on the first run and saves it to `config.json`.

## Building from Source

Simply clone the repository (if you haven't) and run `make`:

```bash
cd tg-ws-proxy-linux
make
```

This will produce the `tg-ws-proxy` executable.

## Usage

### Standard execution
To run the proxy in the foreground on port `8080`:
```bash
./tg-ws-proxy 8080
```
On the first run, the program will generate a unique `config.json` file with your secret. The exact connection links (both MTProto and SOCKS5) tailored for your server will be printed directly to your console.

### Daemon Mode
To run the proxy silently in the background:
```bash
./tg-ws-proxy 8080 --daemon
```
When running in daemon mode, all output is seamlessly redirected to your system's `syslog`. You can read the generated links and connection logs using:
```bash
journalctl -t tg-ws-proxy -f
```

## Autostart Configuration

The proxy comes with a smart auto-installation feature. It will automatically detect your Linux init system (Systemd, OpenRC, or Dinit) and install the appropriate service.

Run the following command:
```bash
./tg-ws-proxy --autostart
```
*Note: The program will intelligently request root privileges via `sudo` or `doas` if it wasn't started as root.*

Once installed, the service will automatically start on system boot.

## Internal Structure
- `src/main.c`: Core server, multiplexer, SOCKS5 handler, and dynamic domain decrypter.
- `src/ws_client.c`: Custom WebSocket over TLS implementation built on top of OpenSSL.
- `include/ws_client.h`: Header for the WSS engine.
- `config.json`: Persists your proxy secret. Automatically generated.
