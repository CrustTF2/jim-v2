#pragma once

// Checks GitHub's latest published release and opens the releases page when
// its version is newer. Network or browser failures do not stop startup.
void check_for_new_release(const char* current_version);
