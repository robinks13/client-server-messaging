# Client–Server Messaging Application

A command-line messaging application built in C with TCP/IP sockets. The server uses poll() to handle multiple connected clients in one process. Clients can exchange private and broadcast messages, manage nicknames, and upload or download files through the server.

> Pair project by Robin Bouvier and Ugo Sancho.

## Features

- TCP client and server using POSIX sockets and getaddrinfo().
- Multiplexed I/O with poll() to handle several clients without one process per connection.
- User login, nickname changes, online-user listing, and conversation history.
- Private messages and broadcasts to all connected users.
- File upload, listing, and download through the server.
- A shared message structure carrying message type, sender, metadata, and payload.

## Project layout

src/ contains the interactive client, multi-client server, shared socket and file-transfer helpers, and message protocol definitions. The Makefile builds the client and server. Runtime folders are created by make and excluded from Git.

## Requirements

- POSIX-compatible system (Linux or macOS)
- C compiler
- make

## Build and run

    make

Start the server in one terminal:

    ./server 8080

Connect a client from another terminal:

    ./client 127.0.0.1 8080

At the login prompt, use /login <nickname> <password>. Type /listcmd in the client to see the commands supported by the running server. For a local demonstration, connect multiple clients to the same server.

Stop the server with Ctrl+C and remove compiled binaries with make clean.

## Design notes

The server listens for TCP connections and uses a poll() loop to watch its listening socket and client sockets. Both programs use shared helpers to exchange a fixed-size message header followed by its payload. File uploads are saved by the server and can later be listed and downloaded by clients.

## Scope

This is an educational pair project, not a production messaging service. It has not been security-audited. Do not use real passwords, sensitive messages, or private files.

## Français

Application de messagerie en ligne de commande, écrite en C avec des sockets TCP/IP. Le serveur utilise poll() pour gérer plusieurs clients dans un même processus. Le projet comprend des messages privés et collectifs, la gestion des pseudos, l’historique des conversations et l’envoi ou le téléchargement de fichiers par le serveur.

Projet réalisé en binôme par Robin Bouvier et Ugo Sancho.
