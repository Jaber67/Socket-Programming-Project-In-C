# Socket Programming Project in C

A multi-client TCP chat application written in C using raw BSD sockets. A
threaded server accepts multiple clients at once, assigns each a unique
username, and relays messages between them.

## Features

- TCP client/server chat over raw sockets (`AF_INET`, `SOCK_STREAM`)
- Multi-client support on the server via `pthread`, one thread per client
- Username registration with validation and duplicate-name checking
- Broadcast messaging to all connected clients, plus direct messaging support
  (see `commands.c`/`commands.h` for the full list of `/`-prefixed commands)
- Join/leave announcements broadcast to the room
- Line-buffered reading (`LineReader`) so messages are never split mid-line
  across TCP packets
- Client runs message receiving in a forked child process, so you can type
  and receive messages at the same time
- Windows toast notifications for incoming messages (via `powershell.exe`)

## Project structure

| File           | Description                                             |
|----------------|----------------------------------------------------------|
| `server.c`     | Server entry point: accepts connections, manages clients |
| `server.h`     | Shared server-side types (`Client`, `LineReader`, etc.)   |
| `client.c`     | Client entry point: connects, sends/receives chat lines   |
| `commands.c`   | Handling of `/`-prefixed chat commands                    |
| `commands.h`   | Command handling declarations                             |
| `username.c`   | Username validation logic                                 |
| `username.h`   | Username validation declarations                          |
| `Makefile`     | Build rules for `server` and `client`                     |

## Requirements

- A Unix-like environment (Linux/macOS/WSL) with `gcc` and `make`
- POSIX threads (`pthread`) — used by the server
- The client's Windows-notification feature calls `powershell.exe` and is a
  no-op (silently fails) on non-Windows/WSL systems

## Building

```bash
make
```

This compiles two executables in the project directory: `server` and
`client`. To remove them:

```bash
make clean
```

## Running

1. Start the server (listens on port `8080` by default):

   ```bash
   ./server
   ```

2. In a separate terminal (or on another machine on the same network), start
   a client:

   ```bash
   ./client
   ```

   You'll be prompted for the server's IP address (use `127.0.0.1` if
   running locally) and then a username.

3. Repeat step 2 in more terminals/machines to add more clients to the chat.

4. Type a message and press Enter to broadcast it to everyone connected.
   Type `exit` to leave the chat.

## Configuration

- **Port**: hardcoded to `8080` via `#define PORT 8080` in both `client.c`
  and `server.c`. Change both and rebuild if you need a different port.
- **Max clients**: controlled by `MAX_CLIENTS` in `server.h`.

## Notes

- All chat traffic is sent in plain text and unauthenticated beyond the
  username handshake — this project is intended for learning/demo purposes
  on a trusted network, not production use.
- Firewalls may need to allow inbound TCP connections on port `8080` for
  clients on other machines to connect.