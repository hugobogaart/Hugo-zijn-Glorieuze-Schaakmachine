#include <iostream>
#include <string>
#include "../unit-tests/unit-tests.h"
#include "cli/UCI.h"

#include <fstream>


// RAII for redirection
class Redirect {
public:

        explicit Redirect(const std::string& filenm):
            _coutbuf{ std::cerr.rdbuf() },   // save original rdbuf
            _outf{ filenm }
        {
                // replace cout's rdbuf with the file's rdbuf
                std::cerr.rdbuf(_outf.rdbuf());
        }

        ~Redirect() {
                // restore cout's rdbuf to the original
                std::cerr << std::flush;
                _outf.close();    ///< not really necessary
                std::cerr.rdbuf(_coutbuf);
        }

private:

        std::streambuf *_coutbuf;
        std::ofstream _outf;
};

auto bot () -> void
{
        while (true) {
                std::string str;
                std::getline(std::cin, str);
                if (str == "uci") {
                        uci();
                        return;
                } else {
                        std::cerr << "Unknown command: \"" << str << "\"\n";
                }
        }
}


int main(int argc, char **argv)
{

        // Redirect redirect("engine_error_output.txt");
        run_tests();
        bot();
        return EXIT_SUCCESS;
}