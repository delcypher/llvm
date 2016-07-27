// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include <stdint.h>
#include <stdlib.h>

static volatile int Sink;

// This is a simple function that will never abort.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (Size > 0 && Data[0] == 'H') {
    Sink = 1;
    if (Size > 1 && Data[1] == 'i') {
      Sink = 2;
      if (Size > 2 && Data[2] == '!') {
        Sink = 3;
        return 0;
      }
    }
  }
  return 0;
}

