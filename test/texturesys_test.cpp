#include <iostream>
#include <string>
#include <OpenImageIO/texture.h>
#include <OpenImageIO/ustring.h>

int main() {
    auto texture_system = OIIO::TextureSystem::create(true);

    OIIO::TextureOpt options;
    float temp_colour[3] = {0.0f, 0.0f, 0.0f};
    
    OIIO::ustring filePath("/home/kazen/dev/nano-kazen/build/test/image.png");
    float s = 0.5f;
    float t = 0.5f; 

    texture_system->texture(
        filePath,
        options,
        s, t,
        0, 0, 0, 0,
        3, &temp_colour[0]);

    std::cout << "temp_colour: " << temp_colour[0] << " " << temp_colour[1] << " " << temp_colour[2] << std::endl;

    return 0;
}