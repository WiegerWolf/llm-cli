#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string name = "World";
    
    if(argc > 1) {
        name = argv[1];
    }
    
    std::cout << "Hello " << name << "!\n";
    std::cout << "Arguments received: " << argc - 1 << std::endl;
    
    return 0;
}
