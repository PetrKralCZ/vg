/** \file deconstruct_main.cpp
 *
 * Defines the "vg deconstruct" subcommand, which turns graphs back into VCFs.
 */


#include <omp.h>
#include <unistd.h>
#include <getopt.h>

#include <iostream>

#include "subcommand.hpp"

#include "../vg.hpp"
#include "../deconstructor.hpp"
#include "../integrated_snarl_finder.hpp"
#include <vg/io/stream.hpp>
#include <vg/io/vpkg.hpp>
#include <bdsg/overlays/overlay_helper.hpp>

using namespace std;
using namespace vg;
using namespace vg::subcommand;

void help_deconstruct(char** argv){
    cerr << "usage: " << argv[0] << " deconstruct [options] [-p|-P] <PATH> <GRAPH>" << endl
         << "Outputs VCF records for Snarls present in a graph (relative to a chosen reference path)." << endl
         << "options: " << endl
         << "    -p, --path NAME          A reference path to deconstruct against (multiple allowed)." << endl
         << "    -P, --path-prefix NAME   All paths beginning with NAME used as reference (multiple allowed)." << endl
         << "    -A, --alt-prefix NAME    Non-reference paths beginning with NAME get lumped together to same sample in VCF (multiple allowed).  Other non-ref paths not considered as samples." << endl
         << "    -r, --snarls FILE        Snarls file (from vg snarls) to avoid recomputing." << endl
         << "    -g, --gbwt FILE          only consider alt traversals that correspond to GBWT threads FILE." << endl
         << "    -e, --path-traversals    Only consider traversals that correspond to paths in the graph." << endl
         << "    -a, --all-snarls         Process all snarls, including nested snarls (by default only top-level snarls reported)." << endl
         << "    -d, --ploidy N           Expected ploidy.  If more traversals found, they will be flagged as conflicts (default: 2)" << endl
         << "    -t, --threads N          Use N threads" << endl
         << "    -v, --verbose            Print some status messages" << endl
         << endl;
}

int main_deconstruct(int argc, char** argv){
    if (argc <= 2) {
        help_deconstruct(argv);
        return 1;
    }

    vector<string> refpaths;
    vector<string> refpath_prefixes;
    vector<string> altpath_prefixes;
    string graphname;
    string snarl_file_name;
    string gbwt_file_name;
    bool path_restricted_traversals = false;
    bool show_progress = false;
    int ploidy = 2;
    bool set_ploidy = false;
    bool all_snarls = false;
    
    int c;
    optind = 2; // force optind past command positional argument
    while (true) {
        static struct option long_options[] =
            {
                {"help", no_argument, 0, 'h'},
                {"path", required_argument, 0, 'p'},
                {"path-prefix", required_argument, 0, 'P'},
                {"alt-prefix", required_argument, 0, 'A'},
                {"snarls", required_argument, 0, 'r'},
                {"gbwt", required_argument, 0, 'g'},                
                {"path-traversals", no_argument, 0, 'e'},
                {"ploidy", required_argument, 0, 'd'},
                {"all-snarls", no_argument, 0, 'a'},
                {"threads", required_argument, 0, 't'},
                {"verbose", no_argument, 0, 'v'},
                {0, 0, 0, 0}

            };

        int option_index = 0;
        c = getopt_long (argc, argv, "hp:P:A:r:g:ed:at:v",
                         long_options, &option_index);

        // Detect the end of the options.
        if (c == -1)
            break;

        switch (c)
        {
        case 'p':
            refpaths.push_back(optarg);
            break;
        case 'P':
            refpath_prefixes.push_back(optarg);
            break;
        case 'A':
            altpath_prefixes.push_back(optarg);
            break;
        case 'r':
            snarl_file_name = optarg;
            break;
        case 'g':
            gbwt_file_name = optarg;
            break;            
        case 'e':
            path_restricted_traversals = true;
            break;
        case 'd':
            ploidy = parse<int>(optarg);
            set_ploidy = true;
            break;
        case 'a':
            all_snarls = true;
            break;
        case 't':
            omp_set_num_threads(parse<int>(optarg));
            break;
        case 'v':
            show_progress = true;
            break;
        case '?':
        case 'h':
            help_deconstruct(argv);
            return 1;
        default:
            help_deconstruct(argv);
            abort();
        }

    }

    if ((!altpath_prefixes.empty() || set_ploidy) && !path_restricted_traversals) {
        cerr << "Error [vg deconstruct]: -A and -d can only be used with -e" << endl;
        return 1;
    }

    if (!gbwt_file_name.empty() && path_restricted_traversals) {
        cerr << "Error [vg deconstruct]: -e cannot be used with -g" << endl;
        return 1;
    }

    // Read the graph
    unique_ptr<PathHandleGraph> path_handle_graph;
    get_input_file(optind, argc, argv, [&](istream& in) {
            path_handle_graph = vg::io::VPKG::load_one<PathHandleGraph>(in);
        });

    bdsg::PathPositionOverlayHelper overlay_helper;
    PathPositionHandleGraph* graph = overlay_helper.apply(path_handle_graph.get());

    // Check our paths
    for (const string& ref_path : refpaths) {
        if (!graph->has_path(ref_path)) {
            cerr << "error [vg call]: Reference path \"" << ref_path << "\" not found in graph" << endl;
            return 1;
        }
    }
    
    if (refpaths.empty() && refpath_prefixes.empty()) {
        // No paths specified: use them all
        graph->for_each_path_handle([&](path_handle_t path_handle) {
                const string& name = graph->get_path_name(path_handle);
                if (!Paths::is_alt(name)) {
                    refpaths.push_back(name);
                }
            });
    }
    
    // Load or compute the snarls
    unique_ptr<SnarlManager> snarl_manager;    
    if (!snarl_file_name.empty()) {
        ifstream snarl_file(snarl_file_name.c_str());
        if (!snarl_file) {
            cerr << "Error [vg deconstruct]: Unable to load snarls file: " << snarl_file_name << endl;
            return 1;
        }
        if (show_progress) {
            cerr << "Loading snarls" << endl;
        }
        snarl_manager = vg::io::VPKG::load_one<SnarlManager>(snarl_file);
    } else {
        IntegratedSnarlFinder finder(*graph);
        if (show_progress) {
            cerr << "Finding snarls" << endl;
        }
        snarl_manager = unique_ptr<SnarlManager>(new SnarlManager(std::move(finder.find_snarls_parallel())));
    }

    unique_ptr<gbwt::GBWT> gbwt_index;
    if (!gbwt_file_name.empty()) {
        gbwt_index = vg::io::VPKG::load_one<gbwt::GBWT>(gbwt_file_name);
        if (gbwt_index.get() == nullptr) {
            cerr << "Error [vg deconstruct]: Unable to load gbwt index file: " << gbwt_file_name << endl;
            return 1;
        }
    }

    // We use this to map, for example, from chromosome to genome (eg S288C.chrXVI --> S288C)
    unordered_map<string, string> alt_path_to_prefix;
    
    // process the prefixes
    if (!refpath_prefixes.empty() || !altpath_prefixes.empty()) {
        graph->for_each_path_handle([&](const path_handle_t& path_handle) {
                string path_name = graph->get_path_name(path_handle);
                bool is_ref = false;
                for (auto& prefix : refpath_prefixes) {
                    if (path_name.compare(0, prefix.size(), prefix) == 0) {
                        refpaths.push_back(path_name);
                        is_ref = true;
                        break;
                    }
                }
                if (!is_ref) {
                    for (auto& prefix : altpath_prefixes) {
                        if (path_name.compare(0, prefix.size(), prefix) == 0) {
                            alt_path_to_prefix[path_name] = prefix;
                        }
                    }
                }
            });
    }

    // make sure we have at least one reference
    bool found_refpath = false;
    for (size_t i = 0; i < refpaths.size() && !found_refpath; ++i) {
        found_refpath = found_refpath || graph->has_path(refpaths[i]);
    }

    if (!found_refpath) {
        cerr << "Error [vg deconstruct]: No specified reference path or prefix found in graph" << endl;
        return 1;
    }

    // Deconstruct
    Deconstructor dd;
    if (show_progress) {
        cerr << "Decsontructing top-level snarls" << endl;
    }
    dd.deconstruct(refpaths, graph, snarl_manager.get(), path_restricted_traversals, ploidy, all_snarls,
                   !alt_path_to_prefix.empty() ? &alt_path_to_prefix : nullptr, gbwt_index.get());
    return 0;
}

// Register subcommand
static Subcommand vg_deconstruct("deconstruct", "create a VCF from variation in the graph", TOOLKIT, main_deconstruct);

