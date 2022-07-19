#include <kazen/common.h>
#include <kazen/scene.h>
#include <kazen/parser.h>
#include <kazen/renderer.h>
#include <filesystem/resolver.h>


#include <xmmintrin.h>
//#include <pmmintrin.h> // use this to get _MM_SET_DENORMALS_ZERO_MODE when compiling for SSE3 or higher

#if !defined(_MM_SET_DENORMALS_ZERO_MODE)
#define _MM_DENORMALS_ZERO_ON   (0x0040)
#define _MM_DENORMALS_ZERO_OFF  (0x0000)
#define _MM_DENORMALS_ZERO_MASK (0x0040)
#define _MM_SET_DENORMALS_ZERO_MODE(x) (_mm_setcsr((_mm_getcsr() & ~_MM_DENORMALS_ZERO_MASK) | (x)))
#endif

using namespace kazen;

int main(int argc, char **argv) {
    /* for best performance set FTZ and DAZ flags in MXCSR control and status register */
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    
    if (argc < 2) {
        cerr << "Syntax: " << argv[0] << " <scene.xml>" <<  endl;
        return -1;
    }

    /* copyRight */
    fmt::print(
        "  __  ___      ___      ________   _______ .__   __.   \n"
        " |  |/  /     /   \\    |       /  |   ____||  \\ |  | \n" 
        " |  '  /     /  ^  \\   `---/  /   |  |__   |   \\|  | \n" 
        " |    <     /  /_\\  \\     /  /    |   __|  |  . `  | \n" 
        " |  .  \\   /  _____  \\   /  /----.|  |____ |  |\\   | \n" 
        " |__|\\__\\ /__/     \\__\\ /________||_______||__| \\__| "   
    );
    std::cout << util::copyright() << '\n';

    /* Parsing scene file path */
    std::string sceneName = "";
    for (int i = 1; i < argc; ++i) {
        filesystem::path path(argv[i]);
        try {
            if (path.extension() == "xml") {
                sceneName = argv[i];

                /* Add the parent directory of the scene file to the
                   file resolver. That way, the XML file can reference
                   resources (OBJ files, textures) using relative paths */
                getFileResolver()->prepend(path.parent_path());
            } else {
                cerr << "Fatal error: unknown file \"" << argv[i]
                     << "\", expected an extension of type .xml or .exr" << endl;
            }
        } catch (const std::exception &e) {
            cerr << "Fatal error: " << e.what() << endl;
            return -1;
        }
    }

    /* Start rendering scene */
    LOG("Rendering scene \"{}\"", sceneName);
    LOG("================");
    if (sceneName =="") {
        cerr << "Please provide the path to a .xml (scene) file." << endl;
        return -1;
    } else {
        try {
            std::unique_ptr<Object> root(loadFromXML(sceneName));
            /* When the XML root object is a scene, start rendering it .. */
            if (root->getClassType() == Object::EScene)
                // std::cout << root->toString() << std::endl;
                renderer::render(static_cast<Scene *>(root.get()), sceneName);
        } catch (const std::exception &e) {
            cerr << e.what() << endl;
            return -1;
        }
    }

    return 0;
}