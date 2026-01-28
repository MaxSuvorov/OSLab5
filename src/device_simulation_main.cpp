#include "device_simulation.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string port_name;
    
    if (argc > 1) {
        port_name = argv[1];
    } else {
#ifdef _WIN32
        port_name = "COM3";
        std::cout << "Using default port: COM3" << std::endl;
        std::cout << "Usage: " << argv[0] << " <port_name>" << std::endl;
        std::cout << "Example: " << argv[0] << " COM3" << std::endl;
#else
        port_name = "/dev/ttyS0";
        std::cout << "Using default port: /dev/ttyS0" << std::endl;
        std::cout << "Usage: " << argv[0] << " <port_name>" << std::endl;
        std::cout << "Example: " << argv[0] << " /dev/ttyUSB0" << std::endl;
#endif
    }
    
    DeviceSimulator simulator(port_name, 9600);
    simulator.set_temperature_range(18.0f, 30.0f);
    
    if (!simulator.start()) {
        std::cerr << "Failed to start simulator" << std::endl;
        return 1;
    }
    
    std::cout << "Device simulator running. Press Enter to stop..." << std::endl;
    std::cin.get();
    
    simulator.stop();
    std::cout << "Device simulator stopped." << std::endl;
    
    return 0;
}
