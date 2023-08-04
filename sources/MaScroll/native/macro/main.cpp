#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <io.h>
#include <iostream>
#include <sstream>
#include <string>

#include "builtins.h"
#include "exec.h"
#include "util.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <hex username> <script> <input>\n";
        return EXIT_FAILURE;
    }

    lang::util::fast_rng_seed(static_cast<std::uint8_t>(std::time(nullptr)));

    try {
        lang::util::g_username = lang::util::hexdecode(argv[1]);
        auto script = lang::util::read_file(argv[2]);
        auto input = lang::util::read_file(argv[3]);
        lang::exec::Context root_ctx;
        lang::builtins::register_builtins(root_ctx);
        lang::exec::Interpreter interp(script, &root_ctx);
        auto output = interp.run(input);
        _setmode(_fileno(stdout), O_BINARY);
        std::cout << output;
        return EXIT_SUCCESS;
    } catch (const std::exception &exc) {
        std::cerr << "Error: " << exc.what() << "\n";
        return EXIT_FAILURE;
    }
}
