#pragma once
#include <atomic>

// Runs within Item Manager. Returns 2 when cancellation was requested.
int run_thumbnail_batch(const std::atomic<bool>& cancelled);
