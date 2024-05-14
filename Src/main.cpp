#include "FluidCanvas2D.h"

#include <Althea/Application.h>

#include <iostream>

using namespace AltheaEngine;

int main() {
  Application app("Stable Fluids", "../..", "../../Extern/Althea");
  app.createGame<StableFluids::FluidCanvas2D>();

  try {
    app.run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}