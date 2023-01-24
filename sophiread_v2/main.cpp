// testing entry file
#include <iostream>
#include <fstream>
#include "tpx3.h"

int main(int argc, char *argv[])
{
    auto input_tpx3 = std::string(argv[1]);
    std::cout << "Processing " << input_tpx3 << std::endl;
    auto hits = readTimepix3RawData(input_tpx3);

    std::ofstream xy_file("sophiread_v2_converted.txt");
    for (auto hit : hits) {
        xy_file << hit.toString() << std::endl;
    }
    return 0;
}
