#include "core/Kernel.hpp"
#include "ui/Shell.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    bool headless = false;
    for (int i = 1; i < argc; ++i)
        if (argv[i] && std::string(argv[i]) == "--headless") headless = true;

    std::cout << "\n========================================\n"
              << "     VELORAOS 0.6  AlphaBuild\n"
              << "     OpenGL UI · Vulkan detect · VelFS\n"
              << "========================================\n\n";

    velora::core::Kernel kernel;
    if (!kernel.initialize()) return 1;
    if (headless) { kernel.shutdown(); return 0; }

    velora::ui::Shell shell(kernel);
    if (!shell.init("VeloraOS")) return 1;
    int code = shell.run();
    kernel.shutdown();
    return code;
}
