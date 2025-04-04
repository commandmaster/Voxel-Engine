#include <EntryPoint.h>
#include <VoxelEngine.h>

Application* CreateApplication()
{
	return new VoxelEngine();
}