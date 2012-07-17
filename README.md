Spot
====

This repository is my public playground for experimenting with the libspotify API. Currently there's only one project in it (hereby dubbed 'spot'), but there will likely be more in the future.

Spot is a command-line tool for playing music through Spotify. It currently serves only one purpose: to search for a track by artist and track name, then play it. Obviously it requires [libspotify](https://developer.spotify.com/technologies/libspotify/), which in turn requires a Premium Spotify account.

To run it, first install libspotify, then create a new source file under src/. This file will need four variables: a spotify key, its size, your username, and your password. Ultimately it should look something like this:

```c
#include <stdint.h>
#include <stdlib.h>

const uint8_t g_appkey[] = ... ;
const size_t g_appkey_size = sizeof(g_appkey);
const char *username = ... ;
const char *password = ... ;
```

After that, run `make` to compile it and `./spot` to run it.

NOTE: if you get random errors, remove the local "tmp" directory and try again. For some reason the session caching breaks every now and then.
