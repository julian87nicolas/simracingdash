#pragma once
#include <Arduino.h>
#include "telemetry.h"

// Parse raw UDP packet into TelemetryFrame. Safe: checks bounds before reads.
void telemetry_parse(const uint8_t* buf, size_t len, TelemetryFrame &out);
