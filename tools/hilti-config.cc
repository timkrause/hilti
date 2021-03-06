///
/// Outputs paths and flags for using HILTI.
///
/// TODO: Currently we do not support installation outside of the built-tree
/// and all values returned here are thus in-tree.
///

#include <iostream>
#include <list>
#include <sstream>
#include <string>

#include <util/util.h>

#include <autogen/hilti-config.h>

using namespace std;

void usage()
{
    std::cerr << R"(
Usage: hilti-config [options]

== General options.

    --distbase              Print path of the HILTI source distribution.
    --prefix                Print path of installation (TODO: same as --distbase currently)
    --version               Print HILTI version.
    --build                 Prints "debug" or "release", depending on the build configuration.
    --help                  Print this usage summary

== Options controlling what to include in output.

    --compiler              Output flags for integrating the C++ compilers into host applications.
    --runtime               Output flags for integrating the C runtime libraries into host applications.

    --debug                 Output flags for working with debugging versions.

== Compiler and linker flags

    --cflags                Print flags for C compiler (--runtime only).
    --cxxflags              Print flags for C++ compiler.
    --ldflags               Print flags for linker.
    --libs                  Print libraries for linker.
    --libs-shared           Print shared libraries for linker.
    --libfiles-static       Print fully qualified paths to all static libraries.

== Options for running the command-line compilers.

    --hiltic-binary         Print the full path to the hiltic binary.
    --hilti-libdirs         Print standard HILTI library directories.
    --hilti-runtime-libs    Print the full path of the HILTI run-time library.

== Enviroment information

    --clang                 Print the full path to the clang used to compile HILTI.
    --clang++               Print the full path to the clang used to compile HILTI.
    --llvm-config           Print the full path to the llvm-config used to compile HILTI.

Example: hilti-config --compiler --cxxflags --debug

)";
}

void printList(stringstream& out, const std::list<string>& l)
{
    out << util::strjoin(l, " ") << " ";
}

void appendList(std::list<string>* dst, const std::list<string>& l, const string& prefix = "")
{
    for ( auto i : l )
        dst->push_back(prefix + i);
}

void appendList(std::list<string>* dst, const string& s, const string& prefix = "")
{
    dst->push_back(prefix + s);
}

void appendListFile(std::list<string>* dst, const string& s, const std::list<string>& search_paths,
                    const string& prefix = "", const string& postfix = "")
{
    auto base = prefix + s + postfix;

    auto path = ::util::findInPaths(base, search_paths);

    if ( path.size() == 0 ) {
        std::cerr << "warning: cannot find library " << s << std::endl;
        return;
    }

    dst->push_back(path);
}

void appendListFile(std::list<string>* dst, const std::list<string>& l,
                    const std::list<string>& search_paths, const string& prefix = "",
                    const string& postfix = "")
{
    for ( auto i : l )
        appendListFile(dst, i, search_paths, prefix, postfix);
}

int main(int argc, char** argv)
{
    bool want_hilti = true;
    bool want_compiler = false;
    bool want_runtime = false;
    bool want_debug = false;

    std::list<string> cflags;
    std::list<string> cxxflags;
    std::list<string> ldflags;
    std::list<string> libs;
    std::list<string> libs_shared;
    std::list<string> libfiles_static;

    std::list<string> options;

    // First pass over arguments: look for control options.

    for ( int i = 1; i < argc; i++ ) {
        string opt = argv[i];

        if ( opt == "--help" || opt == "-h" ) {
            usage();
            return 0;
        }

        if ( opt == "--compiler" ) {
            want_compiler = true;
            continue;
        }

        if ( opt == "--runtime" ) {
            want_runtime = true;
            continue;
        }

        if ( opt == "--debug" ) {
            want_debug = true;
            continue;
        }

        options.push_back(opt);
    }

    auto hilti_config = hilti::configuration();

    ////// HILTI

    if ( want_hilti ) {
        if ( want_compiler ) {
            appendList(&cflags, hilti_config.compiler_include_dirs, "-I");
            appendList(&cflags, hilti_config.compiler_cflags);
            appendList(&cxxflags, hilti_config.compiler_include_dirs, "-I");
            appendList(&cxxflags, hilti_config.compiler_cxxflags);
            appendList(&ldflags, hilti_config.compiler_ldflags);
            appendList(&libs, hilti_config.compiler_static_libraries, "-l");
            appendList(&libs, hilti_config.compiler_static_libraries_jit, "-l");
            appendList(&libs, hilti_config.compiler_shared_libraries, "-l");
            appendList(&libs_shared, hilti_config.compiler_shared_libraries, "-l");
            appendListFile(&libfiles_static, hilti_config.compiler_static_libraries,
                           hilti_config.compiler_library_dirs, "lib", ".a");
            appendListFile(&libfiles_static, hilti_config.compiler_static_libraries_jit,
                           hilti_config.compiler_library_dirs, "lib", ".a");

            // LLVM.
            if ( want_debug ) {
                appendList(&cflags, "-D_DEBUG", "");
                appendList(&cxxflags, "-D_DEBUG", "");
            }

            appendList(&cxxflags, util::strsplit(hilti_config.compiler_llvm_cxxflags));
            appendList(&ldflags, util::strsplit(hilti_config.compiler_llvm_ldflags));

            for ( auto l : util::strsplit(hilti_config.compiler_llvm_libraries) )
                appendList(&libs, l);
        }

        if ( want_runtime ) {
            appendList(&cflags, hilti_config.runtime_cflags);
            appendList(&cflags, hilti_config.runtime_include_dirs, "-I");
            appendList(&cxxflags, hilti_config.runtime_cxxflags);
            appendList(&cxxflags, hilti_config.runtime_include_dirs, "-I");
            appendList(&ldflags, hilti_config.runtime_ldflags);

            if ( want_debug ) {
                appendList(&libs, hilti_config.runtime_library_bca_dbg);
                appendList(&libfiles_static, hilti_config.runtime_library_bca_dbg);
            }

            else {
                appendList(&libs, hilti_config.runtime_library_bca);
                appendList(&libfiles_static, hilti_config.runtime_library_bca);
            }

            appendList(&libs, hilti_config.runtime_shared_libraries, "-l");
            appendList(&libs, hilti_config.runtime_library_a);
            appendList(&libfiles_static, hilti_config.runtime_library_a);
            appendList(&libs_shared, hilti_config.runtime_shared_libraries, "-l");

            if ( want_debug )
                appendList(&ldflags, hilti_config.runtime_ldflags_dbg);
        }
    }

    bool need_component = false;
    stringstream out;
    stringstream lout;

    for ( auto opt : options ) {
        if ( opt == "--distbase" ) {
            out << hilti_config.distbase << std::endl;
            continue;
        }

        if ( opt == "--prefix" ) {
            out << hilti_config.prefix << std::endl;
            continue;
        }

        if ( opt == "--version" ) {
            out << "HILTI " << hilti_config.version << std::endl;
            continue;
        }

        if ( opt == "--build" ) {
#ifdef DEBUG
            out << "debug" << std::endl;
#else
            out << "release" << std::endl;
#endif
            continue;
        }

        if ( opt == "--clang" ) {
            out << hilti_config.path_clang << std::endl;
            continue;
        }

        if ( opt == "--clang++" ) {
            out << hilti_config.path_clangxx << std::endl;
            continue;
        }

        if ( opt == "--llvm-config" ) {
            out << hilti_config.path_llvm_config << std::endl;
            continue;
        }

        if ( opt == "--cflags" ) {
            need_component = true;
            printList(lout, cflags);
            continue;
        }

        if ( opt == "--cxxflags" ) {
            need_component = true;
            printList(lout, cxxflags);
            continue;
        }

        if ( opt == "--ldflags" ) {
            need_component = true;
            printList(lout, ldflags);
            continue;
        }

        if ( opt == "--libs" ) {
            need_component = true;
            printList(lout, libs);
            continue;
        }

        if ( opt == "--libs-shared" ) {
            need_component = true;
            printList(lout, libs_shared);
            continue;
        }

        if ( opt == "--libfiles-static" ) {
            need_component = true;
            printList(lout, libfiles_static);
            continue;
        }

        if ( opt == "--hiltic-binary" ) {
            out << hilti_config.path_hiltic << std::endl;
            continue;
        }

        if ( opt == "--hilti-libdirs" ) {
            printList(lout, hilti_config.hilti_library_dirs);
            continue;
        }

        if ( opt == "--hilti-runtime-libs" ) {
            if ( want_debug )
                out << hilti_config.runtime_library_bca_dbg << std::endl;
            else
                out << hilti_config.runtime_library_bca << std::endl;
            continue;
        }

        std::cerr << "hilti-config: unknown option " << opt << "; use --help to see list."
                  << std::endl;
        return 1;
    }

    if ( need_component && ! (want_compiler || want_runtime) ) {
        cerr << "hilti-config: Either --compiler or --runtime (or both) must be given when "
                "printing flags."
             << std::endl;
        return 1;
    }

    cout << out.str();

    if ( lout.str().size() )
        cout << lout.str() << std::endl;

    return 0;
}
