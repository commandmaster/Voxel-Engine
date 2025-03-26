#include "VoxelEngine.hpp"

int main() 
{
    VoxelEngine& app = VoxelEngine::getInstance();

    try 
    {
        app.run();
    } 
    catch (const std::exception& e) 
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}