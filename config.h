#pragma once

// Your own settings go in config.local.h (git-ignored), for example:
//
//   #define NAMETAG_NAME   "Ada"
//   #define NAMETAG_QR_URL "https://linkedin.com/in/ada"
//
// Anything you don't define there falls back to the defaults below.
#if __has_include("config.local.h")
#include "config.local.h"
#endif

// Text shown on the name screen. Short names look best: it's scaled to fit.
#ifndef NAMETAG_NAME
#define NAMETAG_NAME "Your Name"
#endif

// What the QR code on the second screen points to (any URL or text).
#ifndef NAMETAG_QR_URL
#define NAMETAG_QR_URL "https://example.com"
#endif

// If the content turns the wrong way when you tilt the device, set this to -1.
#ifndef NAMETAG_ROTATION_DIRECTION
#define NAMETAG_ROTATION_DIRECTION 1
#endif
