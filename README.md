Spot
====

This repository is my public playground for experimenting with the libspotify API. Click on a folder to see more about each one.

Note that all of these projects require [libspotify](https://developer.spotify.com/technologies/libspotify/), which in turn requires a Premium Spotify account.

To run one of these, first install libspotify, then create a new source file under src/. This file will need four variables: a spotify key, its size, your username, and your password. Ultimately it should look something like this:

```c
#include <stdint.h>
#include <stdlib.h>

const uint8_t g_appkey[] = ... ;
const size_t g_appkey_size = sizeof(g_appkey);
const char *username = ... ;
const char *password = ... ;
```

After that, run `make` to compile it.
