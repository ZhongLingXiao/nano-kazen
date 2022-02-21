#include <iostream>
#include <string>
#include <OpenImageIO/texture.h>
#include <OpenImageIO/ustring.h>

int main() {
    // create a texture system
    auto ts = OIIO::TextureSystem::create(true);
    int maxfiles = 50;
    int maxMemoryMB = 1024.0f;
    ts->attribute ("max_open_files", maxfiles);
    ts->attribute ("max_memory_MB", maxMemoryMB);

    // load a texture
    OIIO::TextureOpt options;
    OIIO::ustring filePath("/home/kazen/dev/nano-kazen/build/test/image.png");
    float s = 0.5f;
    float t = 0.5f; 
    float color[3] = {0.0f, 0.0f, 0.0f};

    ts->texture(
        filePath,
        options,
        s, t,
        0, 0, 0, 0,
        3, &color[0]);

    std::cout << "[color]: " << color[0] << " " << color[1] << " " << color[2] << std::endl;

    // destroy the texture system
    OIIO::TextureSystem::destroy(ts);

    return 0;
}