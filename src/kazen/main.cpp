#include <kazen/common.h>
#include <kazen/scene.h>
#include <kazen/parser.h>
#include <kazen/renderer.h>
#include <filesystem/resolver.h>

using namespace kazen;

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Syntax: " << argv[0] << " <scene.xml>" <<  endl;
        return -1;
    }

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
    if (sceneName =="") {
        cerr << "Please provide the path to a .xml (scene) file." << endl;
        return -1;
    } else {
        try {
            std::unique_ptr<Object> root(loadFromXML(sceneName));
            /* When the XML root object is a scene, start rendering it .. */
            if (root->getClassType() == Object::EScene)
                std::cout << root->toString() << std::endl;
                renderer::render(static_cast<Scene *>(root.get()), sceneName);
        } catch (const std::exception &e) {
            cerr << e.what() << endl;
            return -1;
        }
    }

    return 0;
}